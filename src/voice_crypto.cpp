#include <sodium.h>

#include <boost/endian/conversion.hpp>
#include <cstring>
#include <ekizu/voice_crypto.hpp>

namespace ekizu {

namespace {
void put_u32_be(std::byte *buf, uint32_t val) {
	auto be = boost::endian::native_to_big(val);
	std::memcpy(buf, &be, sizeof(be));
}
}  // namespace

Result<std::vector<std::byte>> VoiceCrypto::encrypt_rtp(
	std::array<std::byte, 12> const &header,
	boost::span<const std::byte> payload) {
	uint32_t nonce_val = nonce++;

	if (mode == VoiceTransportMode::XChaCha20_Poly1305_RTPSIZE) {
		return encrypt_xchacha20(header, payload, nonce_val);
	}
	if (mode == VoiceTransportMode::AES256_GCM_RTPSIZE) {
		return encrypt_aes256gcm(header, payload, nonce_val);
	}

	return boost::system::errc::not_supported;
}

Result<std::vector<std::byte>> VoiceCrypto::decrypt_rtp(
	std::array<std::byte, 12> const &header,
	boost::span<const std::byte> encrypted_payload) const {
	if (mode == VoiceTransportMode::XChaCha20_Poly1305_RTPSIZE) {
		return decrypt_xchacha20(header, encrypted_payload);
	}
	if (mode == VoiceTransportMode::AES256_GCM_RTPSIZE) {
		return decrypt_aes256gcm(header, encrypted_payload);
	}

	return boost::system::errc::not_supported;
}

Result<std::vector<std::byte>> VoiceCrypto::encrypt_xchacha20(
	std::array<std::byte, 12> const &header,
	boost::span<const std::byte> payload, uint32_t nonce_val) const {
	constexpr std::size_t TAG_SIZE = crypto_aead_xchacha20poly1305_ietf_ABYTES;
	constexpr std::size_t NONCE_SIZE = 4;

	std::vector<std::byte> result(12 + payload.size() + TAG_SIZE + NONCE_SIZE);

	// Copy header
	std::memcpy(result.data(), header.data(), header.size());

	// Prepare 4-byte nonce array
	std::byte nonce_bytes[4];
	put_u32_be(nonce_bytes, nonce_val);

	// Prepare Sodium IV (24 bytes)
	std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES>
		iv{};
	std::memcpy(iv.data(), nonce_bytes, 4);

	unsigned long long ciphertext_len = 0;

	int rc = crypto_aead_xchacha20poly1305_ietf_encrypt(
		reinterpret_cast<unsigned char *>(result.data() + 12), &ciphertext_len,
		reinterpret_cast<const unsigned char *>(payload.data()), payload.size(),
		reinterpret_cast<const unsigned char *>(header.data()), header.size(),
		nullptr, iv.data(),
		reinterpret_cast<const unsigned char *>(key.data()));

	if (rc != 0) { return boost::system::errc::io_error; }

	// Append 4-byte nonce to end
	std::memcpy(result.data() + 12 + ciphertext_len, nonce_bytes, 4);

	return result;
}

Result<std::vector<std::byte>> VoiceCrypto::encrypt_aes256gcm(
	std::array<std::byte, 12> const &header,
	boost::span<const std::byte> payload, uint32_t nonce_val) const {
	constexpr std::size_t TAG_SIZE = crypto_aead_aes256gcm_ABYTES;
	constexpr std::size_t NONCE_SIZE = 4;

	std::vector<std::byte> result(12 + payload.size() + TAG_SIZE + NONCE_SIZE);

	std::memcpy(result.data(), header.data(), header.size());

	std::byte nonce_bytes[4];
	put_u32_be(nonce_bytes, nonce_val);

	std::array<unsigned char, crypto_aead_aes256gcm_NPUBBYTES> iv{};
	std::memcpy(iv.data(), nonce_bytes, 4);

	unsigned long long ciphertext_len = 0;

	int rc = crypto_aead_aes256gcm_encrypt(
		reinterpret_cast<unsigned char *>(result.data() + 12), &ciphertext_len,
		reinterpret_cast<const unsigned char *>(payload.data()), payload.size(),
		reinterpret_cast<const unsigned char *>(header.data()), header.size(),
		nullptr, iv.data(),
		reinterpret_cast<const unsigned char *>(key.data()));

	if (rc != 0) { return boost::system::errc::io_error; }

	std::memcpy(result.data() + 12 + ciphertext_len, nonce_bytes, 4);

	return result;
}

Result<std::vector<std::byte>> VoiceCrypto::decrypt_xchacha20(
	std::array<std::byte, 12> const &header,
	boost::span<const std::byte> encrypted_payload) const {
	constexpr std::size_t TAG_SIZE = crypto_aead_xchacha20poly1305_ietf_ABYTES;
	constexpr std::size_t NONCE_SIZE = 4;

	if (encrypted_payload.size() < TAG_SIZE + NONCE_SIZE) {
		return boost::system::errc::message_size;
	}

	std::byte nonce_bytes[4];
	std::memcpy(
		nonce_bytes,
		encrypted_payload.data() + encrypted_payload.size() - NONCE_SIZE, 4);

	std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES>
		iv{};
	std::memcpy(iv.data(), nonce_bytes, 4);

	std::size_t ciphertext_len = encrypted_payload.size() - NONCE_SIZE;
	std::vector<std::byte> result(ciphertext_len - TAG_SIZE);

	unsigned long long plaintext_len = 0;

	int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
		reinterpret_cast<unsigned char *>(result.data()), &plaintext_len,
		nullptr,
		reinterpret_cast<const unsigned char *>(encrypted_payload.data()),
		ciphertext_len, reinterpret_cast<const unsigned char *>(header.data()),
		header.size(), iv.data(),
		reinterpret_cast<const unsigned char *>(key.data()));

	if (rc != 0) { return boost::system::errc::io_error; }

	result.resize(plaintext_len);
	return result;
}

Result<std::vector<std::byte>> VoiceCrypto::decrypt_aes256gcm(
	std::array<std::byte, 12> const &header,
	boost::span<const std::byte> encrypted_payload) const {
	constexpr std::size_t TAG_SIZE = crypto_aead_aes256gcm_ABYTES;
	constexpr std::size_t NONCE_SIZE = 4;

	if (encrypted_payload.size() < TAG_SIZE + NONCE_SIZE) {
		return boost::system::errc::message_size;
	}

	std::byte nonce_bytes[4];
	std::memcpy(
		nonce_bytes,
		encrypted_payload.data() + encrypted_payload.size() - NONCE_SIZE, 4);

	std::array<unsigned char, crypto_aead_aes256gcm_NPUBBYTES> iv{};
	std::memcpy(iv.data(), nonce_bytes, 4);

	std::size_t ciphertext_len = encrypted_payload.size() - NONCE_SIZE;
	std::vector<std::byte> result(ciphertext_len - TAG_SIZE);

	unsigned long long plaintext_len = 0;

	int rc = crypto_aead_aes256gcm_decrypt(
		reinterpret_cast<unsigned char *>(result.data()), &plaintext_len,
		nullptr,
		reinterpret_cast<const unsigned char *>(encrypted_payload.data()),
		ciphertext_len, reinterpret_cast<const unsigned char *>(header.data()),
		header.size(), iv.data(),
		reinterpret_cast<const unsigned char *>(key.data()));

	if (rc != 0) { return boost::system::errc::io_error; }

	result.resize(plaintext_len);
	return result;
}

}  // namespace ekizu