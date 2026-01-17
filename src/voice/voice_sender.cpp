#include <boost/asio/error.hpp>
#include <ekizu/voice_connection.hpp>

#include "voice_connection_impl.hpp"
#include "voice_util.hpp"


namespace ekizu {

void VoiceConnection::Impl::send_opus(
	std::vector<std::byte> data,
	asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(m_strand, [self, data = std::move(data),
							  h = std::move(h)]() mutable {
		if (data.empty()) {
			std::move(h)(boost::system::errc::invalid_argument);
			return;
		}
		if (!self->m_channel || self->m_disconnected) {
			std::move(h)(boost::asio::error::not_connected);
			return;
		}

		const auto samples = opus_packet_get_samples_per_frame(
			reinterpret_cast<const unsigned char *>(data.data()), SAMPLE_RATE);
		if (samples < 0) {
			std::move(h)(boost::system::error_code{
				samples, boost::system::system_category()});
			return;
		}

		self->m_pending_frames++;
		self->m_channel->async_send(
			boost::system::error_code{},
			AudioPacket{std::move(data), static_cast<size_t>(samples)},
			[self, h = std::move(h)](boost::system::error_code ec) mutable {
				if (ec) {
					self->m_pending_frames--;
					std::move(h)(ec);
					return;
				}
				std::move(h)(outcome::success());
			});
	});
}

void VoiceConnection::Impl::send_raw(
	std::vector<int16_t> data, asio::any_completion_handler<void(Result<>)> h) {
	auto self = shared_from_this();
	asio::dispatch(
		m_strand, [self, data = std::move(data), h = std::move(h)]() mutable {
			if (data.empty()) {
				std::move(h)(boost::system::errc::invalid_argument);
				return;
			}
			if (!self->m_channel || self->m_disconnected) {
				std::move(h)(boost::asio::error::not_connected);
				return;
			}

			std::vector<std::byte> encoded_audio(data.size());
			auto encoded_res = self->m_codec->encode(
				boost::span<const int16_t>(data.data(), data.size()),
				encoded_audio);
			if (!encoded_res) {
				std::move(h)(encoded_res.error());
				return;
			}

			encoded_audio.resize(encoded_res.value());
			self->send_opus(std::move(encoded_audio), std::move(h));
		});
}

void VoiceConnection::Impl::opus_sender_loop() {
	auto self = shared_from_this();
	if (!m_channel || !m_udp) { return; }
	if (!m_send_timer) { m_send_timer.emplace(m_strand.get_inner_executor()); }

	// Signal "ready" once.
	if (m_ready_chan) {
		m_ready_chan->async_send(
			boost::system::error_code{}, {}, [](boost::system::error_code) {});
	}

	struct SenderOp : std::enable_shared_from_this<SenderOp> {
		std::shared_ptr<VoiceConnection::Impl> impl;
		std::chrono::steady_clock::time_point next_send_time{
			std::chrono::steady_clock::now()};
		int consecutive_failures{0};

		explicit SenderOp(std::shared_ptr<VoiceConnection::Impl> i)
			: impl(std::move(i)) {}

		void step() {
			if (!impl->m_channel || !impl->m_udp) { return; }

			impl->m_channel->async_receive(
				[me = shared_from_this()](
					boost::system::error_code ec, AudioPacket pkt) mutable {
					if (ec) { return; }
					me->process(std::move(pkt));
				});
		}

		void process(AudioPacket pkt) {
			const auto now = std::chrono::steady_clock::now();

			if (next_send_time > now) {
				impl->m_send_timer->expires_at(next_send_time);
				impl->m_send_timer->async_wait(
					[me = shared_from_this(), pkt = std::move(pkt)](
						boost::system::error_code ec) mutable {
						if (ec) { return; }
						me->send(std::move(pkt));
					});
				return;
			}

			const auto drift_ms =
				std::chrono::duration_cast<std::chrono::milliseconds>(
					now - next_send_time)
					.count();
			if (drift_ms > 100) {
				impl->log(
					fmt::format("Large timing drift detected: {}ms, resetting",
								drift_ms),
					LogLevel::Warn);
				next_send_time = now;
			}

			send(std::move(pkt));
		}

		void maintain_sync(size_t frame_count) {
			impl->m_rtp_sequence++;
			impl->m_rtp_timestamp += static_cast<uint32_t>(frame_count);
			next_send_time +=
				std::chrono::milliseconds(frame_count * 1000 / SAMPLE_RATE);
		}

		void send(AudioPacket pkt) {
			if (!impl->m_udp) {
				impl->m_pending_frames--;
				return;
			}

			if (impl->m_dave_manager->is_e2ee_enabled() &&
				!impl->m_dave_manager->ready_to_send()) {
				if (!impl->m_warned_waiting_for_e2ee) {
					impl->log(
						fmt::format("DAVE enabled but not ready at packet {}",
									impl->m_packet_count.load()),
						LogLevel::Warn);
					impl->m_warned_waiting_for_e2ee = true;
				}
				maintain_sync(pkt.frame_count);
				impl->m_pending_frames--;
				step();
				return;
			}

			boost::span<const std::byte> payload_view(
				pkt.encoded.data(), pkt.encoded.size());
			std::vector<std::byte> dave_ciphertext;

			if (impl->m_dave_manager->is_e2ee_enabled()) {
				size_t max_sz = impl->m_dave_manager->max_ciphertext_size(
					discord::dave::MediaType::Audio, pkt.encoded.size());
				dave_ciphertext.resize(max_sz);

				auto res = impl->m_dave_manager->encrypt_frame(
					discord::dave::MediaType::Audio, impl->m_ssrc, pkt.encoded,
					dave_ciphertext);
				if (!res) {
					impl->log("DAVE encrypt failed", LogLevel::Error);
					impl->m_pending_frames--;
					maintain_sync(pkt.frame_count);
					step();
					return;
				}

				dave_ciphertext.resize(res.value());
				payload_view = boost::span<const std::byte>(
					dave_ciphertext.data(), dave_ciphertext.size());
			}

			std::array<std::byte, voice::RTP_HEADER_SIZE> header{};
			header[0] = std::byte{0x80};
			header[1] = std::byte{0x78};
			voice::put_be(header.data() + 2, impl->m_rtp_sequence);
			voice::put_be(header.data() + 4, impl->m_rtp_timestamp);
			voice::put_be(header.data() + 8, impl->m_ssrc);

			auto encrypted_pkt =
				impl->m_crypto.encrypt_rtp(header, payload_view);
			if (!encrypted_pkt) {
				impl->log("Transport encrypt failed", LogLevel::Error);
				impl->m_pending_frames--;
				maintain_sync(pkt.frame_count);
				step();
				return;
			}

			impl->m_udp->send(
				encrypted_pkt.value(),
				[me = shared_from_this(), frame_count = pkt.frame_count](
					Result<size_t> send_res) mutable {
					if (!send_res) {
						++me->consecutive_failures;
						me->impl->log(fmt::format("UDP send failed ({}/10): {}",
												  me->consecutive_failures,
												  send_res.error().message()),
									  LogLevel::Warn);

						me->impl->m_pending_frames--;
						me->maintain_sync(frame_count);

						if (me->consecutive_failures >= 10) {
							me->impl->log(
								"Too many consecutive UDP send failures, "
								"stopping sender",
								LogLevel::Error);
							return;	 // Stop sender loop.
						}

						me->step();
						return;
					}

					me->consecutive_failures = 0;
					me->impl->m_packet_count.fetch_add(1);
					me->impl->m_pending_frames--;
					me->maintain_sync(frame_count);
					me->step();
				});
		}
	};

	auto op = std::make_shared<SenderOp>(self);
	asio::post(m_strand, [op]() mutable { op->step(); });
}

}  // namespace ekizu