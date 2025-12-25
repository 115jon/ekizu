#include <ekizu/opus_codec.hpp>
#include <ekizu/voice_types.hpp>

namespace ekizu {

Codec::Codec(UniqueOpusDecoder decoder, UniqueOpusEncoder encoder)
	: m_decoder{std::move(decoder)}, m_encoder{std::move(encoder)} {}

Result<int> Codec::encode(boost::span<const int16_t> pcm,
						  boost::span<std::byte> opus) const {
	auto err = opus_encode(m_encoder.get(), pcm.data(), FRAME_COUNT,
						   reinterpret_cast<unsigned char *>(opus.data()),
						   static_cast<opus_int32>(opus.size()));

	if (err < 0) {
		return boost::system::error_code{err, boost::system::system_category()};
	}

	return err;
}

Result<UniqueOpusDecoder> create_decoder() {
	int err{};
	auto decoder = UniqueOpusDecoder{opus_decoder_create(SAMPLE_RATE, 2, &err)};

	if (err != OPUS_OK) {
		return boost::system::error_code{err, boost::system::system_category()};
	}

	return decoder;
}

Result<UniqueOpusEncoder> create_encoder() {
	int err{};
	auto encoder = UniqueOpusEncoder{
		opus_encoder_create(SAMPLE_RATE, 2, OPUS_APPLICATION_VOIP, &err)};

	if (err != OPUS_OK) {
		return boost::system::error_code{err, boost::system::system_category()};
	}

	return encoder;
}

}  // namespace ekizu
