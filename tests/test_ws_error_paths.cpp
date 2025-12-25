#include <catch2/catch_test_macros.hpp>
#include <string>

#include "ekizu/ws.hpp"
#include "test_support/ws_fixture.hpp"
#include "test_support/ws_test_hooks.hpp"

using namespace std::chrono_literals;

using test_support::IoPair;
using test_support::ws_fixture::await_ws_connect;
using test_support::ws_fixture::make_server;

using ekizu::Result;
using ekizu::net::WebSocketClient;

TEST_CASE("WebSocketClient connect fails for invalid URI",
		  "[ws][error_paths]") {
	IoPair io;
	auto r = await_ws_connect(io, "not a url");
	REQUIRE_FALSE(r.has_value());
}

TEST_CASE("WebSocketClient connect fails for invalid scheme",
		  "[ws][error_paths]") {
	IoPair io;
	auto r = await_ws_connect(io, "http://127.0.0.1:80/");
	REQUIRE_FALSE(r.has_value());
}

TEST_CASE("WebSocketClient connect fails when TCP connect is refused",
		  "[ws][error_paths]") {
	IoPair io;
	// Port 1 should be closed on most systems; this is intended to exercise the
	// connect error path without requiring DNS.
	auto r = await_ws_connect(io, "ws://127.0.0.1:1/echo");
	REQUIRE_FALSE(r.has_value());
}

TEST_CASE("WebSocketClient wss verify_mode error branch via hook",
		  "[ws][error_paths]") {
#ifdef EKIZU_TESTING
	IoPair io;
	ekizu::net::ScopedWsVerifyModeHook hook([](boost::asio::ssl::context &) {
		return make_error_code(boost::system::errc::not_supported);
	});

	auto r = await_ws_connect(io, "wss://127.0.0.1:1/echo");
	REQUIRE_FALSE(r.has_value());
#else
	SUCCEED("EKIZU_TESTING not enabled");
#endif
}

TEST_CASE("WebSocketClient wss default_verify_paths error branch via hook",
		  "[ws][error_paths]") {
#ifdef EKIZU_TESTING
	IoPair io;
	ekizu::net::ScopedWsDefaultVerifyPathsHook hook(
		[](boost::asio::ssl::context &) {
			return make_error_code(boost::system::errc::io_error);
		});

	auto r = await_ws_connect(io, "wss://127.0.0.1:1/echo");
	REQUIRE_FALSE(r.has_value());
#else
	SUCCEED("EKIZU_TESTING not enabled");
#endif
}

TEST_CASE("WebSocketClient wss SNI failure branch via hook",
		  "[ws][error_paths]") {
#ifdef EKIZU_TESTING
	IoPair io;
	auto server = make_server(io);

	ekizu::net::ScopedWsSniHook hook([](void *, const char *) {
		return false;
	});

	std::string url =
		"wss://127.0.0.1:" + std::to_string(server->port()) + "/echo";
	auto r = await_ws_connect(io, url);
	REQUIRE_FALSE(r.has_value());
#else
	SUCCEED("EKIZU_TESTING not enabled");
#endif
}