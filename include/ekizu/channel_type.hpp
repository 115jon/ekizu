#ifndef EKIZU_CHANNEL_TYPE_HPP
#define EKIZU_CHANNEL_TYPE_HPP

#include <cstdint>

namespace ekizu {

/**
 * @brief Types of channels.
 * @see
 * https://discord.com/developers/docs/resources/channel#channel-object-channel-types
 */
enum class ChannelType : uint8_t {
	/// A text channel within a server.
	GuildText = 0,
	/// A private channel between users.
	Dm = 1,
	/// A voice channel within a server.
	GuildVoice = 2,
	/// A private channel between multiple users.
	GroupDm = 3,
	/// A category that contains up to 50 channels.
	GuildCategory = 4,
	/// A channel that users can follow and crosspost into their own server.
	GuildNews = 5,
	/// A temporary sub-channel within a GUILD_NEWS channel.
	GuildNewsThread = 10,
	/// A temporary sub-channel within a GUILD_NEWS channel.
	GuildPublicThread = 11,
	/// A temporary sub-channel within A GUILD_NEWS channel, that is only
	/// visible by those invited and those with the
	/// MANAGE_THREADS permission.
	GuildPrivateThread = 12,
	/// A voice channel for hosting events with an audience.
	GuildStageVoice = 13,
	/// The channel in a hub containing a list of servers.
	GuildDirectory = 14,
	/// A channel that can only contain threads.
	GuildForum = 15,
	/// A channel that can only contain threads, similar to GuildForum channels
	GuildMedia = 16,
};

}  // namespace ekizu

#endif	// EKIZU_CHANNEL_TYPE_HPP
