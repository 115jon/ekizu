#ifndef TEST_SUPPORT_HTTP_FIXTURE_HPP
#define TEST_SUPPORT_HTTP_FIXTURE_HPP

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "asio_test_harness.hpp"
#include "ekizu/http.hpp"

namespace test_support::http_fixture {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
using tcp = asio::ip::tcp;

using ekizu::Result;
using ekizu::net::HttpConnection;
using ekizu::net::HttpRequest;
using ekizu::net::HttpResponse;

// Simple async HTTP server running on IoPair::net.
// - For most requests, body = (req.body() if non-empty else req.target()).
// - For target "/slow", response is delayed a bit to make cancellation tests
// reliable.
struct HttpServer : std::enable_shared_from_this<HttpServer> {
	tcp::acceptor acceptor_;
	tcp::endpoint endpoint_;

	explicit HttpServer(IoPair &io)
		: acceptor_(io.net, tcp::endpoint{tcp::v4(), 0}),
		  endpoint_(acceptor_.local_endpoint()) {}

	void start() { do_accept(); }

	std::uint16_t port() const { return endpoint_.port(); }

	std::string base_url() const {
		std::ostringstream os;
		os << "http://127.0.0.1:" << endpoint_.port() << "/";
		return os.str();
	}

	// Close the acceptor to force new connections to fail deterministically.
	void stop_accepting() {
		boost::system::error_code ec;
		acceptor_.close(ec);
	}

	void clear_seen() {
		std::lock_guard<std::mutex> lk(seen_mtx_);
		seen_targets_.clear();
	}

	std::vector<std::string> seen_targets() const {
		std::lock_guard<std::mutex> lk(seen_mtx_);
		return seen_targets_;
	}

	bool wait_seen(std::size_t n, std::chrono::milliseconds timeout) const {
		std::unique_lock<std::mutex> lk(seen_mtx_);
		return seen_cv_.wait_for(
			lk, timeout, [&]() { return seen_targets_.size() >= n; });
	}

   private:
	void record_seen(std::string target) {
		{
			std::lock_guard<std::mutex> lk(seen_mtx_);
			seen_targets_.push_back(std::move(target));
		}
		seen_cv_.notify_all();
	}

	void do_accept() {
		acceptor_.async_accept(
			asio::make_strand(acceptor_.get_executor()),
			[self = shared_from_this()](
				boost::system::error_code ec, tcp::socket sock) mutable {
				if (!ec) {
					std::make_shared<Session>(std::move(sock), self)->start();
				} else {
					// If the acceptor was intentionally closed, stop accepting.
					if (ec == asio::error::operation_aborted) { return; }
				}
				self->do_accept();
			});
	}

	struct Session : std::enable_shared_from_this<Session> {
		tcp::socket sock_;
		beast::flat_buffer buffer_;
		http::request<http::string_body> req_;
		http::response<http::string_body> res_;
		asio::steady_timer timer_;
		std::weak_ptr<HttpServer> server_;

		explicit Session(tcp::socket sock,
						 const std::shared_ptr<HttpServer> &server)
			: sock_(std::move(sock)),
			  timer_(sock_.get_executor()),
			  server_(server) {}

		void start() { do_read(); }

		void do_read() {
			req_ = {};

			http::async_read(
				sock_, buffer_, req_,
				[self = shared_from_this()](
					boost::system::error_code ec, std::size_t) mutable {
					if (ec) { return; }

					if (auto s = self->server_.lock()) {
						s->record_seen(std::string(self->req_.target()));
					}

					std::string body;
					if (!self->req_.body().empty()) {
						body = self->req_.body();
					} else {
						body = std::string(self->req_.target());
					}

					self->res_ = http::response<http::string_body>{
						http::status::ok, self->req_.version()};
					self->res_.set(
						http::field::server, "ekizu-test-http-server");
					self->res_.set(http::field::content_type, "text/plain");
					self->res_.keep_alive(self->req_.keep_alive());
					self->res_.body() = std::move(body);
					self->res_.prepare_payload();

					if (self->req_.target() == "/slow") {
						self->timer_.expires_after(
							std::chrono::milliseconds(500));
						self->timer_.async_wait(
							[self](boost::system::error_code tec) mutable {
								if (tec) { return; }
								self->do_write();
							});
						return;
					}

					self->do_write();
				});
		}

		void do_write() {
			http::async_write(
				sock_, res_,
				[self = shared_from_this()](
					boost::system::error_code ec, std::size_t) mutable {
					if (ec) { return; }
					self->do_read();
				});
		}
	};

	mutable std::mutex seen_mtx_;
	mutable std::condition_variable seen_cv_;
	mutable std::vector<std::string> seen_targets_;
};

inline std::shared_ptr<HttpServer> make_server(IoPair &io) {
	auto s = std::make_shared<HttpServer>(io);
	s->start();
	return s;
}

inline Result<HttpConnection> await_http_connect(IoPair &io,
												 std::string const &url) {
	std::promise<Result<HttpConnection>> p;
	auto f = p.get_future();

	HttpConnection::connect(
		io.net.get_executor(), url,
		asio::bind_executor(
			io.cb.get_executor(),
			[pp = std::move(p)](Result<HttpConnection> r) mutable {
				pp.set_value(std::move(r));
			}));

	if (!wait_ready(f, std::chrono::seconds(2))) {
		return boost::system::errc::timed_out;
	}
	return f.get();
}

inline Result<HttpResponse> await_http_request(IoPair &io, HttpConnection &conn,
											   HttpRequest req) {
	std::promise<Result<HttpResponse>> p;
	auto f = p.get_future();

	conn.request(std::move(req),
				 asio::bind_executor(
					 io.cb.get_executor(),
					 [pp = std::move(p)](Result<HttpResponse> r) mutable {
						 pp.set_value(std::move(r));
					 }));

	if (!wait_ready(f, std::chrono::seconds(5))) {
		return boost::system::errc::timed_out;
	}
	return f.get();
}

}  // namespace test_support::http_fixture

#endif	// TEST_SUPPORT_HTTP_FIXTURE_HPP
