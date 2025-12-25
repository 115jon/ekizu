#ifndef EKIZU_REQUEST_CREATE_GUILD_CHANNEL_HPP
#define EKIZU_REQUEST_CREATE_GUILD_CHANNEL_HPP

#include <ekizu/channel.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
/**
 * @brief Represents the fields for the Create Guild Channel API request.
 */
struct CreateGuildChannelFields {
	/// The name of the channel (1-100 characters).
	std::string name;
	/// The type of channel.
	std::optional<ChannelType> type;
	/// Channel topic (0-1024 characters).
	std::optional<std::string> topic;
	/// The bitrate (in bits) of the voice or stage channel; min 8000.
	std::optional<uint32_t> bitrate;
	/// The user limit of the voice channel.
	std::optional<uint16_t> user_limit;
	/// Amount of seconds a user has to wait before sending another message
	/// (0-21600); bots, as well as users with the permission manage_messages or
	/// manage_channel, are unaffected.
	std::optional<uint16_t> rate_limit_per_user;
	/// Sorting position of the channel.
	std::optional<uint64_t> position;
	/// The channel's permission overwrites.
	std::optional<std::vector<PermissionOverwrite>> permission_overwrites;
	/// Id of the parent category for a channel.
	std::optional<Snowflake> parent_id;
	/// Whether the channel is nsfw.
	std::optional<bool> nsfw;
	/// Channel voice region id of the voice or stage channel, automatic when
	/// set to null.
	std::optional<std::string> rtc_region;
	/// The camera video quality mode of the voice channel.
	std::optional<VideoQualityMode> video_quality_mode;
	/// The default duration that the clients use (not the API) for newly
	/// created threads in the channel, in minutes, to automatically archive the
	/// thread after recent activity.
	std::optional<uint16_t> default_auto_archive_duration;
	/// Emoji to show in the add reaction button on a thread in a GUILD_FORUM or
	/// a GUILD_MEDIA channel.
	std::optional<DefaultReaction> default_reaction_emoji;
	/// Set of tags that can be used in a GUILD_FORUM or a GUILD_MEDIA channel.
	std::optional<std::vector<ForumTag>> available_tags;
	/// The default sort order type used to order posts in GUILD_FORUM and
	/// GUILD_MEDIA channels.
	std::optional<ForumSortOrder> default_sort_order;
	/// The default forum layout view used to display posts in GUILD_FORUM
	/// channels.
	std::optional<ForumLayout> default_forum_layout;
	/// The initial rate_limit_per_user to set on newly created threads in a
	/// channel. This field is copied to the thread at creation time and does
	/// not live update.
	std::optional<uint16_t> default_thread_rate_limit_per_user;
};

EKIZU_EXPORT void from_json(const nlohmann::json &j,
							CreateGuildChannelFields &f);
EKIZU_EXPORT void to_json(nlohmann::json &j, const CreateGuildChannelFields &f);

/**
 * @brief Represents the Create Guild Channel API request.
 */
struct CreateGuildChannel {
	/**
	 * @brief Constructor for CreateGuildChannel.
	 * @param sender The request sender.
	 * @param guild_id The ID of the guild.
	 */
	CreateGuildChannel(RequestSender sender, Snowflake guild_id,
					   std::string_view name);

	/**
	 * @brief Converts the request to an HTTP request.
	 * @return The HTTP request.
	 */
	EKIZU_EXPORT operator net::HttpRequest() const;

	CreateGuildChannel &bitrate(uint32_t bitrate) {
		m_fields.bitrate = bitrate;
		return *this;
	}

	CreateGuildChannel &default_auto_archive_duration(uint16_t duration) {
		m_fields.default_auto_archive_duration = duration;
		return *this;
	}

	CreateGuildChannel &default_forum_layout(ForumLayout layout) {
		m_fields.default_forum_layout = layout;
		return *this;
	}

	CreateGuildChannel &default_reaction_emoji(const DefaultReaction &emoji) {
		m_fields.default_reaction_emoji = emoji;
		return *this;
	}

	CreateGuildChannel &nsfw(bool is_nsfw) {
		m_fields.nsfw = is_nsfw;
		return *this;
	}

	CreateGuildChannel &parent_id(Snowflake id) {
		m_fields.parent_id = id;
		return *this;
	}

	CreateGuildChannel &permission_overwrites(
		const std::vector<PermissionOverwrite> &overwrites) {
		m_fields.permission_overwrites = overwrites;
		return *this;
	}

	CreateGuildChannel &position(uint64_t position) {
		m_fields.position = position;
		return *this;
	}

	CreateGuildChannel &rate_limit_per_user(uint16_t limit) {
		m_fields.rate_limit_per_user = limit;
		return *this;
	}

	CreateGuildChannel &rtc_region(std::string region) {
		m_fields.rtc_region = std::move(region);
		return *this;
	}

	CreateGuildChannel &topic(std::string topic) {
		m_fields.topic = std::move(topic);
		return *this;
	}

	CreateGuildChannel &type(ChannelType channel_type) {
		m_fields.type = channel_type;
		return *this;
	}

	CreateGuildChannel &user_limit(uint16_t limit) {
		m_fields.user_limit = limit;
		return *this;
	}

	CreateGuildChannel &video_quality_mode(VideoQualityMode quality_mode) {
		m_fields.video_quality_mode = quality_mode;
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
	Snowflake m_guild_id;
	CreateGuildChannelFields m_fields;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_CREATE_GUILD_CHANNEL_HPP
