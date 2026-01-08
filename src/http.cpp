#include <atomic>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/certify/https_verification.hpp>
#include <boost/url/parse.hpp>
#include <deque>
#include <ekizu/error.hpp>
#include <ekizu/error_context.hpp>
#include <ekizu/http.hpp>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#ifdef EKIZU_TESTING
namespace ekizu::net {

using HttpSetVerifyModeFn =
	boost::system::error_code (*)(boost::asio::ssl::context &);
using HttpSetDefaultVerifyPathsFn =
	boost::system::error_code (*)(boost::asio::ssl::context &);
using HttpSetSniFn = bool (*)(void *native_ssl_handle, const char *host);

static HttpSetVerifyModeFn g_http_set_verify_mode_for_tests = nullptr;
static HttpSetDefaultVerifyPathsFn g_http_set_default_verify_paths_for_tests =
	nullptr;
static HttpSetSniFn g_http_set_sni_for_tests = nullptr;

void ekizu_set_http_verify_mode_for_tests(HttpSetVerifyModeFn fn) {
	g_http_set_verify_mode_for_tests = fn;
}
void ekizu_set_http_default_verify_paths_for_tests(
	HttpSetDefaultVerifyPathsFn fn) {
	g_http_set_default_verify_paths_for_tests = fn;
}
void ekizu_set_http_sni_for_tests(HttpSetSniFn fn) {
	g_http_set_sni_for_tests = fn;
}

}  // namespace ekizu::net
#endif

namespace {
using boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace ssl = boost::asio::ssl;

std::string make_target(const boost::urls::url_view &uri) {
	std::string target =
		uri.encoded_path().empty() ? "/" : std::string(uri.encoded_path());
	if (!uri.encoded_query().empty()) {
		target.push_back('?');
		target += std::string(uri.encoded_query());
	}
	return target;
}

}  // namespace

