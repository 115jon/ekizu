#ifndef DAVE_PERSISTED_KEY_PAIR_HPP
#define DAVE_PERSISTED_KEY_PAIR_HPP

#include <mls/crypto.h>

#include <memory>
#include <string>
#include <vector>

#include "types.hpp"

namespace ekizu::dave {

using KeyPairContextType = const char *;

/**
 * @brief Get persisted key pair
 * @param ctx context (pass nullptr to generate transient key)
 * @param session_id session id (pass empty string to generate transient key)
 * @param version Protocol version
 * @return MLS signature private key
 */
std::shared_ptr<mlspp::SignaturePrivateKey> get_persisted_key_pair(
	KeyPairContextType ctx, const std::string &session_id,
	ProtocolVersion version);

struct KeyAndSelfSignature {
	std::vector<uint8_t> key;
	std::vector<uint8_t> signature;
};

/**
 * @brief Get persisted public key
 * @param ctx context (set to nullptr to get transient key)
 * @param session_id session id (set to empty string to get transient key)
 * @param version protocol version
 * @return Key and self signature
 */
KeyAndSelfSignature get_persisted_public_key(KeyPairContextType ctx,
											 const std::string &session_id,
											 SignatureVersion version);

/**
 * @brief Delete persisted key pair
 * @param ctx context
 * @param session_id session ID
 * @param version protocol version
 * @return true if deleted
 */
bool delete_persisted_key_pair(KeyPairContextType ctx,
							   const std::string &session_id,
							   SignatureVersion version);

inline constexpr unsigned KEY_VERSION = 1;

}  // namespace ekizu::dave

#endif	// DAVE_PERSISTED_KEY_PAIR_HPP
