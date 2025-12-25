#include <ekizu/http_client.hpp>

namespace ekizu {

HttpClient::HttpClient(boost::asio::any_io_executor executor,
					   std::string_view token)
	: m_strand{executor},
	  m_rate_limiter{
		  executor, [this](net::HttpRequest req,
						   boost::asio::any_completion_handler<void(
							   Result<net::HttpResponse>)>
							   h) { send_http(std::move(req), std::move(h)); }},
	  m_token{std::string(token)} {}

GetChannel HttpClient::get_channel(Snowflake channel_id) const {
	return GetChannel{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id};
}

ModifyChannel HttpClient::modify_channel(Snowflake channel_id) const {
	return ModifyChannel{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id};
}

DeleteChannel HttpClient::delete_channel(Snowflake channel_id) const {
	return DeleteChannel{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id};
}

GetChannelMessages HttpClient::get_channel_messages(
	Snowflake channel_id) const {
	return GetChannelMessages{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id};
}

GetChannelMessage HttpClient::get_channel_message(Snowflake channel_id,
												  Snowflake message_id) const {
	return GetChannelMessage{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		message_id};
}

CreateMessage HttpClient::create_message(Snowflake channel_id) const {
	return CreateMessage{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id};
}

CrosspostMessage HttpClient::crosspost_message(Snowflake channel_id,
											   Snowflake message_id) const {
	return CrosspostMessage{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		message_id};
}

CreateReaction HttpClient::create_reaction(
	Snowflake channel_id, Snowflake message_id, RequestReaction emoji) const {
	return CreateReaction{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		message_id, std::move(emoji)};
}

DeleteOwnReaction HttpClient::delete_own_reaction(
	Snowflake channel_id, Snowflake message_id, RequestReaction emoji) const {
	return DeleteOwnReaction{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		message_id, std::move(emoji)};
}

DeleteUserReaction HttpClient::delete_user_reaction(
	Snowflake channel_id, Snowflake message_id, RequestReaction emoji,
	Snowflake user_id) const {
	return DeleteUserReaction{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		message_id, std::move(emoji), user_id};
}

GetReactions HttpClient::get_reactions(
	Snowflake channel_id, Snowflake message_id, RequestReaction emoji) const {
	return GetReactions{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		message_id, std::move(emoji)};
}

DeleteAllReactions HttpClient::delete_all_reactions(
	Snowflake channel_id, Snowflake message_id) const {
	return DeleteAllReactions{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		message_id};
}

DeleteAllReactionsForEmoji HttpClient::delete_all_reactions_for_emoji(
	Snowflake channel_id, Snowflake message_id, RequestReaction emoji) const {
	return DeleteAllReactionsForEmoji{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		message_id, std::move(emoji)};
}

EditMessage HttpClient::edit_message(Snowflake channel_id,
									 Snowflake message_id) const {
	return EditMessage{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		message_id};
}

DeleteMessage HttpClient::delete_message(Snowflake channel_id,
										 Snowflake message_id) const {
	return DeleteMessage{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		message_id};
}

BulkDeleteMessages HttpClient::bulk_delete_messages(
	Snowflake channel_id, const std::vector<Snowflake> &message_ids) const {
	return BulkDeleteMessages{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		message_ids};
}

EditChannelPermissions HttpClient::edit_channel_permissions(
	Snowflake channel_id, const PermissionOverwrite &overwrite) const {
	return EditChannelPermissions{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		overwrite};
}

GetChannelInvites HttpClient::get_channel_invites(Snowflake channel_id) const {
	return GetChannelInvites{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id};
}

CreateInvite HttpClient::create_invite(Snowflake channel_id) const {
	return CreateInvite{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id};
}

DeleteChannelPermission HttpClient::delete_channel_permission(
	Snowflake channel_id, Snowflake overwrite_id) {
	return DeleteChannelPermission{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		overwrite_id};
}

FollowAnnouncementChannel HttpClient::follow_announcement_channel(
	Snowflake channel_id, Snowflake webhook_channel_id) const {
	return FollowAnnouncementChannel{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		webhook_channel_id};
}

TriggerTypingIndicator HttpClient::trigger_typing_indicator(
	Snowflake channel_id) const {
	return TriggerTypingIndicator{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id};
}

GetPinnedMessages HttpClient::get_pinned_messages(Snowflake channel_id) const {
	return GetPinnedMessages{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id};
}

PinMessage HttpClient::pin_message(Snowflake channel_id,
								   Snowflake message_id) const {
	return PinMessage{RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)},
					  channel_id, message_id};
}

UnpinMessage HttpClient::unpin_message(Snowflake channel_id,
									   Snowflake message_id) const {
	return UnpinMessage{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, channel_id,
		message_id};
}

CreateGuild HttpClient::create_guild(std::string_view name) const {
	return CreateGuild{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, name};
}

GetGuild HttpClient::get_guild(Snowflake guild_id) const {
	return GetGuild{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id};
}

GetGuildPreview HttpClient::get_guild_preview(Snowflake guild_id) const {
	return GetGuildPreview{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id};
}

ModifyGuild HttpClient::modify_guild(Snowflake guild_id) const {
	return ModifyGuild{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id};
}

DeleteGuild HttpClient::delete_guild(Snowflake guild_id) const {
	return DeleteGuild{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id};
}

GetGuildChannels HttpClient::get_guild_channels(Snowflake guild_id) const {
	return GetGuildChannels{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id};
}

CreateGuildChannel HttpClient::create_guild_channel(
	Snowflake guild_id, std::string_view name) const {
	return CreateGuildChannel{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id,
		name};
}

