#ifndef EKIZU_WS_HPP
#define EKIZU_WS_HPP

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/core/span.hpp>
#include <ekizu/export.hpp>
#include <ekizu/result.hpp>
#include <memory>
#include <optional>
#include <string>

namespace ekizu::net {
namespace ws = boost::beast::websocket;
using WebSocketCloseCode = ws::close_code;

struct WebSocketMessage {
	std::string payload;
	bool is_binary;
};

struct WebSocketClient {
	WebSocketClient(const WebSocketClient &) = delete;
	WebSocketClient &operator=(const WebSocketClient &) = delete;

	EKIZU_EXPORT WebSocketClient(WebSocketClient &&) noexcept;
	EKIZU_EXPORT WebSocketClient &operator=(WebSocketClient &&) noexcept;
	EKIZU_EXPORT ~WebSocketClient();

	using CompletionExecutor = boost::asio::any_completion_executor;

	/**
	 * @brief Asynchronously connects to a WebSocket URL.
	 *
	 * @param url The URL (ws:// or wss://)
	 * @param token The completion token (callback, use_awaitable, etc.)
	 */
	template <typename CompletionToken = boost::asio::
				  default_completion_token_t<boost::asio::any_io_executor>>
	static auto connect(
		boost::asio::any_io_executor executor, std::string url,
		CompletionToken &&token = boost::asio::default_completion_token_t<
			boost::asio::any_io_executor>{}) {
		return boost::asio::async_initiate<CompletionToken,
										   void(Result<WebSocketClient>)>(
			[](auto handler, std::string u, boost::asio::any_io_executor ex) {
				CompletionExecutor handler_ex =
					boost::asio::get_associated_executor(handler, ex);

				connect_impl(
					std::move(u), std::move(ex),
					[h = std::move(handler)](
						Result<WebSocketClient> r) mutable {
						std::move(h)(std::move(r));
					},
					std::move(handler_ex));
			},
			token, std::move(url), executor);
	}

	/**
	 * @brief Asynchronously reads a message.
	 */
	template <typename CompletionToken>
	auto read(CompletionToken &&token) {
		return boost::asio::async_initiate<CompletionToken,
										   void(Result<WebSocketMessage>)>(
			[this](auto handler) {
				CompletionExecutor handler_ex =
					boost::asio::get_associated_executor(
						handler, get_executor());

				read_impl(
					[h = std::move(handler)](
						Result<WebSocketMessage> r) mutable {
						std::move(h)(std::move(r));
					},
					std::move(handler_ex));
			},
			token);
	}

	/**
	 * @brief Asynchronously closes the connection.
	 */
	template <typename CompletionToken>
	auto close(ws::close_reason reason, CompletionToken &&token) {
		return boost::asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, reason](auto handler) mutable {
				CompletionExecutor handler_ex =
					boost::asio::get_associated_executor(
						handler, get_executor());

				close_impl(
					reason,
					[h = std::move(handler)](Result<> r) mutable {
						std::move(h)(std::move(r));
					},
					std::move(handler_ex));
			},
			token);
	}

	// Convenience wrapper for string literals
	template <typename CompletionToken>
	auto send(std::string_view msg, CompletionToken &&token) {
		return send(
			std::string(msg), false, std::forward<CompletionToken>(token));
	}

	// Convenience wrapper for bytes
	template <typename CompletionToken>
	auto send_bytes(boost::span<const std::byte> msg, CompletionToken &&token) {
		return send(
			std::string(reinterpret_cast<const char *>(msg.data()), msg.size()),
			true, std::forward<CompletionToken>(token));
	}

	[[nodiscard]] bool is_open() const;
	[[nodiscard]] boost::asio::any_io_executor get_executor();
	[[nodiscard]] std::optional<ws::close_reason> close_reason() const;
	void cancel();

   private:
	friend struct ConnectOp;
	struct Impl;
	std::shared_ptr<Impl> m_impl;

	// Internal constructor used by connect_impl
	explicit WebSocketClient(std::shared_ptr<Impl> impl);

	/**
	 * @brief Asynchronously sends a message.
	 */
	template <typename CompletionToken>
	auto send(std::string msg, bool is_binary, CompletionToken &&token) {
		return boost::asio::async_initiate<CompletionToken, void(Result<>)>(
			[this, msg = std::move(msg), is_binary](auto handler) mutable {
				CompletionExecutor handler_ex =
					boost::asio::get_associated_executor(
						handler, get_executor());

				send_impl(
					std::move(msg), is_binary,
					[h = std::move(handler)](Result<> r) mutable {
						std::move(h)(std::move(r));
					},
					std::move(handler_ex));
			},
			token);
	}

	// ABI Boundary: These methods accept type-erased handlers
	static void connect_impl(
		std::string url, boost::asio::any_io_executor executor,
		boost::asio::any_completion_handler<void(Result<WebSocketClient>)>
			handler,
		CompletionExecutor handler_ex);

	void read_impl(
		boost::asio::any_completion_handler<void(Result<WebSocketMessage>)>
			handler,
		CompletionExecutor handler_ex);

	void send_impl(std::string msg, bool is_binary,
				   boost::asio::any_completion_handler<void(Result<>)> handler,
				   CompletionExecutor handler_ex);

	void close_impl(ws::close_reason reason,
					boost::asio::any_completion_handler<void(Result<>)> handler,
					CompletionExecutor handler_ex);
};

}  // namespace ekizu::net

#endif	// EKIZU_WS_HPP
