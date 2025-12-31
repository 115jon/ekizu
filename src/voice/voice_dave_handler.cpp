#include <sodium/crypto_generichash.h>

#include <boost/charconv/from_chars.hpp>
#include <ekizu/json_util.hpp>
#include <ekizu/voice_connection.hpp>

#include "voice_connection_impl.hpp"
#include "voice_util.hpp"

namespace ekizu {

static Result<std::uint64_t> parse_media_session_id_prefix_hex16(
	std::string_view s) {
	if (s.size() < 16) {
		return make_error_code(boost::system::errc::invalid_argument);
	}

	std::uint64_t v = 0;
	const char *first = s.data();
	const char *last = first + 16;

	auto r = boost::charconv::from_chars(first, last, v, 16);

	if (r.ec == std::errc{} && r.ptr == last) { return v; }

	if (r.ec == std::errc::result_out_of_range) {
		return make_error_code(boost::system::errc::result_out_of_range);
	}

	return make_error_code(boost::system::errc::invalid_argument);
}

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
							"process_proposals failed; resetting MLS session",
							LogLevel::Warn);

						self->maybe_start_mls_async(
							true, [self, h = std::move(h)](Result<> r) mutable {
								if (!r) {
									std::move(h)(r.error());
									return;
								}
								std::move(h)(outcome::success());
							});
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
							auto p = payload;
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

				uint16_t tid = voice::read_u16be(payload_span);
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

				uint16_t tid = voice::read_u16be(payload_span);
				auto recognized = self->m_recognized_user_ids;
				recognized.insert(fmt::to_string(self->m_state.user_id));

				if (!self->m_dave_manager->process_welcome(
						payload_span.subspan(2), recognized)) {
					self->recover_mls_after_invalid_transition_async(
						tid, std::move(h));
					return;
				}

				self->log(
					"Welcome processed successfully - installing receiver "
					"ratchets",
					LogLevel::Info);

				int installed = 0;
				int failed = 0;
				for (const auto &user_id : recognized) {
					// Don't install receiver ratchet for yourself
					if (user_id == fmt::to_string(self->m_state.user_id)) {
						continue;
					}

					if (self->m_dave_manager->install_receiver_ratchet(
							user_id)) {
						installed++;
						self->log(fmt::format(
									  "Installed receiver ratchet for user {}",
									  user_id),
								  LogLevel::Info);
					} else {
						failed++;
						self->log(fmt::format("âŒ Failed to install receiver "
											  "ratchet for user {}",
											  user_id),
								  LogLevel::Warn);
					}
				}

				self->log(
					fmt::format("Receiver ratchets installed: {}, failed: {}",
								installed, failed),
					LogLevel::Info);

				nlohmann::json ready{
					{"op",
					 static_cast<uint8_t>(VoiceOpcode::DaveTransitionReady)},
					{"d", {{"transition_id", tid}}}};

				// Send ready and wait for Discord to send DaveExecuteTransition
				self->send_voice_json(ready, std::move(h));
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

}  // namespace ekizu