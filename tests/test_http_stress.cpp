#include <boost/asio/post.hpp>
#include <catch2/catch_test_macros.hpp>
#include <future>
#include <string>
#include <vector>

#include "ekizu/http.hpp"
#include "test_support/http_fixture.hpp"

namespace asio = boost::asio;

using namespace std::chrono_literals;

using test_support::IoPair;
using test_support::http_fixture::await_http_connect;
using test_support::http_fixture::make_server;

using ekizu::Result;
using ekizu::net::HttpConnection;
using ekizu::net::HttpRequest;
using ekizu::net::HttpResponse;

TEST_CASE("HTTP stress: many sequential requests on one connection",
		  "[http][stress]") {
	IoPair io;
	auto server = make_server(io);
	server->clear_seen();

	auto cr = await_http_connect(io, server->base_url());
	REQUIRE(cr.has_value());

	auto conn = std::make_shared<HttpConnection>(std::move(cr.value()));

	constexpr int N = 300;
	std::vector<std::promise<Result<HttpResponse>>> ps(N);
	std::vector<std::future<Result<HttpResponse>>> fs;
	fs.reserve(N);

	for (size_t i = 0; i < N; ++i) { fs.push_back(ps[i].get_future()); }

	asio::post(io.net, [&io, conn, &ps]() mutable {
		for (size_t i = 0; i < N; ++i) {
			const auto target = "/seq/" + std::to_string(i);

			HttpRequest req{ekizu::net::HttpMethod::get, target, 11};
			req.set(ekizu::net::http::field::host, "127.0.0.1");

			conn->request(
				std::move(req),
				asio::bind_executor(
					io.cb.get_executor(),
					[p = std::move(ps[i])](Result<HttpResponse> r) mutable {
						p.set_value(std::move(r));
					}));
		}
	});

	REQUIRE(server->wait_seen(static_cast<std::size_t>(N), 5s));

	for (size_t i = 0; i < N; ++i) {
		REQUIRE(test_support::wait_ready(fs[i], 10s));
		auto r = fs[i].get();
		REQUIRE(r.has_value());
		REQUIRE(r.value().result() == ekizu::net::HttpStatus::ok);
		REQUIRE(r.value().body() == ("/seq/" + std::to_string(i)));
	}
}

TEST_CASE("HTTP stress: many get() operations", "[http][stress]") {
	IoPair io;
	auto server = make_server(io);

	constexpr int N = 300;
	std::vector<std::promise<Result<HttpResponse>>> ps(N);
	std::vector<std::future<Result<HttpResponse>>> fs;
	fs.reserve(N);

	for (size_t i = 0; i < N; ++i) { fs.push_back(ps[i].get_future()); }

	for (size_t i = 0; i < N; ++i) {
		HttpConnection::get(
			io.net.get_executor(), server->base_url() + "ping",
			asio::bind_executor(
				io.cb.get_executor(),
				[p = std::move(ps[i])](Result<HttpResponse> r) mutable {
					p.set_value(std::move(r));
				}));
	}

	for (size_t i = 0; i < N; ++i) {
		REQUIRE(test_support::wait_ready(fs[i], 10s));
		auto r = fs[i].get();
		REQUIRE(r.has_value());
		REQUIRE(r.value().result() == ekizu::net::HttpStatus::ok);
		REQUIRE(r.value().body() == "/ping");
	}
}
