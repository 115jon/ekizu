#include <ekizu/request/delete_guild.hpp>

namespace ekizu {
DeleteGuild::DeleteGuild(RequestSender sender, Snowflake guild_id)
	: m_guild_id{guild_id}, m_sender{sender} {}

DeleteGuild::operator net::HttpRequest() const {
	return net::HttpRequest{
		net::HttpMethod::delete_, fmt::format("/guilds/{}", m_guild_id), 11};
}
}  // namespace ekizu
