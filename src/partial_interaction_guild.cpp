// partial_interaction_guild.cpp
#include <ekizu/json_util.hpp>
#include <ekizu/partial_interaction_guild.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const PartialInteractionGuild &g) {
	serialize(j, "id", g.id);
	serialize(j, "locale", g.locale);
	serialize(j, "features", g.features);
}

void from_json(const nlohmann::json &j, PartialInteractionGuild &g) {
	deserialize(j, "id", g.id);
	deserialize(j, "locale", g.locale);
	deserialize(j, "features", g.features);
}
}  // namespace ekizu
