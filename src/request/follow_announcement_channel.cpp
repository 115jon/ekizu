#include <ekizu/json_util.hpp>
#include <ekizu/request/follow_announcement_channel.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const FollowedChannel &f) {
	serialize(j, "channel_id", f.channel_id);
	serialize(j, "webhook_id", f.webhook_id);
}

void from_json(const nlohmann::json &j, FollowedChannel &f) {
	deserialize(j, "channel_id", f.channel_id);
	deserialize(j, "webhook_id", f.webhook_id);
}

FollowAnnouncementChannel::FollowAnnouncementChannel(
	RequestSender sender, Snowflake channel_id, Snowflake webhook_channel_id)
	: m_channel_id{channel_id},
	  m_webhook_channel_id{webhook_channel_id},
	  m_sender{sender} {}

FollowAnnouncementChannel::operator net::HttpRequest() const {
	net::HttpRequest req{
		net::HttpMethod::post,
		fmt::format("/channels/{}/webhooks", m_channel_id), 11,
		nlohmann::json{{"webhook_channel_id", m_webhook_channel_id}}.dump()};

	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();

	return req;
}
}  // namespace ekizu
