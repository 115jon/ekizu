#include "voice_connection_impl.hpp"

#include <ekizu/json_util.hpp>

#include "../dave/persisted_key_pair.hpp"
#include "voice_util.hpp"

namespace ekizu {

VoiceConnection::Impl::Impl(
	asio::any_io_executor executor, net::WebSocketClient ws, VoiceState state,
	std::string url, std::string token, std::unique_ptr<Codec> codec)
	: m_strand(executor),
	  m_ws(std::move(ws)),
	  m_state(std::move(state)),
	  m_url(std::move(url)),
	  m_token(std::move(token)),
	  m_codec(std::move(codec)) {}

void VoiceConnection::Impl::request_stop() {
	m_disconnected = true;
	m_heartbeat_running = false;
	m_receiver_running = false;

	if (m_heartbeat_timer) { m_heartbeat_timer->cancel(); }
	if (m_send_timer) { m_send_timer->cancel(); }

	m_channel.reset();
	m_ready_chan.reset();
	m_recv_chan.reset();

	m_heartbeat_timer.reset();
	m_send_timer.reset();

	if (m_udp) {
		(void)m_udp->close();
		m_udp.reset();
	}

	if (m_ws) { m_ws->cancel(); }

	// Reset DAVE state
	m_dave.reset();
}

void VoiceConnection::Impl::close(
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, h = std::move(h)]() mutable {
		self->m_logger.info("Closing voice connection...");

		self->m_heartbeat_running = false;
		if (self->m_heartbeat_timer) { self->m_heartbeat_timer->cancel(); }
		if (self->m_send_timer) { self->m_send_timer->cancel(); }

		self->m_channel.reset();
		self->m_ready_chan.reset();
		self->m_recv_chan.reset();
		self->m_heartbeat_timer.reset();
		self->m_send_timer.reset();

		if (self->m_udp) {
			auto close_res = self->m_udp->close();
			self->m_udp.reset();
			if (!close_res) {
				std::move(h)(close_res.error());
				return;
			}
		}

		if (!self->m_dave->session ||
			!self->m_dave->session->has_external_sender()) {
			self->m_logger.warn("MLS start skipped: No external sender key");
			std::move(h)(outcome::success());
			return;
		}

		if (!self->m_ws) {
			self->m_disconnected = true;
			std::move(h)(outcome::success());
			return;
		}

		self->m_ws->close(
			net::ws::close_code::normal,
			[self, h = std::move(h)](Result<> close_res) mutable {
				if (!close_res) {
					std::move(h)(close_res.error());
					return;
				}

				// Best-effort final read; ignore errors (matches old yield
				// behavior).
				self->m_ws->read([self, h = std::move(h)](
									 Result<net::WebSocketMessage>) mutable {
					self->m_ws.reset();
					self->m_disconnected = true;
					std::move(h)(outcome::success());
				});
			});
	});
}

void VoiceConnection::Impl::reconnect(
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, h = std::move(h)]() mutable {
		if (!self->m_ws) {
			std::move(h)(boost::system::errc::operation_not_permitted);
			return;
		}

		self->m_logger.info("Reconnecting to voice server...");

		nlohmann::json payload{
			{"op", static_cast<uint8_t>(VoiceOpcode::Resume)},
			{"d",
			 {{"server_id", fmt::to_string(*self->m_state.guild_id)},
			  {"session_id", self->m_state.session_id},
			  {"token", self->m_token}}}};

		self->m_ws->send(payload.dump(), std::move(h));
	});
}

void VoiceConnection::Impl::run(
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, h = std::move(h)]() mutable {
		if (self->m_ready_chan) {
			std::move(h)(outcome::success());
			return;
		}

		self->m_logger.info("Starting voice connection...");
		self->m_ready_chan.emplace(self->m_strand.get_inner_executor());

		if (!self->m_ws) {
			net::WebSocketClient::connect(
				self->m_strand.get_inner_executor(), self->m_url,
				[self, h = std::move(h)](
					Result<net::WebSocketClient> ws_res) mutable {
					if (!ws_res) {
						std::move(h)(ws_res.error());
						return;
					}

					self->m_ws.emplace(std::move(ws_res.value()));
					self->ws_listen_loop();

					self->m_logger.debug(
						"Waiting for voice connection ready...");
					self->m_ready_chan->async_receive(
						[self, h = std::move(h)](boost::system::error_code ec,
												 boost::blank) mutable {
							if (ec) {
								std::move(h)(ec);
								return;
							}
							self->m_logger.info("Voice connection ready!");
							std::move(h)(outcome::success());
						});
				});
			return;
		}

		self->ws_listen_loop();
		self->m_logger.debug("Waiting for voice connection ready...");
		self->m_ready_chan->async_receive(
			[self, h = std::move(h)](
				boost::system::error_code ec, boost::blank) mutable {
				if (ec) {
					std::move(h)(ec);
					return;
				}
				self->m_logger.info("Voice connection ready!");
				std::move(h)(outcome::success());
			});
	});
}

