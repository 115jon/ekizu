#include <ekizu/json_util.hpp>
#include <ekizu/request/create_message.hpp>
#include <ekizu/request/detail/multipart_form_data.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const CreateMessageFields &f) {
	serialize(j, "content", f.content);
	serialize(j, "nonce", f.nonce);
	serialize(j, "tts", f.tts);
	serialize(j, "embeds", f.embeds);
	serialize(j, "allowed_mentions", f.allowed_mentions);
	serialize(j, "message_reference", f.message_reference);
	serialize(j, "components", f.components);
	serialize(j, "sticker_ids", f.sticker_ids);
	serialize(j, "payload_json", f.payload_json);
	serialize(j, "attachments", f.attachments);
	serialize(j, "flags", f.flags);
}

void from_json(const nlohmann::json &j, CreateMessageFields &f) {
	deserialize(j, "content", f.content);
	deserialize(j, "nonce", f.nonce);
	deserialize(j, "tts", f.tts);
	deserialize(j, "embeds", f.embeds);
	deserialize(j, "allowed_mentions", f.allowed_mentions);
	deserialize(j, "message_reference", f.message_reference);
	deserialize(j, "components", f.components);
	deserialize(j, "sticker_ids", f.sticker_ids);
	deserialize(j, "payload_json", f.payload_json);
	deserialize(j, "attachments", f.attachments);
	deserialize(j, "flags", f.flags);
}

CreateMessage::CreateMessage(RequestSender sender, Snowflake channel_id)
	: m_channel_id{channel_id}, m_sender{sender} {}

static nlohmann::json attachments_json_from_uploads(
	const std::vector<UploadAttachment> &uploads) {
	nlohmann::json attachments = nlohmann::json::array();

	for (size_t i = 0; i < uploads.size(); ++i) {
		const auto &u = uploads[i];

		nlohmann::json a = nlohmann::json::object();
		a["id"] = Snowflake{static_cast<uint64_t>(i)};
		a["filename"] = u.filename;
		if (u.description) { a["description"] = *u.description; }

		attachments.push_back(std::move(a));
	}

	return attachments;
}

CreateMessage::operator net::HttpRequest() const {
	// No uploads -> JSON request.
	if (m_upload_attachments.empty()) {
		std::string body;

		if (m_fields.payload_json) {
			// Twilight-style: payload_json overrides all other fields.
			body = *m_fields.payload_json;
		} else {
			auto j = static_cast<nlohmann::json>(m_fields);
			j.erase("payload_json");
			body = j.dump();
		}

		net::HttpRequest req{net::HttpMethod::post,
							 fmt::format("/channels/{}/messages", m_channel_id),
							 11, std::move(body)};

		req.set(net::http::field::content_type, "application/json");
		req.prepare_payload();
		return req;
	}

	// Uploads -> multipart/form-data request.
	std::string boundary = detail::make_multipart_boundary();

	nlohmann::json payload;

	if (m_fields.payload_json) {
		payload = nlohmann::json::parse(*m_fields.payload_json, nullptr, false);
		if (payload.is_discarded()) {
			// Should have been rejected by send() validation.
			payload = nlohmann::json::object();
		}
	} else {
		payload = static_cast<nlohmann::json>(m_fields);
		payload.erase("payload_json");
	}

	// Attachments metadata is tied to uploads.
	payload["attachments"] =
		attachments_json_from_uploads(m_upload_attachments);

	std::string multipart_body = detail::encode_multipart_form_data(
		boundary, payload.dump(), m_upload_attachments);

	net::HttpRequest req{net::HttpMethod::post,
						 fmt::format("/channels/{}/messages", m_channel_id), 11,
						 std::move(multipart_body)};

	req.set(net::http::field::content_type,
			fmt::format("multipart/form-data; boundary={}", boundary));
	req.prepare_payload();
	return req;
}

}  // namespace ekizu