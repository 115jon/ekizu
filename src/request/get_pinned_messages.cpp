#include <ekizu/json_util.hpp>
#include <ekizu/request/get_pinned_messages.hpp>

namespace ekizu {
GetPinnedMessages::GetPinnedMessages(RequestSender sender, Snowflake channel_id)
	: m_channel_id{channel_id}, m_sender{sender} {}

GetPinnedMessages::operator net::HttpRequest() const {
	return {
		net::HttpMethod::get,
		fmt::format("/channels/{}/pins", m_channel_id),
		11,
	};
}
}  // namespace ekizu
