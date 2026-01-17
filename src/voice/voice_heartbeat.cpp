#include <ekizu/json_util.hpp>
#include <ekizu/voice_connection.hpp>

#include "voice_connection_impl.hpp"

namespace ekizu {

void VoiceConnection::Impl::setup_heartbeat(const nlohmann::json &data) {
	if (m_heartbeat_timer) { return; }
	m_heartbeat_interval_ms = data["d"]["heartbeat_interval"].get<uint32_t>();
	m_heartbeat_timer.emplace(
		m_strand, std::chrono::milliseconds(m_heartbeat_interval_ms));
	m_heartbeat_running = true;
	heartbeat_tick();
}

void VoiceConnection::Impl::heartbeat_tick() {
	auto self = shared_from_this();
	if (!m_heartbeat_running || !m_heartbeat_timer) { return; }

	m_heartbeat_timer->async_wait([self](boost::system::error_code ec) mutable {
		if (ec || !self->m_heartbeat_running) { return; }

		if (!self->m_last_heartbeat_acked) {
			++self->m_missed_heartbeats;
			self->log(
				fmt::format("Missed heartbeat ACK ({}/{})",
							self->m_missed_heartbeats, kMaxMissedHeartbeats),
				LogLevel::Warn);

			if (self->m_missed_heartbeats >= kMaxMissedHeartbeats) {
				self->log(
					"Connection dead, initiating reconnect", LogLevel::Error);
				self->initiate_reconnect();
				return;
			}
		} else {
			self->m_missed_heartbeats = 0;
		}
		self->m_last_heartbeat_acked = false;

		self->send_heartbeat([self](Result<> /*ignored*/) mutable {
			if (!self->m_heartbeat_running || !self->m_heartbeat_timer) {
				return;
			}
			self->m_heartbeat_timer->expires_after(
				std::chrono::milliseconds(self->m_heartbeat_interval_ms));
			self->heartbeat_tick();
		});
	});
}

void VoiceConnection::Impl::send_heartbeat(
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, h = std::move(h)]() mutable {
		if (!self->m_ws) {
			std::move(h)(outcome::success());
			return;
		}

		const auto now_ms =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch())
				.count();

		nlohmann::json payload{
			{"op", static_cast<uint8_t>(VoiceOpcode::Heartbeat)},
			{"d", {{"t", now_ms}, {"seq_ack", self->m_last_seq}}}};

		self->m_ws->send(payload.dump(), std::move(h));
	});
}

}  // namespace ekizu