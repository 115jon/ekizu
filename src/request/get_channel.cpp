#include <ekizu/request/get_channel.hpp>

namespace ekizu {
GetChannel::GetChannel(RequestSender sender, Snowflake channel_id)
	: m_channel_id{channel_id}, m_sender{sender} {}

GetChannel::operator net::HttpRequest() const {
	return net::HttpRequest{
		net::HttpMethod::get, fmt::format("/channels/{}", m_channel_id), 11};
}
}  // namespace ekizu
