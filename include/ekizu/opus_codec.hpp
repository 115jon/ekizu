#ifndef EKIZU_OPUS_CODEC_HPP
#define EKIZU_OPUS_CODEC_HPP

#include <opus/opus.h>

#include <boost/core/span.hpp>
#include <ekizu/export.hpp>
#include <ekizu/result.hpp>
#include <memory>

namespace ekizu {
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

using UniqueOpusDecoder = std::unique_ptr<OpusDecoder, DeleterOf<OpusDecoder>>;
using UniqueOpusEncoder = std::unique_ptr<OpusEncoder, DeleterOf<OpusEncoder>>;

struct Codec {
	Codec(UniqueOpusDecoder decoder, UniqueOpusEncoder encoder);

	[[nodiscard]] Result<int> encode(boost::span<const int16_t> pcm,
									 boost::span<std::byte> opus) const;

   private:
	UniqueOpusDecoder m_decoder;
	UniqueOpusEncoder m_encoder;
};

[[nodiscard]] EKIZU_EXPORT Result<UniqueOpusDecoder> create_decoder();
[[nodiscard]] EKIZU_EXPORT Result<UniqueOpusEncoder> create_encoder();

}  // namespace ekizu

#endif	// EKIZU_OPUS_CODEC_HPP
