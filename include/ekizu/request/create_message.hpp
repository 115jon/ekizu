#ifndef EKIZU_REQUEST_CREATE_MESSAGE_HPP
#define EKIZU_REQUEST_CREATE_MESSAGE_HPP

#include <boost/system/error_code.hpp>
#include <ekizu/http.hpp>
#include <ekizu/message.hpp>
#include <ekizu/request/request_sender.hpp>
#include <ekizu/request/upload_attachment.hpp>
#include <nlohmann/json.hpp>

namespace ekizu {
struct CreateMessageFields {
	/// Message contents (up to 2000 characters).
	std::optional<std::string> content;
	/// Can be used to verify a message was sent (up to 25 characters). Value
	/// will appear in the Message Create event.
	std::optional<uint64_t> nonce;
	/// True if this is a TTS message.
	std::optional<bool> tts;
	/// Up to 10 rich embeds (up to 6000 characters).
	std::optional<std::vector<Embed> > embeds;
	/// Allowed mentions for the message.
	std::optional<AllowedMentions> allowed_mentions;
	/// Include to make your message a reply.
	std::optional<MessageReference> message_reference;
	/// Components to include with the message.
	std::optional<std::vector<MessageComponent> > components;
	/// IDs of up to 3 stickers in the server to send in the message.
	std::optional<std::vector<Snowflake> > sticker_ids;
	/// JSON-encoded body of non-file params, only for multipart/form-data
	/// requests. See Uploading Files.
	std::optional<std::string> payload_json;
	/// Attachment objects with filename and description. See Uploading Files.
	std::optional<std::vector<PartialAttachment> > attachments;
	/// Message flags combined as a bitfield.
	///
	/// NOTE: Discord restricts which flags can be set when creating messages.
	/// This library uses this for things like SuppressEmbeds,
	/// SuppressNotifications, and IsComponentsV2 (Components V2 messages).
	std::optional<MessageFlags> flags;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const CreateMessageFields &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j, CreateMessageFields &f);

struct CreateMessage {
	CreateMessage(RequestSender sender, Snowflake channel_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	CreateMessage &allowed_mentions(AllowedMentions allowed_mentions) {
		m_fields.allowed_mentions = allowed_mentions;
		return *this;
	}

	CreateMessage &components(const std::vector<MessageComponent> &components) {
		m_fields.components = components;
		return *this;
	}

	CreateMessage &content(std::string content) {
		// TODO: Validate content
		m_fields.content = std::move(content);
		return *this;
	}

	CreateMessage &embeds(const std::vector<Embed> &embeds) {
		// TODO: Validate embeds
		m_fields.embeds = embeds;
		return *this;
	}

	CreateMessage &flags(MessageFlags flags) {
		m_fields.flags = flags;
		return *this;
	}

	CreateMessage &nonce(uint64_t nonce) {
		m_fields.nonce = nonce;
		return *this;
	}

	/// Sets raw JSON payload. When set, all other fields are ignored except for
	/// uploaded attachments.
	CreateMessage &payload_json(std::string payload_json) {
		m_fields.payload_json = std::move(payload_json);
		return *this;
	}

	/// Upload attachments. Calling this clears previous calls.
	CreateMessage &attachments(std::vector<UploadAttachment> attachments) {
		m_upload_attachments = std::move(attachments);
		return *this;
	}

	CreateMessage &reply(Snowflake message_id) {
		MessageReference ref;
		ref.message_id = message_id;
		ref.channel_id = m_channel_id;
		m_fields.message_reference = ref;
		return *this;
	}

	CreateMessage &sticker_ids(const std::vector<Snowflake> &sticker_ids) {
		m_fields.sticker_ids = sticker_ids;
		return *this;
	}

	CreateMessage &tts(bool tts) {
		m_fields.tts = tts;
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
	CreateMessageFields m_fields;
	std::vector<UploadAttachment> m_upload_attachments;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_CREATE_MESSAGE_HPP
