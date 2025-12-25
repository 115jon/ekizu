#ifndef EKIZU_REQUEST_GET_GUILD_MEMBER_HPP
#define EKIZU_REQUEST_GET_GUILD_MEMBER_HPP

#include <ekizu/guild_member.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct GetGuildMember {
	GetGuildMember(RequestSender sender, Snowflake guild_id, Snowflake user_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<GuildMember>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<GuildMember>)>(
			[this](auto &&handler) {
				m_sender.send<GuildMember>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_guild_id;
	Snowflake m_user_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_GET_GUILD_MEMBER_HPP
