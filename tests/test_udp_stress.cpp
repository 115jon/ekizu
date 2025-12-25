#include <atomic>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <catch2/catch_test_macros.hpp>
#include <ekizu/udp.hpp>
#include <future>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "test_support/asio_test_harness.hpp"
#include "test_support/udp_fixture.hpp"

namespace asio = boost::asio;
using asio::ip::udp;
using namespace std::chrono_literals;

TEST_CASE("UdpSocket send stress with multi-threaded net context",
		  "[udp][stress]") {
	asio::io_context io_net;
	asio::io_context io_cb;

	auto net_work = asio::make_work_guard(io_net);
	auto cb_work = asio::make_work_guard(io_cb);

	std::vector<std::thread> net_threads;
	net_threads.reserve(4);
	for (int i = 0; i < 4; ++i) {
		net_threads.emplace_back([&] { io_net.run(); });
	}

	std::thread cb_thread([&] { io_cb.run(); });

	udp::socket peer(io_net);
	peer.open(udp::v4());
	peer.bind({asio::ip::make_address("127.0.0.1"), 0});
	const auto port = peer.local_endpoint().port();

	// Create client.
	std::promise<ekizu::Result<ekizu::net::UdpSocket>> bind_p;
	auto bind_f = bind_p.get_future();

	ekizu::net::UdpSocket::bind(
		io_net.get_executor(), "127.0.0.1:0",
		asio::bind_executor(
			io_cb.get_executor(), [p = std::move(bind_p)](auto r) mutable {
				p.set_value(std::move(r));
			}));

	REQUIRE(bind_f.wait_for(2s) == std::future_status::ready);
	auto br = bind_f.get();
	REQUIRE(br.has_value());
	auto client = std::move(br.value());

	std::promise<ekizu::Result<>> connect_p;
	auto connect_f = connect_p.get_future();
	client.connect(
		"127.0.0.1", std::to_string(port),
		asio::bind_executor(
			io_cb.get_executor(), [p = std::move(connect_p)](auto r) mutable {
				p.set_value(std::move(r));
			}));

	REQUIRE(connect_f.wait_for(2s) == std::future_status::ready);
	REQUIRE(connect_f.get().has_value());

	// Optional: also verify peer received N datagrams.
	constexpr int N = 500;
	std::atomic<int> received{0};
	std::promise<void> recv_done_p;
	auto recv_done_f = recv_done_p.get_future();

	auto buf = std::make_shared<std::array<char, 16>>();
	auto from = std::make_shared<udp::endpoint>();

	std::function<void()> recv_loop;
	recv_loop = [&] {
		peer.async_receive_from(asio::buffer(*buf), *from,
								[&](boost::system::error_code, std::size_t) {
									int now = received.fetch_add(1) + 1;
									if (now >= N) {
										recv_done_p.set_value();
										return;
									}
									recv_loop();
								});
	};
	recv_loop();

	std::atomic<int> completed{0};
	std::atomic<int> errors{0};

	auto all_done_p = std::make_shared<std::promise<void>>();
	auto all_done_f = all_done_p->get_future();

	for (int i = 0; i < N; ++i) {
		const std::string msg = "x";
		const auto bytes = boost::span(
			reinterpret_cast<const std::byte *>(msg.data()), msg.size());

		client.send(
			bytes, asio::bind_executor(
					   io_cb.get_executor(),
					   [&, expected = msg.size(),
						all_done_p](ekizu::Result<std::size_t> r) mutable {
						   if (!r.has_value() || r.value() != expected) {
							   errors.fetch_add(1);
						   }
						   int now = completed.fetch_add(1) + 1;
						   if (now == N) { all_done_p->set_value(); }
					   }));
	}

	REQUIRE(all_done_f.wait_for(2s) == std::future_status::ready);
	REQUIRE(errors.load() == 0);

	// If you find this too flaky for CI, delete these 2 lines and keep the test
	// as "completion correctness under concurrency" only.
	REQUIRE(recv_done_f.wait_for(2s) == std::future_status::ready);
	REQUIRE(received.load() >= N);

	net_work.reset();
	cb_work.reset();
	io_net.stop();
	io_cb.stop();

	for (auto &t : net_threads) { t.join(); }
	cb_thread.join();
}

