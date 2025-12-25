#include <ekizu/request/edit_channel_permissions.hpp>

namespace ekizu {
EditChannelPermissions::EditChannelPermissions(
	RequestSender sender, Snowflake channel_id, PermissionOverwrite overwrite)
	: m_channel_id{channel_id}, m_overwrite{overwrite}, m_sender{sender} {}

EditChannelPermissions::operator net::HttpRequest() const {
	net::HttpRequest req{net::HttpMethod::put,
						 fmt::format("/channels/{}/permissions/{}",
									 m_channel_id, m_overwrite.id),
						 11, static_cast<nlohmann::json>(m_overwrite).dump()};

	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();

	return req;
}
}  // namespace ekizu
