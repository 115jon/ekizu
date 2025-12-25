#ifndef EKIZU_VOICE_TYPES_HPP
#define EKIZU_VOICE_TYPES_HPP

#include <string_view>

namespace ekizu {

constexpr uint8_t CHANNEL_COUNT = 2;
constexpr uint16_t FRAME_COUNT = 960;
constexpr uint32_t SAMPLE_RATE = 48'000;

enum class VoiceTransportMode {
	XChaCha20_Poly1305_RTPSIZE,
	AES256_GCM_RTPSIZE,
};

inline std::string_view to_string(VoiceTransportMode m) {
	switch (m) {
		case VoiceTransportMode::XChaCha20_Poly1305_RTPSIZE:
			return "aead_xchacha20_poly1305_rtpsize";
		case VoiceTransportMode::AES256_GCM_RTPSIZE:
			return "aead_aes256_gcm_rtpsize";
	}
	return "unknown";
}

}  // namespace ekizu

#endif	// EKIZU_VOICE_TYPES_HPP
