#include <ekizu/json_util.hpp>
#include <ekizu/request/get_guild_channels.hpp>

namespace ekizu {
GetGuildChannels::GetGuildChannels(RequestSender sender, Snowflake guild_id)
	: m_guild_id{guild_id}, m_sender{sender} {}

GetGuildChannels::operator net::HttpRequest() const {
	return net::HttpRequest{net::HttpMethod::get,
							fmt::format("/guilds/{}/channels", m_guild_id), 11};
}
}  // namespace ekizu
