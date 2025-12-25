#ifndef EKIZU_GATEWAY_RESUMED_HPP
#define EKIZU_GATEWAY_RESUMED_HPP

#include <ekizu/export.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ekizu {
struct Resumed {};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Resumed &r);
}  // namespace ekizu

#endif	// EKIZU_GATEWAY_RESUMED_HPP
