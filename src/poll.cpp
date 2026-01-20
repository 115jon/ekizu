#include <ekizu/json_util.hpp>
#include <ekizu/poll.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const PollMedia &p) {
	serialize(j, "text", p.text);
	serialize(j, "emoji", p.emoji);
}

void from_json(const nlohmann::json &j, PollMedia &p) {
	deserialize(j, "text", p.text);
	deserialize(j, "emoji", p.emoji);
}

void to_json(nlohmann::json &j, const PollAnswer &a) {
	serialize(j, "answer_id", a.answer_id);
	serialize(j, "poll_media", a.poll_media);
}

void from_json(const nlohmann::json &j, PollAnswer &a) {
	deserialize(j, "answer_id", a.answer_id);
	deserialize(j, "poll_media", a.poll_media);
}

void to_json(nlohmann::json &j, const Poll &p) {
	serialize(j, "question", p.question);
	serialize(j, "answers", p.answers);
	serialize(j, "duration", p.duration);
	serialize(j, "allow_multiselect", p.allow_multiselect);
	serialize(j, "layout_type", p.layout_type);
}

void from_json(const nlohmann::json &j, Poll &p) {
	deserialize(j, "question", p.question);
	deserialize(j, "answers", p.answers);
	deserialize(j, "duration", p.duration);
	deserialize(j, "allow_multiselect", p.allow_multiselect);
	deserialize(j, "layout_type", p.layout_type);
}
}  // namespace ekizu
