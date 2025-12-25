#include <boost/asio/bind_executor.hpp>
#include <boost/asio/post.hpp>
#include <catch2/catch_test_macros.hpp>
#include <future>
#include <thread>
#include <vector>

#include "ekizu/http.hpp"
#include "test_support/http_fixture.hpp"

namespace asio = boost::asio;

using namespace std::chrono_literals;

using test_support::IoPair;
using test_support::http_fixture::await_http_connect;
using test_support::http_fixture::await_http_request;
using test_support::http_fixture::make_server;

using ekizu::Result;
using ekizu::net::HttpConnection;
using ekizu::net::HttpRequest;
using ekizu::net::HttpResponse;

TEST_CASE("HttpConnection::connect completes on associated executor",
		  "[http]") {
	IoPair io;
	auto server = make_server(io);

	std::promise<std::thread::id> ran_p;
	std::promise<Result<HttpConnection>> res_p;

	auto ran_f = ran_p.get_future();
	auto res_f = res_p.get_future();

	HttpConnection::connect(
		io.net.get_executor(), server->base_url(),
		asio::bind_executor(io.cb.get_executor(),
							[rp = std::move(ran_p), rvp = std::move(res_p)](
								Result<HttpConnection> r) mutable {
								rp.set_value(std::this_thread::get_id());
								rvp.set_value(std::move(r));
							}));

	REQUIRE(test_support::wait_ready(ran_f, 2s));
	REQUIRE(test_support::wait_ready(res_f, 2s));

	const auto ran_tid = ran_f.get();
	const auto res = res_f.get();

	REQUIRE(res.has_value());
	REQUIRE(ran_tid == io.cb_tid);
	REQUIRE(ran_tid != io.net_tid);
}

TEST_CASE("HttpConnection::connect failure completes on associated executor",
		  "[http]") {
	IoPair io;

	SECTION("Invalid URI") {
		std::promise<std::thread::id> ran_p;
		std::promise<Result<HttpConnection>> res_p;

		auto ran_f = ran_p.get_future();
		auto res_f = res_p.get_future();

		HttpConnection::connect(
			io.net.get_executor(), "not a url",
			asio::bind_executor(io.cb.get_executor(),
								[rp = std::move(ran_p), rvp = std::move(res_p)](
									Result<HttpConnection> r) mutable {
									rp.set_value(std::this_thread::get_id());
									rvp.set_value(std::move(r));
								}));

		REQUIRE(test_support::wait_ready(ran_f, 2s));
		REQUIRE(test_support::wait_ready(res_f, 2s));

		REQUIRE(ran_f.get() == io.cb_tid);

		auto res = res_f.get();
		REQUIRE_FALSE(res.has_value());
	}

	SECTION("Unsupported scheme") {
		std::promise<std::thread::id> ran_p;
		std::promise<Result<HttpConnection>> res_p;

		auto ran_f = ran_p.get_future();
		auto res_f = res_p.get_future();

		HttpConnection::connect(
			io.net.get_executor(), "ftp://127.0.0.1:1234/",
			asio::bind_executor(io.cb.get_executor(),
								[rp = std::move(ran_p), rvp = std::move(res_p)](
									Result<HttpConnection> r) mutable {
									rp.set_value(std::this_thread::get_id());
									rvp.set_value(std::move(r));
								}));

		REQUIRE(test_support::wait_ready(ran_f, 2s));
		REQUIRE(test_support::wait_ready(res_f, 2s));

		REQUIRE(ran_f.get() == io.cb_tid);

		auto res = res_f.get();
		REQUIRE_FALSE(res.has_value());
	}

	SECTION("Refused by fixture (acceptor closed)") {
		auto server = make_server(io);
		server->stop_accepting();

		std::promise<std::thread::id> ran_p;
		std::promise<Result<HttpConnection>> res_p;

		auto ran_f = ran_p.get_future();
		auto res_f = res_p.get_future();

		HttpConnection::connect(
			io.net.get_executor(), server->base_url(),
			asio::bind_executor(io.cb.get_executor(),
								[rp = std::move(ran_p), rvp = std::move(res_p)](
									Result<HttpConnection> r) mutable {
									rp.set_value(std::this_thread::get_id());
									rvp.set_value(std::move(r));
								}));

		REQUIRE(test_support::wait_ready(ran_f, 5s));
		REQUIRE(test_support::wait_ready(res_f, 5s));

		REQUIRE(ran_f.get() == io.cb_tid);

		auto res = res_f.get();
		REQUIRE_FALSE(res.has_value());
	}
}

