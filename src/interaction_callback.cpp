#include <ekizu/interaction_callback.hpp>
#include <ekizu/json_util.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const InteractionCallbackActivityInstance &a) {
	serialize(j, "id", a.id);
}

void from_json(const nlohmann::json &j,
			   InteractionCallbackActivityInstance &a) {
	deserialize(j, "id", a.id);
}

void to_json(nlohmann::json &j, const InteractionCallbackResource &r) {
	serialize(j, "type", r.type);
	serialize(j, "activity_instance", r.activity_instance);
	serialize(j, "message", r.message);
}

void from_json(const nlohmann::json &j, InteractionCallbackResource &r) {
	deserialize(j, "type", r.type);
	deserialize(j, "activity_instance", r.activity_instance);
	deserialize(j, "message", r.message);
}

void to_json(nlohmann::json &j, const InteractionCallback &c) {
	serialize(j, "id", c.id);
	serialize(j, "type", c.type);
	serialize(j, "activity_instance_id", c.activity_instance_id);
	serialize(j, "response_message_id", c.response_message_id);
	serialize(j, "response_message_loading", c.response_message_loading);
	serialize(j, "response_message_ephemeral", c.response_message_ephemeral);
}

void from_json(const nlohmann::json &j, InteractionCallback &c) {
	deserialize(j, "id", c.id);
	deserialize(j, "type", c.type);
	deserialize(j, "activity_instance_id", c.activity_instance_id);
	deserialize(j, "response_message_id", c.response_message_id);
	deserialize(j, "response_message_loading", c.response_message_loading);
	deserialize(j, "response_message_ephemeral", c.response_message_ephemeral);
}

void to_json(nlohmann::json &j, const InteractionCallbackResponse &r) {
	serialize(j, "interaction", r.interaction);
	serialize(j, "resource", r.resource);
}

void from_json(const nlohmann::json &j, InteractionCallbackResponse &r) {
	deserialize(j, "interaction", r.interaction);
	deserialize(j, "resource", r.resource);
}
}  // namespace ekizu
