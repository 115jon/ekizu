#include <ekizu/json_util.hpp>
#include <ekizu/request/get_guild_member.hpp>

namespace ekizu {
GetGuildMember::GetGuildMember(RequestSender sender, Snowflake guild_id,
							   Snowflake user_id)
	: m_guild_id{guild_id}, m_user_id{user_id}, m_sender{sender} {}

GetGuildMember::operator net::HttpRequest() const {
	return net::HttpRequest{
		net::HttpMethod::get,
		fmt::format("/guilds/{}/members/{}", m_guild_id, m_user_id), 11};
}
}  // namespace ekizu
