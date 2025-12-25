#ifndef TEST_SUPPORT_ASIO_TEST_HARNESS_HPP
#define TEST_SUPPORT_ASIO_TEST_HARNESS_HPP

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <chrono>
#include <future>
#include <thread>

namespace test_support {

template <typename T, typename Rep, typename Period>
bool wait_ready(std::future<T> &f, std::chrono::duration<Rep, Period> timeout) {
	return f.wait_for(timeout) == std::future_status::ready;
}

template <typename T>
struct CvWaiter {
	std::mutex m;
	std::condition_variable cv;
	bool done = false;
	std::optional<T> value;

	void set(T v) {
		{
			std::lock_guard<std::mutex> lk(m);
			value.emplace(std::move(v));
			done = true;
		}
		cv.notify_one();
	}

	bool wait_for(std::chrono::milliseconds timeout) {
		std::unique_lock<std::mutex> lk(m);
		return cv.wait_for(lk, timeout, [&] { return done; });
	}
};

struct IoPair {
	boost::asio::io_context net;
	boost::asio::io_context cb;

	boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
		net_work{boost::asio::make_work_guard(net)};
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
		cb_work{boost::asio::make_work_guard(cb)};

	std::thread net_thread;
	std::thread cb_thread;

	std::thread::id net_tid;
	std::thread::id cb_tid;

	IoPair() {
		std::promise<std::thread::id> net_started;
		std::promise<std::thread::id> cb_started;

		auto net_f = net_started.get_future();
		auto cb_f = cb_started.get_future();

		net_thread = std::thread([this, p = std::move(net_started)]() mutable {
			p.set_value(std::this_thread::get_id());
			net.run();
		});

		cb_thread = std::thread([this, p = std::move(cb_started)]() mutable {
			p.set_value(std::this_thread::get_id());
			cb.run();
		});

		net_tid = net_f.get();
		cb_tid = cb_f.get();
	}

	IoPair(IoPair &&) = delete;
	IoPair &operator=(IoPair &&) = delete;
	IoPair(IoPair const &) = delete;
	IoPair &operator=(IoPair const &) = delete;

	void stop() {
		net_work.reset();
		cb_work.reset();
		net.stop();
		cb.stop();
	}

	~IoPair() {
		stop();
		if (net_thread.joinable()) { net_thread.join(); }
		if (cb_thread.joinable()) { cb_thread.join(); }
	}
};

}  // namespace test_support

#endif	// TEST_SUPPORT_ASIO_TEST_HARNESS_HPP
