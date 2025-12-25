#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/udp.hpp>
#include <deque>
#include <ekizu/udp.hpp>

namespace ekizu::net {
using asio::ip::udp;

namespace {

using HostPort = std::pair<std::string, std::string>;

Result<HostPort> parse_host_port(std::string_view address) {
	if (address.empty()) { return boost::system::errc::invalid_argument; }

	if (!address.empty() && address.front() == '[') {
		auto rb = address.find(']');
		if (rb == std::string_view::npos) {
			return boost::system::errc::invalid_argument;
		}
		auto host = address.substr(1, rb - 1);
		auto rest = address.substr(rb + 1);
		if (rest.size() < 2 || rest.front() != ':') {
			return boost::system::errc::invalid_argument;
		}
		auto port = rest.substr(1);
		if (port.empty()) { return boost::system::errc::invalid_argument; }
		return HostPort{std::string(host), std::string(port)};
	}

	auto colon = address.rfind(':');
	if (colon == std::string_view::npos) {
		return boost::system::errc::invalid_argument;
	}

	auto host = address.substr(0, colon);
	auto port = address.substr(colon + 1);
	if (port.empty()) { return boost::system::errc::invalid_argument; }

	if (host.empty()) { host = "0.0.0.0"; }

	return HostPort{std::string(host), std::string(port)};
}

inline boost::system::error_code operation_canceled_ec() {
	return make_error_code(boost::system::errc::operation_canceled);
}

}  // namespace

#ifdef EKIZU_TESTING
using UdpOpenFn = boost::system::error_code (*)(asio::any_io_executor,
												asio::ip::udp::socket &);
using UdpCloseFn = boost::system::error_code (*)(asio::ip::udp::socket &);
inline UdpOpenFn g_udp_open_for_tests = nullptr;
inline UdpCloseFn g_udp_close_for_tests = nullptr;
void ekizu_set_udp_open_for_tests(UdpOpenFn fn) { g_udp_open_for_tests = fn; }
void ekizu_set_udp_close_for_tests(UdpCloseFn fn) {
	g_udp_close_for_tests = fn;
}
#endif

struct UdpSocket::Impl : std::enable_shared_from_this<Impl> {
	using CompletionExecutor = UdpSocket::CompletionExecutor;
	using Endpoint = UdpSocket::Endpoint;

	explicit Impl(asio::any_io_executor ex, udp::socket socket)
		: m_ex(std::move(ex)), m_socket(std::move(socket)) {}

	Impl(const Impl &) = delete;
	Impl(Impl &&) = delete;
	Impl &operator=(Impl &&) = delete;
	Impl &operator=(const Impl &) = delete;

	~Impl() { (void)close(); }

	[[nodiscard]] asio::any_io_executor get_executor() const { return m_ex; }

	Result<> close() {
		std::deque<PendingOp> pending;
		{
			std::scoped_lock lk(m_m);
			if (m_closing) { return outcome::success(); }
			m_closing = true;
			pending.swap(m_recvq);
		}

		boost::system::error_code ec;
#ifdef EKIZU_TESTING
		if (g_udp_close_for_tests != nullptr) {
			ec = g_udp_close_for_tests(m_socket);
		} else
#endif
		{
			boost::system::error_code ignored_ec;
			m_socket.cancel(ignored_ec);
			m_socket.close(ec);
		}

		for (auto &op : pending) { op.complete_canceled(m_ex); }

		if (ec) { return ec; }
		return outcome::success();
	}

