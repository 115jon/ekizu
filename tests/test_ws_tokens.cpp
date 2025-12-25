#include <boost/asio/detached.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <catch2/catch_test_macros.hpp>
#include <future>
#include <optional>

#include "ekizu/ws.hpp"
#include "test_support/ws_fixture.hpp"

#if __has_include(<boost/asio/deferred.hpp>)
#include <boost/asio/deferred.hpp>
#define EKIZU_HAS_ASIO_DEFERRED 1
#endif

#if defined(__cpp_impl_coroutine) && __cpp_impl_coroutine >= 201902L && \
	__has_include(<boost/asio/awaitable.hpp>) &&                         \
    __has_include(<boost/asio/co_spawn.hpp>) &&                          \
    __has_include(<boost/asio/use_awaitable.hpp>)
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#define EKIZU_HAS_ASIO_AWAITABLE 1
#endif

namespace asio = boost::asio;
using namespace std::chrono_literals;

using test_support::IoPair;
using test_support::ws_fixture::await_ws_connect;
using test_support::ws_fixture::await_ws_send;
using test_support::ws_fixture::make_server;

using ekizu::Result;
using ekizu::net::WebSocketClient;
using ekizu::net::WebSocketMessage;

TEST_CASE("WebSocketClient connect/send/read works with use_future",
		  "[ws][tokens]") {
	IoPair io;
	auto server = make_server(io);

	auto connect_f = WebSocketClient::connect(
		io.net.get_executor(), server->url(),
		asio::bind_executor(io.cb.get_executor(), asio::use_future));

	REQUIRE(connect_f.wait_for(2s) == std::future_status::ready);
	auto cr = connect_f.get();
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	auto recv_f = client.read(
		asio::bind_executor(io.cb.get_executor(), asio::use_future));

	auto send_f = client.send(
		"ping", asio::bind_executor(io.cb.get_executor(), asio::use_future));

	REQUIRE(send_f.wait_for(2s) == std::future_status::ready);
	auto sr = send_f.get();
	REQUIRE(sr.has_value());

	REQUIRE(recv_f.wait_for(2s) == std::future_status::ready);
	auto rr = recv_f.get();
	REQUIRE(rr.has_value());
	REQUIRE(rr.value().payload == "ping");
}

TEST_CASE("WebSocketClient callback style destroy while read pending",
		  "[ws][tokens]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_ws_connect(io, server->url());
	REQUIRE(cr.has_value());

	test_support::CvWaiter<Result<WebSocketMessage>> got_result;
	test_support::CvWaiter<std::thread::id> got_tid;

	// Start read and then destroy the client on the net context.
	asio::post(
		io.net,
		[&io, c = std::make_shared<WebSocketClient>(std::move(cr.value())),
		 &got_result, &got_tid]() mutable {
			c->read(asio::bind_executor(
				io.cb.get_executor(),
				[&got_result, &got_tid](Result<WebSocketMessage> r) mutable {
					got_tid.set(std::this_thread::get_id());
					got_result.set(std::move(r));
				}));
			// Drop last owning reference immediately after starting read.
			c.reset();
		});

	REQUIRE(got_result.wait_for(2000ms));
	REQUIRE(got_tid.wait_for(2000ms));
	REQUIRE(got_tid.value.has_value());
	REQUIRE(got_tid.value == io.cb_tid);
	REQUIRE(got_result.value.has_value());
	REQUIRE_FALSE(got_result.value->has_value());
}

