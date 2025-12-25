#include <ekizu/json_util.hpp>
#include <ekizu/request/get_guild_preview.hpp>

namespace ekizu {
GetGuildPreview::GetGuildPreview(RequestSender sender, Snowflake guild_id)
	: m_guild_id{guild_id}, m_sender{sender} {}

GetGuildPreview::operator net::HttpRequest() const {
	return net::HttpRequest{net::HttpMethod::get,
							fmt::format("/guilds/{}/preview", m_guild_id), 11};
}
}  // namespace ekizu
