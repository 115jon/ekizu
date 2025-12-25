#include <ekizu/request/get_channel_messages.hpp>

namespace ekizu {
GetChannelMessagesConfigured::GetChannelMessagesConfigured(
	RequestSender sender, Snowflake channel_id, std::optional<uint16_t> limit,
	Type type, Snowflake message_id)
	: m_channel_id{channel_id},
	  m_limit{limit},
	  m_type{type},
	  m_message_id{message_id},
	  m_sender{sender} {}

GetChannelMessagesConfigured::operator net::HttpRequest() const {
	std::string_view type_str = [this] {
		switch (m_type) {
			case Type::Around: return "around";
			case Type::Before: return "before";
			case Type::After: return "after";
		}

		return "";
	}();

	auto url =
		m_limit ? fmt::format("/channels/{}/messages?limit={}&{}={}",
							  m_channel_id, *m_limit, type_str, m_message_id)
				: fmt::format("/channels/{}/messages?{}={}", m_channel_id,
							  type_str, m_message_id);

	return {net::HttpMethod::get, url, 11};
}

GetChannelMessages::GetChannelMessages(RequestSender sender,
									   Snowflake channel_id)
	: m_channel_id{channel_id}, m_sender{sender} {}

GetChannelMessages::operator net::HttpRequest() const {
	auto url = m_limit ? fmt::format("/channels/{}/messages?limit={}",
									 m_channel_id, *m_limit)
					   : fmt::format("/channels/{}/messages", m_channel_id);

	return {net::HttpMethod::get, url, 11};
}
}  // namespace ekizu
