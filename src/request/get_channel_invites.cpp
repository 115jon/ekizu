#include <ekizu/request/get_channel_invites.hpp>

namespace ekizu {
GetChannelInvites::GetChannelInvites(RequestSender sender, Snowflake channel_id)
	: m_channel_id{channel_id}, m_sender{sender} {}

GetChannelInvites::operator net::HttpRequest() const {
	return {net::HttpMethod::get,
			fmt::format("/channels/{}/invites", m_channel_id), 11};
}
}  // namespace ekizu
