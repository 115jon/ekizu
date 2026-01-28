#ifndef DAVE_CODEC_UTILS_HPP
#define DAVE_CODEC_UTILS_HPP

#include "frame_processors.hpp"

namespace ekizu::dave::codec_utils {

/**
 * @brief process opus audio frame
 * @param processor outbound frame processor
 * @param frame frame bytes
 * @return true if frame could be processed
 */
bool process_frame_opus(OutboundFrameProcessor &processor,
						boost::span<const uint8_t> frame);

/**
 * @brief process VP8 video frame
 * @param processor outbound frame processor
 * @param frame frame bytes
 * @return true if frame could be processed
 */
bool process_frame_vp8(OutboundFrameProcessor &processor,
					   boost::span<const uint8_t> frame);

/**
 * @brief process VP9 video frame
 * @param processor outbound frame processor
 * @param frame frame bytes
 * @return true if frame could be processed
 */
bool process_frame_vp9(OutboundFrameProcessor &processor,
					   boost::span<const uint8_t> frame);

/**
 * @brief process H264 video frame
 * @param processor outbound frame processor
 * @param frame frame bytes
 * @return true if frame could be processed
 */
bool process_frame_h264(OutboundFrameProcessor &processor,
						boost::span<const uint8_t> frame);

/**
 * @brief process H265 video frame
 * @param processor outbound frame processor
 * @param frame frame bytes
 * @return true if frame could be processed
 */
bool process_frame_h265(OutboundFrameProcessor &processor,
						boost::span<const uint8_t> frame);

/**
 * @brief process AV1 video frame
 * @param processor outbound frame processor
 * @param frame frame bytes
 * @return true if frame could be processed
 */
bool process_frame_av1(OutboundFrameProcessor &processor,
					   boost::span<const uint8_t> frame);

/**
 * @brief Check if encrypted frame is valid
 * @param processor outbound frame processor
 * @param frame frame to validate
 * @return true if frame could be validated
 */
bool validate_encrypted_frame(OutboundFrameProcessor &processor,
							  boost::span<uint8_t> frame);

}  // namespace ekizu::dave::codec_utils

#endif	// DAVE_CODEC_UTILS_HPP
