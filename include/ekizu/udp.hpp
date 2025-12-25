#ifndef EKIZU_UDP_HPP
#define EKIZU_UDP_HPP

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/core/span.hpp>
#include <ekizu/export.hpp>
#include <ekizu/result.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace ekizu::net {
namespace asio = boost::asio;

/// UdpSocket is a UDP socket wrapper with queued receive operations.
///
/// This API is intentionally modeled after Rust's std::net::UdpSocket:
/// - bind() creates/binds a local socket.
/// - connect() sets a default peer.
/// - send()/receive() require a connected socket.
/// - send_to()/receive_from() work without connect().
///
/// \par Thread Safety
/// All methods are thread-safe. Concurrent calls are safe.
///
/// \par Destruction Guarantees
/// The destructor guarantees that all pending receive handlers
/// will be invoked with operation_canceled, provided the associated
/// io_context continues running.
struct UdpSocket {
	UdpSocket(const UdpSocket &) = delete;
	UdpSocket &operator=(const UdpSocket &) = delete;

	EKIZU_EXPORT UdpSocket(UdpSocket &&) noexcept;
	EKIZU_EXPORT UdpSocket &operator=(UdpSocket &&) noexcept;
	EKIZU_EXPORT ~UdpSocket();

	using CompletionExecutor = asio::any_completion_executor;
	using Endpoint = asio::ip::udp::endpoint;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<UdpSocket>))
				  CompletionToken>
	[[nodiscard]] static auto bind(asio::any_io_executor ex,
								   std::string_view address,
								   CompletionToken &&token) {
		return asio::async_initiate<CompletionToken, void(Result<UdpSocket>)>(
			[ex, address = std::string(address)](auto &&handler) mutable {
				auto handler_ex = asio::get_associated_executor(handler, ex);
				asio::any_completion_handler<void(Result<UdpSocket>)> h{
					std::forward<decltype(handler)>(handler)};
				bind_impl(ex, std::move(address), std::move(h),
						  std::move(handler_ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<UdpSocket>))
				  CompletionToken>
	[[nodiscard]] static auto bind(asio::any_io_executor ex,
								   std::string_view host, std::string_view port,
								   CompletionToken &&token) {
		return asio::async_initiate<CompletionToken, void(Result<UdpSocket>)>(
			[ex, host = std::string(host),
			 port = std::string(port)](auto &&handler) mutable {
				auto handler_ex = asio::get_associated_executor(handler, ex);
				asio::any_completion_handler<void(Result<UdpSocket>)> h{
					std::forward<decltype(handler)>(handler)};
				bind_impl(ex, std::move(host), std::move(port), std::move(h),
						  std::move(handler_ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto connect(std::string host, std::string port, CompletionToken &&token) {
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, host = std::move(host),
			 port = std::move(port)](auto &&handler) mutable {
				auto handler_ex =
					asio::get_associated_executor(handler, get_executor());
				asio::any_completion_handler<void(Result<>)> h{
					std::forward<decltype(handler)>(handler)};
				connect_impl(std::move(host), std::move(port), std::move(h),
							 std::move(handler_ex));
			},
			token);
	}

	EKIZU_EXPORT Result<> close();

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<std::size_t>))
				  CompletionToken>
	auto send(boost::span<const std::byte> data, CompletionToken &&token) {
		return asio::async_initiate<CompletionToken, void(Result<std::size_t>)>(
			[this, data](auto &&handler) mutable {
				auto handler_ex =
					asio::get_associated_executor(handler, get_executor());
				asio::any_completion_handler<void(Result<std::size_t>)> h{
					std::forward<decltype(handler)>(handler)};
				send_impl(data, std::move(h), std::move(handler_ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<std::string>))
				  CompletionToken>
	auto receive(CompletionToken &&token) {
		return asio::async_initiate<CompletionToken, void(Result<std::string>)>(
			[this](auto &&handler) mutable {
				auto handler_ex =
					asio::get_associated_executor(handler, get_executor());
				asio::any_completion_handler<void(Result<std::string>)> h{
					std::forward<decltype(handler)>(handler)};
				receive_impl(std::move(h), std::move(handler_ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<std::size_t>))
				  CompletionToken>
	auto send_to(boost::span<const std::byte> data, Endpoint ep,
				 CompletionToken &&token) {
		return asio::async_initiate<CompletionToken, void(Result<std::size_t>)>(
			[this, data, ep = std::move(ep)](auto &&handler) mutable {
				auto handler_ex =
					asio::get_associated_executor(handler, get_executor());
				asio::any_completion_handler<void(Result<std::size_t>)> h{
					std::forward<decltype(handler)>(handler)};
				send_to_impl(
					data, std::move(ep), std::move(h), std::move(handler_ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<std::size_t>))
				  CompletionToken>
	auto send_to(boost::span<const std::byte> data, std::string_view address,
				 CompletionToken &&token) {
		return asio::async_initiate<CompletionToken, void(Result<std::size_t>)>(
			[this, data,
			 address = std::string(address)](auto &&handler) mutable {
				auto handler_ex =
					asio::get_associated_executor(handler, get_executor());
				asio::any_completion_handler<void(Result<std::size_t>)> h{
					std::forward<decltype(handler)>(handler)};
				send_to_str_impl(data, std::move(address), std::move(h),
								 std::move(handler_ex));
			},
			token);
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
		void(Result<std::pair<std::string, Endpoint>>)) CompletionToken>
	auto receive_from(CompletionToken &&token) {
		return asio::async_initiate<
			CompletionToken, void(Result<std::pair<std::string, Endpoint>>)>(
			[this](auto &&handler) mutable {
				auto handler_ex =
					asio::get_associated_executor(handler, get_executor());
				asio::any_completion_handler<void(
					Result<std::pair<std::string, Endpoint>>)>
					h{std::forward<decltype(handler)>(handler)};
				receive_from_impl(std::move(h), std::move(handler_ex));
			},
			token);
	}

   public:
	struct Impl;

   private:
	explicit UdpSocket(std::shared_ptr<Impl> impl);

	[[nodiscard]] asio::any_io_executor get_executor() const;

	static EKIZU_EXPORT void bind_impl(
		asio::any_io_executor ex, std::string address,
		asio::any_completion_handler<void(Result<UdpSocket>)> handler,
		CompletionExecutor handler_ex);

	static EKIZU_EXPORT void bind_impl(
		asio::any_io_executor ex, std::string host, std::string port,
		asio::any_completion_handler<void(Result<UdpSocket>)> handler,
		CompletionExecutor handler_ex);

	EKIZU_EXPORT void connect_impl(
		std::string host, std::string port,
		asio::any_completion_handler<void(Result<>)> handler,
		CompletionExecutor handler_ex);

	EKIZU_EXPORT void send_impl(
		boost::span<const std::byte> data,
		asio::any_completion_handler<void(Result<std::size_t>)> handler,
		CompletionExecutor handler_ex);

	EKIZU_EXPORT void receive_impl(
		asio::any_completion_handler<void(Result<std::string>)> handler,
		CompletionExecutor handler_ex);

	EKIZU_EXPORT void send_to_impl(
		boost::span<const std::byte> data, Endpoint ep,
		asio::any_completion_handler<void(Result<std::size_t>)> handler,
		CompletionExecutor handler_ex);

	EKIZU_EXPORT void send_to_str_impl(
		boost::span<const std::byte> data, std::string address,
		asio::any_completion_handler<void(Result<std::size_t>)> handler,
		CompletionExecutor handler_ex);

	EKIZU_EXPORT void receive_from_impl(
		asio::any_completion_handler<
			void(Result<std::pair<std::string, Endpoint>>)>
			handler,
		CompletionExecutor handler_ex);

	std::shared_ptr<Impl> m_impl;
};
}  // namespace ekizu::net

#endif	// EKIZU_UDP_HPP
