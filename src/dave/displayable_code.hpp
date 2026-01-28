#ifndef DAVE_DISPLAYABLE_CODE_HPP
#define DAVE_DISPLAYABLE_CODE_HPP

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace ekizu::dave {

/**
 * @brief Generate a displayable privacy code from epoch authenticator data.
 *
 * This converts raw bytes into a human-readable numeric code that can be
 * displayed to users for verification purposes (similar to Signal's safety
 * numbers).
 *
 * @param data The epoch authenticator bytes
 * @param desired_length Number of digits to generate (default: 30)
 * @param group_size Digits per group (default: 5)
 * @return A displayable code string like "12345 67890 12345 67890 12345 67890 "
 */
inline std::string generate_displayable_code(const std::vector<uint8_t> &data,
											 size_t desired_length = 30,
											 size_t group_size = 5) {
	if (data.empty()) { return ""; }

	const auto group_modulus =
		static_cast<size_t>(std::pow(10, static_cast<double>(group_size)));
	std::stringstream result;

	for (size_t i = 0; i < desired_length; i += group_size) {
		size_t group_value{0};

		for (size_t j = group_size; j > 0; --j) {
			size_t index = i + (group_size - j);
			if (index >= data.size()) { break; }
			const size_t next_byte = data.at(index);
			group_value = (group_value << 8) | next_byte;
		}
		group_value %= group_modulus;
		result << std::setw(static_cast<int>(group_size)) << std::setfill('0')
			   << std::to_string(group_value) << " ";
	}

	return result.str();
}

}  // namespace ekizu::dave

#endif	// DAVE_DISPLAYABLE_CODE_HPP
