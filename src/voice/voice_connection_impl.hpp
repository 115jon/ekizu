#ifndef VOICE_CONNECTION_IMPL_HPP
#define VOICE_CONNECTION_IMPL_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/endian/conversion.hpp>
#include <cstdint>
#include <ekizu/dave_manager.hpp>
#include <ekizu/opus_codec.hpp>
#include <ekizu/udp.hpp>
#include <ekizu/voice_connection.hpp>
#include <ekizu/ws.hpp>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "voice_crypto.hpp"

namespace ekizu {

struct AudioPacket {
	std::vector<std::byte> encoded;
	size_t frame_count{};
};

enum class VoiceConnectionState : uint8_t {
	Disconnected,
	Connecting,
	Identifying,
	Ready,
	Resuming,
	Closed
};

constexpr int kMaxMissedHeartbeats = 3;

// Discord Voice Close Event Codes (per Discord API docs)
enum class VoiceCloseCode : uint16_t {
	UnknownOpcode = 4001,
	FailedToDecodePayload = 4002,
	NotAuthenticated = 4003,
	AuthenticationFailed = 4004,
	AlreadyAuthenticated = 4005,
	SessionNoLongerValid = 4006,
	SessionTimeout = 4009,
	ServerNotFound = 4011,
	UnknownProtocol = 4012,
	Disconnected = 4014,
	VoiceServerCrashed = 4015,
	UnknownEncryptionMode = 4016
};

// Check if a close code allows resuming the session
inline bool should_resume(VoiceCloseCode code) {
	switch (code) {
		case VoiceCloseCode::VoiceServerCrashed:
			return true;		// Try resuming per Discord docs
		default: return false;	// Cannot resume, need new session
	}
}

// Check if a raw close code is resumable
inline bool is_resumable_close_code(uint16_t code) {
	// Standard WebSocket close codes (<4000) are generally resumable
	if (code < 4000) { return true; }

	// Check known Discord voice close codes
	if (code >= 4001 && code <= 4016) {
		return should_resume(static_cast<VoiceCloseCode>(code));
	}

	// Unknown codes - attempt resume
	return true;
}

// Voice connection implementation
struct VoiceConnection::Impl : std::enable_shared_from_this<Impl> {
	Impl(asio::any_io_executor executor, net::WebSocketClient ws,
		 VoiceState state, std::string url, std::string token,
		 std::unique_ptr<Codec> codec);

	asio::any_io_executor get_executor() const { return m_strand; }

	Result<> connect_dave(int protocol_version, std::string const &endpoint);
	std::optional<
		asio::experimental::channel<void(boost::system::error_code, Packet)>> &
	recv_chan() {
		return m_recv_chan;
	}

	void attach_logger(std::function<void(const Log &)> on_log);
	void request_stop();

	// Public API methods
	void close(asio::any_completion_handler<void(Result<>)> h);
	void reconnect(asio::any_completion_handler<void(Result<>)> h);
	void run(asio::any_completion_handler<void(Result<>)> h);
	void send_opus(std::vector<std::byte> data,
				   asio::any_completion_handler<void(Result<>)> h);
	void send_raw(std::vector<int16_t> data,
				  asio::any_completion_handler<void(Result<>)> h);
	void silence(asio::any_completion_handler<void(Result<>)> h);
	void speak(SpeakerFlag flags,
			   asio::any_completion_handler<void(Result<>)> h);
	void flush(asio::any_completion_handler<void(Result<>)> h);

   private:
	// Logging
	void log(std::string_view msg, LogLevel level = LogLevel::Debug) const;

	// Core components (from voice_websocket.cpp)
	void ws_listen_loop();

	// Heartbeat (from voice_heartbeat.cpp)
	void setup_heartbeat(const nlohmann::json &data);
	void heartbeat_tick();
	void send_heartbeat(asio::any_completion_handler<void(Result<>)> h);

	// UDP setup (from voice_udp_setup.cpp)
	void setup_udp_async(const nlohmann::json &data,
						 asio::any_completion_handler<void(Result<>)> h);
	void handle_session_description_async(
		const nlohmann::json &data,
		asio::any_completion_handler<void(Result<>)> h);

