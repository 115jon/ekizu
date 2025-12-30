#include <ekizu/json_util.hpp>
#include <ekizu/message_component.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

static void ensure_object(nlohmann::json &j) {
	if (!j.is_object()) { j = nlohmann::json::object(); }
}

void to_json(nlohmann::json &j, const Button &b) {
	j = b.raw;
	ensure_object(j);

	serialize(j, "type", ComponentType::Button);
	serialize(j, "id", b.id);
	serialize(j, "style", b.style);
	serialize(j, "label", b.label);
	serialize(j, "emoji", b.emoji);
	serialize(j, "custom_id", b.custom_id);
	serialize(j, "url", b.url);
	serialize(j, "sku_id", b.sku_id);
	serialize(j, "disabled", b.disabled);
}

void from_json(const nlohmann::json &j, Button &b) {
	b.raw = j;

	deserialize(j, "id", b.id);
	deserialize(j, "style", b.style);
	deserialize(j, "label", b.label);
	deserialize(j, "emoji", b.emoji);
	deserialize(j, "custom_id", b.custom_id);
	deserialize(j, "url", b.url);
	deserialize(j, "sku_id", b.sku_id);
	deserialize(j, "disabled", b.disabled);
}

void to_json(nlohmann::json &j, const SelectOptions &o) {
	j = o.raw;
	ensure_object(j);

	serialize(j, "label", o.label);
	serialize(j, "value", o.value);
	serialize(j, "description", o.description);
	serialize(j, "emoji", o.emoji);
	serialize(j, "default", o.default_);
}

void from_json(const nlohmann::json &j, SelectOptions &o) {
	o.raw = j;

	deserialize(j, "label", o.label);
	deserialize(j, "value", o.value);
	deserialize(j, "description", o.description);
	deserialize(j, "emoji", o.emoji);
	deserialize(j, "default", o.default_);
}

void to_json(nlohmann::json &j, const SelectMenuDefaultValue &v) {
	j = v.raw;
	ensure_object(j);

	serialize(j, "id", v.id);
	serialize(j, "type", v.type);
}

void from_json(const nlohmann::json &j, SelectMenuDefaultValue &v) {
	v.raw = j;

	deserialize(j, "id", v.id);
	deserialize(j, "type", v.type);
}

static void serialize_select_common(
	nlohmann::json &j, ComponentType type, const std::optional<uint32_t> &id,
	const std::string &custom_id, const std::optional<std::string> &placeholder,
	const std::optional<uint8_t> &min_values,
	const std::optional<uint8_t> &max_values,
	const std::optional<bool> &disabled,
	const std::optional<std::vector<SelectMenuDefaultValue>> &default_values) {
	serialize(j, "type", type);
	serialize(j, "id", id);
	serialize(j, "custom_id", custom_id);
	serialize(j, "placeholder", placeholder);
	serialize(j, "min_values", min_values);
	serialize(j, "max_values", max_values);
	serialize(j, "disabled", disabled);
	serialize(j, "default_values", default_values);
}

static void deserialize_select_common(
	const nlohmann::json &j, const char * /*type_name*/,
	std::optional<uint32_t> &id, std::string &custom_id,
	std::optional<std::string> &placeholder, std::optional<uint8_t> &min_values,
	std::optional<uint8_t> &max_values, std::optional<bool> &disabled,
	std::optional<std::vector<SelectMenuDefaultValue>> *default_values) {
	deserialize(j, "id", id);
	deserialize(j, "custom_id", custom_id);
	deserialize(j, "placeholder", placeholder);
	deserialize(j, "min_values", min_values);
	deserialize(j, "max_values", max_values);
	deserialize(j, "disabled", disabled);
	if (default_values != nullptr) {
		deserialize(j, "default_values", *default_values);
	}
}

void to_json(nlohmann::json &j, const SelectMenu &s) {
	j = s.raw;
	ensure_object(j);

	serialize_select_common(
		j, ComponentType::SelectMenu, s.id, s.custom_id, s.placeholder,
		s.min_values, s.max_values, s.disabled, std::nullopt);
	serialize(j, "options", s.options);
}

