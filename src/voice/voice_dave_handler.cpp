#include <boost/charconv/from_chars.hpp>
#include <ekizu/json_util.hpp>
#include <ekizu/voice_connection.hpp>

#include "../dave/displayable_code.hpp"
#include "voice_connection_impl.hpp"
#include "voice_util.hpp"

namespace ekizu {

namespace {

std::set<uint64_t> convert_to_numeric_ids(
	const std::set<std::string> &string_ids) {
	std::set<uint64_t> result;
	for (const auto &id : string_ids) {
		try {
			result.insert(std::stoull(id));
		} catch (...) {}
	}
	return result;
}

}  // namespace

void VoiceConnection::Impl::handle_binary_event_async(
	VoiceOpcode op, std::vector<std::byte> payload, uint16_t /*seq*/,
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, op, payload = std::move(payload),
							  h = std::move(h)]() mutable {
		auto payload_span =
			boost::span<const std::byte>(payload.data(), payload.size());

		if (!self->m_dave || !self->m_dave->session) {
			std::move(h)(outcome::success());
			return;
		}

		switch (op) {
			case VoiceOpcode::DaveMlsExternalSender: {
				self->m_logger.info("Received dave_mls_external_sender");
				std::vector<uint8_t> sender_data(
					reinterpret_cast<const uint8_t *>(payload.data()),
					reinterpret_cast<const uint8_t *>(payload.data()) +
						payload.size());
				self->m_dave->session->set_external_sender(sender_data);
				self->maybe_start_mls_async(false, std::move(h));
				return;
			}

			case VoiceOpcode::DaveMlsProposals: {
				self->m_logger.info("Received dave_mls_proposals");

				auto recognized =
					convert_to_numeric_ids(self->m_recognized_user_ids);
				recognized.insert(self->m_state.user_id.id);

				std::vector<uint8_t> proposals_data(
					reinterpret_cast<const uint8_t *>(payload.data()),
					reinterpret_cast<const uint8_t *>(payload.data()) +
						payload.size());
				auto res = self->m_dave->session->process_proposals(
					proposals_data, recognized);

				self->m_logger.info("process_proposals result: {}",
									res.has_value() ? "success" : "failure");

				if (!res) {
					self->m_logger.warn(
						"process_proposals failed; ignoring proposal");
					std::move(h)(outcome::success());
					return;
				}

				const auto &commit_msg = res.value();
				std::vector<std::byte> commit_vec(
					reinterpret_cast<const std::byte *>(commit_msg.data()),
					reinterpret_cast<const std::byte *>(commit_msg.data()) +
						commit_msg.size());

				self->m_logger.info(
					"Sending mls_commit_message (size={})", commit_vec.size());

				self->send_mls_commit_welcome(
					std::move(commit_vec), std::move(h));
				return;
			}

			case VoiceOpcode::DaveMlsAnnounceCommitTransition: {
				if (payload_span.size() < 2) {
					std::move(h)(outcome::success());
					return;
				}

				uint16_t tid = voice::read_u16be(payload_span);
				auto commit_data = payload_span.subspan(2);

				std::vector<uint8_t> commit_vec(
					reinterpret_cast<const uint8_t *>(commit_data.data()),
					reinterpret_cast<const uint8_t *>(commit_data.data()) +
						commit_data.size());

				auto result = self->m_dave->session->process_commit(commit_vec);

				if (std::holds_alternative<dave::RosterMap>(result)) {
					auto &roster = std::get<dave::RosterMap>(result);
					self->m_dave->cached_roster_map = roster;

					nlohmann::json ready{
						{"op", static_cast<uint8_t>(
								   VoiceOpcode::DaveTransitionReady)},
						{"d", {{"transition_id", tid}}}};

					if (tid == 0) {
						self->send_voice_json(
							ready,
							[self, h = std::move(h)](Result<> r) mutable {
								if (!r) {
									std::move(h)(r.error());
									return;
								}
								self->execute_dave_transition_now_async(
									static_cast<int>(
										self->m_dave->protocol_version),
									std::move(h));
							});
						return;
					}

					self->send_voice_json(ready, std::move(h));
					return;
				}

				self->m_logger.warn("Failed to process commit (tid={})", tid);
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
				auto recognized =
					convert_to_numeric_ids(self->m_recognized_user_ids);
				recognized.insert(self->m_state.user_id.id);

				std::vector<uint8_t> welcome_data(
					reinterpret_cast<const uint8_t *>(payload_span.data() + 2),
					reinterpret_cast<const uint8_t *>(payload_span.data()) +
						payload_span.size());

				auto welcome_result = self->m_dave->session->process_welcome(
					welcome_data, recognized);

				if (!welcome_result) {
					self->recover_mls_after_invalid_transition_async(
						tid, std::move(h));
					return;
				}

				self->m_logger.info(
					"Welcome processed - installing receiver ratchets");

				// Install decryptors for all users in the roster
				int installed{};
				for (const auto &[user_id, _] : *welcome_result) {
					if (user_id == self->m_state.user_id.id) { continue; }

					auto ratchet =
						self->m_dave->session->get_key_ratchet(user_id);
					if (ratchet) {
						auto decryptor = std::make_unique<dave::Decryptor>();
						decryptor->transition_to_key_ratchet(
							std::move(ratchet));
						self->m_dave->decryptors[std::to_string(user_id)] =
							std::move(decryptor);
						installed++;
					}
				}

				self->m_logger.info(
					"Installed {} receiver decryptors", installed);

				nlohmann::json ready{
					{"op",
					 static_cast<uint8_t>(VoiceOpcode::DaveTransitionReady)},
					{"d", {{"transition_id", tid}}}};

				if (tid == 0) {
					self->send_voice_json(ready, [self, h = std::move(h)](
													 Result<> r) mutable {
						if (!r) {
							std::move(h)(r.error());
							return;
						}
						self->execute_dave_transition_now_async(
							static_cast<int>(self->m_dave->protocol_version),
							std::move(h));
					});
					return;
				}

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
		if (!self->m_dave || self->m_dave->protocol_version <= 0) {
			std::move(h)(outcome::success());
			return;
		}

		if (force_reset && self->m_dave->session) {
			self->m_dave->session->reset();
		}

		if (self->m_dave->session && self->m_dave->session->is_initialized() &&
			!force_reset) {
			std::move(h)(outcome::success());
			return;
		}

		if (!self->m_dave->session ||
			!self->m_dave->session->has_external_sender()) {
			std::move(h)(outcome::success());
			return;
		}

		// Initialize session
		self->m_dave->session->init(
			self->m_dave->protocol_version, self->compute_group_id(),
			self->m_state.user_id.id, self->m_dave->transient_key);

		// Reset encryptor logic to ensure fresh state/nonces
		self->m_dave->encryptor = std::make_unique<dave::Encryptor>();
		self->m_dave->encryptor->assign_ssrc_to_codec(
			self->m_ssrc, dave::Codec::Opus);
		self->m_dave->encryptor->set_passthrough_mode(true);

		self->send_mls_key_package(std::move(h));
	});
}

void VoiceConnection::Impl::execute_dave_transition_now_async(
	int protocol_version, asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, protocol_version,
							  h = std::move(h)]() mutable {
		self->m_logger.info("Executing DAVE transition to protocol version {}",
							protocol_version);

		if (!self->m_dave) {
			std::move(h)(outcome::success());
			return;
		}

		self->m_dave->done_ready = false;
		self->m_dave->protocol_version =
			static_cast<dave::ProtocolVersion>(protocol_version);
		if (self->m_dave->session) {
			self->m_dave->session->set_protocol_version(
				static_cast<dave::ProtocolVersion>(protocol_version));
		}

		if (protocol_version == 0) {
			self->m_logger.info(
				"Transitioning to protocol version 0 (passthrough mode)");
			if (self->m_dave->session) { self->m_dave->session->reset(); }
			if (self->m_dave->encryptor) {
				self->m_dave->encryptor->set_passthrough_mode(true);
			}
			std::move(h)(outcome::success());
			return;
		}

		self->maybe_start_mls_async(false, [self, h = std::move(h)](
											   Result<> r) mutable {
			if (!r) {
				std::move(h)(r.error());
				return;
			}

			// Validate state
			if (!self->m_dave || !self->m_dave->session ||
				!self->m_dave->encryptor) {
				std::move(h)(boost::system::errc::operation_not_permitted);
				return;
			}

			self->update_ratchets();

			if (!self->m_dave->encryptor->has_key_ratchet()) {
				self->m_logger.error("CRITICAL: Failed to get sender ratchet");
				self->m_dave->encryptor->set_passthrough_mode(true);
				std::move(h)(boost::system::errc::operation_not_permitted);
				return;
			}

			self->m_dave->encryptor->set_passthrough_mode(false);
			self->m_warned_waiting_for_e2ee = false;

			self->m_logger.info("Sender ratchet successfully installed");
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
			if (self->m_dave && self->m_dave->session) {
				self->m_dave->session->reset();
			}
			self->maybe_start_mls_async(true, std::move(h));
		});
}

void VoiceConnection::Impl::send_mls_key_package(
	asio::any_completion_handler<void(Result<>)> h) {
	if (!m_dave || !m_dave->session) {
		asio::post(m_strand, [h = std::move(h)]() mutable {
			std::move(h)(boost::system::errc::operation_not_permitted);
		});
		return;
	}

	auto key_package = m_dave->session->get_marshalled_key_package();
	if (key_package.empty()) {
		asio::post(m_strand, [h = std::move(h)]() mutable {
			std::move(h)(boost::system::errc::operation_not_permitted);
		});
		return;
	}

	std::vector<std::byte> key_pkg_bytes(
		reinterpret_cast<const std::byte *>(key_package.data()),
		reinterpret_cast<const std::byte *>(key_package.data()) +
			key_package.size());

	m_logger.info("Sending DaveMlsKeyPackage (size={})", key_pkg_bytes.size());

	send_voice_binary(VoiceOpcode::DaveMlsKeyPackage,
					  boost::span<const std::byte>(
						  key_pkg_bytes.data(), key_pkg_bytes.size()),
					  std::move(h));
}

void VoiceConnection::Impl::send_mls_commit_welcome(
	std::vector<std::byte> payload,
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	send_voice_binary(
		VoiceOpcode::DaveMlsCommitWelcome,
		boost::span<const std::byte>(payload.data(), payload.size()),
		[self, h = std::move(h)](Result<> r) mutable {
			if (r) {
				self->m_logger.info("Successfully sent DaveMlsCommitWelcome");
				std::move(h)(outcome::success());
			} else {
				self->m_logger.error("Failed to send DaveMlsCommitWelcome: {}",
									 r.error().message());
				std::move(h)(r.error());
			}
		});
}

void VoiceConnection::Impl::send_mls_invalid_commit_welcome(
	uint16_t transition_id, asio::any_completion_handler<void(Result<>)> h) {
	nlohmann::json j{
		{"op", static_cast<uint8_t>(VoiceOpcode::DaveMlsInvalidCommitWelcome)},
		{"d", {{"transition_id", transition_id}}}};
	send_voice_json(j, std::move(h));
}

uint64_t VoiceConnection::Impl::compute_group_id() const {
	return static_cast<uint64_t>(m_state.channel_id->id);
}

void VoiceConnection::Impl::update_ratchets() {
	// Whenever a new user joins or a user leaves, this invalidates all old
	// ratchets and they are replaced with new ones.

	if (!m_dave || !m_dave->session) { return; }

	constexpr auto ratchet_expiry = std::chrono::seconds(10);

	m_logger.debug(
		"Updating MLS ratchets for {} user(s)", m_dave_user_list.size() + 1);

	// Update decryptors for all users in the roster
	for (const auto &user_id : m_dave_user_list) {
		if (user_id == m_state.user_id.id) { continue; }

		auto user_id_str = std::to_string(user_id);
		auto it = m_dave->decryptors.find(user_id_str);
		if (it == m_dave->decryptors.end()) {
			// New user - create decryptor
			m_logger.debug(
				"Inserting decryptor key ratchet for NEW user: {}, protocol "
				"version: {}",
				user_id, m_dave->session->get_protocol_version());
			auto [iter, inserted] = m_dave->decryptors.emplace(
				user_id_str, std::make_unique<dave::Decryptor>());
			it = iter;
		}

		// Update the ratchet for this user (new or existing)
		auto ratchet = m_dave->session->get_key_ratchet(user_id);
		if (ratchet) {
			it->second->transition_to_key_ratchet(
				std::move(ratchet), ratchet_expiry);
		}
	}

	// Update encryptor if present
	if (m_dave->encryptor) {
		m_logger.debug("Setting key ratchet for sending audio...");
		auto sender_ratchet =
			m_dave->session->get_key_ratchet(m_state.user_id.id);
		if (sender_ratchet) {
			m_dave->encryptor->set_key_ratchet(std::move(sender_ratchet));
		}
	}

	// Update privacy code from epoch authenticator
	std::string old_code = m_dave->privacy_code;
	auto authenticator = m_dave->session->get_last_epoch_authenticator();
	m_dave->privacy_code = dave::generate_displayable_code(authenticator);

	if (!m_dave->privacy_code.empty() && m_dave->privacy_code != old_code) {
		m_logger.info("New E2EE Privacy Code: {}", m_dave->privacy_code);
	}
}

}  // namespace ekizu