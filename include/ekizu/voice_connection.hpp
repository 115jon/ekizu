#ifndef EKIZU_VOICE_CONNECTION_HPP
#define EKIZU_VOICE_CONNECTION_HPP

#include <array>
#include <boost/asio/any_completion_executor.hpp>
#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/post.hpp>
#include <boost/core/span.hpp>
#include <boost/system/error_code.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ekizu/log.hpp"
#include "ekizu/result.hpp"
#include "ekizu/voice_state.hpp"
#include "snowflake.hpp"

namespace ekizu {

namespace asio = boost::asio;

/// An RTP packet the server send to the client containing the voice data.
struct Packet {
	std::array<std::byte, 2> type{};
	uint16_t sequence{};
	uint32_t timestamp{};
	uint32_t ssrc{};
	std::vector<std::byte> opus;
	Snowflake user_id;
};

enum class SpeakerFlag : uint8_t {
	None = 0,
	Microphone = 1 << 0,
	Soundshared = 1 << 1,
	Priority = 1 << 2
};

enum class VoiceOpcode : uint8_t {
	Identify = 0,
	SelectProtocol = 1,
	Ready = 2,
	Heartbeat = 3,
	SessionDescription = 4,
	Speaking = 5,
	HeartbeatAck = 6,
	Resume = 7,
	Hello = 8,
	Resumed = 9,
	ClientsConnect = 11,
	ClientDisconnect = 13,
	DavePrepareTransition = 21,
	DaveExecuteTransition = 22,
	DaveTransitionReady = 23,
	DavePrepareEpoch = 24,
	DaveMlsExternalSender = 25,
	DaveMlsKeyPackage = 26,
	DaveMlsProposals = 27,
	DaveMlsCommitWelcome = 28,
	DaveMlsAnnounceCommitTransition = 29,
	DaveMlsWelcome = 30,
	DaveMlsInvalidCommitWelcome = 31,
};

struct VoiceConnectionConfig;

struct VoiceConnection {
	using CompletionExecutor = asio::any_completion_executor;

	VoiceConnection(const VoiceConnection &) = delete;
	VoiceConnection &operator=(const VoiceConnection &) = delete;

	EKIZU_EXPORT VoiceConnection(VoiceConnection &&) noexcept;
	EKIZU_EXPORT VoiceConnection &operator=(VoiceConnection &&) noexcept;
	EKIZU_EXPORT ~VoiceConnection();

	EKIZU_EXPORT std::optional<
		asio::experimental::channel<void(boost::system::error_code, Packet)>> &
	recv_chan();

