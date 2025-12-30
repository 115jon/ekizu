#ifndef EKIZU_MESSAGE_COMPONENT_HPP
#define EKIZU_MESSAGE_COMPONENT_HPP

#include <cstdint>
#include <ekizu/select_options.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ekizu {

/**
 * @brief Discord Component Types and Structures.
 *
 * Discord components are interactive or display elements used in messages and
 * modals. This file implements all component types as defined in the Discord
 * API.
 *
 * Component Categories:
 * 1. Layout Components: ActionRow (1), Section (9), Separator (14), Container
 * (17), Label (18)
 * 2. Interactive Components (Message): Button (2), String Select (3), User
 * Select (5), Role Select (6), Mentionable Select (7), Channel Select (8)
 * 3. Interactive Components (Modal): Text Input (4), File Upload (19), + all
 * Select types
 * 4. Content/Display Components: Text Display (10), Thumbnail (11), Media
 * Gallery (12), File (13)
 *
 * Usage:
 * - Message Components: Can be used in message payloads with IS_COMPONENTS_V2
 * flag
 * - Modal Components: Used in MODAL interaction responses (types 4, 18, 19, and
 * selects)
 *
 * @see https://discord.com/developers/docs/interactions/message-components
 * @see
 * https://discord.com/developers/docs/interactions/receiving-and-responding#interaction-object-modal-submit-data-structure
 */
enum class ComponentType : uint8_t {
	ActionRow = 1,
	Button = 2,

	// Select menus
	SelectMenu = 3,	 // String Select (legacy name kept for compatibility)
	TextInput = 4,
	UserSelect = 5,
	RoleSelect = 6,
	MentionableSelect = 7,
	ChannelSelect = 8,

	// Components V2 / display components
	Section = 9,
	TextDisplay = 10,
	Thumbnail = 11,
	MediaGallery = 12,
	File = 13,
	Separator = 14,
	Container = 17,

	// Modal layout + interactive
	Label = 18,
	FileUpload = 19,
};

enum class ButtonStyle : uint8_t {
	Primary = 1,
	Secondary = 2,
	Success = 3,
	Danger = 4,
	Link = 5,

	// NOTE: Some clients/libraries expose Premium-style buttons; if Discord
	// adds additional button styles, they should still round-trip via raw.
};

struct Button {
	/// 32 bit integer used as an optional identifier for component.
	std::optional<uint32_t> id;

	ButtonStyle style{};
	std::optional<std::string> label;
	std::optional<PartialEmoji> emoji;

	/// For non-link buttons (max 100 chars).
	std::optional<std::string> custom_id;

	/// For link buttons.
	std::optional<std::string> url;

	/// Optional SKU for premium/purchase buttons (Snowflake); stored as
	/// uint64_t to avoid requiring additional headers here. Unknown fields are
	/// preserved in raw regardless.
	std::optional<uint64_t> sku_id;

	bool disabled{};

	/// Full raw JSON object for forward compatibility.
	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Button &b);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Button &b);

struct ButtonBuilder {
	[[nodiscard]] Button build() const { return m_button; }

	ButtonBuilder &id(uint32_t id) {
		m_button.id = id;
		return *this;
	}

	ButtonBuilder &style(ButtonStyle style) {
		m_button.style = style;
		return *this;
	}

	ButtonBuilder &label(std::string label) {
		m_button.label = std::move(label);
		return *this;
	}

	ButtonBuilder &emoji(PartialEmoji emoji) {
		m_button.emoji = std::move(emoji);
		return *this;
	}

	ButtonBuilder &custom_id(std::string custom_id) {
		m_button.custom_id = std::move(custom_id);
		return *this;
	}

	ButtonBuilder &url(std::string url) {
		m_button.url = std::move(url);
		return *this;
	}

	ButtonBuilder &sku_id(uint64_t sku_id) {
		m_button.sku_id = sku_id;
		return *this;
	}

	ButtonBuilder &disabled(bool disabled) {
		m_button.disabled = disabled;
		return *this;
	}

   private:
	Button m_button;
};

struct SelectOptions {
	std::string label;
	std::string value;
	std::optional<std::string> description;
	std::optional<PartialEmoji> emoji;
	std::optional<bool> default_;

