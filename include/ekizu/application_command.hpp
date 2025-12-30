#ifndef EKIZU_APPLICATION_COMMAND_HPP
#define EKIZU_APPLICATION_COMMAND_HPP

#include <cstdint>
#include <ekizu/channel_type.hpp>
#include <ekizu/interaction_context_type.hpp>
#include <ekizu/snowflake.hpp>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ekizu {

/**
 * @brief Application command option type.
 * @see
 * https://discord.com/developers/docs/interactions/application-commands#application-command-object-application-command-option-type
 */
enum class ApplicationCommandOptionType : uint8_t {
	Unknown = 0,
	Subcommand = 1,
	SubcommandGroup = 2,
	String = 3,
	Integer = 4,
	Boolean = 5,
	User = 6,
	Channel = 7,
	Role = 8,
	Mentionable = 9,
	Number = 10,
	Attachment = 11,
};

/**
 * @brief Application command types.
 * @see
 * https://discord.com/developers/docs/interactions/application-commands#application-command-object-application-command-types
 */
enum class ApplicationCommandType : uint8_t {
	Unknown = 0,
	ChatInput = 1,
	User = 2,
	Message = 3,
	PrimaryEntryPoint = 4,
};

/**
 * @brief Installation context for the command.
 * @see
 * https://discord.com/developers/docs/resources/application#application-object-application-integration-types
 */
enum class ApplicationIntegrationType : uint8_t {
	GuildInstall = 0,
	UserInstall = 1,
};

/**
 * @brief Variant to hold the value of an option (Interaction) or default value.
 */
using ApplicationCommandOptionValue =
	std::variant<std::monostate, std::string, int64_t, double, bool, Snowflake>;

/**
 * @brief Variant specifically for Choice values (String, Integer, Number).
 */
using ApplicationCommandOptionChoiceValue =
	std::variant<std::string, int64_t, double>;

/**
 * @brief A choice for an option.
 * @see
 * https://discord.com/developers/docs/interactions/application-commands#application-command-object-application-command-option-choice-structure
 */
struct ApplicationCommandOptionChoice {
	/// 1-100 character choice name
	std::string name;
	/// Localization dictionary for the name field
	std::optional<std::map<std::string, std::string>> name_localizations;
	/// Value for the choice (string, integer, or double)
	ApplicationCommandOptionChoiceValue value;
};

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const ApplicationCommandOptionChoice &c);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							ApplicationCommandOptionChoice &c);

/**
 * @brief A single option for an application command.
 * @see
 * https://discord.com/developers/docs/interactions/application-commands#application-command-object-application-command-option-structure
 */
struct ApplicationCommandOption {
	/// Type of option
	ApplicationCommandOptionType type{ApplicationCommandOptionType::Unknown};

	/// 1-32 character name
	std::string name;

	/// Localization dictionary for the name field
	std::optional<std::map<std::string, std::string>> name_localizations;

	/// 1-100 character description
	std::string description;

	/// Localization dictionary for the description field
	std::optional<std::map<std::string, std::string>> description_localizations;

	/// If the parameter is required or optional--default false
	bool required{false};

	/// Choices for String, Integer, and Number types for the user to pick from
	std::optional<std::vector<ApplicationCommandOptionChoice>> choices;

	/// If the option is a subcommand or subcommand group type, these nested
	/// options will be the parameters
	std::optional<std::vector<ApplicationCommandOption>> options;

	/// If the option is a channel type, the channels shown will be restricted
	/// to these types
	std::optional<std::vector<ChannelType>> channel_types;

	/// If the option is an INTEGER or NUMBER type, the minimum value permitted
	std::optional<std::variant<int64_t, double>> min_value;

	/// If the option is an INTEGER or NUMBER type, the maximum value permitted
	std::optional<std::variant<int64_t, double>> max_value;

	/// For STRING option, minimum allowed length (0-6000)
	std::optional<int> min_length;

	/// For STRING option, maximum allowed length (1-6000)
	std::optional<int> max_length;

	/// If autocomplete interactions are enabled for this String, Integer, or
	/// Number type option
	bool autocomplete{false};

	/// (Interaction Only) The value of the option from an interaction payload
	ApplicationCommandOptionValue value;
};

/**
 * @brief Represents an Application Command.
 * @see
 * https://discord.com/developers/docs/interactions/application-commands#application-command-object-application-command-structure
 */
struct ApplicationCommand {
	/// Unique ID of command
	Snowflake id{};

	/// Type of command, defaults to 1 (ChatInput)
	ApplicationCommandType type{ApplicationCommandType::ChatInput};

	/// ID of the parent application
	Snowflake application_id{};

	/// Guild ID of the command, if not global
	std::optional<Snowflake> guild_id;

	/// 1-32 character name
	std::string name;

	/// Localization dictionary for the name field
	std::optional<std::map<std::string, std::string>> name_localizations;

	/// 1-100 character description
	std::string description;

	/// Localization dictionary for the description field
	std::optional<std::map<std::string, std::string>> description_localizations;

	/// Parameters for the command
	std::optional<std::vector<ApplicationCommandOption>> options;

	/// Set of permissions represented as a bit set
	std::optional<std::string> default_member_permissions;

	/// (Deprecated) Indicates whether the command is available in DMs with the
	/// app Use `contexts` instead.
	std::optional<bool> dm_permission;

	/// (Deprecated) Default whether the command is enabled.
	std::optional<bool> default_permission;

	/// Indicates whether the command is age-restricted
	bool nsfw{false};

	/// Installation context(s) where the command is available
	/// Defaults to GUILD_INSTALL (0)
	std::optional<std::vector<ApplicationIntegrationType>> integration_types;

	/// Interaction context(s) where the command can be used
	/// Defaults to all for new commands (Guild, BotDm, PrivateChannel)
	std::optional<std::vector<InteractionContextType>> contexts;

	/// Autoincrementing version identifier updated during substantial record
	/// changes
	Snowflake version{};
};

}  // namespace ekizu

#endif	// EKIZU_APPLICATION_COMMAND_HPP
