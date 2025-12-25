#include "voice_connection_impl.hpp"

#include <sodium/crypto_generichash.h>

#include <boost/charconv/from_chars.hpp>
#include <ekizu/json_util.hpp>

namespace {
ekizu::Result<std::uint64_t> parse_media_session_id_prefix_hex16(
	std::string_view s) {
	if (s.size() < 16) {
		return make_error_code(boost::system::errc::invalid_argument);
	}

	std::uint64_t v = 0;
	const char *first = s.data();
	const char *last = first + 16;

	auto r = boost::charconv::from_chars(
		first, last, v, 16);  // base-16 parse [web:71]

	if (r.ec == std::errc{} && r.ptr == last) { return v; }

	if (r.ec == std::errc::result_out_of_range) {
		return make_error_code(boost::system::errc::result_out_of_range);
	}

	return make_error_code(boost::system::errc::invalid_argument);
}
}  // namespace

namespace ekizu {

VoiceConnection::Impl::Impl(
	asio::any_io_executor executor, net::WebSocketClient ws, VoiceState state,
	std::string url, std::string token, std::unique_ptr<Codec> codec)
	: m_strand(executor),
	  m_ws(std::move(ws)),
	  m_state(std::move(state)),
	  m_url(std::move(url)),
	  m_token(std::move(token)),
	  m_codec(std::move(codec)),
	  m_dave_manager(
		  std::make_shared<DaveManager>(fmt::to_string(m_state.user_id))) {}

void VoiceConnection::Impl::attach_logger(
	std::function<void(const Log &)> on_log) {
	m_on_log = std::move(on_log);
	if (m_on_log) {
		m_dave_manager->set_logger([this](std::string_view msg) {
			log(msg, LogLevel::Debug);
		});
	}
}

void VoiceConnection::Impl::request_stop() {
	m_disconnected = true;
	m_heartbeat_running = false;

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
}

void VoiceConnection::Impl::close(
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, h = std::move(h)]() mutable {
		self->log("Closing voice connection...", LogLevel::Info);

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

		self->log("Reconnecting to voice server...", LogLevel::Info);

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

		self->log("Starting voice connection...", LogLevel::Info);
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

					self->log("Waiting for voice connection ready...",
							  LogLevel::Debug);
					self->m_ready_chan->async_receive(
						[self, h = std::move(h)](boost::system::error_code ec,
												 boost::blank) mutable {
							if (ec) {
								std::move(h)(ec);
								return;
							}
							self->log(
								"Voice connection ready!", LogLevel::Info);
							std::move(h)(outcome::success());
						});
				});
			return;
		}

		self->ws_listen_loop();
		self->log("Waiting for voice connection ready...", LogLevel::Debug);
		self->m_ready_chan->async_receive(
			[self, h = std::move(h)](
				boost::system::error_code ec, boost::blank) mutable {
				if (ec) {
					std::move(h)(ec);
					return;
				}
				self->log("Voice connection ready!", LogLevel::Info);
				std::move(h)(outcome::success());
			});
	});
}

void VoiceConnection::Impl::send_opus(
	std::vector<std::byte> data,
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, data = std::move(data),
							  h = std::move(h)]() mutable {
		if (data.empty()) {
			std::move(h)(boost::system::errc::invalid_argument);
			return;
		}
		if (!self->m_channel) {
			std::move(h)(boost::system::errc::operation_not_permitted);
			return;
		}

		const auto samples = opus_packet_get_samples_per_frame(
			reinterpret_cast<const unsigned char *>(data.data()), SAMPLE_RATE);
		if (samples < 0) {
			std::move(h)(boost::system::error_code{
				samples, boost::system::system_category()});
			return;
		}

		self->m_pending_frames++;
		self->m_channel->async_send(
			boost::system::error_code{},
			AudioPacket{std::move(data), static_cast<size_t>(samples)},
			[self, h = std::move(h)](boost::system::error_code ec) mutable {
				if (ec) {
					self->m_pending_frames--;
					std::move(h)(ec);
					return;
				}
				std::move(h)(outcome::success());
			});
	});
}

