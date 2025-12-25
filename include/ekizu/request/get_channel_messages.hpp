#ifndef EKIZU_REQUEST_GET_CHANNEL_MESSAGES_HPP
#define EKIZU_REQUEST_GET_CHANNEL_MESSAGES_HPP

#include <ekizu/http.hpp>
#include <ekizu/message.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct GetChannelMessages;

struct GetChannelMessagesConfigured {
	enum class Type { Around, Before, After };

	EKIZU_EXPORT GetChannelMessagesConfigured(
		RequestSender sender, Snowflake channel_id,
		std::optional<uint16_t> limit, Type type, Snowflake message_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	GetChannelMessagesConfigured &limit(uint16_t limit) {
		m_limit = limit;
		return *this;
	}

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
	std::optional<uint16_t> m_limit;
	Type m_type;
	Snowflake m_message_id;
	RequestSender m_sender;
};

struct GetChannelMessages {
	EKIZU_EXPORT GetChannelMessages(RequestSender sender, Snowflake channel_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	[[nodiscard]] GetChannelMessagesConfigured around(
		Snowflake message_id) const {
		return GetChannelMessagesConfigured{
			m_sender,	m_channel_id,
			m_limit,	GetChannelMessagesConfigured::Type::Around,
			message_id,
		};
	}

	[[nodiscard]] GetChannelMessagesConfigured before(
		Snowflake message_id) const {
		return GetChannelMessagesConfigured{
			m_sender,	m_channel_id,
			m_limit,	GetChannelMessagesConfigured::Type::Before,
			message_id,
		};
	}

	[[nodiscard]] GetChannelMessagesConfigured after(
		Snowflake message_id) const {
		return GetChannelMessagesConfigured{
			m_sender,	m_channel_id,
			m_limit,	GetChannelMessagesConfigured::Type::After,
			message_id,
		};
	}

	GetChannelMessages &limit(uint16_t limit) {
		m_limit = limit;
		return *this;
	}

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
	std::optional<uint16_t> m_limit;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_GET_CHANNEL_MESSAGES_HPP
