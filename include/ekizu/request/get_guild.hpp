#ifndef EKIZU_REQUEST_GET_GUILD_HPP
#define EKIZU_REQUEST_GET_GUILD_HPP

#include <ekizu/guild.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct GetGuildFields {
	std::optional<bool> with_counts{};
};

struct GetGuild {
	GetGuild(RequestSender sender, Snowflake guild_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	GetGuild &with_counts(bool with_counts) {
		m_fields.with_counts = with_counts;
		return *this;
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<Guild>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<Guild>)>(
			[this](auto &&handler) {
				m_sender.send<Guild>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_guild_id;
	GetGuildFields m_fields;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_GET_GUILD_HPP
