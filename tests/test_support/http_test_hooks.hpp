#ifndef TEST_SUPPORT_HTTP_TEST_HOOKS_HPP
#define TEST_SUPPORT_HTTP_TEST_HOOKS_HPP

#include <boost/asio/ssl/context.hpp>
#include <boost/system/error_code.hpp>

namespace ekizu::net {

#ifdef EKIZU_TESTING

using HttpSetVerifyModeFn =
	boost::system::error_code (*)(boost::asio::ssl::context &);
using HttpSetDefaultVerifyPathsFn =
	boost::system::error_code (*)(boost::asio::ssl::context &);
using HttpSetSniFn = bool (*)(void *native_ssl_handle, const char *host);

void ekizu_set_http_verify_mode_for_tests(HttpSetVerifyModeFn fn);
void ekizu_set_http_default_verify_paths_for_tests(
	HttpSetDefaultVerifyPathsFn fn);
void ekizu_set_http_sni_for_tests(HttpSetSniFn fn);

struct ScopedHttpVerifyModeHook {
	explicit ScopedHttpVerifyModeHook(HttpSetVerifyModeFn fn) {
		ekizu_set_http_verify_mode_for_tests(fn);
	}
	~ScopedHttpVerifyModeHook() {
		ekizu_set_http_verify_mode_for_tests(nullptr);
	}
};

struct ScopedHttpDefaultVerifyPathsHook {
	explicit ScopedHttpDefaultVerifyPathsHook(HttpSetDefaultVerifyPathsFn fn) {
		ekizu_set_http_default_verify_paths_for_tests(fn);
	}
	~ScopedHttpDefaultVerifyPathsHook() {
		ekizu_set_http_default_verify_paths_for_tests(nullptr);
	}
};

struct ScopedHttpSniHook {
	explicit ScopedHttpSniHook(HttpSetSniFn fn) {
		ekizu_set_http_sni_for_tests(fn);
	}
	~ScopedHttpSniHook() { ekizu_set_http_sni_for_tests(nullptr); }
};

#endif

}  // namespace ekizu::net

#endif	// TEST_SUPPORT_HTTP_TEST_HOOKS_HPP
