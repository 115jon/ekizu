#ifndef EKIZU_OPUS_CODEC_HPP
#define EKIZU_OPUS_CODEC_HPP

#include <opus/opus.h>

#include <boost/core/span.hpp>
#include <ekizu/result.hpp>
#include <memory>

namespace ekizu {
constexpr auto OPUS_MAX_PACKET_SIZE = 1275;

template <typename T>
struct DeleterOf;

template <>
struct DeleterOf<OpusDecoder> {
	void operator()(OpusDecoder *decoder) const {
		opus_decoder_destroy(decoder);
	}
};

template <>
struct DeleterOf<OpusEncoder> {
	void operator()(OpusEncoder *encoder) const {
		opus_encoder_destroy(encoder);
	}
};

template <>
struct DeleterOf<OpusRepacketizer> {
	void operator()(OpusRepacketizer *repacketizer) const {
		opus_repacketizer_destroy(repacketizer);
	}
};

using UniqueOpusDecoder = std::unique_ptr<OpusDecoder, DeleterOf<OpusDecoder>>;
using UniqueOpusEncoder = std::unique_ptr<OpusEncoder, DeleterOf<OpusEncoder>>;
using UniqueOpusRepacketizer =
	std::unique_ptr<OpusRepacketizer, DeleterOf<OpusRepacketizer>>;

struct Codec {
	Codec(UniqueOpusDecoder decoder, UniqueOpusEncoder encoder,
		  UniqueOpusRepacketizer repacketizer);

	[[nodiscard]] Result<int> encode(boost::span<const int16_t> pcm,
									 boost::span<std::byte> opus) const;

   private:
	UniqueOpusDecoder m_decoder;
	UniqueOpusEncoder m_encoder;
	UniqueOpusRepacketizer m_repacketizer;
};

[[nodiscard]] Result<UniqueOpusDecoder> create_decoder();
[[nodiscard]] Result<UniqueOpusEncoder> create_encoder();
[[nodiscard]] Result<UniqueOpusRepacketizer> create_repacketizer();

}  // namespace ekizu

#endif	// EKIZU_OPUS_CODEC_HPP
