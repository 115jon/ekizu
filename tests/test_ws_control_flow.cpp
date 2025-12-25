#include <boost/asio/bind_executor.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <catch2/catch_test_macros.hpp>
#include <ekizu/ws.hpp>
#include <future>

#include "test_support/ws_fixture.hpp"
#include "test_support/ws_test_hooks.hpp"

namespace asio = boost::asio;
using namespace std::chrono_literals;
using ekizu::Result;
using ekizu::net::WebSocketClient;
using namespace ekizu::net::ws;
using test_support::IoPair;
using test_support::ws_fixture::await_ws_connect;
using test_support::ws_fixture::make_server;

TEST_CASE("read cancelled before dispatch", "[ws][control_flow]") {
	IoPair io;
	auto server = make_server(io);
	auto cr = await_ws_connect(io, server->url());
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	client.cancel();  // sets m_cancelled

	// Now read hits the check
	std::promise<Result<ekizu::net::WebSocketMessage>> p;
	auto f = p.get_future();
	client.read(asio::bind_executor(
		io.cb.get_executor(),
		[p = std::move(p)](Result<ekizu::net::WebSocketMessage> r) mutable {
			p.set_value(std::move(r));
		}));

	REQUIRE(test_support::wait_ready(f, 2s));
	REQUIRE(f.get().error() == boost::system::errc::operation_canceled);
}

TEST_CASE("start_next_read cancelled with queued handler",
		  "[ws][control_flow]") {
	IoPair io;
	auto server = make_server(io);
	auto client = std::move(await_ws_connect(io, server->url()).value());

	std::promise<Result<ekizu::net::WebSocketMessage>> p1, p2;
	auto f1 = p1.get_future(), f2 = p2.get_future();

	// Queue 2 reads
	client.read(asio::bind_executor(
		io.cb.get_executor(),
		[p1 = std::move(p1)](Result<ekizu::net::WebSocketMessage> r) mutable {
			p1.set_value(std::move(r));
		}));
	client.read(asio::bind_executor(
		io.cb.get_executor(),
		[p2 = std::move(p2)](Result<ekizu::net::WebSocketMessage> r) mutable {
			p2.set_value(std::move(r));
		}));

	client.cancel();  // hits start_next_read() m_cancelled check

	REQUIRE(test_support::wait_ready(f1, 2s));
	REQUIRE(test_support::wait_ready(f2, 2s));
	REQUIRE(f1.get().error() == boost::system::errc::operation_canceled);
	REQUIRE(f2.get().error() == boost::system::errc::operation_canceled);
}

TEST_CASE("DrainOp::step error path", "[ws][control_flow]") {
#ifdef EKIZU_TESTING
	IoPair io;
	ekizu::net::ScopedWsForceDrainErrorHook hook([] {
		return make_error_code(boost::system::errc::not_supported);
	});
	auto server = make_server(io);
	auto cr = await_ws_connect(io, server->url());
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	std::promise<Result<>> p;
	auto f = p.get_future();

	client.send(
		"rejected",
		asio::bind_executor(
			io.cb.get_executor(),
			[p = std::move(p)](Result<> r) mutable { p.set_value(r); }));

	REQUIRE(test_support::wait_ready(f, 5s));
	auto r = f.get();
	REQUIRE_FALSE(r.has_value());
	REQUIRE(r.error() == boost::system::errc::not_supported);
#else
	SUCCEED("EKIZU_TESTING not enabled");
#endif
}
