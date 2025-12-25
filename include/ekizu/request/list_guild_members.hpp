#ifndef EKIZU_REQUEST_LIST_GUILD_MEMBERS_HPP
#define EKIZU_REQUEST_LIST_GUILD_MEMBERS_HPP

#include <ekizu/guild_member.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct ListGuildMembersFields {
	std::optional<uint16_t> limit;
	std::optional<Snowflake> after;
};

struct ListGuildMembers {
	ListGuildMembers(RequestSender sender, Snowflake guild_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	ListGuildMembers &after(Snowflake after) {
		m_fields.after = after;
		return *this;
	}

	ListGuildMembers &limit(uint16_t limit) {
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
	ListGuildMembersFields m_fields;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_LIST_GUILD_MEMBERS_HPP
