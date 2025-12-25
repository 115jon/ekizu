#include <ekizu/request/create_dm.hpp>

namespace ekizu {

CreateDM::CreateDM(RequestSender sender, Snowflake user_id)
	: m_sender{sender}, m_user_id{user_id} {}

CreateDM::operator net::HttpRequest() const {
	net::HttpRequest req{
		net::HttpMethod::post, "/users/@me/channels", 11,
		nlohmann::json{
			{"recipient_id", m_user_id},
		}
			.dump()};
	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();
	return req;
}

}  // namespace ekizu