void VoiceConnection::Impl::send_raw(
	std::vector<int16_t> data, asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(
		m_strand, [self, data = std::move(data), h = std::move(h)]() mutable {
			if (data.empty()) {
				std::move(h)(boost::system::errc::invalid_argument);
				return;
			}
			if (!self->m_channel) {
				std::move(h)(boost::system::errc::operation_not_permitted);
				return;
			}

			std::vector<std::byte> encoded_audio(data.size());
			auto encoded_res = self->m_codec->encode(
				boost::span<const int16_t>(data.data(), data.size()),
				encoded_audio);
			if (!encoded_res) {
				std::move(h)(encoded_res.error());
				return;
			}

			encoded_audio.resize(encoded_res.value());
			self->send_opus(std::move(encoded_audio), std::move(h));
		});
}

void VoiceConnection::Impl::silence(
	asio::any_completion_handler<void(Result<>)> h) {
	std::vector<std::byte> s(
		detail::SILENCE_FRAME.begin(), detail::SILENCE_FRAME.end());
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

void VoiceConnection::Impl::setup_heartbeat(const nlohmann::json &data) {
	if (m_heartbeat_timer) { return; }
	m_heartbeat_interval_ms = data["d"]["heartbeat_interval"].get<uint32_t>();
	m_heartbeat_timer.emplace(
		m_strand, std::chrono::milliseconds(m_heartbeat_interval_ms));
	m_heartbeat_running = true;
	heartbeat_tick();
}

void VoiceConnection::Impl::heartbeat_tick() {
	auto self = shared_from_this();
	if (!m_heartbeat_running || !m_heartbeat_timer) { return; }

	m_heartbeat_timer->async_wait([self](boost::system::error_code ec) mutable {
		if (ec || !self->m_heartbeat_running) { return; }

		if (!self->m_last_heartbeat_acked) {
			self->log("Connection may be dead (heartbeat ack missing)",
					  LogLevel::Warn);
		}
		self->m_last_heartbeat_acked = false;

		self->send_heartbeat([self](Result<> /*ignored*/) mutable {
			if (!self->m_heartbeat_running || !self->m_heartbeat_timer) {
				return;
			}
			self->m_heartbeat_timer->expires_after(
				std::chrono::milliseconds(self->m_heartbeat_interval_ms));
			self->heartbeat_tick();
		});
	});
}

void VoiceConnection::Impl::send_heartbeat(
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, h = std::move(h)]() mutable {
		if (!self->m_ws) {
			std::move(h)(outcome::success());
			return;
		}

		const auto now_ms =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch())
				.count();

		nlohmann::json payload{
			{"op", static_cast<uint8_t>(VoiceOpcode::Heartbeat)},
			{"d", {{"t", now_ms}, {"seq_ack", self->m_last_seq}}}};

		self->m_ws->send(payload.dump(), std::move(h));
	});
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

	m_dave_manager->assign_ssrc_to_codec(m_ssrc, discord::dave::Codec::Opus);
	m_dave_manager->set_passthrough_mode(true);

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
					detail::put_be(pkt->data() + 0, uint16_t{0x1});
					detail::put_be(pkt->data() + 2, uint16_t{70});
					detail::put_be(pkt->data() + 4, self->m_ssrc);

					auto pkt_span =
						boost::span<const std::byte>(pkt->data(), pkt->size());
					self->m_udp->send(
						pkt_span, [self, pkt, h = std::move(h)](
									  Result<size_t> send_res) mutable {
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
									std::move(h)(
										boost::system::errc::message_size);
									return;
								}

								auto ip_view =
									std::string_view(res).substr(8, 64);
								ip_view = ip_view.substr(0, ip_view.find('\0'));

								uint16_t ext_port{};
								std::memcpy(
									&ext_port, res.data() + res.size() - 2, 2);
								ext_port =
									boost::endian::big_to_native(ext_port);

								nlohmann::json payload{
									{"op", static_cast<uint8_t>(
											   VoiceOpcode::SelectProtocol)},
									{"d",
									 {{"protocol", "udp"},
									  {"data",
									   {{"address", ip_view},
										{"port", ext_port},
										{"mode",
										 to_string(self->m_crypto.mode)}}}}}};

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

		self->m_crypto.key.resize(key.size());
		std::memcpy(self->m_crypto.key.data(), key.data(), key.size());
		self->m_crypto.nonce = 0;

		if (mode_str == "aead_xchacha20_poly1305_rtpsize") {
			self->m_crypto.mode =
				VoiceTransportMode::XChaCha20_Poly1305_RTPSIZE;
		} else if (mode_str == "aead_aes256_gcm_rtpsize") {
			self->m_crypto.mode = VoiceTransportMode::AES256_GCM_RTPSIZE;
		} else {
			self->log(
				"Unsupported encryption mode: " + mode_str, LogLevel::Error);
			std::move(h)(boost::system::errc::not_supported);
			return;
		}

		self->log(fmt::format("Session description: mode={}", mode_str),
				  LogLevel::Info);

		int version = 0;
		if (data["d"].contains("dave_protocol_version") &&
			!data["d"]["dave_protocol_version"].is_null()) {
			version = data["d"]["dave_protocol_version"].get<int>();
		}
		self->m_dave_manager->set_protocol_version(version);

		auto after_mls = [self, h = std::move(h)](Result<> r) mutable {
			if (!r) {
				std::move(h)(r.error());
				return;
			}

			if (!self->m_channel) {
				self->m_channel.emplace(self->m_strand.get_inner_executor());
				self->opus_sender_loop();
			}

			std::move(h)(outcome::success());
		};

		if (self->m_dave_manager->protocol_version() > 0) {
			self->m_warned_waiting_for_e2ee = false;
			self->maybe_start_mls_async(false, std::move(after_mls));
			return;
		}

		after_mls(outcome::success());
	});
}

