#include <ekizu/request/bulk_delete_messages.hpp>

namespace ekizu {
BulkDeleteMessages::BulkDeleteMessages(
	RequestSender sender, Snowflake channel_id,
	const std::vector<Snowflake> &message_ids)
	: m_channel_id{channel_id}, m_message_ids{message_ids}, m_sender{sender} {}

BulkDeleteMessages::operator net::HttpRequest() const {
	auto req = net::HttpRequest{
		net::HttpMethod::post,
		fmt::format("/channels/{}/messages/bulk-delete", m_channel_id), 11,
		nlohmann::json{{"messages", m_message_ids}}.dump()};

	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();

	return req;
}
}  // namespace ekizu
