#ifndef EKIZU_EMBED_HPP
#define EKIZU_EMBED_HPP

#include <ekizu/export.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>

namespace ekizu {
/**
 * @brief This structure is used to represent the similar
 * properties that EmbedThumbnail, EmbedVideo, and EmbedImage share.
 */
struct EmbedImage {
	/// The URL of the media.
	std::optional<std::string> url{};
	/// The proxy URL of the media.
	std::optional<std::string> proxy_url{};
	/// The height of the media.
	std::optional<uint16_t> height{};
	/// The width of the media.
	std::optional<uint16_t> width{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const EmbedImage &i);
EKIZU_EXPORT void from_json(const nlohmann::json &j, EmbedImage &i);

using EmbedThumbnail = EmbedImage;
using EmbedVideo = EmbedImage;

struct EmbedProvider {
	/// The name of the provider.
	std::optional<std::string> name;
	/// The URL of the provider.
	std::optional<std::string> url;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const EmbedProvider &p);
EKIZU_EXPORT void from_json(const nlohmann::json &j, EmbedProvider &p);

struct EmbedAuthor {
	/// The name of the author.
	std::string name;
	/// The URL of the author.
	std::optional<std::string> url{};
	/// The URL of the author's icon.
	std::optional<std::string> icon_url{};
	/// The proxied URL of the author's icon.
	std::optional<std::string> proxy_icon_url{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const EmbedAuthor &a);
EKIZU_EXPORT void from_json(const nlohmann::json &j, EmbedAuthor &a);

struct EmbedFooter {
	/// The text of the footer.
	std::string text;
	/// The URL of the footer icon.
	std::optional<std::string> icon_url{};
	/// The proxied URL of the footer icon.
	std::optional<std::string> proxy_icon_url{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const EmbedFooter &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j, EmbedFooter &f);

struct EmbedField {
	/// The name of the field.
	std::string name;
	/// The value of the field.
	std::string value;
	/// Whether or not the field is inline.
	std::optional<bool> inline_{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const EmbedField &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j, EmbedField &f);

/**
 * @brief This structure represents a Discord Embed, an embedded message
 * that can be attached to a regular message to provide rich content.
 */
struct Embed {
	/// The title of the embed.
	std::optional<std::string> title;
	/// The description of the embed.
	std::optional<std::string> description;
	/// The URL of the embed.
	std::optional<std::string> url;
	/// The timestamp of the embed.
	std::optional<std::string> timestamp;
	/// The color code of the embed.
	std::optional<uint16_t> color;
	/// Footer information.
	std::optional<EmbedFooter> footer;
	/// Image information.
	std::optional<EmbedImage> image;
	/// Thumbnail information.
	std::optional<EmbedThumbnail> thumbnail;
	/// Video information.
	std::optional<EmbedVideo> video;
	/// Provider information.
	std::optional<EmbedProvider> provider;
	/// Author information.
	std::optional<EmbedAuthor> author;
	/// Fields information.
	std::optional<std::vector<EmbedField> > fields;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Embed &e);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Embed &e);
}  // namespace ekizu

#endif	// EKIZU_EMBED_HPP
