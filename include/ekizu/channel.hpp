#ifndef EKIZU_CHANNEL_HPP
#define EKIZU_CHANNEL_HPP

#include <ekizu/channel_type.hpp>
#include <ekizu/guild_member.hpp>
#include <ekizu/permissions.hpp>
#include <ekizu/presence.hpp>

namespace ekizu {
enum class ChannelFlags : uint64_t {
	/// Channel is pinned in a forum.
	Pinned = 1 << 1,
	/// New threads in a forum channel require a tag.
	RequireTag = 1 << 4,
};

struct DefaultReaction {
	/// The ID of the emoji.
	Snowflake emoji_id;
	/// Unicode characters of the emoji.
	std::optional<std::string> emoji_name;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const DefaultReaction &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j, DefaultReaction &r);

enum class ForumLayout : uint8_t {
	/// Display posts as a collection of tiles.
	GalleryView,
	/// Display posts as a list.
	ListView,
	/// No default has been set for the forum channel.
	NotSet,
};

enum class ForumSortOrder : uint8_t {
	/// Sort forum posts by creation time (from most recent to oldest).
	CreationDate,
	/// Sort forum posts by activity.
	LatestActivity,
};

struct ForumTag {
	/// The ID of the tag.
	Snowflake id;
	/// The name of the tag (0-20 characters).
	std::string name;
	/// Whether this tag can only be added to or removed from threads by a
	/// member with the MANAGE_THREADS permission.
	bool moderated{};
	/// The ID of a guild's custom emoji.
	std::optional<Snowflake> emoji_id;
	/// The Unicode character of the emoji.
	std::optional<std::string> emoji_name;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const ForumTag &t);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ForumTag &t);

enum class PermissionType : uint8_t { Role, Member };

struct PermissionOverwrite {
	/// The ID of the user to overwrite permissions for.
	Snowflake id;
	/// The type of permission to overwrite.
	PermissionType type{};
	/// Allowed permissions
	Permissions allow{};
	/// Denied permissions
	Permissions deny{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const PermissionOverwrite &p);
EKIZU_EXPORT void from_json(const nlohmann::json &j, PermissionOverwrite &p);

struct ThreadMetadata {
	/// Whether the thread is archived.
	bool archived{};
	/// The thread will stop showing in the channel list after
	/// auto_archive_duration minutes of inactivity (can be set to: 60, 1440,
	/// 4320, 10080).
	int32_t auto_archive_duration{};
	/// Timestamp when the thread's archive status was last changed, used for
	/// calculating recent activity.
	std::string archive_timestamp;
	/// Whether the thread is locked; when a thread is locked, only users with
	/// MANAGE_THREADS can unarchive it.
	bool locked{};
	/// Whether non-moderators can add other non-moderators to a thread; only
	/// available on private threads.
	std::optional<bool> invitable;
	/// Timestamp when the thread was created; only populated for threads
	/// created after 2022-01-09.
	std::optional<std::string> create_timestamp;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const ThreadMetadata &t);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ThreadMetadata &t);

struct ThreadMember {
	/// ID of the thread.
	std::optional<Snowflake> id;
	/// ID of the user.
	std::optional<Snowflake> user_id;
	/// Timestamp of when the member joined the thread.
	std::string join_timestamp;
	/// Any user-thread settings, currently only used for notifications.
	uint64_t flags;
	/// Member associated with the thread.
	std::optional<GuildMember> member;
	/// Presence information for the member.
	std::optional<Presence> presence;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const ThreadMember &m);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ThreadMember &m);

enum class VideoQualityMode : uint8_t {
	/// Discord chooses the quality for optimal performance.
	Auto = 1,
	/// 720p.
	Full,
};

