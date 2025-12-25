#include <ekizu/request/add_guild_member_role.hpp>

namespace ekizu {
AddGuildMemberRole::AddGuildMemberRole(RequestSender sender, Snowflake guild_id,
									   Snowflake user_id, Snowflake role_id)
	: m_guild_id{guild_id},
	  m_user_id{user_id},
	  m_role_id{role_id},
	  m_sender{sender} {}

AddGuildMemberRole::operator net::HttpRequest() const {
	return {net::HttpMethod::put,
			fmt::format("/guilds/{}/members/{}/roles/{}", m_guild_id, m_user_id,
						m_role_id),
			11};
}
}  // namespace ekizu
