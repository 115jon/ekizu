#include <ekizu/json_util.hpp>
#include <ekizu/voice_connection.hpp>

#include "voice_connection_impl.hpp"
#include "voice_util.hpp"

namespace ekizu {

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

						case VoiceOpcode::Speaking: {
							// Track user SSRCs for voice receiving
							if (data["d"].contains("ssrc") &&
								data["d"].contains("user_id")) {
								uint32_t ssrc =
									data["d"]["ssrc"].get<uint32_t>();
								std::string user_id =
									data["d"]["user_id"].get<std::string>();
								bool speaking =
									data["d"].contains("speaking")
										? (data["d"]["speaking"].get<int>() !=
										   0)
										: false;

								me->impl->on_speaking(user_id, ssrc, speaking);
							}
							break;
						}

						case VoiceOpcode::ClientsConnect: {
							// Handle clients_connect opcode (11) - users
							// joining the voice session
							if (data["d"].contains("user_ids") &&
								data["d"]["user_ids"].is_array()) {
								for (const auto &user_id_json :
									 data["d"]["user_ids"]) {
									if (user_id_json.is_string()) {
										std::string user_id =
											user_id_json.get<std::string>();
										me->impl->m_recognized_user_ids.insert(
											user_id);
										me->impl->log(
											fmt::format(
												"User {} connected (now "
												"recognized for MLS)",
												user_id),
											LogLevel::Info);
									}
								}
							}
							break;
						}

						case VoiceOpcode::ClientDisconnect: {
							// Handle client_disconnect opcode (13) - user
							// leaving the voice session
							if (data["d"].contains("user_id") &&
								data["d"]["user_id"].is_string()) {
								std::string user_id =
									data["d"]["user_id"].get<std::string>();
								me->impl->m_recognized_user_ids.erase(user_id);
								me->impl->log(
									fmt::format("User {} disconnected (removed "
												"from recognized MLS users)",
												user_id),
									LogLevel::Info);
							}
							break;
						}

						case VoiceOpcode::DavePrepareTransition: {
							// Handle dave_protocol_prepare_transition opcode
							// (21) When transition_id = 0, this signals sole
							// member initialization
							if (data["d"].contains("transition_id") &&
								data["d"]["transition_id"].is_number()) {
								uint16_t transition_id =
									data["d"]["transition_id"].get<uint16_t>();

								if (transition_id == 0) {
									// Sole member reset: activate the pending
									// group immediately
									me->impl->log(
										"Received sole member init "
										"(transition_id=0), activating pending "
										"group",
										LogLevel::Info);

									if (me->impl->m_dave_manager
											->commit_pending_group()) {
										me->impl->log(
											"Successfully activated pending "
											"group for sole member",
											LogLevel::Info);

										// Execute the transition to install
										// sender ratchet
										me->impl
											->execute_dave_transition_now_async(
												me->impl->m_dave_manager
													->protocol_version(),
												[me](Result<>) mutable {
													me->step();
												});
										return;
									}
									me->impl->log(
										"Failed to activate pending group",
										LogLevel::Error);
								}
							}
							break;
						}

						case VoiceOpcode::DaveExecuteTransition: {
							// Handle dave_protocol_execute_transition opcode
							// (22) This is sent by Discord to instruct us to
							// execute a prepared epoch transition
							if (data["d"].contains("transition_id") &&
								data["d"]["transition_id"].is_number()) {
								uint16_t transition_id =
									data["d"]["transition_id"].get<uint16_t>();
								me->impl->log(
									fmt::format(
										"Executing DAVE transition for tid={}",
										transition_id),
									LogLevel::Info);

								// Execute the transition now
								me->impl->execute_dave_transition_now_async(
									me->impl->m_dave_manager
										->protocol_version(),
									[me](Result<>) mutable { me->step(); });
								return;
							}
							break;
						}

						case VoiceOpcode::DavePrepareEpoch: {
							// Handle dave_protocol_prepare_epoch opcode (24)
							// When epoch = 1, this signals a sole member reset
							if (data["d"].contains("epoch") &&
								data["d"]["epoch"].is_number() &&
								data["d"]["epoch"].get<int>() == 1) {
								me->impl->log(
									"Received sole member reset (epoch=1), "
									"resetting MLS session",
									LogLevel::Info);

								// Reset the MLS session as per DAVE spec
								me->impl->m_dave_manager->reset_session();

								// Re-initialize and send a new key package
								me->impl->maybe_start_mls_async(
									false,	// don't force reset (we just did
											// it)
									[me](Result<>) mutable { me->step(); });
								return;
							}
							break;
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

				uint16_t seq = voice::read_u16be(bytes);
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

}  // namespace ekizu