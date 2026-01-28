#ifndef DAVE_KEY_RATCHET_HPP
#define DAVE_KEY_RATCHET_HPP

#include <mls/key_schedule.h>

#include <cstdint>
#include <vector>

namespace ekizu::dave {

/// Abstract interface for key ratcheting.
/// This allows proper MLS-based key derivation per generation.
struct KeyRatchetInterface {
	virtual ~KeyRatchetInterface() = default;
	virtual std::vector<uint8_t> get_key(uint8_t generation) noexcept = 0;
	virtual void delete_key(uint8_t generation) noexcept = 0;
};

namespace detail {

// Inherit from public interface and implement with MLS-based key derivation
struct MlsKeyRatchet : public KeyRatchetInterface {
	MlsKeyRatchet(mlspp::CipherSuite suite,
				  mlspp::bytes_ns::bytes base_secret) noexcept;

	std::vector<uint8_t> get_key(uint8_t generation) noexcept override;
	void delete_key(uint8_t generation) noexcept override;

   private:
	mlspp::HashRatchet m_ratchet;
};

// Simple key ratchet from raw key bytes (returns same key for all generations)
struct SimpleKeyRatchet : public KeyRatchetInterface {
	explicit SimpleKeyRatchet(std::vector<uint8_t> base_key) noexcept;

	std::vector<uint8_t> get_key(uint8_t generation) noexcept override;
	void delete_key(uint8_t generation) noexcept override;

   private:
	std::vector<uint8_t> m_base_key;
};

}  // namespace detail
}  // namespace ekizu::dave

#endif	// DAVE_KEY_RATCHET_HPP
