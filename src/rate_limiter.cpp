#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/steady_timer.hpp>
#include <ekizu/rate_limiter.hpp>

namespace ekizu {

RateLimiter::RateLimiter(boost::asio::any_io_executor executor, SendFn send_fn)
	: m_strand{executor}, m_send_fn{std::move(send_fn)} {}

void RateLimiter::async_send_impl(
	DiscordApiRequest req, boost::asio::any_io_executor handler_ex,
	boost::asio::any_completion_handler<void(Result<net::HttpResponse>)>
		handler) {
	boost::asio::dispatch(m_strand, [this, req = std::move(req), handler_ex,
									 h = std::move(handler)]() mutable {
		m_queue.push_back(Pending{std::move(req), handler_ex, std::move(h)});
		start_next();
	});
}

void RateLimiter::start_next() {
	if (m_busy || m_queue.empty()) { return; }
	m_busy = true;

	auto p = std::move(m_queue.front());
	m_queue.pop_front();

	maybe_wait_then_send(std::move(p));
}

void RateLimiter::maybe_wait_then_send(Pending p) {
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

	auto timer = std::make_shared<
		boost::asio::basic_waitable_timer<std::chrono::system_clock>>(
		m_strand.get_inner_executor());
	timer->expires_at(reset_time);

	timer->async_wait(boost::asio::bind_executor(
		m_strand, [this, p = std::move(p),
				   timer](const boost::system::error_code &ec) mutable {
			if (ec) {
				finish_one(std::move(p), ec);
				return;
			}
			do_send(std::move(p));
		}));
}

void RateLimiter::do_send(Pending p) {
	if (!m_send_fn) {
		finish_one(std::move(p), boost::system::errc::operation_not_permitted);
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

				// Preserve old semantics: return response only for 2xx, else
				// success().
				if (!res) {
					finish_one(std::move(p), res.error());
					return;
				}

				const auto status = res.value().result();
				if (status >= net::HttpStatus::ok &&
					status < net::HttpStatus::multiple_choices) {
					finish_one(std::move(p), std::move(res));
					return;
				}

				finish_one(std::move(p), outcome::success());
			})});
}

void RateLimiter::finish_one(Pending p, Result<net::HttpResponse> result) {
	// IMPORTANT: p.handler is already the original completion handler; just
	// invoke it. We hop to its associated executor using bind_executor captured
	// at initiation time.
	boost::asio::post(p.handler_ex, [h = std::move(p.handler),
									 result = std::move(result)]() mutable {
		std::move(h)(std::move(result));
	});

	boost::asio::dispatch(m_strand, [this]() {
		m_busy = false;
		start_next();
	});
}

}  // namespace ekizu