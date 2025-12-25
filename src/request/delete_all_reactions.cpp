#include <ekizu/request/delete_all_reactions.hpp>

namespace ekizu {
DeleteAllReactions::DeleteAllReactions(
	RequestSender sender, Snowflake channel_id, Snowflake message_id)
	: m_channel_id{channel_id}, m_message_id{message_id}, m_sender{sender} {}

DeleteAllReactions::operator net::HttpRequest() const {
	return {net::HttpMethod::delete_,
			fmt::format("/channels/{}/messages/{}/reactions/{}", m_channel_id,
						m_message_id),
			11};
}
}  // namespace ekizu