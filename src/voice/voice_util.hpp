#ifndef EKIZU_VOICE_UTIL_HPP
#define EKIZU_VOICE_UTIL_HPP

#include <array>
#include <boost/core/span.hpp>
#include <boost/endian/conversion.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ekizu::voice {

// Constants
constexpr uint16_t MAX_PACKET_SIZE{3 * 1276};
constexpr uint8_t RTP_HEADER_SIZE{12};

constexpr std::array<std::byte, 3> SILENCE_FRAME{
	std::byte{0xF8}, std::byte{0xFF}, std::byte{0xFE}};

// RTP Header Structure (12 bytes)
struct RTPHeader {
	uint8_t version_padding_extension;	// V(2), P(1), X(1), CC(4)
	uint8_t marker_payload_type;		// M(1), PT(7)
	uint16_t sequence;
	uint32_t timestamp;
	uint32_t ssrc;

	static RTPHeader parse(boost::span<const std::byte> data) {
		if (data.size() < RTP_HEADER_SIZE) { return {}; }

		RTPHeader header{};
		header.version_padding_extension = static_cast<uint8_t>(data[0]);
		header.marker_payload_type = static_cast<uint8_t>(data[1]);

		std::memcpy(&header.sequence, data.data() + 2, 2);
		header.sequence = boost::endian::big_to_native(header.sequence);

		std::memcpy(&header.timestamp, data.data() + 4, 4);
		header.timestamp = boost::endian::big_to_native(header.timestamp);

		std::memcpy(&header.ssrc, data.data() + 8, 4);
		header.ssrc = boost::endian::big_to_native(header.ssrc);

		return header;
	}

	void write_to(std::byte *buffer) const {
		buffer[0] = static_cast<std::byte>(version_padding_extension);
		buffer[1] = static_cast<std::byte>(marker_payload_type);

		auto seq_be = boost::endian::native_to_big(sequence);
		std::memcpy(buffer + 2, &seq_be, 2);

		auto ts_be = boost::endian::native_to_big(timestamp);
		std::memcpy(buffer + 4, &ts_be, 4);

		auto ssrc_be = boost::endian::native_to_big(ssrc);
		std::memcpy(buffer + 8, &ssrc_be, 4);
	}
};

// Utility functions
template <typename T>
inline void put_be(std::byte *ptr, T val) {
	auto be = boost::endian::native_to_big(val);
	std::memcpy(ptr, &be, sizeof(be));
}

template <typename T>
inline T read_be(boost::span<const std::byte> s) {
	if (s.size() < sizeof(T)) { return 0; }
	T val{};
	std::memcpy(&val, s.data(), sizeof(T));
	return boost::endian::big_to_native(val);
}

inline uint16_t read_u16be(boost::span<const std::byte> s) {
	return read_be<uint16_t>(s);
}

inline uint32_t read_u32be(boost::span<const std::byte> s) {
	return read_be<uint32_t>(s);
}

}  // namespace ekizu::voice

#endif	// EKIZU_VOICE_UTIL_HPP
