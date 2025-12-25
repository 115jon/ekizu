#include <fmt/format.h>

#include <ekizu/gateway/voice_server_update.hpp>
#include <ekizu/json_util.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const VoiceServerUpdate &v) {
	serialize(j, "token", v.token);
	serialize(j, "guild_id", v.guild_id);
	serialize(j, "endpoint", v.endpoint);
}

void from_json(const nlohmann::json &j, VoiceServerUpdate &v) {
	deserialize(j, "token", v.token);
	deserialize(j, "guild_id", v.guild_id);
	deserialize(j, "endpoint", v.endpoint);
}
}  // namespace ekizu
