#include <fmt/format.h>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl/error.hpp>
#include <cmath>
#include <ekizu/json_util.hpp>
#include <ekizu/shard.hpp>

namespace {
// TODO: Add gateway url customization
constexpr const char *GATEWAY_URL =
	"wss://gateway.discord.gg/?v=10&encoding=json&compress=zlib-stream";
constexpr const char *GATEWAY_JSON_ZLIB_QUERY =
	"?v=10&encoding=json&compress=zlib-stream";

ekizu::Result<ekizu::Event> event_from_str(std::string_view event_type,
										   const nlohmann::json &data) {
#define EKIZU_EVENT(s, v) \
	if (event_type == #s) { return data.get<ekizu::v>(); }

	EKIZU_EVENT(CHANNEL_CREATE, ChannelCreate)
	EKIZU_EVENT(CHANNEL_DELETE, ChannelDelete)
	EKIZU_EVENT(CHANNEL_PINS_UPDATE, ChannelPinsUpdate)
	EKIZU_EVENT(CHANNEL_UPDATE, ChannelUpdate)
	EKIZU_EVENT(GUILD_BAN_ADD, GuildBanAdd)
	EKIZU_EVENT(GUILD_BAN_REMOVE, GuildBanRemove)
	EKIZU_EVENT(GUILD_CREATE, GuildCreate)
	EKIZU_EVENT(GUILD_DELETE, GuildDelete)
	EKIZU_EVENT(GUILD_EMOJIS_UPDATE, GuildEmojisUpdate)
	EKIZU_EVENT(GUILD_INTEGRATIONS_UPDATE, GuildIntegrationsUpdate)
	EKIZU_EVENT(GUILD_MEMBER_ADD, GuildMemberAdd)
	EKIZU_EVENT(GUILD_MEMBER_REMOVE, GuildMemberRemove)
	EKIZU_EVENT(GUILD_MEMBER_UPDATE, GuildMemberUpdate)
	EKIZU_EVENT(GUILD_MEMBERS_CHUNK, GuildMembersChunk)
	EKIZU_EVENT(GUILD_ROLE_CREATE, GuildRoleCreate)
	EKIZU_EVENT(GUILD_ROLE_DELETE, GuildRoleDelete)
	EKIZU_EVENT(GUILD_ROLE_UPDATE, GuildRoleUpdate)
	EKIZU_EVENT(GUILD_SCHEDULED_EVENT_CREATE, GuildScheduledEventCreate)
	EKIZU_EVENT(GUILD_SCHEDULED_EVENT_DELETE, GuildScheduledEventDelete)
	EKIZU_EVENT(GUILD_SCHEDULED_EVENT_UPDATE, GuildScheduledEventUpdate)
	EKIZU_EVENT(GUILD_SCHEDULED_EVENT_USER_ADD, GuildScheduledEventUserAdd)
	EKIZU_EVENT(
		GUILD_SCHEDULED_EVENT_USER_REMOVE, GuildScheduledEventUserRemove)
	EKIZU_EVENT(GUILD_STICKERS_UPDATE, GuildStickersUpdate)
	EKIZU_EVENT(GUILD_UPDATE, GuildUpdate)
	EKIZU_EVENT(INTEGRATION_CREATE, IntegrationCreate)
	EKIZU_EVENT(INTEGRATION_DELETE, IntegrationDelete)
	EKIZU_EVENT(INTEGRATION_UPDATE, IntegrationUpdate)
	EKIZU_EVENT(INTERACTION_CREATE, InteractionCreate)
	EKIZU_EVENT(INVITE_CREATE, InviteCreate)
	EKIZU_EVENT(INVITE_DELETE, InviteDelete)
	EKIZU_EVENT(MESSAGE_CREATE, MessageCreate)
	EKIZU_EVENT(MESSAGE_DELETE, MessageDelete)
	EKIZU_EVENT(MESSAGE_DELETE_BULK, MessageDeleteBulk)
	EKIZU_EVENT(MESSAGE_REACTION_ADD, MessageReactionAdd)
	EKIZU_EVENT(MESSAGE_REACTION_REMOVE, MessageReactionRemove)
	EKIZU_EVENT(MESSAGE_REACTION_REMOVE_ALL, MessageReactionRemoveAll)
	EKIZU_EVENT(MESSAGE_REACTION_REMOVE_EMOJI, MessageReactionRemoveEmoji)
	EKIZU_EVENT(MESSAGE_UPDATE, MessageUpdate)
	EKIZU_EVENT(PRESENCE_UPDATE, PresenceUpdate)
	EKIZU_EVENT(READY, Ready)

	// Special case: Resumed has no data so we return a dummy object.
	if (event_type == "RESUMED") { return ekizu::Resumed{}; }

	EKIZU_EVENT(STAGE_INSTANCE_CREATE, StageInstanceCreate)
	EKIZU_EVENT(STAGE_INSTANCE_DELETE, StageInstanceDelete)
	EKIZU_EVENT(STAGE_INSTANCE_UPDATE, StageInstanceUpdate)
	EKIZU_EVENT(THREAD_CREATE, ThreadCreate)
	EKIZU_EVENT(THREAD_DELETE, ThreadDelete)
	EKIZU_EVENT(THREAD_LIST_SYNC, ThreadListSync)
	EKIZU_EVENT(THREAD_MEMBER_UPDATE, ThreadMemberUpdate)
	EKIZU_EVENT(THREAD_MEMBERS_UPDATE, ThreadMembersUpdate)
	EKIZU_EVENT(THREAD_UPDATE, ThreadUpdate)
	EKIZU_EVENT(TYPING_START, TypingStart)
	EKIZU_EVENT(USER_UPDATE, UserUpdate)
	EKIZU_EVENT(VOICE_SERVER_UPDATE, VoiceServerUpdate)
	EKIZU_EVENT(VOICE_STATE_UPDATE, VoiceStateUpdate)
	EKIZU_EVENT(WEBHOOKS_UPDATE, WebhooksUpdate)

	// Not an error, we just don't handle it.
	return boost::system::error_code{};
}

template <typename Fn>
void post_via(ekizu::asio::any_io_executor via,
			  ekizu::Shard::CompletionExecutor ex, Fn fn) {
	ekizu::asio::post(std::move(via),
					  ekizu::asio::bind_executor(std::move(ex), std::move(fn)));
}

template <typename R>
void post_completion(ekizu::asio::any_io_executor via,
					 ekizu::Shard::CompletionExecutor ex,
					 ekizu::asio::any_completion_handler<void(R)> h, R r) {
	post_via(std::move(via), std::move(ex),
			 [h = std::move(h), r = std::move(r)]() mutable {
				 std::move(h)(std::move(r));
			 });
}

bool is_transient_read_error(const boost::system::error_code &ec) {
	return ec == ekizu::asio::error::operation_aborted ||
		   ec == ekizu::asio::error::eof ||
		   ec == ekizu::asio::error::connection_reset ||
		   ec == ekizu::net::ws::error::closed ||
		   ec == boost::asio::ssl::error::stream_truncated;
}

}  // namespace
   // namespace

namespace ekizu {
void to_json(nlohmann::json &j, const UpdatePresence &p) {
	using json_util::serialize;
	if (p.idle_since) {
		j["since"] = *p.idle_since;

	} else {
		j["since"] = nullptr;
	}
	serialize(j, "activities", p.activities);
	serialize(j, "status", p.status);
	serialize(j, "afk", p.afk);
}

Shard::Shard(asio::any_io_executor executor, ShardId id, std::string_view token,
			 Intents intents)
	: m_strand{executor},
	  m_id{id.id},
	  m_config{std::string{token}, intents, id.total} {
	m_logger.prefix = fmt::format("shard[{}, {}]", m_id, m_config.shard_count);
}

void Shard::close_impl(CloseFrame reason,
					   asio::any_completion_handler<void(Result<>)> h,
					   CompletionExecutor hex) {
	auto via = get_executor();
	asio::dispatch(m_strand, [this, via, reason, h = std::move(h),
							  hex = std::move(hex)]() mutable {
		// Always set intentional_close for normal shutdown, even if not
		// connected. This prevents next_event_impl from attempting to
		// reconnect.
		if (reason.code == net::ws::close_code::normal ||
			reason.code == net::ws::close_code::going_away) {
			m_intentional_close = true;
			m_resume_gateway_url.reset();
			m_session.reset();
		}

		// Always cancel heartbeat timer - it keeps io_context alive
		if (m_timer) {
			m_heartbeat_running = false;
			m_timer->cancel();
			m_timer.reset();
		}

		if (!m_ws) {
			post_completion(via, std::move(hex), std::move(h),
							Result<>{boost::system::errc::not_connected});
			return;
		}

		m_logger.debug("sending websocket close message | code={}, reason={}",
					   reason.code, reason.reason.data());

		net::ws::close_reason cr{
			static_cast<net::ws::close_code>(reason.code),
			boost::string_view{reason.reason.data(), reason.reason.size()},
		};

		m_ws->close(std::move(cr), [this, via, h = std::move(h),
									hex = std::move(hex)](Result<> r) mutable {
			post_completion(via, std::move(hex), std::move(h), r);
		});
	});
}

void Shard::join_voice_channel_impl(
	Snowflake guild_id, Snowflake channel_id,
	asio::any_completion_handler<void(Result<>)> h, CompletionExecutor hex) {
	auto via = get_executor();
	asio::dispatch(m_strand, [this, via, guild_id, channel_id, h = std::move(h),
							  hex = std::move(hex)]() mutable {
		if (!m_ws) {
			post_completion(via, std::move(hex), std::move(h),
							Result<>{boost::system::errc::not_connected});
			return;
		}

		nlohmann::json payload{
			{"op", static_cast<uint8_t>(GatewayOpcode::VoiceStateUpdate)},
			{"d",
			 {
				 {"guild_id", guild_id},
				 {"channel_id", channel_id},
				 {"self_mute", false},
				 {"self_deaf", false},
			 }},
		};
		m_logger.debug(
			"joining voice channel | guild_id={}, channel_id={}, raw={}",
			guild_id, channel_id, payload.dump());

		m_ws->send(payload.dump(), [this, via, h = std::move(h),
									hex = std::move(hex)](Result<> r) mutable {
			post_completion(via, std::move(hex), std::move(h), r);
		});
	});
}

void Shard::leave_voice_channel_impl(
	Snowflake guild_id, asio::any_completion_handler<void(Result<>)> h,
	CompletionExecutor hex) {
	auto via = get_executor();
	asio::dispatch(m_strand, [this, via, guild_id, h = std::move(h),
							  hex = std::move(hex)]() mutable {
		if (!m_ws) {
			post_completion(via, std::move(hex), std::move(h),
							Result<>{boost::system::errc::not_connected});
			return;
		}

		nlohmann::json payload{
			{"op", static_cast<uint8_t>(GatewayOpcode::VoiceStateUpdate)},
			{"d",
			 {
				 {"guild_id", guild_id},
				 {"channel_id", nullptr},
				 {"self_mute", false},
				 {"self_deaf", false},
			 }},
		};
		m_logger.debug("leaving voice channel | guild_id={}, raw={}", guild_id,
					   payload.dump());

		m_ws->send(payload.dump(), [this, via, h = std::move(h),
									hex = std::move(hex)](Result<> r) mutable {
			post_completion(via, std::move(hex), std::move(h), r);
		});
	});
}

void Shard::update_presence_impl(UpdatePresence presence,
								 asio::any_completion_handler<void(Result<>)> h,
								 CompletionExecutor hex) {
	auto via = get_executor();
	asio::dispatch(m_strand, [this, via, presence = std::move(presence),
							  h = std::move(h),
							  hex = std::move(hex)]() mutable {
		if (!m_ws) {
			post_completion(via, std::move(hex), std::move(h),
							Result<>{boost::system::errc::not_connected});
			return;
		}

		nlohmann::json payload{
			{"op", static_cast<uint8_t>(GatewayOpcode::PresenceUpdate)},
			{"d", presence},
		};
		m_logger.debug("updating presence | raw={}", payload.dump());

		m_ws->send(payload.dump(), [this, via, h = std::move(h),
									hex = std::move(hex)](Result<> r) mutable {
			post_completion(via, std::move(hex), std::move(h), r);
		});
	});
}

void Shard::start_heartbeat(uint32_t heartbeat_interval) {
	if (!m_ws) { return; }

	m_heartbeat_interval = heartbeat_interval;
	if (!m_timer) { m_timer.emplace(m_strand); }

	m_heartbeat_running = true;
	m_logger.debug("started heartbeat timer");
	heartbeat_tick();
}

void Shard::heartbeat_tick() {
	if (!m_timer || !m_heartbeat_running) { return; }

	m_timer->expires_after(std::chrono::milliseconds(m_heartbeat_interval));
	m_timer->async_wait([this](boost::system::error_code ec) {
		asio::dispatch(m_strand, [this, ec]() {
			if (ec || !m_heartbeat_running) { return; }
			if (!m_ws) { return; }

			if (!m_last_heartbeat_acked) {
				++m_missed_heartbeats;
				m_logger.warn("Missed heartbeat ACK ({}/{})",
							  m_missed_heartbeats, kMaxMissedHeartbeats);

				if (m_missed_heartbeats >= kMaxMissedHeartbeats) {
					m_logger.error("Connection dead, closing for reconnect");
					m_heartbeat_running = false;

					CompletionExecutor hex{m_strand};
					close_impl(
						CloseFrame::SESSION_EXPIRED,
						[/*ignored*/](Result<> /*r*/) {}, std::move(hex));
					return;
				}
			} else {
				m_missed_heartbeats = 0;
			}

			m_last_heartbeat_acked = false;
			send_heartbeat_async([this](Result<> /*ignored*/) {
				asio::dispatch(m_strand, [this]() { heartbeat_tick(); });
			});
		});
	});
}

void Shard::send_heartbeat_async(
	asio::any_completion_handler<void(Result<>)> h) {
	if (!m_ws) {
		std::move(h)(boost::system::errc::not_connected);
		return;
	}

	nlohmann::json d{nullptr};
	if (m_session) { d = m_session->sequence; }

	const nlohmann::json payload{
		{"op", static_cast<uint8_t>(GatewayOpcode::Heartbeat)},
		{"d", d},
	};

	m_logger.debug("sending heartbeat | sequence={}",
				   m_session ? boost::to_string(m_session->sequence) : "null");

	m_ws->send(payload.dump(), std::move(h));
}

void Shard::send_identify_async(
	asio::any_completion_handler<void(Result<>)> h) {
	if (!m_ws) {
		std::move(h)(boost::system::errc::not_connected);
		return;
	}

	nlohmann::json d{
		{"token", m_config.token},
		{"compress", false},
		{"shard", {m_id, m_config.shard_count}},
		{"presence",
		 {
			 {"status", "online"},
			 {"since", 0},
			 {"activities", {}},
			 {"afk", false},
		 }},
	};

	if (m_config.is_bot) {
		d.merge_patch(
			{{"properties",
			  {{"$os", "Linux"}, {"$browser", "ekizu"}, {"$device", "ekizu"}}},
			 {"large_threshold", 250},	// NOLINT
			 {"intents", static_cast<uint32_t>(*m_config.intents)}});

	} else {
		d.merge_patch({
			{"client_state",
			 {
				 {"guild_hashes", {}},
				 {"highest_last_message_id", "0"},
				 {"read_state_version", 0},
				 {"user_guild_settings_version", -1},
				 {"user_settings_version", -1},
			 }},
			{"properties",
			 {{"browser_user_agent",
			   "Mozilla/5.0 (Windows NT 10.0; Win64; "
			   "x64)"
			   "AppleWebKit/537.36 (KHTML, like Gecko) "
			   "Chrome/103.0.0.0 "
			   "Safari/537.36"},
			  {"browser_version", "103.0.0.0"},
			  {"client_build_number", 137095},	// NOLINT
			  {"os", "Windows"},
			  {"device", ""},
			  {"os_version", "10"},
			  {"referrer", ""},
			  {"referrer_current", ""},
			  {"referring_domain", ""},
			  {"referring_domain_current", ""},
			  {"release_channel", "stable"},
			  {"system_locale", "en-US"}}},
		});
	}

	const nlohmann::json payload = {
		{"op", static_cast<uint8_t>(GatewayOpcode::Identify)},
		{"d", d},
	};

	m_ws->send(payload.dump(), std::move(h));
}

void Shard::send_resume_async(asio::any_completion_handler<void(Result<>)> h) {
	if (!m_ws) {
		std::move(h)(boost::system::errc::not_connected);
		return;
	}
	if (!m_session) {
		std::move(h)(boost::system::errc::operation_not_permitted);
		return;
	}

	const nlohmann::json payload = {
		{"op", static_cast<uint8_t>(GatewayOpcode::Resume)},
		{"d",
		 {
			 {"token", m_config.token},
			 {"session_id", m_session->id},
			 {"seq", m_session->sequence},
		 }},
	};

	m_logger.info("sending resume | session_id={}, sequence={}", m_session->id,
				  m_session->sequence);

	m_ws->send(payload.dump(), std::move(h));
}

void Shard::reconnect_async(asio::any_completion_handler<void(Result<>)> h) {
	// If intentional close was requested, don't reconnect
	if (m_intentional_close) {
		std::move(h)(boost::system::errc::operation_canceled);
		return;
	}

	auto t = std::make_shared<asio::steady_timer>(m_strand);

	const uint64_t backoff =
		std::min(static_cast<uint64_t>(std::pow(2, m_reconnect_attempts)),
				 uint64_t{128});
	t->expires_after(std::chrono::seconds(backoff));

	t->async_wait([this, t,
				   h = std::move(h)](boost::system::error_code ec) mutable {
		asio::dispatch(m_strand, [this, ec, h = std::move(h)]() mutable {
			if (ec) {
				std::move(h)(ec);
				return;
			}

			// Re-check intentional_close after timer fires - close may have
			// been called while we were waiting
			if (m_intentional_close) {
				std::move(h)(boost::system::errc::operation_canceled);
				return;
			}

			auto url = m_resume_gateway_url
						   ? fmt::format("{}/{}", *m_resume_gateway_url,
										 GATEWAY_JSON_ZLIB_QUERY)
						   : GATEWAY_URL;
			m_logger.debug(
				"{}connecting to {}", m_resume_gateway_url ? "re" : "", url);

			net::WebSocketClient::connect(
				m_strand.get_inner_executor(), url,
				[this, h = std::move(h)](
					Result<net::WebSocketClient> ws_res) mutable {
					asio::dispatch(m_strand, [this, ws_res = std::move(ws_res),
											  h = std::move(h)]() mutable {
						if (!ws_res) {
							m_logger.error(
								"failed to connect to {} | error={} | "
								"reconnect_attempts={}",
								m_resume_gateway_url
									? fmt::format(
										  "{}/{}", *m_resume_gateway_url,
										  GATEWAY_JSON_ZLIB_QUERY)
									: std::string(GATEWAY_URL),
								ws_res.error().message(), m_reconnect_attempts);
							++m_reconnect_attempts;
							m_resume_gateway_url.reset();
							std::move(h)(ws_res.error());
							return;
						}

						m_ws.emplace(std::move(ws_res.value()));

						if (m_config.compression) {
							auto inflater_res = Inflater::create();
							if (!inflater_res) {
								std::move(h)(inflater_res.error());
								return;
							}
							m_inflater.emplace(std::move(inflater_res.value()));
						}

						std::move(h)(outcome::success());
					});
				});
		});
	});
}

void Shard::next_event_impl(asio::any_completion_handler<void(Result<Event>)> h,
							CompletionExecutor hex) {
	auto via = get_executor();
	asio::dispatch(m_strand, [this, via, h = std::move(h),
							  hex = std::move(hex)]() mutable {
		struct Op : std::enable_shared_from_this<Op> {
			Shard *self;
			asio::any_io_executor via;
			asio::any_completion_handler<void(Result<Event>)> h;
			CompletionExecutor hex;

			explicit Op(
				Shard *s, asio::any_io_executor v,
				asio::any_completion_handler<void(Result<Event>)> handler,
				CompletionExecutor handler_ex)
				: self(s),
				  via(std::move(v)),
				  h(std::move(handler)),
				  hex(std::move(handler_ex)) {}

			void complete(Result<Event> r) {
				post_completion(
					std::move(via), std::move(hex), std::move(h), std::move(r));
			}

			void start() { ensure_connected_then_read(); }

			void ensure_connected_then_read() {
				// Strand-only.
				if (!self->m_ws || !self->m_ws->is_open()) {
					// Don't reconnect if we intentionally closed.
					if (self->m_intentional_close) {
						self->m_logger.debug(
							"connection closed intentionally, not "
							"reconnecting");
						complete(boost::system::errc::not_connected);
						return;
					}

					self->reconnect_async([me = this->shared_from_this()](
											  Result<> r) mutable {
						asio::dispatch(me->self->m_strand, [me, r]() mutable {
							if (!r) {
								me->complete(r.error());
								return;
							}
							me->read_one();
						});
					});
					return;
				}

				read_one();
			}

			void read_one() {
				// Strand-only.
				if (!self->m_ws) {
					complete(boost::system::errc::not_connected);
					return;
				}

				self->m_ws->read([me = this->shared_from_this()](
									 Result<net::WebSocketMessage> rr) mutable {
					asio::dispatch(
						me->self->m_strand, [me, rr = std::move(rr)]() mutable {
							if (!rr) {
								me->on_read_error(rr.error());
								return;
							}
							me->on_message(std::move(rr.value()));
						});
				});
			}

			void on_read_error(const boost::system::error_code &ec) {
				// Strand-only.
				// Strand-only.
				if (self->m_ws && self->m_ws->close_reason()) {
					if (self->m_ws->close_reason()->code ==
						net::ws::close_code::normal) {
						// Filter out normal close logs to debug
						self->m_logger.debug(
							"read error (expected) | ec={}, msg={}, "
							"close_reason={{code={}, reason={}}}",
							ec.value(), ec.message(),
							self->m_ws->close_reason()->code,
							self->m_ws->close_reason()->reason.data());
					} else {
						self->m_logger.error(
							"read error | ec={}, msg={}, "
							"close_reason={{code={}, reason={}}}",
							ec.value(), ec.message(),
							self->m_ws->close_reason()->code,
							self->m_ws->close_reason()->reason.data());
					}

				} else {
					self->m_logger.error(
						"read error | ec={}, msg={}", ec.value(), ec.message());
				}

				// If we intentionally closed, don't attempt to reconnect.
				if (self->m_intentional_close) {
					self->m_logger.info(
						"connection error after intentional close, "
						"not "
						"reconnecting");
					complete(ec);
					return;
				}

				// Only reconnect on specific transient errors; otherwise
				// surface it.
				if (!is_transient_read_error(ec)) {
					complete(ec);
					return;
				}

				// Retry (will reconnect if needed).
				ensure_connected_then_read();
			}

			void on_message(net::WebSocketMessage msg) {
				// Strand-only.
				if (self->m_inflater && msg.is_binary) {
					auto inflated = self->m_inflater->inflate(msg.payload);
					if (!inflated) {
						complete(inflated.error());
						return;
					}
					msg.payload = std::move(inflated.value());
				}

				handle_gateway_payload(msg.payload);
			}

			void handle_gateway_payload(std::string_view data) {
				// Strand-only.
				const auto json = nlohmann::json::parse(data, nullptr, false);
				if (json.is_discarded() || !json.contains("op") ||
					!json["op"].is_number()) {
					complete(boost::system::errc::invalid_argument);
					return;
				}

				switch (static_cast<GatewayOpcode>(json["op"].get<uint8_t>())) {
					case GatewayOpcode::Dispatch: {
						if (!json.contains("t") || !json["t"].is_string() ||
							!json.contains("d") || !json["d"].is_object()) {
							complete(boost::system::errc::invalid_argument);
							return;
						}

						std::optional<uint64_t> sequence;
						if (json.contains("s") && json["s"].is_number()) {
							sequence = json["s"];
						}

						const std::string event_type = json["t"];
						const auto &event = json["d"];

						self->m_logger.debug(
							"received dispatch {{t: {}, s: {}, d: {}}}",
							event_type,
							sequence ? boost::to_string(*sequence) : "null",
							event.dump());

						if (event_type == "READY") {
							if (!event.contains("resume_gateway_"
												"ur"
												"l") ||
								!event["resume_gateway_url"].is_string() ||
								!event.contains("session_"
												"i"
												"d") ||
								!event["session_id"].is_string()) {
								complete(boost::system::errc::invalid_argument);
								return;
							}

							self->m_session.emplace(
								event["session_id"].get<std::string>(),
								sequence.value_or(0));
							self->m_resume_gateway_url =
								event["resume_gateway_url"].get<std::string>();

							self->m_logger.info(
								"received ready | resume_gateway_url={}, "
								"session_id={}",
								*self->m_resume_gateway_url,
								self->m_session->id);
						}

						if (self->m_session && sequence) {
							// TODO: Handle out of order sequences.
							self->m_session->sequence = *sequence;
						}

						complete(event_from_str(event_type, event));
						return;
					}

					case GatewayOpcode::Heartbeat: {
						// [https://discord.com/developers/docs/topics/gateway#heartbeat-requests](https://discord.com/developers/docs/topics/gateway#heartbeat-requests)
						self->send_heartbeat_async(
							[me =
								 this->shared_from_this()](Result<> r) mutable {
								asio::dispatch(
									me->self->m_strand, [me, r]() mutable {
										if (!r) {
											me->complete(r.error());
											return;
										}
										me->complete(
											boost::system::error_code{});
									});
							});
						return;
					}

					case GatewayOpcode::Reconnect: {
						self->m_logger.info("received reconnect");
						self->close_impl(
							CloseFrame::RESUME,
							[me =
								 this->shared_from_this()](Result<> r) mutable {
								asio::dispatch(
									me->self->m_strand, [me, r]() mutable {
										if (!r) {
											me->complete(r.error());
											return;
										}
										me->complete(
											boost::system::error_code{});
									});
							},
							self->m_strand);
						return;
					}

					case GatewayOpcode::InvalidSession: {
						if (!json.contains("d") || !json["d"].is_boolean()) {
							complete(boost::system::errc::invalid_argument);
							return;
						}

						const bool resumable = json["d"];
						self->m_logger.info(
							"received invalid session | resumable={}",
							resumable);

						self->close_impl(
							resumable ? CloseFrame::RESUME : CloseFrame::NORMAL,
							[me =
								 this->shared_from_this()](Result<> r) mutable {
								asio::dispatch(
									me->self->m_strand, [me, r]() mutable {
										if (!r) {
											me->complete(r.error());
											return;
										}
										me->complete(
											boost::system::error_code{});
									});
							},
							self->m_strand);
						return;
					}

					case GatewayOpcode::Hello: {
						self->m_last_heartbeat_acked = true;

						if (!json.contains("d")) {
							complete(boost::system::errc::invalid_argument);
							return;
						}
						const auto &hello = json["d"];
						if (!hello.contains("heartbeat_"
											"interva"
											"l") ||
							!hello["heartbeat_interval"].is_number_unsigned()) {
							complete(boost::system::errc::invalid_argument);
							return;
						}

						const uint32_t heartbeat_interval =
							hello["heartbeat_interval"];
						self->m_logger.info(
							"received hello | heartbeat_interval={}",
							heartbeat_interval);

						self->start_heartbeat(heartbeat_interval);

						auto after_ident = [me = this->shared_from_this()](
											   Result<> r) mutable {
							asio::dispatch(
								me->self->m_strand, [me, r]() mutable {
									if (!r) {
										me->complete(r.error());
										return;
									}
									me->complete(boost::system::error_code{});
								});
						};

						if (self->m_session) {
							self->send_resume_async(std::move(after_ident));

						} else {
							self->send_identify_async(std::move(after_ident));
						}
						return;
					}

					case GatewayOpcode::HeartbeatAck: {
						self->m_last_heartbeat_acked = true;
						self->m_logger.debug("received heartbeat ack");
						complete(boost::system::error_code{});
						return;
					}

					default: {
						// Not a failure, but not a dispatch event either.
						complete(boost::system::error_code{});
						return;
					}
				}
			}
		};

		std::make_shared<Op>(this, std::move(via), std::move(h), std::move(hex))
			->start();
	});
}
}  // namespace ekizu
