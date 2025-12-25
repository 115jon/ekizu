#ifndef EKIZU_PERMISSIONS_HPP
#define EKIZU_PERMISSIONS_HPP

#include <cstdint>
#include <ekizu/export.hpp>
#include <nlohmann/json_fwd.hpp>
#include <type_traits>

namespace ekizu {
enum class Permissions : uint64_t {
	/// Allows creation of instant invites
	CreateInstantInvite = 1ULL << 0,
	/// Allows kicking members
	KickMembers = 1ULL << 1,
	/// Allows banning members
	BanMembers = 1ULL << 2,
	/// Allows all permissions and bypasses channel permission overwrites
	Administrator = 1ULL << 3,
	/// Allows management and editing of channels
	ManageChannels = 1ULL << 4,
	/// Allows management and editing of the guild
	ManageGuild = 1ULL << 5,
	/// Allows for the addition of reactions to messages
	AddReactions = 1ULL << 6,
	/// Allows for viewing of audit logs
	ViewAuditLog = 1ULL << 7,
	/// Allows for using priority speaker in a voice channel
	PrioritySpeaker = 1ULL << 8,
	/// Allows the user to go live
	Stream = 1ULL << 9,
	/// Allows guild members to view a channel, which includes reading messages
	/// in text channels and joining voice channels
	ViewChannel = 1ULL << 10,
	/// Allows for sending messages in a channel and creating threads in a forum
	/// (does not allow sending messages in threads)
	SendMessages = 1ULL << 11,
	/// Allows for sending of /tts messages
	SendTTSMessages = 1ULL << 12,
	/// Allows for deletion of other users' messages
	ManageMessages = 1ULL << 13,
	/// Links sent by users with this permission will be auto-embedded
	EmbedLinks = 1ULL << 14,
	/// Allows for uploading images and files
	AttachFiles = 1ULL << 15,
	/// Allows for reading of message history
	ReadMessageHistory = 1ULL << 16,
	/// Allows for using the @everyone tag to notify all users in a channel, and
	/// the @here tag to notify all online users in a channel
	MentionEveryone = 1ULL << 17,
	/// Allows the usage of custom emojis from other servers
	UseExternalEmojis = 1ULL << 18,
	/// Allows for viewing guild insights
	ViewGuildInsights = 1ULL << 19,
	/// Allows for joining a voice channel
	Connect = 1ULL << 20,
	/// Allows for speaking in a voice channel
	Speak = 1ULL << 21,
	/// Allows for muting members in a voice channel
	MuteMembers = 1ULL << 22,
	/// Allows for deafening of members in a voice channel
	DeafenMembers = 1ULL << 23,
	/// Allows for moving of members between voice channels
	MoveMembers = 1ULL << 24,
	/// Allows for using voice-activity-detection
	UseVAD = 1ULL << 25,
	/// Allows for modification of own nickname
	ChangeNickname = 1ULL << 26,
	/// Allows for modification of other users' nicknames
	ManageNicknames = 1ULL << 27,
	/// Allows management and editing of roles
	ManageRoles = 1ULL << 28,
	/// Allows management and editing of webhooks
	ManageWebhooks = 1ULL << 29,
	/// Allows management and editing of emojis, stickers, and soundboard sounds
	ManageGuildExpressions = 1ULL << 30,
	/// Allows members to use application commands, including slash commands and
	/// context menu commands
	UseApplicationCommands = 1ULL << 31,
	/// Allows for requesting to speak in stage channels
	RequestToSpeak = 1ULL << 32,
	/// Allows for creating, editing, and deleting scheduled events
	ManageEvents = 1ULL << 33,
	/// Allows for deleting and archiving threads, and viewing all private
	/// threads
	ManageThreads = 1ULL << 34,
	/// Allows for creating public and announcement threads
	CreatePublicThreads = 1ULL << 35,
	/// Allows for creating private threads
	CreatePrivateThreads = 1ULL << 36,
	/// Allows the usage of custom stickers from other servers
	UseExternalStickers = 1ULL << 37,
	/// Allows for sending messages in threads
	SendMessagesInThreads = 1ULL << 38,
	/// Allows for using Activities (applications with the EMBEDDED flag) in a
	/// voice channel
	UseEmbeddedActivities = 1ULL << 39,
	/// Allows for timing out users to prevent them from sending or reacting to
	/// messages in chat and threads, and from speaking in voice and stage
	/// channels
	ModerateMembers = 1ULL << 40,
	/// Allows for viewing role subscription insights
	ViewCreatorMonetizationAnalytics = 1ULL << 41,
	/// Allows for using soundboard in a voice channel
	UseSoundboard = 1ULL << 42,
	/// Allows the usage of custom soundboard sounds from other servers
	UseExternalSounds = 1ULL << 45,
	/// Allows sending voice messages
	SendVoiceMessages = 1ULL << 46
};

constexpr Permissions operator|(Permissions lhs, Permissions rhs) {
	return static_cast<Permissions>(
		static_cast<std::underlying_type_t<Permissions> >(lhs) |
		static_cast<std::underlying_type_t<Permissions> >(rhs));
}

constexpr Permissions operator&(Permissions lhs, Permissions rhs) {
	return static_cast<Permissions>(
		static_cast<std::underlying_type_t<Permissions> >(lhs) &
		static_cast<std::underlying_type_t<Permissions> >(rhs));
}

EKIZU_EXPORT void to_json(nlohmann::json &j, const Permissions &p);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Permissions &p);
}  // namespace ekizu

#endif	// EKIZU_PERMISSIONS_HPP