	// Convenience wrapper: lets users "just receive packets" without touching
	// the channel.
	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
												   Packet)) CompletionToken>
	auto receive_packet(CompletionToken &&token) {
		auto ex = get_executor();
		return asio::async_initiate<CompletionToken,
									void(boost::system::error_code, Packet)>(
			[this, ex](auto &&handler) mutable {
				auto hex = asio::get_associated_executor(handler, ex);

				auto &ch = recv_chan();
				if (!ch) {
					asio::post(hex, [h = std::forward<decltype(handler)>(
										 handler)]() mutable {
						h(make_error_code(
							  boost::system::errc::operation_not_supported),
						  Packet{});
					});
					return;
				}

				ch->async_receive(std::forward<decltype(handler)>(handler));
			},
			token);
	}

	EKIZU_EXPORT void attach_logger(std::function<void(const Log &)> on_log);

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto close(CompletionToken &&token) {
		auto ex = get_executor();
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, ex](auto &&handler) mutable {
				close_impl(
					asio::any_completion_handler<void(Result<>)>{
						std::forward<decltype(handler)>(handler)},
					asio::get_associated_executor(handler, ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto reconnect(CompletionToken &&token) {
		auto ex = get_executor();
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, ex](auto &&handler) mutable {
				reconnect_impl(
					asio::any_completion_handler<void(Result<>)>{
						std::forward<decltype(handler)>(handler)},
					asio::get_associated_executor(handler, ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto run(CompletionToken &&token) {
		auto ex = get_executor();
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, ex](auto &&handler) mutable {
				run_impl(
					asio::any_completion_handler<void(Result<>)>{
						std::forward<decltype(handler)>(handler)},
					asio::get_associated_executor(handler, ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto send_opus(boost::span<const std::byte> data, CompletionToken &&token) {
		auto ex = get_executor();
		std::vector<std::byte> copy(data.begin(), data.end());
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, ex, copy = std::move(copy)](auto &&handler) mutable {
				send_opus_impl(std::move(copy),
							   asio::any_completion_handler<void(Result<>)>{
								   std::forward<decltype(handler)>(handler)},
							   asio::get_associated_executor(handler, ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto send_raw(boost::span<const int16_t> data, CompletionToken &&token) {
		auto ex = get_executor();
		std::vector<int16_t> copy(data.begin(), data.end());
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, ex, copy = std::move(copy)](auto &&handler) mutable {
				send_raw_impl(std::move(copy),
							  asio::any_completion_handler<void(Result<>)>{
								  std::forward<decltype(handler)>(handler)},
							  asio::get_associated_executor(handler, ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto silence(CompletionToken &&token) {
		auto ex = get_executor();
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, ex](auto &&handler) mutable {
				silence_impl(
					asio::any_completion_handler<void(Result<>)>{
						std::forward<decltype(handler)>(handler)},
					asio::get_associated_executor(handler, ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto speak(SpeakerFlag flags, CompletionToken &&token) {
		auto ex = get_executor();
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, ex, flags](auto &&handler) mutable {
				speak_impl(flags,
						   asio::any_completion_handler<void(Result<>)>{
							   std::forward<decltype(handler)>(handler)},
						   asio::get_associated_executor(handler, ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto flush(CompletionToken &&token) {
		auto ex = get_executor();
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, ex](auto &&handler) mutable {
				flush_impl(
					asio::any_completion_handler<void(Result<>)>{
						std::forward<decltype(handler)>(handler)},
					asio::get_associated_executor(handler, ex));
			},
			token);
	}

	EKIZU_EXPORT void request_stop();

   private:
	friend struct VoiceConnectionConfig;

	struct Impl;
	EKIZU_EXPORT explicit VoiceConnection(std::shared_ptr<Impl> impl);

	EKIZU_EXPORT void close_impl(asio::any_completion_handler<void(Result<>)> h,
								 CompletionExecutor hex);
	EKIZU_EXPORT void reconnect_impl(
		asio::any_completion_handler<void(Result<>)> h, CompletionExecutor hex);
	EKIZU_EXPORT void run_impl(asio::any_completion_handler<void(Result<>)> h,
							   CompletionExecutor hex);
	EKIZU_EXPORT void send_opus_impl(
		std::vector<std::byte> data,
		asio::any_completion_handler<void(Result<>)> h, CompletionExecutor hex);
	EKIZU_EXPORT void send_raw_impl(
		std::vector<int16_t> data,
		asio::any_completion_handler<void(Result<>)> h, CompletionExecutor hex);
	EKIZU_EXPORT void silence_impl(
		asio::any_completion_handler<void(Result<>)> h, CompletionExecutor hex);
	EKIZU_EXPORT void speak_impl(SpeakerFlag flags,
								 asio::any_completion_handler<void(Result<>)> h,
								 CompletionExecutor hex);
	EKIZU_EXPORT void flush_impl(asio::any_completion_handler<void(Result<>)> h,
								 CompletionExecutor hex);

	[[nodiscard]] EKIZU_EXPORT asio::any_io_executor get_executor() const;

	std::shared_ptr<Impl> m_impl;
};

struct VoiceConnectionConfig {
	using CompletionExecutor = asio::any_completion_executor;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<VoiceConnection>))
				  CompletionToken>
	[[nodiscard]] auto connect(asio::any_io_executor executor,
							   CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken,
									void(Result<VoiceConnection>)>(
			[this, executor](auto &&handler) mutable {
				connect_impl(
					executor,
					asio::any_completion_handler<void(Result<VoiceConnection>)>{
						std::forward<decltype(handler)>(handler)},
					asio::get_associated_executor(handler, executor));
			},
			token);
	}

	std::optional<VoiceState> state;
	std::optional<std::string> endpoint;
	std::optional<std::string> token;

   private:
	EKIZU_EXPORT void connect_impl(
		asio::any_io_executor executor,
		asio::any_completion_handler<void(Result<VoiceConnection>)> h,
		CompletionExecutor hex) const;
};

}  // namespace ekizu

#endif	// EKIZU_VOICE_CONNECTION_HPP