	/// Full raw JSON object for forward compatibility.
	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const SelectOptions &o);
EKIZU_EXPORT void from_json(const nlohmann::json &j, SelectOptions &o);

struct SelectOptionsBuilder {
	[[nodiscard]] SelectOptions build() const { return m_options; }

	SelectOptionsBuilder &label(std::string label) {
		m_options.label = std::move(label);
		return *this;
	}

	SelectOptionsBuilder &value(std::string value) {
		m_options.value = std::move(value);
		return *this;
	}

	SelectOptionsBuilder &description(std::string description) {
		m_options.description = std::move(description);
		return *this;
	}

	SelectOptionsBuilder &emoji(PartialEmoji emoji) {
		m_options.emoji = std::move(emoji);
		return *this;
	}

	SelectOptionsBuilder &default_(bool default_) {
		m_options.default_ = default_;
		return *this;
	}

   private:
	SelectOptions m_options;
};

/**
 * @brief Default value entry for auto-populated selects.
 *
 * Discord represents the referenced entity using an id plus a "type"
 * discriminator; "type" is stored as a string to remain permissive and
 * forward-compatible.
 *
 * @see https://discord.com/developers/docs/components/reference
 */
struct SelectMenuDefaultValue {
	uint64_t id{};
	std::string type;

	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const SelectMenuDefaultValue &v);
EKIZU_EXPORT void from_json(const nlohmann::json &j, SelectMenuDefaultValue &v);

struct SelectMenuDefaultValueBuilder {
	[[nodiscard]] SelectMenuDefaultValue build() const { return m_value; }

	SelectMenuDefaultValueBuilder &id(uint64_t id) {
		m_value.id = id;
		return *this;
	}

	SelectMenuDefaultValueBuilder &type(std::string type) {
		m_value.type = std::move(type);
		return *this;
	}

   private:
	SelectMenuDefaultValue m_value;
};

/**
 * @brief String Select (ComponentType 3).
 *
 * Kept as SelectMenu for compatibility with existing code, but it is the
 * "String Select" structure in Discord's component reference.
 *
 * @see https://discord.com/developers/docs/components/reference#string-select
 */
struct SelectMenu {
	/// Optional identifier for component.
	std::optional<uint32_t> id;

	/// A unique identifier for the component (max 100 characters).
	std::string custom_id;

	/// The options for the select menu (max 25).
	std::vector<SelectOptions> options;

	/// The placeholder text for the select menu (max 150 characters).
	std::optional<std::string> placeholder;

	/// The minimum number of values that must be selected (default 1, min 0,
	/// max 25).
	std::optional<uint8_t> min_values;

	/// The maximum number of values that can be selected (default 1, max 25).
	std::optional<uint8_t> max_values;

	/// Whether the select menu is disabled (default false).
	std::optional<bool> disabled;

	/// Full raw JSON object for forward compatibility.
	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const SelectMenu &s);
EKIZU_EXPORT void from_json(const nlohmann::json &j, SelectMenu &s);

struct SelectMenuBuilder {
	[[nodiscard]] SelectMenu build() const { return m_select_menu; }

	SelectMenuBuilder &id(uint32_t id) {
		m_select_menu.id = id;
		return *this;
	}

	SelectMenuBuilder &custom_id(std::string custom_id) {
		m_select_menu.custom_id = std::move(custom_id);
		return *this;
	}

	SelectMenuBuilder &options(std::vector<SelectOptions> options) {
		m_select_menu.options = std::move(options);
		return *this;
	}

	SelectMenuBuilder &placeholder(std::string placeholder) {
		m_select_menu.placeholder = std::move(placeholder);
		return *this;
	}

	SelectMenuBuilder &min_values(uint8_t min_values) {
		m_select_menu.min_values = min_values;
		return *this;
	}

	SelectMenuBuilder &max_values(uint8_t max_values) {
		m_select_menu.max_values = max_values;
		return *this;
	}

	SelectMenuBuilder &disabled(bool disabled) {
		m_select_menu.disabled = disabled;
		return *this;
	}

   private:
	SelectMenu m_select_menu;
};

