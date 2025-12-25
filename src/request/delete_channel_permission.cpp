#include <ekizu/request/delete_channel_permission.hpp>

namespace ekizu {
DeleteChannelPermission::DeleteChannelPermission(
	RequestSender sender, Snowflake channel_id, Snowflake overwrite_id)
	: m_channel_id{channel_id},
	  m_overwrite_id{overwrite_id},
	  m_sender{sender} {}

DeleteChannelPermission::operator net::HttpRequest() const {
	return {net::HttpMethod::delete_,
			fmt::format(
				"/channels/{}/permissions/{}", m_channel_id, m_overwrite_id),
			11};
}
}  // namespace ekizu
