#ifndef DAVE_TYPES_HPP
#define DAVE_TYPES_HPP

#include <cstdint>
#include <map>
#include <variant>
#include <vector>

namespace ekizu::dave {

using ProtocolVersion = uint16_t;
using SignatureVersion = uint16_t;
using TransitionId = uint16_t;
using KeyGeneration = uint8_t;

enum class MediaType : uint8_t { Audio, Video };

enum class Codec : uint8_t { Unknown, Opus, Vp8, Vp9, H264, H265, Av1 };

struct Failed {};
struct Ignored {};

using RosterMap = std::map<uint64_t, std::vector<uint8_t>>;
using RosterVariant = std::variant<Failed, Ignored, RosterMap>;

constexpr ProtocolVersion max_protocol_version() noexcept { return 1; }

}  // namespace ekizu::dave

#endif	// DAVE_TYPES_HPP
