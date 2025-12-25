#include <ekizu/request/interaction/get_original_response.hpp>

namespace ekizu {
GetOriginalResponse::GetOriginalResponse(RequestSender sender,
										 Snowflake application_id,
										 std::string_view interaction_token)
	: m_application_id{application_id},
	  m_interaction_token{interaction_token},
	  m_sender{sender} {}

GetOriginalResponse::operator net::HttpRequest() const {
	return {net::HttpMethod::get,
			fmt::format("/webhooks/{}/{}/messages/@original", m_application_id,
						m_interaction_token),
			11};
}
}  // namespace ekizu
