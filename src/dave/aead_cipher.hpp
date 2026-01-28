#ifndef DAVE_AEAD_CIPHER_HPP
#define DAVE_AEAD_CIPHER_HPP

#include <openssl/evp.h>

#include <boost/core/span.hpp>
#include <ekizu/result.hpp>
#include <vector>

#include "common.hpp"

namespace ekizu::dave::detail {

struct AeadCipher {
	explicit AeadCipher(const EncryptionKey &key);
	~AeadCipher();

	AeadCipher(const AeadCipher &) = delete;
	AeadCipher &operator=(const AeadCipher &) = delete;
	AeadCipher(AeadCipher &&other) noexcept;
	AeadCipher &operator=(AeadCipher &&other) noexcept;

	[[nodiscard]] bool is_valid() const noexcept { return m_ctx != nullptr; }

	Result<size_t> encrypt(boost::span<const uint8_t> plaintext,
						   boost::span<const uint8_t> additional_data,
						   boost::span<const uint8_t> nonce,
						   boost::span<uint8_t> ciphertext_out);

	Result<size_t> decrypt(boost::span<const uint8_t> ciphertext,
						   boost::span<const uint8_t> additional_data,
						   boost::span<const uint8_t> nonce,
						   boost::span<uint8_t> plaintext_out);

   private:
	EVP_CIPHER_CTX *m_ctx{nullptr};
	std::vector<uint8_t> m_key;
};

}  // namespace ekizu::dave::detail

#endif	// DAVE_AEAD_CIPHER_HPP
