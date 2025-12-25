#include <ekizu/json_util.hpp>
#include <ekizu/request/get_guild_ban.hpp>

namespace ekizu {
GetGuildBan::GetGuildBan(RequestSender sender, Snowflake guild_id,
						 Snowflake user_id)
	: m_guild_id{guild_id}, m_user_id{user_id}, m_sender{sender} {}

GetGuildBan::operator net::HttpRequest() const {
	return {net::HttpMethod::get,
			fmt::format("/guilds/{}/bans/{}", m_guild_id, m_user_id), 11};
}
}  // namespace ekizu
