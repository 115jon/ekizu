#include <atomic>
#include <boost/asio/bind_executor.hpp>
#include <catch2/catch_test_macros.hpp>
#include <future>
#include <thread>

#include "ekizu/ws.hpp"
#include "test_support/ws_fixture.hpp"

namespace asio = boost::asio;
using namespace std::chrono_literals;

using test_support::IoPair;
using test_support::ws_fixture::await_ws_connect;
using test_support::ws_fixture::await_ws_send;
using test_support::ws_fixture::make_server;

using ekizu::Result;
using ekizu::net::WebSocketClient;
using ekizu::net::WebSocketMessage;

TEST_CASE("WebSocketClient::connect completes on associated executor", "[ws]") {
	IoPair io;
	auto server = make_server(io);

	std::promise<std::thread::id> ran_p;
	std::promise<Result<WebSocketClient>> res_p;
	auto ran_f = ran_p.get_future();
	auto res_f = res_p.get_future();

	WebSocketClient::connect(
		io.net.get_executor(), server->url(),
		asio::bind_executor(io.cb.get_executor(),
							[rp = std::move(ran_p), rvp = std::move(res_p)](
								Result<WebSocketClient> r) mutable {
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
	REQUIRE(res.value().is_open());
}

TEST_CASE("WebSocketClient send delivers frame to echo server", "[ws]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_ws_connect(io, server->url());
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	// Start read to receive echo.
	std::promise<Result<WebSocketMessage>> recv_p;
	auto recv_f = recv_p.get_future();

	client.read(asio::bind_executor(
		io.cb.get_executor(),
		[p = std::move(recv_p)](Result<WebSocketMessage> r) mutable {
			p.set_value(std::move(r));
		}));

	auto sr = await_ws_send(io, client, "ping");
	REQUIRE(sr.has_value());

	REQUIRE(test_support::wait_ready(recv_f, 2s));
	auto rr = recv_f.get();
	REQUIRE(rr.has_value());
	REQUIRE(rr.value().payload == "ping");
	REQUIRE_FALSE(rr.value().is_binary);
}

TEST_CASE("WebSocketClient read echo roundtrip", "[ws]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_ws_connect(io, server->url());
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	// Start read first.
	std::promise<Result<WebSocketMessage>> recv_p;
	std::promise<std::thread::id> recv_tid_p;
	auto recv_f = recv_p.get_future();
	auto recv_tid_f = recv_tid_p.get_future();

	client.read(asio::bind_executor(
		io.cb.get_executor(),
		[p = std::move(recv_p),
		 tp = std::move(recv_tid_p)](Result<WebSocketMessage> r) mutable {
			tp.set_value(std::this_thread::get_id());
			p.set_value(std::move(r));
		}));

	auto sr = await_ws_send(io, client, "ping");
	REQUIRE(sr.has_value());

	REQUIRE(test_support::wait_ready(recv_f, 2s));
	REQUIRE(test_support::wait_ready(recv_tid_f, 2s));

	auto rr = recv_f.get();
	REQUIRE(rr.has_value());
	REQUIRE(rr.value().payload == "ping");
	REQUIRE_FALSE(rr.value().is_binary);
	REQUIRE(recv_tid_f.get() == io.cb_tid);
}

TEST_CASE("WebSocketClient read completes with error if closed while pending",
		  "[ws]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_ws_connect(io, server->url());
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	auto pp = std::make_shared<std::promise<Result<WebSocketMessage>>>();
	auto f = pp->get_future();

	// Order on net thread: start read then close.
	asio::post(io.net, [&] {
		client.read(asio::bind_executor(
			io.cb.get_executor(), [pp](Result<WebSocketMessage> r) mutable {
				pp->set_value(std::move(r));
			}));
		client.close(
			ekizu::net::WebSocketCloseCode::normal,
			asio::bind_executor(io.cb.get_executor(), [](Result<>) {}));
	});

	REQUIRE(test_support::wait_ready(f, 2s));
	auto r = f.get();
	REQUIRE_FALSE(r.has_value());
}

TEST_CASE("WebSocketClient close cancels queued reads", "[ws]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_ws_connect(io, server->url());
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	constexpr int N = 10;
	std::vector<std::promise<Result<WebSocketMessage>>> ps(N);
	std::vector<std::future<Result<WebSocketMessage>>> fs;
	fs.reserve(N);

	// Start N reads (queue them).
	for (size_t i = 0; i < N; ++i) {
		fs.push_back(ps[i].get_future());
		client.read(asio::bind_executor(
			io.cb.get_executor(),
			[p = std::move(ps[i])](Result<WebSocketMessage> r) mutable {
				p.set_value(std::move(r));
			}));
	}

	// Close on net thread to serialize with internal async ops.
	asio::post(io.net, [&] {
		client.close(
			ekizu::net::WebSocketCloseCode::normal,
			asio::bind_executor(io.cb.get_executor(), [](Result<>) {}));
	});

	// All N must complete with error (not hang).
	for (auto &f : fs) {
		REQUIRE(test_support::wait_ready(f, 2s));
		auto r = f.get();
		REQUIRE_FALSE(r.has_value());
	}
}

TEST_CASE(
	"WebSocketClient destruction from different thread completes all handlers",
	"[ws]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_ws_connect(io, server->url());
	REQUIRE(cr.has_value());

	constexpr int N = 10;
	std::vector<std::promise<Result<WebSocketMessage>>> ps(N);
	std::vector<std::future<Result<WebSocketMessage>>> fs;
	fs.reserve(N);

	auto c = std::make_shared<WebSocketClient>(std::move(cr.value()));

	for (size_t i = 0; i < N; ++i) {
		fs.push_back(ps[i].get_future());
		c->read(asio::bind_executor(
			io.cb.get_executor(),
			[p = std::move(ps[i])](Result<WebSocketMessage> r) mutable {
				p.set_value(std::move(r));
			}));
	}

	// Destroy from a SEPARATE thread, not io.net
	std::promise<void> destroyed_p;
	auto destroyed_f = destroyed_p.get_future();
	std::thread destroyer([&c, &destroyed_p]() mutable {
		c.reset();
		destroyed_p.set_value();
	});

	REQUIRE(test_support::wait_ready(destroyed_f, 2000ms));
	destroyer.join();

	// All handlers must still complete
	for (auto &f : fs) {
		REQUIRE(test_support::wait_ready(f, 5000ms));
		auto r = f.get();
		REQUIRE_FALSE(r.has_value());
	}
}

TEST_CASE(
	"WebSocketClient destruction on running executor completes all handlers",
	"[ws]") {
	test_support::IoPair io;
	auto server = test_support::ws_fixture::make_server(io);

	auto cr = test_support::ws_fixture::await_ws_connect(io, server->url());
	REQUIRE(cr.has_value());

	std::atomic<int> completed{0};
	constexpr int N = 20;

	auto client =
		std::make_shared<ekizu::net::WebSocketClient>(std::move(cr.value()));

	for (int i = 0; i < N; ++i) {
		client->read(asio::bind_executor(
			io.cb.get_executor(),
			[&completed](ekizu::Result<ekizu::net::WebSocketMessage>) mutable {
				completed.fetch_add(1, std::memory_order_relaxed);
			}));
	}

	std::promise<void> destroyed_p;
	auto destroyed_f = destroyed_p.get_future();

	asio::post(io.net, [client = std::move(client), &destroyed_p]() mutable {
		client.reset();	 // ← SYNCHRONOUS destruction
		destroyed_p.set_value();
	});

	REQUIRE(test_support::wait_ready(destroyed_f, 2s));

	auto deadline = std::chrono::steady_clock::now() + 5s;
	while (completed.load() < N &&
		   std::chrono::steady_clock::now() < deadline) {
		std::this_thread::sleep_for(10ms);
	}

	REQUIRE(completed.load(std::memory_order_relaxed) == N);
}
