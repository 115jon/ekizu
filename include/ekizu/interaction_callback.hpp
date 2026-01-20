#ifndef EKIZU_INTERACTION_CALLBACK_HPP
#define EKIZU_INTERACTION_CALLBACK_HPP

#include <ekizu/interaction_response_type.hpp>
#include <ekizu/interaction_type.hpp>
#include <ekizu/message.hpp>

namespace ekizu {
struct InteractionCallbackActivityInstance {
	std::string id;
};

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const InteractionCallbackActivityInstance &a);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							InteractionCallbackActivityInstance &a);

struct InteractionCallbackResource {
	InteractionResponseType type{};
	std::optional<InteractionCallbackActivityInstance> activity_instance;
	std::optional<Message> message;
};

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const InteractionCallbackResource &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							InteractionCallbackResource &r);

struct InteractionCallback {
	Snowflake id;
	InteractionType type{};
	std::optional<std::string> activity_instance_id;
	std::optional<Snowflake> response_message_id;
	std::optional<bool> response_message_loading;
	std::optional<bool> response_message_ephemeral;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const InteractionCallback &c);
EKIZU_EXPORT void from_json(const nlohmann::json &j, InteractionCallback &c);

struct InteractionCallbackResponse {
	InteractionCallback interaction;
	std::optional<InteractionCallbackResource> resource;
};

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const InteractionCallbackResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							InteractionCallbackResponse &r);
}  // namespace ekizu

#endif	// EKIZU_INTERACTION_CALLBACK_HPP
