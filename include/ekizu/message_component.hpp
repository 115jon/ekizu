#ifndef EKIZU_MESSAGE_COMPONENT_HPP
#define EKIZU_MESSAGE_COMPONENT_HPP

#include <ekizu/select_options.hpp>
#include <variant>

namespace ekizu {
enum class ComponentType : uint8_t {
	ActionRow = 1,
	Button = 2,
	SelectMenu = 3,
	TextInput = 4,
};

enum class ButtonStyle : uint8_t {
	Primary = 1,
	Secondary = 2,
	Success = 3,
	Danger = 4,
	Link = 5
};

struct Button {
	ButtonStyle style{};
	std::optional<std::string> label;
	std::optional<PartialEmoji> emoji;
	std::optional<std::string> custom_id;
	std::optional<std::string> url;
	bool disabled{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Button &b);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Button &b);

struct ButtonBuilder {
	[[nodiscard]] Button build() const { return m_button; }

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

   private:
	Button m_button;
};

struct SelectOptions {
	std::string label;
	std::string value;
	std::optional<std::string> description;
	std::optional<PartialEmoji> emoji;
	std::optional<bool> default_;
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
		m_options.description = description;
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

struct SelectMenu {
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
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const SelectMenu &s);
EKIZU_EXPORT void from_json(const nlohmann::json &j, SelectMenu &s);

struct SelectMenuBuilder {
	[[nodiscard]] SelectMenu build() const { return m_select_menu; }

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

using ActionRowComponent = std::variant<Button, SelectMenu>;

EKIZU_EXPORT void to_json(nlohmann::json &j, const ActionRowComponent &c);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ActionRowComponent &c);

/**
 * @brief A non-interactive container for component that holds other types of
 * components. You can have up to 5 Action Rows in a message, and Action Rows
 * cannot contain other Action Rows.
 *
 * @see
 * https://discord.com/developers/docs/interactions/message-components#action-rows
 *
 * @param client The client that created this Action Row.
 * @param data The JSON data to create this Action Row with.
 */
struct ActionRow {
	std::vector<ActionRowComponent> components{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const ActionRow &a);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ActionRow &a);

struct ActionRowBuilder {
	[[nodiscard]] ActionRow build() const { return m_action_row; }

	ActionRowBuilder &components(std::vector<ActionRowComponent> components) {
		m_action_row.components = std::move(components);
		return *this;
	}

   private:
	ActionRow m_action_row;
};

using MessageComponent = std::variant<ActionRow, Button, SelectMenu>;

EKIZU_EXPORT void to_json(nlohmann::json &j, const MessageComponent &c);
// TODO: Not how it works.
EKIZU_EXPORT void from_json(const nlohmann::json &j, MessageComponent &c);
}  // namespace ekizu

#endif	// EKIZU_MESSAGE_COMPONENT_HPP