void VoiceConnection::Impl::opus_sender_loop() {
	auto self = shared_from_this();
	if (!m_channel || !m_udp) { return; }

	if (!m_send_timer) { m_send_timer.emplace(m_strand.get_inner_executor()); }

	// Signal "ready" once.
	if (m_ready_chan) {
		m_ready_chan->async_send(
			boost::system::error_code{}, {}, [](boost::system::error_code) {});
	}

	struct SenderOp : std::enable_shared_from_this<SenderOp> {
		std::shared_ptr<VoiceConnection::Impl> impl;
		std::chrono::steady_clock::time_point next_send_time{
			std::chrono::steady_clock::now()};
		int consecutive_failures{0};

		explicit SenderOp(std::shared_ptr<VoiceConnection::Impl> i)
			: impl(std::move(i)) {}

		void step() {
			if (!impl->m_channel || !impl->m_udp) { return; }

			impl->m_channel->async_receive(
				[me = shared_from_this()](
					boost::system::error_code ec, AudioPacket pkt) mutable {
					if (ec) { return; }
					me->process(std::move(pkt));
				});
		}

		void process(AudioPacket pkt) {
			const auto now = std::chrono::steady_clock::now();

			if (next_send_time > now) {
				impl->m_send_timer->expires_at(next_send_time);
				impl->m_send_timer->async_wait(
					[me = shared_from_this(), pkt = std::move(pkt)](
						boost::system::error_code ec) mutable {
						if (ec) { return; }
						me->send(std::move(pkt));
					});
				return;
			}

			const auto drift_ms =
				std::chrono::duration_cast<std::chrono::milliseconds>(
					now - next_send_time)
					.count();
			if (drift_ms > 100) {
				impl->log(
					fmt::format("Large timing drift detected: {}ms, resetting",
								drift_ms),
					LogLevel::Warn);
				next_send_time = now;
			}

			send(std::move(pkt));
		}

		void maintain_sync(size_t frame_count) {
			impl->m_rtp_sequence++;
			impl->m_rtp_timestamp += static_cast<uint32_t>(frame_count);
			next_send_time +=
				std::chrono::milliseconds(frame_count * 1000 / SAMPLE_RATE);
		}

		void send(AudioPacket pkt) {
			if (!impl->m_udp) {
				impl->m_pending_frames--;
				return;
			}

			if (impl->m_dave_manager->is_e2ee_enabled() &&
				!impl->m_dave_manager->ready_to_send()) {
				if (!impl->m_warned_waiting_for_e2ee) {
					impl->log(
						fmt::format("DAVE enabled but not ready at packet {}",
									impl->m_packet_count.load()),
						LogLevel::Warn);
					impl->m_warned_waiting_for_e2ee = true;
				}
				maintain_sync(pkt.frame_count);
				impl->m_pending_frames--;
				step();
				return;
			}

			boost::span<const std::byte> payload_view(
				pkt.encoded.data(), pkt.encoded.size());
			std::vector<std::byte> dave_ciphertext;

			if (impl->m_dave_manager->is_e2ee_enabled()) {
				size_t max_sz = impl->m_dave_manager->max_ciphertext_size(
					discord::dave::MediaType::Audio, pkt.encoded.size());
				dave_ciphertext.resize(max_sz);

				auto res = impl->m_dave_manager->encrypt_frame(
					discord::dave::MediaType::Audio, impl->m_ssrc, pkt.encoded,
					dave_ciphertext);
				if (!res) {
					impl->log("DAVE encrypt failed", LogLevel::Error);
					impl->m_pending_frames--;
					maintain_sync(pkt.frame_count);
					step();
					return;
				}

				dave_ciphertext.resize(res.value());
				payload_view = boost::span<const std::byte>(
					dave_ciphertext.data(), dave_ciphertext.size());
			}

			std::array<std::byte, detail::RTP_HEADER_SIZE> header{};
			header[0] = std::byte{0x80};
			header[1] = std::byte{0x78};
			detail::put_be(header.data() + 2, impl->m_rtp_sequence);
			detail::put_be(header.data() + 4, impl->m_rtp_timestamp);
			detail::put_be(header.data() + 8, impl->m_ssrc);

			auto encrypted_pkt =
				impl->m_crypto.encrypt_rtp(header, payload_view);
			if (!encrypted_pkt) {
				impl->log("Transport encrypt failed", LogLevel::Error);
				impl->m_pending_frames--;
				maintain_sync(pkt.frame_count);
				step();
				return;
			}

			impl->m_udp->send(
				encrypted_pkt.value(),
				[me = shared_from_this(), frame_count = pkt.frame_count](
					Result<size_t> send_res) mutable {
					if (!send_res) {
						++me->consecutive_failures;
						me->impl->log(fmt::format("UDP send failed ({}/10): {}",
												  me->consecutive_failures,
												  send_res.error().message()),
									  LogLevel::Warn);

						me->impl->m_pending_frames--;
						me->maintain_sync(frame_count);

						if (me->consecutive_failures >= 10) {
							me->impl->log(
								"Too many consecutive UDP send failures, "
								"stopping sender",
								LogLevel::Error);
							return;	 // Stop sender loop (matches old “bail out”
									 // behavior).
						}

						me->step();
						return;
					}

					me->consecutive_failures = 0;
					me->impl->m_packet_count.fetch_add(1);
					me->impl->m_pending_frames--;
					me->maintain_sync(frame_count);
					me->step();
				});
		}
	};

	auto op = std::make_shared<SenderOp>(self);
	asio::post(m_strand, [op]() mutable { op->step(); });
}

