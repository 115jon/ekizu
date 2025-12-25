#include <ekizu/request/unpin_message.hpp>

namespace ekizu {
UnpinMessage::UnpinMessage(RequestSender sender, Snowflake channel_id,
						   Snowflake message_id)
	: m_channel_id{channel_id}, m_message_id{message_id}, m_sender{sender} {}

UnpinMessage::operator net::HttpRequest() const {
	return {net::HttpMethod::delete_,
			fmt::format("/channels/{}/pins/{}", m_channel_id, m_message_id),
			11};
}
}  // namespace ekizu
