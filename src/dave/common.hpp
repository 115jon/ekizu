#ifndef DAVE_COMMON_HPP
#define DAVE_COMMON_HPP

#include <mls/crypto.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace ekizu::dave::detail {

using Bytes = mlspp::bytes_ns::bytes;
using EncryptionKey = Bytes;

using unencrypted_frame_header_size = uint16_t;

inline constexpr size_t AES_GCM_128_KEY_BYTES = 16;
inline constexpr size_t AES_GCM_128_NONCE_BYTES = 12;
inline constexpr size_t AES_GCM_128_TRUNCATED_SYNC_NONCE_BYTES = 4;
inline constexpr size_t AES_GCM_128_TRUNCATED_SYNC_NONCE_OFFSET =
	AES_GCM_128_NONCE_BYTES - AES_GCM_128_TRUNCATED_SYNC_NONCE_BYTES;
inline constexpr size_t AES_GCM_128_TRUNCATED_TAG_BYTES = 8;
inline constexpr size_t RATCHET_GENERATION_BYTES = 1;
inline constexpr size_t RATCHET_GENERATION_SHIFT_BITS =
	8 * (AES_GCM_128_TRUNCATED_SYNC_NONCE_BYTES - RATCHET_GENERATION_BYTES);

using truncated_sync_nonce = uint32_t;
using supplemental_bytes_size = uint8_t;
using magic_marker = uint16_t;

inline constexpr uint16_t MARKER_BYTES = 0xFAFA;

inline constexpr auto DEFAULT_TRANSITION_EXPIRY = std::chrono::seconds(10);
inline constexpr auto CIPHER_EXPIRY = std::chrono::seconds(10);
inline constexpr int MAX_GENERATION_GAP = 250;
inline constexpr int MAX_MISSING_NONCES = 1000;
inline constexpr int GENERATION_WRAP = 1 << (8 * RATCHET_GENERATION_BYTES);
inline constexpr int MAX_FRAMES_PER_SECOND =
	50 + 2 * 60;  // 50 audio + 2*60fps video

inline constexpr size_t SUPPLEMENTAL_BYTES =
	AES_GCM_128_TRUNCATED_TAG_BYTES + sizeof(supplemental_bytes_size) +
	sizeof(magic_marker);
inline constexpr size_t TRANSFORM_PADDING_BYTES = 64;

inline constexpr std::array<uint8_t, 3> OPUS_SILENCE_PACKET = {
	0xF8, 0xFF, 0xFE};

// Type aliases for nonce tracking
using key_generation = uint32_t;
using big_nonce = uint64_t;

inline Bytes big_endian_bytes_from(uint64_t value) {
	std::vector<uint8_t> vec(sizeof(value));
	for (size_t i = 0; i < sizeof(value); ++i) {
		vec[sizeof(value) - 1 - i] = static_cast<uint8_t>(value >> (8 * i));
	}
	return Bytes(vec);
}

inline uint64_t from_big_endian_bytes(Bytes const &bytes) {
	uint64_t value = 0;
	auto vec = bytes.as_vec();
	for (size_t i = 0; i < vec.size() && i < sizeof(value); ++i) {
		value = (value << 8) | vec[i];
	}
	return value;
}

}  // namespace ekizu::dave::detail

#endif	// DAVE_COMMON_HPP
