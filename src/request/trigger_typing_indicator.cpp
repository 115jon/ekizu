#include <ekizu/request/trigger_typing_indicator.hpp>

namespace ekizu {
TriggerTypingIndicator::TriggerTypingIndicator(RequestSender sender,
											   Snowflake channel_id)
	: m_channel_id{channel_id}, m_sender{sender} {}

TriggerTypingIndicator::operator net::HttpRequest() const {
	return {net::HttpMethod::post,
			fmt::format("/channels/{}/typing", m_channel_id), 11};
}
}  // namespace ekizu
