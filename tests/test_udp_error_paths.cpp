#include <boost/asio/ip/udp.hpp>
#include <boost/asio/post.hpp>
#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <string>
#include <thread>

#include "test_support/udp_fixture.hpp"

namespace asio = boost::asio;
using asio::ip::udp;
using namespace std::chrono_literals;

using test_support::udp_fixture::await_bind;
using test_support::udp_fixture::await_bind_connected;
using test_support::udp_fixture::await_connect;
using test_support::udp_fixture::await_send;
using test_support::udp_fixture::make_peer;

TEST_CASE("UdpSocket connect fails for invalid host (resolve error path)",
		  "[udp]") {
	test_support::IoPair io;

	auto br = await_bind(io, "127.0.0.1:0");
	REQUIRE(br.has_value());
	auto sock = std::move(br.value());

	auto r = await_connect(io, sock, "definitely.not.a.host.invalid", "9999");
	REQUIRE_FALSE(r.has_value());
}

TEST_CASE(
	"UdpSocket connect fails when service is invalid (resolve error path)",
	"[udp]") {
	test_support::IoPair io;

	auto br = await_bind(io, "127.0.0.1:0");
	REQUIRE(br.has_value());
	auto sock = std::move(br.value());

	auto r = await_connect(io, sock, "127.0.0.1", "notaport");
	REQUIRE_FALSE(r.has_value());
}

TEST_CASE("UdpSocket close is idempotent", "[udp]") {
	test_support::IoPair io;
	auto peer = make_peer(io);

	auto cr = await_bind_connected(io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	REQUIRE(client.close().has_value());
	REQUIRE(client.close().has_value());
	REQUIRE(client.close().has_value());
}

TEST_CASE("UdpSocket send after close completes with error", "[udp]") {
	test_support::IoPair io;
	auto peer = make_peer(io);

	auto cr = await_bind_connected(io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	REQUIRE(client.close().has_value());

	auto sr = await_send(io, client, "x");
	REQUIRE_FALSE(sr.has_value());
}

TEST_CASE("UdpSocket receive completes with error if destroyed while pending",
		  "[udp]") {
	test_support::IoPair io;
	auto peer = make_peer(io);

	auto cr = await_bind_connected(io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());

	// Use optional so a posted lambda can destroy it deterministically.
	std::optional<ekizu::net::UdpSocket> client{std::move(cr.value())};

	auto res_p = std::make_shared<std::promise<ekizu::Result<std::string>>>();
	auto tid_p = std::make_shared<std::promise<std::thread::id>>();
	auto res_f = res_p->get_future();
	auto tid_f = tid_p->get_future();

	// Order on net thread: initiate receive, then destroy the socket.
	asio::post(io.net, [&] {
		client->receive(asio::bind_executor(
			io.cb.get_executor(),
			[res_p, tid_p](ekizu::Result<std::string> r) mutable {
				tid_p->set_value(std::this_thread::get_id());
				res_p->set_value(std::move(r));
			}));
	});
	asio::post(
		io.net, [&] { client.reset(); });  // destructor while receive pending

	REQUIRE(test_support::wait_ready(res_f, 2s));
	REQUIRE(test_support::wait_ready(tid_f, 2s));

	REQUIRE(tid_f.get() == io.cb_tid);
	REQUIRE_FALSE(
		res_f.get().has_value());  // expect operation_aborted / similar
}

TEST_CASE("UdpSocket destruction with stopped executor documents behavior",
		  "[udp]") {
	// This test documents that handlers posted to stopped executors don't run
	// This is EXPECTED Asio behavior - user must keep executor running

	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);

	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());

	auto client =
		std::make_shared<ekizu::net::UdpSocket>(std::move(cr.value()));

	std::promise<ekizu::Result<std::string>> recv_p;
	auto recv_f = recv_p.get_future();
	client->receive(asio::bind_executor(
		io.cb.get_executor(), [p = std::move(recv_p)](auto r) mutable {
			p.set_value(std::move(r));
		}));

	// Stop net executor, then destroy client
	io.net_work.reset();
	io.net.stop();
	if (io.net_thread.joinable()) { io.net_thread.join(); }

	client.reset();

	// Handler won't complete because net executor stopped
	// This documents user-facing contract: keep executor running
	bool completed = test_support::wait_ready(recv_f, 500ms);
	REQUIRE_FALSE(completed);  // Future never becomes ready

	// Note: Can't call recv_f.get() - would block forever
	// This is the documented failure mode
}
