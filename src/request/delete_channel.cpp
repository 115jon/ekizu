#include <ekizu/json_util.hpp>
#include <ekizu/request/delete_channel.hpp>

namespace ekizu {
DeleteChannel::DeleteChannel(RequestSender sender, Snowflake channel_id)
	: m_channel_id{channel_id}, m_sender{sender} {}

DeleteChannel::operator net::HttpRequest() const {
	return {net::HttpMethod::delete_, fmt::format("/channels/{}", m_channel_id),
			11};
}
}  // namespace ekizu
