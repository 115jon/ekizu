#ifndef DAVE_FRAME_PROCESSORS_HPP
#define DAVE_FRAME_PROCESSORS_HPP

#include <boost/container/small_vector.hpp>
#include <cstdint>

#include "common.hpp"
#include "types.hpp"

namespace ekizu::dave {

// Inline buffer size for small_vector - covers typical audio frames (~480
// bytes) without heap allocation. Larger video frames will spill to heap.
inline constexpr size_t INLINE_BUFFER_SIZE = 1024;

/// Stack-optimized byte buffer for frame processing
using ByteBuffer = boost::container::small_vector<uint8_t, INLINE_BUFFER_SIZE>;

// Inline capacity for range vectors (video frames typically have ≤8 ranges)
inline constexpr size_t INLINE_RANGE_CAPACITY = 8;

/**
 * @brief Range inside a frame
 */
struct Range {
	size_t offset;
	size_t size;
};

/**
 * @brief Vector of ranges in a frame (typically ≤8 for video)
 */
using Ranges = boost::container::small_vector<Range, INLINE_RANGE_CAPACITY>;

/**
 * @brief Get total size of unencrypted ranges
 * @param unencrypted_ranges unencrypted ranges
 * @return size
 */
uint8_t unencrypted_ranges_size(const Ranges &unencrypted_ranges);

/**
 * @brief Serialise unencrypted ranges
 * @param unencrypted_ranges unencrypted ranges
 * @param buffer buffer to serialise to
 * @param buffer_size size of buffer
 * @return size of ranges written
 */
uint8_t serialize_unencrypted_ranges(const Ranges &unencrypted_ranges,
									 uint8_t *buffer, size_t buffer_size);

/**
 * @brief Deserialise unencrypted ranges
 * @param read_at buffer to write to
 * @param buffer_size buffer size
 * @param unencrypted_ranges unencrypted ranges to write to
 * @return size of unencrypted ranges written
 */
uint8_t deserialize_unencrypted_ranges(const uint8_t *&read_at,
									   const uint8_t buffer_size,
									   Ranges &unencrypted_ranges);

/**
 * @brief Validate unencrypted ranges
 * @param unencrypted_ranges unencrypted ranges
 * @param frame_size frame size
 * @return true if validated
 */
bool validate_unencrypted_ranges(const Ranges &unencrypted_ranges,
								 size_t frame_size);

/**
 * @brief Processes inbound frames from the decryptor
 */
struct InboundFrameProcessor {
	/**
	 * @brief Parse inbound frame
	 * @param frame frame bytes
	 */
	void parse_frame(boost::span<const uint8_t> frame);

	/**
	 * @brief Rebuild frame after decryption
	 * @param frame frame bytes
	 * @return size of reconstructed frame
	 */
	[[nodiscard]] size_t reconstruct_frame(boost::span<uint8_t> frame) const;

	/**
	 * @brief True if encrypted
	 * @return is encrypted
	 */
	[[nodiscard]] bool is_encrypted() const { return m_encrypted; }

	/**
	 * @brief Get size
	 * @return Original frame size
	 */
	[[nodiscard]] size_t size() const { return m_original_size; }

	/**
	 * @brief Clear the processor state
	 */
	void clear();

	/**
	 * @brief get AEAD tag for frame processor
	 * @return AEAD tag
	 */
	[[nodiscard]] boost::span<const uint8_t> get_tag() const { return m_tag; }

	/**
	 * @brief Get truncated sync nonce
	 * @return truncated sync nonce
	 */
	[[nodiscard]] detail::truncated_sync_nonce get_truncated_nonce() const {
		return m_truncated_nonce;
	}

	/**
	 * @brief Get authenticated AEAD data
	 * @return AEAD auth data
	 */
	[[nodiscard]] boost::span<const uint8_t> get_authenticated_data() const {
		return {m_authenticated.data(), m_authenticated.size()};
	}

	/**
	 * @brief Get ciphertext
	 * @return Ciphertext view
	 */
	[[nodiscard]] boost::span<const uint8_t> get_ciphertext() const {
		return {m_ciphertext.data(), m_ciphertext.size()};
	}

