#ifndef EKIZU_REQUEST_EDIT_MESSAGE_HPP
#define EKIZU_REQUEST_EDIT_MESSAGE_HPP

#include <boost/system/error_code.hpp>
#include <ekizu/http.hpp>
#include <ekizu/message.hpp>
#include <ekizu/request/request_sender.hpp>
#include <ekizu/request/upload_attachment.hpp>
#include <nlohmann/json.hpp>

namespace ekizu {
struct EditMessageFields {
	/// Message contents (up to 2000 characters).
	std::optional<std::string> content;
	/// Up to 10 rich embeds (up to 6000 characters).
	std::optional<std::vector<Embed>> embeds;
	/// Message flags combined as a bitfield.
	///
	/// NOTE: Discord restricts which flags can be set when editing messages.
	/// This library uses this for things like SuppressEmbeds,
	/// SuppressNotifications, and IsComponentsV2 (Components V2 messages).
	std::optional<MessageFlags> flags;
	/// Allowed mentions for the message.
	std::optional<AllowedMentions> allowed_mentions;
	/// Components to include with the message.
	std::optional<std::vector<MessageComponent>> components;
	/// JSON-encoded body of non-file params, only for multipart/form-data
	/// requests. See Uploading Files.
	std::optional<std::string> payload_json;
	/// Attachment objects with filename and description. See Uploading Files.
	std::optional<std::vector<PartialAttachment>> attachments;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const EditMessageFields &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j, EditMessageFields &f);

struct EditMessage {
	EditMessage(RequestSender sender, Snowflake channel_id,
				Snowflake message_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	EditMessage &allowed_mentions(AllowedMentions allowed_mentions) {
		m_fields.allowed_mentions = allowed_mentions;
		return *this;
	}

	EditMessage &content(std::string content) {
		// TODO: Validate content
		m_fields.content = std::move(content);
		return *this;
	}

	EditMessage &components(std::vector<MessageComponent> components) {
		// TODO: Validate components
		m_fields.components = std::move(components);
		return *this;
	}

	EditMessage &embeds(std::vector<Embed> embeds) {
		// TODO: Validate embeds
		m_fields.embeds = std::move(embeds);
		return *this;
	}

	EditMessage &flags(MessageFlags flags) {
		m_fields.flags = flags;
		return *this;
	}

	/// Sets raw JSON payload. When set, all other fields are ignored except for
	/// uploaded attachments.
	EditMessage &payload_json(std::string payload_json) {
		m_fields.payload_json = std::move(payload_json);
		return *this;
	}

	/// Upload attachments. Calling this clears previous calls.
	EditMessage &attachments(std::vector<UploadAttachment> attachments) {
		m_upload_attachments = std::move(attachments);
		return *this;
	}

	/// When uploading attachments, specifies which existing attachment IDs to
	/// keep. If not called, existing attachments may be removed depending on
	/// the attachments payload sent.
	EditMessage &keep_attachment_ids(std::vector<Snowflake> ids) {
		m_keep_attachment_ids = std::move(ids);
		return *this;
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<Message>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<Message>)>(
			[this](auto &&handler) {
				if (m_upload_attachments.size() > 10) {
					return std::forward<decltype(handler)>(handler)(
						Result<Message>{boost::system::errc::invalid_argument});
				}

				for (const auto &a : m_upload_attachments) {
					if (a.filename.empty() ||
						a.filename.find('\r') != std::string::npos ||
						a.filename.find('\n') != std::string::npos) {
						return std::forward<decltype(handler)>(
							handler)(Result<Message>{
							boost::system::errc::invalid_argument});
					}

					if (a.description && a.description->size() > 1024) {
						return std::forward<decltype(handler)>(
							handler)(Result<Message>{
							boost::system::errc::invalid_argument});
					}
				}

				// If we're uploading files and payload_json is used, we must be
				// able to parse it to inject attachments metadata.
				if (!m_upload_attachments.empty() && m_fields.payload_json) {
					auto j = nlohmann::json::parse(
						*m_fields.payload_json, nullptr, false);
					if (j.is_discarded()) {
						return std::forward<decltype(handler)>(
							handler)(Result<Message>{
							boost::system::errc::invalid_argument});
					}
				}

				m_sender.send<Message>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_channel_id;
	Snowflake m_message_id;
	EditMessageFields m_fields;
	std::vector<UploadAttachment> m_upload_attachments;
	std::optional<std::vector<Snowflake>> m_keep_attachment_ids;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_EDIT_MESSAGE_HPP
