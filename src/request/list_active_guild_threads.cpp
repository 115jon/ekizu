#include <ekizu/json_util.hpp>
#include <ekizu/request/list_active_guild_threads.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j,
			 const ListActiveGuildThreadsResponse &response) {
	serialize(j, "threads", response.threads);
	serialize(j, "members", response.members);
}

void from_json(const nlohmann::json &j,
			   ListActiveGuildThreadsResponse &response) {
	deserialize(j, "threads", response.threads);
	deserialize(j, "members", response.members);
}

ListActiveGuildThreads::ListActiveGuildThreads(RequestSender sender,
											   Snowflake guild_id)
	: m_guild_id{guild_id}, m_sender{sender} {}

ListActiveGuildThreads::operator net::HttpRequest() const {
	return net::HttpRequest{
		net::HttpMethod::get,
		fmt::format("/guilds/{}/threads/active", m_guild_id), 11};
}
}  // namespace ekizu
