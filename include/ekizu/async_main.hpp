#ifndef EKIZU_ASYNC_MAIN_HPP
#define EKIZU_ASYNC_MAIN_HPP

#include <boost/asio/detached.hpp>
#include <boost/asio/spawn.hpp>
#include <ekizu/result.hpp>

#define async_main(yield)                                         \
	ekizu::Result<> async_main(yield);                            \
	int main() {                                                  \
		int ret{};                                                \
		boost::asio::io_context ctx;                              \
		spawn(                                                    \
			ctx, [&ret](auto y) { ret = async_main(y) ? 0 : 1; }, \
			boost::asio::detached);                               \
		ctx.run();                                                \
		return ret;                                               \
	}                                                             \
	ekizu::Result<> async_main(yield)

#define async_main_with_args(a, b, yield)                          \
	ekizu::Result<> async_main_with_args(a, b, yield);             \
	int main(int argc, char **argv) {                              \
		int ret{};                                                 \
		boost::asio::io_context ctx;                               \
		boost::asio::spawn(                                        \
			ctx,                                                   \
			[&ret, argc, argv](auto y) {                           \
				ret = async_main_with_args(argc, argv, y) ? 0 : 1; \
			},                                                     \
			boost::asio::detached);                                \
		ctx.run();                                                 \
		return ret;                                                \
	}                                                              \
	ekizu::Result<> async_main_with_args(a, b, yield)

#endif	// EKIZU_ASYNC_MAIN_HPP