struct Channel {
	/// The ID of the channel.
	Snowflake id;
	/// The type of channel.
	ChannelType type{};
	/// The ID of the guild the channel belongs to.
	std::optional<Snowflake> guild_id;
	/// The sorting position of the channel.
	std::optional<int32_t> position;
	/// Explicit permissions overwrites for members and roles.
	std::optional<std::vector<PermissionOverwrite> > permission_overwrites;
	/// The name of the channel (1-100 characters).
	std::optional<std::string> name;
	/// The channel topic (0-4096 characters for GUILD_FORUM and GUILD_MEDIA
	/// channels, 0-1024 characters for all others).
	std::optional<std::string> topic;
	/// Whether the channel is NSFW.
	bool nsfw{};
	/// The ID of the last message sent in this channel (or thread for
	/// GUILD_FORUM or GUILD_MEDIA channels) (may not point to an existing or
	/// valid message or thread).
	std::optional<Snowflake> last_message_id;
	/// The bitrate (in bits) of the voice channel.
	std::optional<int32_t> bitrate;
	/// The user limit of the voice channel.
	std::optional<int32_t> user_limit;
	/// Amount of seconds a user has to wait before sending another message
	/// (0-21600); bots, as well as users with the permission manage_messages or
	/// manage_channel, are unaffected.
	int32_t rate_limit_per_user{};
	/// The recipients of the DM.
	std::optional<std::vector<User> > recipients;
	/// Icon hash of the group DM.
	std::optional<std::string> icon;
	/// ID of the creator of the group DM or thread.
	std::optional<Snowflake> owner_id;
	/// Application ID of the group DM creator if it is bot-created.
	std::optional<Snowflake> application_id;
	/// Whether the channel is managed by an application via the gdm.join OAuth2
	/// scope.
	bool managed{};
	/// ID of the parent category for a channel (each parent category can
	/// contain up to 50 channels), for threads: ID of the text channel this
	/// thread was created.
	std::optional<Snowflake> parent_id;
	/// When the last pinned message was pinned. This may be null in events such
	/// as GUILD_CREATE when a message is not pinned.
	std::optional<std::string> last_pin_timestamp;
	/// Voice region ID for the voice channel, automatic when set to null.
	std::optional<std::string> rtc_region;
	/// The camera video quality mode of the voice channel, 1 when not present.
	std::optional<int32_t> video_quality_mode;
	/// Number of messages (not including the initial message or deleted
	/// messages) in a thread.
	std::optional<int32_t> message_count;
	/// An approximate count of users in a thread, stops counting at 50.
	std::optional<int32_t> member_count;
	/// Thread-specific fields not needed by other channels.
	std::optional<ThreadMetadata> thread_metadata;
	/// Thread member object for the current user, if they have joined the
	/// thread, only included on certain API endpoints.
	std::optional<ThreadMember> member;
	/// Default duration, copied onto newly created threads, in minutes, threads
	/// will stop showing in the channel list after the specified period of
	/// inactivity, can be set to: 60, 1440, 4320, 10080.
	std::optional<int32_t> default_auto_archive_duration;
	/// Computed permissions for the invoking user in the channel, including
	/// overwrites, only included when part of the resolved data received on a
	/// slash command interaction. This does not include implicit permissions,
	/// which may need to be checked separately.
	std::optional<std::string> permissions;
	/// Channel flags combined as a bitfield.
	std::optional<int32_t> flags;
	/// Number of messages ever sent in a thread, it's similar to message_count
	/// on message creation, but will not decrement the number when a message is
	/// deleted.
	std::optional<int32_t> total_messages_sent;
	/// The set of tags that can be used in a GUILD_FORUM or a GUILD_MEDIA
	/// channel.
	std::optional<std::vector<ForumTag> > available_tags;
	/// The IDs of the set of tags that have been applied to a thread in a
	/// GUILD_FORUM or a GUILD_MEDIA channel.
	std::optional<std::vector<Snowflake> > applied_tags;
	/// The emoji to show in the add reaction button on a thread in a
	/// GUILD_FORUM or a GUILD_MEDIA channel.
	std::optional<DefaultReaction> default_reaction_emoji;
	/// The initial rate_limit_per_user to set on newly created threads in a
	/// channel. This field is copied to the thread at creation time and does
	/// not live update.
	std::optional<int32_t> default_thread_rate_limit_per_user;
	/// The default sort order type used to order posts in GUILD_FORUM and
	/// GUILD_MEDIA channels. Defaults to null, which indicates a preferred sort
	/// order hasn't been set by a channel admin.
	std::optional<int32_t> default_sort_order;
	/// The default forum layout view used to display posts in GUILD_FORUM
	/// channels. Defaults to 0, which indicates a layout view has not been set
	/// by a channel admin.
	std::optional<int32_t> default_forum_layout;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Channel &c);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Channel &c);
}  // namespace ekizu

#endif	// EKIZU_CHANNEL_HPP
