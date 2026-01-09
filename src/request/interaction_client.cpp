#include <ekizu/request/interaction_client.hpp>

namespace ekizu {
InteractionClient::InteractionClient(RequestSender sender,
									 Snowflake application_id)
	: m_application_id{application_id}, m_sender{sender} {}

CreateResponse InteractionClient::create_response(
	Snowflake interaction_id, std::string interaction_token,
	InteractionResponse response) const {
	return {m_sender, interaction_id, std::move(interaction_token),
			std::move(response)};
}

GetOriginalResponse InteractionClient::get_original_response(
	Snowflake application_id, std::string interaction_token) const {
	return {m_sender, m_application_id, std::move(interaction_token)};
}

EditOriginalResponse InteractionClient::edit_original_response(
	std::string interaction_token) const {
	return {m_sender, m_application_id, std::move(interaction_token)};
}
}  // namespace ekizu