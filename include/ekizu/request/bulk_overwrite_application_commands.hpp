#ifndef EKIZU_REQUEST_BULK_OVERWRITE_GLOBAL_APPLICATION_COMMANDS_HPP
#define EKIZU_REQUEST_BULK_OVERWRITE_GLOBAL_APPLICATION_COMMANDS_HPP

#include <ekizu/application_command.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {

/**
 * @brief Fields for creating/updating an application command.
 */
struct ApplicationCommandCreateFields {
	/// Name of command (1-32 characters)
	std::string name;
	/// Localization dictionary for the name field
	std::optional<std::map<std::string, std::string>> name_localizations;
	/// Description for ChatInput commands (1-100 characters)
	std::optional<std::string> description;
	/// Localization dictionary for the description field
	std::optional<std::map<std::string, std::string>> description_localizations;
	/// Parameters for the command
	std::optional<std::vector<ApplicationCommandOption>> options;
	/// Set of permissions represented as a bit set
	std::optional<std::string> default_member_permissions;
	/// Installation context(s) where the command is available
	std::optional<std::vector<ApplicationIntegrationType>> integration_types;
	/// Interaction context(s) where the command can be used
	std::optional<std::vector<InteractionContextType>> contexts;
	/// Type of command, defaults to ChatInput
	ApplicationCommandType type{ApplicationCommandType::ChatInput};
	/// Indicates whether the command is age-restricted
	bool nsfw{false};
};

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const ApplicationCommandCreateFields &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							ApplicationCommandCreateFields &f);
EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const ApplicationCommandOption &opt);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							ApplicationCommandOption &opt);
EKIZU_EXPORT void to_json(nlohmann::json &j, const ApplicationCommand &cmd);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ApplicationCommand &cmd);

/**
 * @brief Bulk overwrite global application commands.
 *
 * Takes a list of application commands, overwriting the existing global
 * command list for this application. Returns 200 and a list of application
 * command objects. Commands that do not already exist will count toward daily
 * application command create limits.
 *
 * @see
 * https://discord.com/developers/docs/interactions/application-commands#bulk-overwrite-global-application-commands
 */
struct BulkOverwriteGlobalApplicationCommands {
	EKIZU_EXPORT BulkOverwriteGlobalApplicationCommands(
		RequestSender sender, Snowflake application_id,
		std::vector<ApplicationCommandCreateFields> commands);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
		void(Result<std::vector<ApplicationCommand>>)) CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<
			CompletionToken, void(Result<std::vector<ApplicationCommand>>)>(
			[this](auto &&handler) {
				m_sender.send<std::vector<ApplicationCommand>>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_application_id;
	std::vector<ApplicationCommandCreateFields> m_commands;
	RequestSender m_sender;
};

/**
 * @brief Bulk overwrite guild application commands.
 *
 * Takes a list of application commands, overwriting the existing command list
 * for this application for the targeted guild. Returns 200 and a list of
 * application command objects.
 *
 * @see
 * https://discord.com/developers/docs/interactions/application-commands#bulk-overwrite-guild-application-commands
 */
struct BulkOverwriteGuildApplicationCommands {
	EKIZU_EXPORT BulkOverwriteGuildApplicationCommands(
		RequestSender sender, Snowflake application_id, Snowflake guild_id,
		std::vector<ApplicationCommandCreateFields> commands);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
		void(Result<std::vector<ApplicationCommand>>)) CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<
			CompletionToken, void(Result<std::vector<ApplicationCommand>>)>(
			[this](auto &&handler) {
				m_sender.send<std::vector<ApplicationCommand>>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_application_id;
	Snowflake m_guild_id;
	std::vector<ApplicationCommandCreateFields> m_commands;
	RequestSender m_sender;
};

}  // namespace ekizu

#endif	// EKIZU_REQUEST_BULK_OVERWRITE_GLOBAL_APPLICATION_COMMANDS_HPP
