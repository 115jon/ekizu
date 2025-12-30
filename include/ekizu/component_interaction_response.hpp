#ifndef EKIZU_COMPONENT_INTERACTION_RESPONSE_HPP
#define EKIZU_COMPONENT_INTERACTION_RESPONSE_HPP

#include <ekizu/export.hpp>
#include <ekizu/snowflake.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ekizu {

/**
 * @brief Component Interaction Response Structures.
 *
 * When a user interacts with components (in modals or messages), Discord
 * returns simplified versions of the components with only the relevant
 * interaction data. These structures contain the submitted/selected values, not
 * the full component configuration.
 *
 * Used in:
 * - Modal Submit Data (MODAL_SUBMIT interaction type 5)
 * - Message Component Data (MESSAGE_COMPONENT interaction type 3)
 *
 * @see
 * https://discord.com/developers/docs/interactions/receiving-and-responding#interaction-object-modal-submit-data-structure
 * @see
 * https://discord.com/developers/docs/interactions/receiving-and-responding#interaction-object-component-interaction-response-structures
 */

/**
 * @brief String Select interaction response (Type 3).
 */
struct StringSelectResponse {
	std::optional<uint32_t> id;
	std::string custom_id;
	std::vector<std::string> values;
};

/**
 * @brief Text Input interaction response (Type 4).
 */
struct TextInputResponse {
	std::optional<uint32_t> id;
	std::string custom_id;
	std::string value;
};

/**
 * @brief User Select interaction response (Type 5).
 */
struct UserSelectResponse {
	std::optional<uint32_t> id;
	std::string custom_id;
	std::vector<Snowflake> values;
};

/**
 * @brief Role Select interaction response (Type 6).
 */
struct RoleSelectResponse {
	std::optional<uint32_t> id;
	std::string custom_id;
	std::vector<Snowflake> values;
};

/**
 * @brief Mentionable Select interaction response (Type 7).
 */
struct MentionableSelectResponse {
	std::optional<uint32_t> id;
	std::string custom_id;
	std::vector<Snowflake> values;
};

/**
 * @brief Channel Select interaction response (Type 8).
 */
struct ChannelSelectResponse {
	std::optional<uint32_t> id;
	std::string custom_id;
	std::vector<Snowflake> values;
};

/**
 * @brief Text Display interaction response (Type 10).
 */
struct TextDisplayResponse {
	std::optional<uint32_t> id;
};

/**
 * @brief File Upload interaction response (Type 19).
 */
struct FileUploadResponse {
	std::optional<uint32_t> id;
	std::string custom_id;
	std::vector<Snowflake> values;
};

/**
 * @brief Unknown component response for forward compatibility.
 */
struct UnknownComponentResponse {
	uint8_t type{};
	nlohmann::json raw;
};

/**
 * @brief Interactive components that can be children of Label or ActionRow.
 *
 * According to Discord docs:
 * - Label can contain: TextInput, Selects, FileUpload (NOT other Labels or
 * ActionRows)
 * - ActionRow (deprecated) historically contained: TextInput only in legacy
 * modals
 */
using InteractiveComponentResponse =
	std::variant<TextInputResponse, StringSelectResponse, UserSelectResponse,
				 RoleSelectResponse, MentionableSelectResponse,
				 ChannelSelectResponse, FileUploadResponse>;

/**
 * @brief Label interaction response (Type 18).
 *
 * Contains a single interactive child component (no nesting of Labels or
 * ActionRows).
 */
struct LabelResponse {
	std::optional<uint32_t> id;
	std::optional<InteractiveComponentResponse> component;
};

/**
 * @brief Action Row interaction response (Type 1) - DEPRECATED.
 *
 * For legacy modal support only. Modern modals use Label instead.
 * Historically contained only TextInput components.
 */
struct ActionRowResponse {
	std::optional<uint32_t> id;
	std::vector<InteractiveComponentResponse> components;
};

/**
 * @brief Variant of all possible component interaction response types.
 *
 * This is the main type for component responses. No pointers needed since
 * Discord's API doesn't allow recursive nesting - Label and ActionRow can only
 * contain interactive leaf components, not other Labels or ActionRows.
 */
using ComponentInteractionResponse =
	std::variant<StringSelectResponse, TextInputResponse, UserSelectResponse,
				 RoleSelectResponse, MentionableSelectResponse,
				 ChannelSelectResponse, TextDisplayResponse, LabelResponse,
				 ActionRowResponse, FileUploadResponse,
				 UnknownComponentResponse>;

// JSON serialization functions
EKIZU_EXPORT void to_json(nlohmann::json &j, const StringSelectResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j, StringSelectResponse &r);

EKIZU_EXPORT void to_json(nlohmann::json &j, const TextInputResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j, TextInputResponse &r);

EKIZU_EXPORT void to_json(nlohmann::json &j, const UserSelectResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j, UserSelectResponse &r);

EKIZU_EXPORT void to_json(nlohmann::json &j, const RoleSelectResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j, RoleSelectResponse &r);

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const MentionableSelectResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							MentionableSelectResponse &r);

EKIZU_EXPORT void to_json(nlohmann::json &j, const ChannelSelectResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ChannelSelectResponse &r);

EKIZU_EXPORT void to_json(nlohmann::json &j, const TextDisplayResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j, TextDisplayResponse &r);

EKIZU_EXPORT void to_json(nlohmann::json &j, const LabelResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j, LabelResponse &r);

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const InteractiveComponentResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							InteractiveComponentResponse &r);

EKIZU_EXPORT void to_json(nlohmann::json &j, const ActionRowResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ActionRowResponse &r);

EKIZU_EXPORT void to_json(nlohmann::json &j, const FileUploadResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j, FileUploadResponse &r);

EKIZU_EXPORT void to_json(nlohmann::json &j, const UnknownComponentResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							UnknownComponentResponse &r);

EKIZU_EXPORT void to_json(nlohmann::json &j,
						  const ComponentInteractionResponse &c);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							ComponentInteractionResponse &c);

}  // namespace ekizu

#endif	// EKIZU_COMPONENT_INTERACTION_RESPONSE_HPP
