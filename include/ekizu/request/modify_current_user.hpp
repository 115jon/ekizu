#ifndef EKIZU_REQUEST_MODIFY_CURRENT_USER_HPP
#define EKIZU_REQUEST_MODIFY_CURRENT_USER_HPP

#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>
#include <ekizu/user.hpp>

namespace ekizu {
struct ModifyCurrentUserFields {
	std::optional<std::string> avatar;
	std::optional<std::string> username;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const ModifyCurrentUserFields &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							ModifyCurrentUserFields &f);

struct ModifyCurrentUser {
	explicit ModifyCurrentUser(RequestSender sender);

	EKIZU_EXPORT operator net::HttpRequest() const;

	ModifyCurrentUser &avatar(std::string avatar) {
		m_fields.avatar = std::move(avatar);
		return *this;
	}

	ModifyCurrentUser &username(std::string username) {
		// TODO: Validate username
		m_fields.username = std::move(username);
		return *this;
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<User>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<User>)>(
			[this](auto &&handler) {
				m_sender.send<User>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	ModifyCurrentUserFields m_fields;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_MODIFY_CURRENT_USER_HPP