namespace ekizu::net {

struct HttpConnection::Impl
	: std::enable_shared_from_this<HttpConnection::Impl> {
	friend HttpConnection;
	using PlainStream = beast::tcp_stream;
	using SslStream = beast::ssl_stream<beast::tcp_stream>;
	using Stream = std::variant<PlainStream, SslStream>;

	struct Pending {
		HttpRequest req;
		asio::any_completion_handler<void(Result<HttpResponse>)> handler;
		HttpConnection::CompletionExecutor handler_ex;
	};

	explicit Impl(asio::any_io_executor ex, bool https,
				  const std::shared_ptr<ssl::context> &ssl_ctx)
		: m_ex{ex},
		  m_strand{ex},
		  m_https{https},
		  m_ssl_ctx{ssl_ctx},
		  m_resolver{ex},
		  m_stream{https ? Stream{std::in_place_type<SslStream>, ex, *ssl_ctx}
						 : Stream{std::in_place_type<PlainStream>, ex}} {}

	asio::any_io_executor get_executor() const { return m_ex; }

	void enqueue(Pending p) {
		asio::dispatch(
			m_strand, [self = shared_from_this(), p = std::move(p)]() mutable {
				if (self->m_cancelled.load(std::memory_order_relaxed)) {
					asio::dispatch(asio::bind_executor(
						p.handler_ex, [h = std::move(p.handler)]() mutable {
							std::move(h)(make_error_code(
								boost::system::errc::operation_canceled));
						}));
					return;
				}

				self->m_queue.push_back(std::move(p));
				self->start_next();
			});
	}

	void cancel_on_strand() {
		if (m_cancelled_on_strand) { return; }
		m_cancelled_on_strand = true;

		// Complete queued requests immediately.
		while (!m_queue.empty()) {
			auto p = std::move(m_queue.front());
			m_queue.pop_front();

			asio::dispatch(asio::bind_executor(
				p.handler_ex, [h = std::move(p.handler)]() mutable {
					std::move(h)(make_error_code(
						boost::system::errc::operation_canceled));
				}));
		}

		// Cancel any in-flight ops by cancelling/closing the socket.
		boost::system::error_code ignored_ec;
		m_resolver.cancel();

		std::visit(
			[&](auto &stream) {
				auto &lowest = beast::get_lowest_layer(stream);
				lowest.socket().cancel(ignored_ec);
				lowest.socket().close(ignored_ec);
			},
			m_stream);
	}

	template <typename Done>
	void async_run(std::string host, std::string port, Done done) {
		auto self = shared_from_this();
		m_host = std::move(host);
		m_port = std::move(port);

		m_resolver.async_resolve(
			m_host, m_port,
			asio::bind_executor(m_ex, [self, done = std::move(done)](
										  const boost::system::error_code &ec,
										  tcp::resolver::results_type
											  results) mutable {
				if (ec) {
					done(ec);
					return;
				}

				std::visit(
					[&](auto &stream) {
						beast::get_lowest_layer(stream).expires_after(
							std::chrono::seconds(30));
						beast::get_lowest_layer(stream).async_connect(
							results,
							asio::bind_executor(
								self->m_ex,
								[self, done = std::move(done)](
									const boost::system::error_code &ec2,
									const tcp::endpoint &) mutable {
									if (ec2) {
										done(ec2);
										return;
									}

									if (!self->m_https) {
										done(outcome::success());
										return;
									}

									auto &ssl_stream =
										std::get<SslStream>(self->m_stream);

#ifdef EKIZU_TESTING
									if (g_http_set_sni_for_tests != nullptr) {
										if (!g_http_set_sni_for_tests(
												ssl_stream.native_handle(),
												self->m_host.c_str())) {
											beast::error_code ssl_ec{
												static_cast<int>(
													::ERR_get_error()),
												asio::error::
													get_ssl_category()};
											done(ssl_ec);
											return;
										}
									} else
#endif
									{
										if (!SSL_set_tlsext_host_name(
												ssl_stream.native_handle(),
												self->m_host.c_str())) {
											beast::error_code ssl_ec{
												static_cast<int>(
													::ERR_get_error()),
												asio::error::
													get_ssl_category()};
											done(ssl_ec);
											return;
										}
									}

									ssl_stream.async_handshake(
										ssl::stream_base::client,
										asio::bind_executor(
											self->m_ex,
											[done = std::move(done)](
												const boost::system::error_code
													&ec3) mutable {
												if (ec3) {
													done(ec3);
													return;
												}
												done(outcome::success());
											}));
								}));
					},
					self->m_stream);
			}));
	}

	void cancel() {
		// IMPORTANT: make cancellation observable immediately, even if the
		// strand cancellation handler runs later.
		const bool was_cancelled =
			m_cancelled.exchange(true, std::memory_order_relaxed);
		if (was_cancelled) { return; }

		asio::dispatch(m_strand, [self = shared_from_this()]() mutable {
			self->cancel_on_strand();
		});
	}

   private:
	struct RequestOp : std::enable_shared_from_this<RequestOp> {
		std::shared_ptr<Impl> impl;
		Pending p;
		HttpResponse res;

		RequestOp(std::shared_ptr<Impl> i, Pending pending)
			: impl(std::move(i)), p(std::move(pending)) {}

		void start() {
			auto self = shared_from_this();

			impl->m_buffer.consume(impl->m_buffer.size());

			std::visit(
				[&](auto &stream) {
					beast::get_lowest_layer(stream).expires_after(
						std::chrono::seconds(30));
					http::async_write(
						stream, p.req,
						asio::bind_executor(
							impl->m_strand,
							[self](const boost::system::error_code &ec,
								   std::size_t) mutable {
								if (ec) {
									self->finish(ec);
									return;
								}
								self->read();
							}));
				},
				impl->m_stream);
		}

		void read() {
			auto self = shared_from_this();

			std::visit(
				[&](auto &stream) {
					beast::get_lowest_layer(stream).expires_after(
						std::chrono::seconds(30));
					http::async_read(
						stream, impl->m_buffer, res,
						asio::bind_executor(
							impl->m_strand,
							[self](const boost::system::error_code &ec,
								   std::size_t) mutable {
								if (ec) {
									self->finish(ec);
									return;
								}
								self->finish(std::move(self->res));
							}));
				},
				impl->m_stream);
		}

		void finish(Result<HttpResponse> result) {
			if (impl->m_cancelled.load(std::memory_order_relaxed)) {
				result =
					make_error_code(boost::system::errc::operation_canceled);
			}

			// Complete on the executor associated with the completion token.
			asio::dispatch(asio::bind_executor(
				p.handler_ex, [h = std::move(p.handler),
							   result = std::move(result)]() mutable {
					std::move(h)(std::move(result));
				}));

			asio::dispatch(impl->m_strand, [impl = impl]() {
				impl->m_busy = false;
				impl->start_next();
			});
		}
	};

	void start_next() {
		if (m_cancelled.load(std::memory_order_relaxed)) {
			cancel_on_strand();
			return;
		}

		if (m_busy || m_queue.empty()) { return; }
		m_busy = true;

		auto p = std::move(m_queue.front());
		m_queue.pop_front();

		std::make_shared<RequestOp>(shared_from_this(), std::move(p))->start();
	}

	asio::any_io_executor m_ex;
	asio::strand<asio::any_io_executor> m_strand;

	bool m_https{};
	std::shared_ptr<ssl::context> m_ssl_ctx;

	tcp::resolver m_resolver;
	Stream m_stream;
	beast::flat_buffer m_buffer;

	std::string m_host;
	std::string m_port;

	bool m_busy{false};
	std::atomic_bool m_cancelled{false};
	bool m_cancelled_on_strand{false};
	std::deque<Pending> m_queue;
};

HttpConnection::HttpConnection(HttpConnection &&) noexcept = default;
HttpConnection &HttpConnection::operator=(HttpConnection &&) noexcept = default;

HttpConnection::~HttpConnection() {
	if (m_impl) { m_impl->cancel(); }
}

asio::any_io_executor HttpConnection::get_executor() const {
	return m_impl ? m_impl->get_executor() : asio::any_io_executor{};
}

void HttpConnection::connect_impl(
	asio::any_io_executor ex, std::string_view url,
	asio::any_completion_handler<void(Result<HttpConnection>)> handler,
	CompletionExecutor handler_ex) {
	auto parsed = boost::urls::parse_uri(url);
	if (!parsed) {
		asio::dispatch(asio::bind_executor(
			handler_ex, [h = std::move(handler), e = parsed.error()]() mutable {
				ekizu::clear_error_context();
				ekizu::set_error_context("HTTP URL parse failed");
				std::move(h)(e);
			}));
		return;
	}

	const auto uri = parsed.value();

	if (uri.scheme() != "http" && uri.scheme() != "https") {
		asio::dispatch(asio::bind_executor(
			handler_ex, [h = std::move(handler), url_s = std::string(url),
						 scheme_s = std::string(uri.scheme())]() mutable {
				ekizu::clear_error_context();
				ekizu::set_error_context(
					"Unsupported URL scheme for HttpConnection::connect: "
					"scheme=" +
					scheme_s + " url=" + url_s);
				std::move(h)(ekizu::make_error_code(
					ekizu::errc::http_unsupported_scheme));
			}));
		return;
	}

	const bool https = (uri.scheme() == "https");

	std::shared_ptr<ssl::context> ctx;
	if (https) {
		auto ssl_ctx =
			std::make_shared<ssl::context>(ssl::context::tlsv12_client);
		boost::system::error_code ec;

#ifdef EKIZU_TESTING
		if (g_http_set_verify_mode_for_tests != nullptr) {
			ec = g_http_set_verify_mode_for_tests(*ssl_ctx);
		} else
#endif
		{
			ssl_ctx->set_verify_mode(
				ssl::context::verify_peer |
					ssl::context::verify_fail_if_no_peer_cert,
				ec);
		}

		if (ec) {
			asio::dispatch(asio::bind_executor(
				handler_ex,
				[h = std::move(handler), ec]() mutable { std::move(h)(ec); }));
			return;
		}

#ifdef EKIZU_TESTING
		if (g_http_set_default_verify_paths_for_tests != nullptr) {
			ec = g_http_set_default_verify_paths_for_tests(*ssl_ctx);
		} else
#endif
		{
			ssl_ctx->set_default_verify_paths(ec);
		}

		if (ec) {
			asio::dispatch(asio::bind_executor(
				handler_ex,
				[h = std::move(handler), ec]() mutable { std::move(h)(ec); }));
			return;
		}

		boost::certify::enable_native_https_server_verification(*ssl_ctx);
		ctx = std::move(ssl_ctx);
	}

	auto impl = std::make_shared<Impl>(ex, https, std::move(ctx));

	std::string host = std::string(uri.host());
	std::string port;
	if (!uri.port().empty()) {
		port = std::string(uri.port());
	} else {
		port = https ? "443" : "80";
	}

	struct ConnectOp : std::enable_shared_from_this<ConnectOp> {
		std::shared_ptr<Impl> impl;
		asio::any_completion_handler<void(Result<HttpConnection>)> handler;
		CompletionExecutor handler_ex;

		ConnectOp(std::shared_ptr<Impl> i,
				  asio::any_completion_handler<void(Result<HttpConnection>)> h,
				  CompletionExecutor h_ex)
			: impl(std::move(i)),
			  handler(std::move(h)),
			  handler_ex(std::move(h_ex)) {}

		void complete(Result<> r) {
			asio::dispatch(asio::bind_executor(
				handler_ex,
				[impl = std::move(impl), h = std::move(handler), r]() mutable {
					if (!r) {
						std::move(h)(r.error());
						return;
					}
					std::move(h)(HttpConnection{std::move(impl)});
				}));
		}
	};

	auto op = std::make_shared<ConnectOp>(
		impl, std::move(handler), std::move(handler_ex));

	impl->async_run(std::move(host), std::move(port),
					[op](Result<> r) mutable { op->complete(r); });
}

void HttpConnection::request_impl(
	HttpRequest req,
	asio::any_completion_handler<void(Result<HttpResponse>)> handler,
	CompletionExecutor handler_ex) {
	if (!m_impl) {
		asio::dispatch(
			asio::bind_executor(handler_ex, [h = std::move(handler)]() mutable {
				ekizu::clear_error_context();
				ekizu::set_error_context(
					"HttpConnection::request called without a live connection "
					"(moved-from or not connected)");
				std::move(h)(
					ekizu::make_error_code(ekizu::errc::http_not_connected));
			}));
		return;
	}

	m_impl->enqueue(Impl::Pending{
		std::move(req), std::move(handler), std::move(handler_ex)});
}

void HttpConnection::get_impl(
	asio::any_io_executor ex, std::string_view url,
	asio::any_completion_handler<void(Result<HttpResponse>)> handler,
	CompletionExecutor handler_ex) {
	auto parsed = boost::urls::parse_uri(url);
	if (!parsed) {
		asio::dispatch(asio::bind_executor(
			handler_ex, [h = std::move(handler), e = parsed.error()]() mutable {
				ekizu::clear_error_context();
				ekizu::set_error_context("HTTP URL parse failed");
				std::move(h)(e);
			}));
		return;
	}

	const auto uri = parsed.value();
	const auto target = make_target(uri);

	HttpRequest req{http::verb::get, target, 11};
	req.set(http::field::host, uri.host());

	// Make a copy of handler_ex to capture in the lambda
	auto h_ex_copy = handler_ex;

	HttpConnection::connect(
		ex, url,
		asio::bind_executor(
			handler_ex,
			[req = std::move(req), h_ex = std::move(h_ex_copy),
			 h = std::move(handler)](Result<HttpConnection> conn) mutable {
				if (!conn) {
					std::move(h)(conn.error());
					return;
				}

				// Keep the connection alive until the request completion runs.
				auto sp =
					std::make_shared<HttpConnection>(std::move(conn.value()));

				sp->request(
					std::move(req),
					asio::bind_executor(
						h_ex,
						[sp, h = std::move(h)](Result<HttpResponse> r) mutable {
							(void)sp;
							std::move(h)(std::move(r));
						}));
			}));
}

HttpConnection::HttpConnection(std::shared_ptr<Impl> impl)
	: m_impl{std::move(impl)} {}

}  // namespace ekizu::net
