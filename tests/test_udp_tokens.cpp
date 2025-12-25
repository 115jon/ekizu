#include <fmt/core.h>

#include <array>
#include <boost/asio/detached.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <catch2/catch_test_macros.hpp>
#include <ekizu/udp.hpp>
#include <future>
#include <string>
#include <thread>

#if __has_include(<boost/asio/deferred.hpp>)
#include <boost/asio/deferred.hpp>
#define EKIZU_HAS_ASIO_DEFERRED 1
#endif

#if defined(__cpp_impl_coroutine) && __cpp_impl_coroutine >= 201902L && \
	__has_include(<boost/asio/awaitable.hpp>) && \
	__has_include(<boost/asio/co_spawn.hpp>) && \
	__has_include(<boost/asio/use_awaitable.hpp>)

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#define EKIZU_HAS_ASIO_AWAITABLE 1
#endif

#include "test_support/udp_fixture.hpp"

namespace asio = boost::asio;
using asio::ip::udp;
using namespace std::chrono_literals;

TEST_CASE("UdpSocket bind/connect/send/receive works with use_future",
		  "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);

	auto bind_f = ekizu::net::UdpSocket::bind(
		io.net.get_executor(), "127.0.0.1:0",
		asio::bind_executor(io.cb.get_executor(), asio::use_future));

	REQUIRE(bind_f.wait_for(2s) == std::future_status::ready);
	auto br = bind_f.get();
	REQUIRE(br.has_value());
	auto client = std::move(br.value());

	auto connect_f = client.connect(
		"127.0.0.1", std::to_string(peer.port),
		asio::bind_executor(io.cb.get_executor(), asio::use_future));

	REQUIRE(connect_f.wait_for(2s) == std::future_status::ready);
	auto cr = connect_f.get();
	REQUIRE(cr.has_value());

	// Start receive first (client side).
	auto recv_f = client.receive(
		asio::bind_executor(io.cb.get_executor(), asio::use_future));

	// Peer waits for ping then replies pong.
	std::array<char, 64> buf{};
	udp::endpoint sender;
	test_support::CvWaiter<std::string> got_ping;
	peer.sock.async_receive_from(
		asio::buffer(buf), sender,
		[&](boost::system::error_code ec, std::size_t n) mutable {
			if (ec) {
				got_ping.set(std::string{});
				return;
			}
			got_ping.set(std::string(buf.data(), n));
			const std::string pong = "pong";
			boost::system::error_code ec2;
			peer.sock.send_to(asio::buffer(pong), sender, 0, ec2);
			(void)ec2;
		});

	// Send ping (client side).
	const std::string ping = "ping";
	auto bytes = boost::span(
		reinterpret_cast<const std::byte *>(ping.data()), ping.size());

	auto send_f = client.send(
		bytes, asio::bind_executor(io.cb.get_executor(), asio::use_future));

	REQUIRE(send_f.wait_for(2s) == std::future_status::ready);
	auto sr = send_f.get();
	REQUIRE(sr.has_value());
	REQUIRE(sr.value() == ping.size());

	REQUIRE(got_ping.wait_for(2000ms));
	REQUIRE(got_ping.value.has_value());
	REQUIRE(*got_ping.value == "ping");

	REQUIRE(recv_f.wait_for(2s) == std::future_status::ready);
	auto rr = recv_f.get();
	REQUIRE(rr.has_value());
	REQUIRE(rr.value() == "pong");
}

TEST_CASE("UdpSocket callback style destroy while receive pending", "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);

	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());

	test_support::CvWaiter<ekizu::Result<std::string>> got_result;
	test_support::CvWaiter<std::thread::id> got_tid;

	// Start receive and then destroy the socket on the net context to mimic the
	// problematic scenario.
	asio::post(
		io.net,
		[c = std::make_shared<ekizu::net::UdpSocket>(std::move(cr.value())),
		 &io, &got_result, &got_tid]() mutable {
			c->receive(asio::bind_executor(
				io.cb.get_executor(),
				[&got_result, &got_tid](ekizu::Result<std::string> r) mutable {
					got_tid.set(std::this_thread::get_id());
					got_result.set(std::move(r));
				}));

			// Drop the last owning reference immediately after starting
			// receive.
			c.reset();
		});

	REQUIRE(got_result.wait_for(2000ms));
	REQUIRE(got_tid.wait_for(2000ms));

	// Completion must run on the callback thread/executor.
	REQUIRE(got_tid.value.has_value());
	REQUIRE(*got_tid.value == io.cb_tid);

	// Expect an error (no datagram ever sent).
	REQUIRE(got_result.value.has_value());
	REQUIRE_FALSE(got_result.value->has_value());
}

