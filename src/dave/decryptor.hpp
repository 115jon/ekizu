#ifndef DAVE_DECRYPTOR_HPP
#define DAVE_DECRYPTOR_HPP

#include <boost/config.hpp>
#include <boost/core/span.hpp>
#include <chrono>
#include <ekizu/logger.hpp>
#include <ekizu/result.hpp>
#include <memory>

#include "cipher_manager.hpp"
#include "frame_processors.hpp"
#include "key_ratchet.hpp"
#include "types.hpp"

namespace ekizu::dave {

struct DecryptorImpl;

struct DecryptionStats {
	uint64_t passthroughs{};
	uint64_t decrypt_success{};
	uint64_t decrypt_failure{};
	uint64_t decrypt_duration{};
	uint64_t decrypt_attempts{};
};

/// @brief DAVE decryptor implementation.
/// @note This class is NOT thread-safe. All access must be serialized
///       (e.g., via asio strand). This is by design for performance.
struct Decryptor {
	using TimePoint = std::chrono::steady_clock::time_point;
	using Duration = std::chrono::seconds;

	void transition_to_key_ratchet(std::vector<uint8_t> key,
								   Duration transition_expiry = Duration(10));
	void transition_to_key_ratchet(std::unique_ptr<KeyRatchetInterface> ratchet,
								   Duration transition_expiry = Duration(10));

	void transition_to_passthrough_mode(
		bool passthrough_mode, Duration transition_expiry = Duration(10));

	Result<size_t> decrypt(MediaType media_type,
						   boost::span<const uint8_t> encrypted_frame,
						   boost::span<uint8_t> frame_out);

   private:
	/// Type-safe stats accessor
	[[nodiscard]] DecryptionStats &stats_for(MediaType type) {
		return m_stats[static_cast<size_t>(type)];
	}
	[[nodiscard]] const DecryptionStats &stats_for(MediaType type) const {
		return m_stats[static_cast<size_t>(type)];
	}

	bool decrypt_impl(detail::CipherManager &manager, MediaType media_type,
					  InboundFrameProcessor &frame_processor,
					  boost::span<uint8_t> frame_out, size_t *bytes_written);

	void update_cryptor_manager_expiry(Duration expiry);
	void cleanup_expired_managers();

	std::unique_ptr<InboundFrameProcessor> get_or_create_frame_processor();
	void return_frame_processor(std::unique_ptr<InboundFrameProcessor> fp);

	// Cipher managers (strand-serialized access only)
	std::deque<detail::CipherManager> m_managers;

	// Frame processor pool (strand-serialized, reuse to avoid allocations)
	std::vector<std::unique_ptr<InboundFrameProcessor>> m_frame_processors;

	// Reusable temp buffer for combined ciphertext+tag (avoids per-frame alloc)
	ByteBuffer m_temp_combined;

	TimePoint m_allow_passthrough_until{TimePoint::min()};
	std::array<DecryptionStats, 2> m_stats{};

	// Logger
	PrefixedLogger m_logger{"dave.decryptor"};
};

}  // namespace ekizu::dave

#endif	// DAVE_DECRYPTOR_HPP
