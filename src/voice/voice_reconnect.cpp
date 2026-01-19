#include <ekizu/voice_connection.hpp>

#include "voice_connection_impl.hpp"

namespace ekizu {

void VoiceConnection::Impl::initiate_reconnect() {
	// Don't reconnect if we've been intentionally disconnected
	if (m_disconnected) {
		log("Skipping reconnect - connection was intentionally closed",
			LogLevel::Info);
		return;
	}

	auto self = shared_from_this();

	// Stop current heartbeat
	m_heartbeat_running = false;
	if (m_heartbeat_timer) { m_heartbeat_timer->cancel(); }

	// Determine if we can resume based on current state
	bool can_resume = (m_connection_state == VoiceConnectionState::Ready);

	if (can_resume) {
		log("Initiating Resume reconnect", LogLevel::Info);
		m_connection_state = VoiceConnectionState::Resuming;
	} else {
		log("Initiating fresh reconnect", LogLevel::Info);
		m_connection_state = VoiceConnectionState::Connecting;
	}

	// Close current WebSocket if open
	if (m_ws) {
		m_ws->cancel();
		m_ws.reset();
	}

	// Reconnect after brief delay (backoff)
	auto reconnect_timer =
		std::make_shared<asio::steady_timer>(m_strand, std::chrono::seconds(1));

	reconnect_timer->async_wait(
		[self, reconnect_timer](boost::system::error_code ec) mutable {
			if (ec) { return; }

			self->connect_ws_async([self](Result<> r) {
				if (!r) {
					self->log(fmt::format(
								  "Reconnect failed: {}", r.error().message()),
							  LogLevel::Error);
					self->m_connection_state = VoiceConnectionState::Closed;
					self->m_disconnected = true;
					return;
				}

				self->log("Reconnected successfully", LogLevel::Info);
			});
		});
}

void VoiceConnection::Impl::connect_ws_async(
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();

	net::WebSocketClient::connect(
		m_strand.get_inner_executor(), m_url,
		[self, h = std::move(h)](Result<net::WebSocketClient> ws_res) mutable {
			if (!ws_res) {
				std::move(h)(ws_res.error());
				return;
			}

			self->m_ws.emplace(std::move(ws_res.value()));

			// Start listening for messages
			self->ws_listen_loop();

			// Reset heartbeat state for new connection
			self->m_last_heartbeat_acked = true;
			self->m_missed_heartbeats = 0;

			std::move(h)(outcome::success());
		});
}

}  // namespace ekizu
