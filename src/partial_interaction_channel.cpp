#include <ekizu/json_util.hpp>
#include <ekizu/partial_interaction_channel.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const PartialInteractionChannel &c) {
	serialize(j, "id", c.id);
	serialize(j, "type", c.type);
	serialize(j, "name", c.name);
	serialize(j, "parent_id", c.parent_id);
	serialize(j, "permissions", c.permissions);
	serialize(j, "thread_metadata", c.thread_metadata);
}

void from_json(const nlohmann::json &j, PartialInteractionChannel &c) {
	deserialize(j, "id", c.id);
	deserialize(j, "type", c.type);
	deserialize(j, "name", c.name);
	deserialize(j, "parent_id", c.parent_id);
	deserialize(j, "permissions", c.permissions);
	deserialize(j, "thread_metadata", c.thread_metadata);
}
}  // namespace ekizu
