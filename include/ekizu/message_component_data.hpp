#ifndef EKIZU_MESSAGE_COMPONENT_DATA_HPP
#define EKIZU_MESSAGE_COMPONENT_DATA_HPP

#include <ekizu/application_command_data_resolved.hpp>
#include <ekizu/message_component.hpp>
#include <optional>
#include <string>
#include <vector>

namespace ekizu {
/**
 * @brief Message component interaction data.
 *
 * This corresponds to the "Message Component Data Structure" in the Interaction
 * object documentation.
 *
 * @see
 * https://discord.com/developers/docs/interactions/receiving-and-responding
 */
struct MessageComponentData {
	/// The custom ID of the component.
	std::string custom_id;
	/// Component type.
	ComponentType type{};
	/// Values selected by the user (for select menus).
	std::optional<std::vector<std::string>> values;
	/// Resolved entities from selected options.
	std::optional<ApplicationCommandInteractionDataResolved> resolved;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const MessageComponentData &d);
EKIZU_EXPORT void from_json(const nlohmann::json &j, MessageComponentData &d);
}  // namespace ekizu

#endif	// EKIZU_MESSAGE_COMPONENT_DATA_HPP