void from_json(const nlohmann::json &j, SelectMenu &s) {
	s.raw = j;

	deserialize_select_common(j, "SelectMenu", s.id, s.custom_id, s.placeholder,
							  s.min_values, s.max_values, s.disabled, nullptr);
	deserialize(j, "options", s.options);
}

void to_json(nlohmann::json &j, const UserSelectMenu &s) {
	j = s.raw;
	ensure_object(j);
	serialize_select_common(
		j, ComponentType::UserSelect, s.id, s.custom_id, s.placeholder,
		s.min_values, s.max_values, s.disabled, s.default_values);
}

void from_json(const nlohmann::json &j, UserSelectMenu &s) {
	s.raw = j;
	deserialize_select_common(
		j, "UserSelectMenu", s.id, s.custom_id, s.placeholder, s.min_values,
		s.max_values, s.disabled, &s.default_values);
}

void to_json(nlohmann::json &j, const RoleSelectMenu &s) {
	j = s.raw;
	ensure_object(j);
	serialize_select_common(
		j, ComponentType::RoleSelect, s.id, s.custom_id, s.placeholder,
		s.min_values, s.max_values, s.disabled, s.default_values);
}

void from_json(const nlohmann::json &j, RoleSelectMenu &s) {
	s.raw = j;
	deserialize_select_common(
		j, "RoleSelectMenu", s.id, s.custom_id, s.placeholder, s.min_values,
		s.max_values, s.disabled, &s.default_values);
}

void to_json(nlohmann::json &j, const MentionableSelectMenu &s) {
	j = s.raw;
	ensure_object(j);
	serialize_select_common(
		j, ComponentType::MentionableSelect, s.id, s.custom_id, s.placeholder,
		s.min_values, s.max_values, s.disabled, s.default_values);
}

void from_json(const nlohmann::json &j, MentionableSelectMenu &s) {
	s.raw = j;
	deserialize_select_common(
		j, "MentionableSelectMenu", s.id, s.custom_id, s.placeholder,
		s.min_values, s.max_values, s.disabled, &s.default_values);
}

void to_json(nlohmann::json &j, const ChannelSelectMenu &s) {
	j = s.raw;
	ensure_object(j);

	serialize_select_common(
		j, ComponentType::ChannelSelect, s.id, s.custom_id, s.placeholder,
		s.min_values, s.max_values, s.disabled, s.default_values);
	serialize(j, "channel_types", s.channel_types);
}

void from_json(const nlohmann::json &j, ChannelSelectMenu &s) {
	s.raw = j;

	deserialize_select_common(
		j, "ChannelSelectMenu", s.id, s.custom_id, s.placeholder, s.min_values,
		s.max_values, s.disabled, &s.default_values);
	deserialize(j, "channel_types", s.channel_types);
}

void to_json(nlohmann::json &j, const TextInput &t) {
	j = t.raw;
	ensure_object(j);

	serialize(j, "type", ComponentType::TextInput);
	serialize(j, "id", t.id);
	serialize(j, "custom_id", t.custom_id);
	serialize(j, "style", t.style);
	serialize(j, "label", t.label);
	serialize(j, "min_length", t.min_length);
	serialize(j, "max_length", t.max_length);
	serialize(j, "required", t.required);
	serialize(j, "value", t.value);
	serialize(j, "placeholder", t.placeholder);
}

void from_json(const nlohmann::json &j, TextInput &t) {
	t.raw = j;

	deserialize(j, "id", t.id);
	deserialize(j, "custom_id", t.custom_id);
	deserialize(j, "style", t.style);
	deserialize(j, "label", t.label);
	deserialize(j, "min_length", t.min_length);
	deserialize(j, "max_length", t.max_length);
	deserialize(j, "required", t.required);
	deserialize(j, "value", t.value);
	deserialize(j, "placeholder", t.placeholder);
}

void to_json(nlohmann::json &j, const ActionRowComponent &c) {
	std::visit([&j](const auto &v) { to_json(j, v); }, c);
}

