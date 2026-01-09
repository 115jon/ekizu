#ifndef EKIZU_REQUEST_INTERACTION_CLIENT_HPP
#define EKIZU_REQUEST_INTERACTION_CLIENT_HPP

#include <ekizu/request/interaction/create_response.hpp>
#include <ekizu/request/interaction/edit_original_response.hpp>
#include <ekizu/request/interaction/get_original_response.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct InteractionClient {
	InteractionClient(RequestSender sender, Snowflake application_id);

	/// https://discord.com/developers/docs/interactions/receiving-and-responding#endpoints

	[[nodiscard]] EKIZU_EXPORT CreateResponse
	create_response(Snowflake interaction_id, std::string interaction_token,
					InteractionResponse response) const;

	[[nodiscard]] EKIZU_EXPORT GetOriginalResponse get_original_response(
		Snowflake application_id, std::string interaction_token) const;

	/// Edit the initial response to an interaction.
	/// https://discord.com/developers/docs/interactions/receiving-and-responding#edit-original-interaction-response
	[[nodiscard]] EKIZU_EXPORT EditOriginalResponse
	edit_original_response(std::string interaction_token) const;

   private:
	Snowflake m_application_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_INTERACTION_CLIENT_HPP
