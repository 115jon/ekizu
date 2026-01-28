#ifndef DAVE_SESSION_HPP
#define DAVE_SESSION_HPP

#include <mls/crypto.h>
#include <mls/messages.h>
#include <mls/state.h>

#include <ekizu/logger.hpp>
#include <ekizu/result.hpp>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "key_ratchet.hpp"
#include "persisted_key_pair.hpp"
#include "types.hpp"

namespace ekizu::dave {

struct QueuedProposal {
	mlspp::ValidatedContent content;
	mlspp::bytes_ns::bytes ref;
};

struct Session {
	using MlsFailureCallback =
		std::function<void(std::string const &, std::string const &)>;

	Session(KeyPairContextType context, std::string auth_session_id,
			MlsFailureCallback callback = nullptr) noexcept;

	void init(
		ProtocolVersion version, uint64_t group_id, uint64_t self_user_id,
		std::shared_ptr<mlspp::SignaturePrivateKey> &transient_key) noexcept;

	void reset() noexcept;

	void set_protocol_version(ProtocolVersion version) noexcept;

	[[nodiscard]] ProtocolVersion get_protocol_version() const noexcept;

	[[nodiscard]] std::vector<uint8_t> get_last_epoch_authenticator()
		const noexcept;

	void set_external_sender(
		std::vector<uint8_t> const &external_sender_package) noexcept;

	[[nodiscard]] bool has_external_sender() const noexcept;

	std::optional<std::vector<uint8_t>> process_proposals(
		const std::vector<uint8_t> &proposals,
		std::set<uint64_t> const &recognised_user_ids) noexcept;

	RosterVariant process_commit(const std::vector<uint8_t> &commit) noexcept;

	std::optional<RosterMap> process_welcome(
		const std::vector<uint8_t> &welcome,
		std::set<uint64_t> const &recognised_user_ids) noexcept;

	std::vector<uint8_t> get_marshalled_key_package() noexcept;

	[[nodiscard]] std::vector<uint8_t> get_key_ratchet_key(
		uint64_t user_id) const noexcept;

	[[nodiscard]] std::unique_ptr<KeyRatchetInterface> get_key_ratchet(
		uint64_t user_id) const noexcept;

	[[nodiscard]] bool is_initialized() const noexcept;

	[[nodiscard]] bool has_current_state() const noexcept;

   private:
	void init_leaf_node(
		uint64_t self_user_id,
		std::shared_ptr<mlspp::SignaturePrivateKey> &transient_key) noexcept;
	void reset_join_key_package() noexcept;
	void create_pending_group() noexcept;

	[[nodiscard]] bool has_cryptographic_state_for_welcome() const noexcept;

	[[nodiscard]] bool is_recognized_user_id(
		mlspp::Credential const &cred,
		std::set<uint64_t> const &recognised_user_ids) const;

	[[nodiscard]] bool validate_proposal_message(
		mlspp::AuthenticatedContent const &message,
		mlspp::State const &target_state,
		std::set<uint64_t> const &recognised_user_ids) const;

	[[nodiscard]] bool verify_welcome_state(
		mlspp::State const &state,
		std::set<uint64_t> const &recognised_user_ids) const;

	[[nodiscard]] bool can_process_commit(
		mlspp::MLSMessage const &commit) noexcept;

	RosterMap replace_state(std::unique_ptr<mlspp::State> &&state);

	void clear_pending_state();

	static constexpr const char *USER_MEDIA_KEY_BASE_LABEL =
		"Discord Secure Frames v0";

	ProtocolVersion m_session_protocol_version{0};
	std::vector<uint8_t> m_session_group_id;
	std::string m_signing_key_id;
	KeyPairContextType m_key_pair_context{nullptr};
	uint64_t m_bot_user_id{0};

	std::unique_ptr<mlspp::LeafNode> m_self_leaf_node;
	std::shared_ptr<mlspp::SignaturePrivateKey> m_signature_private_key;
	std::unique_ptr<mlspp::HPKEPrivateKey> m_hpke_private_key;
	std::unique_ptr<mlspp::HPKEPrivateKey> m_join_init_private_key;
	std::unique_ptr<mlspp::KeyPackage> m_join_key_package;
	std::unique_ptr<mlspp::ExternalSender> m_mls_external_sender;

	std::unique_ptr<mlspp::State> m_pending_group_state;
	std::unique_ptr<mlspp::MLSMessage> m_pending_group_commit;
	std::unique_ptr<mlspp::State> m_outbound_cached_group_state;
	std::unique_ptr<mlspp::State> m_current_state;

	RosterMap m_roster;
	std::unique_ptr<mlspp::State> m_state_with_proposals;
	std::list<QueuedProposal> m_proposal_queue;

	MlsFailureCallback m_failure_callback;

	// Logger
	PrefixedLogger m_logger{"dave.session"};
};

}  // namespace ekizu::dave

#endif	// DAVE_SESSION_HPP
