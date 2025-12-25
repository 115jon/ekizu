#include <array>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/udp.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <thread>
#include <type_traits>

#include "test_support/udp_fixture.hpp"

namespace asio = boost::asio;
using asio::ip::udp;
using namespace std::chrono_literals;

using test_support::udp_fixture::await_bind;
using test_support::udp_fixture::await_bind_connected;
using test_support::udp_fixture::await_connect;

using test_support::udp_fixture::await_receive;
using test_support::udp_fixture::await_send;
using test_support::udp_fixture::make_peer;

TEST_CASE("UdpSocket::bind completes on associated executor", "[udp]") {
	test_support::IoPair io;
	auto peer = make_peer(io);

	std::promise<std::thread::id> ran_p;
	std::promise<ekizu::Result<ekizu::net::UdpSocket>> res_p;
	auto ran_f = ran_p.get_future();
	auto res_f = res_p.get_future();

	ekizu::net::UdpSocket::bind(
		io.net.get_executor(), "127.0.0.1:0",
		asio::bind_executor(
			io.cb.get_executor(),
			[rp = std::move(ran_p), rvp = std::move(res_p)](auto r) mutable {
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

TEST_CASE("UdpSocket::bind failure completes on associated executor", "[udp]") {
	test_support::IoPair io;
	auto peer = make_peer(io);

	// Reserve a local port so the UdpSocket::bind below will fail.
	std::promise<uint16_t> port_p;
	auto port_f = port_p.get_future();

	auto blocker = std::make_shared<boost::asio::ip::udp::socket>(io.net);

	boost::asio::post(
		io.net.get_executor(), [blocker, pp = std::move(port_p)]() mutable {
			boost::system::error_code ec;

			blocker->open(boost::asio::ip::udp::v4(), ec);
			if (ec) {
				pp.set_value(0);
				return;
			}

			auto addr = boost::asio::ip::make_address("127.0.0.1", ec);
			if (ec) {
				pp.set_value(0);
				return;
			}

			blocker->bind(boost::asio::ip::udp::endpoint(addr, 0), ec);
			if (ec) {
				pp.set_value(0);
				return;
			}

			pp.set_value(blocker->local_endpoint().port());
		});

	REQUIRE(test_support::wait_ready(port_f, 2s));
	const auto port = port_f.get();
	REQUIRE(port != 0);

	std::promise<std::thread::id> ran_p;
	std::promise<ekizu::Result<ekizu::net::UdpSocket>> res_p;
	auto ran_f = ran_p.get_future();
	auto res_f = res_p.get_future();

	ekizu::net::UdpSocket::bind(
		io.net.get_executor(), "127.0.0.1:" + std::to_string(port),
		boost::asio::bind_executor(
			io.cb.get_executor(),
			[rp = std::move(ran_p), rvp = std::move(res_p)](auto r) mutable {
				rp.set_value(std::this_thread::get_id());
				rvp.set_value(std::move(r));
			}));

	REQUIRE(test_support::wait_ready(ran_f, 2s));
	REQUIRE(test_support::wait_ready(res_f, 2s));

	const auto ran_tid = ran_f.get();
	const auto res = res_f.get();

	REQUIRE_FALSE(res.has_value());
	REQUIRE(ran_tid == io.cb_tid);
	REQUIRE(ran_tid != io.net_tid);

	// Keep blocker alive until after bind completes (it is captured above).
}

TEST_CASE("UdpSocket send delivers datagram to peer", "[udp]") {
	test_support::IoPair io;
	auto peer = make_peer(io);

	auto cr = await_bind_connected(io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	std::array<char, 64> buf{};
	udp::endpoint from{};
	std::promise<std::string> recv_p;
	auto recv_f = recv_p.get_future();

	peer.sock.async_receive_from(
		asio::buffer(buf), from,
		[&](boost::system::error_code ec, std::size_t n) {
			if (ec) {
				recv_p.set_value("<ec>");
			} else {
				recv_p.set_value(std::string(buf.data(), n));
			}
		});

	auto sr = await_send(io, client, "ping");
	REQUIRE(sr.has_value());
	REQUIRE(sr.value() == 4);

	REQUIRE(test_support::wait_ready(recv_f, 2s));
	REQUIRE(recv_f.get() == "ping");
}

TEST_CASE("UdpSocket::receive ping/pong roundtrip", "[udp]") {
	test_support::IoPair io;
	auto peer = make_peer(io);

	auto cr = await_bind_connected(io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	// Start receive first.
	std::promise<ekizu::Result<std::string>> recv_p;
	std::promise<std::thread::id> recv_tid_p;
	auto recv_f = recv_p.get_future();
	auto recv_tid_f = recv_tid_p.get_future();

	client.receive(asio::bind_executor(
		io.cb.get_executor(),
		[p = std::move(recv_p), tp = std::move(recv_tid_p)](auto r) mutable {
			tp.set_value(std::this_thread::get_id());
			p.set_value(std::move(r));
		}));

	// Peer: receive ping, reply pong.
	std::array<char, 64> buf{};
	udp::endpoint sender{};
	std::promise<std::string> ping_p;
	auto ping_f = ping_p.get_future();

	peer.sock.async_receive_from(
		asio::buffer(buf), sender,
		[&](boost::system::error_code ec, std::size_t n) {
			if (ec) {
				ping_p.set_value("<ec>");
				return;
			}
			ping_p.set_value(std::string(buf.data(), n));
			const std::string pong = "pong";
			boost::system::error_code ec2;
			peer.sock.send_to(asio::buffer(pong), sender, 0, ec2);
		});

	auto sr = await_send(io, client, "ping");
	REQUIRE(sr.has_value());
	REQUIRE(sr.value() == 4);

	REQUIRE(test_support::wait_ready(ping_f, 2s));
	REQUIRE(ping_f.get() == "ping");

	REQUIRE(test_support::wait_ready(recv_f, 2s));
	REQUIRE(test_support::wait_ready(recv_tid_f, 2s));

	auto rr = recv_f.get();
	REQUIRE(rr.has_value());
	REQUIRE(rr.value() == "pong");
	REQUIRE(recv_tid_f.get() == io.cb_tid);
}

TEST_CASE("UdpSocket receive completes with error if closed while pending",
		  "[udp]") {
	test_support::IoPair io;
	auto peer = make_peer(io);

	auto cr = await_bind_connected(io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	auto pp = std::make_shared<std::promise<ekizu::Result<std::string>>>();
	auto f = pp->get_future();

	// Order on net thread: start receive then close.
	asio::post(io.net, [&] {
		client.receive(asio::bind_executor(
			io.cb.get_executor(),
			[pp](auto r) mutable { pp->set_value(std::move(r)); }));
		(void)client.close();
	});

	REQUIRE(test_support::wait_ready(f, 2s));
	REQUIRE_FALSE(f.get().has_value());
}

TEST_CASE("UdpSocket close cancels queued receives", "[udp]") {
	test_support::IoPair io;
	auto peer = make_peer(io);
	auto cr = await_bind_connected(io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	constexpr int N = 10;

	std::vector<std::promise<ekizu::Result<std::string>>> ps(N);
	std::vector<std::future<ekizu::Result<std::string>>> fs;
	fs.reserve(N);

	// Start N receives (queue them).
	for (size_t i = 0; i < N; ++i) {
		fs.push_back(ps[i].get_future());
		client.receive(asio::bind_executor(
			io.cb.get_executor(),
			[p = std::move(ps[i])](ekizu::Result<std::string> r) mutable {
				p.set_value(std::move(r));
			}));
	}

	// Close on net thread to serialize with internal async ops.
	asio::post(io.net, [&] { (void)client.close(); });

	// All N must complete with error (not hang).
	for (auto &f : fs) {
		REQUIRE(test_support::wait_ready(f, 2s));
		auto r = f.get();
		REQUIRE_FALSE(r.has_value());
	}
}

TEST_CASE("UdpSocket destroy from different thread completes all handlers",
		  "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);
	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());

	constexpr int N = 10;
	std::vector<std::promise<ekizu::Result<std::string>>> ps(N);
	std::vector<std::future<ekizu::Result<std::string>>> fs;
	fs.reserve(N);

	auto c = std::make_shared<ekizu::net::UdpSocket>(std::move(cr.value()));

	for (size_t i = 0; i < N; ++i) {
		fs.push_back(ps[i].get_future());
		c->receive(asio::bind_executor(
			io.cb.get_executor(),
			[p = std::move(ps[i])](ekizu::Result<std::string> r) mutable {
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

TEST_CASE("UdpSocket queued receives report operation_canceled error code",
		  "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);
	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	constexpr int N = 5;
	std::vector<std::promise<ekizu::Result<std::string>>> ps(N);
	std::vector<std::future<ekizu::Result<std::string>>> fs;
	fs.reserve(N);

	for (size_t i = 0; i < N; ++i) {
		fs.push_back(ps[i].get_future());
		client.receive(asio::bind_executor(
			io.cb.get_executor(),
			[p = std::move(ps[i])](ekizu::Result<std::string> r) mutable {
				p.set_value(std::move(r));
			}));
	}

	asio::post(io.net, [&] { (void)client.close(); });

	for (auto &f : fs) {
		REQUIRE(test_support::wait_ready(f, 2s));
		auto r = f.get();
		REQUIRE_FALSE(r.has_value());

		// Verify exact error code
		auto ec = r.error();
		REQUIRE(ec == boost::system::errc::operation_canceled);
	}
}

TEST_CASE("UdpSocket distinguishes in-flight and queued cancellation",
		  "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);
	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	std::promise<ekizu::Result<std::string>> inflight_p;
	std::promise<ekizu::Result<std::string>> queued_p;
	auto inflight_f = inflight_p.get_future();
	auto queued_f = queued_p.get_future();

	// First receive becomes in-flight
	client.receive(asio::bind_executor(
		io.cb.get_executor(), [p = std::move(inflight_p)](auto r) mutable {
			p.set_value(std::move(r));
		}));

	// Second receive stays queued
	client.receive(asio::bind_executor(
		io.cb.get_executor(), [p = std::move(queued_p)](auto r) mutable {
			p.set_value(std::move(r));
		}));

	// Give first receive time to actually call async_receive_from
	std::this_thread::sleep_for(50ms);

	// Close cancels both
	asio::post(io.net, [&] { (void)client.close(); });

	REQUIRE(test_support::wait_ready(inflight_f, 2s));
	REQUIRE(test_support::wait_ready(queued_f, 2s));

	auto r1 = inflight_f.get();
	auto r2 = queued_f.get();

	REQUIRE_FALSE(r1.has_value());
	REQUIRE_FALSE(r2.has_value());
	REQUIRE(r1.error() == boost::system::errc::operation_canceled);
	REQUIRE(r2.error() == boost::system::errc::operation_canceled);
}

TEST_CASE(
	"UdpSocket destruction guarantees promise fulfillment on running executor",
	"[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);
	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());

	std::atomic<int> handler_count{0};
	constexpr int N = 20;

	std::vector<std::future<ekizu::Result<std::string>>> fs;
	fs.reserve(N);

	{
		auto client =
			std::make_shared<ekizu::net::UdpSocket>(std::move(cr.value()));

		for (int i = 0; i < N; ++i) {
			auto p =
				std::make_shared<std::promise<ekizu::Result<std::string>>>();
			fs.push_back(p->get_future());

			client->receive(asio::bind_executor(
				io.cb.get_executor(),
				[p, &handler_count](ekizu::Result<std::string> r) mutable {
					handler_count.fetch_add(1, std::memory_order_relaxed);
					p->set_value(std::move(r));
				}));
		}

		// Destroy client while executors are RUNNING
		asio::post(io.net, [client]() mutable { client.reset(); });
	}

	// All futures must become ready
	for (auto &f : fs) {
		REQUIRE(test_support::wait_ready(f, 5000ms));
		(void)f.get();	// Must not throw std::future_error
	}

	REQUIRE(handler_count.load() == N);
}

TEST_CASE(
	"Moved-from UdpSocket returns operation_not_permitted for send/receive",
	"[udp]") {
	test_support::IoPair io;
	auto peer = make_peer(io);

	auto cr = await_bind_connected(io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	auto live = std::move(client);	// client becomes moved-from

	auto sr = await_send(io, client, "x");
	REQUIRE_FALSE(sr.has_value());

	auto rr = await_receive(io, client);
	REQUIRE_FALSE(rr.has_value());

	REQUIRE(client.close().has_value());  // close on null impl => success
	(void)live;
}

TEST_CASE("UdpSocket move assignment is exercised", "[udp]") {
	static_assert(std::is_nothrow_move_assignable_v<ekizu::net::UdpSocket>);

	test_support::IoPair io;
	auto peer1 = make_peer(io);
	auto peer2 = make_peer(io);

	auto c1r =
		await_bind_connected(io, "127.0.0.1", std::to_string(peer1.port));
	auto c2r =
		await_bind_connected(io, "127.0.0.1", std::to_string(peer2.port));
	REQUIRE(c1r.has_value());
	REQUIRE(c2r.has_value());

	auto c1 = std::move(c1r.value());
	auto c2 = std::move(c2r.value());

	c2 = std::move(c1);
}

TEST_CASE("UdpSocket close() is idempotent", "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);
	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	// First close
	auto r1 = client.close();
	REQUIRE(r1.has_value());

	// Second close must succeed (idempotent)
	auto r2 = client.close();
	REQUIRE(r2.has_value());

	// Third close (verify no state corruption)
	auto r3 = client.close();
	REQUIRE(r3.has_value());
}

TEST_CASE("UdpSocket destructor invokes close() safely", "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);
	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());

	std::promise<ekizu::Result<std::string>> recv_p;
	auto recv_f = recv_p.get_future();

	{
		auto client = std::move(cr.value());
		client.receive(asio::bind_executor(
			io.cb.get_executor(), [p = std::move(recv_p)](auto r) mutable {
				p.set_value(std::move(r));
			}));

		// Manual close before destructor
		REQUIRE(client.close().has_value());

	}  // ~UdpSocket() calls close() again - must not fail

	REQUIRE(test_support::wait_ready(recv_f, 2s));
	REQUIRE_FALSE(recv_f.get().has_value());
}

TEST_CASE("UdpSocket move during pending receive completes safely", "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);
	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());

	auto client1 = std::move(cr.value());

	std::promise<ekizu::Result<std::string>> recv_p;
	auto recv_f = recv_p.get_future();

	client1.receive(asio::bind_executor(
		io.cb.get_executor(), [p = std::move(recv_p)](auto r) mutable {
			p.set_value(std::move(r));
		}));

	// Move while receive is pending
	auto client2 = std::move(client1);

	// Close via moved-to client
	REQUIRE(client2.close().has_value());

	// Handler must still complete
	REQUIRE(test_support::wait_ready(recv_f, 2s));
	REQUIRE_FALSE(recv_f.get().has_value());
}

TEST_CASE("UdpSocket destruction completes all handlers", "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);
	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());

	std::atomic<int> completed{0};
	constexpr int N = 10;

	{
		auto client =
			std::make_shared<ekizu::net::UdpSocket>(std::move(cr.value()));

		for (int i = 0; i < N; ++i) {
			client->receive(asio::bind_executor(
				io.cb.get_executor(), [&completed](auto /*r*/) {
					completed.fetch_add(1, std::memory_order_relaxed);
				}));
		}

		// Destroy on net thread
		std::promise<void> destroyed;
		auto destroyed_f = destroyed.get_future();
		asio::post(io.net, [client, &destroyed]() mutable {
			client.reset();
			destroyed.set_value();
		});

		REQUIRE(test_support::wait_ready(destroyed_f, 2s));
	}

	// Spin until all handlers execute (or timeout)
	auto deadline = std::chrono::steady_clock::now() + 5s;
	while (completed.load() < N &&
		   std::chrono::steady_clock::now() < deadline) {
		std::this_thread::sleep_for(10ms);
	}

	REQUIRE(completed.load() == N);

	// Now IoPair destructor is safe
}

TEST_CASE("UdpSocket close() from callback executor is safe", "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);
	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	std::promise<ekizu::Result<>> close_res_p;
	auto close_res_f = close_res_p.get_future();

	asio::post(io.cb, [&client, p = std::move(close_res_p)]() mutable {
		p.set_value(client.close());
	});

	REQUIRE(test_support::wait_ready(close_res_f, 2s));
	REQUIRE(close_res_f.get().has_value());
}