TEST_CASE("UdpSocket serializes and queues many receives", "[udp][stress]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);

	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	constexpr int N = 100;

	// Queue N receives up-front.
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

	// Peer waits for a ping to learn the client's ephemeral port, then sends N
	// messages. Post each send to avoid flooding and dropping packets.
	std::array<char, 64> buf{};
	auto sender = std::make_shared<udp::endpoint>();

	std::promise<void> got_ping_p;
	auto got_ping_f = got_ping_p.get_future();

	peer.sock.async_receive_from(
		asio::buffer(buf), *sender,
		[&peer, sender,
		 &got_ping_p](boost::system::error_code ec, std::size_t) mutable {
			if (ec) return;

			got_ping_p.set_value();

			struct SendState : std::enable_shared_from_this<SendState> {
				udp::socket &sock;
				udp::endpoint ep;
				int i = 0;

				SendState(udp::socket &s, udp::endpoint e)
					: sock(s), ep(std::move(e)) {}

				void step() {
					const std::string msg = "msg_" + std::to_string(i++);
					boost::system::error_code ec2;
					sock.send_to(asio::buffer(msg), ep, 0, ec2);

					if (i < N) {
						asio::post(
							sock.get_executor(),
							[self = shared_from_this()] { self->step(); });
					}
				}
			};

			auto st = std::make_shared<SendState>(peer.sock, *sender);
			asio::post(peer.sock.get_executor(), [st] { st->step(); });
		});

	// Trigger peer to learn our endpoint.
	const std::string ping = "ping";
	auto bytes = boost::span<const std::byte>(
		reinterpret_cast<const std::byte *>(ping.data()), ping.size());

	std::promise<ekizu::Result<std::size_t>> sent_p;
	auto sent_f = sent_p.get_future();
	client.send(
		bytes,
		asio::bind_executor(
			io.cb.get_executor(),
			[p = std::move(sent_p)](ekizu::Result<std::size_t> r) mutable {
				p.set_value(r);
			}));

	REQUIRE(test_support::wait_ready(sent_f, 2s));
	REQUIRE(sent_f.get().has_value());

	REQUIRE(test_support::wait_ready(got_ping_f, 2s));

	// Collect results.
	std::set<std::string> got;
	for (auto &f : fs) {
		REQUIRE(test_support::wait_ready(f, 5s));
		auto r = f.get();
		REQUIRE(r.has_value());
		got.insert(r.value());
	}

	// Validate we got exactly msg_0..msg_(N-1).
	std::set<std::string> expected;
	for (int i = 0; i < N; ++i) { expected.insert("msg_" + std::to_string(i)); }
	REQUIRE(got == expected);
}

TEST_CASE("UdpSocket concurrent receive and close completes safely", "[udp]") {
	test_support::IoPair io;
	auto peer = test_support::udp_fixture::make_peer(io);

	auto cr = test_support::udp_fixture::await_bind_connected(
		io, "127.0.0.1", std::to_string(peer.port));
	REQUIRE(cr.has_value());
	auto client = std::move(cr.value());

	std::atomic<bool> start_close{false};
	std::vector<std::promise<ekizu::Result<std::string>>> promises;
	std::vector<std::future<ekizu::Result<std::string>>> fs;
	std::mutex promises_mutex;
	constexpr int N = 100;

	// Thread: Rapidly queue receives
	std::thread receiver([&] {
		for (int i = 0; i < N; ++i) {
			if (start_close.load(std::memory_order_acquire)) break;

			std::promise<ekizu::Result<std::string>> p;
			{
				std::lock_guard lk(promises_mutex);
				fs.push_back(p.get_future());
				promises.push_back(std::move(p));
			}

			// Capture by index to avoid dangling reference
			size_t idx = promises.size() - 1;
			client.receive(asio::bind_executor(
				io.cb.get_executor(),
				[&promises, &promises_mutex, idx](auto r) mutable {
					std::lock_guard lk(promises_mutex);
					promises[idx].set_value(std::move(r));
				}));
		}
	});

	// Close after brief delay
	std::this_thread::sleep_for(10ms);
	start_close.store(true, std::memory_order_release);
	asio::post(io.net, [&] { (void)client.close(); });

	receiver.join();

	// All queued receives must complete
	for (auto &f : fs) {
		REQUIRE(test_support::wait_ready(f, 5000ms));
		auto r = f.get();
		REQUIRE_FALSE(r.has_value());
	}
}