void VoiceConnection::Impl::ws_listen_loop() {
	auto self = shared_from_this();
	if (!m_ws) { return; }

	struct ListenOp : std::enable_shared_from_this<ListenOp> {
		std::shared_ptr<VoiceConnection::Impl> impl;

		explicit ListenOp(std::shared_ptr<VoiceConnection::Impl> i)
			: impl(std::move(i)) {}

		void step() {
			if (!impl->m_ws) { return; }

			impl->m_ws->read([me = shared_from_this()](
								 Result<net::WebSocketMessage>
									 msg_res) mutable {
				if (!msg_res) {
					auto close_reason =
						me->impl->m_ws ? me->impl->m_ws->close_reason()
									   : std::nullopt;
					std::string msg = fmt::format(
						"WS Listen failed: {}", msg_res.error().message());
					if (close_reason) {
						msg += fmt::format(
							", close_code: {}, close_reason: {}",
							close_reason->code, close_reason->reason);
					}
					me->impl->log(msg, LogLevel::Error);
					return;
				}

				auto msg = std::move(msg_res.value());
				if (msg.payload.empty()) {
					me->step();
					return;
				}

				if (!msg.is_binary) {
					auto data_res = json_util::try_parse(msg.payload);
					if (!data_res) {
						me->step();
						return;
					}

					const auto &data = data_res.value();
					if (data.contains("seq")) {
						me->impl->m_last_seq = data["seq"].get<int64_t>();
					}

					auto opcode =
						static_cast<VoiceOpcode>(data["op"].get<int>());
					switch (opcode) {
						case VoiceOpcode::Ready:
							me->impl->setup_udp_async(
								data, [me](Result<>) mutable { me->step(); });
							return;

						case VoiceOpcode::SessionDescription:
							me->impl->handle_session_description_async(
								data, [me](Result<>) mutable { me->step(); });
							return;

						case VoiceOpcode::HeartbeatAck:
							me->impl->m_last_heartbeat_acked = true;
							break;

						case VoiceOpcode::Hello: {
							me->impl->setup_heartbeat(data);

							nlohmann::json id_payload{
								{"op",
								 static_cast<uint8_t>(VoiceOpcode::Identify)},
								{"d",
								 {{"server_id",
								   fmt::to_string(*me->impl->m_state.guild_id)},
								  {"user_id",
								   fmt::to_string(me->impl->m_state.user_id)},
								  {"session_id", me->impl->m_state.session_id},
								  {"token", me->impl->m_token},
								  {"max_dave_protocol_version",
								   discord::dave::
									   MaxSupportedProtocolVersion()}}}};

							if (me->impl->m_disconnected) {
								id_payload["op"] =
									static_cast<uint8_t>(VoiceOpcode::Resume);
								id_payload["d"] = {
									{"server_id",
									 fmt::to_string(
										 *me->impl->m_state.guild_id)},
									{"session_id",
									 me->impl->m_state.session_id},
									{"token", me->impl->m_token}};
								me->impl->m_disconnected = false;
							}

							me->impl->m_ws->send(
								id_payload.dump(),
								[me](Result<>) mutable { me->step(); });
							return;
						}

						default: break;
					}

					me->step();
					return;
				}

				// Binary message
				auto bytes = boost::span<const std::byte>(
					reinterpret_cast<const std::byte *>(msg.payload.data()),
					msg.payload.size());
				if (bytes.size() < 3) {
					me->step();
					return;
				}

				uint16_t seq = detail::read_u16be(bytes);
				auto op =
					static_cast<VoiceOpcode>(static_cast<uint8_t>(bytes[2]));
				me->impl->m_last_seq = seq;

				std::vector<std::byte> payload(bytes.begin() + 3, bytes.end());
				me->impl->handle_binary_event_async(
					op, std::move(payload), seq,
					[me](Result<>) mutable { me->step(); });
			});
		}
	};

	auto op = std::make_shared<ListenOp>(self);
	asio::post(m_strand, [op]() mutable { op->step(); });
}

