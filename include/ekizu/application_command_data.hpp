#ifndef EKIZU_APPLICATION_COMMAND_DATA_HPP
#define EKIZU_APPLICATION_COMMAND_DATA_HPP

#include <ekizu/application_command.hpp>
#include <ekizu/application_command_data_resolved.hpp>
#include <ekizu/snowflake.hpp>
#include <optional>
#include <string>
#include <vector>

namespace ekizu {

/**
 * @brief Application command interaction option payload.
 *
 * This is recursive: options can contain nested options for subcommands and
 * groups, or a typed value for leaf options.
 *
 * @see
 * https://discord.com/developers/docs/interactions/receiving-and-responding#interaction-object-application-command-interaction-data-option-structure
 */
struct ApplicationCommandInteractionDataOption {
	/// Name of the option.
	std::string name;
	/// Type of the option.
	ApplicationCommandOptionType type{ApplicationCommandOptionType::Unknown};
	/// Typed value for leaf options.
	std::optional<ApplicationCommandOptionValue> value;
	/// Nested options (for subcommand/group).
	std::vector<ApplicationCommandInteractionDataOption> options;
	/// True if this option is currently focused (autocomplete).
	std::optional<bool> focused;
};

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const ApplicationCommandInteractionDataOption &c);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							ApplicationCommandInteractionDataOption &c);

struct ApplicationCommandData {
	/// ID of the command.
	Snowflake id;
	/// Name of the command.
	std::string name;
	/// Type of the command.
	ApplicationCommandType type{ApplicationCommandType::Unknown};
	/// Resolved data from the interaction's options.
	std::optional<ApplicationCommandInteractionDataResolved> resolved;
	/// List of options specified by the user.
	std::vector<ApplicationCommandInteractionDataOption> options;
	/// ID of the guild the command is registered to.
	std::optional<Snowflake> guild_id;
	/// If this is a user or message command, the ID of the targeted
	/// user/message.
	std::optional<Snowflake> target_id;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const ApplicationCommandData &c);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ApplicationCommandData &c);
}  // namespace ekizu

#endif	// EKIZU_APPLICATION_COMMAND_DATA_HPP
