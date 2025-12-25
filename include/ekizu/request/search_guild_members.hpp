#ifndef EKIZU_REQUEST_SEARCH_GUILD_MEMBERS_HPP
#define EKIZU_REQUEST_SEARCH_GUILD_MEMBERS_HPP

#include <ekizu/guild_member.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct SearchGuildMembersFields {
	std::optional<std::string> query;
	std::optional<uint16_t> limit;
};

struct SearchGuildMembers {
	SearchGuildMembers(RequestSender sender, Snowflake guild_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	SearchGuildMembers &query(std::string_view query) {
		m_fields.query = query;
		return *this;
	}

	SearchGuildMembers &limit(uint16_t limit) {
		m_fields.limit = limit;
		return *this;
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
		void(Result<std::vector<GuildMember>>)) CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken,
									void(Result<std::vector<GuildMember>>)>(
			[this](auto &&handler) {
				m_sender.send<std::vector<GuildMember>>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_guild_id;
	SearchGuildMembersFields m_fields;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_SEARCH_GUILD_MEMBERS_HPP