struct UserSelectMenu {
	std::optional<uint32_t> id;
	std::string custom_id;

	std::optional<std::string> placeholder;
	std::optional<uint8_t> min_values;
	std::optional<uint8_t> max_values;
	std::optional<bool> disabled;

	std::optional<std::vector<SelectMenuDefaultValue>> default_values;

	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const UserSelectMenu &s);
EKIZU_EXPORT void from_json(const nlohmann::json &j, UserSelectMenu &s);

struct UserSelectMenuBuilder {
	[[nodiscard]] UserSelectMenu build() const { return m_menu; }

	UserSelectMenuBuilder &id(uint32_t id) {
		m_menu.id = id;
		return *this;
	}

	UserSelectMenuBuilder &custom_id(std::string custom_id) {
		m_menu.custom_id = std::move(custom_id);
		return *this;
	}

	UserSelectMenuBuilder &placeholder(std::string placeholder) {
		m_menu.placeholder = std::move(placeholder);
		return *this;
	}

	UserSelectMenuBuilder &min_values(uint8_t min_values) {
		m_menu.min_values = min_values;
		return *this;
	}

	UserSelectMenuBuilder &max_values(uint8_t max_values) {
		m_menu.max_values = max_values;
		return *this;
	}

	UserSelectMenuBuilder &disabled(bool disabled) {
		m_menu.disabled = disabled;
		return *this;
	}

	UserSelectMenuBuilder &default_values(
		std::vector<SelectMenuDefaultValue> v) {
		m_menu.default_values = std::move(v);
		return *this;
	}

   private:
	UserSelectMenu m_menu;
};

struct RoleSelectMenu {
	std::optional<uint32_t> id;
	std::string custom_id;

	std::optional<std::string> placeholder;
	std::optional<uint8_t> min_values;
	std::optional<uint8_t> max_values;
	std::optional<bool> disabled;

	std::optional<std::vector<SelectMenuDefaultValue>> default_values;

	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const RoleSelectMenu &s);
EKIZU_EXPORT void from_json(const nlohmann::json &j, RoleSelectMenu &s);

struct RoleSelectMenuBuilder {
	[[nodiscard]] RoleSelectMenu build() const { return m_menu; }

	RoleSelectMenuBuilder &id(uint32_t id) {
		m_menu.id = id;
		return *this;
	}

	RoleSelectMenuBuilder &custom_id(std::string custom_id) {
		m_menu.custom_id = std::move(custom_id);
		return *this;
	}

	RoleSelectMenuBuilder &placeholder(std::string placeholder) {
		m_menu.placeholder = std::move(placeholder);
		return *this;
	}

	RoleSelectMenuBuilder &min_values(uint8_t min_values) {
		m_menu.min_values = min_values;
		return *this;
	}

	RoleSelectMenuBuilder &max_values(uint8_t max_values) {
		m_menu.max_values = max_values;
		return *this;
	}

	RoleSelectMenuBuilder &disabled(bool disabled) {
		m_menu.disabled = disabled;
		return *this;
	}

	RoleSelectMenuBuilder &default_values(
		std::vector<SelectMenuDefaultValue> v) {
		m_menu.default_values = std::move(v);
		return *this;
	}

   private:
	RoleSelectMenu m_menu;
};

struct MentionableSelectMenu {
	std::optional<uint32_t> id;
	std::string custom_id;

	std::optional<std::string> placeholder;
	std::optional<uint8_t> min_values;
	std::optional<uint8_t> max_values;
	std::optional<bool> disabled;

	std::optional<std::vector<SelectMenuDefaultValue>> default_values;

	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const MentionableSelectMenu &s);
EKIZU_EXPORT void from_json(const nlohmann::json &j, MentionableSelectMenu &s);

struct MentionableSelectMenuBuilder {
	[[nodiscard]] MentionableSelectMenu build() const { return m_menu; }

	MentionableSelectMenuBuilder &id(uint32_t id) {
		m_menu.id = id;
		return *this;
	}

	MentionableSelectMenuBuilder &custom_id(std::string custom_id) {
		m_menu.custom_id = std::move(custom_id);
		return *this;
	}