// --- DAVE/MLS helpers (kept functionally equivalent, but with async handler
// style) ---

void VoiceConnection::Impl::handle_binary_event_async(
	VoiceOpcode op, std::vector<std::byte> payload, uint16_t /*seq*/,
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, op, payload = std::move(payload),
							  h = std::move(h)]() mutable {
		auto payload_span =
			boost::span<const std::byte>(payload.data(), payload.size());

		switch (op) {
			case VoiceOpcode::DaveMlsExternalSender: {
				self->m_dave_manager->set_external_sender(
					{payload.begin(), payload.end()});
				self->maybe_start_mls_async(false, std::move(h));
				return;
			}

			case VoiceOpcode::DaveMlsProposals: {
				self->log("Received dave_mls_proposals", LogLevel::Info);

				auto process = [self](
								   std::vector<std::byte> &payload,
								   asio::any_completion_handler<void(Result<>)>
									   &h) mutable {
					auto recognized = self->m_recognized_user_ids;
					recognized.insert(fmt::to_string(self->m_state.user_id));

					auto res = self->m_dave_manager->process_proposals(
						payload, recognized);
					if (!res) {
						self->log(
							"process_proposals failed; staying in current MLS "
							"state",
							LogLevel::Warn);
						std::move(h)(outcome::success());
						return;
					}

					const auto &cw = res.value();
					std::vector<std::byte> cw_vec(
						reinterpret_cast<const std::byte *>(cw.data()),
						reinterpret_cast<const std::byte *>(cw.data()) +
							cw.size());
					self->send_mls_commit_welcome(
						std::move(cw_vec), std::move(h));
				};

				if (!self->m_dave_manager->is_initialized()) {
					// If MLS start fails, propagate the error (don’t “clear”
					// the lambda).
					self->maybe_start_mls_async(
						false, [self, payload = std::move(payload),
								h = std::move(h), process](Result<> r) mutable {
							if (!r) {
								std::move(h)(r.error());
								return;
							}
							auto p = payload;  // or keep payload mutable by
											   // removing const on capture
							auto hh = std::move(h);
							process(p, hh);
						});
					return;
				}

				process(payload, h);
				return;
			}

			case VoiceOpcode::DaveMlsAnnounceCommitTransition: {
				if (payload_span.size() < 2) {
					std::move(h)(outcome::success());
					return;
				}

				uint16_t tid = detail::read_u16be(payload_span);
				auto commit_data = payload_span.subspan(2);

				if (self->m_dave_manager->process_commit(commit_data)) {
					nlohmann::json ready{
						{"op", static_cast<uint8_t>(
								   VoiceOpcode::DaveTransitionReady)},
						{"d", {{"transition_id", tid}}}};
					self->send_voice_json(ready, std::move(h));
					return;
				}

				self->log(fmt::format("Failed to process commit (tid={})", tid),
						  LogLevel::Warn);
				self->recover_mls_after_invalid_transition_async(
					tid, std::move(h));
				return;
			}

			case VoiceOpcode::DaveMlsWelcome: {
				if (payload_span.size() < 2) {
					std::move(h)(outcome::success());
					return;
				}

				uint16_t tid = detail::read_u16be(payload_span);
				auto recognized = self->m_recognized_user_ids;
				recognized.insert(fmt::to_string(self->m_state.user_id));

				if (!self->m_dave_manager->process_welcome(
						payload_span.subspan(2), recognized)) {
					self->recover_mls_after_invalid_transition_async(
						tid, std::move(h));
					return;
				}

				nlohmann::json ready{
					{"op",
					 static_cast<uint8_t>(VoiceOpcode::DaveTransitionReady)},
					{"d", {{"transition_id", tid}}}};

				self->send_voice_json(
					ready, [self, h = std::move(h)](Result<> r) mutable {
						if (!r) {
							std::move(h)(r.error());
							return;
						}
						self->execute_dave_transition_now_async(
							self->m_dave_manager->protocol_version(),
							std::move(h));
					});
				return;
			}

			default: break;
		}

		std::move(h)(outcome::success());
	});
}

