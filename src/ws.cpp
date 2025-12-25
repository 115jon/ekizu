#include <fmt/format.h>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/certify/extensions.hpp>
#include <boost/certify/https_verification.hpp>
#include <boost/system/detail/errc.hpp>
#include <boost/system/errc.hpp>
#include <boost/url.hpp>
#include <ekizu/version.hpp>
#include <ekizu/ws.hpp>
#include <queue>
#include <variant>

#ifdef EKIZU_TESTING
namespace ekizu::net {
// Test hooks (patterned after UDP hooks) to force otherwise hard-to-reach
// error branches deterministically in unit tests.
using WsSetVerifyModeFn =
	boost::system::error_code (*)(boost::asio::ssl::context &);
using WsSetDefaultVerifyPathsFn =
	boost::system::error_code (*)(boost::asio::ssl::context &);
using WsSetSniFn = bool (*)(void *native_ssl_handle, const char *host);
using WsForceDrainErrorFn = boost::system::error_code (*)();

static WsSetVerifyModeFn g_ws_set_verify_mode_for_tests = nullptr;
static WsSetDefaultVerifyPathsFn g_ws_set_default_verify_paths_for_tests =
	nullptr;
static WsSetSniFn g_ws_set_sni_for_tests = nullptr;
static WsForceDrainErrorFn g_ws_force_drain_error_for_tests = nullptr;

void ekizu_set_ws_verify_mode_for_tests(WsSetVerifyModeFn fn) {
	g_ws_set_verify_mode_for_tests = fn;
}
void ekizu_set_ws_default_verify_paths_for_tests(WsSetDefaultVerifyPathsFn fn) {
	g_ws_set_default_verify_paths_for_tests = fn;
}
void ekizu_set_ws_sni_for_tests(WsSetSniFn fn) { g_ws_set_sni_for_tests = fn; }
void ekizu_set_ws_force_drain_error_for_tests(WsForceDrainErrorFn fn) {
	g_ws_force_drain_error_for_tests = fn;
}
}  // namespace ekizu::net
#endif

namespace {
using namespace ekizu;
using namespace ekizu::net;
namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace ssl = boost::asio::ssl;
namespace outcome = boost::outcome_v2;
namespace ws = boost::beast::websocket;
namespace asio = boost::asio;
using asio::ip::tcp;

using PlainWs = ws::stream<beast::tcp_stream>;
using SslWs = ws::stream<beast::ssl_stream<beast::tcp_stream>>;
using StreamVariant = std::variant<PlainWs, SslWs>;

template <typename Stream>
void ws_decorate(bool use_ssl, Stream &stream) {
	stream.set_option(
		ws::stream_base::timeout::suggested(beast::role_type::client));
	stream.set_option(ws::stream_base::decorator([use_ssl](auto &req) {
		req.set(
			http::field::user_agent, fmt::format("ekizu/{}", EKIZU_VERSION));
	}));
}

template <typename ViaExecutor, typename Fn>
void post_via(ViaExecutor via, WebSocketClient::CompletionExecutor ex, Fn fn) {
	// Important: do NOT call asio::post(ex, ...) with any_completion_executor.
	// asio::post tries to require(ex, execution::blocking.never), which fails
	// for any_completion_executor in practice.
	//
	// Instead: post via a known-good IO executor, then execute on the
	// completion executor inside that posted continuation. If the completion
	// executor is empty, fall back to posting on 'via'.

	// Make a copy of the via executor for the inner lambda to own.
	asio::post(via, [via2 = ViaExecutor{via}, ex = std::move(ex),
					 fn = std::move(fn)]() mutable {
		if (ex) {
			ex.execute(std::move(fn));
		} else {
			// Fallback: no associated completion executor, just run on via2.
			asio::post(via2, std::move(fn));
		}
	});
}
}  // namespace

