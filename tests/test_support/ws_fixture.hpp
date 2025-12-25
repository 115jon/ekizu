#ifndef TEST_SUPPORT_WS_FIXTURE_HPP
#define TEST_SUPPORT_WS_FIXTURE_HPP

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/coroutine/exceptions.hpp>
#include <future>
#include <memory>
#include <sstream>
#include <string>

#include "asio_test_harness.hpp"
#include "ekizu/ws.hpp"

namespace test_support::ws_fixture {

namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;

using ekizu::Result;
using ekizu::net::WebSocketClient;
using ekizu::net::WebSocketMessage;

// Simple async WebSocket echo server running on IoPair::net.
struct WsServer : std::enable_shared_from_this<WsServer> {
	tcp::acceptor acceptor_;
	tcp::endpoint endpoint_;

	explicit WsServer(IoPair &io)
		: acceptor_(io.net, tcp::endpoint{tcp::v4(), 0}),
		  endpoint_(acceptor_.local_endpoint()) {}

	void start() { do_accept(); }

	std::uint16_t port() const { return endpoint_.port(); }

	std::string url() const {
		std::ostringstream os;
		os << "ws://127.0.0.1:" << endpoint_.port() << "/echo";
		return os.str();
	}

   private:
	void do_accept() {
		acceptor_.async_accept(
			asio::make_strand(acceptor_.get_executor()),
			[self = shared_from_this()](
				boost::system::error_code ec, tcp::socket sock) mutable {
				if (!ec) {
					std::make_shared<Session>(std::move(sock))->start();
				}
				// Keep accepting even if this accept failed.
				self->do_accept();
			});
	}

	struct Session : std::enable_shared_from_this<Session> {
		beast::websocket::stream<tcp::socket> ws_;
		beast::flat_buffer buffer_;

		explicit Session(tcp::socket sock) : ws_(std::move(sock)) {}

		void start() {
			// Minimal echo server: accept handshake, then echo all messages.
			ws_.async_accept([self = shared_from_this()](
								 boost::system::error_code ec) mutable {
				if (!ec) { self->do_read(); }
			});
		}

		void do_read() {
			ws_.async_read(buffer_, [self = shared_from_this()](
										boost::system::error_code ec,
										std::size_t) mutable {
				if (ec) {
					return;	 // connection closed or error
				}
				// Echo preserving text/binary flag
				self->ws_.text(self->ws_.got_text());
				self->ws_.async_write(
					self->buffer_.data(),
					[self](boost::system::error_code ec2, std::size_t) mutable {
						if (ec2) { return; }
						self->buffer_.consume(self->buffer_.size());
						self->do_read();
					});
			});
		}
	};
};

inline std::shared_ptr<WsServer> make_server(IoPair &io) {
	auto s = std::make_shared<WsServer>(io);
	s->start();
	return s;
}

// Helpers analogous to udp_fixture::await_create / await_send.

inline Result<WebSocketClient> await_ws_connect(IoPair &io,
												std::string const &url) {
	std::promise<Result<WebSocketClient>> p;
	auto f = p.get_future();

	WebSocketClient::connect(
		io.net.get_executor(), url,
		asio::bind_executor(
			io.cb.get_executor(),
			[pp = std::move(p)](Result<WebSocketClient> r) mutable {
				pp.set_value(std::move(r));
			}));

	if (!wait_ready(f, std::chrono::seconds(2))) {
		// Test harness: treat timeout as a failure with generic error.
		return boost::system::errc::timed_out;
	}
	return f.get();
}

inline Result<> await_ws_send(IoPair &io, WebSocketClient &client,
							  std::string const &message) {
	std::promise<Result<>> p;
	auto f = p.get_future();

	client.send(
		message,
		asio::bind_executor(
			io.cb.get_executor(),
			[pp = std::move(p)](Result<> r) mutable { pp.set_value(r); }));

	if (!wait_ready(f, std::chrono::seconds(2))) {
		return boost::system::errc::timed_out;
	}
	return f.get();
}

}  // namespace test_support::ws_fixture

#endif	// TEST_SUPPORT_WS_FIXTURE_HPP
