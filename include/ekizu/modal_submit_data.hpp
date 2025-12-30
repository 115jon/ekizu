#ifndef EKIZU_MODAL_SUBMIT_DATA_HPP
#define EKIZU_MODAL_SUBMIT_DATA_HPP

#include <ekizu/application_command_data_resolved.hpp>
#include <ekizu/component_interaction_response.hpp>
#include <ekizu/export.hpp>
#include <optional>
#include <string>
#include <vector>

namespace ekizu {
/**
 * @brief Modal submit interaction data.
 *
 * When a user submits a modal, Discord returns component interaction response
 * structures containing the submitted values. These are simplified versions of
 * the components with just the interaction data (values, custom_ids), not the
 * full component configuration.
 *
 * @see
 * https://discord.com/developers/docs/interactions/receiving-and-responding#interaction-object-modal-submit-data-structure
 */
struct ModalSubmitData {
	/// The custom ID provided for the modal.
	std::string custom_id;
	/// Component interaction responses with submitted values.
	std::vector<ComponentInteractionResponse> components;
	/// Resolved entities from selected options (users, roles, channels, etc.).
	std::optional<ApplicationCommandInteractionDataResolved> resolved;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const ModalSubmitData &d);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ModalSubmitData &d);
}  // namespace ekizu

#endif	// EKIZU_MODAL_SUBMIT_DATA_HPP
