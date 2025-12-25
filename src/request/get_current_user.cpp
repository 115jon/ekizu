#include <ekizu/request/get_current_user.hpp>

namespace ekizu {
GetCurrentUser::GetCurrentUser(RequestSender sender) : m_sender{sender} {}

GetCurrentUser::operator net::HttpRequest() const {
	return net::HttpRequest{net::HttpMethod::get, "/users/@me", 11};
}
}  // namespace ekizu
