#ifndef EKIZU_REQUEST_GET_GUILD_PREVIEW_HPP
#define EKIZU_REQUEST_GET_GUILD_PREVIEW_HPP

#include <ekizu/guild_preview.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct GetGuildPreview {
	GetGuildPreview(RequestSender sender, Snowflake guild_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<GuildPreview>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken,
									void(Result<GuildPreview>)>(
			[this](auto &&handler) {
				m_sender.send<GuildPreview>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_guild_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_GET_GUILD_PREVIEW_HPP
