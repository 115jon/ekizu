#ifndef EKIZU_MESSAGE_HPP
#define EKIZU_MESSAGE_HPP

#include <ekizu/attachment.hpp>
#include <ekizu/channel.hpp>
#include <ekizu/embed.hpp>
#include <ekizu/function_view.hpp>
#include <ekizu/interaction_type.hpp>
#include <ekizu/message_component.hpp>
#include <ekizu/partial_member.hpp>
#include <ekizu/role.hpp>

namespace ekizu {
/**
 * This structure represents a form of controlling mentions in a Message
 * object.
 *
 * @see
 * [https://discord.com/developers/docs/resources/channel#allowed-mentions-object](https://discord.com/developers/docs/resources/channel#allowed-mentions-object)
 */
struct AllowedMentions {
	/// List of allowed mention types to parse from the content.
	std::vector<std::string> parse;
	/// List of role_ids to mention (max 100).
	std::vector<Snowflake> roles;
	/// List of user_ids to mention (max 100).
	std::vector<Snowflake> users;
	/// Whether to mention the author of the message being replied (default
	/// false).
	bool replied_user{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const AllowedMentions &m);
EKIZU_EXPORT void from_json(const nlohmann::json &j, AllowedMentions &m);

struct ChannelMention {
	Snowflake id;
	Snowflake guild_id;
	ChannelType type{};
	std::string name;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const ChannelMention &m);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ChannelMention &m);

enum class MessageActivityType : uint8_t {
	Join = 1,
	Spectate = 2,
	Listen = 3,
	JoinRequest = 5
};

struct MessageActivity {
	/// Type of message activity.
	MessageActivityType type{};
	/// ID of the player's party, lobby or group.
	std::optional<std::string> party_id;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const MessageActivity &a);
EKIZU_EXPORT void from_json(const nlohmann::json &j, MessageActivity &a);

struct MessageApplication {
	/// ID of the application.
	Snowflake id;
	/// Name of the application.
	std::string name;
	/// The icon hash of the app.
	std::optional<std::string> icon;
	/// The description of the app.
	std::string description;
	/// The application's default rich presence invite cover image hash.
	std::optional<std::string> cover_image;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const MessageApplication &a);
EKIZU_EXPORT void from_json(const nlohmann::json &j, MessageApplication &a);

enum class MessageFlags : uint32_t {
	/// This message has been published to subscribed channels (via Channel
	/// Following).
	Crossposted = 1 << 0,
	/// This message originated from a message in another channel (via Channel
	/// Following).
	IsCrosspost = 1 << 1,
	/// Do not include any embeds when serializing this message.
	SuppressEmbeds = 1 << 2,
	/// The source message for this crosspost has been deleted (via Channel
	/// Following).
	SourceMessageDeleted = 1 << 3,
	/// This message came from the urgent message system.
	Urgent = 1 << 4,
	/// This message has an associated thread, with the same id as the message.
	HasThread = 1 << 5,
	/// This message is only visible to the user who invoked the Interaction.
	Ephemeral = 1 << 6,
	/// This message is an Interaction Response and the bot is "thinking".
	Loading = 1 << 7,
	/// This message failed to mention some roles and add their members to the
	/// thread.
	FailedToMentionSomeRolesInThread = 1 << 8,
	/// This message will not trigger push and desktop notifications.
	SuppressNotifications = 1 << 12,
	/// This message is a voice message.
	IsVoiceMessage = 1 << 13,

