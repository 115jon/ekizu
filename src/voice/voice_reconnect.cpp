#include <ekizu/voice_connection.hpp>

#include "voice_connection_impl.hpp"

namespace ekizu {

void VoiceConnection::Impl::initiate_reconnect() {
	// Don't reconnect if we've been intentionally disconnected
	if (m_disconnected) {
		m_logger.info(
			"Skipping reconnect - connection was intentionally closed");
		return;
	}

	auto self = shared_from_this();

	// Stop current heartbeat
	m_heartbeat_running = false;
	if (m_heartbeat_timer) { m_heartbeat_timer->cancel(); }

	// Determine if we can resume based on current state
	bool can_resume = (m_connection_state == VoiceConnectionState::Ready);

	if (can_resume) {
		m_logger.info("Initiating Resume reconnect");
		m_connection_state = VoiceConnectionState::Resuming;
	} else {
		m_logger.info("Initiating fresh reconnect");
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

			// Re-check disconnected flag - close may have been called while
			// waiting
			if (self->m_disconnected) {
				self->m_logger.info(
					"Skipping reconnect - connection closed during wait");
				return;
			}

			self->connect_ws_async([self](Result<> r) {
				if (!r) {
					self->m_logger.error(
						"Reconnect failed: {}", r.error().message());
					self->m_connection_state = VoiceConnectionState::Closed;
					self->m_disconnected = true;
					return;
				}

				self->m_logger.info("Reconnected successfully");
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
