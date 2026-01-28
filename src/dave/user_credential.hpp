#ifndef DAVE_USER_CREDENTIAL_HPP
#define DAVE_USER_CREDENTIAL_HPP

#include <mls/credential.h>

#include <string>
#include <vector>

#include "common.hpp"

namespace ekizu::dave::detail {

inline mlspp::Credential create_user_credential(std::string const &user_id,
												uint16_t /*protocol_version*/) {
	try {
		uint64_t id = std::stoull(user_id);
		auto credential_bytes = big_endian_bytes_from(id);
		return mlspp::Credential::basic(credential_bytes);
	} catch (...) {
		// Fallback for non-numeric IDs (though should not happen for Discord
		// IDs)
		std::vector<uint8_t> identity_bytes(user_id.begin(), user_id.end());
		return mlspp::Credential::basic(mlspp::bytes_ns::bytes(identity_bytes));
	}
}

inline std::string user_credential_to_string(mlspp::Credential const &cred,
											 uint16_t /*protocol_version*/) {
	if (cred.type() != mlspp::CredentialType::basic) { return ""; }

	auto const &basic = cred.get<mlspp::BasicCredential>();
	// Try to interpret as big-endian u64
	if (basic.identity.size() == 8) {
		auto uid_val = from_big_endian_bytes(basic.identity);
		return std::to_string(uid_val);
	}

	// Fallback: treat as ASCII string
	auto const &identity = basic.identity.as_vec();
	return std::string(identity.begin(), identity.end());
}

}  // namespace ekizu::dave::detail

#endif	// DAVE_USER_CREDENTIAL_HPP
