#include "leb128.hpp"

namespace ekizu::dave {

size_t leb128_size(uint64_t value) {
	int size{};
	while (value >= 0x80) {
		++size;
		value >>= 7;
	}
	return size + 1;
}

uint64_t read_leb128(const uint8_t *&read_at, const uint8_t *end) {
	uint64_t value{};
	int fill_bits{};
	while (read_at != end && fill_bits < 64 - 7) {
		uint8_t leb_128_byte = *read_at;
		value |= uint64_t{leb_128_byte & 0x7Fu} << fill_bits;
		++read_at;
		fill_bits += 7;
		if ((leb_128_byte & 0x80) == 0) { return value; }
	}
	// Read 9 bytes and didn't find the terminator byte. Check if 10th byte
	// is that terminator, however to fit result into uint64_t it may carry only
	// single bit.
	if (read_at != end && *read_at <= 1) {
		value |= uint64_t{*read_at} << fill_bits;
		++read_at;
		return value;
	}
	// Failed to find terminator leb128 byte.
	read_at = nullptr;
	return 0;
}

size_t write_leb128(uint64_t value, uint8_t *buffer) {
	int size{};
	while (value >= 0x80) {
		buffer[size] = 0x80 | (value & 0x7F);
		++size;
		value >>= 7;
	}
	buffer[size] = static_cast<uint8_t>(value);
	++size;
	return size;
}

}  // namespace ekizu::dave
