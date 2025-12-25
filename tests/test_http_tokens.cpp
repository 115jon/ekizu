#include <boost/asio/bind_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <catch2/catch_test_macros.hpp>
#include <future>
#include <optional>
#include <thread>

#include "ekizu/http.hpp"
#include "test_support/http_fixture.hpp"

#if __has_include(<boost/asio/deferred.hpp>)
#include <boost/asio/deferred.hpp>
#define EKIZU_HAS_ASIO_DEFERRED 1
#endif

#if defined(BOOST_ASIO_HAS_CO_AWAIT) && \
	__has_include(                      \
		<boost/asio/awaitable.hpp>) && __has_include(<boost/asio/co_spawn.hpp>) && \
	__has_include(<boost/asio/use_awaitable.hpp>)
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#define EKIZU_HAS_ASIO_AWAITABLE 1
#endif

namespace asio = boost::asio;

using namespace std::chrono_literals;

using test_support::IoPair;
using test_support::http_fixture::make_server;

using ekizu::Result;
using ekizu::net::HttpConnection;
using ekizu::net::HttpRequest;
using ekizu::net::HttpResponse;

TEST_CASE("HttpConnection connect/request works with use_future",
		  "[http][tokens]") {
	IoPair io;
	auto server = make_server(io);

	auto connect_f = HttpConnection::connect(
		io.net.get_executor(), server->base_url(),
		asio::bind_executor(io.cb.get_executor(), asio::use_future));

	REQUIRE(connect_f.wait_for(2s) == std::future_status::ready);
	auto cr = connect_f.get();
	REQUIRE(cr.has_value());

	auto conn = std::move(cr.value());

	HttpRequest req{ekizu::net::HttpMethod::get, "/ping", 11};
	req.set(ekizu::net::http::field::host, "127.0.0.1");

	auto req_f = conn.request(
		std::move(req),
		asio::bind_executor(io.cb.get_executor(), asio::use_future));

	REQUIRE(req_f.wait_for(2s) == std::future_status::ready);
	auto rr = req_f.get();
	REQUIRE(rr.has_value());
	REQUIRE(rr.value().result() == ekizu::net::HttpStatus::ok);
	REQUIRE(rr.value().body() == "/ping");
}

TEST_CASE("HttpConnection callback style destroy while request pending",
		  "[http][tokens]") {
	IoPair io;
	auto server = make_server(io);

	auto cr =
		test_support::http_fixture::await_http_connect(io, server->base_url());
	REQUIRE(cr.has_value());

	test_support::CvWaiter<Result<HttpResponse>> got_result;
	test_support::CvWaiter<std::thread::id> got_tid;

	asio::post(
		io.net,
		[&io, c = std::make_shared<HttpConnection>(std::move(cr.value())),
		 &got_result, &got_tid]() mutable {
			HttpRequest req{ekizu::net::HttpMethod::get, "/slow", 11};
			req.set(ekizu::net::http::field::host, "127.0.0.1");

			c->request(
				std::move(req),
				asio::bind_executor(
					io.cb.get_executor(),
					[&got_result, &got_tid](Result<HttpResponse> r) mutable {
						got_tid.set(std::this_thread::get_id());
						got_result.set(std::move(r));
					}));

			// Drop last owning reference immediately after starting request.
			c.reset();
		});

	REQUIRE(got_result.wait_for(5000ms));
	REQUIRE(got_tid.wait_for(5000ms));

	REQUIRE(got_tid.value.has_value());
	REQUIRE(got_tid.value == io.cb_tid);

	REQUIRE(got_result.value.has_value());
	REQUIRE_FALSE(got_result.value->has_value());
	REQUIRE(got_result.value->error() ==
			boost::system::errc::operation_canceled);
}

TEST_CASE("HttpConnection works with asio::spawn/yield", "[http][tokens]") {
	IoPair io;
	auto server = make_server(io);

	test_support::CvWaiter<Result<>> done;
	test_support::CvWaiter<std::thread::id> ran_tid;

	asio::spawn(
		io.cb,
		[&](asio::yield_context yield) {
			ran_tid.set(std::this_thread::get_id());

			auto cr = HttpConnection::connect(
				io.net.get_executor(), server->base_url(), yield);
			if (!cr.has_value()) {
				done.set(cr.error());
				return;
			}

			auto conn = std::move(cr.value());

			HttpRequest req{ekizu::net::HttpMethod::get, "/ping", 11};
			req.set(ekizu::net::http::field::host, "127.0.0.1");

			auto rr = conn.request(std::move(req), yield);
			if (!rr.has_value()) {
				done.set(rr.error());
				return;
			}

			if (rr.value().body() != "/ping") {
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

TEST_CASE("HttpConnection works with asio::deferred", "[http][tokens]") {
	IoPair io;
	auto server = make_server(io);

	test_support::CvWaiter<Result<>> done;

	asio::spawn(
		io.cb,
		[&](asio::yield_context yield) {
			auto create_op = HttpConnection::connect(
				io.net.get_executor(), server->base_url(), asio::deferred);
			auto cr = std::move(create_op)(yield);
			if (!cr.has_value()) {
				done.set(cr.error());
				return;
			}

			auto conn = std::move(cr.value());

			HttpRequest req{ekizu::net::HttpMethod::get, "/ping", 11};
			req.set(ekizu::net::http::field::host, "127.0.0.1");

			auto req_op = conn.request(std::move(req), asio::deferred);
			auto rr = std::move(req_op)(yield);
			if (!rr.has_value()) {
				done.set(rr.error());
				return;
			}

			if (rr.value().body() != "/ping") {
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

TEST_CASE("HttpConnection works with co_spawn/use_awaitable",
		  "[http][tokens]") {
	IoPair io;
	auto server = make_server(io);

	test_support::CvWaiter<Result<>> done;
	test_support::CvWaiter<std::thread::id> ran_tid;

	asio::co_spawn(
		io.cb,
		[&]() -> asio::awaitable<void> {
			ran_tid.set(std::this_thread::get_id());

			auto cr = co_await HttpConnection::connect(
				io.net.get_executor(), server->base_url(), asio::use_awaitable);
			if (!cr.has_value()) {
				done.set(cr.error());
				co_return;
			}

			auto conn = std::move(cr.value());

			HttpRequest req{ekizu::net::HttpMethod::get, "/ping", 11};
			req.set(ekizu::net::http::field::host, "127.0.0.1");

			auto rr =
				co_await conn.request(std::move(req), asio::use_awaitable);
			if (!rr.has_value()) {
				done.set(rr.error());
				co_return;
			}

			if (rr.value().body() != "/ping") {
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