	MentionableSelectMenuBuilder &placeholder(std::string placeholder) {
		m_menu.placeholder = std::move(placeholder);
		return *this;
	}

	MentionableSelectMenuBuilder &min_values(uint8_t min_values) {
		m_menu.min_values = min_values;
		return *this;
	}

	MentionableSelectMenuBuilder &max_values(uint8_t max_values) {
		m_menu.max_values = max_values;
		return *this;
	}

	MentionableSelectMenuBuilder &disabled(bool disabled) {
		m_menu.disabled = disabled;
		return *this;
	}

	MentionableSelectMenuBuilder &default_values(
		std::vector<SelectMenuDefaultValue> v) {
		m_menu.default_values = std::move(v);
		return *this;
	}

   private:
	MentionableSelectMenu m_menu;
};

struct ChannelSelectMenu {
	std::optional<uint32_t> id;
	std::string custom_id;

	std::optional<std::string> placeholder;
	std::optional<uint8_t> min_values;
	std::optional<uint8_t> max_values;
	std::optional<bool> disabled;

	/// Array of channel type integers.
	std::optional<std::vector<uint8_t>> channel_types;

	std::optional<std::vector<SelectMenuDefaultValue>> default_values;

	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const ChannelSelectMenu &s);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ChannelSelectMenu &s);

struct ChannelSelectMenuBuilder {
	[[nodiscard]] ChannelSelectMenu build() const { return m_menu; }

	ChannelSelectMenuBuilder &id(uint32_t id) {
		m_menu.id = id;
		return *this;
	}

	ChannelSelectMenuBuilder &custom_id(std::string custom_id) {
		m_menu.custom_id = std::move(custom_id);
		return *this;
	}

	ChannelSelectMenuBuilder &placeholder(std::string placeholder) {
		m_menu.placeholder = std::move(placeholder);
		return *this;
	}

	ChannelSelectMenuBuilder &min_values(uint8_t min_values) {
		m_menu.min_values = min_values;
		return *this;
	}

	ChannelSelectMenuBuilder &max_values(uint8_t max_values) {
		m_menu.max_values = max_values;
		return *this;
	}

	ChannelSelectMenuBuilder &disabled(bool disabled) {
		m_menu.disabled = disabled;
		return *this;
	}

	ChannelSelectMenuBuilder &channel_types(std::vector<uint8_t> types) {
		m_menu.channel_types = std::move(types);
		return *this;
	}

	ChannelSelectMenuBuilder &default_values(
		std::vector<SelectMenuDefaultValue> v) {
		m_menu.default_values = std::move(v);
		return *this;
	}

   private:
	ChannelSelectMenu m_menu;
};

enum class TextInputStyle : uint8_t { Short = 1, Paragraph = 2 };

/**
 * @brief Text input component (ComponentType 4).
 *
 * Primarily used in modals (inside Action Rows).
 *
 * @see https://discord.com/developers/docs/components/reference#text-input
 */
struct TextInput {
	std::optional<uint32_t> id;

	/// Developer-defined identifier for the input; max 100 chars.
	std::string custom_id;

	/// The TextInput style (short/paragraph).
	TextInputStyle style{};

	/// Label for the input; max 45 chars.
	std::string label;

	/// Minimum input length; min 0, max 4000.
	std::optional<uint16_t> min_length;

	/// Maximum input length; min 1, max 4000.
	std::optional<uint16_t> max_length;

	/// Whether this component is required.
	std::optional<bool> required;

	/// Prefilled value.
	std::optional<std::string> value;

	/// Placeholder text; max 100 chars.
	std::optional<std::string> placeholder;

	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const TextInput &t);
EKIZU_EXPORT void from_json(const nlohmann::json &j, TextInput &t);

struct TextInputBuilder {
	[[nodiscard]] TextInput build() const { return m_input; }

	TextInputBuilder &id(uint32_t id) {
		m_input.id = id;
		return *this;
	}

	TextInputBuilder &custom_id(std::string custom_id) {
		m_input.custom_id = std::move(custom_id);
		return *this;
	}

	TextInputBuilder &style(TextInputStyle style) {
		m_input.style = style;
		return *this;
	}

	TextInputBuilder &label(std::string label) {
		m_input.label = std::move(label);
		return *this;
	}

