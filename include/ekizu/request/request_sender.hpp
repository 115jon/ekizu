#ifndef EKIZU_REQUEST_REQUEST_SENDER_HPP
#define EKIZU_REQUEST_REQUEST_SENDER_HPP

#include <ekizu/json_util.hpp>
#include <ekizu/rate_limiter.hpp>

namespace ekizu {

/**
 * @brief Helper class for sending HTTP requests with any completion token.
 *
 * This class wraps RateLimiter and provides a completion-token-generic
 * interface that can be stored in request builder objects like CreateDM.
 */
struct RequestSender {
	EKIZU_EXPORT explicit RequestSender(RateLimiter *rate_limiter)
		: m_rate_limiter{rate_limiter} {}

	/**
	 * @brief Send an HTTP request with any completion token.
	 *
	 * @tparam T The expected response type (for deserialization)
	 * @tparam CompletionToken The completion token type
	 * @param req The HTTP request to send
	 * @param token The completion token
	 */
	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<net::HttpResponse>))
				  CompletionToken>
	auto send(net::HttpRequest req, CompletionToken &&token) const {
		return m_rate_limiter->send(DiscordApiRequest{std::move(req)},
									std::forward<CompletionToken>(token));
	}

	/**
	 * @brief Send an HTTP request and deserialize response.
	 *
	 * This is a convenience overload that accepts a callback which receives
	 * the raw HttpResponse, allowing the caller to handle deserialization.
	 *
	 * @tparam CompletionToken The completion token type
	 * @param req The HTTP request to send
	 * @param token The completion token
	 */
	// RequestSender.hpp
	template <typename T,
			  BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<T>)) CompletionToken>
	auto send(net::HttpRequest req, CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<T>)>(
			[this, req = std::move(req)](auto &&handler) mutable {
				m_rate_limiter->send(
					DiscordApiRequest{std::move(req)},
					[h = std::forward<decltype(handler)>(handler)](
						Result<net::HttpResponse> res) mutable {
						if (!res) {
							// Construct Result<T> from error
							return std::move(h)(Result<T>{res.error()});
						}

						auto parsed =
							json_util::deserialize<T>(res.value().body());
						if (!parsed) {
							// Handle deserialization failure
							return std::move(h)(Result<T>{parsed.error()});
						}

						std::move(h)(Result<T>{std::move(parsed.value())});
					});
			},
			token);
	}

   private:
	RateLimiter *m_rate_limiter;
};

}  // namespace ekizu

#endif	// EKIZU_REQUEST_REQUEST_SENDER_HPP
