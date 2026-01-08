#include <fmt/format.h>

#include <boost/asio/bind_executor.hpp>
#include <ekizu/error.hpp>
#include <ekizu/error_context.hpp>
#include <ekizu/rate_limiter.hpp>

namespace ekizu {

namespace {
constexpr std::size_t k_error_body_limit = 1024;

static std::string build_http_status_context(const net::HttpRequest &req,
											 const net::HttpResponse &res) {
	std::string body = res.body();
	if (body.size() > k_error_body_limit) {
		body.resize(k_error_body_limit);
		body.append("...(truncated)");
	}

	std::string headers;
	for (const auto &field : req) {
		fmt::format_to(std::back_inserter(headers), "\n  {}: {}",
					   field.name_string(), field.value());
	}

	if (body.empty()) {
		return fmt::format(
			"HTTP status {} ({}) for {} {}{}", res.result_int(), res.reason(),
			req.method_string(), req.target(), headers);
	}
	return fmt::format(
		"HTTP status {} ({}) for {} {}{}; body: {}", res.result_int(),
		res.reason(), req.method_string(), req.target(), headers, body);
}

static ekizu::errc map_http_status_to_errc(net::HttpStatus status) {
	// Discord API expected success responses are 2xx; everything else is
	// treated as an error.
	if (status == net::HttpStatus::too_many_requests) {
		return ekizu::errc::http_rate_limited;
	}
	auto s = static_cast<unsigned>(status);
	if (s >= 300 && s < 400) { return ekizu::errc::http_redirection; }
	if (s >= 400 && s < 500) { return ekizu::errc::http_client_error; }
	if (s >= 500 && s < 600) { return ekizu::errc::http_server_error; }
	return ekizu::errc::http_error;
}

}  // namespace

RateLimiter::RateLimiter(const boost::asio::any_io_executor &executor,
						 SendFn send_fn)
	: m_strand{executor}, m_send_fn{std::move(send_fn)} {}

void RateLimiter::shutdown() {
	boost::asio::dispatch(m_strand, [this] {
		const bool was = m_stopping.exchange(true, std::memory_order_relaxed);
		if (was) { return; }

		// Prevent any further sends.
		m_send_fn = {};

		// Cancel any pending rate-limit wait.
		if (m_wait_timer) { m_wait_timer->cancel(); }

		// Fail any "waiting" request.
		if (m_waiting) {
			auto p = std::move(*m_waiting);
			m_waiting.reset();
			finish_one(
				std::move(p),
				ekizu::make_error_code(ekizu::errc::rate_limiter_stopped),
				"RateLimiter: shutdown");
		}

		// Fail queued requests immediately.
		while (!m_queue.empty()) {
			auto p = std::move(m_queue.front());
			m_queue.pop_front();
			finish_one(
				std::move(p),
				ekizu::make_error_code(ekizu::errc::rate_limiter_stopped),
				"RateLimiter: shutdown");
		}

		// If something was considered "busy", allow start_next() to stop
		// cleanly.
		m_busy = false;
	});
}

void RateLimiter::async_send_impl(
	DiscordApiRequest req, boost::asio::any_io_executor handler_ex,
	boost::asio::any_completion_handler<void(Result<net::HttpResponse>)>
		handler) {
	boost::asio::dispatch(m_strand, [this, req = std::move(req), handler_ex,
									 h = std::move(handler)]() mutable {
		if (m_stopping.load(std::memory_order_relaxed)) {
			// Complete immediately on the handler's associated executor.
			boost::asio::post(handler_ex, [h = std::move(h)]() mutable {
				ekizu::clear_error_context();
				ekizu::set_error_context("RateLimiter: shutdown");
				std::move(h)(
					ekizu::make_error_code(ekizu::errc::rate_limiter_stopped));
			});
			return;
		}

		m_queue.push_back(Pending{std::move(req), handler_ex, std::move(h)});
		start_next();
	});
}

void RateLimiter::start_next() {
	if (m_stopping.load(std::memory_order_relaxed)) {
		m_busy = false;
		return;
	}

	if (m_busy || m_queue.empty()) { return; }

	m_busy = true;
	auto p = std::move(m_queue.front());
	m_queue.pop_front();
	maybe_wait_then_send(std::move(p));
}

void RateLimiter::maybe_wait_then_send(Pending p) {
	if (m_stopping.load(std::memory_order_relaxed)) {
		finish_one(std::move(p),
				   ekizu::make_error_code(ekizu::errc::rate_limiter_stopped),
				   "RateLimiter: shutdown");
		return;
	}

	std::chrono::system_clock::time_point reset_time;
	bool should_wait = false;

	{
		std::scoped_lock lk{m_mtx};
		auto &rate_limits = m_rate_limits[p.req.inner.method()];
		auto it = rate_limits.find(std::string(p.req.inner.target()));
		if (it != rate_limits.end()) {
			auto &rl = it->second;
			auto now = std::chrono::system_clock::now();
			if (rl.remaining == 0 && now < rl.reset_time) {
				reset_time = rl.reset_time;
				should_wait = true;
			}
		}
	}

	if (!should_wait) {
		do_send(std::move(p));
		return;
	}

	// Store as the single "waiting" request (this RateLimiter is serialized by
	// m_strand).
	m_waiting.emplace(std::move(p));
	if (!m_wait_timer) {
		m_wait_timer = std::make_shared<
			boost::asio::basic_waitable_timer<std::chrono::system_clock>>(
			m_strand.get_inner_executor());
	}

	m_wait_timer->expires_at(reset_time);
	m_wait_timer->async_wait(boost::asio::bind_executor(
		m_strand, [this](const boost::system::error_code &ec) mutable {
			if (!m_waiting) {
				// Spurious: nothing waiting anymore.
				m_busy = false;
				start_next();
				return;
			}

			auto p2 = std::move(*m_waiting);
			m_waiting.reset();

			if (m_stopping.load(std::memory_order_relaxed)) {
				finish_one(
					std::move(p2),
					ekizu::make_error_code(ekizu::errc::rate_limiter_stopped),
					"RateLimiter: shutdown");
				return;
			}

			if (ec) {
				// Includes operation_aborted from timer cancel.
				finish_one(std::move(p2), ec);
				return;
			}

			do_send(std::move(p2));
		}));
}

void RateLimiter::do_send(Pending p) {
	if (m_stopping.load(std::memory_order_relaxed)) {
		finish_one(std::move(p),
				   ekizu::make_error_code(ekizu::errc::rate_limiter_stopped),
				   "RateLimiter: shutdown");
		return;
	}

	if (!m_send_fn) {
		finish_one(
			std::move(p),
			ekizu::make_error_code(ekizu::errc::rate_limiter_send_disabled),
			"RateLimiter: send function not set");
		return;
	}

	// Force the send_fn completion back onto THIS strand by binding.
	net::HttpRequest req = p.req.inner;

	m_send_fn(
		std::move(req),
		boost::asio::any_completion_handler<
			void(Result<net::HttpResponse>)>{boost::asio::bind_executor(
			m_strand,
			[this, p = std::move(p)](Result<net::HttpResponse> res) mutable {
				if (res) {
					std::scoped_lock lk{m_mtx};

					auto &rate_limits = m_rate_limits[p.req.inner.method()];
					const auto &headers = res.value().base();

					auto itL = headers.find("X-RateLimit-Limit");
					auto itR = headers.find("X-RateLimit-Remaining");
					auto itT = headers.find("X-RateLimit-Reset");

					if (itL != headers.end() && itR != headers.end() &&
						itT != headers.end()) {
						rate_limits.insert_or_assign(
							std::string(p.req.inner.target()),
							RateLimit{
								static_cast<uint16_t>(std::stoul(itL->value())),
								static_cast<uint16_t>(std::stoul(itR->value())),
								std::chrono::system_clock::from_time_t(
									std::stol(itT->value()))});
					}
				}

				// Return response only for 2xx; treat all other statuses as
				// errors.
				if (!res) {
					std::string ctx = fmt::format(
						"HTTP transport error for {} {}: {}",
						p.req.inner.method_string(), p.req.inner.target(),
						res.error().message());
					finish_one(std::move(p), res.error(), std::move(ctx));
					return;
				}

				const auto status = res.value().result();
				if (status >= net::HttpStatus::ok &&
					status < net::HttpStatus::multiple_choices) {
					finish_one(std::move(p), std::move(res));
					return;
				}

				const auto ec =
					ekizu::make_error_code(map_http_status_to_errc(status));
				finish_one(std::move(p), ec,
						   build_http_status_context(p.req.inner, res.value()));
			})});
}

void RateLimiter::finish_one(Pending p, Result<net::HttpResponse> result,
							 std::string context) {
	// Hop to the handler's associated executor.
	boost::asio::post(
		p.handler_ex, [h = std::move(p.handler), result = std::move(result),
					   context = std::move(context)]() mutable {
			ekizu::clear_error_context();
			ekizu::set_error_context(context);
			std::move(h)(std::move(result));
		});

	boost::asio::dispatch(m_strand, [this] {
		m_busy = false;
		start_next();
	});
}

void RateLimiter::finish_one(Pending p, Result<net::HttpResponse> result) {
	// Hop to the handler's associated executor.
	boost::asio::post(p.handler_ex, [h = std::move(p.handler),
									 result = std::move(result)]() mutable {
		std::move(h)(std::move(result));
	});

	boost::asio::dispatch(m_strand, [this] {
		m_busy = false;
		start_next();
	});
}

}  // namespace ekizu