namespace ekizu::net {

struct WebSocketClient::Impl
	: std::enable_shared_from_this<WebSocketClient::Impl> {
	using CompletionExecutor = WebSocketClient::CompletionExecutor;

	struct PendingRead {
		asio::any_completion_handler<void(Result<WebSocketMessage>)> handler;
		CompletionExecutor handler_ex;
	};

	explicit Impl(asio::any_io_executor executor, tcp::resolver resolver,
				  StreamVariant stream, std::shared_ptr<ssl::context> ssl_ctx,
				  std::string host, std::string path)
		: m_ex(asio::make_strand(executor)),
		  m_resolver(std::move(resolver)),
		  m_stream(std::move(stream)),
		  m_ssl_ctx(std::move(ssl_ctx)),
		  m_host(std::move(host)),
		  m_path(std::move(path)) {}

	[[nodiscard]] asio::any_io_executor get_executor() const { return m_ex; }

	[[nodiscard]] bool is_open() const {
		return std::visit([](const auto &s) { return s.is_open(); }, m_stream);
	}

	[[nodiscard]] std::optional<ws::close_reason> close_reason() const {
		return m_close_reason;
	}

	void async_read(
		asio::any_completion_handler<void(Result<WebSocketMessage>)> handler,
		CompletionExecutor handler_ex) {
		asio::dispatch(m_ex, [self = shared_from_this(), h = std::move(handler),
							  handler_ex = std::move(handler_ex)]() mutable {
			if (self->m_cancelled) {
				auto ec =
					make_error_code(boost::system::errc::operation_canceled);
				post_via(
					self->m_ex, std::move(handler_ex),
					[h = std::move(h), ec]() mutable { std::move(h)(ec); });
				return;
			}

			self->m_read_queue.push({std::move(h), handler_ex});

			if (!self->m_reading) {
				self->m_reading = true;
				self->start_next_read();
			}
		});
	}

	void start_next_read() {
		for (;;) {
			if (m_read_queue.empty()) {
				m_reading = false;
				return;
			}

			auto pr = std::move(m_read_queue.front());
			m_read_queue.pop();

			if (m_cancelled) {
				auto ec =
					make_error_code(boost::system::errc::operation_canceled);
				post_via(m_ex, std::move(pr.handler_ex),
						 [h = std::move(pr.handler), ec]() mutable {
							 std::move(h)(ec);
						 });
				continue;
			}

			std::visit(
				[&](auto &stream) {
					stream.async_read(
						m_buffer,
						asio::bind_executor(
							m_ex, [self = shared_from_this(),
								   handler = std::move(pr.handler),
								   handler_ex = std::move(pr.handler_ex)](
									  boost::system::error_code ec,
									  std::size_t) mutable {
								if (ec == ws::error::closed) {
									self->m_close_reason = std::visit(
										[](auto &s) { return s.reason(); },
										self->m_stream);
								}

								Result<WebSocketMessage> res =
									outcome::success();
								if (ec) {
									res = ec;
								} else {
									const bool is_binary = std::visit(
										[](auto &s) { return s.got_binary(); },
										self->m_stream);
									auto data = self->m_buffer.data();
									std::string payload(
										static_cast<const char *>(data.data()),
										data.size());
									self->m_buffer.consume(
										self->m_buffer.size());
									res = WebSocketMessage{
										std::move(payload), is_binary};
								}

								post_via(self->m_ex, std::move(handler_ex),
										 [h = std::move(handler),
										  r = std::move(res)]() mutable {
											 std::move(h)(std::move(r));
										 });

								self->start_next_read();
							}));
				},
				m_stream);

			return;
		}
	}

	void async_close(ws::close_reason reason,
					 asio::any_completion_handler<void(Result<>)> handler,
					 CompletionExecutor handler_ex) {
		asio::dispatch(m_ex, [self = shared_from_this(),
							  reason = std::move(reason),
							  h = std::move(handler), handler_ex]() mutable {
			if (self->m_closing) {
				post_via(self->m_ex, handler_ex, [h = std::move(h)]() mutable {
					std::move(h)(boost::system::errc::operation_in_progress);
				});
				return;
			}
			self->m_closing = true;

			std::visit(
				[&](auto &stream) {
					stream.async_close(
						reason,
						asio::bind_executor(
							self->m_ex,
							[self, h = std::move(h),
							 handler_ex = std::move(handler_ex)](
								boost::system::error_code ec) mutable {
								self->m_closing = false;
								if (ec) {
									post_via(self->m_ex, handler_ex,
											 [h = std::move(h), ec]() mutable {
												 std::move(h)(ec);
											 });
									return;
								}

								post_via(self->m_ex, handler_ex,
										 [h = std::move(h)]() mutable {
											 std::move(h)(outcome::success());
										 });
							}));
				},
				self->m_stream);
		});
	}

	void async_send(std::string msg, bool is_binary,
					asio::any_completion_handler<void(Result<>)> handler,
					CompletionExecutor handler_ex) {
		asio::dispatch(m_ex, [self = shared_from_this(), msg = std::move(msg),
							  is_binary, h = std::move(handler),
							  handler_ex]() mutable {
			self->m_send_queue.emplace(std::move(msg), is_binary);

			// Preserve old semantics: if already draining, "send()"
			// completes immediately.
			if (self->m_sending) {
				post_via(self->m_ex, handler_ex, [h = std::move(h)]() mutable {
					std::move(h)(outcome::success());
				});
				return;
			}

			self->m_sending = true;
			std::make_shared<DrainOp>(self, std::move(h), std::move(handler_ex))
				->start();
		});
	}

	struct DrainOp : std::enable_shared_from_this<DrainOp> {
		std::shared_ptr<Impl> self;
		asio::any_completion_handler<void(Result<>)> first_handler;
		CompletionExecutor handler_ex;

		DrainOp(std::shared_ptr<Impl> s,
				asio::any_completion_handler<void(Result<>)> h,
				CompletionExecutor ex)
			: self(std::move(s)),
			  first_handler(std::move(h)),
			  handler_ex(std::move(ex)) {}

		void start() { step(outcome::success()); }

		void step(Result<> last) {
#ifdef EKIZU_TESTING
			if (g_ws_force_drain_error_for_tests != nullptr) {
				last = g_ws_force_drain_error_for_tests();
			}
#endif

			if (!last) {
				self->m_sending = false;
				post_via(self->m_ex, handler_ex,
						 [h = std::move(first_handler),
						  ec = last.error()]() mutable { std::move(h)(ec); });
				return;
			}

			if (self->m_send_queue.empty()) {
				self->m_sending = false;
				post_via(self->m_ex, handler_ex,
						 [h = std::move(first_handler)]() mutable {
							 std::move(h)(outcome::success());
						 });
				return;
			}

			auto pair = std::move(self->m_send_queue.front());
			self->m_send_queue.pop();
			auto binary = pair.second;
			auto msg_keepalive =
				std::make_shared<std::string>(std::move(pair.first));
			std::visit(
				[&](auto &stream) {
					stream.binary(binary);
					stream.async_write(
						asio::buffer(*msg_keepalive),
						asio::bind_executor(
							self->m_ex,
							[me = shared_from_this(), msg_keepalive](
								boost::system::error_code ec,
								std::size_t) mutable {
								if (ec) {
									me->step(ec);
									return;
								}
								me->step(outcome::success());
							}));
				},
				self->m_stream);
		}
	};

	void cancel() {
		asio::dispatch(m_ex, [self = shared_from_this()] {
			self->m_cancelled = true;

			std::visit(
				[](auto &stream) {
					beast::get_lowest_layer(stream).cancel();

					boost::system::error_code ignored_ec;
					beast::get_lowest_layer(stream).socket().close(ignored_ec);
				},
				self->m_stream);

			auto ec = make_error_code(boost::system::errc::operation_canceled);
			while (!self->m_read_queue.empty()) {
				auto pr = std::move(self->m_read_queue.front());
				self->m_read_queue.pop();

				post_via(self->m_ex, std::move(pr.handler_ex),
						 [h = std::move(pr.handler), ec]() mutable {
							 std::move(h)(ec);
						 });
			}

			// Also clear send queue so drain op stops
			while (!self->m_send_queue.empty()) { self->m_send_queue.pop(); }
		});
	}

	asio::any_io_executor m_ex;
	tcp::resolver m_resolver;
	StreamVariant m_stream;

	// Must outlive SslWs if used.
	std::shared_ptr<ssl::context> m_ssl_ctx;

	boost::beast::flat_buffer m_buffer;
	std::string m_host;
	std::string m_path;

	bool m_closing{};
	bool m_sending{};
	bool m_reading{false};
	bool m_cancelled{false};

	std::queue<std::pair<std::string, bool>> m_send_queue;
	std::queue<PendingRead> m_read_queue;
	std::optional<ws::close_reason> m_close_reason;
};

struct ConnectOp : std::enable_shared_from_this<ConnectOp> {
	asio::any_io_executor ex;
	tcp::resolver resolver;
	StreamVariant stream;
	std::shared_ptr<ssl::context> ssl_ctx;
	bool use_ssl{};
	std::string host;
	std::string port;
	std::string path;
	asio::any_completion_handler<void(Result<WebSocketClient>)> handler;
	WebSocketClient::CompletionExecutor handler_ex;

