#ifndef EKIZU_REQUEST_GET_GUILD_INVITES_HPP
#define EKIZU_REQUEST_GET_GUILD_INVITES_HPP

#include <ekizu/http.hpp>
#include <ekizu/invite.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct GetGuildInvites {
	GetGuildInvites(RequestSender sender, Snowflake guild_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<std::vector<Invite>>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken,
									void(Result<std::vector<Invite>>)>(
			[this](auto &&handler) {
				m_sender.send<std::vector<Invite>>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_guild_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_GET_GUILD_INVITES_HPP