void VoiceConnection::Impl::silence(
	asio::any_completion_handler<void(Result<>)> h) {
	std::vector<std::byte> s(
		voice::SILENCE_FRAME.begin(), voice::SILENCE_FRAME.end());
	send_opus(std::move(s), std::move(h));
}

void VoiceConnection::Impl::speak(
	SpeakerFlag flags, asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, flags, h = std::move(h)]() mutable {
		if (!self->m_ws) {
			std::move(h)(boost::system::errc::operation_not_permitted);
			return;
		}

		nlohmann::json payload{
			{"op", static_cast<uint8_t>(VoiceOpcode::Speaking)},
			{"d",
			 {{"speaking", static_cast<uint8_t>(flags)},
			  {"delay", 0},
			  {"ssrc", self->m_ssrc}}}};

		self->m_speaking = (static_cast<uint8_t>(flags) != 0);
		self->m_ws->send(payload.dump(), std::move(h));
	});
}

void VoiceConnection::Impl::flush(
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();

	struct FlushOp : std::enable_shared_from_this<FlushOp> {
		std::shared_ptr<VoiceConnection::Impl> impl;
		asio::steady_timer timer;
		asio::any_completion_handler<void(Result<>)> handler;

		FlushOp(std::shared_ptr<VoiceConnection::Impl> i,
				asio::any_completion_handler<void(Result<>)> h)
			: impl(std::move(i)),
			  timer(impl->m_strand, std::chrono::milliseconds(10)),
			  handler(std::move(h)) {}

		void check() {
			if (impl->m_pending_frames <= 0 || !impl->m_udp ||
				impl->m_disconnected) {
				std::move(handler)(outcome::success());
				return;
			}

			timer.expires_after(std::chrono::milliseconds(10));
			timer.async_wait([me = shared_from_this()](
								 boost::system::error_code ec) mutable {
				if (ec) {
					std::move(me->handler)(ec);
					return;
				}
				me->check();
			});
		}
	};

	auto op = std::make_shared<FlushOp>(self, std::move(h));
	asio::dispatch(m_strand, [op]() mutable { op->check(); });
}

void VoiceConnection::Impl::send_voice_json(
	const nlohmann::json &j, asio::any_completion_handler<void(Result<>)> h) {
	if (!m_ws) {
		asio::post(m_strand, [h = std::move(h)]() mutable {
			std::move(h)(outcome::success());
		});
		return;
	}
	// send() takes ownership of the std::string
	m_ws->send(j.dump(), std::move(h));
}

void VoiceConnection::Impl::send_voice_binary(
	VoiceOpcode opcode, boost::span<const std::byte> payload,
	asio::any_completion_handler<void(Result<>)> h) {
	if (!m_ws) {
		asio::post(m_strand, [h = std::move(h)]() mutable {
			std::move(h)(outcome::success());
		});
		return;
	}

	// IMPORTANT: keep buffer alive until async send completes.
	auto buf = std::make_shared<std::vector<std::byte>>();
	buf->reserve(1 + payload.size());
	buf->push_back(static_cast<std::byte>(opcode));
	buf->insert(buf->end(), payload.begin(), payload.end());

	m_ws->send_bytes(
		*buf, [buf, h = std::move(h)](Result<> r) mutable { std::move(h)(r); });
}

void VoiceConnection::Impl::setup_udp_async(
	const nlohmann::json &data,
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();

	m_ssrc = data["d"]["ssrc"].get<uint32_t>();
	std::string ip = data["d"]["ip"].get<std::string>();
	uint16_t port = data["d"]["port"].get<uint16_t>();

	// Initialize DAVE state if needed
	if (!m_dave) {
		m_dave = std::make_unique<DaveState>();
		m_dave->encryptor = std::make_unique<dave::Encryptor>();
		m_dave->session = std::make_unique<dave::Session>(
			nullptr, m_state.session_id,
			[self](const std::string &type, const std::string &reason) {
				self->m_logger.warn("DAVE Error [{}]: {}", type, reason);
			});
		m_dave->transient_key = dave::get_persisted_key_pair(
			"ekizu_voice", fmt::to_string(m_state.user_id), dave::KEY_VERSION);
	}
	m_dave->encryptor->assign_ssrc_to_codec(m_ssrc, dave::Codec::Opus);
	m_dave->encryptor->set_passthrough_mode(true);

	// Bind a local socket (Rust-style), then connect to the Discord voice peer.
	// Use an address family wildcard that matches the discovered peer.
	const bool peer_is_v6 = (ip.find(':') != std::string::npos);
	std::string bind_addr = peer_is_v6 ? "[::]:0" : "0.0.0.0:0";
	std::string port_str = std::to_string(port);

	net::UdpSocket::bind(
		m_strand, bind_addr,
		[self, h = std::move(h), ip = std::move(ip),
		 port_str =
			 std::move(port_str)](Result<net::UdpSocket> udp_res) mutable {
			if (!udp_res) {
				std::move(h)(udp_res.error());
				return;
			}
			self->m_udp.emplace(std::move(udp_res.value()));

			self->m_udp->connect(
				std::move(ip), std::move(port_str),
				[self, h = std::move(h)](Result<> conn_res) mutable {
					if (!conn_res) {
						self->m_udp.reset();
						std::move(h)(conn_res.error());
						return;
					}

					// IP discovery packet (must stay alive)
					auto pkt = std::make_shared<std::vector<std::byte>>(74);
					voice::put_be(pkt->data() + 0, uint16_t{0x1});
					voice::put_be(pkt->data() + 2, uint16_t{70});
					voice::put_be(pkt->data() + 4, self->m_ssrc);

					auto pkt_span =
						boost::span<const std::byte>(pkt->data(), pkt->size());
					self->m_udp->send(pkt_span, [self, pkt, h = std::move(h)](
													Result<size_t>
														send_res) mutable {
						if (!send_res) {
							std::move(h)(send_res.error());
							return;
						}

						self->m_udp->receive([self, h = std::move(h)](
												 Result<std::string>
													 recv_res) mutable {
							if (!recv_res) {
								std::move(h)(recv_res.error());
								return;
							}

							const auto &res = recv_res.value();
							if (res.size() < 74) {
								std::move(h)(boost::system::errc::message_size);
								return;
							}

							auto ip_view = std::string_view(res).substr(8, 64);
							ip_view = ip_view.substr(0, ip_view.find('\0'));

							uint16_t ext_port{};
							std::memcpy(
								&ext_port, res.data() + res.size() - 2, 2);
							ext_port = boost::endian::big_to_native(ext_port);

							nlohmann::json payload{
								{"op", static_cast<uint8_t>(
										   VoiceOpcode::SelectProtocol)},
								{"d",
								 {{"protocol", "udp"},
								  {"data",
								   {{"address", ip_view},
									{"port", ext_port},
									{"mode", "aead_aes256_gcm_rtpsize"}}}}}};

							self->m_ws->send(payload.dump(), std::move(h));
						});
					});
				});
		});
}

