#include "voice/opus_codec.hpp"

#include <ekizu/voice_types.hpp>

namespace ekizu {

Codec::Codec(UniqueOpusDecoder decoder, UniqueOpusEncoder encoder,
			 UniqueOpusRepacketizer repacketizer)
	: m_decoder{std::move(decoder)},
	  m_encoder{std::move(encoder)},
	  m_repacketizer{std::move(repacketizer)} {}

Result<int> Codec::encode(boost::span<const int16_t> pcm,
						  boost::span<std::byte> opus) const {
	if (pcm.size() % (FRAME_COUNT * CHANNEL_COUNT) != 0) {
		return boost::system::error_code{
			OPUS_BAD_ARG, boost::system::system_category()};
	}

	const auto num_frames = pcm.size() / (FRAME_COUNT * CHANNEL_COUNT);
	opus_repacketizer_init(m_repacketizer.get());
	auto *out = reinterpret_cast<unsigned char *>(opus.data());
	auto out_size = static_cast<opus_int32>(opus.size());

	for (size_t i = 0; i < num_frames; ++i) {
		std::array<unsigned char, OPUS_MAX_PACKET_SIZE> frame_buf{};
		const auto *pcm_ptr = pcm.data() + (i * FRAME_COUNT * CHANNEL_COUNT);
		const auto encoded_len =
			opus_encode(m_encoder.get(), pcm_ptr, FRAME_COUNT, frame_buf.data(),
						static_cast<opus_int32>(frame_buf.size()));

		if (encoded_len < 0) {
			return boost::system::error_code{
				encoded_len, boost::system::system_category()};
		}

		const auto ret = opus_repacketizer_cat(
			m_repacketizer.get(), frame_buf.data(), encoded_len);

		if (ret != OPUS_OK) {
			return boost::system::error_code{
				ret, boost::system::system_category()};
		}
	}

	const auto ret = opus_repacketizer_out(m_repacketizer.get(), out, out_size);

	if (ret < 0) {
		return boost::system::error_code{ret, boost::system::system_category()};
	}

	return ret;
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

Result<UniqueOpusRepacketizer> create_repacketizer() {
	auto *repacketizer = opus_repacketizer_create();

	if (repacketizer == nullptr) {
		return boost::system::error_code{
			OPUS_ALLOC_FAIL, boost::system::system_category()};
	}

	return UniqueOpusRepacketizer{repacketizer};
}

}  // namespace ekizu
