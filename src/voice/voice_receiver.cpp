#include <boost/core/span.hpp>
#include <cstddef>
#include <cstdint>
#include <ekizu/voice_connection.hpp>
#include <nlohmann/json.hpp>
#include <vector>

#include "voice_connection_impl.hpp"
#include "voice_util.hpp"

namespace ekizu {
namespace {

boost::span<const std::byte> strip_rtp_padding(
	boost::span<const std::byte> payload, bool padding_bit_set) {
	if (!padding_bit_set || payload.empty()) { return payload; }

	const auto pad = static_cast<std::uint8_t>(payload.back());
	if (pad == 0 || pad > payload.size()) { return payload; }

	return payload.first(payload.size() - pad);
}

}  // namespace

void VoiceConnection::Impl::udp_receiver_loop() {
	auto self = shared_from_this();
	if (!m_udp || !m_recv_chan) { return; }

	m_receiver_running = true;

	struct ReceiverOp : std::enable_shared_from_this<ReceiverOp> {
		std::shared_ptr<VoiceConnection::Impl> impl;

		// Used only to avoid log spam when DAVE isn't ready / ratchets aren't
		// set yet.
		std::uint64_t dave_fail_count = 0;

		explicit ReceiverOp(std::shared_ptr<VoiceConnection::Impl> i)
			: impl(std::move(i)) {}

		void step() {
			if (!impl->m_udp || !impl->m_receiver_running) { return; }

			impl->m_udp->receive([me = shared_from_this()](
									 Result<std::string> recv_res) mutable {
				if (!recv_res) {
					me->impl->log(fmt::format("UDP receive error: {}",
											  recv_res.error().message()),
								  LogLevel::Warn);
					return;
				}

				me->process(recv_res.value());
				me->step();
			});
		}

		void process(const std::string &data) {
			auto bytes = boost::span<const std::byte>(
				reinterpret_cast<const std::byte *>(data.data()), data.size());

			if (bytes.size() < voice::RTP_HEADER_SIZE) { return; }

			// RTP first byte: V(2), P(1), X(1), CC(4)
			const auto b0 = static_cast<std::uint8_t>(bytes[0]);
			const auto version = (b0 >> 6) & 0x03;
			if (version != 2) { return; }

			// Check payload type (second byte, bits 0-6)
			const auto b1 = static_cast<std::uint8_t>(bytes[1]);
			const auto payload_type = b1 & 0x7F;  // mask off marker bit

			// Skip non-Opus frames (Discord uses PT 120 for Opus)
			// Common silence/CN frames use PT 13, or other values
			if (payload_type != 120) { return; }

			const bool padding = (b0 & 0x20) != 0;
			const bool extension = (b0 & 0x10) != 0;
			const std::size_t cc = (b0 & 0x0F);

			// For the *_rtpsize modes, the AEAD AAD is the unencrypted RTP
			// header: base header + CSRC list + (if X=1) the 4-byte extension
			// preamble.
			std::size_t aad_len = 12 + (cc * 4);
			if (bytes.size() < aad_len) { return; }

			std::uint16_t ext_len_words = 0;
			if (extension) {
				if (bytes.size() < aad_len + 4) { return; }
				ext_len_words =
					voice::read_u16be(bytes.subspan(aad_len + 2, 2));
				aad_len += 4;
			}

			if (bytes.size() <= aad_len) { return; }

			auto aad_header = bytes.first(aad_len);
			auto encrypted_payload = bytes.subspan(aad_len);

			auto decrypted_res =
				impl->m_crypto.decrypt_rtp(aad_header, encrypted_payload);
			if (!decrypted_res) {
				const std::uint8_t x = extension ? 1 : 0;
				const std::uint8_t p = padding ? 1 : 0;

				std::uint8_t n0 = 0, n1 = 0, n2 = 0, n3 = 0;
				if (encrypted_payload.size() >= 4) {
					auto last4 =
						encrypted_payload.subspan(encrypted_payload.size() - 4);
					n0 = static_cast<std::uint8_t>(last4[0]);
					n1 = static_cast<std::uint8_t>(last4[1]);
					n2 = static_cast<std::uint8_t>(last4[2]);
					n3 = static_cast<std::uint8_t>(last4[3]);
				}

				impl->log(
					fmt::format("Transport decrypt failed: {} (b0=0x{:02x} "
								"cc={} x={} p={} "
								"aad_len={} pkt_len={} enc_len={} ext_words={} "
								"nonce={:02x}{:02x}{:02x}{:02x})",
								decrypted_res.error().message(), b0, cc, x, p,
								aad_len, bytes.size(), encrypted_payload.size(),
								ext_len_words, n0, n1, n2, n3),
					LogLevel::Debug);
				return;
			}

			auto &decrypted = decrypted_res.value();
			auto media_payload = boost::span<const std::byte>(
				decrypted.data(), decrypted.size());

			// If X=1, the extension payload is decrypted and begins the
			// plaintext (the 4-byte preamble was unencrypted / in AAD).
			if (extension) {
				const std::size_t ext_bytes =
					static_cast<std::size_t>(ext_len_words) * 4;
				if (ext_bytes > media_payload.size()) { return; }
				media_payload = media_payload.subspan(ext_bytes);
			}

			media_payload = strip_rtp_padding(media_payload, padding);

			const uint32_t ssrc = voice::read_u32be(bytes.subspan(8, 4));
			const uint16_t sequence = voice::read_u16be(bytes.subspan(2, 2));
			const uint32_t timestamp = voice::read_u32be(bytes.subspan(4, 4));

			// If E2EE is enabled, do not emit transport-only audio; drop until
			// DAVE is ready so you only record real audio.
			if (impl->m_dave_manager &&
				impl->m_dave_manager->is_e2ee_enabled()) {
				// Wait until we can attempt decryption (sender ready)
				if (!impl->m_dave_manager->can_attempt_decrypt()) { return; }

				auto it = impl->m_ssrc_to_user_id.find(ssrc);
				if (it == impl->m_ssrc_to_user_id.end()) { return; }

				const std::string &user_id = it->second;

				// Discord SFU silence packet: 3-byte sequence 0xF8FFFE
				// Per DAVE protocol, this is synthesized when source is muted
				// and must be allowed through without DAVE decryption.

				if (media_payload.size() == 3 &&
					std::equal(media_payload.begin(), media_payload.end(),
							   voice::SILENCE_FRAME.begin())) {
					Packet pkt;
					pkt.ssrc = ssrc;
					pkt.sequence = sequence;
					pkt.timestamp = timestamp;
					// Empty opus signals silence to receiver
					pkt.user_id = nlohmann::json(user_id);

					impl->m_recv_chan->async_send(
						boost::system::error_code{}, std::move(pkt),
						[impl = impl](boost::system::error_code ec) {
							if (ec && !impl->m_disconnected) {
								impl->log(fmt::format("Channel send failed: {}",
													  ec.message()),
										  LogLevel::Warn);
							}
						});
					return;
				}

				constexpr std::size_t min_dave_frame_size = 44;
				if (media_payload.size() < min_dave_frame_size) { return; }

				std::vector<std::byte> plaintext(media_payload.size());
				auto decrypt_res = impl->m_dave_manager->decrypt_frame(
					discord::dave::MediaType::Audio, user_id,
					boost::span<const std::byte>(
						media_payload.data(), media_payload.size()),
					boost::span<std::byte>(plaintext.data(), plaintext.size()));

				if (!decrypt_res) { return; }

				// First successful decrypt - mark transition complete
				if (!impl->m_dave_manager->is_transition_complete()) {
					impl->m_dave_manager->set_transition_complete(true);
				}

				plaintext.resize(decrypt_res.value());

				Packet pkt;
				pkt.ssrc = ssrc;
				pkt.sequence = sequence;
				pkt.timestamp = timestamp;
				pkt.opus = std::move(plaintext);
				pkt.user_id = nlohmann::json(it->second);

				impl->m_recv_chan->async_send(
					boost::system::error_code{}, std::move(pkt),
					[impl = impl](boost::system::error_code ec) {
						if (ec && !impl->m_disconnected) {
							impl->log(fmt::format("Channel send failed: {}",
												  ec.message()),
									  LogLevel::Warn);
						}
					});
				return;
			}

			// E2EE disabled: forward transport-decrypted payload.
			std::vector<std::byte> plaintext;
			plaintext.assign(media_payload.begin(), media_payload.end());

			Packet pkt;
			pkt.ssrc = ssrc;
			pkt.sequence = sequence;
			pkt.timestamp = timestamp;
			pkt.opus = std::move(plaintext);

			if (auto it = impl->m_ssrc_to_user_id.find(ssrc);
				it != impl->m_ssrc_to_user_id.end()) {
				pkt.user_id = nlohmann::json(it->second);
			}

			impl->m_recv_chan->async_send(
				boost::system::error_code{}, std::move(pkt),
				[impl = impl](boost::system::error_code ec) {
					if (ec && !impl->m_disconnected) {
						impl->log(fmt::format(
									  "Channel send failed: {}", ec.message()),
								  LogLevel::Warn);
					}
				});
		}
	};

	auto op = std::make_shared<ReceiverOp>(self);
	asio::post(m_strand, [op]() mutable { op->step(); });
}

void VoiceConnection::Impl::on_speaking(std::string user_id, uint32_t ssrc,
										bool speaking) {
	asio::dispatch(m_strand, [this, user_id = std::move(user_id), ssrc,
							  speaking]() {
		if (speaking) {
			m_ssrc_to_user_id[ssrc] = user_id;
			m_user_id_to_ssrc[user_id] = ssrc;
			m_recognized_user_ids.insert(user_id);

			log(fmt::format("User {} is speaking with SSRC {}", user_id, ssrc),
				LogLevel::Debug);

			if (m_dave_manager && m_dave_manager->is_e2ee_enabled() &&
				m_dave_manager->has_joined_via_welcome()) {
				(void)m_dave_manager->install_receiver_ratchet(user_id);
			}
		}
	});
}

}  // namespace ekizu
