#ifndef EKIZU_RATE_LIMITER_HPP
#define EKIZU_RATE_LIMITER_HPP

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <deque>
#include <ekizu/http.hpp>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ekizu {

/**
 * @brief Represents a Discord API request.
 */
struct DiscordApiRequest {
	/// The actual HTTP request to send to the Discord API.
	net::HttpRequest inner;
	/// Whether this request is for a bot account.
	bool bot{true};
};

/**
 * @brief The RateLimiter class is responsible for rate limiting requests to the
 * Discord API.
 */
struct RateLimiter {
	using SendFn = std::function<void(
		net::HttpRequest,
		boost::asio::any_completion_handler<void(Result<net::HttpResponse>)>)>;

	EKIZU_EXPORT explicit RateLimiter(
		const boost::asio::any_io_executor &executor, SendFn send_fn);

	EKIZU_EXPORT void shutdown();

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<net::HttpResponse>))
				  CompletionToken>
	auto send(DiscordApiRequest req, CompletionToken &&token) {
		return boost::asio::async_initiate<CompletionToken,
										   void(Result<net::HttpResponse>)>(
			[this, req = std::move(req)](auto &&handler) mutable {
				auto handler_ex = boost::asio::get_associated_executor(
					handler, m_strand.get_inner_executor());
				async_send_impl(std::move(req), handler_ex,
								boost::asio::any_completion_handler<void(
									Result<net::HttpResponse>)>{
									std::forward<decltype(handler)>(handler)});
			},
			token);
	}

   private:
	struct RateLimit {
		uint16_t limit{};
		uint16_t remaining{};
		std::chrono::system_clock::time_point reset_time;
	};

	struct Pending {
		DiscordApiRequest req;
		boost::asio::any_io_executor handler_ex;
		boost::asio::any_completion_handler<void(Result<net::HttpResponse>)>
			handler;
	};

	EKIZU_EXPORT void async_send_impl(
		DiscordApiRequest req, boost::asio::any_io_executor handler_ex,
		boost::asio::any_completion_handler<void(Result<net::HttpResponse>)>
			handler);

	void start_next();
	void maybe_wait_then_send(Pending p);
	void do_send(Pending p);
	void finish_one(Pending p, Result<net::HttpResponse> result);
	void finish_one(Pending p, Result<net::HttpResponse> result,
					std::string context);

	boost::asio::strand<boost::asio::any_io_executor> m_strand;
	SendFn m_send_fn;
	std::atomic_bool m_stopping{false};

	std::mutex m_mtx;
	std::unordered_map<net::HttpMethod,
					   std::unordered_map<std::string, RateLimit>>
		m_rate_limits;

	bool m_busy{false};
	std::deque<Pending> m_queue;
	std::shared_ptr<
		boost::asio::basic_waitable_timer<std::chrono::system_clock>>
		m_wait_timer;
	std::optional<Pending> m_waiting;
};

}  // namespace ekizu

#endif	// EKIZU_RATE_LIMITER_HPP
