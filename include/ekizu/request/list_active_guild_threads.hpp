#ifndef EKIZU_REQUEST_LIST_ACTIVE_GUILD_THREADS_HPP
#define EKIZU_REQUEST_LIST_ACTIVE_GUILD_THREADS_HPP

#include <ekizu/channel.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct ListActiveGuildThreadsResponse {
	std::vector<Channel> threads;
	std::vector<ThreadMember> members;
};

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const ListActiveGuildThreadsResponse &response);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							ListActiveGuildThreadsResponse &response);

struct ListActiveGuildThreads {
	ListActiveGuildThreads(RequestSender sender, Snowflake guild_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
		void(Result<ListActiveGuildThreadsResponse>)) CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<
			CompletionToken, void(Result<ListActiveGuildThreadsResponse>)>(
			[this](auto &&handler) {
				m_sender.send<ListActiveGuildThreadsResponse>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_guild_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_LIST_ACTIVE_GUILD_THREADS_HPP
