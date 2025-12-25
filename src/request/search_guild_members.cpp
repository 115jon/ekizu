#include <ekizu/json_util.hpp>
#include <ekizu/request/search_guild_members.hpp>

namespace ekizu {
using json_util::deserialize;

SearchGuildMembers::SearchGuildMembers(RequestSender sender, Snowflake guild_id)
	: m_guild_id{guild_id}, m_sender{sender} {}

SearchGuildMembers::operator net::HttpRequest() const {
	auto url = fmt::format("/guilds/{}/members/search", m_guild_id);

	if (m_fields.query) { url += fmt::format("?query={}", *m_fields.query); }

	if (m_fields.limit) {
		url += fmt::format(
			"{}limit={}", m_fields.query ? '&' : '?', *m_fields.limit);
	}

	return {net::HttpMethod::get, url, 11};
}
}  // namespace ekizu
