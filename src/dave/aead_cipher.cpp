#include "aead_cipher.hpp"

#include <openssl/err.h>

#include <algorithm>

namespace ekizu::dave::detail {

AeadCipher::AeadCipher(const EncryptionKey &key)
	: m_ctx(EVP_CIPHER_CTX_new()), m_key(key.begin(), key.end()) {}

AeadCipher::~AeadCipher() {
	if (m_ctx) { EVP_CIPHER_CTX_free(m_ctx); }
}

AeadCipher::AeadCipher(AeadCipher &&other) noexcept
	: m_ctx(other.m_ctx), m_key(std::move(other.m_key)) {
	other.m_ctx = nullptr;
}

AeadCipher &AeadCipher::operator=(AeadCipher &&other) noexcept {
	if (this != &other) {
		if (m_ctx) { EVP_CIPHER_CTX_free(m_ctx); }
		m_ctx = other.m_ctx;
		m_key = std::move(other.m_key);
		other.m_ctx = nullptr;
	}
	return *this;
}

Result<size_t> AeadCipher::encrypt(boost::span<const uint8_t> plaintext,
								   boost::span<const uint8_t> additional_data,
								   boost::span<const uint8_t> nonce,
								   boost::span<uint8_t> ciphertext_out) {
	if (!m_ctx) { return boost::system::errc::operation_not_permitted; }

	int len{};
	const EVP_CIPHER *cipher = EVP_aes_128_gcm();

	if (EVP_EncryptInit_ex(m_ctx, cipher, nullptr, nullptr, nullptr) == 0) {
		return boost::system::errc::io_error;
	}

	if (EVP_CIPHER_CTX_ctrl(m_ctx, EVP_CTRL_GCM_SET_IVLEN,
							static_cast<int>(nonce.size()), nullptr) == 0) {
		return boost::system::errc::io_error;
	}

	if (EVP_EncryptInit_ex(
			m_ctx, nullptr, nullptr, m_key.data(), nonce.data()) == 0) {
		return boost::system::errc::io_error;
	}

	if (!additional_data.empty()) {
		if (EVP_EncryptUpdate(m_ctx, nullptr, &len, additional_data.data(),
							  static_cast<int>(additional_data.size())) == 0) {
			return boost::system::errc::io_error;
		}
	}

	size_t written{};
	if (EVP_EncryptUpdate(m_ctx, ciphertext_out.data(), &len, plaintext.data(),
						  static_cast<int>(plaintext.size())) == 0) {
		return boost::system::errc::io_error;
	}
	written = static_cast<size_t>(len);

	if (EVP_EncryptFinal_ex(m_ctx, ciphertext_out.data() + written, &len) ==
		0) {
		return boost::system::errc::io_error;
	}
	written += static_cast<size_t>(len);

	uint8_t tag[AES_GCM_128_TRUNCATED_TAG_BYTES];
	if (EVP_CIPHER_CTX_ctrl(m_ctx, EVP_CTRL_GCM_GET_TAG,
							AES_GCM_128_TRUNCATED_TAG_BYTES, tag) == 0) {
		return boost::system::errc::io_error;
	}

	if (ciphertext_out.size() >= written + AES_GCM_128_TRUNCATED_TAG_BYTES) {
		auto tag_dest =
			ciphertext_out.subspan(written, AES_GCM_128_TRUNCATED_TAG_BYTES);
		std::copy(std::begin(tag), std::end(tag), tag_dest.begin());
		written += AES_GCM_128_TRUNCATED_TAG_BYTES;
	}

	return written;
}

Result<size_t> AeadCipher::decrypt(boost::span<const uint8_t> ciphertext,
								   boost::span<const uint8_t> additional_data,
								   boost::span<const uint8_t> nonce,
								   boost::span<uint8_t> plaintext_out) {
	if (!m_ctx) { return boost::system::errc::operation_not_permitted; }

	if (ciphertext.size() < AES_GCM_128_TRUNCATED_TAG_BYTES) {
		return boost::system::errc::message_size;
	}

	int len{};
	const EVP_CIPHER *cipher = EVP_aes_128_gcm();

	size_t actual_ciphertext_len =
		ciphertext.size() - AES_GCM_128_TRUNCATED_TAG_BYTES;
	auto tag_data = ciphertext.subspan(actual_ciphertext_len);

	if (EVP_DecryptInit_ex(m_ctx, cipher, nullptr, nullptr, nullptr) == 0) {
		return boost::system::errc::io_error;
	}

	if (EVP_CIPHER_CTX_ctrl(m_ctx, EVP_CTRL_GCM_SET_IVLEN,
							static_cast<int>(nonce.size()), nullptr) == 0) {
		return boost::system::errc::io_error;
	}

	if (EVP_DecryptInit_ex(
			m_ctx, nullptr, nullptr, m_key.data(), nonce.data()) == 0) {
		return boost::system::errc::io_error;
	}

	if (!additional_data.empty()) {
		if (EVP_DecryptUpdate(m_ctx, nullptr, &len, additional_data.data(),
							  static_cast<int>(additional_data.size())) == 0) {
			return boost::system::errc::io_error;
		}
	}

	size_t written{};
	if (EVP_DecryptUpdate(m_ctx, plaintext_out.data(), &len, ciphertext.data(),
						  static_cast<int>(actual_ciphertext_len)) == 0) {
		return boost::system::errc::io_error;
	}
	written = static_cast<size_t>(len);

	// Set truncated tag (set expected tag length to 8)
	if (EVP_CIPHER_CTX_ctrl(
			m_ctx, EVP_CTRL_GCM_SET_TAG, AES_GCM_128_TRUNCATED_TAG_BYTES,
			const_cast<uint8_t *>(tag_data.data())) == 0) {
		return boost::system::errc::io_error;
	}

	if (EVP_DecryptFinal_ex(m_ctx, plaintext_out.data() + written, &len) == 0) {
		return boost::system::errc::permission_denied;	// Auth failed
	}
	written += static_cast<size_t>(len);

	return written;
}

}  // namespace ekizu::dave::detail
