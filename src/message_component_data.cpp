#include <ekizu/json_util.hpp>
#include <ekizu/message_component_data.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const MessageComponentData &d) {
	serialize(j, "custom_id", d.custom_id);
	serialize(j, "component_type", d.type);
	serialize(j, "values", d.values);
	serialize(j, "resolved", d.resolved);
}

void from_json(const nlohmann::json &j, MessageComponentData &d) {
	deserialize(j, "custom_id", d.custom_id);
	deserialize(j, "component_type", d.type);
	deserialize(j, "values", d.values);
	deserialize(j, "resolved", d.resolved);
}
}  // namespace ekizu
