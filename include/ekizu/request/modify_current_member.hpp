#ifndef EKIZU_REQUEST_MODIFY_CURRENT_MEMBER_HPP
#define EKIZU_REQUEST_MODIFY_CURRENT_MEMBER_HPP

#include <ekizu/guild_member.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct ModifyCurrentMemberFields {
	/// Value to set user's nickname to.
	std::optional<std::string> nick;
};

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const ModifyCurrentMemberFields &m);

struct ModifyCurrentMember {
	ModifyCurrentMember(RequestSender sender, Snowflake guild_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	ModifyCurrentMember &nick(std::string nick) {
		m_fields.nick = std::move(nick);
		return *this;
	}

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
	ModifyCurrentMemberFields m_fields;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_MODIFY_CURRENT_MEMBER_HPP