	void async_connect(std::string host, std::string port,
					   asio::any_completion_handler<void(Result<>)> handler,
					   CompletionExecutor handler_ex) {
		struct Op : std::enable_shared_from_this<Op> {
			std::shared_ptr<Impl> impl;
			udp::resolver resolver;
			std::string host, port;
			asio::any_completion_handler<void(Result<>)> handler;
			CompletionExecutor handler_ex;

			Op(std::shared_ptr<Impl> i, std::string h, std::string p,
			   asio::any_completion_handler<void(Result<>)> cb,
			   CompletionExecutor cb_ex)
				: impl(std::move(i)),
				  resolver(impl->m_ex),
				  host(std::move(h)),
				  port(std::move(p)),
				  handler(std::move(cb)),
				  handler_ex(std::move(cb_ex)) {}

			void start() {
				resolver.async_resolve(
					host, port,
					[self = shared_from_this()](
						boost::system::error_code ec,
						const udp::resolver::results_type &results) mutable {
						if (ec) {
							self->finish(ec);
							return;
						}
						if (results.empty()) {
							self->finish(boost::asio::error::host_not_found);
							return;
						}

						{
							std::scoped_lock lk(self->impl->m_m);
							if (self->impl->m_closing) {
								self->finish(operation_canceled_ec());
								return;
							}
						}

						auto ep = results.begin()->endpoint();
						self->impl->m_socket.async_connect(
							ep, [self = std::move(self)](
									boost::system::error_code ec2) mutable {
								if (!ec2) {
									std::scoped_lock lk(self->impl->m_m);
									self->impl->m_connected = true;
								}
								self->finish(ec2);
							});
					});
			}

			void finish(boost::system::error_code ec) {
				auto r = Result<>(outcome::success());
				if (ec) { r = ec; }
				asio::post(impl->m_ex,
						   asio::bind_executor(
							   handler_ex, [h = std::move(handler),
											r]() mutable { std::move(h)(r); }));
			}
		};

		auto self = shared_from_this();
		asio::post(
			m_ex,
			[op = std::make_shared<Op>(
				 self, std::move(host), std::move(port), std::move(handler),
				 std::move(handler_ex))]() mutable { op->start(); });
	}

	void async_send(
		boost::span<const std::byte> data,
		asio::any_completion_handler<void(Result<std::size_t>)> handler,
		CompletionExecutor handler_ex) {
		bool connected = false;
		{
			std::scoped_lock lk(m_m);
			connected = m_connected;
		}
		if (!connected) {
			asio::post(
				m_ex, asio::bind_executor(
						  handler_ex, [h = std::move(handler)]() mutable {
							  std::move(h)(
								  boost::system::errc::operation_not_permitted);
						  }));
			return;
		}

		auto buf =
			std::make_shared<std::vector<std::byte>>(data.begin(), data.end());
		m_socket.async_send(
			asio::buffer(*buf),
			[ex = m_ex, h_ex = std::move(handler_ex), h = std::move(handler),
			 buf](boost::system::error_code ec, std::size_t n) mutable {
				asio::post(ex, asio::bind_executor(
								   h_ex, [h = std::move(h), ec, n]() mutable {
									   if (ec) {
										   std::move(h)(ec);
									   } else {
										   std::move(h)(n);
									   }
								   }));
			});
	}

	void async_send_to(
		boost::span<const std::byte> data, Endpoint ep,
		asio::any_completion_handler<void(Result<std::size_t>)> handler,
		CompletionExecutor handler_ex) {
		auto buf =
			std::make_shared<std::vector<std::byte>>(data.begin(), data.end());
		m_socket.async_send_to(
			asio::buffer(*buf), ep,
			[ex = m_ex, h_ex = std::move(handler_ex), h = std::move(handler),
			 buf](boost::system::error_code ec, std::size_t n) mutable {
				asio::post(ex, asio::bind_executor(
								   h_ex, [h = std::move(h), ec, n]() mutable {
									   if (ec) {
										   std::move(h)(ec);
									   } else {
										   std::move(h)(n);
									   }
								   }));
			});
	}

	void async_send_to_str(
		boost::span<const std::byte> data, std::string address,
		asio::any_completion_handler<void(Result<std::size_t>)> handler,
		CompletionExecutor handler_ex) {
		struct Op : std::enable_shared_from_this<Op> {
			std::shared_ptr<Impl> impl;
			udp::resolver resolver;
			std::string address;
			boost::span<const std::byte> data;
			asio::any_completion_handler<void(Result<std::size_t>)> handler;
			CompletionExecutor handler_ex;

			Op(std::shared_ptr<Impl> i, std::string a,
			   boost::span<const std::byte> d,
			   asio::any_completion_handler<void(Result<std::size_t>)> h,
			   CompletionExecutor ex)
				: impl(std::move(i)),
				  resolver(impl->m_ex),
				  address(std::move(a)),
				  data(d),
				  handler(std::move(h)),
				  handler_ex(std::move(ex)) {}

			void start() {
				auto hp = parse_host_port(address);
				if (!hp) {
					finish(hp.error());
					return;
				}

				resolver.async_resolve(
					hp.value().first, hp.value().second,
					[self = shared_from_this()](
						boost::system::error_code ec,
						const udp::resolver::results_type &results) mutable {
						if (ec) {
							self->finish(ec);
							return;
						}
						if (results.empty()) {
							self->finish(make_error_code(
								boost::asio::error::host_not_found));
							return;
						}
						self->impl->async_send_to(
							self->data, results.begin()->endpoint(),
							std::move(self->handler), self->handler_ex);
					});
			}

			void finish(Result<std::size_t> r) {
				asio::post(impl->m_ex,
						   asio::bind_executor(
							   handler_ex, [h = std::move(handler),
											r]() mutable { std::move(h)(r); }));
			}
		};

		auto self = shared_from_this();
		asio::post(
			m_ex, [op = std::make_shared<Op>(
					   self, std::move(address), data, std::move(handler),
					   std::move(handler_ex))]() mutable { op->start(); });
	}