	// Audio transmission (from voice_sender.cpp)
	void opus_sender_loop();

	// Audio reception (from voice_receiver.cpp)
	void udp_receiver_loop();
	void on_speaking(std::string user_id, uint32_t ssrc, bool speaking);

	// Reconnection (from voice_reconnect.cpp)
	void initiate_reconnect();
	void connect_ws_async(asio::any_completion_handler<void(Result<>)> h);

	// DAVE/MLS handling (from voice_dave_handler.cpp)
	void handle_binary_event_async(
		VoiceOpcode op, std::vector<std::byte> payload, uint16_t seq,
		asio::any_completion_handler<void(Result<>)> h);
	void send_voice_json(const nlohmann::json &j,
						 asio::any_completion_handler<void(Result<>)> h);
	void send_voice_binary(VoiceOpcode opcode,
						   boost::span<const std::byte> payload,
						   asio::any_completion_handler<void(Result<>)> h);
	void maybe_start_mls_async(bool force_reset,
							   asio::any_completion_handler<void(Result<>)> h);
	void execute_dave_transition_now_async(
		int protocol_version, asio::any_completion_handler<void(Result<>)> h);
	void recover_mls_after_invalid_transition_async(
		uint16_t transition_id, asio::any_completion_handler<void(Result<>)> h);
	void send_mls_key_package(asio::any_completion_handler<void(Result<>)> h);
	void send_mls_commit_welcome(
		std::vector<std::byte> payload,
		asio::any_completion_handler<void(Result<>)> h);
	void send_mls_invalid_commit_welcome(
		uint16_t transition_id, asio::any_completion_handler<void(Result<>)> h);

	uint64_t compute_group_id() const;

	// Core executor/networking
	asio::strand<asio::any_io_executor> m_strand;
	std::optional<net::WebSocketClient> m_ws;

	// Channels (depend on strand)
	std::optional<asio::experimental::channel<void(
		boost::system::error_code, AudioPacket)>>
		m_channel;
	std::optional<asio::experimental::channel<void(
		boost::system::error_code, boost::blank)>>
		m_ready_chan;
	std::optional<
		asio::experimental::channel<void(boost::system::error_code, Packet)>>
		m_recv_chan;

	// Timers (depend on strand)
	std::optional<asio::steady_timer> m_heartbeat_timer;
	std::optional<asio::steady_timer> m_send_timer;

	// UDP (networking)
	std::optional<net::UdpSocket> m_udp;

	// Transport encryption
	VoiceCrypto m_crypto;

	// DAVE/MLS state (uses networking, destroyed before ws/strand)
	std::shared_ptr<DaveManager> m_dave_manager;
	std::string m_media_session_id;
	std::set<std::string> m_recognized_user_ids;
	boost::container::flat_map<uint16_t, int> m_dave_transition_versions;

	// Codec
	std::unique_ptr<Codec> m_codec;

	// State (no dependencies, destroyed first)
	VoiceState m_state;
	std::string m_url;
	std::string m_token;
	uint32_t m_heartbeat_interval_ms{0};
	bool m_last_heartbeat_acked{true};
	bool m_heartbeat_running{false};
	bool m_warned_waiting_for_e2ee{false};
	int64_t m_last_seq{0};
	std::atomic<size_t> m_pending_frames{0};
	uint32_t m_ssrc{};
	VoiceConnectionState m_connection_state{VoiceConnectionState::Disconnected};
	bool m_disconnected{false};
	bool m_speaking{false};
	std::function<void(const Log &)> m_on_log;
	int m_missed_heartbeats{0};
	std::atomic<uint64_t> m_packet_count{0};
	uint16_t m_rtp_sequence{0};
	uint32_t m_rtp_timestamp{0};

	// Receiver maps (simple containers, destroyed first)
	std::unordered_map<uint32_t, std::string> m_ssrc_to_user_id;
	std::unordered_map<std::string, uint32_t> m_user_id_to_ssrc;
	bool m_receiver_running{false};
};

}  // namespace ekizu

#endif	// VOICE_CONNECTION_IMPL_HPP
