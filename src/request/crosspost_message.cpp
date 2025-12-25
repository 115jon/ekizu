#include <ekizu/request/crosspost_message.hpp>

namespace ekizu {
CrosspostMessage::CrosspostMessage(RequestSender sender, Snowflake channel_id,
								   Snowflake message_id)
	: m_channel_id{channel_id}, m_message_id{message_id}, m_sender{sender} {}

CrosspostMessage::operator net::HttpRequest() const {
	return {net::HttpMethod::post,
			fmt::format("/channels/{}/messages/{}/crosspost", m_channel_id,
						m_message_id),
			11};
}
}  // namespace ekizu
