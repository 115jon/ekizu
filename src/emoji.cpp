#include <ekizu/emoji.hpp>
#include <ekizu/json_util.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const Emoji &e) {
	serialize(j, "id", e.id);
	serialize(j, "name", e.name);
	serialize(j, "roles", e.roles);
	serialize(j, "user_id", e.user_id);
	serialize(j, "require_colons", e.require_colons);
	serialize(j, "managed", e.managed);
	serialize(j, "animated", e.animated);
	serialize(j, "available", e.available);
	serialize(j, "user", e.user);
}

void from_json(const nlohmann::json &j, Emoji &e) {
	deserialize(j, "id", e.id);
	deserialize(j, "name", e.name);
	deserialize(j, "roles", e.roles);
	deserialize(j, "user_id", e.user_id);
	deserialize(j, "require_colons", e.require_colons);
	deserialize(j, "managed", e.managed);
	deserialize(j, "animated", e.animated);
	deserialize(j, "available", e.available);
	deserialize(j, "user", e.user);
}

void to_json(nlohmann::json &j, const PartialEmoji &p) {
	serialize(j, "id", p.id);
	serialize(j, "name", p.name);
	serialize(j, "animated", p.animated);
}

void from_json(const nlohmann::json &j, PartialEmoji &p) {
	deserialize(j, "id", p.id);
	deserialize(j, "name", p.name);
	deserialize(j, "animated", p.animated);
}

EmojiBuilder &EmojiBuilder::id(Snowflake id) {
	m_emoji.id = id;
	return *this;
}

EmojiBuilder &EmojiBuilder::name(std::string name) {
	m_emoji.name = std::move(name);
	return *this;
}

EmojiBuilder &EmojiBuilder::animated(bool animated) {
	m_emoji.animated = animated;
	return *this;
}

PartialEmoji EmojiBuilder::build() const { return m_emoji; }
}  // namespace ekizu
