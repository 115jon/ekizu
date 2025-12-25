#ifndef EKIZU_REQUEST_ADD_GUILD_MEMBER_HPP
#define EKIZU_REQUEST_ADD_GUILD_MEMBER_HPP

#include <ekizu/guild_member.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct AddGuildMemberFields {
	std::string access_token;
	std::optional<std::string> nick;
	std::optional<std::vector<Snowflake>> roles;
	std::optional<bool> mute;
	std::optional<bool> deaf;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const AddGuildMemberFields &m);

struct AddGuildMember {
	AddGuildMember(RequestSender sender, Snowflake guild_id, Snowflake user_id,
				   std::string access_token);

	EKIZU_EXPORT operator net::HttpRequest() const;

	AddGuildMember &nick(std::string nick) {
		m_fields.nick = std::move(nick);
		return *this;
	}

	AddGuildMember &roles(std::vector<Snowflake> roles) {
		m_fields.roles = std::move(roles);
		return *this;
	}

	AddGuildMember &mute(bool mute) {
		m_fields.mute = mute;
		return *this;
	}

	AddGuildMember &deaf(bool deaf) {
		m_fields.deaf = deaf;
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
	Snowflake m_user_id;
	AddGuildMemberFields m_fields;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_ADD_GUILD_MEMBER_HPP