void VoiceConnection::Impl::handle_session_description_async(
	const nlohmann::json &data,
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, data, h = std::move(h)]() mutable {
		if (data["d"].contains("media_session_id") &&
			!data["d"]["media_session_id"].is_null()) {
			self->m_media_session_id =
				data["d"]["media_session_id"].get<std::string>();
		}

		std::string mode_str = data["d"]["mode"].get<std::string>();
		std::vector<uint8_t> key =
			data["d"]["secret_key"].get<std::vector<uint8_t>>();

		self->m_logger.info("🔑 Extracted secret key: {} bytes", key.size());

		if (key.size() != 32) {
			self->m_logger.error(
				"❌ Invalid key size: {} (expected 32)", key.size());
		}

		self->m_crypto.key.resize(key.size());
		std::memcpy(self->m_crypto.key.data(), key.data(), key.size());
		self->m_crypto.nonce = 0;

		if (mode_str == "aead_xchacha20_poly1305_rtpsize") {
			self->m_crypto.mode =
				VoiceTransportMode::XChaCha20_Poly1305_RTPSIZE;
		} else if (mode_str == "aead_aes256_gcm_rtpsize") {
			self->m_crypto.mode = VoiceTransportMode::AES256_GCM_RTPSIZE;
		} else {
			self->m_logger.error("Unsupported encryption mode: {}", mode_str);
			std::move(h)(boost::system::errc::not_supported);
			return;
		}

		self->m_logger.info("✅ Session description: mode={}, key_size={}",
							mode_str, key.size());

		if (data["d"].contains("dave_protocol_version")) {
			if (!data["d"]["dave_protocol_version"].is_null()) {
				int version = data["d"]["dave_protocol_version"].get<int>();
				self->m_logger.info(
					"DAVE Protocol Version in Session Description: {}",
					version);
			} else {
				self->m_logger.warn("DAVE Protocol Version is NULL");
			}
		} else {
			self->m_logger.warn(
				"DAVE Protocol Version MISSING from Session Description");
			self->m_logger.debug("Full payload: {}", data.dump());
		}

		int version{};
		if (data["d"].contains("dave_protocol_version") &&
			!data["d"]["dave_protocol_version"].is_null()) {
			version = data["d"]["dave_protocol_version"].get<int>();
		}
		if (self->m_dave) {
			self->m_dave->protocol_version =
				static_cast<dave::ProtocolVersion>(version);
			self->m_dave->session->set_protocol_version(
				static_cast<dave::ProtocolVersion>(version));
		}

		auto after_mls = [self, h = std::move(h)](Result<> r) mutable {
			if (!r) {
				std::move(h)(r.error());
				return;
			}

			if (!self->m_channel) {
				self->m_channel.emplace(self->m_strand.get_inner_executor());
			}
			self->opus_sender_loop();

			if (!self->m_recv_chan) {
				self->m_recv_chan.emplace(self->m_strand.get_inner_executor());
			}
			self->udp_receiver_loop();

			std::move(h)(outcome::success());
		};

		if (self->m_dave && self->m_dave->protocol_version > 0) {
			self->m_warned_waiting_for_e2ee = false;
			self->maybe_start_mls_async(false, std::move(after_mls));
			return;
		}

		after_mls(outcome::success());
	});
}

}  // namespace ekizu