	struct PendingOp {
		enum class Kind : uint8_t { Connected, From };
		Kind kind{};
		CompletionExecutor handler_ex;

		asio::any_completion_handler<void(Result<std::string>)> handler_str;
		asio::any_completion_handler<void(
			Result<std::pair<std::string, Endpoint>>)>
			handler_from;

		static PendingOp connected(
			asio::any_completion_handler<void(Result<std::string>)> h,
			CompletionExecutor ex) {
			PendingOp op;
			op.kind = Kind::Connected;
			op.handler_ex = std::move(ex);
			op.handler_str = std::move(h);
			return op;
		}

		static PendingOp from(asio::any_completion_handler<void(
								  Result<std::pair<std::string, Endpoint>>)>
								  h,
							  CompletionExecutor ex) {
			PendingOp op;
			op.kind = Kind::From;
			op.handler_ex = std::move(ex);
			op.handler_from = std::move(h);
			return op;
		}

		void complete_canceled(asio::any_io_executor net_ex) {
			auto h_ex = handler_ex;
			asio::post(
				net_ex,
				asio::bind_executor(h_ex, [op = std::move(*this)]() mutable {
					if (op.kind == Kind::Connected) {
						auto h = std::move(op.handler_str);
						std::move(h)(operation_canceled_ec());
					} else {
						auto h = std::move(op.handler_from);
						std::move(h)(operation_canceled_ec());
					}
				}));
		}
	};

	void async_receive(
		asio::any_completion_handler<void(Result<std::string>)> handler,
		CompletionExecutor handler_ex) {
		bool connected = false;
		bool need_start = false;
		{
			std::scoped_lock lk(m_m);
			if (m_closing) {
				asio::post(
					m_ex, asio::bind_executor(
							  handler_ex, [h = std::move(handler)]() mutable {
								  std::move(h)(operation_canceled_ec());
							  }));
				return;
			}
			connected = m_connected;
			if (!connected) {
				asio::post(
					m_ex,
					asio::bind_executor(
						handler_ex, [h = std::move(handler)]() mutable {
							std::move(h)(
								boost::system::errc::operation_not_permitted);
						}));
				return;
			}

			m_recvq.push_back(PendingOp::connected(
				std::move(handler), std::move(handler_ex)));
			need_start = !m_receive_inflight;
			if (need_start) { m_receive_inflight = true; }
		}

		if (need_start) { start_receive(); }
	}

	void async_receive_from(asio::any_completion_handler<
								void(Result<std::pair<std::string, Endpoint>>)>
								handler,
							CompletionExecutor handler_ex) {
		bool need_start = false;
		{
			std::scoped_lock lk(m_m);
			if (m_closing) {
				asio::post(
					m_ex, asio::bind_executor(
							  handler_ex, [h = std::move(handler)]() mutable {
								  std::move(h)(operation_canceled_ec());
							  }));
				return;
			}

			m_recvq.push_back(
				PendingOp::from(std::move(handler), std::move(handler_ex)));
			need_start = !m_receive_inflight;
			if (need_start) { m_receive_inflight = true; }
		}

		if (need_start) { start_receive(); }
	}

