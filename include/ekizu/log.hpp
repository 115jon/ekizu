#ifndef EKIZU_LOG_HPP
#define EKIZU_LOG_HPP

#include <ekizu/export.hpp>
#include <ekizu/log_level.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>

namespace ekizu {
struct Log {
	LogLevel level;
	std::string message;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Log &l);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Log &l);
}  // namespace ekizu

#endif	// EKIZU_LOG_HPP
