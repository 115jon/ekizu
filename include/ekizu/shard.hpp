#ifndef EKIZU_SHARD_HPP
#define EKIZU_SHARD_HPP

#include <boost/asio/any_completion_executor.hpp>
#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/steady_timer.hpp>
#include <ekizu/event.hpp>
#include <ekizu/inflater.hpp>
#include <ekizu/intents.hpp>
#include <ekizu/log.hpp>
#include <ekizu/ws.hpp>

namespace ekizu {
namespace asio = boost::asio;
struct Client;
struct ShardAttorney;
struct ShardManager;

namespace detail {
struct CloseFrame {
	constexpr CloseFrame(std::uint16_t code_, std::string_view reason_)
		: code{code_}, reason{reason_.begin(), reason_.end()} {}

	std::uint16_t code;
	net::ws::reason_string reason;
};

struct CloseFrameConstants {
	static constexpr CloseFrame NORMAL{1000, "closing connection"};
	static constexpr CloseFrame RESUME{4000, "resuming connection"};
	static constexpr CloseFrame SESSION_EXPIRED{4009, "session expired"};
};
}  // namespace detail

struct CloseFrame : detail::CloseFrame, detail::CloseFrameConstants {
	using detail::CloseFrame::CloseFrame;
	constexpr CloseFrame(const detail::CloseFrame &base)
		: detail::CloseFrame(base) {}
};

/**
 * @brief The Discord gateway opcodes.
 */
enum class GatewayOpcode : uint8_t {
	Dispatch = 0,
	Heartbeat = 1,
	Identify = 2,
	PresenceUpdate = 3,
	VoiceStateUpdate = 4,
	Resume = 6,
	Reconnect = 7,
	RequestGuildMembers = 8,
	InvalidSession = 9,
	Hello = 10,
	HeartbeatAck = 11,
};

struct ShardId {
	/// The shard ID.
	uint64_t id;
	/// The total number of shards.
	uint64_t total;

	static const ShardId ONE;
};

inline const ShardId ShardId::ONE{0, 1};

struct UpdatePresence {
	std::optional<uint64_t> idle_since;
	std::vector<Activity> activities;
	Status status{};
	bool afk{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const UpdatePresence &p);

/**
 * @brief Represents a Discord shard, which is a connection to the Discord
 * gateway.
 */
struct Shard {
	using CompletionExecutor = asio::any_completion_executor;

	struct Config {
		std::string token;
		std::optional<Intents> intents;
		uint64_t shard_count{1};
		bool compression{true};
		bool is_bot{true};
	};

	EKIZU_EXPORT Shard(asio::any_io_executor executor, ShardId id,
					   std::string_view token, Intents intents);

	[[nodiscard]] uint64_t id() const { return m_id; }

	// Keep this returning the strand executor so default handler dispatch stays
	// serialized with internal shard state.
	[[nodiscard]] asio::any_io_executor get_executor() const {
		return m_strand;
	}

	EKIZU_EXPORT void attach_logger(std::function<void(Log)> on_log);

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto close(CloseFrame reason, CompletionToken &&token) {
		auto ex0 = get_executor();
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, ex0, reason](auto &&handler) mutable {
				auto hex = asio::get_associated_executor(handler, ex0);
				close_impl(reason,
						   asio::any_completion_handler<void(Result<>)>{
							   std::forward<decltype(handler)>(handler)},
						   std::move(hex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto join_voice_channel(Snowflake guild_id, Snowflake channel_id,
							CompletionToken &&token) {
		auto ex0 = get_executor();
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, ex0, guild_id, channel_id](auto &&handler) mutable {
				auto hex = asio::get_associated_executor(handler, ex0);
				join_voice_channel_impl(
					guild_id, channel_id,
					asio::any_completion_handler<void(Result<>)>{
						std::forward<decltype(handler)>(handler)},
					std::move(hex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto leave_voice_channel(Snowflake guild_id, CompletionToken &&token) {
		auto ex0 = get_executor();
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, ex0, guild_id](auto &&handler) mutable {
				auto hex = asio::get_associated_executor(handler, ex0);
				leave_voice_channel_impl(
					guild_id,
					asio::any_completion_handler<void(Result<>)>{
						std::forward<decltype(handler)>(handler)},
					std::move(hex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<Event>))
				  CompletionToken>
	auto next_event(CompletionToken &&token) {
		auto ex0 = get_executor();
		return asio::async_initiate<CompletionToken, void(Result<Event>)>(
			[this, ex0](auto &&handler) mutable {
				auto hex = asio::get_associated_executor(handler, ex0);
				next_event_impl(
					asio::any_completion_handler<void(Result<Event>)>{
						std::forward<decltype(handler)>(handler)},
					std::move(hex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto update_presence(UpdatePresence presence, CompletionToken &&token) {
		auto ex0 = get_executor();
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, ex0,
			 presence = std::move(presence)](auto &&handler) mutable {
				auto hex = asio::get_associated_executor(handler, ex0);
				update_presence_impl(
					std::move(presence),
					asio::any_completion_handler<void(Result<>)>{
						std::forward<decltype(handler)>(handler)},
					std::move(hex));
			},
			token);
	}

   private:
	struct Session {
		Session(std::string_view id_, uint64_t sequence_)
			: id{id_}, sequence{sequence_} {}

		std::string id;
		uint64_t sequence{};
	};

	EKIZU_EXPORT void close_impl(CloseFrame reason,
								 asio::any_completion_handler<void(Result<>)> h,
								 CompletionExecutor hex);

	EKIZU_EXPORT void join_voice_channel_impl(
		Snowflake guild_id, Snowflake channel_id,
		asio::any_completion_handler<void(Result<>)> h, CompletionExecutor hex);

	EKIZU_EXPORT void leave_voice_channel_impl(
		Snowflake guild_id, asio::any_completion_handler<void(Result<>)> h,
		CompletionExecutor hex);

	EKIZU_EXPORT void next_event_impl(
		asio::any_completion_handler<void(Result<Event>)> h,
		CompletionExecutor hex);

	EKIZU_EXPORT void update_presence_impl(
		UpdatePresence presence, asio::any_completion_handler<void(Result<>)> h,
		CompletionExecutor hex);

	// Internal helpers (strand-only).
	void start_heartbeat(uint32_t heartbeat_interval);
	void heartbeat_tick();
	void send_heartbeat_async(asio::any_completion_handler<void(Result<>)> h);
	void send_identify_async(asio::any_completion_handler<void(Result<>)> h);
	void send_resume_async(asio::any_completion_handler<void(Result<>)> h);
	void reconnect_async(asio::any_completion_handler<void(Result<>)> h);

	void log(std::string_view msg, LogLevel level = LogLevel::Debug) const;

	asio::strand<asio::any_io_executor> m_strand;
	std::optional<asio::steady_timer> m_timer;
	uint64_t m_id;
	Config m_config;
	bool m_last_heartbeat_acked{true};
	uint32_t m_heartbeat_interval{};
	std::function<void(Log)> m_on_log;
	/// May or may not be used based on runtime options.
	std::optional<Inflater> m_inflater;
	std::optional<Session> m_session;
	std::optional<std::string> m_resume_gateway_url;
	std::optional<net::WebSocketClient> m_ws;
	uint64_t m_reconnect_attempts{};
	bool m_heartbeat_running{false};
	bool m_intentional_close{false};
	int m_missed_heartbeats{0};
	static constexpr int kMaxMissedHeartbeats = 3;
};

}  // namespace ekizu

#endif	// EKIZU_SHARD_HPP
