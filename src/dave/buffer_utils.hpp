#ifndef DAVE_BUFFER_UTILS_HPP
#define DAVE_BUFFER_UTILS_HPP

/// @file buffer_utils.hpp
/// @brief Safe buffer manipulation utilities to replace raw memcpy/pointer
/// arithmetic

#include <array>
#include <boost/core/span.hpp>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>

namespace ekizu::dave::detail {

/// Copy bytes from src to dst (asserts dst has enough space)
inline void copy_bytes(boost::span<uint8_t> dst,
					   boost::span<const uint8_t> src) noexcept {
	assert(dst.size() >= src.size());
	std::memcpy(dst.data(), src.data(), src.size());
}

/// Copy bytes to a specific offset in dst
inline void copy_at(boost::span<uint8_t> dst, size_t offset,
					boost::span<const uint8_t> src) noexcept {
	assert(offset + src.size() <= dst.size());
	copy_bytes(dst.subspan(offset, src.size()), src);
}

/// Compare bytes for equality
inline bool bytes_equal(boost::span<const uint8_t> a,
						boost::span<const uint8_t> b) noexcept {
	if (a.size() != b.size()) { return false; }
	return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

/// Check if buffer ends with specific bytes (compile-time size)
template <size_t N>
inline bool ends_with(boost::span<const uint8_t> buf,
					  const std::array<uint8_t, N> &suffix) noexcept {
	if (buf.size() < N) { return false; }
	auto tail = buf.last(N);
	return std::memcmp(tail.data(), suffix.data(), N) == 0;
}

/// Check if buffer ends with specific bytes (runtime size)
inline bool ends_with(boost::span<const uint8_t> buf,
					  boost::span<const uint8_t> suffix) noexcept {
	if (buf.size() < suffix.size()) { return false; }
	auto tail = buf.last(suffix.size());
	return std::memcmp(tail.data(), suffix.data(), suffix.size()) == 0;
}

/// Check if buffer ends with a trivially copyable value (e.g., uint16_t marker)
template <typename T>
inline bool ends_with(boost::span<const uint8_t> buf, const T &value) noexcept {
	static_assert(std::is_trivially_copyable_v<T>);
	if (buf.size() < sizeof(T)) { return false; }
	auto tail = buf.last(sizeof(T));
	return std::memcmp(tail.data(), &value, sizeof(T)) == 0;
}

/// Read a trivially copyable value from buffer (unaligned read)
template <typename T>
inline T read_value(boost::span<const uint8_t> buf) noexcept {
	static_assert(std::is_trivially_copyable_v<T>);
	assert(buf.size() >= sizeof(T));
	T value;
	std::memcpy(&value, buf.data(), sizeof(T));
	return value;
}

/// Read a trivially copyable value from buffer at offset
template <typename T>
inline T read_value_at(boost::span<const uint8_t> buf, size_t offset) noexcept {
	assert(offset + sizeof(T) <= buf.size());
	return read_value<T>(buf.subspan(offset, sizeof(T)));
}

/// Write a trivially copyable value to buffer (unaligned write)
template <typename T>
inline void write_value(boost::span<uint8_t> buf, T value) noexcept {
	static_assert(std::is_trivially_copyable_v<T>);
	assert(buf.size() >= sizeof(T));
	std::memcpy(buf.data(), &value, sizeof(T));
}

/// Write a trivially copyable value to buffer at offset
template <typename T>
inline void write_value_at(boost::span<uint8_t> buf, size_t offset,
						   T value) noexcept {
	assert(offset + sizeof(T) <= buf.size());
	write_value(buf.subspan(offset, sizeof(T)), value);
}

/// Safe checked addition (returns nullopt on overflow)
[[nodiscard]] inline std::optional<size_t> checked_add(size_t a,
													   size_t b) noexcept {
	if (a > std::numeric_limits<size_t>::max() - b) { return std::nullopt; }
	return a + b;
}

/// Safe checked multiply (returns nullopt on overflow)
[[nodiscard]] inline std::optional<size_t> checked_mul(size_t a,
													   size_t b) noexcept {
	if (b != 0 && a > std::numeric_limits<size_t>::max() / b) {
		return std::nullopt;
	}
	return a * b;
}

/// Get the last N bytes of a buffer as a subspan
template <size_t N>
[[nodiscard]] inline boost::span<const uint8_t> last_bytes(
	boost::span<const uint8_t> buf) noexcept {
	assert(buf.size() >= N);
	return buf.last(N);
}

/// Get the last N bytes of a buffer as a subspan (runtime size)
[[nodiscard]] inline boost::span<const uint8_t> last_bytes(
	boost::span<const uint8_t> buf, size_t n) noexcept {
	assert(buf.size() >= n);
	return buf.last(n);
}

}  // namespace ekizu::dave::detail

#endif	// DAVE_BUFFER_UTILS_HPP
