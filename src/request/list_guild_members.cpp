#include <ekizu/json_util.hpp>
#include <ekizu/request/list_guild_members.hpp>

namespace ekizu {
using json_util::deserialize;

ListGuildMembers::ListGuildMembers(RequestSender sender, Snowflake guild_id)
	: m_guild_id{guild_id}, m_sender{sender} {}

ListGuildMembers::operator net::HttpRequest() const {
	auto url = fmt::format("/guilds/{}/members", m_guild_id);

	if (m_fields.limit) { url += fmt::format("?limit={}", *m_fields.limit); }

	if (m_fields.after) {
		url += fmt::format(
			"{}after={}", m_fields.limit ? '&' : '?', *m_fields.after);
	}

	return {net::HttpMethod::get, url, 11};
}
}  // namespace ekizu
