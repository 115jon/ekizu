#ifndef EKIZU_SHARD_HPP
#define EKIZU_SHARD_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/strand.hpp>
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
	std::optional<uint64_t> idle_since{};
	std::vector<Activity> activities{};
	Status status{};
	bool afk{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const UpdatePresence &p);

/**
 * @brief Represents a Discord shard, which is a connection to the Discord
 * gateway.
 */
struct Shard {
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
	[[nodiscard]] asio::any_io_executor get_executor() const {
		return m_strand.get_inner_executor();
	}

	EKIZU_EXPORT void attach_logger(std::function<void(Log)> on_log);

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto close(CloseFrame reason, CompletionToken &&token);

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto join_voice_channel(Snowflake guild_id, Snowflake channel_id,
							CompletionToken &&token);

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto leave_voice_channel(Snowflake guild_id, CompletionToken &&token);

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<Event>))
				  CompletionToken>
	auto next_event(CompletionToken &&token);

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto update_presence(UpdatePresence presence, CompletionToken &&token);

   private:
	struct Session {
		Session(std::string_view id_, uint64_t sequence_)
			: id{id_}, sequence{sequence_} {}

		std::string id;
		uint64_t sequence{};
	};

	Result<Event> handle_event(std::string_view data,
							   const boost::asio::yield_context &yield);
	Result<Event> handle_dispatch(const nlohmann::json &data);
	Result<> handle_reconnect(const boost::asio::yield_context &yield);
	Result<> handle_invalid_session(const nlohmann::json &data,
									const boost::asio::yield_context &yield);
	Result<> handle_hello(const nlohmann::json &data,
						  const boost::asio::yield_context &yield);
	void handle_heartbeat_ack();
	void log(std::string_view msg, LogLevel level = LogLevel::Debug) const;

	Result<net::WebSocketMessage> next_message(
		const boost::asio::yield_context &yield);
	Result<> reconnect(const boost::asio::yield_context &yield);
	Result<> start_heartbeat(uint32_t heartbeat_interval);
	Result<> send_heartbeat(const boost::asio::yield_context &yield);
	Result<> send_identify(const boost::asio::yield_context &yield);
	Result<> send_resume(const boost::asio::yield_context &yield);

	// Implementation methods (internal, use yield_context)
	Result<> close_impl(CloseFrame reason,
						const boost::asio::yield_context &yield);
	Result<> join_voice_channel_impl(Snowflake guild_id, Snowflake channel_id,
									 const boost::asio::yield_context &yield);
	Result<> leave_voice_channel_impl(Snowflake guild_id,
									  const boost::asio::yield_context &yield);
	Result<Event> next_event_impl(const boost::asio::yield_context &yield);
	Result<> update_presence_impl(UpdatePresence presence,
								  const boost::asio::yield_context &yield);

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
};

// Template implementations

template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
auto Shard::close(CloseFrame reason, CompletionToken &&token) {
	return asio::async_initiate<CompletionToken, void(Result<>)>(
		[this, reason](auto &&handler) {
			asio::spawn(
				m_strand,
				[this, reason, h = std::forward<decltype(handler)>(handler)](
					auto yield) mutable {
					auto result = close_impl(reason, yield);
					auto ex = asio::get_associated_executor(
						h, m_strand.get_inner_executor());
					asio::post(ex, [h = std::move(h), result]() mutable {
						std::move(h)(result);
					});
				},
				asio::detached);
		},
		token);
}

template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
auto Shard::join_voice_channel(Snowflake guild_id, Snowflake channel_id,
							   CompletionToken &&token) {
	return asio::async_initiate<CompletionToken, void(Result<>)>(
		[this, guild_id, channel_id](auto &&handler) {
			asio::spawn(
				m_strand,
				[this, guild_id, channel_id,
				 h = std::forward<decltype(handler)>(handler)](
					auto yield) mutable {
					auto result =
						join_voice_channel_impl(guild_id, channel_id, yield);
					auto ex = asio::get_associated_executor(
						h, m_strand.get_inner_executor());
					asio::post(ex, [h = std::move(h), result]() mutable {
						std::move(h)(result);
					});
				},
				asio::detached);
		},
		token);
}

template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
auto Shard::leave_voice_channel(Snowflake guild_id, CompletionToken &&token) {
	return asio::async_initiate<CompletionToken, void(Result<>)>(
		[this, guild_id](auto &&handler) {
			asio::spawn(
				m_strand,
				[this, guild_id, h = std::forward<decltype(handler)>(handler)](
					auto yield) mutable {
					auto result = leave_voice_channel_impl(guild_id, yield);
					auto ex = asio::get_associated_executor(
						h, m_strand.get_inner_executor());
					asio::post(ex, [h = std::move(h), result]() mutable {
						std::move(h)(result);
					});
				},
				asio::detached);
		},
		token);
}

template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<Event>)) CompletionToken>
auto Shard::next_event(CompletionToken &&token) {
	return asio::async_initiate<CompletionToken, void(Result<Event>)>(
		[this](auto &&handler) {
			asio::spawn(
				m_strand,
				[this, h = std::forward<decltype(handler)>(handler)](
					auto yield) mutable {
					auto result = next_event_impl(yield);
					auto ex = asio::get_associated_executor(
						h, m_strand.get_inner_executor());
					asio::post(ex, [h = std::move(h),
									result = std::move(result)]() mutable {
						std::move(h)(std::move(result));
					});
				},
				asio::detached);
		},
		token);
}

template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
auto Shard::update_presence(UpdatePresence presence, CompletionToken &&token) {
	return asio::async_initiate<CompletionToken, void(Result<>)>(
		[this, presence = std::move(presence)](auto &&handler) mutable {
			asio::spawn(
				m_strand,
				[this, presence = std::move(presence),
				 h = std::forward<decltype(handler)>(handler)](
					auto yield) mutable {
					auto result =
						update_presence_impl(std::move(presence), yield);
					auto ex = asio::get_associated_executor(
						h, m_strand.get_inner_executor());
					asio::post(ex, [h = std::move(h), result]() mutable {
						std::move(h)(result);
					});
				},
				asio::detached);
		},
		token);
}

}  // namespace ekizu

#endif	// EKIZU_SHARD_HPP
