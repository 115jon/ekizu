#ifndef EKIZU_REQUEST_MODIFY_CHANNEL_HPP
#define EKIZU_REQUEST_MODIFY_CHANNEL_HPP

#include <ekizu/channel.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct ModifyChannelFields {
	std::optional<std::string> name;
	std::optional<ChannelType> type;
	std::optional<uint64_t> position;
	std::optional<std::string> topic;
	std::optional<bool> nsfw;
	std::optional<uint16_t> rate_limit_per_user;
	std::optional<uint32_t> bitrate;
	std::optional<uint16_t> user_limit;
	std::optional<std::vector<PermissionOverwrite>> permission_overwrites;
	std::optional<Snowflake> parent_id;
	std::optional<std::string> rtc_region;
	std::optional<VideoQualityMode> video_quality_mode;
	std::optional<ChannelFlags> flags;
	std::optional<std::vector<ForumTag>> available_tags;
	std::optional<DefaultReaction> default_reaction_emoji;
	std::optional<uint16_t> default_thread_rate_limit_per_user;
	std::optional<ForumSortOrder> default_sort_order;
	std::optional<ForumLayout> default_forum_layout;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const ModifyChannelFields &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ModifyChannelFields &f);

struct ModifyChannel {
	ModifyChannel(RequestSender sender, Snowflake channel_id);

	/**
	 * @brief Converts the UpdateCurrentUser to an HTTP request.
	 *
	 * @return The HTTP request.
	 */
	EKIZU_EXPORT operator net::HttpRequest() const;

	ModifyChannel &name(const std::optional<std::string> &name) {
		m_fields.name = name;
		return *this;
	}

	ModifyChannel &type(const std::optional<ChannelType> &type) {
		m_fields.type = type;
		return *this;
	}

	ModifyChannel &position(const std::optional<uint64_t> &position) {
		m_fields.position = position;
		return *this;
	}

	ModifyChannel &topic(const std::optional<std::string> &topic) {
		m_fields.topic = topic;
		return *this;
	}

	ModifyChannel &nsfw(const std::optional<bool> &nsfw) {
		m_fields.nsfw = nsfw;
		return *this;
	}

	ModifyChannel &rate_limit_per_user(
		const std::optional<uint16_t> &rate_limit_per_user) {
		m_fields.rate_limit_per_user = rate_limit_per_user;
		return *this;
	}

	ModifyChannel &bitrate(const std::optional<uint32_t> &bitrate) {
		m_fields.bitrate = bitrate;
		return *this;
	}

	ModifyChannel &user_limit(const std::optional<uint16_t> &user_limit) {
		m_fields.user_limit = user_limit;
		return *this;
	}

	ModifyChannel &permission_overwrites(
		const std::optional<std::vector<PermissionOverwrite>>
			&permission_overwrites) {
		m_fields.permission_overwrites = permission_overwrites;
		return *this;
	}

	ModifyChannel &parent_id(const std::optional<Snowflake> &parent_id) {
		m_fields.parent_id = parent_id;
		return *this;
	}

	ModifyChannel &rtc_region(const std::optional<std::string> &rtc_region) {
		m_fields.rtc_region = rtc_region;
		return *this;
	}

	ModifyChannel &video_quality_mode(
		const std::optional<VideoQualityMode> &video_quality_mode) {
		m_fields.video_quality_mode = video_quality_mode;
		return *this;
	}

	ModifyChannel &flags(const std::optional<ChannelFlags> &flags) {
		m_fields.flags = flags;
		return *this;
	}

	ModifyChannel &available_tags(
		const std::optional<std::vector<ForumTag>> &available_tags) {
		m_fields.available_tags = available_tags;
		return *this;
	}

	ModifyChannel &default_reaction_emoji(
		const std::optional<DefaultReaction> &default_reaction_emoji) {
		m_fields.default_reaction_emoji = default_reaction_emoji;
		return *this;
	}

	ModifyChannel &default_thread_rate_limit_per_user(
		const std::optional<uint16_t> &default_thread_rate_limit_per_user) {
		m_fields.default_thread_rate_limit_per_user =
			default_thread_rate_limit_per_user;
		return *this;
	}

	ModifyChannel &default_sort_order(
		const std::optional<ForumSortOrder> &default_sort_order) {
		m_fields.default_sort_order = default_sort_order;
		return *this;
	}

	ModifyChannel &default_forum_layout(
		const std::optional<ForumLayout> &default_forum_layout) {
		m_fields.default_forum_layout = default_forum_layout;
		return *this;
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<Channel>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<Channel>)>(
			[this](auto &&handler) {
				m_sender.send<Channel>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_channel_id;
	ModifyChannelFields m_fields;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_MODIFY_CHANNEL_HPP