	ConnectOp(asio::any_io_executor ex_, bool use_ssl_, std::string host_,
			  std::string port_, std::string path_, StreamVariant stream_,
			  std::shared_ptr<ssl::context> ctx_,
			  asio::any_completion_handler<void(Result<WebSocketClient>)> h,
			  WebSocketClient::CompletionExecutor cb_ex)
		: ex(std::move(ex_)),
		  resolver(ex),
		  stream(std::move(stream_)),
		  ssl_ctx(std::move(ctx_)),
		  use_ssl(use_ssl_),
		  host(std::move(host_)),
		  port(std::move(port_)),
		  path(std::move(path_)),
		  handler(std::move(h)),
		  handler_ex(std::move(cb_ex)) {}

	void start() {
		resolver.async_resolve(
			host, port,
			asio::bind_executor(
				ex, [self = shared_from_this()](
						boost::system::error_code ec,
						tcp::resolver::results_type results) mutable {
					if (ec) {
						post_via(self->ex, self->handler_ex,
								 [h = std::move(self->handler), ec]() mutable {
									 std::move(h)(ec);
								 });
						return;
					}
					self->on_resolve(std::move(results));
				}));
	}

	void on_resolve(tcp::resolver::results_type results) {
		std::visit(
			[&](auto &s) {
				beast::get_lowest_layer(s).expires_after(
					std::chrono::seconds(30));
				beast::get_lowest_layer(s).async_connect(
					results,
					asio::bind_executor(
						ex, [self = shared_from_this()](
								boost::system::error_code ec,
								const tcp::endpoint &ep) mutable {
							if (ec) {
								post_via(self->ex, self->handler_ex,
										 [h = std::move(self->handler),
										  ec]() mutable { std::move(h)(ec); });
								return;
							}
							self->on_connect(ep);
						}));
			},
			stream);
	}

