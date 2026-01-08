#include <ekizu/error.hpp>
#include <ekizu/error_context.hpp>
#include <ekizu/http_client.hpp>

namespace ekizu {

HttpClient::HttpClient(const boost::asio::any_io_executor &executor,
					   std::string token)
	: m_strand{executor},
	  m_rate_limiter{
		  executor, [this](net::HttpRequest req,
						   boost::asio::any_completion_handler<void(
							   Result<net::HttpResponse>)>
							   h) { send_http(std::move(req), std::move(h)); }},
	  m_token{std::move(token)} {}

void HttpClient::shutdown() {
	boost::asio::dispatch(m_strand, [this] {
		m_rate_limiter.shutdown();
		m_token.reset();
		m_http.reset();
	});
}

GetChannel HttpClient::get_channel(Snowflake channel_id) {
	return GetChannel{RequestSender{&m_rate_limiter}, channel_id};
}

ModifyChannel HttpClient::modify_channel(Snowflake channel_id) {
	return ModifyChannel{RequestSender{&m_rate_limiter}, channel_id};
}

DeleteChannel HttpClient::delete_channel(Snowflake channel_id) {
	return DeleteChannel{RequestSender{&m_rate_limiter}, channel_id};
}

GetChannelMessages HttpClient::get_channel_messages(Snowflake channel_id) {
	return GetChannelMessages{RequestSender{&m_rate_limiter}, channel_id};
}

GetChannelMessage HttpClient::get_channel_message(Snowflake channel_id,
												  Snowflake message_id) {
	return GetChannelMessage{
		RequestSender{&m_rate_limiter}, channel_id, message_id};
}

CreateMessage HttpClient::create_message(Snowflake channel_id) {
	return CreateMessage{RequestSender{&m_rate_limiter}, channel_id};
}

CrosspostMessage HttpClient::crosspost_message(Snowflake channel_id,
											   Snowflake message_id) {
	return CrosspostMessage{
		RequestSender{&m_rate_limiter}, channel_id, message_id};
}

CreateReaction HttpClient::create_reaction(
	Snowflake channel_id, Snowflake message_id, RequestReaction emoji) {
	return CreateReaction{RequestSender{&m_rate_limiter}, channel_id,
						  message_id, std::move(emoji)};
}

DeleteOwnReaction HttpClient::delete_own_reaction(
	Snowflake channel_id, Snowflake message_id, RequestReaction emoji) {
	return DeleteOwnReaction{RequestSender{&m_rate_limiter}, channel_id,
							 message_id, std::move(emoji)};
}

DeleteUserReaction HttpClient::delete_user_reaction(
	Snowflake channel_id, Snowflake message_id, RequestReaction emoji,
	Snowflake user_id) {
	return DeleteUserReaction{RequestSender{&m_rate_limiter}, channel_id,
							  message_id, std::move(emoji), user_id};
}

GetReactions HttpClient::get_reactions(
	Snowflake channel_id, Snowflake message_id, RequestReaction emoji) {
	return GetReactions{RequestSender{&m_rate_limiter}, channel_id, message_id,
						std::move(emoji)};
}

DeleteAllReactions HttpClient::delete_all_reactions(Snowflake channel_id,
													Snowflake message_id) {
	return DeleteAllReactions{
		RequestSender{&m_rate_limiter}, channel_id, message_id};
}

DeleteAllReactionsForEmoji HttpClient::delete_all_reactions_for_emoji(
	Snowflake channel_id, Snowflake message_id, RequestReaction emoji) {
	return DeleteAllReactionsForEmoji{RequestSender{&m_rate_limiter},
									  channel_id, message_id, std::move(emoji)};
}

EditMessage HttpClient::edit_message(Snowflake channel_id,
									 Snowflake message_id) {
	return EditMessage{RequestSender{&m_rate_limiter}, channel_id, message_id};
}

DeleteMessage HttpClient::delete_message(Snowflake channel_id,
										 Snowflake message_id) {
	return DeleteMessage{
		RequestSender{&m_rate_limiter}, channel_id, message_id};
}

BulkDeleteMessages HttpClient::bulk_delete_messages(
	Snowflake channel_id, const std::vector<Snowflake> &message_ids) {
	return BulkDeleteMessages{
		RequestSender{&m_rate_limiter}, channel_id, message_ids};
}

EditChannelPermissions HttpClient::edit_channel_permissions(
	Snowflake channel_id, const PermissionOverwrite &overwrite) {
	return EditChannelPermissions{
		RequestSender{&m_rate_limiter}, channel_id, overwrite};
}

GetChannelInvites HttpClient::get_channel_invites(Snowflake channel_id) {
	return GetChannelInvites{RequestSender{&m_rate_limiter}, channel_id};
}

CreateInvite HttpClient::create_invite(Snowflake channel_id) {
	return CreateInvite{RequestSender{&m_rate_limiter}, channel_id};
}

DeleteChannelPermission HttpClient::delete_channel_permission(
	Snowflake channel_id, Snowflake overwrite_id) {
	return DeleteChannelPermission{
		RequestSender{&m_rate_limiter}, channel_id, overwrite_id};
}

FollowAnnouncementChannel HttpClient::follow_announcement_channel(
	Snowflake channel_id, Snowflake webhook_channel_id) {
	return FollowAnnouncementChannel{
		RequestSender{&m_rate_limiter}, channel_id, webhook_channel_id};
}

TriggerTypingIndicator HttpClient::trigger_typing_indicator(
	Snowflake channel_id) {
	return TriggerTypingIndicator{RequestSender{&m_rate_limiter}, channel_id};
}

GetPinnedMessages HttpClient::get_pinned_messages(Snowflake channel_id) {
	return GetPinnedMessages{RequestSender{&m_rate_limiter}, channel_id};
}

PinMessage HttpClient::pin_message(Snowflake channel_id, Snowflake message_id) {
	return PinMessage{RequestSender{&m_rate_limiter}, channel_id, message_id};
}

UnpinMessage HttpClient::unpin_message(Snowflake channel_id,
									   Snowflake message_id) {
	return UnpinMessage{RequestSender{&m_rate_limiter}, channel_id, message_id};
}

CreateGuild HttpClient::create_guild(std::string name) {
	return CreateGuild{RequestSender{&m_rate_limiter}, std::move(name)};
}

GetGuild HttpClient::get_guild(Snowflake guild_id) {
	return GetGuild{RequestSender{&m_rate_limiter}, guild_id};
}

GetGuildPreview HttpClient::get_guild_preview(Snowflake guild_id) {
	return GetGuildPreview{RequestSender{&m_rate_limiter}, guild_id};
}

ModifyGuild HttpClient::modify_guild(Snowflake guild_id) {
	return ModifyGuild{RequestSender{&m_rate_limiter}, guild_id};
}

DeleteGuild HttpClient::delete_guild(Snowflake guild_id) {
	return DeleteGuild{RequestSender{&m_rate_limiter}, guild_id};
}

GetGuildChannels HttpClient::get_guild_channels(Snowflake guild_id) {
	return GetGuildChannels{RequestSender{&m_rate_limiter}, guild_id};
}

CreateGuildChannel HttpClient::create_guild_channel(Snowflake guild_id,
													std::string name) {
	return CreateGuildChannel{
		RequestSender{&m_rate_limiter}, guild_id, std::move(name)};
}

ModifyGuildChannelPositions HttpClient::modify_guild_channel_positions(
	Snowflake guild_id, std::vector<ModifyGuildChannelPosition> channels) {
	return ModifyGuildChannelPositions{
		RequestSender{&m_rate_limiter}, guild_id, std::move(channels)};
}

ListActiveGuildThreads HttpClient::list_active_guild_threads(
	Snowflake guild_id) {
	return ListActiveGuildThreads{RequestSender{&m_rate_limiter}, guild_id};
}

GetGuildMember HttpClient::get_guild_member(Snowflake guild_id,
											Snowflake user_id) {
	return GetGuildMember{RequestSender{&m_rate_limiter}, guild_id, user_id};
}

ListGuildMembers HttpClient::list_guild_members(Snowflake guild_id) {
	return ListGuildMembers{RequestSender{&m_rate_limiter}, guild_id};
}

SearchGuildMembers HttpClient::search_guild_members(Snowflake guild_id) {
	return SearchGuildMembers{RequestSender{&m_rate_limiter}, guild_id};
}

AddGuildMember HttpClient::add_guild_member(
	Snowflake guild_id, Snowflake user_id, std::string access_token) {
	return AddGuildMember{RequestSender{&m_rate_limiter}, guild_id, user_id,
						  std::move(access_token)};
}

ModifyGuildMember HttpClient::modify_guild_member(Snowflake guild_id,
												  Snowflake user_id) {
	return ModifyGuildMember{RequestSender{&m_rate_limiter}, guild_id, user_id};
}

ModifyCurrentMember HttpClient::modify_current_member(Snowflake guild_id) {
	return ModifyCurrentMember{RequestSender{&m_rate_limiter}, guild_id};
}

AddGuildMemberRole HttpClient::add_guild_member_role(
	Snowflake guild_id, Snowflake user_id, Snowflake role_id) {
	return AddGuildMemberRole{
		RequestSender{&m_rate_limiter}, guild_id, user_id, role_id};
}

RemoveGuildMemberRole HttpClient::remove_guild_member_role(
	Snowflake guild_id, Snowflake user_id, Snowflake role_id) {
	return RemoveGuildMemberRole{
		RequestSender{&m_rate_limiter}, guild_id, user_id, role_id};
}

RemoveGuildMember HttpClient::remove_guild_member(Snowflake guild_id,
												  Snowflake user_id) {
	return RemoveGuildMember{RequestSender{&m_rate_limiter}, guild_id, user_id};
}

GetGuildBans HttpClient::get_guild_bans(Snowflake guild_id) {
	return GetGuildBans{RequestSender{&m_rate_limiter}, guild_id};
}

GetGuildBan HttpClient::get_guild_ban(Snowflake guild_id, Snowflake user_id) {
	return GetGuildBan{RequestSender{&m_rate_limiter}, guild_id, user_id};
}

GetGuildInvites HttpClient::get_guild_invites(Snowflake guild_id) {
	return GetGuildInvites{RequestSender{&m_rate_limiter}, guild_id};
}

GetCurrentUser HttpClient::get_current_user() {
	return GetCurrentUser{RequestSender{&m_rate_limiter}};
}

GetUser HttpClient::get_user(Snowflake user_id) {
	return GetUser{RequestSender{&m_rate_limiter}, user_id};
}

ModifyCurrentUser HttpClient::modify_current_user() {
	return ModifyCurrentUser{RequestSender{&m_rate_limiter}};
}

CreateDM HttpClient::create_dm(Snowflake user_id) {
	return CreateDM{RequestSender{&m_rate_limiter}, user_id};
}

InteractionClient HttpClient::interaction(Snowflake application_id) {
	return InteractionClient{RequestSender{&m_rate_limiter}, application_id};
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
		ekizu::clear_error_context();
		ekizu::set_error_context(
			"HttpClient: missing bot token (client shut down or not "
			"initialized)");
		std::move(handler)(
			ekizu::make_error_code(ekizu::errc::http_not_authenticated));
		return;
	}

	// Normalize request
	req.set(net::http::field::authorization, fmt::format("Bot {}", *m_token));
	req.set(net::http::field::host, "discord.com");

	// Normalize target (ensure leading '/' and avoid double-prefixing on
	// retry).
	std::string target = boost::to_string(req.target());
	if (target.empty() || target.front() != '/') {
		target.insert(target.begin(), '/');
	}
	if (target.rfind("/api/", 0) != 0) { target = "/api/v10" + target; }
	req.target(target);

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