#include <ekizu/json_util.hpp>
#include <ekizu/request/modify_guild.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const ModifyGuildFields &f) {
	serialize(j, "name", f.name);
	serialize(j, "region", f.region);
	serialize(j, "verification_level", f.verification_level);
	serialize(
		j, "default_message_notifications", f.default_message_notifications);
	serialize(j, "explicit_content_filter", f.explicit_content_filter);
	serialize(j, "afk_channel_id", f.afk_channel_id);
	serialize(j, "afk_timeout", f.afk_timeout);
	serialize(j, "icon", f.icon);
	serialize(j, "owner_id", f.owner_id);
	serialize(j, "splash", f.splash);
	serialize(j, "discovery_splash", f.discovery_splash);
	serialize(j, "banner", f.banner);
	serialize(j, "system_channel_id", f.system_channel_id);
	serialize(j, "system_channel_flags", f.system_channel_flags);
	serialize(j, "rules_channel_id", f.rules_channel_id);
	serialize(j, "public_updates_channel_id", f.public_updates_channel_id);
	serialize(j, "preferred_locale", f.preferred_locale);
	serialize(j, "features", f.features);
	serialize(j, "description", f.description);
	serialize(
		j, "premium_progress_bar_enabled", f.premium_progress_bar_enabled);
	serialize(j, "safety_alerts_channel_id", f.safety_alerts_channel_id);
}

void from_json(const nlohmann::json &j, ModifyGuildFields &f) {
	deserialize(j, "name", f.name);
	deserialize(j, "region", f.region);
	deserialize(j, "verification_level", f.verification_level);
	deserialize(
		j, "default_message_notifications", f.default_message_notifications);
	deserialize(j, "explicit_content_filter", f.explicit_content_filter);
	deserialize(j, "afk_channel_id", f.afk_channel_id);
	deserialize(j, "afk_timeout", f.afk_timeout);
	deserialize(j, "icon", f.icon);
	deserialize(j, "owner_id", f.owner_id);
	deserialize(j, "splash", f.splash);
	deserialize(j, "discovery_splash", f.discovery_splash);
	deserialize(j, "banner", f.banner);
	deserialize(j, "system_channel_id", f.system_channel_id);
	deserialize(j, "system_channel_flags", f.system_channel_flags);
	deserialize(j, "rules_channel_id", f.rules_channel_id);
	deserialize(j, "public_updates_channel_id", f.public_updates_channel_id);
	deserialize(j, "preferred_locale", f.preferred_locale);
	deserialize(j, "features", f.features);
	deserialize(j, "description", f.description);
	deserialize(
		j, "premium_progress_bar_enabled", f.premium_progress_bar_enabled);
	deserialize(j, "safety_alerts_channel_id", f.safety_alerts_channel_id);
}

ModifyGuild::ModifyGuild(RequestSender sender, Snowflake guild_id)
	: m_guild_id{guild_id}, m_sender{sender} {}

ModifyGuild::operator net::HttpRequest() const {
	// Create and return the HTTP request
	net::HttpRequest req{
		net::HttpMethod::patch, fmt::format("/guilds/{}", m_guild_id), 11,
		static_cast<nlohmann::json>(m_fields).dump()};

	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();

	return req;
}
}  // namespace ekizu
