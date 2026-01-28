#include "key_ratchet.hpp"

namespace ekizu::dave::detail {

MlsKeyRatchet::MlsKeyRatchet(mlspp::CipherSuite suite,
							 mlspp::bytes_ns::bytes base_secret) noexcept
	: m_ratchet(suite, std::move(base_secret)) {}

std::vector<uint8_t> MlsKeyRatchet::get_key(uint8_t generation) noexcept {
	try {
		auto key_and_nonce = m_ratchet.get(generation);
		return key_and_nonce.key.as_vec();
	} catch (...) { return {}; }
}

void MlsKeyRatchet::delete_key(uint8_t generation) noexcept {
	m_ratchet.erase(generation);
}

SimpleKeyRatchet::SimpleKeyRatchet(std::vector<uint8_t> base_key) noexcept
	: m_base_key(std::move(base_key)) {}

std::vector<uint8_t> SimpleKeyRatchet::get_key(
	uint8_t /*generation*/) noexcept {
	return m_base_key;
}

void SimpleKeyRatchet::delete_key(uint8_t /*generation*/) noexcept {}

}  // namespace ekizu::dave::detail