void from_json(const nlohmann::json &j, ActionRowComponent &c) {
	if (!(json_util::not_null_all(j, "type") && j["type"].is_number())) {
		return;
	}

	const auto type_u8 = j["type"].get<uint8_t>();
	switch (static_cast<ComponentType>(type_u8)) {
		case ComponentType::Button: {
			Button b;
			from_json(j, b);
			c.emplace<Button>(std::move(b));
			break;
		}
		case ComponentType::SelectMenu: {
			SelectMenu s;
			from_json(j, s);
			c.emplace<SelectMenu>(std::move(s));
			break;
		}
		case ComponentType::UserSelect: {
			UserSelectMenu s;
			from_json(j, s);
			c.emplace<UserSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::RoleSelect: {
			RoleSelectMenu s;
			from_json(j, s);
			c.emplace<RoleSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::MentionableSelect: {
			MentionableSelectMenu s;
			from_json(j, s);
			c.emplace<MentionableSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::ChannelSelect: {
			ChannelSelectMenu s;
			from_json(j, s);
			c.emplace<ChannelSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::TextInput: {
			TextInput t;
			from_json(j, t);
			c.emplace<TextInput>(std::move(t));
			break;
		}
		case ComponentType::FileUpload: {
			FileUpload f;
			from_json(j, f);
			c.emplace<FileUpload>(std::move(f));
			break;
		}
		default: {
			break;
		}
	}
}

void to_json(nlohmann::json &j, const ActionRow &a) {
	j = a.raw;
	ensure_object(j);

	serialize(j, "type", ComponentType::ActionRow);
	serialize(j, "id", a.id);
	serialize(j, "components", a.components);
}

void from_json(const nlohmann::json &j, ActionRow &a) {
	a.raw = j;

	deserialize(j, "id", a.id);
	deserialize(j, "components", a.components);
}

// ----------------------------
// Components V2 (display/layout)
// ----------------------------

void to_json(nlohmann::json &j, const UnfurledMediaItem &m) {
	j = m.raw;
	ensure_object(j);

	serialize(j, "url", m.url);
}

void from_json(const nlohmann::json &j, UnfurledMediaItem &m) {
	m.raw = j;
	deserialize(j, "url", m.url);
}

void to_json(nlohmann::json &j, const TextDisplay &t) {
	j = t.raw;
	ensure_object(j);

	serialize(j, "type", ComponentType::TextDisplay);
	serialize(j, "id", t.id);
	serialize(j, "content", t.content);
}

void from_json(const nlohmann::json &j, TextDisplay &t) {
	t.raw = j;

	deserialize(j, "id", t.id);
	deserialize(j, "content", t.content);
}

void to_json(nlohmann::json &j, const Thumbnail &t) {
	j = t.raw;
	ensure_object(j);

	serialize(j, "type", ComponentType::Thumbnail);
	serialize(j, "id", t.id);
	serialize(j, "media", t.media);
	serialize(j, "description", t.description);
	serialize(j, "spoiler", t.spoiler);
}

void from_json(const nlohmann::json &j, Thumbnail &t) {
	t.raw = j;

	deserialize(j, "id", t.id);
	deserialize(j, "media", t.media);
	deserialize(j, "description", t.description);
	deserialize(j, "spoiler", t.spoiler);
}

void to_json(nlohmann::json &j, const Section &s) {
	j = s.raw;
	ensure_object(j);

	serialize(j, "type", ComponentType::Section);
	serialize(j, "id", s.id);
	serialize(j, "components", s.components);

	if (s.accessory) {
		nlohmann::json accessory_json;
		std::visit(
			[&accessory_json](const auto &v) { to_json(accessory_json, v); },
			*s.accessory);
		j["accessory"] = std::move(accessory_json);
	}
}

void from_json(const nlohmann::json &j, Section &s) {
	s.raw = j;

	deserialize(j, "id", s.id);
	deserialize(j, "components", s.components);

	if (json_util::not_null_all(j, "accessory") && j["accessory"].is_object()) {
		const auto &a = j["accessory"];
		if (json_util::not_null_all(a, "type") && a["type"].is_number()) {
			switch (a["type"].get<ComponentType>()) {
				case ComponentType::Button: {
					Button b;
					from_json(a, b);
					s.accessory = SectionAccessory{std::move(b)};
					break;
				}
				case ComponentType::Thumbnail: {
					Thumbnail t;
					from_json(a, t);
					s.accessory = SectionAccessory{std::move(t)};
					break;
				}
				default: {
					break;
				}
			}
		}
	}
}

void to_json(nlohmann::json &j, const MediaGalleryItem &m) {
	j = m.raw;
	ensure_object(j);

	serialize(j, "media", m.media);
	serialize(j, "description", m.description);
	serialize(j, "spoiler", m.spoiler);
}

void from_json(const nlohmann::json &j, MediaGalleryItem &m) {
	m.raw = j;

	deserialize(j, "media", m.media);
	deserialize(j, "description", m.description);
	deserialize(j, "spoiler", m.spoiler);
}

void to_json(nlohmann::json &j, const MediaGallery &m) {
	j = m.raw;
	ensure_object(j);

	serialize(j, "type", ComponentType::MediaGallery);
	serialize(j, "id", m.id);
	serialize(j, "items", m.items);
}

void from_json(const nlohmann::json &j, MediaGallery &m) {
	m.raw = j;

	deserialize(j, "id", m.id);
	deserialize(j, "items", m.items);
}

void to_json(nlohmann::json &j, const File &f) {
	j = f.raw;
	ensure_object(j);

	serialize(j, "type", ComponentType::File);
	serialize(j, "id", f.id);
	serialize(j, "file", f.file);
	serialize(j, "spoiler", f.spoiler);
}

void from_json(const nlohmann::json &j, File &f) {
	f.raw = j;

	deserialize(j, "id", f.id);
	deserialize(j, "file", f.file);
	deserialize(j, "spoiler", f.spoiler);
}

void to_json(nlohmann::json &j, const Separator &s) {
	j = s.raw;
	ensure_object(j);

	serialize(j, "type", ComponentType::Separator);
	serialize(j, "id", s.id);
	serialize(j, "divider", s.divider);
	serialize(j, "spacing", s.spacing);
}

void from_json(const nlohmann::json &j, Separator &s) {
	s.raw = j;

	deserialize(j, "id", s.id);
	deserialize(j, "divider", s.divider);
	deserialize(j, "spacing", s.spacing);
}

void to_json(nlohmann::json &j, const FileUpload &f) {
	j = f.raw;
	ensure_object(j);

	serialize(j, "type", ComponentType::FileUpload);
	serialize(j, "id", f.id);
	serialize(j, "custom_id", f.custom_id);
	serialize(j, "min_values", f.min_values);
	serialize(j, "max_values", f.max_values);
	serialize(j, "required", f.required);
}

void from_json(const nlohmann::json &j, FileUpload &f) {
	f.raw = j;

	deserialize(j, "id", f.id);
	deserialize(j, "custom_id", f.custom_id);
	deserialize(j, "min_values", f.min_values);
	deserialize(j, "max_values", f.max_values);
	deserialize(j, "required", f.required);
}

static void label_component_to_json(nlohmann::json &j,
									const decltype(Label::component) &c) {
	std::visit([&j](const auto &v) { to_json(j, v); }, c);
}

static void label_component_from_json(const nlohmann::json &j,
									  decltype(Label::component) &c) {
	if (!(json_util::not_null_all(j, "type") && j["type"].is_number())) {
		return;
	}

	const auto type_u8 = j["type"].get<uint8_t>();
	switch (static_cast<ComponentType>(type_u8)) {
		case ComponentType::TextInput: {
			TextInput t;
			from_json(j, t);
			c.emplace<TextInput>(std::move(t));
			break;
		}
		case ComponentType::FileUpload: {
			FileUpload f;
			from_json(j, f);
			c.emplace<FileUpload>(std::move(f));
			break;
		}
		case ComponentType::SelectMenu: {
			SelectMenu s;
			from_json(j, s);
			c.emplace<SelectMenu>(std::move(s));
			break;
		}
		case ComponentType::UserSelect: {
			UserSelectMenu s;
			from_json(j, s);
			c.emplace<UserSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::RoleSelect: {
			RoleSelectMenu s;
			from_json(j, s);
			c.emplace<RoleSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::MentionableSelect: {
			MentionableSelectMenu s;
			from_json(j, s);
			c.emplace<MentionableSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::ChannelSelect: {
			ChannelSelectMenu s;
			from_json(j, s);
			c.emplace<ChannelSelectMenu>(std::move(s));
			break;
		}
		default: {
			break;
		}
	}
}

void to_json(nlohmann::json &j, const Label &l) {
	j = l.raw;
	ensure_object(j);

	serialize(j, "type", ComponentType::Label);
	serialize(j, "id", l.id);
	serialize(j, "label", l.label);
	serialize(j, "description", l.description);

	nlohmann::json child;
	label_component_to_json(child, l.component);
	j["component"] = std::move(child);
}

void from_json(const nlohmann::json &j, Label &l) {
	l.raw = j;

	deserialize(j, "id", l.id);
	deserialize(j, "label", l.label);
	deserialize(j, "description", l.description);

	if (json_util::not_null_all(j, "component") && j["component"].is_object()) {
		label_component_from_json(j["component"], l.component);
	}
}

void to_json(nlohmann::json &j, const UnknownMessageComponent &u) {
	j = u.raw;
	ensure_object(j);

	if (!j.contains("type")) { j["type"] = u.type; }
}

void from_json(const nlohmann::json &j, UnknownMessageComponent &u) {
	u.raw = j;
	if (json_util::not_null_all(j, "type") && j["type"].is_number()) {
		u.type = j["type"].get<uint8_t>();
	}
}

void to_json(nlohmann::json &j, const Container &c) {
	j = c.raw;
	ensure_object(j);

	serialize(j, "type", ComponentType::Container);
	serialize(j, "id", c.id);
	serialize(j, "accent_color", c.accent_color);
	serialize(j, "spoiler", c.spoiler);

	nlohmann::json comps = nlohmann::json::array();
	for (const auto &comp : c.components) {
		nlohmann::json elem;
		std::visit([&elem](const auto &v) { to_json(elem, v); }, comp);
		comps.push_back(std::move(elem));
	}
	j["components"] = std::move(comps);
}

static void container_child_from_json(const nlohmann::json &j,
									  Container::Child &out) {
	if (!(json_util::not_null_all(j, "type") && j["type"].is_number())) {
		return;
	}

	const auto type_u8 = j["type"].get<uint8_t>();
	switch (static_cast<ComponentType>(type_u8)) {
		case ComponentType::ActionRow: {
			ActionRow a;
			from_json(j, a);
			out.emplace<ActionRow>(std::move(a));
			break;
		}
		case ComponentType::Section: {
			Section s;
			from_json(j, s);
			out.emplace<Section>(std::move(s));
			break;
		}
		case ComponentType::TextDisplay: {
			TextDisplay t;
			from_json(j, t);
			out.emplace<TextDisplay>(std::move(t));
			break;
		}
		case ComponentType::Thumbnail: {
			Thumbnail t;
			from_json(j, t);
			out.emplace<Thumbnail>(std::move(t));
			break;
		}
		case ComponentType::MediaGallery: {
			MediaGallery m;
			from_json(j, m);
			out.emplace<MediaGallery>(std::move(m));
			break;
		}
		case ComponentType::File: {
			File f;
			from_json(j, f);
			out.emplace<File>(std::move(f));
			break;
		}
		case ComponentType::Separator: {
			Separator s;
			from_json(j, s);
			out.emplace<Separator>(std::move(s));
			break;
		}
		case ComponentType::Button: {
			Button b;
			from_json(j, b);
			out.emplace<Button>(std::move(b));
			break;
		}
		case ComponentType::SelectMenu: {
			SelectMenu s;
			from_json(j, s);
			out.emplace<SelectMenu>(std::move(s));
			break;
		}
		case ComponentType::UserSelect: {
			UserSelectMenu s;
			from_json(j, s);
			out.emplace<UserSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::RoleSelect: {
			RoleSelectMenu s;
			from_json(j, s);
			out.emplace<RoleSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::MentionableSelect: {
			MentionableSelectMenu s;
			from_json(j, s);
			out.emplace<MentionableSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::ChannelSelect: {
			ChannelSelectMenu s;
			from_json(j, s);
			out.emplace<ChannelSelectMenu>(std::move(s));
			break;
		}
		default: {
			UnknownMessageComponent u;
			u.type = type_u8;
			u.raw = j;
			out.emplace<UnknownMessageComponent>(std::move(u));
			break;
		}
	}
}

void from_json(const nlohmann::json &j, Container &c) {
	c.raw = j;

	deserialize(j, "id", c.id);

	if (json_util::not_null_all(j, "accent_color") &&
		j["accent_color"].is_number_unsigned()) {
		c.accent_color = j["accent_color"].get<uint32_t>();
	}

	deserialize(j, "spoiler", c.spoiler);

	c.components.clear();
	if (!json_util::not_null_all(j, "components") ||
		!j["components"].is_array()) {
		return;
	}

	for (const auto &elem : j["components"]) {
		Container::Child child = UnknownMessageComponent{};
		container_child_from_json(elem, child);
		c.components.emplace_back(std::move(child));
	}
}

void to_json(nlohmann::json &j, const MessageComponent &c) {
	std::visit([&j](const auto &v) { to_json(j, v); }, c);
}

void from_json(const nlohmann::json &j, MessageComponent &c) {
	if (!(json_util::not_null_all(j, "type") && j["type"].is_number())) {
		return;
	}

	const auto type_u8 = j["type"].get<uint8_t>();
	switch (static_cast<ComponentType>(type_u8)) {
		case ComponentType::ActionRow: {
			ActionRow a;
			from_json(j, a);
			c.emplace<ActionRow>(std::move(a));
			break;
		}
		case ComponentType::Button: {
			Button b;
			from_json(j, b);
			c.emplace<Button>(std::move(b));
			break;
		}
		case ComponentType::SelectMenu: {
			SelectMenu s;
			from_json(j, s);
			c.emplace<SelectMenu>(std::move(s));
			break;
		}
		case ComponentType::UserSelect: {
			UserSelectMenu s;
			from_json(j, s);
			c.emplace<UserSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::RoleSelect: {
			RoleSelectMenu s;
			from_json(j, s);
			c.emplace<RoleSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::MentionableSelect: {
			MentionableSelectMenu s;
			from_json(j, s);
			c.emplace<MentionableSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::ChannelSelect: {
			ChannelSelectMenu s;
			from_json(j, s);
			c.emplace<ChannelSelectMenu>(std::move(s));
			break;
		}
		case ComponentType::TextInput: {
			TextInput t;
			from_json(j, t);
			c.emplace<TextInput>(std::move(t));
			break;
		}
		case ComponentType::Section: {
			Section s;
			from_json(j, s);
			c.emplace<Section>(std::move(s));
			break;
		}
		case ComponentType::TextDisplay: {
			TextDisplay t;
			from_json(j, t);
			c.emplace<TextDisplay>(std::move(t));
			break;
		}
		case ComponentType::Thumbnail: {
			Thumbnail t;
			from_json(j, t);
			c.emplace<Thumbnail>(std::move(t));
			break;
		}
		case ComponentType::MediaGallery: {
			MediaGallery m;
			from_json(j, m);
			c.emplace<MediaGallery>(std::move(m));
			break;
		}
		case ComponentType::File: {
			File f;
			from_json(j, f);
			c.emplace<File>(std::move(f));
			break;
		}
		case ComponentType::Separator: {
			Separator s;
			from_json(j, s);
			c.emplace<Separator>(std::move(s));
			break;
		}
		case ComponentType::Container: {
			Container ct;
			from_json(j, ct);
			c.emplace<Container>(std::move(ct));
			break;
		}
		case ComponentType::Label: {
			Label l;
			from_json(j, l);
			c.emplace<Label>(std::move(l));
			break;
		}
		case ComponentType::FileUpload: {
			FileUpload f;
			from_json(j, f);
			c.emplace<FileUpload>(std::move(f));
			break;
		}
		default: {
			UnknownMessageComponent u;
			u.type = type_u8;
			u.raw = j;
			c.emplace<UnknownMessageComponent>(std::move(u));
			break;
		}
	}
}

}  // namespace ekizu
