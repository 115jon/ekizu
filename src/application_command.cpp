#include <ekizu/application_command.hpp>
#include <ekizu/json_util.hpp>

namespace ekizu {

using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const ApplicationCommandOptionChoice &c) {
	serialize(j, "name", c.name);
	serialize(j, "name_localizations", c.name_localizations);
	std::visit([&j](const auto &v) { serialize(j, "value", v); }, c.value);
}

void from_json(const nlohmann::json &j, ApplicationCommandOptionChoice &c) {
	deserialize(j, "name", c.name);
	deserialize(j, "name_localizations", c.name_localizations);
	std::visit([&j](auto &v) { deserialize(j, "value", v); }, c.value);
}

}  // namespace ekizu
