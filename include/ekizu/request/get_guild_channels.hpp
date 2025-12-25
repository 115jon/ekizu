#ifndef EKIZU_REQUEST_GET_GUILD_CHANNELS_HPP
#define EKIZU_REQUEST_GET_GUILD_CHANNELS_HPP

#include <ekizu/channel.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
/**
 * @brief Represents the Get Guild Channels API request.
 */
struct GetGuildChannels {
	/**
	 * @brief Constructor for GetGuildChannels.
	 * @param sender The request sender.
	 * @param guild_id The ID of the guild.
	 */
	GetGuildChannels(RequestSender sender, Snowflake guild_id);

	/**
	 * @brief Converts the request to an HTTP request.
	 * @return The HTTP request.
	 */
	EKIZU_EXPORT operator net::HttpRequest() const;

	/**
	 * @brief Sends the API request to get guild channels.
	 */
	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
		void(Result<std::vector<Channel>>)) CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken,
									void(Result<std::vector<Channel>>)>(
			[this](auto &&handler) {
				m_sender.send<std::vector<Channel>>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_guild_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_GET_GUILD_CHANNELS_HPP