TEST_CASE("HttpConnection request/response roundtrip", "[http]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_http_connect(io, server->base_url());
	REQUIRE(cr.has_value());

	auto conn = std::move(cr.value());

	HttpRequest req{ekizu::net::HttpMethod::get, "/ping", 11};
	req.set(ekizu::net::http::field::host, "127.0.0.1");

	std::promise<std::thread::id> tid_p;
	std::promise<Result<HttpResponse>> res_p;

	auto tid_f = tid_p.get_future();
	auto res_f = res_p.get_future();

	conn.request(
		std::move(req),
		asio::bind_executor(io.cb.get_executor(),
							[tp = std::move(tid_p), rp = std::move(res_p)](
								Result<HttpResponse> r) mutable {
								tp.set_value(std::this_thread::get_id());
								rp.set_value(std::move(r));
							}));

	REQUIRE(test_support::wait_ready(tid_f, 2s));
	REQUIRE(test_support::wait_ready(res_f, 2s));

	REQUIRE(tid_f.get() == io.cb_tid);

	auto rr = res_f.get();
	REQUIRE(rr.has_value());
	REQUIRE(rr.value().result() == ekizu::net::HttpStatus::ok);
	REQUIRE(rr.value().body() == "/ping");
}

TEST_CASE("HttpConnection::get success and invalid URI", "[http]") {
	IoPair io;
	auto server = make_server(io);

	SECTION("Success") {
		std::promise<std::thread::id> ran_p;
		std::promise<Result<HttpResponse>> res_p;

		auto ran_f = ran_p.get_future();
		auto res_f = res_p.get_future();

		HttpConnection::get(
			io.net.get_executor(), server->base_url() + "ping",
			asio::bind_executor(io.cb.get_executor(),
								[rp = std::move(ran_p), rvp = std::move(res_p)](
									Result<HttpResponse> r) mutable {
									rp.set_value(std::this_thread::get_id());
									rvp.set_value(std::move(r));
								}));

		REQUIRE(test_support::wait_ready(ran_f, 2s));
		REQUIRE(test_support::wait_ready(res_f, 2s));

		REQUIRE(ran_f.get() == io.cb_tid);

		auto res = res_f.get();
		REQUIRE(res.has_value());
		REQUIRE(res.value().result() == ekizu::net::HttpStatus::ok);
		REQUIRE(res.value().body() == "/ping");
	}

	SECTION("Invalid URI") {
		std::promise<std::thread::id> ran_p;
		std::promise<Result<HttpResponse>> res_p;

		auto ran_f = ran_p.get_future();
		auto res_f = res_p.get_future();

		HttpConnection::get(
			io.net.get_executor(), "not a url",
			asio::bind_executor(io.cb.get_executor(),
								[rp = std::move(ran_p), rvp = std::move(res_p)](
									Result<HttpResponse> r) mutable {
									rp.set_value(std::this_thread::get_id());
									rvp.set_value(std::move(r));
								}));

		REQUIRE(test_support::wait_ready(ran_f, 2s));
		REQUIRE(test_support::wait_ready(res_f, 2s));

		REQUIRE(ran_f.get() == io.cb_tid);

		auto res = res_f.get();
		REQUIRE_FALSE(res.has_value());
	}
}

TEST_CASE("HttpConnection moved-from request returns operation_not_permitted",
		  "[http]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_http_connect(io, server->base_url());
	REQUIRE(cr.has_value());

	HttpConnection a = std::move(cr.value());
	HttpConnection b = std::move(a);

	std::promise<std::thread::id> tid_p;
	std::promise<Result<HttpResponse>> res_p;

	auto tid_f = tid_p.get_future();
	auto res_f = res_p.get_future();

	HttpRequest req{ekizu::net::HttpMethod::get, "/ping", 11};
	req.set(ekizu::net::http::field::host, "127.0.0.1");

	a.request(std::move(req),
			  asio::bind_executor(
				  io.cb.get_executor(),
				  [tp = std::move(tid_p),
				   rp = std::move(res_p)](Result<HttpResponse> r) mutable {
					  tp.set_value(std::this_thread::get_id());
					  rp.set_value(std::move(r));
				  }));

	REQUIRE(test_support::wait_ready(tid_f, 2s));
	REQUIRE(test_support::wait_ready(res_f, 2s));

	REQUIRE(tid_f.get() == io.cb_tid);

	auto rr = res_f.get();
	REQUIRE_FALSE(rr.has_value());
	REQUIRE(rr.error() == boost::system::errc::operation_not_permitted);

	(void)b;
}

TEST_CASE("HttpConnection::get /slow succeeds", "[http]") {
	IoPair io;
	auto server = make_server(io);

	std::promise<std::thread::id> ran_p;
	std::promise<Result<HttpResponse>> res_p;

	auto ran_f = ran_p.get_future();
	auto res_f = res_p.get_future();

	HttpConnection::get(
		io.net.get_executor(), server->base_url() + "slow",
		asio::bind_executor(io.cb.get_executor(),
							[rp = std::move(ran_p), rvp = std::move(res_p)](
								Result<HttpResponse> r) mutable {
								rp.set_value(std::this_thread::get_id());
								rvp.set_value(std::move(r));
							}));

	REQUIRE(test_support::wait_ready(ran_f, 5s));
	REQUIRE(test_support::wait_ready(res_f, 5s));

	REQUIRE(ran_f.get() == io.cb_tid);

	auto rr = res_f.get();
	REQUIRE(rr.has_value());
	REQUIRE(rr.value().result() == ekizu::net::HttpStatus::ok);
	REQUIRE(rr.value().body() == "/slow");
}