	TextInputBuilder &min_length(uint16_t v) {
		m_input.min_length = v;
		return *this;
	}

	TextInputBuilder &max_length(uint16_t v) {
		m_input.max_length = v;
		return *this;
	}

	TextInputBuilder &required(bool v) {
		m_input.required = v;
		return *this;
	}

	TextInputBuilder &value(std::string v) {
		m_input.value = std::move(v);
		return *this;
	}

	TextInputBuilder &placeholder(std::string v) {
		m_input.placeholder = std::move(v);
		return *this;
	}

   private:
	TextInput m_input;
};

// ----------------------------
// Components V2 (display/layout)
// ----------------------------

/**
 * @brief Unfurled media item.
 *
 * For outgoing Components V2, url is the primary field.
 * For incoming payloads, additional fields may exist and will be preserved
 * in raw.
 *
 * @see https://discord.com/developers/docs/components/reference
 */
struct UnfurledMediaItem {
	std::string url;
	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const UnfurledMediaItem &m);
EKIZU_EXPORT void from_json(const nlohmann::json &j, UnfurledMediaItem &m);

struct UnfurledMediaItemBuilder {
	[[nodiscard]] UnfurledMediaItem build() const { return m_item; }

	UnfurledMediaItemBuilder &url(std::string url) {
		m_item.url = std::move(url);
		return *this;
	}

   private:
	UnfurledMediaItem m_item;
};

/**
 * @brief Markdown text block (ComponentType 10).
 */
struct TextDisplay {
	std::optional<uint32_t> id;
	std::string content;
	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const TextDisplay &t);
EKIZU_EXPORT void from_json(const nlohmann::json &j, TextDisplay &t);

struct TextDisplayBuilder {
	[[nodiscard]] TextDisplay build() const { return m_text; }

	TextDisplayBuilder &id(uint32_t id) {
		m_text.id = id;
		return *this;
	}

	TextDisplayBuilder &content(std::string content) {
		m_text.content = std::move(content);
		return *this;
	}

   private:
	TextDisplay m_text;
};

/**
 * @brief Thumbnail accessory image (ComponentType 11).
 */
struct Thumbnail {
	std::optional<uint32_t> id;
	UnfurledMediaItem media;
	std::optional<std::string> description;
	std::optional<bool> spoiler;
	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Thumbnail &t);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Thumbnail &t);

struct ThumbnailBuilder {
	[[nodiscard]] Thumbnail build() const { return m_thumb; }

	ThumbnailBuilder &id(uint32_t id) {
		m_thumb.id = id;
		return *this;
	}

	ThumbnailBuilder &media(UnfurledMediaItem media) {
		m_thumb.media = std::move(media);
		return *this;
	}

	ThumbnailBuilder &description(std::string description) {
		m_thumb.description = std::move(description);
		return *this;
	}

	ThumbnailBuilder &spoiler(bool spoiler) {
		m_thumb.spoiler = spoiler;
		return *this;
	}

   private:
	Thumbnail m_thumb;
};

using SectionAccessory = std::variant<Button, Thumbnail>;

/**
 * @brief Section layout component (ComponentType 9).
 *
 * A Section is a container displaying text content alongside an accessory.
 */
struct Section {
	std::optional<uint32_t> id;

	/// Section contains TextDisplay blocks.
	std::vector<TextDisplay> components;

	/// Either a Button or a Thumbnail.
	std::optional<SectionAccessory> accessory;

	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Section &s);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Section &s);

struct SectionBuilder {
	[[nodiscard]] Section build() const { return m_section; }

	SectionBuilder &id(uint32_t id) {
		m_section.id = id;
		return *this;
	}

	SectionBuilder &components(std::vector<TextDisplay> components) {
		m_section.components = std::move(components);
		return *this;
	}

	SectionBuilder &add(TextDisplay component) {
		m_section.components.emplace_back(std::move(component));
		return *this;
	}

	SectionBuilder &accessory(SectionAccessory accessory) {
		m_section.accessory = std::move(accessory);
		return *this;
	}

	SectionBuilder &accessory_button(Button b) {
		m_section.accessory = SectionAccessory{std::move(b)};
		return *this;
	}