	/// This message uses Components V2 (containers/sections/text display, etc).
	IsComponentsV2 = 1 << 15,
};

struct MessageInteraction {
	/// ID of the interaction.
	Snowflake id;
	/// Type of interaction.
	InteractionType type;
	/// Name of the application command, including subcommands and subcommand
	/// groups.
	std::string name;
	/// User who invoked the interaction.
	User user;
	/// Member who invoked the interaction in the guild.
	std::optional<PartialMember> member;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const MessageInteraction &i);
EKIZU_EXPORT void from_json(const nlohmann::json &j, MessageInteraction &i);

/**
 * @see
 * [https://discord.com/developers/docs/resources/channel#message-reference-object-message-reference-structure](https://discord.com/developers/docs/resources/channel#message-reference-object-message-reference-structure)
 */
struct MessageReference {
	/// The ID of the originating message.
	std::optional<Snowflake> message_id;
	/// The ID of the originating channel. Is optional when creating a
	/// reply,
	/// but will always be present when receiving a message.
	std::optional<Snowflake> channel_id;
	/// The ID of the originating guild.
	std::optional<Snowflake> guild_id;
	/// When sending, whether to error if the referenced message doesn't
	/// exist
	/// instead of sending as a normal (non-reply) message, default true.
	std::optional<bool> fail_if_not_exists;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const MessageReference &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j, MessageReference &r);

/// Represents the parameters you send to the API when creating a message.
struct MessagePayload {
	/// The content of the message (up to 2000 characters).
	std::optional<std::string> content;
	/// Whether or not this is a TTS message.
	std::optional<bool> tts;
	/// Embedded rich content (up to 6000 characters).
	std::optional<std::vector<Embed> > embeds;
	/// Allowed mentions for the message.
	std::optional<AllowedMentions> allowed_mentions;
	/// Include this to make the message a reply.
	std::optional<MessageReference> message_reference;
	/// Components to include within the message.
	std::optional<std::vector<MessageComponent> > components;
	/// IDs of up to 3 stickers in the server to send in the message.
	std::optional<std::vector<Snowflake> > sticker_ids;
};

enum class MessageType : uint8_t {
	Default = 0,
	RecipientAdd = 1,
	RecipientRemove = 2,
	Call = 3,
	ChannelNameChange = 4,
	ChannelIconChange = 5,
	ChannelPinnedMessage = 6,
	GuildMemberJoin = 7,
	UserPremiumGuildSubscription = 8,
	UserPremiumGuildSubscriptionTier1 = 9,
	UserPremiumGuildSubscriptionTier2 = 10,
	UserPremiumGuildSubscriptionTier3 = 11,
	ChannelFollowAdd = 12,
	GuildDiscoveryDisqualified = 14,
	GuildDiscoveryRequalified = 15,
	GuildDiscoveryGracePeriodInitialWarning = 16,
	GuildDiscoveryGracePeriodFinalWarning = 17,
	ThreadCreated = 18,
	Reply = 19,
	ChatInputCommand = 20,
	ThreadStarterMessage = 21,
	GuildInviteReminder = 22,
	ContextMenuCommand = 23,
};

struct Message {
	/// The ID of the message.
	Snowflake id;
	/// The ID of the channel the message was sent in.
	Snowflake channel_id;
	/// The ID of the guild the message was sent in.
	std::optional<Snowflake> guild_id;
	/// The author of this message (not guaranteed to be a valid user, see
	/// below).
	User author;
	/// The content of the message.
	std::string content;
	/// The timestamp of the message.
	std::string timestamp;
	/// The timestamp of the message's last edit.
	std::optional<std::string> edited_timestamp;
	/// Whether the message is TTS.
	bool tts{};
	/// Whether the message mentions everyone.
	bool mention_everyone{};
	/// The users specifically mentioned in the message.
	std::vector<User> mentions;
	/// The roles specifically mentioned in the message.
	std::vector<Snowflake> mention_roles;
	/// The channels specifically mentioned in the message.
	std::vector<ChannelMention> mention_channels;
	/// The attachments in the message.
	std::vector<Attachment> attachments;
	/// The embeds in the message.
	std::vector<Embed> embeds;
	/// Used for validating a message was sent.
	std::string nonce;
	/// Whether the message is pinned.
	bool pinned{};
	/// The ID of the webhook that sent the message.
	Snowflake webhook_id;
	/// The type of the message.
	MessageType type;
	/// This is sent with Rich Presence-related chat embeds.
	std::optional<MessageActivity> activity;
	/// This is sent with Rich Presence-related chat embeds.
	std::optional<MessageApplication> application;
	/// If the message is an Interaction or application-owned webhook, this is
	/// The id of the application.
	std::optional<Snowflake> application_id;
	/// Data showing the source of a crosspost, channel follow add, pin, or
	/// reply message.
	MessageReference message_reference;
	/// Message flags combined as a bitfield.
	MessageFlags flags;
	/// The message associated with the message_reference.
	Snowflake referenced_message;
	/// Sent if the message is a response to an Interaction.
	std::optional<MessageInteraction> interaction;
	/// The thread that was started from this message.
	// std::shared_ptr<Channel> thread ;
	/// The top-level components field is an array of Action Row components.
	std::vector<MessageComponent> components;
	/// Sent if the message contains stickers.
	// std::vector<StickerItem> sticker_items ;
	/// Whether the message was crossposted.
	bool crossposted{};
	/// Whether the message was deleted.
	bool deleted{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Message &m);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Message &m);
}  // namespace ekizu

#endif	// EKIZU_MESSAGE_HPP
