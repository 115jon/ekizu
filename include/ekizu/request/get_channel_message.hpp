#ifndef EKIZU_REQUEST_GET_CHANNEL_MESSAGE_HPP
#define EKIZU_REQUEST_GET_CHANNEL_MESSAGE_HPP

#include <ekizu/http.hpp>
#include <ekizu/message.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
/**
 * @brief Represents the Get Channel REST API endpoint.
 */
struct GetChannelMessage {
	GetChannelMessage(RequestSender sender, Snowflake channel_id,
					  Snowflake message_id);

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
	Snowflake m_channel_id;
	Snowflake m_message_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_GET_CHANNEL_MESSAGE_HPP
