#ifndef EKIZU_REQUEST_INTERACTION_GET_ORIGINAL_RESPONSE_HPP
#define EKIZU_REQUEST_INTERACTION_GET_ORIGINAL_RESPONSE_HPP

#include <ekizu/http.hpp>
#include <ekizu/message.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct GetOriginalResponse {
	GetOriginalResponse(RequestSender sender, Snowflake application_id,
						std::string interaction_token);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<Message>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<Message>)>(
			[this](auto &&handler) {
				m_sender.send<Message>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_application_id;
	std::string m_interaction_token;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_INTERACTION_GET_ORIGINAL_RESPONSE_HPP
