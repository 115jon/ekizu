#include <ekizu/json_util.hpp>
#include <ekizu/request/modify_channel.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const ModifyChannelFields &f) {
	serialize(j, "name", f.name);
	serialize(j, "type", f.type);
	serialize(j, "position", f.position);
	serialize(j, "topic", f.topic);
	serialize(j, "nsfw", f.nsfw);
	serialize(j, "rate_limit_per_user", f.rate_limit_per_user);
	serialize(j, "bitrate", f.bitrate);
	serialize(j, "user_limit", f.user_limit);
	serialize(j, "permission_overwrites", f.permission_overwrites);
	serialize(j, "parent_id", f.parent_id);
	serialize(j, "rtc_region", f.rtc_region);
	serialize(j, "video_quality_mode", f.video_quality_mode);
	serialize(j, "flags", f.flags);
	serialize(j, "available_tags", f.available_tags);
	serialize(j, "default_reaction_emoji", f.default_reaction_emoji);
	serialize(j, "default_thread_rate_limit_per_user",
			  f.default_thread_rate_limit_per_user);
	serialize(j, "default_sort_order", f.default_sort_order);
	serialize(j, "default_forum_layout", f.default_forum_layout);
}

void from_json(const nlohmann::json &j, ModifyChannelFields &f) {
	deserialize(j, "name", f.name);
	deserialize(j, "type", f.type);
	deserialize(j, "position", f.position);
	deserialize(j, "topic", f.topic);
	deserialize(j, "nsfw", f.nsfw);
	deserialize(j, "rate_limit_per_user", f.rate_limit_per_user);
	deserialize(j, "bitrate", f.bitrate);
	deserialize(j, "user_limit", f.user_limit);
	deserialize(j, "permission_overwrites", f.permission_overwrites);
	deserialize(j, "parent_id", f.parent_id);
	deserialize(j, "rtc_region", f.rtc_region);
	deserialize(j, "video_quality_mode", f.video_quality_mode);
	deserialize(j, "flags", f.flags);
	deserialize(j, "available_tags", f.available_tags);
	deserialize(j, "default_reaction_emoji", f.default_reaction_emoji);
	deserialize(j, "default_thread_rate_limit_per_user",
				f.default_thread_rate_limit_per_user);
	deserialize(j, "default_sort_order", f.default_sort_order);
	deserialize(j, "default_forum_layout", f.default_forum_layout);
}

ModifyChannel::ModifyChannel(RequestSender sender, Snowflake channel_id)
	: m_channel_id{channel_id}, m_sender{sender} {}

ModifyChannel::operator net::HttpRequest() const {
	net::HttpRequest req{
		net::HttpMethod::patch, fmt::format("/channels/{}", m_channel_id), 11,
		static_cast<nlohmann::json>(m_fields).dump()};

	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();

	return req;
}
}  // namespace ekizu