	SectionBuilder &accessory_thumbnail(Thumbnail t) {
		m_section.accessory = SectionAccessory{std::move(t)};
		return *this;
	}

   private:
	Section m_section;
};

/**
 * @brief Media gallery item (used by MediaGallery).
 */
struct MediaGalleryItem {
	UnfurledMediaItem media;
	std::optional<std::string> description;
	std::optional<bool> spoiler;
	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const MediaGalleryItem &m);
EKIZU_EXPORT void from_json(const nlohmann::json &j, MediaGalleryItem &m);

struct MediaGalleryItemBuilder {
	[[nodiscard]] MediaGalleryItem build() const { return m_item; }

	MediaGalleryItemBuilder &media(UnfurledMediaItem media) {
		m_item.media = std::move(media);
		return *this;
	}

	MediaGalleryItemBuilder &description(std::string description) {
		m_item.description = std::move(description);
		return *this;
	}

	MediaGalleryItemBuilder &spoiler(bool spoiler) {
		m_item.spoiler = spoiler;
		return *this;
	}

   private:
	MediaGalleryItem m_item;
};

/**
 * @brief Media Gallery (ComponentType 12).
 *
 * @see https://discord.com/developers/docs/components/reference#media-gallery
 */
struct MediaGallery {
	std::optional<uint32_t> id;
	std::vector<MediaGalleryItem> items;
	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const MediaGallery &m);
EKIZU_EXPORT void from_json(const nlohmann::json &j, MediaGallery &m);

struct MediaGalleryBuilder {
	[[nodiscard]] MediaGallery build() const { return m_gallery; }

	MediaGalleryBuilder &id(uint32_t id) {
		m_gallery.id = id;
		return *this;
	}

	MediaGalleryBuilder &items(std::vector<MediaGalleryItem> items) {
		m_gallery.items = std::move(items);
		return *this;
	}

	MediaGalleryBuilder &add(MediaGalleryItem item) {
		m_gallery.items.emplace_back(std::move(item));
		return *this;
	}

   private:
	MediaGallery m_gallery;
};

/**
 * @brief File display component (ComponentType 13).
 *
 * This references an uploaded attachment using attachment://<filename>.
 *
 * @see https://discord.com/developers/docs/components/reference#file
 */
struct File {
	std::optional<uint32_t> id;
	UnfurledMediaItem file;
	std::optional<bool> spoiler;
	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const File &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j, File &f);

struct FileBuilder {
	[[nodiscard]] File build() const { return m_file; }

	FileBuilder &id(uint32_t id) {
		m_file.id = id;
		return *this;
	}

	FileBuilder &file(UnfurledMediaItem file) {
		m_file.file = std::move(file);
		return *this;
	}

	FileBuilder &spoiler(bool spoiler) {
		m_file.spoiler = spoiler;
		return *this;
	}

   private:
	File m_file;
};

/**
 * @brief Separator layout component (ComponentType 14).
 *
 * @see https://discord.com/developers/docs/components/reference#separator
 */
struct Separator {
	std::optional<uint32_t> id;

	/// Whether a visual divider line should be displayed.
	std::optional<bool> divider;

	/// Size of separator padding. Stored as uint8_t to stay permissive.
	std::optional<uint8_t> spacing;

	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Separator &s);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Separator &s);

struct SeparatorBuilder {
	[[nodiscard]] Separator build() const { return m_sep; }

	SeparatorBuilder &id(uint32_t id) {
		m_sep.id = id;
		return *this;
	}

	SeparatorBuilder &divider(bool divider) {
		m_sep.divider = divider;
		return *this;
	}

	SeparatorBuilder &spacing(uint8_t spacing) {
		m_sep.spacing = spacing;
		return *this;
	}

   private:
	Separator m_sep;
};

/**
 * @brief File upload component (ComponentType 19).
 *
 * Used in modals to allow uploading 0..10 items.
 *
 * @see https://discord.com/developers/docs/components/reference#file-upload
 */
struct FileUpload {
	std::optional<uint32_t> id;

	/// Id for the file upload; max 100 characters.
	std::string custom_id;

