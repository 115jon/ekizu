#include <array>
#include <boost/asio/bind_executor.hpp>
#include <boost/core/span.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstring>
#include <future>
#include <string>

#include "ekizu/ws.hpp"
#include "test_support/ws_fixture.hpp"

namespace asio = boost::asio;
using namespace std::chrono_literals;

using test_support::IoPair;
using test_support::ws_fixture::await_ws_connect;
using test_support::ws_fixture::make_server;

using ekizu::Result;
using ekizu::net::WebSocketClient;
using ekizu::net::WebSocketMessage;

TEST_CASE(
	"WebSocketClient send_bytes does not depend on caller buffer lifetime",
	"[ws][buffer_lifetime]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_ws_connect(io, server->url());
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	std::promise<Result<WebSocketMessage>> recv_p;
	auto recv_f = recv_p.get_future();
	client.read(asio::bind_executor(
		io.cb.get_executor(),
		[p = std::move(recv_p)](Result<WebSocketMessage> r) mutable {
			p.set_value(std::move(r));
		}));

	std::promise<Result<>> send_p;
	auto send_f = send_p.get_future();

	std::array<std::byte, 256> tx{};
	for (std::size_t i = 0; i < tx.size(); ++i) {
		tx[i] = static_cast<std::byte>(i & 0xFF);
	}
	std::string expected(reinterpret_cast<const char *>(tx.data()), tx.size());

	client.send_bytes(
		boost::span<const std::byte>(tx.data(), tx.size()),
		asio::bind_executor(
			io.cb.get_executor(),
			[p = std::move(send_p)](Result<> r) mutable { p.set_value(r); }));

	std::memset(tx.data(), 0xCD, tx.size());

	REQUIRE(test_support::wait_ready(send_f, 2s));
	auto sr = send_f.get();
	REQUIRE(sr.has_value());

	REQUIRE(test_support::wait_ready(recv_f, 2s));
	auto rr = recv_f.get();
	REQUIRE(rr.has_value());
	REQUIRE(rr.value().is_binary);
	REQUIRE(rr.value().payload == expected);
}

TEST_CASE("WebSocketClient send does not depend on caller string lifetime",
		  "[ws][buffer_lifetime]") {
	IoPair io;
	auto server = make_server(io);

	auto cr = await_ws_connect(io, server->url());
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	std::promise<Result<WebSocketMessage>> recv_p;
	auto recv_f = recv_p.get_future();
	client.read(asio::bind_executor(
		io.cb.get_executor(),
		[p = std::move(recv_p)](Result<WebSocketMessage> r) mutable {
			p.set_value(std::move(r));
		}));

	std::promise<Result<>> send_p;
	auto send_f = send_p.get_future();

	std::string expected;
	expected.resize(256 * 1024);
	for (std::size_t i = 0; i < expected.size(); ++i) {
		expected[i] = static_cast<char>('a' + (i % 26));
	}

	std::string tmp = expected;
	client.send(
		std::string_view(tmp),
		asio::bind_executor(
			io.cb.get_executor(),
			[p = std::move(send_p)](Result<> r) mutable { p.set_value(r); }));

	std::fill(tmp.begin(), tmp.end(), static_cast<char>(0xCD));

	REQUIRE(test_support::wait_ready(send_f, 5s));
	auto sr = send_f.get();
	REQUIRE(sr.has_value());

	REQUIRE(test_support::wait_ready(recv_f, 5s));
	auto rr = recv_f.get();
	REQUIRE(rr.has_value());
	REQUIRE_FALSE(rr.value().is_binary);
	REQUIRE(rr.value().payload == expected);
}