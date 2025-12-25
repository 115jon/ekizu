#include <ekizu/request/remove_guild_member.hpp>

namespace ekizu {
RemoveGuildMember::RemoveGuildMember(RequestSender sender, Snowflake guild_id,
									 Snowflake user_id)
	: m_guild_id{guild_id}, m_user_id{user_id}, m_sender{sender} {}

RemoveGuildMember::operator net::HttpRequest() const {
	return {net::HttpMethod::delete_,
			fmt::format("/guilds/{}/members/{}", m_guild_id, m_user_id), 11};
}
}  // namespace ekizu