	void on_connect(tcp::endpoint ep) {
		if (!use_ssl) {
			ws_handshake(ep);
			return;
		}

		auto &s = std::get<SslWs>(stream);
		bool sni_ok = false;
		bool sni_hooked = false;
#ifdef EKIZU_TESTING
		if (g_ws_set_sni_for_tests != nullptr) {
			sni_hooked = true;
			sni_ok = g_ws_set_sni_for_tests(
				reinterpret_cast<void *>(s.next_layer().native_handle()),
				host.c_str());
		} else
#endif
		{
			sni_ok = (SSL_set_tlsext_host_name(
						  s.next_layer().native_handle(), host.c_str()) != 0);
		}

		if (!sni_ok) {
			if (sni_hooked) {
				auto ssl_ec =
					make_error_code(boost::system::errc::protocol_error);
				post_via(
					ex, handler_ex, [h = std::move(handler), ssl_ec]() mutable {
						std::move(h)(ssl_ec);
					});
				return;
			}

			beast::error_code ssl_ec{static_cast<int>(::ERR_get_error()),
									 asio::error::get_ssl_category()};
			post_via(
				ex, handler_ex, [h = std::move(handler), ssl_ec]() mutable {
					std::move(h)(ssl_ec);
				});
			return;
		}
		beast::get_lowest_layer(s).expires_after(std::chrono::seconds(30));
		s.next_layer().async_handshake(
			ssl::stream_base::client,
			asio::bind_executor(ex, [self = shared_from_this(),
									 ep](boost::system::error_code ec) mutable {
				if (ec) {
					post_via(self->ex, self->handler_ex,
							 [h = std::move(self->handler), ec]() mutable {
								 std::move(h)(ec);
							 });
					return;
				}
				self->ws_handshake(ep);
			}));
	}

