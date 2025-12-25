#include <ekizu/request/get_user.hpp>

namespace ekizu {

GetUser::GetUser(RequestSender sender, Snowflake user_id)
	: m_user_id{user_id}, m_sender{sender} {}

GetUser::operator net::HttpRequest() const {
	return net::HttpRequest{
		net::HttpMethod::get, fmt::format("/users/{}", m_user_id), 11};
}

}  // namespace ekizu