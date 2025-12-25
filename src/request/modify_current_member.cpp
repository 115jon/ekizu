#include <ekizu/json_util.hpp>
#include <ekizu/request/modify_current_member.hpp>

namespace ekizu {
using json_util::serialize;

void to_json(nlohmann::json &j, const ModifyCurrentMemberFields &m) {
	serialize(j, "nick", m.nick);
}

ModifyCurrentMember::ModifyCurrentMember(RequestSender sender,
										 Snowflake guild_id)
	: m_guild_id{guild_id}, m_sender{sender} {}

ModifyCurrentMember::operator net::HttpRequest() const {
	net::HttpRequest req{net::HttpMethod::patch,
						 fmt::format("/guilds/{}/members/@me", m_guild_id), 11,
						 static_cast<nlohmann::json>(m_fields).dump()};

	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();

	return req;
}
}  // namespace ekizu
