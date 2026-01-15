#include <ekizu/json_util.hpp>
#include <ekizu/request/create_guild_channel.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const CreateGuildChannelFields &f) {
	serialize(j, "name", f.name);
	serialize(j, "type", f.type);
	serialize(j, "topic", f.topic);
	serialize(j, "bitrate", f.bitrate);
	serialize(j, "user_limit", f.user_limit);
	serialize(j, "rate_limit_per_user", f.rate_limit_per_user);
	serialize(j, "position", f.position);
	serialize(j, "permission_overwrites", f.permission_overwrites);
	serialize(j, "parent_id", f.parent_id);
	serialize(j, "nsfw", f.nsfw);
	serialize(j, "rtc_region", f.rtc_region);
	serialize(j, "video_quality_mode", f.video_quality_mode);
	serialize(
		j, "default_auto_archive_duration", f.default_auto_archive_duration);
	serialize(j, "default_reaction_emoji", f.default_reaction_emoji);
	serialize(j, "available_tags", f.available_tags);
	serialize(j, "default_sort_order", f.default_sort_order);
	serialize(j, "default_forum_layout", f.default_forum_layout);
	serialize(j, "default_thread_rate_limit_per_user",
			  f.default_thread_rate_limit_per_user);
}

void from_json(const nlohmann::json &j, CreateGuildChannelFields &f) {
	deserialize(j, "name", f.name);
	deserialize(j, "type", f.type);
	deserialize(j, "topic", f.topic);
	deserialize(j, "bitrate", f.bitrate);
	deserialize(j, "user_limit", f.user_limit);
	deserialize(j, "rate_limit_per_user", f.rate_limit_per_user);
	deserialize(j, "position", f.position);
	deserialize(j, "permission_overwrites", f.permission_overwrites);
	deserialize(j, "parent_id", f.parent_id);
	deserialize(j, "nsfw", f.nsfw);
	deserialize(j, "rtc_region", f.rtc_region);
	deserialize(j, "video_quality_mode", f.video_quality_mode);
	deserialize(
		j, "default_auto_archive_duration", f.default_auto_archive_duration);
	deserialize(j, "default_reaction_emoji", f.default_reaction_emoji);
	deserialize(j, "available_tags", f.available_tags);
	deserialize(j, "default_sort_order", f.default_sort_order);
	deserialize(j, "default_forum_layout", f.default_forum_layout);
	deserialize(j, "default_thread_rate_limit_per_user",
				f.default_thread_rate_limit_per_user);
}

CreateGuildChannel::CreateGuildChannel(RequestSender sender, Snowflake guild_id,
									   std::string name)
	: m_guild_id{guild_id}, m_fields{std::move(name)}, m_sender{sender} {}

CreateGuildChannel::operator net::HttpRequest() const {
	net::HttpRequest req{
		net::HttpMethod::post, fmt::format("/guilds/{}/channels", m_guild_id),
		11,
		static_cast<nlohmann::json>(m_fields).dump(
			-1, ' ', false, nlohmann::json::error_handler_t::replace)};

	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();

	return req;
}
}  // namespace ekizu
