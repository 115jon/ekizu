#include <ekizu/json_util.hpp>
#include <ekizu/request/get_guild.hpp>

namespace ekizu {
GetGuild::GetGuild(RequestSender sender, Snowflake guild_id)
	: m_guild_id{guild_id}, m_sender{sender} {}

GetGuild::operator net::HttpRequest() const {
	return net::HttpRequest{
		net::HttpMethod::get,
		fmt::format("/guilds/{}{}", m_guild_id,
					m_fields.with_counts
						? fmt::format("?with_counts={}", *m_fields.with_counts)
						: ""),
		11};
}
}  // namespace ekizu