	void ws_handshake(tcp::endpoint ep) {
		std::visit(
			[&](auto &s) {
				beast::get_lowest_layer(s).expires_never();
				ws_decorate(use_ssl, s);
				s.async_handshake(
					fmt::format("{}:{}", host, ep.port()), path,
					asio::bind_executor(
						ex, [self = shared_from_this()](
								boost::system::error_code ec) mutable {
							if (ec) {
								post_via(self->ex, self->handler_ex,
										 [h = std::move(self->handler),
										  ec]() mutable { std::move(h)(ec); });
								return;
							}

							auto impl = std::make_shared<WebSocketClient::Impl>(
								self->ex, std::move(self->resolver),
								std::move(self->stream),
								std::move(self->ssl_ctx), std::move(self->host),
								std::move(self->path));

							// Note: WebSocketClient constructor is explicit, so
							// we construct it here
							post_via(self->ex, self->handler_ex,
									 [h = std::move(self->handler),
									  impl = std::move(impl)]() mutable {
										 std::move(h)(
											 WebSocketClient{std::move(impl)});
									 });
						}));
			},
			stream);
	}
};

// Implementations of the ABI boundary functions from the header
WebSocketClient::WebSocketClient(WebSocketClient &&) noexcept = default;
WebSocketClient &WebSocketClient::operator=(WebSocketClient &&) noexcept =
	default;
WebSocketClient::~WebSocketClient() {
	if (m_impl) { m_impl->cancel(); }
}
WebSocketClient::WebSocketClient(std::shared_ptr<Impl> impl)
	: m_impl(std::move(impl)) {}

void WebSocketClient::connect_impl(
	std::string url, asio::any_io_executor executor,
	asio::any_completion_handler<void(Result<WebSocketClient>)> handler,
	CompletionExecutor handler_ex) {
	auto parsed = boost::urls::parse_uri(url);
	if (!parsed) {
		post_via(executor, handler_ex,
				 [h = std::move(handler), e = parsed.error()]() mutable {
					 std::move(h)(e);
				 });
		return;
	}

	auto uri = parsed.value();
	if (!uri.has_scheme() || (uri.scheme() != "ws" && uri.scheme() != "wss")) {
		post_via(executor, handler_ex, [h = std::move(handler)]() mutable {
			std::move(h)(boost::system::errc::invalid_argument);
		});
		return;
	}

	const bool use_ssl = (uri.scheme() == "wss");
	std::string host{uri.host()};
	std::string path = std::string(uri.path());
	if (!uri.query().empty()) { path += "?" + std::string(uri.query()); }
	if (path.empty()) { path = "/"; }
	std::string port =
		uri.has_port() ? std::string(uri.port()) : (use_ssl ? "443" : "80");

	std::shared_ptr<ssl::context> ctx;
	StreamVariant stream = [&]() -> StreamVariant {
		if (!use_ssl) { return PlainWs{executor}; }

		auto ssl_ctx =
			std::make_shared<ssl::context>(ssl::context::tlsv12_client);
		boost::system::error_code ec;
#ifdef EKIZU_TESTING
		if (g_ws_set_verify_mode_for_tests != nullptr) {
			ec = g_ws_set_verify_mode_for_tests(*ssl_ctx);
		} else
#endif
		{
			ssl_ctx->set_verify_mode(
				ssl::context::verify_peer |
					ssl::context::verify_fail_if_no_peer_cert,
				ec);
		}
		if (ec) {
			post_via(
				executor, handler_ex,
				[h = std::move(handler), ec]() mutable { std::move(h)(ec); });
			return PlainWs{executor};
		}

#ifdef EKIZU_TESTING
		if (g_ws_set_default_verify_paths_for_tests != nullptr) {
			ec = g_ws_set_default_verify_paths_for_tests(*ssl_ctx);
		} else
#endif
		{
			ssl_ctx->set_default_verify_paths(ec);
		}
		if (ec) {
			post_via(
				executor, handler_ex,
				[h = std::move(handler), ec]() mutable { std::move(h)(ec); });
			return PlainWs{executor};
		}
		boost::certify::enable_native_https_server_verification(*ssl_ctx);
		ctx = ssl_ctx;
		return SslWs{executor, *ssl_ctx};
	}();

	// If SSL context setup failed, the completion was already posted above
	if (use_ssl && !ctx) { return; }

	std::make_shared<ConnectOp>(
		executor, use_ssl, std::move(host), std::move(port), std::move(path),
		std::move(stream), std::move(ctx), std::move(handler),
		std::move(handler_ex))
		->start();
}

void WebSocketClient::read_impl(
	asio::any_completion_handler<void(Result<WebSocketMessage>)> handler,
	CompletionExecutor handler_ex) {
	if (!m_impl) {
		post_via(asio::system_executor(), handler_ex,
				 [h = std::move(handler)]() mutable {
					 std::move(h)(ws::error::closed);
				 });
		return;
	}
	m_impl->async_read(std::move(handler), handler_ex);
}

void WebSocketClient::send_impl(
	std::string msg, bool is_binary,
	asio::any_completion_handler<void(Result<>)> handler,
	CompletionExecutor handler_ex) {
	if (!m_impl) {
		post_via(asio::system_executor(), handler_ex,
				 [h = std::move(handler)]() mutable {
					 std::move(h)(ws::error::closed);
				 });
		return;
	}
	m_impl->async_send(
		std::move(msg), is_binary, std::move(handler), handler_ex);
}

void WebSocketClient::close_impl(
	ws::close_reason reason,
	asio::any_completion_handler<void(Result<>)> handler,
	CompletionExecutor handler_ex) {
	if (!m_impl) {
		post_via(asio::system_executor(), handler_ex,
				 [h = std::move(handler)]() mutable {
					 std::move(h)(ws::error::closed);
				 });
		return;
	}
	m_impl->async_close(std::move(reason), std::move(handler), handler_ex);
}

asio::any_io_executor WebSocketClient::get_executor() {
	// If no impl, return a default system executor or throw.
	// However, get_executor is mostly used when impl exists.
	if (!m_impl) { return asio::make_strand(asio::system_executor()); }
	return m_impl->get_executor();
}

bool WebSocketClient::is_open() const { return m_impl && m_impl->is_open(); }

std::optional<ws::close_reason> WebSocketClient::close_reason() const {
	return m_impl ? m_impl->close_reason() : std::nullopt;
}

void WebSocketClient::cancel() {
	if (m_impl) { m_impl->cancel(); }
}
}  // namespace ekizu::net
