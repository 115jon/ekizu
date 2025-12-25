#ifndef EKIZU_REQUEST_MODIFY_GUILD_CHANNEL_POSITIONS_HPP
#define EKIZU_REQUEST_MODIFY_GUILD_CHANNEL_POSITIONS_HPP

#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>
#include <ekizu/snowflake.hpp>

namespace ekizu {
struct ModifyGuildChannelPosition {
	/// Channel ID to be modified.
	Snowflake id;
	/// Sorting position of the channel (optional).
	std::optional<uint64_t> position;
	/// Syncs permission overwrites with the new parent if moving to a new
	/// category (optional).
	std::optional<bool> lock_permissions{};
	/// New parent ID for the channel that is moved (optional).
	std::optional<Snowflake> parent_id{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const ModifyGuildChannelPosition &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							ModifyGuildChannelPosition &f);

struct ModifyGuildChannelPositions {
	ModifyGuildChannelPositions(
		RequestSender sender, Snowflake guild_id,
		std::vector<ModifyGuildChannelPosition> channels);

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
	std::vector<ModifyGuildChannelPosition> m_channels;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_MODIFY_GUILD_CHANNEL_POSITIONS_HPP