TEST_CASE("HttpConnection destruction cancels queued requests", "[http]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_http_connect(io, server->base_url());
	REQUIRE(cr.has_value());

	auto conn = std::make_shared<HttpConnection>(std::move(cr.value()));

	constexpr int N = 10;
	std::vector<std::promise<Result<HttpResponse>>> ps(N);
	std::vector<std::future<Result<HttpResponse>>> fs;
	fs.reserve(N);

	for (size_t i = 0; i < N; ++i) { fs.push_back(ps[i].get_future()); }

	asio::post(io.net, [&io, conn = std::move(conn), &ps]() mutable {
		for (size_t i = 0; i < N; ++i) {
			HttpRequest req{ekizu::net::HttpMethod::get, "/slow", 11};
			req.set(ekizu::net::http::field::host, "127.0.0.1");

			conn->request(
				std::move(req),
				asio::bind_executor(
					io.cb.get_executor(),
					[p = std::move(ps[i])](Result<HttpResponse> r) mutable {
						p.set_value(std::move(r));
					}));
		}

		// Drop last owning reference immediately after queueing requests.
		conn.reset();
	});

	for (auto &f : fs) {
		REQUIRE(test_support::wait_ready(f, 5s));
		auto r = f.get();
		REQUIRE_FALSE(r.has_value());
		REQUIRE(r.error() == boost::system::errc::operation_canceled);
	}
}

TEST_CASE("HttpConnection destruction cancels in-flight request", "[http]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_http_connect(io, server->base_url());
	REQUIRE(cr.has_value());

	auto conn = std::make_shared<HttpConnection>(std::move(cr.value()));

	auto pp = std::make_shared<std::promise<Result<HttpResponse>>>();
	auto f = pp->get_future();

	asio::post(io.net, [&io, conn = std::move(conn), pp]() mutable {
		HttpRequest req{ekizu::net::HttpMethod::get, "/slow", 11};
		req.set(ekizu::net::http::field::host, "127.0.0.1");

		conn->request(
			std::move(req),
			asio::bind_executor(
				io.cb.get_executor(), [pp](Result<HttpResponse> r) mutable {
					pp->set_value(std::move(r));
				}));

		// Drop last owning reference while request should be in-flight.
		conn.reset();
	});

	REQUIRE(test_support::wait_ready(f, 5s));
	auto r = f.get();
	REQUIRE_FALSE(r.has_value());
	REQUIRE(r.error() == boost::system::errc::operation_canceled);
}

TEST_CASE("HttpConnection FIFO start order (server-observed)", "[http]") {
	IoPair io;
	auto server = make_server(io);
	server->clear_seen();

	auto cr = await_http_connect(io, server->base_url());
	REQUIRE(cr.has_value());

	auto conn = std::make_shared<HttpConnection>(std::move(cr.value()));

	constexpr int N = 10;
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

	REQUIRE(server->wait_seen(N, 2s));
	const auto seen = server->seen_targets();
	REQUIRE(seen.size() >= N);

	for (size_t i = 0; i < N; ++i) {
		REQUIRE(seen[static_cast<std::size_t>(i)] ==
				("/seq/" + std::to_string(i)));
	}

	for (size_t i = 0; i < N; ++i) {
		REQUIRE(test_support::wait_ready(fs[i], 5s));
		auto r = fs[i].get();
		REQUIRE(r.has_value());
		REQUIRE(r.value().body() == ("/seq/" + std::to_string(i)));
	}
}

TEST_CASE("HttpConnection destruction from different thread completes handlers",
		  "[http]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_http_connect(io, server->base_url());
	REQUIRE(cr.has_value());

	constexpr int N = 10;
	std::vector<std::promise<Result<HttpResponse>>> ps(N);
	std::vector<std::future<Result<HttpResponse>>> fs;
	fs.reserve(N);

	auto conn = std::make_shared<HttpConnection>(std::move(cr.value()));

	for (size_t i = 0; i < N; ++i) {
		fs.push_back(ps[i].get_future());

		HttpRequest req{ekizu::net::HttpMethod::get, "/slow", 11};
		req.set(ekizu::net::http::field::host, "127.0.0.1");

		conn->request(
			std::move(req),
			asio::bind_executor(
				io.cb.get_executor(),
				[p = std::move(ps[i])](Result<HttpResponse> r) mutable {
					p.set_value(std::move(r));
				}));
	}

	std::promise<void> destroyed_p;
	auto destroyed_f = destroyed_p.get_future();

	std::thread destroyer([&conn, &destroyed_p]() mutable {
		conn.reset();
		destroyed_p.set_value();
	});

	REQUIRE(test_support::wait_ready(destroyed_f, 2000ms));
	destroyer.join();

	for (auto &f : fs) {
		REQUIRE(test_support::wait_ready(f, 5s));
		auto r = f.get();
		REQUIRE_FALSE(r.has_value());
	}
}