void VoiceConnection::Impl::maybe_start_mls_async(
	bool force_reset, asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, force_reset, h = std::move(h)]() mutable {
		if (self->m_dave_manager->protocol_version() <= 0) {
			std::move(h)(outcome::success());
			return;
		}

		if (force_reset) { self->m_dave_manager->reset_session(); }
		if (self->m_dave_manager->is_initialized() && !force_reset) {
			std::move(h)(outcome::success());
			return;
		}

		if (!self->m_dave_manager->has_external_sender()) {
			std::move(h)(outcome::success());
			return;
		}

		auto sig = self->m_dave_manager->get_or_create_sig_key(
			self->m_dave_manager->protocol_version());
		if (!sig) {
			std::move(h)(outcome::success());
			return;
		}

		auto init_res = self->m_dave_manager->initialize_session(
			self->m_dave_manager->protocol_version(), self->compute_group_id(),
			sig);
		if (!init_res) {
			std::move(h)(init_res.error());
			return;
		}

		self->send_mls_key_package(std::move(h));
	});
}

void VoiceConnection::Impl::execute_dave_transition_now_async(
	int protocol_version, asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, protocol_version,
							  h = std::move(h)]() mutable {
		self->log(
			fmt::format("Executing DAVE transition to protocol version {}",
						protocol_version),
			LogLevel::Info);
		self->m_dave_manager->set_protocol_version(protocol_version);

		if (protocol_version == 0) {
			self->log("Transitioning to protocol version 0 (passthrough mode)",
					  LogLevel::Info);
			self->m_dave_manager->reset_session();
			std::move(h)(outcome::success());
			return;
		}

		self->maybe_start_mls_async(
			false, [self, h = std::move(h)](Result<> r) mutable {
				if (!r) {
					std::move(h)(r.error());
					return;
				}

				if (!self->m_dave_manager->install_sender_ratchet()) {
					self->log(
						"CRITICAL: Sender ratchet installation FAILED - "
						"forcing passthrough",
						LogLevel::Error);
					self->m_dave_manager->set_passthrough_mode(true);
					std::move(h)(boost::system::errc::operation_not_permitted);
					return;
				}

				self->log(
					"Sender ratchet successfully installed", LogLevel::Info);
				self->m_dave_manager->set_passthrough_mode(false);
				self->m_warned_waiting_for_e2ee = false;
				std::move(h)(outcome::success());
			});
	});
}

