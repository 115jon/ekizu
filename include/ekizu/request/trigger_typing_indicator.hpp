#ifndef EKIZU_REQUEST_TRIGGER_TYPING_INDICATOR_HPP
#define EKIZU_REQUEST_TRIGGER_TYPING_INDICATOR_HPP

#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>
#include <ekizu/snowflake.hpp>

namespace ekizu {
struct TriggerTypingIndicator {
	TriggerTypingIndicator(RequestSender sender, Snowflake channel_id);

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
	Snowflake m_channel_id;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_TRIGGER_TYPING_INDICATOR_HPP