   private:
	void start_receive() {
		PendingOp op;
		{
			std::scoped_lock lk(m_m);
			if (m_closing) {
				m_receive_inflight = false;
				return;
			}
			if (m_recvq.empty()) {
				m_receive_inflight = false;
				return;
			}
			op = std::move(m_recvq.front());
			m_recvq.pop_front();
		}

		auto buf = std::make_shared<std::array<char, 2048>>();
		auto sender = std::make_shared<Endpoint>();
		auto self = shared_from_this();

		m_socket.async_receive_from(
			asio::buffer(*buf), *sender,
			[self, op = std::move(op), buf,
			 sender](boost::system::error_code ec, std::size_t n) mutable {
				(void)sender;

				bool closing = false;
				{
					std::scoped_lock lk(self->m_m);
					closing = self->m_closing;
				}
				if (ec && closing) { ec = operation_canceled_ec(); }

				if (ec) {
					auto h_ex = op.handler_ex;
					asio::post(self->m_ex,
							   asio::bind_executor(h_ex, [op = std::move(op),
														  ec]() mutable {
								   if (op.kind == PendingOp::Kind::Connected) {
									   auto h = std::move(op.handler_str);
									   std::move(h)(ec);
								   } else {
									   auto h = std::move(op.handler_from);
									   std::move(h)(ec);
								   }
							   }));
				} else {
					auto msg = std::string(buf->data(), n);
					auto ep = *sender;
					auto h_ex = op.handler_ex;
					asio::post(
						self->m_ex,
						asio::bind_executor(
							h_ex, [op = std::move(op), msg = std::move(msg),
								   ep]() mutable {
								if (op.kind == PendingOp::Kind::Connected) {
									auto h = std::move(op.handler_str);
									std::move(h)(std::move(msg));
								} else {
									auto h = std::move(op.handler_from);
									std::move(h)(
										std::make_pair(std::move(msg), ep));
								}
							}));
				}

				bool start_next = false;
				{
					std::scoped_lock lk(self->m_m);
					start_next = !self->m_closing && !self->m_recvq.empty();
					if (!start_next) { self->m_receive_inflight = false; }
				}
				if (start_next) { self->start_receive(); }
			});
	}

	std::mutex m_m;
	std::deque<PendingOp> m_recvq;
	bool m_receive_inflight = false;
	bool m_closing = false;
	bool m_connected = false;