	/// Minimum number of items that must be uploaded (defaults to 1); min 0,
	/// max 10.
	std::optional<uint8_t> min_values;

	/// Maximum number of items that can be uploaded (defaults to 1); max 10.
	std::optional<uint8_t> max_values;

	/// Whether the file upload requires files to be uploaded before submitting
	/// the modal (defaults to true).
	std::optional<bool> required;

	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const FileUpload &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j, FileUpload &f);

struct FileUploadBuilder {
	[[nodiscard]] FileUpload build() const { return m_upload; }

	FileUploadBuilder &id(uint32_t id) {
		m_upload.id = id;
		return *this;
	}

	FileUploadBuilder &custom_id(std::string custom_id) {
		m_upload.custom_id = std::move(custom_id);
		return *this;
	}

	FileUploadBuilder &min_values(uint8_t v) {
		m_upload.min_values = v;
		return *this;
	}

	FileUploadBuilder &max_values(uint8_t v) {
		m_upload.max_values = v;
		return *this;
	}

	FileUploadBuilder &required(bool v) {
		m_upload.required = v;
		return *this;
	}

   private:
	FileUpload m_upload;
};

/**
 * @brief Modal Label container (ComponentType 18).
 *
 * Discord models Label as a layout container that associates label/description
 * with a single child input component.
 *
 * Unknown keys are preserved in raw.
 *
 * @see https://discord.com/developers/docs/components/reference#label
 */
struct Label {
	std::optional<uint32_t> id;

	std::string label;
	std::optional<std::string> description;

	/// Child component. This library supports commonly-documented modal inputs,
	/// but preserves unknown structures through raw.
	std::variant<TextInput, FileUpload, SelectMenu, UserSelectMenu,
				 RoleSelectMenu, MentionableSelectMenu, ChannelSelectMenu>
		component;

	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Label &l);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Label &l);

struct LabelBuilder {
	using ComponentVariant =
		std::variant<TextInput, FileUpload, SelectMenu, UserSelectMenu,
					 RoleSelectMenu, MentionableSelectMenu, ChannelSelectMenu>;

	[[nodiscard]] Label build() const { return m_label; }

	LabelBuilder &id(uint32_t id) {
		m_label.id = id;
		return *this;
	}

	LabelBuilder &label(std::string label) {
		m_label.label = std::move(label);
		return *this;
	}

	LabelBuilder &description(std::string description) {
		m_label.description = std::move(description);
		return *this;
	}

	LabelBuilder &component(ComponentVariant component) {
		m_label.component = std::move(component);
		return *this;
	}

	LabelBuilder &component(TextInput v) {
		return component(ComponentVariant{std::move(v)});
	}
	LabelBuilder &component(FileUpload v) {
		return component(ComponentVariant{std::move(v)});
	}
	LabelBuilder &component(SelectMenu v) {
		return component(ComponentVariant{std::move(v)});
	}
	LabelBuilder &component(UserSelectMenu v) {
		return component(ComponentVariant{std::move(v)});
	}
	LabelBuilder &component(RoleSelectMenu v) {
		return component(ComponentVariant{std::move(v)});
	}
	LabelBuilder &component(MentionableSelectMenu v) {
		return component(ComponentVariant{std::move(v)});
	}
	LabelBuilder &component(ChannelSelectMenu v) {
		return component(ComponentVariant{std::move(v)});
	}

   private:
	Label m_label;
};

/**
 * @brief Forward-compat unknown component.
 *
 * This preserves both:
 * - Unknown component types.
 * - Known types with new fields that this library version does not model.
 */
struct UnknownMessageComponent {
	uint8_t type{};
	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const UnknownMessageComponent &u);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							UnknownMessageComponent &u);

/**
 * @brief ActionRow component list item.
 *
 * ActionRows can contain:
 * - In messages: Buttons and select menus.
 * - In modals: inputs like TextInput and FileUpload.
 */
using ActionRowComponent =
	std::variant<Button, SelectMenu, UserSelectMenu, RoleSelectMenu,
				 MentionableSelectMenu, ChannelSelectMenu, TextInput,
				 FileUpload>;

EKIZU_EXPORT void to_json(nlohmann::json &j, const ActionRowComponent &c);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ActionRowComponent &c);

