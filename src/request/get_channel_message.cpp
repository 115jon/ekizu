#include <ekizu/request/get_channel_message.hpp>

namespace ekizu {
GetChannelMessage::GetChannelMessage(RequestSender sender, Snowflake channel_id,
									 Snowflake message_id)
	: m_channel_id{channel_id}, m_message_id{message_id}, m_sender{sender} {}

GetChannelMessage::operator net::HttpRequest() const {
	return {net::HttpMethod::get,
			fmt::format("/channels/{}/messages/{}", m_channel_id, m_message_id),
			11};
}
}  // namespace ekizu
