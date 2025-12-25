#ifndef EKIZU_REQUEST_GET_PINNED_MESSAGES_HPP
#define EKIZU_REQUEST_GET_PINNED_MESSAGES_HPP

#include <ekizu/http.hpp>
#include <ekizu/message.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct GetPinnedMessages {
	GetPinnedMessages(RequestSender sender, Snowflake channel_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
		void(Result<std::vector<Message>>)) CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken,
									void(Result<std::vector<Message>>)>(
			[this](auto &&handler) {
				m_sender.send<std::vector<Message>>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_channel_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_GET_PINNED_MESSAGES_HPP
