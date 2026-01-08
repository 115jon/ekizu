#ifndef EKIZU_PARTIAL_INTERACTION_GUILD_HPP
#define EKIZU_PARTIAL_INTERACTION_GUILD_HPP

#include <ekizu/export.hpp>
#include <ekizu/guild_feature.hpp>
#include <ekizu/snowflake.hpp>
#include <string>
#include <vector>

namespace ekizu {
/**
 * @brief Partial guild object included in an Interaction.
 *
 * Twilight models this as a small subset containing id, locale, and features.
 *
 * @see
 * https://discord.com/developers/docs/interactions/receiving-and-responding
 */
struct PartialInteractionGuild {
	/// ID of the guild.
	Snowflake id;
	/// Preferred locale of the guild.
	std::string locale;
	/// Enabled guild features.
	std::vector<GuildFeature> features;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const PartialInteractionGuild &g);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							PartialInteractionGuild &g);
}  // namespace ekizu

#endif	// EKIZU_PARTIAL_INTERACTION_GUILD_HPP
