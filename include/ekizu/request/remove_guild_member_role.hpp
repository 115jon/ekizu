#ifndef EKIZU_REQUEST_REMOVE_GUILD_MEMBER_ROLE_HPP
#define EKIZU_REQUEST_REMOVE_GUILD_MEMBER_ROLE_HPP

#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>
#include <ekizu/snowflake.hpp>

namespace ekizu {
struct RemoveGuildMemberRole {
	RemoveGuildMemberRole(RequestSender sender, Snowflake guild_id,
						  Snowflake user_id, Snowflake role_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this](auto &&handler) {
				m_sender.send(
					*this, [h = std::forward<decltype(handler)>(handler)](
							   Result<net::HttpResponse> res) mutable {
						if (!res) { return std::move(h)(res.error()); }

						if (res.value().result() !=
							net::HttpStatus::no_content) {
							return std::move(h)(
								boost::system::errc::operation_not_permitted);
						}

						std::move(h)(outcome::success());
					});
			},
			token);
	}

   private:
	Snowflake m_guild_id;
	Snowflake m_user_id;
	Snowflake m_role_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_REMOVE_GUILD_MEMBER_ROLE_HPP
