#ifndef EKIZU_VOICE_CRYPTO_HPP
#define EKIZU_VOICE_CRYPTO_HPP

#include <boost/core/span.hpp>
#include <ekizu/result.hpp>
#include <ekizu/voice_types.hpp>

namespace ekizu {
struct VoiceCrypto {
	VoiceTransportMode mode = VoiceTransportMode::XChaCha20_Poly1305_RTPSIZE;
	std::vector<std::byte> key;
	uint32_t nonce = 0;

	[[nodiscard]] Result<std::vector<std::byte>> encrypt_rtp(
		std::array<std::byte, 12> const &header,
		boost::span<const std::byte> payload);

	[[nodiscard]] Result<std::vector<std::byte>> decrypt_rtp(
		std::array<std::byte, 12> const &header,
		boost::span<const std::byte> encrypted_payload) const;

   private:
	[[nodiscard]] Result<std::vector<std::byte>> encrypt_xchacha20(
		std::array<std::byte, 12> const &header,
		boost::span<const std::byte> payload, uint32_t nonce_val) const;

	[[nodiscard]] Result<std::vector<std::byte>> encrypt_aes256gcm(
		std::array<std::byte, 12> const &header,
		boost::span<const std::byte> payload, uint32_t nonce_val) const;

	[[nodiscard]] Result<std::vector<std::byte>> decrypt_xchacha20(
		std::array<std::byte, 12> const &header,
		boost::span<const std::byte> encrypted_payload) const;

	[[nodiscard]] Result<std::vector<std::byte>> decrypt_aes256gcm(
		std::array<std::byte, 12> const &header,
		boost::span<const std::byte> encrypted_payload) const;
};
}  // namespace ekizu

#endif	// EKIZU_VOICE_CRYPTO_HPP
