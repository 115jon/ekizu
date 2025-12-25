#include <ekizu/json_util.hpp>
#include <ekizu/request/add_guild_member.hpp>

namespace ekizu {
using json_util::serialize;

void to_json(nlohmann::json &j, const AddGuildMemberFields &m) {
	serialize(j, "access_token", m.access_token);
	serialize(j, "nick", m.nick);
	serialize(j, "roles", m.roles);
	serialize(j, "mute", m.mute);
	serialize(j, "deaf", m.deaf);
}

AddGuildMember::AddGuildMember(RequestSender sender, Snowflake guild_id,
							   Snowflake user_id, std::string_view access_token)
	: m_guild_id{guild_id},
	  m_user_id{user_id},
	  m_fields{std::string{access_token}},
	  m_sender{sender} {}

AddGuildMember::operator net::HttpRequest() const {
	net::HttpRequest req{
		net::HttpMethod::put,
		fmt::format("/guilds/{}/members/{}", m_guild_id, m_user_id), 11,
		static_cast<nlohmann::json>(m_fields).dump()};

	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();

	return req;
}
}  // namespace ekizu
