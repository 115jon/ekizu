#ifndef EKIZU_HTTP_CLIENT_HPP
#define EKIZU_HTTP_CLIENT_HPP

#include <ekizu/rate_limiter.hpp>
#include <ekizu/request/add_guild_member.hpp>
#include <ekizu/request/add_guild_member_role.hpp>
#include <ekizu/request/bulk_delete_messages.hpp>
#include <ekizu/request/create_dm.hpp>
#include <ekizu/request/create_guild.hpp>
#include <ekizu/request/create_guild_channel.hpp>
#include <ekizu/request/create_invite.hpp>
#include <ekizu/request/create_message.hpp>
#include <ekizu/request/create_reaction.hpp>
#include <ekizu/request/crosspost_message.hpp>
#include <ekizu/request/delete_all_reactions.hpp>
#include <ekizu/request/delete_all_reactions_for_emoji.hpp>
#include <ekizu/request/delete_channel.hpp>
#include <ekizu/request/delete_channel_permission.hpp>
#include <ekizu/request/delete_guild.hpp>
#include <ekizu/request/delete_message.hpp>
#include <ekizu/request/delete_own_reaction.hpp>
#include <ekizu/request/delete_user_reaction.hpp>
#include <ekizu/request/edit_channel_permissions.hpp>
#include <ekizu/request/edit_message.hpp>
#include <ekizu/request/follow_announcement_channel.hpp>
#include <ekizu/request/get_channel.hpp>
#include <ekizu/request/get_channel_invites.hpp>
#include <ekizu/request/get_channel_message.hpp>
#include <ekizu/request/get_channel_messages.hpp>
#include <ekizu/request/get_current_user.hpp>
#include <ekizu/request/get_guild.hpp>
#include <ekizu/request/get_guild_ban.hpp>
#include <ekizu/request/get_guild_bans.hpp>
#include <ekizu/request/get_guild_channels.hpp>
#include <ekizu/request/get_guild_invites.hpp>
#include <ekizu/request/get_guild_member.hpp>
#include <ekizu/request/get_guild_preview.hpp>
#include <ekizu/request/get_pinned_messages.hpp>
#include <ekizu/request/get_reactions.hpp>
#include <ekizu/request/get_user.hpp>
#include <ekizu/request/interaction_client.hpp>
#include <ekizu/request/list_active_guild_threads.hpp>
#include <ekizu/request/list_guild_members.hpp>
#include <ekizu/request/modify_channel.hpp>
#include <ekizu/request/modify_current_member.hpp>
#include <ekizu/request/modify_current_user.hpp>
#include <ekizu/request/modify_guild.hpp>
#include <ekizu/request/modify_guild_channel_positions.hpp>
#include <ekizu/request/modify_guild_member.hpp>
#include <ekizu/request/pin_message.hpp>
#include <ekizu/request/remove_guild_member.hpp>
#include <ekizu/request/remove_guild_member_role.hpp>
#include <ekizu/request/search_guild_members.hpp>
#include <ekizu/request/trigger_typing_indicator.hpp>
#include <ekizu/request/unpin_message.hpp>

namespace ekizu {
struct HttpClient {
	EKIZU_EXPORT explicit HttpClient(
		const boost::asio::any_io_executor &executor, std::string_view token);

	[[nodiscard]] asio::any_io_executor get_executor() const {
		return m_strand.get_inner_executor();
	}

	EKIZU_EXPORT void shutdown();

	// (public request builders unchanged)
	[[nodiscard]] EKIZU_EXPORT GetChannel get_channel(Snowflake channel_id);
	[[nodiscard]] EKIZU_EXPORT ModifyChannel
	modify_channel(Snowflake channel_id);
	[[nodiscard]] EKIZU_EXPORT DeleteChannel
	delete_channel(Snowflake channel_id);
	[[nodiscard]] EKIZU_EXPORT GetChannelMessages
	get_channel_messages(Snowflake channel_id);
	[[nodiscard]] EKIZU_EXPORT GetChannelMessage
	get_channel_message(Snowflake channel_id, Snowflake message_id);
	[[nodiscard]] EKIZU_EXPORT CreateMessage
	create_message(Snowflake channel_id);
	[[nodiscard]] EKIZU_EXPORT CrosspostMessage
	crosspost_message(Snowflake channel_id, Snowflake message_id);
	[[nodiscard]] EKIZU_EXPORT CreateReaction create_reaction(
		Snowflake channel_id, Snowflake message_id, RequestReaction emoji);
	[[nodiscard]] EKIZU_EXPORT DeleteOwnReaction delete_own_reaction(
		Snowflake channel_id, Snowflake message_id, RequestReaction emoji);
	[[nodiscard]] EKIZU_EXPORT DeleteUserReaction delete_user_reaction(
		Snowflake channel_id, Snowflake message_id, RequestReaction emoji,
		Snowflake user_id);
	[[nodiscard]] EKIZU_EXPORT GetReactions get_reactions(
		Snowflake channel_id, Snowflake message_id, RequestReaction emoji);
	[[nodiscard]] EKIZU_EXPORT DeleteAllReactions
	delete_all_reactions(Snowflake channel_id, Snowflake message_id);
	[[nodiscard]] EKIZU_EXPORT DeleteAllReactionsForEmoji
	delete_all_reactions_for_emoji(Snowflake channel_id, Snowflake message_id,
								   RequestReaction emoji);
	[[nodiscard]] EKIZU_EXPORT EditMessage edit_message(Snowflake channel_id,
														Snowflake message_id);
	[[nodiscard]] EKIZU_EXPORT DeleteMessage
	delete_message(Snowflake channel_id, Snowflake message_id);
	[[nodiscard]] EKIZU_EXPORT BulkDeleteMessages bulk_delete_messages(
		Snowflake channel_id, const std::vector<Snowflake> &message_ids);
	[[nodiscard]] EKIZU_EXPORT EditChannelPermissions edit_channel_permissions(
		Snowflake channel_id, const PermissionOverwrite &overwrite);
	[[nodiscard]] EKIZU_EXPORT GetChannelInvites
	get_channel_invites(Snowflake channel_id);
	[[nodiscard]] EKIZU_EXPORT CreateInvite create_invite(Snowflake channel_id);
	[[nodiscard]] EKIZU_EXPORT DeleteChannelPermission
	delete_channel_permission(Snowflake channel_id, Snowflake overwrite_id);
	[[nodiscard]] EKIZU_EXPORT FollowAnnouncementChannel
	follow_announcement_channel(Snowflake channel_id,
								Snowflake webhook_channel_id);
	[[nodiscard]] EKIZU_EXPORT TriggerTypingIndicator
	trigger_typing_indicator(Snowflake channel_id);
	[[nodiscard]] EKIZU_EXPORT GetPinnedMessages
	get_pinned_messages(Snowflake channel_id);
	[[nodiscard]] EKIZU_EXPORT PinMessage pin_message(Snowflake channel_id,
													  Snowflake message_id);
	[[nodiscard]] EKIZU_EXPORT UnpinMessage unpin_message(Snowflake channel_id,
														  Snowflake message_id);

