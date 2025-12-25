#ifndef EKIZU_REQUEST_CREATE_DM_HPP
#define EKIZU_REQUEST_CREATE_DM_HPP

#include <ekizu/channel.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct CreateDM {
	EKIZU_EXPORT CreateDM(RequestSender sender, Snowflake user_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<Channel>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<Channel>)>(
			[this](auto &&handler) {
				m_sender.send<Channel>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_user_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_CREATE_DM_HPP
