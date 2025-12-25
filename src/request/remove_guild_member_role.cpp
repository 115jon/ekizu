#include <ekizu/request/remove_guild_member_role.hpp>

namespace ekizu {
RemoveGuildMemberRole::RemoveGuildMemberRole(
	RequestSender sender, Snowflake guild_id, Snowflake user_id,
	Snowflake role_id)
	: m_guild_id{guild_id},
	  m_user_id{user_id},
	  m_role_id{role_id},
	  m_sender{sender} {}

RemoveGuildMemberRole::operator net::HttpRequest() const {
	return {net::HttpMethod::delete_,
			fmt::format("/guilds/{}/members/{}/roles/{}", m_guild_id, m_user_id,
						m_role_id),
			11};
}
}  // namespace ekizu
