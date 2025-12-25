#ifndef EKIZU_HTTP_HPP
#define EKIZU_HTTP_HPP

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4242)
#endif

#include <boost/asio/any_completion_executor.hpp>
#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/beast/http.hpp>
#include <ekizu/export.hpp>
#include <ekizu/result.hpp>
#include <string>
#include <string_view>

#ifdef _WIN32
#pragma warning(pop)
#endif

namespace ekizu {
namespace asio = boost::asio;
}  // namespace ekizu

namespace ekizu::net {

namespace http = boost::beast::http;

using HttpRequest = http::request<http::string_body>;
using HttpResponse = http::response<http::string_body>;
using HttpMethod = http::verb;
using HttpStatus = http::status;

struct HttpConnection {
	HttpConnection(const HttpConnection &) = delete;
	HttpConnection &operator=(const HttpConnection &) = delete;
	EKIZU_EXPORT HttpConnection(HttpConnection &&) noexcept;
	EKIZU_EXPORT HttpConnection &operator=(HttpConnection &&) noexcept;
	EKIZU_EXPORT ~HttpConnection();

	using CompletionExecutor = asio::any_completion_executor;

	[[nodiscard]] EKIZU_EXPORT asio::any_io_executor get_executor() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<HttpConnection>))
				  CompletionToken>
	[[nodiscard]] static auto connect(asio::any_io_executor ex,
									  std::string_view url,
									  CompletionToken &&token) {
		// IMPORTANT: url is a string_view. When using asio::deferred, the
		// initiation may happen later, after the original string is destroyed.
		// Copy it here.
		return asio::async_initiate<CompletionToken,
									void(Result<HttpConnection>)>(
			[ex, url_s = std::string(url)](auto &&handler) mutable {
				CompletionExecutor handler_ex =
					asio::get_associated_executor(handler, ex);

				auto any_handler =
					asio::any_completion_handler<void(Result<HttpConnection>)>{
						std::forward<decltype(handler)>(handler)};

				connect_impl(ex, std::string_view{url_s},
							 std::move(any_handler), std::move(handler_ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<HttpResponse>))
				  CompletionToken>
	auto request(HttpRequest req, CompletionToken &&token) {
		auto ex = get_executor();
		return asio::async_initiate<CompletionToken,
									void(Result<HttpResponse>)>(
			[this, ex, req = std::move(req)](auto &&handler) mutable {
				CompletionExecutor handler_ex =
					asio::get_associated_executor(handler, ex);

				auto any_handler =
					asio::any_completion_handler<void(Result<HttpResponse>)>{
						std::forward<decltype(handler)>(handler)};

				request_impl(std::move(req), std::move(any_handler),
							 std::move(handler_ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<HttpResponse>))
				  CompletionToken>
	[[nodiscard]] static auto get(asio::any_io_executor ex,
								  std::string_view url,
								  CompletionToken &&token) {
		// Same lifetime issue as connect(): url may dangle with asio::deferred.
		return asio::async_initiate<CompletionToken,
									void(Result<HttpResponse>)>(
			[ex, url_s = std::string(url)](auto &&handler) mutable {
				CompletionExecutor handler_ex =
					asio::get_associated_executor(handler, ex);

				auto any_handler =
					asio::any_completion_handler<void(Result<HttpResponse>)>{
						std::forward<decltype(handler)>(handler)};

				get_impl(ex, std::string_view{url_s}, std::move(any_handler),
						 std::move(handler_ex));
			},
			token);
	}

   private:
	struct Impl;

	static EKIZU_EXPORT void connect_impl(
		asio::any_io_executor ex, std::string_view url,
		asio::any_completion_handler<void(Result<HttpConnection>)> handler,
		CompletionExecutor handler_ex);

	EKIZU_EXPORT void request_impl(
		HttpRequest req,
		asio::any_completion_handler<void(Result<HttpResponse>)> handler,
		CompletionExecutor handler_ex);

	static EKIZU_EXPORT void get_impl(
		asio::any_io_executor ex, std::string_view url,
		asio::any_completion_handler<void(Result<HttpResponse>)> handler,
		CompletionExecutor handler_ex);

	explicit HttpConnection(std::shared_ptr<Impl> impl);

	std::shared_ptr<Impl> m_impl;
};
}  // namespace ekizu::net

#endif	// EKIZU_HTTP_HPP
