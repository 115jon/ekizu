#include <ekizu/json_util.hpp>
#include <ekizu/request/get_guild_invites.hpp>

namespace ekizu {
GetGuildInvites::GetGuildInvites(RequestSender sender, Snowflake guild_id)
	: m_guild_id{guild_id}, m_sender{sender} {}

GetGuildInvites::operator net::HttpRequest() const {
	return net::HttpRequest{net::HttpMethod::get,
							fmt::format("/guilds/{}/invites", m_guild_id), 11};
}
}  // namespace ekizu