ModifyGuildChannelPositions HttpClient::modify_guild_channel_positions(
	Snowflake guild_id,
	std::vector<ModifyGuildChannelPosition> channels) const {
	return ModifyGuildChannelPositions{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id,
		std::move(channels)};
}

ListActiveGuildThreads HttpClient::list_active_guild_threads(
	Snowflake guild_id) const {
	return ListActiveGuildThreads{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id};
}

GetGuildMember HttpClient::get_guild_member(Snowflake guild_id,
											Snowflake user_id) const {
	return GetGuildMember{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id,
		user_id};
}

ListGuildMembers HttpClient::list_guild_members(Snowflake guild_id) const {
	return ListGuildMembers{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id};
}

SearchGuildMembers HttpClient::search_guild_members(Snowflake guild_id) const {
	return SearchGuildMembers{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id};
}

AddGuildMember HttpClient::add_guild_member(
	Snowflake guild_id, Snowflake user_id,
	std::string_view access_token) const {
	return AddGuildMember{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id,
		user_id, access_token};
}

ModifyGuildMember HttpClient::modify_guild_member(Snowflake guild_id,
												  Snowflake user_id) const {
	return ModifyGuildMember{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id,
		user_id};
}

ModifyCurrentMember HttpClient::modify_current_member(
	Snowflake guild_id) const {
	return ModifyCurrentMember{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id};
}

AddGuildMemberRole HttpClient::add_guild_member_role(
	Snowflake guild_id, Snowflake user_id, Snowflake role_id) const {
	return AddGuildMemberRole{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id,
		user_id, role_id};
}

RemoveGuildMemberRole HttpClient::remove_guild_member_role(
	Snowflake guild_id, Snowflake user_id, Snowflake role_id) const {
	return RemoveGuildMemberRole{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id,
		user_id, role_id};
}

RemoveGuildMember HttpClient::remove_guild_member(Snowflake guild_id,
												  Snowflake user_id) const {
	return RemoveGuildMember{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id,
		user_id};
}

GetGuildBans HttpClient::get_guild_bans(Snowflake guild_id) const {
	return GetGuildBans{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id};
}

GetGuildBan HttpClient::get_guild_ban(Snowflake guild_id,
									  Snowflake user_id) const {
	return GetGuildBan{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id,
		user_id};
}

GetGuildInvites HttpClient::get_guild_invites(Snowflake guild_id) const {
	return GetGuildInvites{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, guild_id};
}

GetCurrentUser HttpClient::get_current_user() const {
	return GetCurrentUser{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}};
}

GetUser HttpClient::get_user(Snowflake user_id) const {
	return GetUser{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, user_id};
}

ModifyCurrentUser HttpClient::modify_current_user() const {
	return ModifyCurrentUser{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}};
}

CreateDM HttpClient::create_dm(Snowflake user_id) const {
	return CreateDM{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)}, user_id};
}

InteractionClient HttpClient::interaction(Snowflake application_id) const {
	return InteractionClient{
		RequestSender{const_cast<RateLimiter *>(&m_rate_limiter)},
		application_id};
}

void HttpClient::send_http(
	net::HttpRequest req,
	boost::asio::any_completion_handler<void(Result<net::HttpResponse>)>
		handler) {
	// Serialize on the HttpClient strand.
	boost::asio::dispatch(m_strand, [this, req = std::move(req),
									 h = std::move(handler)]() mutable {
		send_http_attempt(std::move(req), 0, std::move(h));
	});
}

void HttpClient::send_http_attempt(
	net::HttpRequest req, int attempt,
	boost::asio::any_completion_handler<void(Result<net::HttpResponse>)>
		handler) {
	if (!m_token) {
		std::move(handler)(boost::system::errc::operation_not_permitted);
		return;
	}

	// Normalize request
	req.set(net::http::field::authorization, fmt::format("Bot {}", *m_token));
	req.set(net::http::field::host, "discord.com");
	req.target(fmt::format("/api/v10{}", boost::to_string(req.target())));

	// Copy for a single retry.
	net::HttpRequest original = req;

	auto retry_or_finish =
		[this, attempt, original = std::move(original)](
			boost::asio::any_completion_handler<void(Result<net::HttpResponse>)>
				h,
			Result<net::HttpResponse> res) mutable {
			if (res) {
				std::move(h)(std::move(res));
				return;
			}

			if (attempt >= 1) {
				std::move(h)(res.error());
				return;
			}

			// Reconnect and retry once.
			m_http.reset();
			send_http_attempt(std::move(original), attempt + 1, std::move(h));
		};

	if (!m_http) {
		net::HttpConnection::connect(
			m_strand.get_inner_executor(), "https://discord.com",
			boost::asio::bind_executor(
				m_strand, [this, req = std::move(req), h = std::move(handler),
						   retry_or_finish = std::move(retry_or_finish)](
							  Result<net::HttpConnection> conn) mutable {
					if (!conn) {
						std::move(h)(conn.error());
						return;
					}
					m_http = std::move(conn.value());

					m_http->request(
						std::move(req),
						boost::asio::bind_executor(
							m_strand,
							[h = std::move(h),
							 retry_or_finish = std::move(retry_or_finish)](
								Result<net::HttpResponse> res) mutable {
								retry_or_finish(std::move(h), std::move(res));
							}));
				}));
		return;
	}

	m_http->request(
		std::move(req),
		boost::asio::bind_executor(
			m_strand, [h = std::move(handler),
					   retry_or_finish = std::move(retry_or_finish)](
						  Result<net::HttpResponse> res) mutable {
				retry_or_finish(std::move(h), std::move(res));
			}));
}

}  // namespace ekizu