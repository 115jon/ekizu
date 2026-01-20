#ifndef EKIZU_REQUEST_INTERACTION_CREATE_RESPONSE_HPP
#define EKIZU_REQUEST_INTERACTION_CREATE_RESPONSE_HPP

#include <ekizu/application_command.hpp>
#include <ekizu/http.hpp>
#include <ekizu/interaction_callback.hpp>
#include <ekizu/interaction_response_type.hpp>
#include <ekizu/message.hpp>
#include <ekizu/poll.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {

struct InteractionResponseData {
	std::optional<bool> tts;
	std::optional<std::string> content;
	std::optional<std::vector<Embed>> embeds;
	std::optional<AllowedMentions> allowed_mentions;
	std::optional<MessageFlags> flags;
	std::optional<std::vector<MessageComponent>> components;
	std::optional<std::vector<PartialAttachment>> attachments;
	std::optional<std::vector<ApplicationCommandOptionChoice>> choices;
	std::optional<std::string> custom_id;
	std::optional<std::string> title;
	std::optional<Poll> poll;
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

	InteractionResponseBuilder &content(std::string content) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->content = std::move(content);
		return *this;
	}

	InteractionResponseBuilder &embeds(std::vector<Embed> embeds) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->embeds = std::move(embeds);
		return *this;
	}

	InteractionResponseBuilder &allowed_mentions(
		AllowedMentions allowed_mentions) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->allowed_mentions = std::move(allowed_mentions);
		return *this;
	}

	InteractionResponseBuilder &flags(MessageFlags flags) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->flags = flags;
		return *this;
	}

	InteractionResponseBuilder &components(
		std::vector<MessageComponent> components) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->components = std::move(components);
		return *this;
	}

	InteractionResponseBuilder &attachments(
		std::vector<PartialAttachment> attachments) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->attachments = std::move(attachments);
		return *this;
	}

	InteractionResponseBuilder &choices(
		std::vector<ApplicationCommandOptionChoice> choices) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->choices = std::move(choices);
		return *this;
	}

	InteractionResponseBuilder &custom_id(std::string custom_id) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->custom_id = std::move(custom_id);
		return *this;
	}

	InteractionResponseBuilder &title(std::string title) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->title = std::move(title);
		return *this;
	}

	InteractionResponseBuilder &poll(Poll poll) {
		if (!m_response.data) { m_response.data.emplace(); }
		m_response.data->poll = std::move(poll);
		return *this;
	}

   private:
	InteractionResponse m_response;
};

struct CreateResponseWithResponse {
	EKIZU_EXPORT CreateResponseWithResponse(
		RequestSender sender, Snowflake interaction_id,
		std::string interaction_token, InteractionResponse response);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
		void(Result<InteractionCallbackResponse>)) CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken,
									void(Result<InteractionCallbackResponse>)>(
			[this](auto &&handler) {
				m_sender.send<InteractionCallbackResponse>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_interaction_id;
	std::string m_interaction_token;
	InteractionResponse m_response;
	RequestSender m_sender;
};

struct CreateResponse {
	CreateResponse(RequestSender sender, Snowflake interaction_id,
				   std::string interaction_token, InteractionResponse response);

	EKIZU_EXPORT operator net::HttpRequest() const;

	/// Transforms this builder into one that returns the callback response.
	/// This consumes the current builder.
	[[nodiscard]] CreateResponseWithResponse with_response() && {
		return CreateResponseWithResponse{
			m_sender, m_interaction_id, std::move(m_interaction_token),
			std::move(m_response)};
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this](auto &&handler) {
				m_sender.send(
					*this, [h = std::forward<decltype(handler)>(handler)](
							   Result<net::HttpResponse> res) mutable {
						if (!res) { return std::move(h)(res.error()); }

						const auto &r = res.value();
						if (r.result() != net::HttpStatus::no_content &&
							r.result() != net::HttpStatus::ok) {
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
