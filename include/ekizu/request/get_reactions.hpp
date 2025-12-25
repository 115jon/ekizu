#ifndef EKIZU_REQUEST_GET_REACTIONS_HPP
#define EKIZU_REQUEST_GET_REACTIONS_HPP

#include <ekizu/request/create_reaction.hpp>

namespace ekizu {
struct GetReactions {
	GetReactions(RequestSender sender, Snowflake channel_id,
				 Snowflake message_id, RequestReaction emoji);

	EKIZU_EXPORT operator net::HttpRequest() const;

	GetReactions &after(Snowflake after) {
		m_after = after;
		return *this;
	}

	GetReactions &limit(uint8_t limit) {
		m_limit = limit;
		return *this;
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<std::vector<User>>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken,
									void(Result<std::vector<User>>)>(
			[this](auto &&handler) {
				m_sender.send<std::vector<User>>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	std::optional<Snowflake> m_after;
	Snowflake m_channel_id;
	RequestReaction m_emoji;
	std::optional<uint8_t> m_limit;
	Snowflake m_message_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_GET_REACTIONS_HPP
