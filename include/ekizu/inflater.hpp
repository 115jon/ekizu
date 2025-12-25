#ifndef EKIZU_INFLATER_HPP
#define EKIZU_INFLATER_HPP

#include <boost/core/span.hpp>
#include <ekizu/export.hpp>
#include <ekizu/result.hpp>
#include <memory>

namespace ekizu {
/**
 * @brief A wrapper around zlib's decompression functions. Will be used if
 * the client connected to the gateway using the zlib-streaming compression
 * method.
 */
struct Inflater {
	Inflater(const Inflater &) = delete;
	Inflater &operator=(const Inflater &) = delete;
	Inflater(Inflater &&) noexcept;
	Inflater &operator=(Inflater &&) noexcept;
	EKIZU_EXPORT ~Inflater();

	EKIZU_EXPORT static Result<Inflater> create();

	[[nodiscard]] EKIZU_EXPORT Result<std::string> inflate(
		boost::span<const char> data);

   private:
	struct Impl;

	explicit Inflater(std::unique_ptr<Impl> impl);

	static constexpr size_t BUFFER_SIZE = 1024;

	std::array<char, BUFFER_SIZE> m_buffer{};
	std::unique_ptr<Impl> m_impl;
};
}  // namespace ekizu

#endif	// EKIZU_INFLATER_HPP
