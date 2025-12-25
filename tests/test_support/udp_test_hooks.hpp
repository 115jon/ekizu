#ifndef TEST_SUPPORT_UDP_TEST_HOOKS_HPP
#define TEST_SUPPORT_UDP_TEST_HOOKS_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>

namespace ekizu::net {

#ifdef EKIZU_TESTING
using UdpOpenFn = boost::system::error_code (*)(boost::asio::any_io_executor,
												boost::asio::ip::udp::socket &);
using UdpCloseFn =
	boost::system::error_code (*)(boost::asio::ip::udp::socket &);

void ekizu_set_udp_open_for_tests(UdpOpenFn fn);
void ekizu_set_udp_close_for_tests(UdpCloseFn fn);

struct ScopedUdpOpenHook {
	explicit ScopedUdpOpenHook(UdpOpenFn fn) {
		ekizu_set_udp_open_for_tests(fn);
	}
	~ScopedUdpOpenHook() { ekizu_set_udp_open_for_tests(nullptr); }
};

struct ScopedUdpCloseHook {
	explicit ScopedUdpCloseHook(UdpCloseFn fn) {
		ekizu_set_udp_close_for_tests(fn);
	}
	~ScopedUdpCloseHook() { ekizu_set_udp_close_for_tests(nullptr); }
};

#endif

}  // namespace ekizu::net

#endif	// TEST_SUPPORT_UDP_TEST_HOOKS_HPP