TEST_CASE("UdpSocket works with asio::spawn/yield", "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);

	test_support::CvWaiter<ekizu::Result<void>> done;
	test_support::CvWaiter<std::thread::id> ran_tid;

	asio::spawn(
		io.cb,
		[&](asio::yield_context yield) {
			ran_tid.set(std::this_thread::get_id());

			auto br = ekizu::net::UdpSocket::bind(
				io.net.get_executor(), "127.0.0.1:0", yield);
			if (!br.has_value()) {
				done.set(br.error());
				return;
			}

			auto client = std::move(br.value());

			auto cr =
				client.connect("127.0.0.1", std::to_string(peer.port), yield);
			if (!cr.has_value()) {
				done.set(cr.error());
				return;
			}

			// Peer: receive ping, then post pong (gives client time to start
			// receive).
			std::array<char, 64> buf{};
			udp::endpoint sender;
			peer.sock.async_receive_from(
				asio::buffer(buf), sender,
				[&](boost::system::error_code ec, std::size_t /*n*/) mutable {
					if (ec) { return; }

					const std::string pong = "pong";
					asio::post(peer.sock.get_executor(), [&, pong,
														  sender]() mutable {
						boost::system::error_code ec2;
						peer.sock.send_to(asio::buffer(pong), sender, 0, ec2);
						(void)ec2;
					});
				});

			const std::string ping = "ping";
			auto bytes = boost::span(
				reinterpret_cast<const std::byte *>(ping.data()), ping.size());

			auto sr = client.send(bytes, yield);
			if (!sr.has_value()) {
				done.set(sr.error());
				return;
			}

			auto rr = client.receive(yield);
			if (!rr.has_value()) {
				done.set(rr.error());
				return;
			}

			if (rr.value() != "pong") {
				done.set(make_error_code(boost::system::errc::protocol_error));
				return;
			}

			done.set(ekizu::outcome::success());
		},
		asio::detached);

	REQUIRE(ran_tid.wait_for(2000ms));
	REQUIRE(done.wait_for(2000ms));

	REQUIRE(ran_tid.value.has_value());
	REQUIRE(*ran_tid.value == io.cb_tid);

	REQUIRE(done.value.has_value());
	REQUIRE(done.value->has_value());
}

#ifdef EKIZU_HAS_ASIO_DEFERRED
TEST_CASE("UdpSocket works with asio::deferred", "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);

	test_support::CvWaiter<ekizu::Result<void>> done;

	asio::spawn(
		io.cb,
		[&](asio::yield_context yield) {
			auto bind_op = ekizu::net::UdpSocket::bind(
				io.net.get_executor(), "127.0.0.1:0", asio::deferred);
			auto br = std::move(bind_op)(yield);
			if (!br.has_value()) {
				done.set(br.error());
				return;
			}

			auto client = std::move(br.value());

			auto connect_op = client.connect(
				"127.0.0.1", std::to_string(peer.port), asio::deferred);
			auto cr = std::move(connect_op)(yield);
			if (!cr.has_value()) {
				done.set(cr.error());
				return;
			}

			// Peer: receive ping then reply pong.
			std::array<char, 64> buf{};
			udp::endpoint sender;
			peer.sock.async_receive_from(
				asio::buffer(buf), sender,
				[&](boost::system::error_code ec, std::size_t /*n*/) mutable {
					if (ec) { return; }

					const std::string pong = "pong";
					boost::system::error_code ec2;
					peer.sock.send_to(asio::buffer(pong), sender, 0, ec2);
					(void)ec2;
				});

			const std::string ping = "ping";
			auto bytes = boost::span(
				reinterpret_cast<const std::byte *>(ping.data()), ping.size());

			auto send_op = client.send(bytes, asio::deferred);
			auto sr = std::move(send_op)(yield);
			if (!sr.has_value()) {
				done.set(sr.error());
				return;
			}

			auto recv_op = client.receive(asio::deferred);
			auto rr = std::move(recv_op)(yield);
			if (!rr.has_value()) {
				done.set(rr.error());
				return;
			}

			if (rr.value() != "pong") {
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

#ifdef EKIZU_HAS_ASIO_AWAITABLE
TEST_CASE("UdpSocket works with co_spawn/use_awaitable", "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);

	test_support::CvWaiter<ekizu::Result<void>> done;
	test_support::CvWaiter<std::thread::id> ran_tid;

	asio::co_spawn(
		io.cb,
		[&]() -> asio::awaitable<void> {
			ran_tid.set(std::this_thread::get_id());

			auto br = co_await ekizu::net::UdpSocket::bind(
				io.net.get_executor(), "127.0.0.1:0", asio::use_awaitable);
			if (!br.has_value()) {
				done.set(br.error());
				co_return;
			}

			auto client = std::move(br.value());

			auto cr = co_await client.connect(
				"127.0.0.1", std::to_string(peer.port), asio::use_awaitable);
			if (!cr.has_value()) {
				done.set(cr.error());
				co_return;
			}

			// Peer: receive ping then reply pong.
			std::array<char, 64> buf{};
			udp::endpoint sender;
			peer.sock.async_receive_from(
				asio::buffer(buf), sender,
				[&](boost::system::error_code ec, std::size_t /*n*/) mutable {
					if (ec) { return; }

					const std::string pong = "pong";
					boost::system::error_code ec2;
					peer.sock.send_to(asio::buffer(pong), sender, 0, ec2);
					(void)ec2;
				});

			const std::string ping = "ping";
			auto bytes = boost::span(
				reinterpret_cast<const std::byte *>(ping.data()), ping.size());

			auto sr = co_await client.send(bytes, asio::use_awaitable);
			if (!sr.has_value()) {
				done.set(sr.error());
				co_return;
			}

			auto rr = co_await client.receive(asio::use_awaitable);
			if (!rr.has_value()) {
				done.set(rr.error());
				co_return;
			}

			if (rr.value() != "pong") {
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
	REQUIRE(*ran_tid.value == io.cb_tid);

	REQUIRE(done.value.has_value());
	REQUIRE(done.value->has_value());
}
#endif
