#ifndef EKIZU_EMOJI_HPP
#define EKIZU_EMOJI_HPP

#include <ekizu/user.hpp>

namespace ekizu {
/**
 * @brief Represents an emoji.
 *
 * @see https://discord.com/developers/docs/resources/emoji#emoji-object
 */
struct Emoji {
	/// The ID of the emoji.
	std::optional<Snowflake> id;
	/// The name of the emoji (can be null only in reaction emoji objects).
	std::optional<std::string> name;
	/// The roles this emoji is active for.
	std::vector<Snowflake> roles;
	/// The user ID of the emoji creator.
	Snowflake user_id;
	/// Whether this emoji requires colons to be used.
	bool require_colons{};
	/// Whether this emoji is managed.
	bool managed{};
	/// Whether this emoji is animated.
	bool animated{};
	/// Whether this emoji is available, which may be false due to loss of
	/// Server Boosts.
	bool available{};
	/// The user that sent this emoji.
	User user;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Emoji &e);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Emoji &e);

struct PartialEmoji {
	Snowflake id{};
	std::string name;
	std::optional<bool> animated;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const PartialEmoji &p);
EKIZU_EXPORT void from_json(const nlohmann::json &j, PartialEmoji &p);

/**
 * @brief Builder for PartialEmoji objects used in components.
 */
struct EKIZU_EXPORT EmojiBuilder {
	/**
	 * @brief Sets the ID of the emoji.
	 * Use this for custom guild emojis.
	 *
	 * @param id The snowflake ID of the emoji.
	 * @return Reference to the builder.
	 */
	EmojiBuilder &id(Snowflake id);

	/**
	 * @brief Sets the name of the emoji.
	 * For custom emojis, this is the name.
	 * For unicode emojis, this is the actual unicode character (e.g., "⏮️").
	 *
	 * @param name The name or unicode character.
	 * @return Reference to the builder.
	 */
	EmojiBuilder &name(std::string name);

	/**
	 * @brief Sets whether the emoji is animated.
	 *
	 * @param animated True if animated.
	 * @return Reference to the builder.
	 */
	EmojiBuilder &animated(bool animated);

	/**
	 * @brief Builds the PartialEmoji object.
	 *
	 * @return The constructed PartialEmoji.
	 */
	[[nodiscard]] PartialEmoji build() const;

   private:
	PartialEmoji m_emoji;
};
}  // namespace ekizu

#endif	// EKIZU_EMOJI_HPP
