#include <ekizu/component_interaction_response.hpp>
#include <ekizu/json_util.hpp>
#include <ekizu/message_component.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

// String Select Response
void to_json(nlohmann::json &j, const StringSelectResponse &r) {
	j = nlohmann::json::object();
	serialize(j, "type", 3);
	serialize(j, "id", r.id);
	serialize(j, "custom_id", r.custom_id);
	serialize(j, "values", r.values);
}

void from_json(const nlohmann::json &j, StringSelectResponse &r) {
	deserialize(j, "id", r.id);
	deserialize(j, "custom_id", r.custom_id);
	deserialize(j, "values", r.values);
}

// Text Input Response
void to_json(nlohmann::json &j, const TextInputResponse &r) {
	j = nlohmann::json::object();
	serialize(j, "type", 4);
	serialize(j, "id", r.id);
	serialize(j, "custom_id", r.custom_id);
	serialize(j, "value", r.value);
}

void from_json(const nlohmann::json &j, TextInputResponse &r) {
	deserialize(j, "id", r.id);
	deserialize(j, "custom_id", r.custom_id);
	deserialize(j, "value", r.value);
}

// User Select Response
void to_json(nlohmann::json &j, const UserSelectResponse &r) {
	j = nlohmann::json::object();
	serialize(j, "type", 5);
	serialize(j, "id", r.id);
	serialize(j, "custom_id", r.custom_id);
	serialize(j, "values", r.values);
}

void from_json(const nlohmann::json &j, UserSelectResponse &r) {
	deserialize(j, "id", r.id);
	deserialize(j, "custom_id", r.custom_id);
	deserialize(j, "values", r.values);
}

// Role Select Response
void to_json(nlohmann::json &j, const RoleSelectResponse &r) {
	j = nlohmann::json::object();
	serialize(j, "type", 6);
	serialize(j, "id", r.id);
	serialize(j, "custom_id", r.custom_id);
	serialize(j, "values", r.values);
}

void from_json(const nlohmann::json &j, RoleSelectResponse &r) {
	deserialize(j, "id", r.id);
	deserialize(j, "custom_id", r.custom_id);
	deserialize(j, "values", r.values);
}

// Mentionable Select Response
void to_json(nlohmann::json &j, const MentionableSelectResponse &r) {
	j = nlohmann::json::object();
	serialize(j, "type", 7);
	serialize(j, "id", r.id);
	serialize(j, "custom_id", r.custom_id);
	serialize(j, "values", r.values);
}

void from_json(const nlohmann::json &j, MentionableSelectResponse &r) {
	deserialize(j, "id", r.id);
	deserialize(j, "custom_id", r.custom_id);
	deserialize(j, "values", r.values);
}

// Channel Select Response
void to_json(nlohmann::json &j, const ChannelSelectResponse &r) {
	j = nlohmann::json::object();
	serialize(j, "type", 8);
	serialize(j, "id", r.id);
	serialize(j, "custom_id", r.custom_id);
	serialize(j, "values", r.values);
}

void from_json(const nlohmann::json &j, ChannelSelectResponse &r) {
	deserialize(j, "id", r.id);
	deserialize(j, "custom_id", r.custom_id);
	deserialize(j, "values", r.values);
}

// Text Display Response
void to_json(nlohmann::json &j, const TextDisplayResponse &r) {
	j = nlohmann::json::object();
	serialize(j, "type", 10);
	serialize(j, "id", r.id);
}

void from_json(const nlohmann::json &j, TextDisplayResponse &r) {
	deserialize(j, "id", r.id);
}

// Interactive Component Response (variant)
void to_json(nlohmann::json &j, const InteractiveComponentResponse &r) {
	std::visit([&j](const auto &alt) { to_json(j, alt); }, r);
}

void from_json(const nlohmann::json &j, InteractiveComponentResponse &r) {
	if (!json_util::not_null_all(j, "type") || !j["type"].is_number()) {
		return;
	}

	const auto type_u8 = j["type"].get<uint8_t>();

	switch (static_cast<ComponentType>(type_u8)) {
		case ComponentType::SelectMenu: {
			StringSelectResponse r;
			from_json(j, r);
			r = std::move(r);
			break;
		}
		case ComponentType::TextInput: {
			TextInputResponse r;
			from_json(j, r);
			r = std::move(r);
			break;
		}
		case ComponentType::UserSelect: {
			UserSelectResponse r;
			from_json(j, r);
			r = std::move(r);
			break;
		}
		case ComponentType::RoleSelect: {
			RoleSelectResponse r;
			from_json(j, r);
			r = std::move(r);
			break;
		}
		case ComponentType::MentionableSelect: {
			MentionableSelectResponse r;
			from_json(j, r);
			r = std::move(r);
			break;
		}
		case ComponentType::ChannelSelect: {
			ChannelSelectResponse r;
			from_json(j, r);
			r = std::move(r);
			break;
		}
		case ComponentType::FileUpload: {
			FileUploadResponse r;
			from_json(j, r);
			r = std::move(r);
			break;
		}
		default:
			// Invalid child type for Label/ActionRow, use first variant
			// alternative
			r = TextInputResponse{};
			break;
	}
}

