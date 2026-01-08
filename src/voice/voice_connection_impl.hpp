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

// Audio packet structure for internal transmission
struct AudioPacket {
	std::vector<std::byte> encoded;
	size_t frame_count{};
};

// Voice connection implementation
struct VoiceConnection::Impl : std::enable_shared_from_this<Impl> {
	Impl(asio::any_io_executor executor, net::WebSocketClient ws,
		 VoiceState state, std::string url, std::string token,
		 std::unique_ptr<Codec> codec);

	asio::any_io_executor get_executor() const { return m_strand; }

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

	// Core state
	asio::strand<asio::any_io_executor> m_strand;
	std::optional<net::WebSocketClient> m_ws;
	VoiceState m_state;
	std::string m_url;
	std::string m_token;

	// Channels
	std::optional<asio::experimental::channel<void(
		boost::system::error_code, AudioPacket)>>
		m_channel;
	std::optional<asio::experimental::channel<void(
		boost::system::error_code, boost::blank)>>
		m_ready_chan;
	std::optional<
		asio::experimental::channel<void(boost::system::error_code, Packet)>>
		m_recv_chan;

	// Heartbeat state
	std::optional<asio::steady_timer> m_heartbeat_timer;
	uint32_t m_heartbeat_interval_ms{0};
	bool m_last_heartbeat_acked{true};
	bool m_heartbeat_running{false};

	// Voice state
	int64_t m_last_seq{0};
	std::atomic<size_t> m_pending_frames{0};
	uint32_t m_ssrc{};
	std::optional<net::UdpSocket> m_udp;

	// Transport encryption
	VoiceCrypto m_crypto;

	// DAVE/MLS state
	std::shared_ptr<DaveManager> m_dave_manager;
	std::string m_media_session_id;
	std::set<std::string> m_recognized_user_ids;
	boost::container::flat_map<uint16_t, int> m_dave_transition_versions;
	bool m_warned_waiting_for_e2ee{false};

	// Codec
	std::unique_ptr<Codec> m_codec;

	// Connection state
	bool m_disconnected{false};
	bool m_speaking{false};
	std::function<void(const Log &)> m_on_log;

	// Sender state
	std::optional<asio::steady_timer> m_send_timer;
	std::atomic<uint64_t> m_packet_count{0};
	uint16_t m_rtp_sequence{0};
	uint32_t m_rtp_timestamp{0};

	// Receiver state (NEW)
	std::unordered_map<uint32_t, std::string> m_ssrc_to_user_id;
	std::unordered_map<std::string, uint32_t> m_user_id_to_ssrc;
	bool m_receiver_running{false};
};

}  // namespace ekizu

#endif	// VOICE_CONNECTION_IMPL_HPP
