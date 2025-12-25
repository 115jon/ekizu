#ifndef TEST_SUPPORT_WS_TEST_HOOKS_HPP
#define TEST_SUPPORT_WS_TEST_HOOKS_HPP

#include <boost/asio/ssl/context.hpp>
#include <boost/system/error_code.hpp>

namespace ekizu::net {

#ifdef EKIZU_TESTING

using WsSetVerifyModeFn =
	boost::system::error_code (*)(boost::asio::ssl::context &);
using WsSetDefaultVerifyPathsFn =
	boost::system::error_code (*)(boost::asio::ssl::context &);
using WsSetSniFn = bool (*)(void *native_ssl_handle, const char *host);
using WsForceDrainErrorFn = boost::system::error_code (*)();

void ekizu_set_ws_verify_mode_for_tests(WsSetVerifyModeFn fn);
void ekizu_set_ws_default_verify_paths_for_tests(WsSetDefaultVerifyPathsFn fn);
void ekizu_set_ws_sni_for_tests(WsSetSniFn fn);
void ekizu_set_ws_force_drain_error_for_tests(WsForceDrainErrorFn fn);

struct ScopedWsVerifyModeHook {
	explicit ScopedWsVerifyModeHook(WsSetVerifyModeFn fn) {
		ekizu_set_ws_verify_mode_for_tests(fn);
	}
	~ScopedWsVerifyModeHook() { ekizu_set_ws_verify_mode_for_tests(nullptr); }
};

struct ScopedWsDefaultVerifyPathsHook {
	explicit ScopedWsDefaultVerifyPathsHook(WsSetDefaultVerifyPathsFn fn) {
		ekizu_set_ws_default_verify_paths_for_tests(fn);
	}
	~ScopedWsDefaultVerifyPathsHook() {
		ekizu_set_ws_default_verify_paths_for_tests(nullptr);
	}
};

struct ScopedWsSniHook {
	explicit ScopedWsSniHook(WsSetSniFn fn) { ekizu_set_ws_sni_for_tests(fn); }
	~ScopedWsSniHook() { ekizu_set_ws_sni_for_tests(nullptr); }
};

struct ScopedWsForceDrainErrorHook {
	explicit ScopedWsForceDrainErrorHook(WsForceDrainErrorFn fn) {
		ekizu_set_ws_force_drain_error_for_tests(fn);
	}
	~ScopedWsForceDrainErrorHook() {
		ekizu_set_ws_force_drain_error_for_tests(nullptr);
	}
};

#endif

}  // namespace ekizu::net

#endif	// TEST_SUPPORT_WS_TEST_HOOKS_HPP
