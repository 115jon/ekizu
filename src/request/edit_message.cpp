#include <ekizu/json_util.hpp>
#include <ekizu/request/edit_message.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const EditMessageFields &f) {
	serialize(j, "content", f.content);
	serialize(j, "embeds", f.embeds);
	serialize(j, "flags", f.flags);
	serialize(j, "allowed_mentions", f.allowed_mentions);
	serialize(j, "components", f.components);
	serialize(j, "payload_json", f.payload_json);
	serialize(j, "attachments", f.attachments);
}

void from_json(const nlohmann::json &j, EditMessageFields &f) {
	deserialize(j, "content", f.content);
	deserialize(j, "embeds", f.embeds);
	deserialize(j, "flags", f.flags);
	deserialize(j, "allowed_mentions", f.allowed_mentions);
	deserialize(j, "components", f.components);
	deserialize(j, "payload_json", f.payload_json);
	deserialize(j, "attachments", f.attachments);
}

EditMessage::EditMessage(RequestSender sender, Snowflake channel_id,
						 Snowflake message_id)
	: m_channel_id{channel_id}, m_message_id{message_id}, m_sender{sender} {}

EditMessage::operator net::HttpRequest() const {
	auto req = net::HttpRequest{
		net::HttpMethod::patch,
		fmt::format("/channels/{}/messages/{}", m_channel_id, m_message_id), 11,
		static_cast<nlohmann::json>(m_fields).dump()};

	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();

	return req;
}
}  // namespace ekizu