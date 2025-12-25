#ifndef EKIZU_REQUEST_GET_GUILD_BANS_HPP
#define EKIZU_REQUEST_GET_GUILD_BANS_HPP

#include <ekizu/ban.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct GetGuildBansFields {
	std::optional<uint64_t> limit;
	std::optional<Snowflake> before;
	std::optional<Snowflake> after;
};

struct GetGuildBans {
	GetGuildBans(RequestSender sender, Snowflake guild_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	GetGuildBans &limit(uint64_t limit) {
		m_fields.limit = limit;
		return *this;
	}

	GetGuildBans &before(Snowflake before) {
		m_fields.before = before;
		return *this;
	}

	GetGuildBans &after(Snowflake after) {
		m_fields.after = after;
		return *this;
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<std::vector<Ban>>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken,
									void(Result<std::vector<Ban>>)>(
			[this](auto &&handler) {
				m_sender.send<std::vector<Ban>>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_guild_id;
	GetGuildBansFields m_fields;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_GET_GUILD_BANS_HPP