	/**
	 * @brief Get plain text
	 * @return Plain text view
	 */
	[[nodiscard]] boost::span<uint8_t> get_plaintext() {
		return {m_plaintext.data(), m_plaintext.size()};
	}

   private:
	/**
	 * @brief Add authenticated bytes
	 * @param data authenticated data
	 */
	void add_authenticated_bytes(boost::span<const uint8_t> data);

	/**
	 * @brief Add ciphertext bytes
	 * @param data ciphertext data
	 */
	void add_ciphertext_bytes(boost::span<const uint8_t> data);

	/**
	 * @brief True if frames are encrypted
	 */
	bool m_encrypted{false};

	/**
	 * @brief Original size
	 */
	size_t m_original_size{0};

	/**
	 * @brief AEAD tag
	 */
	boost::span<const uint8_t> m_tag;

	/**
	 * @brief Truncated nonce
	 */
	detail::truncated_sync_nonce m_truncated_nonce{};

	/**
	 * @brief Unencrypted parts of the frames
	 */
	Ranges m_unencrypted_ranges;

	/**
	 * @brief additional authenticated data
	 */
	ByteBuffer m_authenticated;

	/**
	 * @brief Ciphertext
	 */
	ByteBuffer m_ciphertext;

	/**
	 * @brief Plaintext
	 */
	ByteBuffer m_plaintext;
};

/**
 * @brief Outbound frame processor, processes outbound frames for encryption
 */
struct OutboundFrameProcessor {
	/**
	 * @brief Process outbound frame
	 * @param frame frame data
	 * @param codec codec to use
	 */
	void process_frame(boost::span<const uint8_t> frame, Codec codec);

	/**
	 * @brief do_reconstruct frame
	 * @param frame frame data
	 * @return size of reconstructed frame
	 */
	size_t reconstruct_frame(boost::span<uint8_t> frame);

	/**
	 * @brief Get codec
	 * @return codec
	 */
	[[nodiscard]] Codec get_codec() const { return m_frame_codec; }

	/**
	 * @brief Get unencrypted bytes
	 * @return unencrypted bytes
	 */
	[[nodiscard]] const ByteBuffer &get_unencrypted_bytes() const {
		return m_unencrypted_bytes;
	}

	/**
	 * @brief Get encrypted bytes
	 * @return Encrypted bytes
	 */
	[[nodiscard]] const ByteBuffer &get_encrypted_bytes() const {
		return m_encrypted_bytes;
	}

	/**
	 * @brief Get ciphertext bytes
	 * @return ciphertext bytes
	 */
	[[nodiscard]] ByteBuffer &get_ciphertext_bytes() {
		return m_ciphertext_bytes;
	}

	/**
	 * @brief Get unencrypted bytes
	 * @return unencrypted bytes
	 */
	[[nodiscard]] const Ranges &get_unencrypted_ranges() const {
		return m_unencrypted_ranges;
	}

	/**
	 * @brief Reset outbound processor
	 */
	void reset();

	/**
	 * @brief Add unencrypted bytes
	 * @param bytes unencrypted bytes
	 */
	void add_unencrypted_bytes(boost::span<const uint8_t> bytes);

	/**
	 * @brief Add encrypted bytes
	 * @param bytes encrypted bytes
	 */
	void add_encrypted_bytes(boost::span<const uint8_t> bytes);

   private:
	/**
	 * @brief Codec used to decrypt
	 */
	Codec m_frame_codec{Codec::Unknown};

	/**
	 * @brief Frame index
	 */
	size_t m_frame_index{0};

	/**
	 * @brief Unencrypted bytes
	 */
	ByteBuffer m_unencrypted_bytes;

	/**
	 * @brief Encrypted bytes
	 */
	ByteBuffer m_encrypted_bytes;

	/**
	 * @brief Ciphertext bytes
	 */
	ByteBuffer m_ciphertext_bytes;

	/**
	 * @brief Unencrypted ranges that need to be kept plaintext to allow for RTP
	 * routing
	 */
	Ranges m_unencrypted_ranges;
};

}  // namespace ekizu::dave

#endif	// DAVE_FRAME_PROCESSORS_HPP
