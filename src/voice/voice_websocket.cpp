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
					me->impl->m_logger.error(msg);

					// Check if we can resume based on close code
					uint16_t close_code =
						close_reason ? static_cast<uint16_t>(close_reason->code)
									 : 1006;  // Abnormal closure

					if (is_resumable_close_code(close_code) &&
						me->impl->m_connection_state ==
							VoiceConnectionState::Ready) {
						me->impl->initiate_reconnect();
					} else {
						me->impl->m_connection_state =
							VoiceConnectionState::Closed;
						me->impl->m_disconnected = true;
					}
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
							me->impl->m_disconnected = false;
							me->impl->m_connection_state =
								VoiceConnectionState::Ready;
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

							// Check if we're resuming or identifying
							const bool resuming =
								(me->impl->m_connection_state ==
								 VoiceConnectionState::Resuming);

							nlohmann::json id_payload;
							if (resuming) {
								me->impl->m_logger.info("Sending Resume");
								id_payload = {
									{"op",
									 static_cast<uint8_t>(VoiceOpcode::Resume)},
									{"d",
									 {{"server_id",
									   fmt::to_string(
										   *me->impl->m_state.guild_id)},
									  {"session_id",
									   me->impl->m_state.session_id},
									  {"token", me->impl->m_token},
									  {"seq_ack", me->impl->m_last_seq}}}};
							} else {
								me->impl->m_logger.info("Sending Identify");
								me->impl->m_connection_state =
									VoiceConnectionState::Identifying;
								id_payload = {
									{"op", static_cast<uint8_t>(
											   VoiceOpcode::Identify)},
									{"d",
									 {{"server_id",
									   fmt::to_string(
										   *me->impl->m_state.guild_id)},
									  {"user_id",
									   fmt::to_string(
										   me->impl->m_state.user_id)},
									  {"session_id",
									   me->impl->m_state.session_id},
									  {"token", me->impl->m_token},
									  {"max_dave_protocol_version",
									   dave::max_protocol_version()}}}};
							}

							me->impl->m_ws->send(
								id_payload.dump(),
								[me](Result<>) mutable { me->step(); });
							return;
						}

						case VoiceOpcode::Resumed:
							me->impl->m_logger.info("Resumed successfully");
							me->impl->m_connection_state =
								VoiceConnectionState::Ready;
							me->impl->m_missed_heartbeats = 0;
							me->impl->m_disconnected = false;
							break;

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
										me->impl->m_logger.info(
											"User {} connected (now recognized "
											"for MLS)",
											user_id);
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
								me->impl->m_logger.info(
									"User {} disconnected (removed from "
									"recognized MLS users)",
									user_id);
							}
							break;
						}

						case VoiceOpcode::DavePrepareTransition: {
							// Handle dave_protocol_prepare_transition opcode
							// (21)
							if (!data["d"].contains("transition_id") ||
								!data["d"]["transition_id"].is_number()) {
								break;
							}

							uint16_t transition_id =
								data["d"]["transition_id"].get<uint16_t>();
							int protocol_version =
								data["d"].contains("protocol_version")
									? data["d"]["protocol_version"].get<int>()
									: (me->impl->m_dave
										   ? static_cast<int>(
												 me->impl->m_dave
													 ->protocol_version)
										   : 0);

							me->impl->m_logger.info(
								"Received DavePrepareTransition: tid={}, "
								"version={}",
								transition_id, protocol_version);

							// Store pending transition version
							me->impl
								->m_dave_transition_versions[transition_id] =
								protocol_version;

							// transition_id = 0 means immediate execution
							if (transition_id == 0) {
								// For sole member, session already ready
								if (me->impl->m_dave &&
									me->impl->m_dave->session &&
									me->impl->m_dave->session
										->has_current_state()) {
									me->impl->m_logger.info(
										"Activated pending group for sole "
										"member");
									me->impl->execute_dave_transition_now_async(
										protocol_version,
										[me](Result<>) mutable { me->step(); });
									return;
								}
								me->impl->m_logger.error(
									"Failed to activate pending group");
								break;
							}

							// Downgrade to transport-only encryption
							if (protocol_version == 0) {
								me->impl->m_logger.info(
									"Preparing for downgrade to passthrough");
								// Enable passthrough on receive side now
								if (me->impl->m_dave &&
									me->impl->m_dave->encryptor) {
									me->impl->m_dave->encryptor
										->set_passthrough_mode(true);
								}
							}

							// Send ready_for_transition acknowledgement
							nlohmann::json ready{
								{"op", static_cast<uint8_t>(
										   VoiceOpcode::DaveTransitionReady)},
								{"d", {{"transition_id", transition_id}}}};
							me->impl->m_ws->send(
								ready.dump(),
								[me](Result<>) mutable { me->step(); });
							return;
						}

						case VoiceOpcode::DaveExecuteTransition: {
							// Handle dave_protocol_execute_transition opcode
							// (22) This is sent by Discord to instruct us to
							// execute a prepared epoch transition
							if (!data["d"].contains("transition_id") ||
								!data["d"]["transition_id"].is_number()) {
								break;
							}

							uint16_t transition_id =
								data["d"]["transition_id"].get<uint16_t>();

							// Look up the protocol version from prepare phase
							int protocol_version =
								me->impl->m_dave
									? static_cast<int>(
										  me->impl->m_dave->protocol_version)
									: 0;
							auto it = me->impl->m_dave_transition_versions.find(
								transition_id);
							if (it !=
								me->impl->m_dave_transition_versions.end()) {
								protocol_version = it->second;
								me->impl->m_dave_transition_versions.erase(it);
							}

							me->impl->m_logger.info(
								"Executing DAVE transition: tid={}, version={}",
								transition_id, protocol_version);

							me->impl->execute_dave_transition_now_async(
								protocol_version,
								[me](Result<>) mutable { me->step(); });
							return;
						}

						case VoiceOpcode::DavePrepareEpoch: {
							// Handle dave_protocol_prepare_epoch opcode (24)
							// When epoch = 1, this signals we need to create a
							// new MLS group for the given protocol version
							if (!data["d"].contains("epoch") ||
								!data["d"]["epoch"].is_number() ||
								data["d"]["epoch"].get<int>() != 1) {
								break;
							}

							// Extract protocol_version for the new epoch
							int protocol_version = 1;  // Default to 1
							if (data["d"].contains("protocol_version") &&
								data["d"]["protocol_version"].is_number()) {
								protocol_version =
									data["d"]["protocol_version"].get<int>();
							}

							me->impl->m_logger.info(
								"Received DavePrepareEpoch: epoch=1, "
								"version={} - creating new MLS group",
								protocol_version);

							// Set the new protocol version BEFORE reset
							if (me->impl->m_dave && me->impl->m_dave->session) {
								me->impl->m_dave->protocol_version =
									static_cast<dave::ProtocolVersion>(
										protocol_version);
								me->impl->m_dave->session->set_protocol_version(
									static_cast<dave::ProtocolVersion>(
										protocol_version));

								// Reset the MLS session
								me->impl->m_dave->session->reset();
							}

							// Re-initialize and send a new key package
							me->impl->maybe_start_mls_async(
								false, [me](Result<>) mutable { me->step(); });
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
