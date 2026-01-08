#ifndef EKIZU_INTERACTION_HPP
#define EKIZU_INTERACTION_HPP

#include <ekizu/application_command_data.hpp>
#include <ekizu/entitlement.hpp>
#include <ekizu/guild_member.hpp>
#include <ekizu/interaction_type.hpp>
#include <ekizu/message.hpp>
#include <ekizu/message_component_data.hpp>
#include <ekizu/modal_submit_data.hpp>
#include <ekizu/partial_interaction_channel.hpp>
#include <ekizu/partial_interaction_guild.hpp>
#include <ekizu/permissions.hpp>
#include <ekizu/user.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ekizu {

/**
 * @brief Dictionary of authorizing integration owners for the interaction.
 *
 * Discord sends this as an object with string keys `"0"` and `"1"`, which map
 * to application integration types (guild install, user install).
 *
 * @see
 * https://discord.com/developers/docs/interactions/receiving-and-responding
 */
struct AuthorizingIntegrationOwners {
	/// Owner ID for a guild installation ("0"), if present.
	std::optional<Snowflake> guild_install;
	/// Owner ID for a user installation ("1"), if present.
	std::optional<Snowflake> user_install;
};

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const AuthorizingIntegrationOwners &o);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							AuthorizingIntegrationOwners &o);

using InteractionData =
	std::variant<ApplicationCommandData, MessageComponentData, ModalSubmitData>;

EKIZU_EXPORT void to_json(nlohmann::json &j, const InteractionData &i);

/**
 * @brief Represents an incoming Discord interaction.
 *
 * @see
 * https://discord.com/developers/docs/interactions/receiving-and-responding#interaction-object
 */
struct Interaction {
	/// ID of the interaction.
	Snowflake id;
	/// ID of the application this interaction is for.
	Snowflake application_id;
	/// Type of interaction.
	InteractionType type;
	/// Interaction data payload.
	std::optional<InteractionData> data;

	/// Guild that the interaction was sent from.
	std::optional<Snowflake> guild_id;
	/// Partial guild object (interaction subset).
	std::optional<PartialInteractionGuild> guild;

	/// Channel that the interaction was sent from (partial).
	std::optional<PartialInteractionChannel> channel;
	/// Channel that the interaction was sent from (deprecated).
	std::optional<Snowflake> channel_id;

	/// Guild member data for the invoking user, including permissions.
	std::optional<GuildMember> member;
	/// User object for the invoking user, if invoked in a DM.
	std::optional<User> user;

	/// Continuation token for responding to the interaction.
	std::string token;
	/// Read-only property, always 1.
	int version{};

	/// For components, the message they were attached to.
	std::optional<Message> message;

	/// Bitwise set of permissions the app or bot has within the channel the
	/// interaction was sent from.
	std::optional<Permissions> app_permissions;

	/// Array of entitlements for the invoking user and guild.
	std::optional<std::vector<Entitlement> > entitlements;

	/// Authorizing integration owners for the interaction.
	std::optional<AuthorizingIntegrationOwners> authorizing_integration_owners;

	/// Selected language of the invoking user.
	std::optional<std::string> locale;
	/// Guild's preferred locale, if invoked in a guild.
	std::optional<std::string> guild_locale;

	/// Interaction context type.
	std::optional<InteractionContextType> context;

	/// Maximum attachment size (in bytes) allowed for this interaction context.
	std::optional<uint64_t> attachment_size_limit;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Interaction &i);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Interaction &i);
}  // namespace ekizu

#endif	// EKIZU_INTERACTION_HPP