	asio::any_io_executor m_ex;
	udp::socket m_socket;
};

UdpSocket::UdpSocket(std::shared_ptr<Impl> impl) : m_impl(std::move(impl)) {}
UdpSocket::UdpSocket(UdpSocket &&) noexcept = default;
UdpSocket &UdpSocket::operator=(UdpSocket &&) noexcept = default;
UdpSocket::~UdpSocket() { (void)close(); }

asio::any_io_executor UdpSocket::get_executor() const {
	return m_impl ? m_impl->get_executor() : asio::any_io_executor{};
}

Result<> UdpSocket::close() {
	return m_impl ? m_impl->close() : outcome::success();
}

void UdpSocket::bind_impl(
	asio::any_io_executor ex, std::string address,
	asio::any_completion_handler<void(Result<UdpSocket>)> handler,
	CompletionExecutor handler_ex) {
	auto hp = parse_host_port(address);
	if (!hp) {
		asio::post(
			ex, asio::bind_executor(handler_ex, [h = std::move(handler),
												 e = hp.error()]() mutable {
				std::move(h)(e);
			}));
		return;
	}
	bind_impl(ex, std::move(hp.value().first), std::move(hp.value().second),
			  std::move(handler), std::move(handler_ex));
}

void UdpSocket::bind_impl(
	asio::any_io_executor ex, std::string host, std::string port,
	asio::any_completion_handler<void(Result<UdpSocket>)> handler,
	CompletionExecutor handler_ex) {
	struct Op : std::enable_shared_from_this<Op> {
		asio::any_io_executor ex;
		udp::resolver resolver;
		std::string host, port;
		asio::any_completion_handler<void(Result<UdpSocket>)> handler;
		CompletionExecutor handler_ex;

		Op(asio::any_io_executor ex_, std::string h, std::string p,
		   asio::any_completion_handler<void(Result<UdpSocket>)> cb,
		   CompletionExecutor cb_ex)
			: ex(std::move(ex_)),
			  resolver(ex),
			  host(std::move(h)),
			  port(std::move(p)),
			  handler(std::move(cb)),
			  handler_ex(std::move(cb_ex)) {}

		void start() {
			resolver.async_resolve(
				host, port,
				[self = shared_from_this()](
					boost::system::error_code ec,
					const udp::resolver::results_type &results) mutable {
					if (ec) {
						self->finish(ec);
						return;
					}
					if (results.empty()) {
						self->finish(boost::asio::error::host_not_found);
						return;
					}
					self->on_resolve(results.begin()->endpoint());
				});
		}

		void on_resolve(const udp::endpoint &local_ep) {
			boost::system::error_code ec;
			udp::socket sock{ex};

#ifdef EKIZU_TESTING
			if (g_udp_open_for_tests != nullptr) {
				ec = g_udp_open_for_tests(ex, sock);
			} else
#endif
			{
				sock.open(local_ep.protocol(), ec);
			}

			if (ec) {
				finish(ec);
				return;
			}

			sock.bind(local_ep, ec);
			if (ec) {
				finish(ec);
				return;
			}

			auto impl = std::make_shared<UdpSocket::Impl>(ex, std::move(sock));
			finish(UdpSocket{std::move(impl)});
		}

		void finish(Result<UdpSocket> r) {
			asio::post(ex, asio::bind_executor(
							   handler_ex, [h = std::move(handler),
											r = std::move(r)]() mutable {
								   std::move(h)(std::move(r));
							   }));
		}
	};

	std::make_shared<Op>(std::move(ex), std::move(host), std::move(port),
						 std::move(handler), std::move(handler_ex))
		->start();
}

void UdpSocket::connect_impl(
	std::string host, std::string port,
	asio::any_completion_handler<void(Result<>)> handler,
	CompletionExecutor handler_ex) {
	if (!m_impl) {
		asio::post(
			asio::system_executor(),
			asio::bind_executor(handler_ex, [h = std::move(handler)]() mutable {
				std::move(h)(boost::system::errc::operation_not_permitted);
			}));
		return;
	}
	m_impl->async_connect(std::move(host), std::move(port), std::move(handler),
						  std::move(handler_ex));
}

void UdpSocket::send_impl(
	boost::span<const std::byte> data,
	asio::any_completion_handler<void(Result<std::size_t>)> handler,
	CompletionExecutor handler_ex) {
	if (!m_impl) {
		asio::post(
			asio::system_executor(),
			asio::bind_executor(handler_ex, [h = std::move(handler)]() mutable {
				std::move(h)(boost::system::errc::operation_not_permitted);
			}));
		return;
	}
	m_impl->async_send(data, std::move(handler), std::move(handler_ex));
}

void UdpSocket::receive_impl(
	asio::any_completion_handler<void(Result<std::string>)> handler,
	CompletionExecutor handler_ex) {
	if (!m_impl) {
		asio::post(
			asio::system_executor(),
			asio::bind_executor(handler_ex, [h = std::move(handler)]() mutable {
				std::move(h)(boost::system::errc::operation_not_permitted);
			}));
		return;
	}
	m_impl->async_receive(std::move(handler), std::move(handler_ex));
}

void UdpSocket::send_to_impl(
	boost::span<const std::byte> data, Endpoint ep,
	asio::any_completion_handler<void(Result<std::size_t>)> handler,
	CompletionExecutor handler_ex) {
	if (!m_impl) {
		asio::post(
			asio::system_executor(),
			asio::bind_executor(handler_ex, [h = std::move(handler)]() mutable {
				std::move(h)(boost::system::errc::operation_not_permitted);
			}));
		return;
	}
	m_impl->async_send_to(
		data, std::move(ep), std::move(handler), std::move(handler_ex));
}

void UdpSocket::send_to_str_impl(
	boost::span<const std::byte> data, std::string address,
	asio::any_completion_handler<void(Result<std::size_t>)> handler,
	CompletionExecutor handler_ex) {
	if (!m_impl) {
		asio::post(
			asio::system_executor(),
			asio::bind_executor(handler_ex, [h = std::move(handler)]() mutable {
				std::move(h)(boost::system::errc::operation_not_permitted);
			}));
		return;
	}
	m_impl->async_send_to_str(
		data, std::move(address), std::move(handler), std::move(handler_ex));
}

void UdpSocket::receive_from_impl(
	asio::any_completion_handler<void(Result<std::pair<std::string, Endpoint>>)>
		handler,
	CompletionExecutor handler_ex) {
	if (!m_impl) {
		asio::post(
			asio::system_executor(),
			asio::bind_executor(handler_ex, [h = std::move(handler)]() mutable {
				std::move(h)(boost::system::errc::operation_not_permitted);
			}));
		return;
	}
	m_impl->async_receive_from(std::move(handler), std::move(handler_ex));
}

}  // namespace ekizu::net