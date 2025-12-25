#include <array>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/core/span.hpp>
#include <catch2/catch_test_macros.hpp>
#include <ekizu/udp.hpp>
#include <future>
#include <set>
#include <string>

#include "test_support/udp_fixture.hpp"

namespace asio = boost::asio;
using asio::ip::udp;
using namespace std::chrono_literals;

TEST_CASE("UdpSocket overlapping receives deliver distinct datagrams",
		  "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);

	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	std::promise<ekizu::Result<std::string>> r1_p;
	std::promise<ekizu::Result<std::string>> r2_p;
	auto r1_f = r1_p.get_future();
	auto r2_f = r2_p.get_future();

	client.receive(asio::bind_executor(
		io.cb.get_executor(),
		[p = std::move(r1_p)](ekizu::Result<std::string> r) mutable {
			p.set_value(std::move(r));
		}));

	client.receive(asio::bind_executor(
		io.cb.get_executor(),
		[p = std::move(r2_p)](ekizu::Result<std::string> r) mutable {
			p.set_value(std::move(r));
		}));

	// Peer learns the client's ephemeral port from a ping, then replies with
	// two distinct datagrams back-to-back.
	std::array<char, 64> buf{};
	udp::endpoint sender;
	peer.sock.async_receive_from(
		asio::buffer(buf), sender,
		[&](boost::system::error_code ec, std::size_t /*n*/) mutable {
			if (ec) { return; }

			const std::string m1 = "overlap_one";
			const std::string m2 = "overlap_two";

			boost::system::error_code ec2;
			peer.sock.send_to(asio::buffer(m1), sender, 0, ec2);
			peer.sock.send_to(asio::buffer(m2), sender, 0, ec2);
			(void)ec2;
		});

	// Trigger peer's reply.
	const std::string ping = "ping";
	auto bytes = boost::span<const std::byte>(
		reinterpret_cast<const std::byte *>(ping.data()), ping.size());

	std::promise<ekizu::Result<std::size_t>> sent_p;
	auto sent_f = sent_p.get_future();
	client.send(
		bytes,
		asio::bind_executor(
			io.cb.get_executor(),
			[pp = std::move(sent_p)](ekizu::Result<std::size_t> r) mutable {
				pp.set_value(r);
			}));

	REQUIRE(test_support::wait_ready(sent_f, 2000ms));
	auto sr = sent_f.get();
	REQUIRE(sr.has_value());

	REQUIRE(test_support::wait_ready(r1_f, 2000ms));
	REQUIRE(test_support::wait_ready(r2_f, 2000ms));

	auto rr1 = r1_f.get();
	auto rr2 = r2_f.get();

	REQUIRE(rr1.has_value());
	REQUIRE(rr2.has_value());

	std::set<std::string> got{rr1.value(), rr2.value()};
	REQUIRE(got == std::set<std::string>{"overlap_one", "overlap_two"});
}
