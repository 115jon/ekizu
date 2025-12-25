#include <ekizu/request/pin_message.hpp>

namespace ekizu {
PinMessage::PinMessage(RequestSender sender, Snowflake channel_id,
					   Snowflake message_id)
	: m_channel_id{channel_id}, m_message_id{message_id}, m_sender{sender} {}

PinMessage::operator net::HttpRequest() const {
	return {net::HttpMethod::put,
			fmt::format("/channels/{}/pins/{}", m_channel_id, m_message_id),
			11};
}
}  // namespace ekizu
