#include <ekizu/json_util.hpp>
#include <ekizu/request/detail/multipart_form_data.hpp>
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

static nlohmann::json attachments_json_from_keep_ids(
	const std::vector<Snowflake> &keep_ids) {
	nlohmann::json attachments = nlohmann::json::array();

	for (const auto &id : keep_ids) {
		nlohmann::json a = nlohmann::json::object();
		a["id"] = id;
		attachments.push_back(std::move(a));
	}

	return attachments;
}

EditMessage::operator net::HttpRequest() const {
	const bool has_uploads = !m_upload_attachments.empty();
	const bool has_payload_json_override = m_fields.payload_json.has_value();

	// No uploads -> JSON request.
	if (!has_uploads) {
		std::string body;

		if (has_payload_json_override) {
			// Twilight-style: payload_json overrides all other fields.
			// keep_attachment_ids is ignored in this mode.
			body = *m_fields.payload_json;
		} else {
			auto j = static_cast<nlohmann::json>(m_fields);
			j.erase("payload_json");

			// keep_attachment_ids is only meaningful when payload_json is NOT
			// used.
			if (m_keep_attachment_ids) {
				j["attachments"] =
					attachments_json_from_keep_ids(*m_keep_attachment_ids);
			} else {
				// Avoid accidentally sending a null/empty attachments value.
				j.erase("attachments");
			}

			body = j.dump();
		}

		auto req = net::HttpRequest{
			net::HttpMethod::patch,
			fmt::format("/channels/{}/messages/{}", m_channel_id, m_message_id),
			11, std::move(body)};

		req.set(net::http::field::content_type, "application/json");
		req.prepare_payload();
		return req;
	}

	// Uploads -> multipart/form-data request.
	std::string boundary = detail::make_multipart_boundary();

	nlohmann::json payload;

	if (has_payload_json_override) {
		// Twilight-style: payload_json overrides all other fields, but uploaded
		// attachments still apply.
		payload = nlohmann::json::parse(*m_fields.payload_json, nullptr, false);
		if (payload.is_discarded()) {
			// Should have been rejected by send() validation.
			payload = nlohmann::json::object();
		}

		// keep_attachment_ids is ignored when payload_json is used.
		payload["attachments"] =
			attachments_json_from_uploads(m_upload_attachments);
	} else {
		payload = static_cast<nlohmann::json>(m_fields);
		payload.erase("payload_json");

		nlohmann::json attachments = nlohmann::json::array();

		if (m_keep_attachment_ids) {
			auto keep = attachments_json_from_keep_ids(*m_keep_attachment_ids);
			for (auto &elem : keep) { attachments.push_back(std::move(elem)); }
		}

		{
			auto new_uploads =
				attachments_json_from_uploads(m_upload_attachments);
			for (auto &elem : new_uploads) {
				attachments.push_back(std::move(elem));
			}
		}

		payload["attachments"] = std::move(attachments);
	}

	std::string multipart_body = detail::encode_multipart_form_data(
		boundary, payload.dump(), m_upload_attachments);

	auto req = net::HttpRequest{
		net::HttpMethod::patch,
		fmt::format("/channels/{}/messages/{}", m_channel_id, m_message_id), 11,
		std::move(multipart_body)};

	req.set(net::http::field::content_type,
			fmt::format("multipart/form-data; boundary={}", boundary));
	req.prepare_payload();
	return req;
}

}  // namespace ekizu