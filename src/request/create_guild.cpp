#include <ekizu/json_util.hpp>
#include <ekizu/request/create_guild.hpp>

namespace ekizu {
using json_util::serialize;

void to_json(nlohmann::json &j, const RoleFields &f) {
	serialize(j, "id", f.id);
	serialize(j, "name", f.name);
	serialize(j, "color", f.color);
	serialize(j, "hoist", f.hoist);
	serialize(j, "position", f.position);
	serialize(j, "permissions", f.permissions);
	serialize(j, "mentionable", f.mentionable);
}

void to_json(nlohmann::json &j, const CategoryChannelFields &f) {
	serialize(j, "id", f.id);
	serialize(j, "type", f.type);
	serialize(j, "name", f.name);
	serialize(j, "permission_overwrites", f.permission_overwrites);
}

void to_json(nlohmann::json &j, const TextChannelFields &f) {
	serialize(j, "id", f.id);
	serialize(j, "type", f.type);
	serialize(j, "name", f.name);
	serialize(j, "topic", f.topic);
	serialize(j, "nsfw", f.nsfw);
	serialize(j, "rate_limit_per_user", f.rate_limit_per_user);
	serialize(j, "parent_id", f.parent_id);
}

void to_json(nlohmann::json &j, const VoiceChannelFields &f) {
	serialize(j, "id", f.id);
	serialize(j, "type", f.type);
	serialize(j, "permission_overwrites", f.permission_overwrites);
	serialize(j, "name", f.name);
	serialize(j, "bitrate", f.bitrate);
	serialize(j, "user_limit", f.user_limit);
	serialize(j, "parent_id", f.parent_id);
}

void to_json(nlohmann::json &j, const GuildChannelFields &f) {
	std::visit([&j](auto &&v) { to_json(j, v); }, f);
}

void to_json(nlohmann::json &j, const CreateGuildFields &f) {
	serialize(j, "name", f.name);
	serialize(j, "region", f.region);
	serialize(j, "icon", f.icon);
	serialize(j, "verification_level", f.verification_level);
	serialize(
		j, "default_message_notifications", f.default_message_notifications);
	serialize(j, "explicit_content_filter", f.explicit_content_filter);
	serialize(j, "roles", f.roles);
	serialize(j, "channels", f.channels);
}

CreateGuild::CreateGuild(RequestSender sender, std::string_view name)
	: m_fields{std::string{name}}, m_sender{sender} {}

CreateGuild::operator net::HttpRequest() const {
	net::HttpRequest req{net::HttpMethod::post, "/guilds", 11,
						 static_cast<nlohmann::json>(m_fields).dump()};

	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();

	return req;
}
}  // namespace ekizu
