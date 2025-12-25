#ifndef EKIZU_REQUEST_EDIT_MESSAGE_HPP
#define EKIZU_REQUEST_EDIT_MESSAGE_HPP

#include <ekizu/http.hpp>
#include <ekizu/message.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct EditMessageFields {
	/// Message contents (up to 2000 characters).
	std::optional<std::string> content;
	/// Up to 10 rich embeds (up to 6000 characters).
	std::optional<std::vector<Embed> > embeds;
	/// Message flags combined as a bitfield (only SUPPRESS_EMBEDS and
	/// SUPPRESS_NOTIFICATIONS can be set).
	std::optional<MessageFlags> flags;
	/// Allowed mentions for the message.
	std::optional<AllowedMentions> allowed_mentions;
	/// Components to include with the message.
	std::optional<std::vector<MessageComponent> > components;
	/// JSON-encoded body of non-file params, only for multipart/form-data
	/// requests. See Uploading Files.
	std::optional<std::string> payload_json;
	/// Attachment objects with filename and description. See Uploading Files.
	std::optional<std::vector<PartialAttachment> > attachments;
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

	EditMessage &content(std::string_view content) {
		// TODO: Validate content
		m_fields.content = content;
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

	EditMessage &payload_json(std::string_view payload_json) {
		m_fields.payload_json = payload_json;
		return *this;
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<Message>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<Message>)>(
			[this](auto &&handler) {
				m_sender.send<Message>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_channel_id;
	Snowflake m_message_id;
	EditMessageFields m_fields;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_EDIT_MESSAGE_HPP
