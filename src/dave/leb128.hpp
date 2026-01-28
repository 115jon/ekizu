#ifndef DAVE_LEB128_HPP
#define DAVE_LEB128_HPP

#include <cstdint>

namespace ekizu::dave {

/**
 * @brief Maximum size of LEB128 value
 */
constexpr size_t LEB128_MAX_SIZE = 10;

/**
 * @brief Returns number of bytes needed to store `value` in leb128 format.
 * @param value value to return size for
 * @return size of leb128
 */
size_t leb128_size(uint64_t value);

/**
 * @brief Reads leb128 encoded value and advance read_at by number of bytes
 * consumed. Sets read_at to nullptr on error.
 * @param read_at start position
 * @param end end position
 * @return decoded value
 */
uint64_t read_leb128(const uint8_t *&read_at, const uint8_t *end);

/**
 * @brief Encodes `value` in leb128 format. Assumes buffer has size of
 * at least Leb128Size(value). Returns number of bytes consumed.
 * @param value value to encode
 * @param buffer buffer to encode into
 * @return size of encoding
 */
size_t write_leb128(uint64_t value, uint8_t *buffer);

}  // namespace ekizu::dave

#endif	// DAVE_LEB128_HPP
