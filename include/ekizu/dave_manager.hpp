#ifndef EKIZU_DAVE_MANAGER_HPP
#define EKIZU_DAVE_MANAGER_HPP

#include <dave/decryptor.h>
#include <dave/encryptor.h>
#include <dave/mls/session.h>

#include <boost/core/span.hpp>
#include <ekizu/result.hpp>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace ekizu {
// Manages all DAVE/MLS encryption state and operations
struct DaveManager {
	explicit DaveManager(std::string user_id);

	void set_logger(std::function<void(std::string_view)> logger);

	// Protocol version management
	void set_protocol_version(int version) { m_protocol_version = version; }
	int protocol_version() const noexcept { return m_protocol_version; }
	bool is_e2ee_enabled() const noexcept { return m_protocol_version > 0; }

	// MLS session lifecycle
	Result<> initialize_session(
		int protocol_version, uint64_t group_id,
		std::shared_ptr<mlspp::SignaturePrivateKey> sig_key);
	void reset_session();

	bool is_initialized() const noexcept { return m_mls_initialized; }
	bool has_joined_via_welcome() const noexcept {
		return m_joined_via_welcome;
	}

	// External sender (required before init)
	void set_external_sender(std::vector<std::byte> package);
	bool has_external_sender() const noexcept { return m_have_external_sender; }

	// Key package operations
	Result<std::vector<std::byte>> get_marshalled_key_package();

	// MLS message processing
	Result<std::vector<std::uint8_t>> process_proposals(
		boost::span<const std::byte> proposals,
		std::set<std::string> const &recognized_users);

	bool process_commit(boost::span<const std::byte> commit);

	bool process_welcome(boost::span<const std::byte> welcome,
						 std::set<std::string> const &recognized_users);

	// Encryption/Decryption operations
	void assign_ssrc_to_codec(uint32_t ssrc, discord::dave::Codec codec);

	bool install_sender_ratchet();
	bool install_receiver_ratchet(const std::string &user_id);

	void set_passthrough_mode(bool enabled);
	bool is_passthrough_mode() const noexcept;
	bool has_key_ratchet() const noexcept;

	// Transition state management
	void set_transition_complete(bool complete) {
		m_transition_complete = complete;
	}
	bool is_transition_complete() const noexcept {
		return m_transition_complete;
	}

	void set_sender_ready(bool ready) { m_sender_ready = ready; }
	bool is_sender_ready() const noexcept { return m_sender_ready; }

	// Can attempt decrypt after sender ready, even before first success
	bool can_attempt_decrypt() const noexcept;

	bool ready_to_send() const noexcept;
	bool ready_to_receive() const noexcept;

	Result<std::size_t> encrypt_frame(discord::dave::MediaType media_type,
									  uint32_t ssrc,
									  boost::span<const std::byte> plaintext,
									  boost::span<std::byte> ciphertext_out);

	Result<std::size_t> decrypt_frame(discord::dave::MediaType media_type,
									  const std::string &user_id,
									  boost::span<const std::byte> ciphertext,
									  boost::span<std::byte> plaintext_out);

	std::size_t max_ciphertext_size(discord::dave::MediaType media_type,
									std::size_t plaintext_size);

	std::shared_ptr<mlspp::SignaturePrivateKey> get_or_create_sig_key(
		int protocol_version);

	bool commit_pending_group();

   private:
	void log(std::string_view msg) const;

	std::string m_user_id;
	int m_protocol_version = 0;
	bool m_mls_initialized = false;
	bool m_joined_via_welcome = false;
	bool m_have_external_sender = false;
	bool m_transition_complete = false;
	bool m_sender_ready = false;

	std::vector<uint8_t> m_external_sender_package;
	std::unique_ptr<discord::dave::mls::Session> m_session;

	discord::dave::Encryptor m_encryptor;
	std::unordered_map<std::string, discord::dave::Decryptor> m_user_decryptors;
	bool m_passthrough_mode = true;

	std::shared_ptr<mlspp::SignaturePrivateKey> m_cached_sig_key;
	std::function<void(std::string_view)> m_logger;
};
}  // namespace ekizu

#endif	// EKIZU_DAVE_MANAGER_HPP
