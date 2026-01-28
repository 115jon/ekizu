#ifndef DAVE_ENCRYPTOR_HPP
#define DAVE_ENCRYPTOR_HPP

#include <atomic>
#include <boost/config.hpp>
#include <boost/core/span.hpp>
#include <ekizu/logger.hpp>
#include <ekizu/result.hpp>
#include <memory>
#include <unordered_map>

#include "aead_cipher.hpp"
#include "frame_processors.hpp"
#include "key_ratchet.hpp"
#include "types.hpp"

namespace ekizu::dave {

/// @brief DAVE encryptor for E2EE audio/video frames.
/// @note This class is NOT thread-safe. All access must be serialized
///       (e.g., via asio strand). This is by design for performance.
struct Encryptor {
	void set_key_ratchet(std::vector<uint8_t> key);
	void set_key_ratchet(std::unique_ptr<KeyRatchetInterface> ratchet);
	void set_passthrough_mode(bool passthrough_mode);

	[[nodiscard]] bool has_key_ratchet() const;
	[[nodiscard]] bool is_passthrough_mode() const noexcept {
		return m_passthrough_mode.load(std::memory_order_acquire);
	}

	void assign_ssrc_to_codec(uint32_t ssrc, Codec codec);
	[[nodiscard]] Codec codec_for_ssrc(uint32_t ssrc) const;

	/// @brief Encrypt a frame using DAVE E2EE.
	/// @param media_type Audio or Video
	/// @param ssrc Source identifier for codec lookup
	/// @param frame Input plaintext frame
	/// @param encrypted_frame_out Output buffer (must be sized via
	/// get_max_ciphertext_byte_size)
	/// @return Number of bytes written on success, or DaveError on failure
	[[nodiscard]] Result<size_t> encrypt(
		MediaType media_type, uint32_t ssrc, boost::span<const uint8_t> frame,
		boost::span<uint8_t> encrypted_frame_out);

	[[nodiscard]] size_t get_max_ciphertext_byte_size(MediaType media_type,
													  size_t frame_size) const;

	[[nodiscard]] ProtocolVersion get_protocol_version() const noexcept {
		return m_current_protocol_version;
	}

   private:
	// Passthrough is atomic since it may be queried from other contexts
	std::atomic_bool m_passthrough_mode{true};

	// Key ratchet state (strand-serialized access only)
	std::unique_ptr<KeyRatchetInterface> m_ratchet;
	std::shared_ptr<detail::AeadCipher> m_cryptor;
	uint8_t m_current_key_generation{0};
	uint32_t m_truncated_nonce{0};

	// Codec mapping (strand-serialized access only)
	std::unordered_map<uint32_t, Codec> m_ssrc_codec_map;

	// Frame processor (mutable for const encrypt)
	mutable OutboundFrameProcessor m_frame_processor;

	// Reusable temp buffer for encryption output (avoids per-frame allocation)
	mutable ByteBuffer m_temp_cipher_output;

	ProtocolVersion m_current_protocol_version{max_protocol_version()};

	// Logger
	PrefixedLogger m_logger{"dave.encryptor"};
};

}  // namespace ekizu::dave

#endif	// DAVE_ENCRYPTOR_HPP
