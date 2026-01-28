#include "session.hpp"

#include <algorithm>
#include <utility>

#include "parameters.hpp"
#include "user_credential.hpp"

namespace ekizu::dave {

Session::Session(KeyPairContextType context, std::string auth_session_id,
				 MlsFailureCallback callback) noexcept
	: m_failure_callback(std::move(callback)),
	  m_signing_key_id(std::move(auth_session_id)),
	  m_key_pair_context(context) {}

void Session::init(
	ProtocolVersion version, uint64_t group_id, uint64_t self_user_id,
	std::shared_ptr<mlspp::SignaturePrivateKey> &transient_key) noexcept {
	reset();

	m_bot_user_id = self_user_id;
	m_session_protocol_version = version;
	m_session_group_id = detail::big_endian_bytes_from(group_id).as_vec();

	init_leaf_node(self_user_id, transient_key);
	create_pending_group();
}

void Session::reset() noexcept {
	clear_pending_state();
	m_current_state.reset();
	m_outbound_cached_group_state.reset();
	m_session_protocol_version = 0;
	m_session_group_id.clear();
}

void Session::set_protocol_version(ProtocolVersion version) noexcept {
	m_session_protocol_version = version;
}

ProtocolVersion Session::get_protocol_version() const noexcept {
	return m_session_protocol_version;
}

std::vector<uint8_t> Session::get_last_epoch_authenticator() const noexcept {
	if (!m_current_state) { return {}; }
	return m_current_state->epoch_authenticator().as_vec();
}

void Session::set_external_sender(
	std::vector<uint8_t> const &external_sender_package) noexcept {
	try {
		if (m_current_state) { return; }

		m_mls_external_sender = std::make_unique<mlspp::ExternalSender>(
			mlspp::tls::get<mlspp::ExternalSender>(external_sender_package));

		if (!m_session_group_id.empty()) { create_pending_group(); }
	} catch (const std::exception &e) {
		m_logger.warn("set_external_sender failed: {}", e.what());
	} catch (...) {
		m_logger.warn("set_external_sender failed: unknown exception");
	}
}

bool Session::has_external_sender() const noexcept {
	return m_mls_external_sender != nullptr;
}

std::optional<std::vector<uint8_t>> Session::process_proposals(
	const std::vector<uint8_t> &proposals,
	std::set<uint64_t> const &recognised_user_ids) noexcept {
	try {
		if (!m_pending_group_state && !m_current_state) { return std::nullopt; }

		if (!m_state_with_proposals) {
			m_state_with_proposals = std::make_unique<mlspp::State>(
				m_pending_group_state ? *m_pending_group_state
									  : *m_current_state);
		}

		mlspp::tls::istream in_stream(proposals);

		bool is_revoke{};
		in_stream >> is_revoke;

		auto const suite = m_state_with_proposals->cipher_suite();

		if (is_revoke) {
			std::vector<mlspp::bytes_ns::bytes> refs;
			in_stream >> refs;

			for (auto const &ref : refs) {
				for (auto it = m_proposal_queue.begin();
					 it != m_proposal_queue.end(); it++) {
					if (it->ref == ref) {
						m_proposal_queue.erase(it);
						break;
					}
				}
			}

			m_state_with_proposals = std::make_unique<mlspp::State>(
				m_pending_group_state ? *m_pending_group_state
									  : *m_current_state);

			for (auto &prop : m_proposal_queue) {
				m_state_with_proposals->handle(prop.content);
			}
		} else {
			std::vector<mlspp::MLSMessage> messages;
			in_stream >> messages;

			for (auto const &proposal_message : messages) {
				auto validated_content =
					m_state_with_proposals->unwrap(proposal_message);

				if (!validate_proposal_message(
						validated_content.authenticated_content(),
						*m_state_with_proposals, recognised_user_ids)) {
					return std::nullopt;
				}

				m_state_with_proposals->handle(validated_content);

				auto ref = suite.ref(validated_content.authenticated_content());
				m_proposal_queue.push_back(
					{std::move(validated_content), std::move(ref)});
			}
		}

		auto commit_secret = mlspp::hpke::random_bytes(suite.secret_size());

		auto commit_options = mlspp::CommitOpts{{}, true, false, {}};

		auto [commit_message, welcome_message, new_state] =
			m_state_with_proposals->commit(commit_secret, commit_options, {});

		auto out_stream = mlspp::tls::ostream();
		out_stream << commit_message;

		m_pending_group_commit =
			std::make_unique<mlspp::MLSMessage>(std::move(commit_message));

		if (!welcome_message.secrets.empty()) { out_stream << welcome_message; }

		m_outbound_cached_group_state =
			std::make_unique<mlspp::State>(std::move(new_state));

		return out_stream.bytes();
	} catch (const std::exception &e) {
		m_logger.warn("process_proposals failed: {}", e.what());
		return std::nullopt;
	} catch (...) {
		m_logger.warn("process_proposals failed: unknown exception");
		return std::nullopt;
	}
}

RosterVariant Session::process_commit(
	const std::vector<uint8_t> &commit) noexcept {
	try {
		auto commit_message = mlspp::tls::get<mlspp::MLSMessage>(commit);

		if (!can_process_commit(commit_message)) { return Ignored{}; }

		std::optional<mlspp::State> optional_cached_state = std::nullopt;
		if (m_outbound_cached_group_state) {
			optional_cached_state = *m_outbound_cached_group_state;
		}

		auto new_state = m_state_with_proposals->handle(
			commit_message, optional_cached_state);
		if (!new_state) { return Failed{}; }

		RosterMap ret = replace_state(
			std::make_unique<mlspp::State>(std::move(*new_state)));

		m_outbound_cached_group_state.reset();
		clear_pending_state();

		return ret;
	} catch (const std::exception &e) {
		m_logger.warn("process_commit failed: {}", e.what());
		return Failed{};
	} catch (...) {
		m_logger.warn("process_commit failed: unknown exception");
		return Failed{};
	}
}

std::optional<RosterMap> Session::process_welcome(
	const std::vector<uint8_t> &welcome,
	std::set<uint64_t> const &recognised_user_ids) noexcept {
	try {
		if (!has_cryptographic_state_for_welcome()) { return std::nullopt; }
		if (!m_mls_external_sender) { return std::nullopt; }
		if (m_current_state) { return std::nullopt; }

		auto unmarshalled_welcome = mlspp::tls::get<mlspp::Welcome>(welcome);

		auto new_state = std::make_unique<mlspp::State>(
			*m_join_init_private_key, *m_hpke_private_key,
			*m_signature_private_key, *m_join_key_package, unmarshalled_welcome,
			std::nullopt,
			std::map<mlspp::bytes_ns::bytes, mlspp::bytes_ns::bytes>());

		if (!verify_welcome_state(*new_state, recognised_user_ids)) {
			return std::nullopt;
		}

		RosterMap ret = replace_state(std::move(new_state));
		clear_pending_state();

		return ret;
	} catch (const std::exception &e) {
		m_logger.warn("process_welcome failed: {}", e.what());
		return std::nullopt;
	} catch (...) {
		m_logger.warn("process_welcome failed: unknown exception");
		return std::nullopt;
	}
}

std::vector<uint8_t> Session::get_marshalled_key_package() noexcept {
	try {
		reset_join_key_package();
		if (!m_join_key_package) { return {}; }
		return mlspp::tls::marshal(*m_join_key_package);
	} catch (const std::exception &e) {
		m_logger.warn("get_marshalled_key_package failed: {}", e.what());
		return {};
	} catch (...) {
		m_logger.warn("get_marshalled_key_package failed: unknown exception");
		return {};
	}
}

std::vector<uint8_t> Session::get_key_ratchet_key(
	uint64_t user_id) const noexcept {
	if (!m_current_state) { return {}; }

	try {
		uint64_t u64_user_id = user_id;
		auto user_id_bytes = mlspp::bytes_ns::bytes(sizeof(u64_user_id));
		const auto *as_bytes = reinterpret_cast<const uint8_t *>(&u64_user_id);
		std::copy(
			as_bytes, as_bytes + sizeof(u64_user_id), user_id_bytes.data());

		auto secret = m_current_state->do_export(
			Session::USER_MEDIA_KEY_BASE_LABEL, user_id_bytes,
			detail::AES_GCM_128_KEY_BYTES);

		return secret.as_vec();
	} catch (const std::exception &e) {
		m_logger.warn(
			"get_key_ratchet_key failed for user {}: {}", user_id, e.what());
		return {};
	} catch (...) {
		m_logger.warn(
			"get_key_ratchet_key failed for user {}: unknown exception",
			user_id);
		return {};
	}
}

std::unique_ptr<KeyRatchetInterface> Session::get_key_ratchet(
	uint64_t user_id) const noexcept {
	if (!m_current_state) { return nullptr; }

	try {
		uint64_t u64_user_id = user_id;
		auto user_id_bytes = mlspp::bytes_ns::bytes(sizeof(u64_user_id));
		const auto *as_bytes = reinterpret_cast<const uint8_t *>(&u64_user_id);
		std::copy(
			as_bytes, as_bytes + sizeof(u64_user_id), user_id_bytes.data());

		auto secret = m_current_state->do_export(
			Session::USER_MEDIA_KEY_BASE_LABEL, user_id_bytes,
			detail::AES_GCM_128_KEY_BYTES);

		// Return proper MlsKeyRatchet with cipher suite for correct key
		// derivation
		return std::make_unique<detail::MlsKeyRatchet>(
			m_current_state->cipher_suite(), std::move(secret));
	} catch (const std::exception &e) {
		m_logger.warn(
			"get_key_ratchet failed for user {}: {}", user_id, e.what());
		return nullptr;
	} catch (...) {
		m_logger.warn(
			"get_key_ratchet failed for user {}: unknown exception", user_id);
		return nullptr;
	}
}

bool Session::is_initialized() const noexcept {
	return !m_session_group_id.empty();
}

bool Session::has_current_state() const noexcept {
	return m_current_state != nullptr;
}

void Session::init_leaf_node(
	uint64_t self_user_id,
	std::shared_ptr<mlspp::SignaturePrivateKey> &transient_key) noexcept {
	try {
		auto ciphersuite = detail::ciphersuite_for_protocol_version(
			m_session_protocol_version);

		if (!transient_key) {
			if (!m_signing_key_id.empty()) {
				transient_key = get_persisted_key_pair(
					m_key_pair_context, m_signing_key_id,
					m_session_protocol_version);
				if (!transient_key) {
					m_logger.warn(
						"Did not receive MLS signature private key from "
						"get_persisted_key_pair; aborting");
					return;
				}
			} else {
				transient_key = std::make_shared<mlspp::SignaturePrivateKey>(
					mlspp::SignaturePrivateKey::generate(ciphersuite));
			}
		}

		m_signature_private_key = transient_key;

		auto self_credential = detail::create_user_credential(
			std::to_string(self_user_id), m_session_protocol_version);

		m_hpke_private_key = std::make_unique<mlspp::HPKEPrivateKey>(
			mlspp::HPKEPrivateKey::generate(ciphersuite));

		m_self_leaf_node = std::make_unique<mlspp::LeafNode>(
			ciphersuite, m_hpke_private_key->public_key,
			m_signature_private_key->public_key, std::move(self_credential),
			detail::leaf_node_capabilities_for_protocol_version(
				m_session_protocol_version),
			mlspp::Lifetime::create_default(),
			detail::leaf_node_extensions_for_protocol_version(
				m_session_protocol_version),
			*m_signature_private_key);
	} catch (const std::exception &e) {
		m_logger.warn("init_leaf_node failed: {}", e.what());
	} catch (...) { m_logger.warn("init_leaf_node failed: unknown exception"); }
}

void Session::reset_join_key_package() noexcept {
	try {
		if (!m_self_leaf_node) { return; }

		auto ciphersuite = detail::ciphersuite_for_protocol_version(
			m_session_protocol_version);

		m_join_init_private_key = std::make_unique<mlspp::HPKEPrivateKey>(
			mlspp::HPKEPrivateKey::generate(ciphersuite));

		m_join_key_package = std::make_unique<mlspp::KeyPackage>(
			ciphersuite, m_join_init_private_key->public_key, *m_self_leaf_node,
			detail::leaf_node_extensions_for_protocol_version(
				m_session_protocol_version),
			*m_signature_private_key);
	} catch (const std::exception &e) {
		m_logger.warn("reset_join_key_package failed: {}", e.what());
	} catch (...) {
		m_logger.warn("reset_join_key_package failed: unknown exception");
	}
}

void Session::create_pending_group() noexcept {
	try {
		if (m_session_group_id.empty()) { return; }
		if (!m_mls_external_sender) { return; }
		if (!m_self_leaf_node) { return; }

		auto ciphersuite = detail::ciphersuite_for_protocol_version(
			m_session_protocol_version);

		m_pending_group_state = std::make_unique<mlspp::State>(
			m_session_group_id, ciphersuite, *m_hpke_private_key,
			*m_signature_private_key, *m_self_leaf_node,
			detail::group_extensions_for_protocol_version(
				m_session_protocol_version, *m_mls_external_sender));
	} catch (const std::exception &e) {
		m_logger.warn("create_pending_group failed: {}", e.what());
	} catch (...) {
		m_logger.warn("create_pending_group failed: unknown exception");
	}
}

bool Session::has_cryptographic_state_for_welcome() const noexcept {
	return m_join_key_package && m_join_init_private_key &&
		   m_signature_private_key && m_hpke_private_key;
}

bool Session::is_recognized_user_id(
	mlspp::Credential const &cred,
	std::set<uint64_t> const &recognised_user_ids) const {
	std::string uid_str =
		detail::user_credential_to_string(cred, m_session_protocol_version);
	if (uid_str.empty()) { return false; }

	try {
		uint64_t uid = std::stoull(uid_str);
		return recognised_user_ids.find(uid) != recognised_user_ids.end();
	} catch (const std::exception &e) {
		m_logger.debug("is_recognized_user_id parse failed: {}", e.what());
		return false;
	} catch (...) {
		m_logger.debug("is_recognized_user_id parse failed: unknown exception");
		return false;
	}
}

bool Session::validate_proposal_message(
	mlspp::AuthenticatedContent const &message,
	mlspp::State const &target_state,
	std::set<uint64_t> const &recognised_user_ids) const {
	if (message.wire_format != mlspp::WireFormat::mls_public_message) {
		return false;
	}

	if (message.content.epoch != target_state.epoch()) { return false; }

	if (message.content.content_type() != mlspp::ContentType::proposal) {
		return false;
	}

	if (message.content.sender.sender_type() != mlspp::SenderType::external) {
		return false;
	}

	auto const &proposal =
		mlspp::tls::var::get<mlspp::Proposal>(message.content.content);

	switch (proposal.proposal_type()) {
		case mlspp::ProposalType::add: {
			auto const &credential =
				mlspp::tls::var::get<mlspp::Add>(proposal.content)
					.key_package.leaf_node.credential;
			if (!is_recognized_user_id(credential, recognised_user_ids)) {
				return false;
			}
			break;
		}
		case mlspp::ProposalType::remove: break;
		default: return false;
	}

	return true;
}

bool Session::can_process_commit(mlspp::MLSMessage const &commit) noexcept {
	if (!m_state_with_proposals) { return false; }
	if (commit.group_id() != m_session_group_id) { return false; }
	return true;
}

bool Session::verify_welcome_state(
	mlspp::State const &state,
	std::set<uint64_t> const &recognised_user_ids) const {
	if (!m_mls_external_sender) { return false; }

	auto ext = state.extensions().find<mlspp::ExternalSendersExtension>();
	if (!ext) { return false; }
	if (ext->senders.size() != 1) { return false; }
	if (ext->senders.front() != *m_mls_external_sender) { return false; }

	for (auto const &leaf : state.roster()) {
		(void)is_recognized_user_id(leaf.credential, recognised_user_ids);
	}

	return true;
}

RosterMap Session::replace_state(std::unique_ptr<mlspp::State> &&state) {
	RosterMap new_roster;
	for (mlspp::LeafNode const &node : state->roster()) {
		if (node.credential.type() != mlspp::CredentialType::basic) {
			continue;
		}

		auto const &cred = node.credential.get<mlspp::BasicCredential>();
		new_roster[detail::from_big_endian_bytes(cred.identity)] =
			node.signature_key.data.as_vec();
	}

	RosterMap change_map;
	std::set_difference(
		new_roster.begin(), new_roster.end(), m_roster.begin(), m_roster.end(),
		std::inserter(change_map, change_map.end()));

	struct MissingItemWrapper {
		RosterMap &map;
		using iterator = RosterMap::iterator;
		using const_iterator = RosterMap::const_iterator;
		using value_type = RosterMap::value_type;

		iterator insert(const_iterator it, value_type const &value) {
			return map.try_emplace(
				std::move(it), value.first, std::vector<uint8_t>{});
		}

		iterator begin() { return map.begin(); }
		iterator end() { return map.end(); }
	};

	MissingItemWrapper wrapper{change_map};
	std::set_difference(
		m_roster.begin(), m_roster.end(), new_roster.begin(), new_roster.end(),
		std::inserter(wrapper, wrapper.end()));

	m_roster = std::move(new_roster);
	m_current_state = std::move(state);

	return change_map;
}

void Session::clear_pending_state() {
	m_pending_group_state.reset();
	m_pending_group_commit.reset();
	m_join_init_private_key.reset();
	m_join_key_package.reset();
	m_hpke_private_key.reset();
	m_self_leaf_node.reset();
	m_state_with_proposals.reset();
	m_proposal_queue.clear();
}

}  // namespace ekizu::dave
