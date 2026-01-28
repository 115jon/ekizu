#include "decryptor.hpp"

#include <algorithm>

#include "buffer_utils.hpp"

namespace ekizu::dave {

void Decryptor::transition_to_key_ratchet(std::vector<uint8_t> key,
										  Duration transition_expiry) {
	// Note: All access is strand-serialized, no lock needed
	m_logger.debug("Transitioning to key ratchet (key size: {}, expiry: {}s)",
				   key.size(), transition_expiry.count());

	// Update expiry time for all existing managers
	update_cryptor_manager_expiry(transition_expiry);

	// Add new manager with the key
	if (!key.empty()) {
		m_managers.emplace_back(
			std::make_unique<detail::SimpleKeyRatchet>(std::move(key)));
	}
}

void Decryptor::transition_to_key_ratchet(
	std::unique_ptr<KeyRatchetInterface> ratchet, Duration transition_expiry) {
	m_logger.debug("Transitioning to MLS key ratchet (expiry: {}s)",
				   transition_expiry.count());
	// Update expiry time for all existing managers
	update_cryptor_manager_expiry(transition_expiry);

	// Add new manager with the ratchet (proper MLS key derivation)
	if (ratchet) { m_managers.emplace_back(std::move(ratchet)); }
}

void Decryptor::transition_to_passthrough_mode(bool passthrough_mode,
											   Duration transition_expiry) {
	m_logger.info(
		"Passthrough mode: {} (expiry: {}s)",
		passthrough_mode ? "enabled" : "disabled", transition_expiry.count());
	if (passthrough_mode) {
		m_allow_passthrough_until = TimePoint::max();
	} else {
		// Cap at transition_expiry from now
		auto max_expiry = std::chrono::steady_clock::now() + transition_expiry;
		m_allow_passthrough_until =
			std::min(m_allow_passthrough_until, max_expiry);
	}
}

Result<size_t> Decryptor::decrypt(MediaType media_type,
								  boost::span<const uint8_t> encrypted_frame,
								  boost::span<uint8_t> frame_out) {
	auto start = std::chrono::steady_clock::now();

	// Get a frame processor from pool
	auto local_frame = get_or_create_frame_processor();

	// RAII cleanup - return frame processor to pool
	struct ScopeExit {
		Decryptor *self;
		std::unique_ptr<InboundFrameProcessor> &fp;
		~ScopeExit() { self->return_frame_processor(std::move(fp)); }
	} scope_exit{this, local_frame};

	// Skip decrypting for silence frames (common case for audio)
	if (BOOST_UNLIKELY(
			media_type == MediaType::Audio &&
			encrypted_frame.size() == detail::OPUS_SILENCE_PACKET.size() &&
			std::equal(encrypted_frame.begin(), encrypted_frame.end(),
					   detail::OPUS_SILENCE_PACKET.begin()))) {
		std::copy(
			encrypted_frame.begin(), encrypted_frame.end(), frame_out.begin());
		return encrypted_frame.size();
	}

	cleanup_expired_managers();

	// Parse frame
	local_frame->parse_frame(encrypted_frame);

	auto now = std::chrono::steady_clock::now();
	bool can_passthrough = m_allow_passthrough_until > now;

	// If not encrypted and passthrough allowed
	if (!local_frame->is_encrypted() && can_passthrough) {
		std::copy(
			encrypted_frame.begin(), encrypted_frame.end(), frame_out.begin());
		stats_for(media_type).passthroughs++;
		return encrypted_frame.size();
	}

	// If not encrypted and can't passthrough, fail (rare error case)
	if (BOOST_UNLIKELY(!local_frame->is_encrypted())) {
		stats_for(media_type).decrypt_failure++;
		return boost::system::errc::bad_message;
	}

	// Try to decrypt with each valid cipher manager
	// REVERSE iterate to try newest cryptors first
	bool success{};
	size_t bytes_written{};
	for (auto it = m_managers.rbegin(); it != m_managers.rend(); ++it) {
		if (decrypt_impl(
				*it, media_type, *local_frame, frame_out, &bytes_written)) {
			success = true;
			break;
		}
	}

	auto end = std::chrono::steady_clock::now();
	stats_for(media_type).decrypt_duration +=
		std::chrono::duration_cast<std::chrono::microseconds>(end - start)
			.count();

	if (BOOST_LIKELY(success)) {
		stats_for(media_type).decrypt_success++;
		return bytes_written;
	}

	stats_for(media_type).decrypt_failure++;
	return boost::system::errc::bad_message;
}

bool Decryptor::decrypt_impl(
	detail::CipherManager &manager, MediaType media_type,
	InboundFrameProcessor &frame_processor, boost::span<uint8_t> frame_out,
	size_t *bytes_written) {
	if (BOOST_UNLIKELY(!frame_processor.is_encrypted())) { return false; }

	auto tag = frame_processor.get_tag();
	auto truncated_nonce = frame_processor.get_truncated_nonce();
	auto authenticated_data = frame_processor.get_authenticated_data();
	auto ciphertext = frame_processor.get_ciphertext();
	auto plaintext_buffer = frame_processor.get_plaintext();

	// Compute generation from nonce
	auto generation = manager.compute_wrapped_generation(
		truncated_nonce >> detail::RATCHET_GENERATION_SHIFT_BITS);

	// Check if we can process this nonce (replay prevention)
	if (!manager.can_process_nonce(generation, truncated_nonce)) {
		return false;
	}

	// Get cipher for this generation
	auto *cipher = manager.get_cipher(generation);
	if (BOOST_UNLIKELY(cipher == nullptr)) { return false; }

	// Expand truncated nonce to full size
	std::array<uint8_t, detail::AES_GCM_128_NONCE_BYTES> nonce_buffer{};
	detail::write_value(
		boost::span<uint8_t>(nonce_buffer)
			.subspan(detail::AES_GCM_128_TRUNCATED_SYNC_NONCE_OFFSET),
		truncated_nonce);

	// Combine ciphertext and tag for decryption - reuse temp buffer
	m_temp_combined.resize(ciphertext.size() + tag.size());
	auto combined_span = boost::span<uint8_t>(m_temp_combined);
	std::copy(ciphertext.begin(), ciphertext.end(), combined_span.begin());
	std::copy(tag.begin(), tag.end(),
			  combined_span.subspan(ciphertext.size()).begin());

	// Decrypt
	stats_for(media_type).decrypt_attempts++;

	auto result = cipher->decrypt(
		{m_temp_combined.data(), m_temp_combined.size()}, authenticated_data,
		{nonce_buffer.data(), nonce_buffer.size()},
		{plaintext_buffer.data(), plaintext_buffer.size()});

	if (BOOST_UNLIKELY(!result)) { return false; }

	// Report success for nonce tracking
	manager.report_cipher_success(generation, truncated_nonce);

	// Reconstruct frame
	size_t written = frame_processor.reconstruct_frame(frame_out);
	if (BOOST_UNLIKELY(written == 0)) { return false; }

	*bytes_written = written;
	return true;
}

void Decryptor::update_cryptor_manager_expiry(Duration expiry) {
	auto max_expiry_time = std::chrono::steady_clock::now() + expiry;
	for (auto &manager : m_managers) { manager.update_expiry(max_expiry_time); }
}

void Decryptor::cleanup_expired_managers() {
	while (!m_managers.empty() && m_managers.front().is_expired()) {
		m_managers.pop_front();
	}
}

std::unique_ptr<InboundFrameProcessor>
Decryptor::get_or_create_frame_processor() {
	// Note: All access is strand-serialized, no lock needed
	if (m_frame_processors.empty()) {
		return std::make_unique<InboundFrameProcessor>();
	}
	auto fp = std::move(m_frame_processors.back());
	m_frame_processors.pop_back();
	return fp;
}

void Decryptor::return_frame_processor(
	std::unique_ptr<InboundFrameProcessor> fp) {
	m_frame_processors.push_back(std::move(fp));
}

}  // namespace ekizu::dave
