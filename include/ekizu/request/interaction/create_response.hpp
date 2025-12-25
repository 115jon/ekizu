#ifndef EKIZU_REQUEST_INTERACTION_CREATE_RESPONSE_HPP
#define EKIZU_REQUEST_INTERACTION_CREATE_RESPONSE_HPP

#include <ekizu/http.hpp>
#include <ekizu/message.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
enum class InteractionResponseType : uint8_t {
	Pong = 1,
	ChannelMessageWithSource = 4,
	DeferredChannelMessageWithSource = 5,
	DeferredUpdateMessage = 6,
	UpdateMessage = 7,
	ApplicationCommandAutoCompleteResult = 8,
	Modal = 9
};

struct InteractionResponseData {
	std::optional<bool> tts;
	std::optional<std::string> content;
	std::optional<std::vector<Embed>> embeds;
	std::optional<AllowedMentions> allowed_mentions;
	std::optional<MessageFlags> flags;
	std::optional<std::vector<MessageComponent>> components;
	std::optional<std::vector<PartialAttachment>> attachments;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const InteractionResponseData &d);
EKIZU_EXPORT void from_json(const nlohmann::json &j,
							InteractionResponseData &d);

struct InteractionResponse {
	InteractionResponseType type;
	std::optional<InteractionResponseData> data;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const InteractionResponse &r);
EKIZU_EXPORT void from_json(const nlohmann::json &j, InteractionResponse &r);

struct InteractionResponseBuilder {
	[[nodiscard]] InteractionResponse build() { return m_response; }

	InteractionResponseBuilder &type(InteractionResponseType type) {
		m_response.type = type;
		return *this;
	}

	InteractionResponseBuilder &tts(bool tts) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->tts = tts;
		return *this;
	}

	InteractionResponseBuilder &content(std::string_view content) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->content = content;
		return *this;
	}

	InteractionResponseBuilder &embeds(const std::vector<Embed> &embeds) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->embeds = embeds;
		return *this;
	}

	InteractionResponseBuilder &allowed_mentions(
		const AllowedMentions &allowed_mentions) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->allowed_mentions = allowed_mentions;
		return *this;
	}

	InteractionResponseBuilder &flags(MessageFlags flags) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->flags = flags;
		return *this;
	}

	InteractionResponseBuilder &components(
		const std::vector<MessageComponent> &components) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->components = components;
		return *this;
	}

	InteractionResponseBuilder &attachments(
		const std::vector<PartialAttachment> &attachments) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->attachments = attachments;
		return *this;
	}

   private:
	InteractionResponse m_response;
};

struct CreateResponse {
	CreateResponse(RequestSender sender, Snowflake interaction_id,
				   std::string_view interaction_token,
				   InteractionResponse response);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this](auto &&handler) {
				m_sender.send(
					*this, [h = std::forward<decltype(handler)>(handler)](
							   Result<net::HttpResponse> res) mutable {
						if (!res) { return std::move(h)(res.error()); }

						if (res.value().result() !=
							net::HttpStatus::no_content) {
							return std::move(h)(
								boost::system::errc::operation_not_permitted);
						}

						std::move(h)(outcome::success());
					});
			},
			token);
	}

   private:
	Snowflake m_interaction_id;
	std::string m_interaction_token;
	InteractionResponse m_response;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_INTERACTION_CREATE_RESPONSE_HPP
