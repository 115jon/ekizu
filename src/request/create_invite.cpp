#include <ekizu/request/create_invite.hpp>

namespace ekizu {
void to_json(nlohmann::json &j, const CreateInviteFields &f) {}

void from_json(const nlohmann::json &j, CreateInviteFields &f) {}

CreateInvite::CreateInvite(RequestSender sender, Snowflake channel_id)
	: m_channel_id{channel_id}, m_sender{sender} {}

CreateInvite::operator net::HttpRequest() const {
	auto req = net::HttpRequest{
		net::HttpMethod::post,
		fmt::format("/channels/{}/invites", m_channel_id), 11,
		static_cast<nlohmann::json>(m_fields).dump()};

	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();

	return req;
}
}  // namespace ekizu
