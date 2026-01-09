#include <fmt/format.h>

#include <ekizu/json_util.hpp>
#include <ekizu/request/interaction/edit_original_response.hpp>


namespace ekizu {
EditOriginalResponse::EditOriginalResponse(RequestSender sender,
										   Snowflake application_id,
										   std::string interaction_token)
	: m_application_id{application_id},
	  m_interaction_token{std::move(interaction_token)},
	  m_sender{sender} {}

EditOriginalResponse::operator net::HttpRequest() const {
	nlohmann::json body = nlohmann::json::object();

	json_util::serialize(body, "content", m_content);
	json_util::serialize(body, "embeds", m_embeds);
	json_util::serialize(body, "allowed_mentions", m_allowed_mentions);
	json_util::serialize(body, "components", m_components);

	if (m_flags) {
		body["flags"] =
			static_cast<std::underlying_type_t<MessageFlags>>(*m_flags);
	}

	net::HttpRequest req{net::HttpMethod::patch,
						 fmt::format("/webhooks/{}/{}/messages/@original",
									 m_application_id, m_interaction_token),
						 11, body.dump()};
	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();
	return req;
}
}  // namespace ekizu
