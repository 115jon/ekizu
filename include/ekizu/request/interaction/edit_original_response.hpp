#ifndef EKIZU_REQUEST_INTERACTION_EDIT_ORIGINAL_RESPONSE_HPP
#define EKIZU_REQUEST_INTERACTION_EDIT_ORIGINAL_RESPONSE_HPP

#include <ekizu/http.hpp>
#include <ekizu/message.hpp>
#include <ekizu/request/request_sender.hpp>
#include <nlohmann/json.hpp>

namespace ekizu {

/// https://discord.com/developers/docs/interactions/receiving-and-responding#edit-original-interaction-response
/// PATCH /webhooks/{application.id}/{interaction.token}/messages/@original
struct EditOriginalResponse {
	EditOriginalResponse(RequestSender sender, Snowflake application_id,
						 std::string interaction_token);

	EKIZU_EXPORT operator net::HttpRequest() const;

	EditOriginalResponse &allowed_mentions(AllowedMentions allowed_mentions) {
		m_allowed_mentions = allowed_mentions;
		return *this;
	}

	EditOriginalResponse &content(std::string content) {
		m_content = std::move(content);
		return *this;
	}

	EditOriginalResponse &components(std::vector<MessageComponent> components) {
		m_components = std::move(components);
		return *this;
	}

	EditOriginalResponse &embeds(std::vector<Embed> embeds) {
		m_embeds = std::move(embeds);
		return *this;
	}

	EditOriginalResponse &flags(MessageFlags flags) {
		m_flags = flags;
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
	Snowflake m_application_id;
	std::string m_interaction_token;
	std::optional<std::string> m_content;
	std::optional<std::vector<Embed>> m_embeds;
	std::optional<MessageFlags> m_flags;
	std::optional<AllowedMentions> m_allowed_mentions;
	std::optional<std::vector<MessageComponent>> m_components;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_INTERACTION_EDIT_ORIGINAL_RESPONSE_HPP