	[[nodiscard]] EKIZU_EXPORT CreateGuild create_guild(std::string_view name);
	[[nodiscard]] EKIZU_EXPORT GetGuild get_guild(Snowflake guild_id);
	[[nodiscard]] EKIZU_EXPORT GetGuildPreview
	get_guild_preview(Snowflake guild_id);
	[[nodiscard]] EKIZU_EXPORT ModifyGuild modify_guild(Snowflake guild_id);
	[[nodiscard]] EKIZU_EXPORT DeleteGuild delete_guild(Snowflake guild_id);
	[[nodiscard]] EKIZU_EXPORT GetGuildChannels
	get_guild_channels(Snowflake guild_id);
	[[nodiscard]] EKIZU_EXPORT CreateGuildChannel
	create_guild_channel(Snowflake guild_id, std::string_view name);
	[[nodiscard]] EKIZU_EXPORT ModifyGuildChannelPositions
	modify_guild_channel_positions(
		Snowflake guild_id, std::vector<ModifyGuildChannelPosition> channels);
	[[nodiscard]] EKIZU_EXPORT ListActiveGuildThreads
	list_active_guild_threads(Snowflake guild_id);
	[[nodiscard]] EKIZU_EXPORT GetGuildMember
	get_guild_member(Snowflake guild_id, Snowflake user_id);
	[[nodiscard]] EKIZU_EXPORT ListGuildMembers
	list_guild_members(Snowflake guild_id);
	[[nodiscard]] EKIZU_EXPORT SearchGuildMembers
	search_guild_members(Snowflake guild_id);
	[[nodiscard]] EKIZU_EXPORT AddGuildMember add_guild_member(
		Snowflake guild_id, Snowflake user_id, std::string_view access_token);
	[[nodiscard]] EKIZU_EXPORT ModifyGuildMember
	modify_guild_member(Snowflake guild_id, Snowflake user_id);
	[[nodiscard]] EKIZU_EXPORT ModifyCurrentMember
	modify_current_member(Snowflake guild_id);
	[[nodiscard]] EKIZU_EXPORT AddGuildMemberRole add_guild_member_role(
		Snowflake guild_id, Snowflake user_id, Snowflake role_id);
	[[nodiscard]] EKIZU_EXPORT RemoveGuildMemberRole remove_guild_member_role(
		Snowflake guild_id, Snowflake user_id, Snowflake role_id);
	[[nodiscard]] EKIZU_EXPORT RemoveGuildMember
	remove_guild_member(Snowflake guild_id, Snowflake user_id);
	[[nodiscard]] EKIZU_EXPORT GetGuildBans get_guild_bans(Snowflake guild_id);
	[[nodiscard]] EKIZU_EXPORT GetGuildBan get_guild_ban(Snowflake guild_id,
														 Snowflake user_id);
	[[nodiscard]] EKIZU_EXPORT GetGuildInvites
	get_guild_invites(Snowflake guild_id);

	[[nodiscard]] EKIZU_EXPORT GetCurrentUser get_current_user();
	[[nodiscard]] EKIZU_EXPORT GetUser get_user(Snowflake user_id);
	[[nodiscard]] EKIZU_EXPORT ModifyCurrentUser modify_current_user();
	[[nodiscard]] EKIZU_EXPORT CreateDM create_dm(Snowflake user_id);
	[[nodiscard]] EKIZU_EXPORT InteractionClient
	interaction(Snowflake application_id);

   private:
	// Low-level async send used by RateLimiter; completion handler will be
	// invoked.
	EKIZU_EXPORT void send_http(
		net::HttpRequest req,
		boost::asio::any_completion_handler<void(Result<net::HttpResponse>)>
			handler);

	EKIZU_EXPORT void send_http_attempt(
		net::HttpRequest req, int attempt,
		boost::asio::any_completion_handler<void(Result<net::HttpResponse>)>
			handler);

	boost::asio::strand<boost::asio::any_io_executor> m_strand;
	std::optional<net::HttpConnection> m_http;
	RateLimiter m_rate_limiter;
	std::optional<std::string> m_token;
};
}  // namespace ekizu

#endif	// EKIZU_HTTP_CLIENT_HPP
