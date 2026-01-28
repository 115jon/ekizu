#include "frame_processors.hpp"

#include <algorithm>
#include <boost/config.hpp>
#include <limits>

#include "buffer_utils.hpp"
#include "codec_utils.hpp"
#include "common.hpp"
#include "leb128.hpp"

namespace ekizu::dave {

// Use checked_add from buffer_utils for overflow detection
std::pair<bool, size_t> overflow_add(size_t a, size_t b) {
	auto result = detail::checked_add(a, b);
	if (!result) { return {true, 0}; }	// overflow occurred
	return {false, *result};
}

uint8_t unencrypted_ranges_size(const Ranges &unencrypted_ranges) {
	size_t size{};
	for (const auto &range : unencrypted_ranges) {
		size += leb128_size(range.offset);
		size += leb128_size(range.size);
	}
	return static_cast<uint8_t>(size);
}

uint8_t serialize_unencrypted_ranges(const Ranges &unencrypted_ranges,
									 uint8_t *buffer, size_t buffer_size) {
	auto *write_at = buffer;
	auto *end = buffer + buffer_size;
	for (const auto &range : unencrypted_ranges) {
		auto range_size = leb128_size(range.offset) + leb128_size(range.size);
		if (range_size > static_cast<size_t>(end - write_at)) { break; }

		write_at += write_leb128(range.offset, write_at);
		write_at += write_leb128(range.size, write_at);
	}
	return static_cast<uint8_t>(write_at - buffer);
}

uint8_t deserialize_unencrypted_ranges(const uint8_t *&read_at,
									   const uint8_t buffer_size,
									   Ranges &unencrypted_ranges) {
	const auto *start = read_at;
	const auto *end = read_at + buffer_size;
	while (read_at < end) {
		size_t offset = read_leb128(read_at, end);
		if (read_at == nullptr) { break; }

		size_t size = read_leb128(read_at, end);
		if (read_at == nullptr) { break; }
		unencrypted_ranges.push_back({offset, size});
	}

	if (read_at != end) {
		unencrypted_ranges.clear();
		read_at = nullptr;
		return 0;
	}

	return static_cast<uint8_t>(read_at - start);
}

bool validate_unencrypted_ranges(const Ranges &unencrypted_ranges,
								 size_t frame_size) {
	if (unencrypted_ranges.empty()) { return true; }

	// validate that the ranges are in order and don't overlap
	for (auto i = 0U; i < unencrypted_ranges.size(); ++i) {
		auto current = unencrypted_ranges[i];
		// The current range should not overflow into the next range
		// or if it is the last range, the end of the frame
		auto max_end = i + 1 < unencrypted_ranges.size()
						   ? unencrypted_ranges[i + 1].offset
						   : frame_size;

		auto [did_overflow, current_end] =
			overflow_add(current.offset, current.size);
		if (did_overflow || current_end > max_end) { return false; }
	}

	return true;
}

size_t do_reconstruct(const Ranges &ranges, const ByteBuffer &range_bytes,
					  const ByteBuffer &other_bytes,
					  boost::span<uint8_t> output) {
	size_t frame_index{};
	size_t range_bytes_index{};
	size_t other_bytes_index{};

	const auto copy_range_bytes = [&](size_t size) {
		auto src = boost::span<const uint8_t>(range_bytes)
					   .subspan(range_bytes_index, size);
		auto dst = output.subspan(frame_index, size);
		std::copy(src.begin(), src.end(), dst.begin());
		range_bytes_index += size;
		frame_index += size;
	};

	const auto copy_other_bytes = [&](size_t size) {
		auto src = boost::span<const uint8_t>(other_bytes)
					   .subspan(other_bytes_index, size);
		auto dst = output.subspan(frame_index, size);
		std::copy(src.begin(), src.end(), dst.begin());
		other_bytes_index += size;
		frame_index += size;
	};

	for (const auto &range : ranges) {
		if (range.offset > frame_index) {
			copy_other_bytes(range.offset - frame_index);
		}

		copy_range_bytes(range.size);
	}

	if (other_bytes_index < other_bytes.size()) {
		copy_other_bytes(other_bytes.size() - other_bytes_index);
	}

	return frame_index;
}

void InboundFrameProcessor::clear() {
	m_encrypted = false;
	m_original_size = 0;
	m_truncated_nonce =
		std::numeric_limits<detail::truncated_sync_nonce>::max();
	m_unencrypted_ranges.clear();
	m_authenticated.clear();
	m_ciphertext.clear();
	m_plaintext.clear();
}

void InboundFrameProcessor::parse_frame(boost::span<const uint8_t> frame) {
	clear();

	constexpr auto min_supplemental_bytes_size =
		detail::AES_GCM_128_TRUNCATED_TAG_BYTES +
		sizeof(detail::supplemental_bytes_size) + sizeof(detail::magic_marker);
	if (BOOST_UNLIKELY(frame.size() < min_supplemental_bytes_size)) { return; }

	// Check the frame ends with the magic marker using span operations
	if (BOOST_UNLIKELY(!detail::ends_with(frame, detail::MARKER_BYTES))) {
		return;
	}

	// Read the supplemental bytes size (1 byte before marker)
	constexpr auto marker_and_size =
		sizeof(detail::magic_marker) + sizeof(detail::supplemental_bytes_size);
	auto size_byte_span = frame.last(marker_and_size).first(1);
	auto bytes_size =
		detail::read_value<detail::supplemental_bytes_size>(size_byte_span);

	// Check the frame is large enough to contain the supplemental bytes
	if (BOOST_UNLIKELY(frame.size() < bytes_size)) { return; }

	// Check that supplemental bytes size is large enough
	if (BOOST_UNLIKELY(bytes_size < min_supplemental_bytes_size)) { return; }

	// Get supplemental bytes region using span
	auto supplemental_bytes = frame.last(bytes_size);

	// Read the tag (first N bytes of supplemental)
	m_tag = supplemental_bytes.first(detail::AES_GCM_128_TRUNCATED_TAG_BYTES);

	// Read the nonce and ranges from remaining supplemental bytes
	// Skip: tag, then read until we hit the size byte and marker at end
	auto nonce_and_ranges = supplemental_bytes.subspan(
		detail::AES_GCM_128_TRUNCATED_TAG_BYTES,
		bytes_size - detail::AES_GCM_128_TRUNCATED_TAG_BYTES -
			sizeof(detail::supplemental_bytes_size) -
			sizeof(detail::magic_marker));

	const auto *read_at = nonce_and_ranges.data();
	const auto *end = read_at + nonce_and_ranges.size();
	m_truncated_nonce = static_cast<uint32_t>(read_leb128(read_at, end));
	if (BOOST_UNLIKELY(read_at == nullptr)) { return; }

	// Read the unencrypted ranges
	auto ranges_size = static_cast<uint8_t>(end - read_at);
	deserialize_unencrypted_ranges(read_at, ranges_size, m_unencrypted_ranges);
	if (BOOST_UNLIKELY(read_at == nullptr)) { return; }

	if (BOOST_UNLIKELY(
			!validate_unencrypted_ranges(m_unencrypted_ranges, frame.size()))) {
		return;
	}

	// This is overly aggressive but will keep reallocations to a minimum
	m_authenticated.reserve(frame.size());
	m_ciphertext.reserve(frame.size());
	m_plaintext.reserve(frame.size());

	m_original_size = frame.size();

	// Split the frame into authenticated and ciphertext bytes
	size_t frame_index{};
	auto actual_frame_size = frame.size() - bytes_size;
	for (const auto &range : m_unencrypted_ranges) {
		auto encrypted_bytes = range.offset - frame_index;
		if (encrypted_bytes > 0) {
			add_ciphertext_bytes(frame.subspan(frame_index, encrypted_bytes));
		}

		add_authenticated_bytes(frame.subspan(range.offset, range.size));
		frame_index = range.offset + range.size;
	}
	if (frame_index < actual_frame_size) {
		add_ciphertext_bytes(
			frame.subspan(frame_index, actual_frame_size - frame_index));
	}

	// Make sure the plaintext buffer is the same size as the ciphertext buffer
	m_plaintext.resize(m_ciphertext.size());

	// We've successfully parsed the frame
	// Mark the frame as encrypted
	m_encrypted = true;
}

size_t InboundFrameProcessor::reconstruct_frame(
	boost::span<uint8_t> frame) const {
	if (!m_encrypted) { return 0; }

	if (m_authenticated.size() + m_plaintext.size() > frame.size()) {
		return 0;
	}

	return do_reconstruct(
		m_unencrypted_ranges, m_authenticated, m_plaintext, frame);
}

void InboundFrameProcessor::add_authenticated_bytes(
	boost::span<const uint8_t> data) {
	m_authenticated.insert(m_authenticated.end(), data.begin(), data.end());
}

void InboundFrameProcessor::add_ciphertext_bytes(
	boost::span<const uint8_t> data) {
	m_ciphertext.insert(m_ciphertext.end(), data.begin(), data.end());
}

void OutboundFrameProcessor::reset() {
	m_frame_codec = Codec::Unknown;
	m_frame_index = 0;
	m_unencrypted_bytes.clear();
	m_encrypted_bytes.clear();
	m_unencrypted_ranges.clear();
}

void OutboundFrameProcessor::process_frame(boost::span<const uint8_t> frame,
										   Codec codec) {
	reset();

	m_frame_codec = codec;
	m_unencrypted_bytes.reserve(frame.size());
	m_encrypted_bytes.reserve(frame.size());

	bool success = false;
	switch (codec) {
		case Codec::Opus:
			success = codec_utils::process_frame_opus(*this, frame);
			break;
		case Codec::Vp8:
			success = codec_utils::process_frame_vp8(*this, frame);
			break;
		case Codec::Vp9:
			success = codec_utils::process_frame_vp9(*this, frame);
			break;
		case Codec::H264:
			success = codec_utils::process_frame_h264(*this, frame);
			break;
		case Codec::H265:
			success = codec_utils::process_frame_h265(*this, frame);
			break;
		case Codec::Av1:
			success = codec_utils::process_frame_av1(*this, frame);
			break;
		default: success = false; break;
	}

	if (BOOST_UNLIKELY(!success)) {
		m_frame_index = 0;
		m_unencrypted_bytes.clear();
		m_encrypted_bytes.clear();
		m_unencrypted_ranges.clear();
		add_encrypted_bytes(frame);
	}

	m_ciphertext_bytes.resize(m_encrypted_bytes.size());
}

size_t OutboundFrameProcessor::reconstruct_frame(boost::span<uint8_t> frame) {
	if (m_unencrypted_bytes.size() + m_ciphertext_bytes.size() > frame.size()) {
		return 0;
	}

	return do_reconstruct(
		m_unencrypted_ranges, m_unencrypted_bytes, m_ciphertext_bytes, frame);
}

void OutboundFrameProcessor::add_unencrypted_bytes(
	boost::span<const uint8_t> bytes) {
	if (!m_unencrypted_ranges.empty() &&
		m_unencrypted_ranges.back().offset + m_unencrypted_ranges.back().size ==
			m_frame_index) {
		// extend the last range
		m_unencrypted_ranges.back().size += bytes.size();
	} else {
		// add a new range (offset, size)
		m_unencrypted_ranges.push_back({m_frame_index, bytes.size()});
	}

	m_unencrypted_bytes.insert(
		m_unencrypted_bytes.end(), bytes.begin(), bytes.end());
	m_frame_index += bytes.size();
}

void OutboundFrameProcessor::add_encrypted_bytes(
	boost::span<const uint8_t> bytes) {
	m_encrypted_bytes.insert(
		m_encrypted_bytes.end(), bytes.begin(), bytes.end());
	m_frame_index += bytes.size();
}

}  // namespace ekizu::dave
