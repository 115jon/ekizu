#ifndef EKIZU_PARTIAL_INTERACTION_CHANNEL_HPP
#define EKIZU_PARTIAL_INTERACTION_CHANNEL_HPP

#include <ekizu/channel.hpp>
#include <ekizu/export.hpp>
#include <ekizu/permissions.hpp>
#include <ekizu/snowflake.hpp>
#include <optional>
#include <string>

namespace ekizu {
/**
 * @brief Partial channel object included in an Interaction.
 *
 * Partial channel objects in interactions are documented as a small subset
 * (id, name, type, permissions), with thread fields included for threads.
 *
 * @see
 * https://discord.com/developers/docs/interactions/receiving-and-responding
 */
struct PartialInteractionChannel {
	/// ID of the channel.
	Snowflake id;
	/// Type of the channel.
	ChannelType type{};
	/// Name of the channel.
	std::string name;
	/// ID of the channel the thread was created in.
	std::optional<Snowflake> parent_id;
	/// Computed permissions, including overwrites, for the invoking user.
	Permissions permissions{};
	/// Metadata about a thread.
	std::optional<ThreadMetadata> thread_metadata;
};

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const PartialInteractionChannel &c);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							PartialInteractionChannel &c);
}  // namespace ekizu

#endif	// EKIZU_PARTIAL_INTERACTION_CHANNEL_HPP
