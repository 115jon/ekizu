#include <ekizu/request/modify_current_user.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const ModifyCurrentUserFields &f) {
	serialize(j, "avatar", f.avatar);
	serialize(j, "username", f.username);
}

void from_json(const nlohmann::json &j, ModifyCurrentUserFields &f) {
	deserialize(j, "avatar", f.avatar);
	deserialize(j, "username", f.username);
}

ModifyCurrentUser::ModifyCurrentUser(RequestSender sender) : m_sender{sender} {}

ModifyCurrentUser::operator net::HttpRequest() const {
	net::HttpRequest req{net::HttpMethod::patch, "/users/@me", 11,
						 static_cast<nlohmann::json>(m_fields).dump()};

	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();

	return req;
}
}  // namespace ekizu