/**
 * @brief An Action Row container.
 *
 * @see
 * https://discord.com/developers/docs/interactions/message-components#action-rows
 */
struct ActionRow {
	std::optional<uint32_t> id;
	std::vector<ActionRowComponent> components;
	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const ActionRow &a);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ActionRow &a);

struct ActionRowBuilder {
	[[nodiscard]] ActionRow build() const { return m_action_row; }

	ActionRowBuilder &id(uint32_t id) {
		m_action_row.id = id;
		return *this;
	}

	ActionRowBuilder &components(std::vector<ActionRowComponent> components) {
		m_action_row.components = std::move(components);
		return *this;
	}

	ActionRowBuilder &add(ActionRowComponent component) {
		m_action_row.components.emplace_back(std::move(component));
		return *this;
	}

	ActionRowBuilder &add_button(Button b) {
		return add(ActionRowComponent{std::move(b)});
	}
	ActionRowBuilder &add_select_menu(SelectMenu s) {
		return add(ActionRowComponent{std::move(s)});
	}
	ActionRowBuilder &add_user_select(UserSelectMenu s) {
		return add(ActionRowComponent{std::move(s)});
	}
	ActionRowBuilder &add_role_select(RoleSelectMenu s) {
		return add(ActionRowComponent{std::move(s)});
	}
	ActionRowBuilder &add_mentionable_select(MentionableSelectMenu s) {
		return add(ActionRowComponent{std::move(s)});
	}
	ActionRowBuilder &add_channel_select(ChannelSelectMenu s) {
		return add(ActionRowComponent{std::move(s)});
	}
	ActionRowBuilder &add_text_input(TextInput t) {
		return add(ActionRowComponent{std::move(t)});
	}
	ActionRowBuilder &add_file_upload(FileUpload f) {
		return add(ActionRowComponent{std::move(f)});
	}

   private:
	ActionRow m_action_row;
};

/**
 * @brief Container layout component (ComponentType 17).
 *
 * Container can contain mixed component types.
 * This library also allows UnknownMessageComponent as a child so nested
 * unknown components can be preserved.
 *
 * @see https://discord.com/developers/docs/components/reference#container
 */
struct Container {
	std::optional<uint32_t> id;

	/// Accent color for the container.
	std::optional<uint32_t> accent_color;

	/// Whether the container content is marked as spoiler.
	std::optional<bool> spoiler;

	using Child =
		std::variant<ActionRow, Section, TextDisplay, Thumbnail, MediaGallery,
					 File, Separator, Button, SelectMenu, UserSelectMenu,
					 RoleSelectMenu, MentionableSelectMenu, ChannelSelectMenu,
					 UnknownMessageComponent>;

	std::vector<Child> components;

	nlohmann::json raw;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Container &c);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Container &c);

struct ContainerBuilder {
	[[nodiscard]] Container build() const { return m_container; }

	ContainerBuilder &id(uint32_t id) {
		m_container.id = id;
		return *this;
	}

	ContainerBuilder &accent_color(uint32_t color) {
		m_container.accent_color = color;
		return *this;
	}

	ContainerBuilder &spoiler(bool spoiler) {
		m_container.spoiler = spoiler;
		return *this;
	}

	ContainerBuilder &components(std::vector<Container::Child> components) {
		m_container.components = std::move(components);
		return *this;
	}

	ContainerBuilder &add(Container::Child child) {
		m_container.components.emplace_back(std::move(child));
		return *this;
	}

   private:
	Container m_container;
};

using MessageComponent =
	std::variant<ActionRow, Button, SelectMenu, UserSelectMenu, RoleSelectMenu,
				 MentionableSelectMenu, ChannelSelectMenu, TextInput, Section,
				 TextDisplay, Thumbnail, MediaGallery, File, Separator,
				 Container, Label, FileUpload, UnknownMessageComponent>;

EKIZU_EXPORT void to_json(nlohmann::json &j, const MessageComponent &c);
EKIZU_EXPORT void from_json(const nlohmann::json &j, MessageComponent &c);

}  // namespace ekizu

#endif	// EKIZU_MESSAGE_COMPONENT_HPP
