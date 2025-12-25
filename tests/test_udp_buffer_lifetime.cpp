#include <array>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/core/span.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstring>
#include <ekizu/udp.hpp>
#include <future>
#include <string>

#include "test_support/udp_fixture.hpp"

namespace asio = boost::asio;
using asio::ip::udp;
using namespace std::chrono_literals;

TEST_CASE("UdpSocket send does not depend on caller buffer lifetime (stack)",
		  "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);

	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	std::array<char, 512> rx_buf{};
	udp::endpoint from;

	std::promise<std::string> got_p;
	auto got_f = got_p.get_future();

	peer.sock.async_receive_from(
		asio::buffer(rx_buf), from,
		[p = std::move(got_p),
		 &rx_buf](boost::system::error_code ec, std::size_t n) mutable {
			if (ec) {
				p.set_value(std::string{});
				return;
			}
			p.set_value(std::string(rx_buf.data(), n));
		});

	std::string expected;
	expected.reserve(256);
	for (int i = 0; i < 256; i++) {
		expected.push_back(static_cast<char>('A' + (i % 26)));
	}

	std::promise<ekizu::Result<std::size_t>> sent_p;
	auto sent_f = sent_p.get_future();

	// Initiate send with a stack buffer, then overwrite it immediately.
	{
		std::array<char, 256> tx_buf{};
		std::memcpy(tx_buf.data(), expected.data(), expected.size());

		auto bytes = boost::span<const std::byte>(
			reinterpret_cast<const std::byte *>(tx_buf.data()),
			expected.size());

		client.send(
			bytes,
			asio::bind_executor(
				io.cb.get_executor(),
				[pp = std::move(sent_p)](ekizu::Result<std::size_t> r) mutable {
					pp.set_value(r);
				}));

		std::memset(tx_buf.data(), 0xCD, tx_buf.size());
	}

	REQUIRE(test_support::wait_ready(sent_f, 2000ms));
	auto sr = sent_f.get();
	REQUIRE(sr.has_value());
	REQUIRE(sr.value() == expected.size());

	REQUIRE(test_support::wait_ready(got_f, 2000ms));
	REQUIRE(got_f.get() == expected);
}

TEST_CASE(
	"UdpSocket send does not depend on caller buffer lifetime (temporary)",
	"[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);

	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	std::array<char, 2048> rx_buf{};
	udp::endpoint from;

	std::promise<std::string> got_p;
	auto got_f = got_p.get_future();

	peer.sock.async_receive_from(
		asio::buffer(rx_buf), from,
		[p = std::move(got_p),
		 &rx_buf](boost::system::error_code ec, std::size_t n) mutable {
			if (ec) {
				p.set_value(std::string{});
				return;
			}
			p.set_value(std::string(rx_buf.data(), n));
		});

	std::string expected;
	expected.reserve(1024);
	for (int i = 0; i < 1024; i++) {
		expected.push_back(static_cast<char>('a' + (i % 26)));
	}

	std::promise<ekizu::Result<std::size_t>> sent_p;
	auto sent_f = sent_p.get_future();

	// Build a temporary string, initiate send, then destroy the string.
	{
		std::string tmp = expected;

		auto bytes = boost::span<const std::byte>(
			reinterpret_cast<const std::byte *>(tmp.data()), tmp.size());

		client.send(
			bytes,
			asio::bind_executor(
				io.cb.get_executor(),
				[pp = std::move(sent_p)](ekizu::Result<std::size_t> r) mutable {
					pp.set_value(r);
				}));
	}

	REQUIRE(test_support::wait_ready(sent_f, 2000ms));
	auto sr = sent_f.get();
	REQUIRE(sr.has_value());
	REQUIRE(sr.value() == expected.size());

	REQUIRE(test_support::wait_ready(got_f, 2000ms));
	REQUIRE(got_f.get() == expected);
}