TEST_CASE("WebSocketClient works with asio::spawn/yield", "[ws][tokens]") {
	IoPair io;
	auto server = make_server(io);

	test_support::CvWaiter<Result<void>> done;
	test_support::CvWaiter<std::thread::id> ran_tid;

	asio::spawn(
		io.cb,
		[&](asio::yield_context yield) {
			ran_tid.set(std::this_thread::get_id());

			auto cr = WebSocketClient::connect(
				io.net.get_executor(), server->url(), yield);

			if (!cr.has_value()) {
				done.set(cr.error());
				return;
			}

			auto client = std::move(cr.value());

			auto sr = client.send("ping", yield);
			if (!sr.has_value()) {
				done.set(sr.error());
				return;
			}

			auto rr = client.read(yield);
			if (!rr.has_value()) {
				done.set(rr.error());
				return;
			}

			if (rr.value().payload != "ping") {
				done.set(make_error_code(boost::system::errc::protocol_error));
				return;
			}

			done.set(ekizu::outcome::success());
		},
		asio::detached);

	REQUIRE(ran_tid.wait_for(2000ms));
	REQUIRE(done.wait_for(2000ms));
	REQUIRE(ran_tid.value.has_value());
	REQUIRE(ran_tid.value == io.cb_tid);
	REQUIRE(done.value.has_value());
	REQUIRE(done.value->has_value());
}

#if defined(EKIZU_HAS_ASIO_DEFERRED)
TEST_CASE("WebSocketClient works with asio::deferred", "[ws][tokens]") {
	IoPair io;
	auto server = make_server(io);

	test_support::CvWaiter<Result<void>> done;

	asio::spawn(
		io.cb,
		[&](asio::yield_context yield) {
			auto create_op = WebSocketClient::connect(
				io.net.get_executor(), server->url(), asio::deferred);

			auto cr = std::move(create_op)(yield);
			if (!cr.has_value()) {
				done.set(cr.error());
				return;
			}

			auto client = std::move(cr.value());

			auto send_op = client.send("ping", asio::deferred);
			auto sr = std::move(send_op)(yield);
			if (!sr.has_value()) {
				done.set(sr.error());
				return;
			}

			auto recv_op = client.read(asio::deferred);
			auto rr = std::move(recv_op)(yield);
			if (!rr.has_value()) {
				done.set(rr.error());
				return;
			}

			if (rr.value().payload != "ping") {
				done.set(make_error_code(boost::system::errc::protocol_error));
				return;
			}

			done.set(ekizu::outcome::success());
		},
		asio::detached);

	REQUIRE(done.wait_for(2000ms));
	REQUIRE(done.value.has_value());
	REQUIRE(done.value->has_value());
}
#endif

#if defined(EKIZU_HAS_ASIO_AWAITABLE)
TEST_CASE("WebSocketClient works with co_spawn/use_awaitable", "[ws][tokens]") {
	IoPair io;
	auto server = make_server(io);

	test_support::CvWaiter<Result<void>> done;
	test_support::CvWaiter<std::thread::id> ran_tid;

	asio::co_spawn(
		io.cb,
		[&]() -> asio::awaitable<void> {
			ran_tid.set(std::this_thread::get_id());

			auto cr = co_await WebSocketClient::connect(
				io.net.get_executor(), server->url(), asio::use_awaitable);

			if (!cr.has_value()) {
				done.set(cr.error());
				co_return;
			}

			auto client = std::move(cr.value());

			auto sr = co_await client.send("ping", asio::use_awaitable);
			if (!sr.has_value()) {
				done.set(sr.error());
				co_return;
			}

			auto rr = co_await client.read(asio::use_awaitable);
			if (!rr.has_value()) {
				done.set(rr.error());
				co_return;
			}

			if (rr.value().payload != "ping") {
				done.set(make_error_code(boost::system::errc::protocol_error));
				co_return;
			}

			done.set(ekizu::outcome::success());
			co_return;
		},
		asio::detached);

	REQUIRE(ran_tid.wait_for(2000ms));
	REQUIRE(done.wait_for(2000ms));
	REQUIRE(ran_tid.value.has_value());
	REQUIRE(ran_tid.value == io.cb_tid);
	REQUIRE(done.value.has_value());
	REQUIRE(done.value->has_value());
}
#endif