void VoiceConnection::Impl::recover_mls_after_invalid_transition_async(
	uint16_t transition_id, asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	send_mls_invalid_commit_welcome(
		transition_id, [self, h = std::move(h)](Result<> r) mutable {
			if (!r) {
				std::move(h)(r.error());
				return;
			}
			self->m_dave_manager->reset_session();
			self->maybe_start_mls_async(true, std::move(h));
		});
}

void VoiceConnection::Impl::send_mls_key_package(
	asio::any_completion_handler<void(Result<>)> h) {
	auto res = m_dave_manager->get_marshalled_key_package();
	if (!res) {
		asio::post(m_strand, [h = std::move(h), ec = res.error()]() mutable {
			std::move(h)(ec);
		});
		return;
	}
	send_voice_binary(
		VoiceOpcode::DaveMlsKeyPackage, res.value(), std::move(h));
}

void VoiceConnection::Impl::send_mls_commit_welcome(
	std::vector<std::byte> payload,
	asio::any_completion_handler<void(Result<>)> h) {
	send_voice_binary(
		VoiceOpcode::DaveMlsCommitWelcome,
		boost::span<const std::byte>(payload.data(), payload.size()),
		std::move(h));
}

void VoiceConnection::Impl::send_mls_invalid_commit_welcome(
	uint16_t transition_id, asio::any_completion_handler<void(Result<>)> h) {
	nlohmann::json j{
		{"op", static_cast<uint8_t>(VoiceOpcode::DaveMlsInvalidCommitWelcome)},
		{"d", {{"transition_id", transition_id}}}};
	send_voice_json(j, std::move(h));
}

uint64_t VoiceConnection::Impl::compute_group_id() const {
	if (auto r = parse_media_session_id_prefix_hex16(m_media_session_id)) {
		return r.value();
	}

	std::string s = m_media_session_id.empty()
						? fmt::to_string(*m_state.guild_id)
						: m_media_session_id;
	unsigned char out[crypto_generichash_BYTES];
	crypto_generichash(
		out, sizeof(out), reinterpret_cast<const unsigned char *>(s.data()),
		s.size(), nullptr, 0);

	uint64_t v = 0;
	for (int i = 0; i < 8; ++i) { v = (v << 8) | out[i]; }
	return v;
}

void VoiceConnection::Impl::log(std::string_view msg, LogLevel level) const {
	if (m_on_log) {
		m_on_log(Log{level, fmt::format("voice_connection{{ssrc={}}}: {}",
										m_ssrc, msg)});
	}
}

}  // namespace ekizu
