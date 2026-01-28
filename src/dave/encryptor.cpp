#include "encryptor.hpp"

#include <algorithm>

#include "buffer_utils.hpp"
#include "error.hpp"
#include "leb128.hpp"

namespace ekizu::dave {

void Encryptor::set_key_ratchet(std::vector<uint8_t> key) {
	m_logger.debug("Setting key ratchet (key size: {})", key.size());
	m_ratchet = std::make_unique<detail::SimpleKeyRatchet>(std::move(key));
	m_cryptor = nullptr;
	m_current_key_generation = 0;
	m_truncated_nonce = 0;
}

void Encryptor::set_key_ratchet(std::unique_ptr<KeyRatchetInterface> ratchet) {
	m_logger.debug("Setting MLS key ratchet");
	m_ratchet = std::move(ratchet);
	m_cryptor = nullptr;
	m_current_key_generation = 0;
	m_truncated_nonce = 0;
}

void Encryptor::set_passthrough_mode(bool passthrough_mode) {
	m_logger.info(
		"Passthrough mode: {}", passthrough_mode ? "enabled" : "disabled");
	m_passthrough_mode = passthrough_mode;
	m_current_protocol_version = passthrough_mode ? 0 : max_protocol_version();
}

bool Encryptor::has_key_ratchet() const { return m_ratchet != nullptr; }

void Encryptor::assign_ssrc_to_codec(uint32_t ssrc, Codec codec) {
	m_ssrc_codec_map[ssrc] = codec;
}

Codec Encryptor::codec_for_ssrc(uint32_t ssrc) const {
	auto it = m_ssrc_codec_map.find(ssrc);
	return it != m_ssrc_codec_map.end() ? it->second : Codec::Opus;
}

Result<size_t> Encryptor::encrypt(MediaType media_type, uint32_t ssrc,
								  boost::span<const uint8_t> frame,
								  boost::span<uint8_t> encrypted_frame_out) {
	// Fast path: passthrough mode (no encryption)
	if (BOOST_LIKELY(m_passthrough_mode)) {
		std::copy(frame.begin(), frame.end(), encrypted_frame_out.begin());
		return frame.size();
	}

	// Error path: no key ratchet configured
	if (BOOST_UNLIKELY(!m_ratchet)) {
		m_logger.warn("Encryption failed: no key ratchet");
		return DaveError::RatchetNotReady;
	}

	auto codec = codec_for_ssrc(ssrc);

	// Use the frame processor to prepare the frame
	m_frame_processor.process_frame(frame, codec);

	const auto &unencrypted_bytes = m_frame_processor.get_unencrypted_bytes();
	const auto &encrypted_bytes = m_frame_processor.get_encrypted_bytes();
	auto &ciphertext_bytes = m_frame_processor.get_ciphertext_bytes();
	const auto &unencrypted_ranges = m_frame_processor.get_unencrypted_ranges();

	auto nonce_buffer = std::array<uint8_t, detail::AES_GCM_128_NONCE_BYTES>();

	// Calculate Nonce (pre-increment)
	m_truncated_nonce++;

	auto generation = static_cast<uint8_t>(
		m_truncated_nonce >> detail::RATCHET_GENERATION_SHIFT_BITS);

	// Key rotation check
	if (!m_cryptor || generation != m_current_key_generation) {
		auto key = m_ratchet->get_key(generation);
		if (BOOST_UNLIKELY(key.empty())) {
			m_logger.warn(
				"Key ratchet returned empty key for generation {}", generation);
			return DaveError::InvalidGeneration;
		}

		m_cryptor = std::make_shared<detail::AeadCipher>(key);
		m_current_key_generation = generation;
	}

	// Build nonce buffer
	detail::write_value(
		boost::span<uint8_t>(nonce_buffer)
			.subspan(detail::AES_GCM_128_TRUNCATED_SYNC_NONCE_OFFSET),
		m_truncated_nonce);

	// Encrypt frame content - reuse temp buffer to avoid per-frame allocation
	m_temp_cipher_output.resize(
		encrypted_bytes.size() + detail::AES_GCM_128_TRUNCATED_TAG_BYTES);

	auto encrypt_res = m_cryptor->encrypt(
		{encrypted_bytes.data(), encrypted_bytes.size()},
		{unencrypted_bytes.data(), unencrypted_bytes.size()},
		{nonce_buffer.data(), nonce_buffer.size()},
		{m_temp_cipher_output.data(), m_temp_cipher_output.size()});

	if (BOOST_UNLIKELY(!encrypt_res)) {
		m_logger.warn("AES-GCM encryption failed");
		return DaveError::EncryptionFailed;
	}

	// Split temp buffer into ciphertext (for body) and tag (for footer)
	auto cipher_src = boost::span<const uint8_t>(m_temp_cipher_output)
						  .first(encrypted_bytes.size());
	std::copy(cipher_src.begin(), cipher_src.end(), ciphertext_bytes.begin());

	auto tag_span = boost::span<const uint8_t>(m_temp_cipher_output)
						.subspan(encrypted_bytes.size(),
								 detail::AES_GCM_128_TRUNCATED_TAG_BYTES);

	// Reconstruct frame body
	size_t written_offset =
		m_frame_processor.reconstruct_frame(encrypted_frame_out);
	size_t body_size = written_offset;

	if (BOOST_UNLIKELY(written_offset == 0)) {
		m_logger.warn("Frame reconstruction failed");
		return DaveError::EncryptionFailed;
	}

	// Append Tag
	auto tag_dest = encrypted_frame_out.subspan(
		written_offset, detail::AES_GCM_128_TRUNCATED_TAG_BYTES);
	std::copy(tag_span.begin(), tag_span.end(), tag_dest.begin());
	written_offset += detail::AES_GCM_128_TRUNCATED_TAG_BYTES;

	// Append Truncated Nonce (LEB128)
	written_offset += write_leb128(
		m_truncated_nonce, encrypted_frame_out.data() + written_offset);

	// Append Unencrypted Ranges (LEB128)
	written_offset += serialize_unencrypted_ranges(
		unencrypted_ranges, encrypted_frame_out.data() + written_offset,
		encrypted_frame_out.size() - written_offset);

	// Calculate and append supplemental bytes size
	size_t current_supplemental_payload = written_offset - body_size;
	current_supplemental_payload += sizeof(detail::supplemental_bytes_size);
	current_supplemental_payload += sizeof(detail::MARKER_BYTES);

	auto size_val = static_cast<detail::supplemental_bytes_size>(
		current_supplemental_payload);

	detail::write_value(encrypted_frame_out.subspan(written_offset), size_val);
	written_offset += sizeof(size_val);

	// Append Magic Marker
	detail::write_value(
		encrypted_frame_out.subspan(written_offset), detail::MARKER_BYTES);
	written_offset += sizeof(detail::MARKER_BYTES);

	return written_offset;
}

size_t Encryptor::get_max_ciphertext_byte_size(MediaType /*media_type*/,
											   size_t frame_size) const {
	return frame_size + detail::SUPPLEMENTAL_BYTES +
		   detail::TRANSFORM_PADDING_BYTES;
}

}  // namespace ekizu::dave
