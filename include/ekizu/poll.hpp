#ifndef EKIZU_POLL_HPP
#define EKIZU_POLL_HPP

#include <ekizu/emoji.hpp>

namespace ekizu {
enum class PollLayoutType : uint8_t { Default = 1 };

struct PollMedia {
	std::optional<std::string> text;
	std::optional<PartialEmoji> emoji;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const PollMedia &p);
EKIZU_EXPORT void from_json(const nlohmann::json &j, PollMedia &p);

struct PollAnswer {
	uint32_t answer_id{};
	PollMedia poll_media;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const PollAnswer &a);
EKIZU_EXPORT void from_json(const nlohmann::json &j, PollAnswer &a);

struct Poll {
	PollMedia question;
	std::vector<PollAnswer> answers;
	std::optional<uint32_t> duration;
	std::optional<bool> allow_multiselect;
	std::optional<PollLayoutType> layout_type;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Poll &p);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Poll &p);
}  // namespace ekizu

#endif	// EKIZU_POLL_HPP
