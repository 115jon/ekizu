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
	std::optional<std::string> url;
	/// The proxy URL of the media.
	std::optional<std::string> proxy_url;
	/// The height of the media.
	std::optional<uint16_t> height;
	/// The width of the media.
	std::optional<uint16_t> width;
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
	std::optional<std::string> url;
	/// The URL of the author's icon.
	std::optional<std::string> icon_url;
	/// The proxied URL of the author's icon.
	std::optional<std::string> proxy_icon_url;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const EmbedAuthor &a);
EKIZU_EXPORT void from_json(const nlohmann::json &j, EmbedAuthor &a);

struct EmbedFooter {
	/// The text of the footer.
	std::string text;
	/// The URL of the footer icon.
	std::optional<std::string> icon_url;
	/// The proxied URL of the footer icon.
	std::optional<std::string> proxy_icon_url;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const EmbedFooter &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j, EmbedFooter &f);

struct EmbedField {
	/// The name of the field.
	std::string name;
	/// The value of the field.
	std::string value;
	/// Whether or not the field is inline.
	std::optional<bool> inline_;
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

struct EmbedBuilder {
	/**
	 * @brief Adds a field to the Embed.
	 *
	 * @param field The field to add.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &add_field(const EmbedField &field) {
		if (!m_embed.fields) { m_embed.fields.emplace(); }

		m_embed.fields->emplace_back(field);
		return *this;
	}

	/**
	 * @brief Adds multiple fields to the Embed.
	 *
	 * @param fields The fields to add.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &add_fields(const std::vector<EmbedField> &fields) {
		if (!m_embed.fields) { m_embed.fields.emplace(); }

		m_embed.fields->insert(
			m_embed.fields->end(), fields.begin(), fields.end());
		return *this;
	}

	/**
	 * @brief Sets the author of the Embed.
	 *
	 * @param author The author to set.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &set_author(const EmbedAuthor &author) {
		m_embed.author = author;
		return *this;
	}

	/**
	 * @brief Set the color of the Embed.
	 *
	 * @param color The color to set.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &set_color(uint16_t color) {
		m_embed.color = color;
		return *this;
	}

	/**
	 * @brief Set the description of the Embed.
	 *
	 * @param description The description to set.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &set_description(std::string description) {
		m_embed.description = std::move(description);
		return *this;
	}

	/**
	 * @brief Sets the fields of the Embed.
	 *
	 * @param fields The fields to set.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &set_fields(const std::vector<EmbedField> &fields) {
		m_embed.fields = fields;
		return *this;
	}

	/**
	 * @brief Sets the footer of the Embed.
	 *
	 * @param footer The footer to set.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &set_footer(const EmbedFooter &footer) {
		m_embed.footer = footer;
		return *this;
	}

	/**
	 * @brief Sets the image of the Embed.
	 *
	 * @param image The image to set.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &set_image(const EmbedImage &image) {
		m_embed.image = image;
		return *this;
	}

	/**
	 * @brief Sets the provider of the Embed.
	 *
	 * @param provider The provider to set.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &set_provider(const EmbedProvider &provider) {
		m_embed.provider = provider;
		return *this;
	}

	/**
	 * @brief Sets the thumbnail of the Embed.
	 *
	 * @param thumbnail The thumbnail to set.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &set_thumbnail(const EmbedThumbnail &thumbnail) {
		m_embed.thumbnail = thumbnail;
		return *this;
	}

	/**
	 * @brief Sets the title of the Embed.
	 *
	 * @param title The title to set.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &set_title(std::string title) {
		m_embed.title = std::move(title);
		return *this;
	}

	/**
	 * @brief Sets the URL of the Embed.
	 *
	 * @param url The URL to set.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &set_url(std::string url) {
		m_embed.url = std::move(url);
		return *this;
	}

	/**
	 * @brief Sets the video of the Embed.
	 *
	 * @param video The video to set.
	 * @return Embed& A reference to the Embed object.
	 */
	EmbedBuilder &set_video(const EmbedVideo &video) {
		m_embed.video = video;
		return *this;
	}

	/**
	 * @brief Builds the Embed object.
	 *
	 * @return Embed The built Embed object.
	 */
	[[nodiscard]] Embed build() const { return m_embed; }

   private:
	/// The Embed object to build.
	Embed m_embed;
};
}  // namespace ekizu

#endif	// EKIZU_EMBED_HPP
