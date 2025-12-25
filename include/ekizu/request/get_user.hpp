#ifndef EKIZU_REQUEST_GET_USER_HPP
#define EKIZU_REQUEST_GET_USER_HPP

#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>
#include <ekizu/user.hpp>

namespace ekizu {
/**
 * @brief Represents the  REST API endpoint.
 */
struct GetUser {
	GetUser(RequestSender sender, Snowflake user_id);

	/**
	 * @brief Converts the GetUser to an HTTP request.
	 *
	 * @return The HTTP request.
	 */
	EKIZU_EXPORT operator net::HttpRequest() const;

	/**
	 * @brief Sends the GetUser request.
	 *
	 * @return The result of the request as an HTTP response.
	 */
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
	Snowflake m_user_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_GET_USER_HPP
