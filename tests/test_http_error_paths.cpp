#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <catch2/catch_test_macros.hpp>
#include <future>
#include <memory>
#include <string>

#include "ekizu/http.hpp"
#include "test_support/asio_test_harness.hpp"
#include "test_support/http_fixture.hpp"
#include "test_support/http_test_hooks.hpp"

using namespace std::chrono_literals;

using test_support::IoPair;
using test_support::http_fixture::await_http_connect;

using ekizu::Result;
using ekizu::net::HttpConnection;

TEST_CASE("HttpConnection connect fails for invalid URI",
		  "[http][error_paths]") {
	IoPair io;
	auto r = await_http_connect(io, "not a url");
	REQUIRE_FALSE(r.has_value());
}

TEST_CASE("HttpConnection connect fails for invalid scheme",
		  "[http][error_paths]") {
	IoPair io;
	auto r = await_http_connect(io, "ws://127.0.0.1:80/");
	REQUIRE_FALSE(r.has_value());
}

TEST_CASE("HttpConnection connect fails when TCP connect is refused",
		  "[http][error_paths]") {
	IoPair io;
	// Port 1 should be closed on most systems; intended to exercise connect
	// error path.
	auto r = await_http_connect(io, "http://127.0.0.1:1/");
	REQUIRE_FALSE(r.has_value());
}

TEST_CASE("HttpConnection https verify_mode error branch via hook",
		  "[http][error_paths]") {
#ifdef EKIZU_TESTING
	IoPair io;
	ekizu::net::ScopedHttpVerifyModeHook hook([](boost::asio::ssl::context &) {
		return make_error_code(boost::system::errc::not_supported);
	});
	auto r = await_http_connect(io, "https://127.0.0.1:1/");
	REQUIRE_FALSE(r.has_value());
#else
	SUCCEED("EKIZU_TESTING not enabled");
#endif
}

TEST_CASE("HttpConnection https default_verify_paths error branch via hook",
		  "[http][error_paths]") {
#ifdef EKIZU_TESTING
	IoPair io;
	ekizu::net::ScopedHttpDefaultVerifyPathsHook hook(
		[](boost::asio::ssl::context &) {
			return make_error_code(boost::system::errc::io_error);
		});
	auto r = await_http_connect(io, "https://127.0.0.1:1/");
	REQUIRE_FALSE(r.has_value());
#else
	SUCCEED("EKIZU_TESTING not enabled");
#endif
}

TEST_CASE("HttpConnection https SNI failure branch via hook",
		  "[http][error_paths]") {
#ifdef EKIZU_TESTING
	IoPair io;

	namespace asio = boost::asio;
	using tcp = asio::ip::tcp;

	// Minimal TCP acceptor just to allow TCP connect to succeed.
	tcp::acceptor a(io.net, tcp::endpoint{tcp::v4(), 0});
	auto port = a.local_endpoint().port();

	std::promise<void> accepted_p;
	auto accepted_f = accepted_p.get_future();

	auto accepted_sock = std::make_shared<tcp::socket>(io.net);

	a.async_accept(
		*accepted_sock,
		[p = std::move(accepted_p)](boost::system::error_code ec) mutable {
			// Accept can fail on teardown; test focuses on SNI branch.
			(void)ec;
			p.set_value();
		});

	ekizu::net::ScopedHttpSniHook hook([](void *, const char *) {
		return false;
	});

	std::string url = "https://127.0.0.1:" + std::to_string(port) + "/";
	auto r = await_http_connect(io, url);
	REQUIRE_FALSE(r.has_value());

	REQUIRE(test_support::wait_ready(accepted_f, 2s));
#else
	SUCCEED("EKIZU_TESTING not enabled");
#endif
}