// Label Response
void to_json(nlohmann::json &j, const LabelResponse &r) {
	j = nlohmann::json::object();
	serialize(j, "type", 18);
	serialize(j, "id", r.id);
	if (r.component) { j["component"] = *r.component; }
}

void from_json(const nlohmann::json &j, LabelResponse &r) {
	deserialize(j, "id", r.id);
	if (json_util::not_null_all(j, "component") && j["component"].is_object()) {
		r.component = j["component"].get<InteractiveComponentResponse>();
	}
}

// File Upload Response
void to_json(nlohmann::json &j, const FileUploadResponse &r) {
	j = nlohmann::json::object();
	serialize(j, "type", 19);
	serialize(j, "id", r.id);
	serialize(j, "custom_id", r.custom_id);
	serialize(j, "values", r.values);
}

void from_json(const nlohmann::json &j, FileUploadResponse &r) {
	deserialize(j, "id", r.id);
	deserialize(j, "custom_id", r.custom_id);
	deserialize(j, "values", r.values);
}

// Action Row Response
void to_json(nlohmann::json &j, const ActionRowResponse &r) {
	j = nlohmann::json::object();
	serialize(j, "type", 1);
	serialize(j, "id", r.id);
	serialize(j, "components", r.components);
}

void from_json(const nlohmann::json &j, ActionRowResponse &r) {
	deserialize(j, "id", r.id);
	deserialize(j, "components", r.components);
}

// Unknown Component Response
void to_json(nlohmann::json &j, const UnknownComponentResponse &r) {
	j = r.raw.is_object() ? r.raw : nlohmann::json::object();
	if (!j.contains("type")) { j["type"] = r.type; }
}

void from_json(const nlohmann::json &j, UnknownComponentResponse &r) {
	r.raw = j;
	if (json_util::not_null_all(j, "type") && j["type"].is_number()) {
		r.type = j["type"].get<uint8_t>();
	}
}

// Component Interaction Response (variant)
void to_json(nlohmann::json &j, const ComponentInteractionResponse &c) {
	std::visit([&j](const auto &alt) { to_json(j, alt); }, c);
}

void from_json(const nlohmann::json &j, ComponentInteractionResponse &c) {
	// Check for type field
	if (!json_util::not_null_all(j, "type") || !j["type"].is_number()) {
		c = UnknownComponentResponse{};
		return;
	}

	const auto type_u8 = j["type"].get<uint8_t>();

	switch (static_cast<ComponentType>(type_u8)) {
		case ComponentType::ActionRow: {
			ActionRowResponse r;
			from_json(j, r);
			c = std::move(r);
			break;
		}
		case ComponentType::SelectMenu: {
			StringSelectResponse r;
			from_json(j, r);
			c = std::move(r);
			break;
		}
		case ComponentType::TextInput: {
			TextInputResponse r;
			from_json(j, r);
			c = std::move(r);
			break;
		}
		case ComponentType::UserSelect: {
			UserSelectResponse r;
			from_json(j, r);
			c = std::move(r);
			break;
		}
		case ComponentType::RoleSelect: {
			RoleSelectResponse r;
			from_json(j, r);
			c = std::move(r);
			break;
		}
		case ComponentType::MentionableSelect: {
			MentionableSelectResponse r;
			from_json(j, r);
			c = std::move(r);
			break;
		}
		case ComponentType::ChannelSelect: {
			ChannelSelectResponse r;
			from_json(j, r);
			c = std::move(r);
			break;
		}
		case ComponentType::TextDisplay: {
			TextDisplayResponse r;
			from_json(j, r);
			c = r;
			break;
		}
		case ComponentType::Label: {
			LabelResponse r;
			from_json(j, r);
			c = std::move(r);
			break;
		}
		case ComponentType::FileUpload: {
			FileUploadResponse r;
			from_json(j, r);
			c = std::move(r);
			break;
		}
		default: {
			UnknownComponentResponse r;
			r.type = type_u8;
			r.raw = j;
			c = std::move(r);
			break;
		}
	}
}

}  // namespace ekizu
