#ifndef TEST_SUPPORT_UDP_FIXTURE_HPP
#define TEST_SUPPORT_UDP_FIXTURE_HPP

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/udp.hpp>
#include <chrono>
#include <ekizu/udp.hpp>
#include <future>
#include <string>
#include <string_view>

#include "test_support/asio_test_harness.hpp"

namespace test_support::udp_fixture {

namespace asio = boost::asio;
using asio::ip::udp;
using namespace std::chrono_literals;

using ekizu::net::UdpSocket;

struct Peer {
	udp::socket sock;
	uint16_t port{};
};

inline Peer make_peer(test_support::IoPair &io) {
	Peer p{udp::socket(io.net), 0};
	p.sock.open(udp::v4());
	p.sock.bind({asio::ip::make_address("127.0.0.1"), 0});
	p.port = p.sock.local_endpoint().port();
	return p;
}

inline ekizu::Result<UdpSocket> await_bind(
	test_support::IoPair &io, std::string address,
	std::chrono::milliseconds timeout = 2000ms) {
	std::promise<ekizu::Result<UdpSocket>> p;
	auto f = p.get_future();

	UdpSocket::bind(
		io.net.get_executor(), std::move(address),
		asio::bind_executor(
			io.cb.get_executor(), [pp = std::move(p)](auto r) mutable {
				pp.set_value(std::move(r));
			}));

	if (!test_support::wait_ready(f, timeout)) {
		return make_error_code(boost::system::errc::timed_out);
	}
	return f.get();
}

inline ekizu::Result<> await_connect(
	test_support::IoPair &io, UdpSocket &s, std::string host, std::string port,
	std::chrono::milliseconds timeout = 2000ms) {
	std::promise<ekizu::Result<>> p;
	auto f = p.get_future();

	s.connect(std::move(host), std::move(port),
			  asio::bind_executor(
				  io.cb.get_executor(), [pp = std::move(p)](auto r) mutable {
					  pp.set_value(std::move(r));
				  }));

	if (!test_support::wait_ready(f, timeout)) {
		return make_error_code(boost::system::errc::timed_out);
	}
	return f.get();
}

inline ekizu::Result<UdpSocket> await_bind_connected(
	test_support::IoPair &io, std::string remote_host, std::string remote_port,
	std::chrono::milliseconds timeout = 2000ms) {
	auto br = await_bind(io, "127.0.0.1:0", timeout);
	if (!br) { return br.error(); }
	auto sock = std::move(br.value());
	auto cr = await_connect(
		io, sock, std::move(remote_host), std::move(remote_port), timeout);
	if (!cr) { return cr.error(); }
	return sock;
}

inline ekizu::Result<std::size_t> await_send(
	test_support::IoPair &io, UdpSocket &c, std::string_view msg,
	std::chrono::milliseconds timeout = 2000ms) {
	std::promise<ekizu::Result<std::size_t>> p;
	auto f = p.get_future();

	auto bytes = boost::span(
		reinterpret_cast<const std::byte *>(msg.data()), msg.size());

	c.send(bytes, asio::bind_executor(io.cb.get_executor(),
									  [pp = std::move(p)](auto r) mutable {
										  pp.set_value(std::move(r));
									  }));

	if (!test_support::wait_ready(f, timeout)) {
		return make_error_code(boost::system::errc::timed_out);
	}
	return f.get();
}

inline ekizu::Result<std::string> await_receive(
	test_support::IoPair &io, UdpSocket &c,
	std::chrono::milliseconds timeout = 2000ms) {
	std::promise<ekizu::Result<std::string>> p;
	auto f = p.get_future();

	c.receive(asio::bind_executor(
		io.cb.get_executor(),
		[pp = std::move(p)](auto r) mutable { pp.set_value(std::move(r)); }));

	if (!test_support::wait_ready(f, timeout)) {
		return make_error_code(boost::system::errc::timed_out);
	}
	return f.get();
}

}  // namespace test_support::udp_fixture

#endif	// TEST_SUPPORT_UDP_FIXTURE_HPP
