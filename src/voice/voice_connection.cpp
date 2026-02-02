#include <sodium/core.h>

#include <boost/asio/post.hpp>
#include <ekizu/voice_connection.hpp>

#include "voice_connection_impl.hpp"

namespace ekizu {

namespace {

template <typename Fn>
void post_via(asio::any_io_executor via, VoiceConnection::CompletionExecutor ex,
			  Fn fn) {
	asio::post(via, [ex = std::move(ex), fn = std::move(fn)]() mutable {
		ex.execute(std::move(fn));
	});
}

template <typename R>
void post_completion(asio::any_io_executor via,
					 VoiceConnection::CompletionExecutor ex,
					 asio::any_completion_handler<void(R)> h, R r) {
	post_via(std::move(via), std::move(ex),
			 [h = std::move(h), r = std::move(r)]() mutable {
				 std::move(h)(std::move(r));
			 });
}

}  // namespace

VoiceConnection::VoiceConnection(VoiceConnection &&) noexcept = default;

VoiceConnection &VoiceConnection::operator=(VoiceConnection &&other) noexcept {
	if (this != &other) {
		if (m_impl) { m_impl->request_stop(); }
		m_impl = std::move(other.m_impl);
	}
	return *this;
}

VoiceConnection::~VoiceConnection() {
	if (m_impl) { m_impl->request_stop(); }
}

VoiceConnection::VoiceConnection(std::shared_ptr<Impl> impl)
	: m_impl(std::move(impl)) {}

std::optional<
	asio::experimental::channel<void(boost::system::error_code, Packet)>> &
VoiceConnection::recv_chan() {
	return m_impl->recv_chan();
}

void VoiceConnection::request_stop() { m_impl->request_stop(); }

asio::any_io_executor VoiceConnection::get_executor() const {
	return m_impl->get_executor();
}

void VoiceConnection::close_impl(asio::any_completion_handler<void(Result<>)> h,
								 CompletionExecutor hex) {
	auto via = m_impl->get_executor();
	m_impl->close(
		[via, h = std::move(h), hex = std::move(hex)](Result<> r) mutable {
			post_completion(via, std::move(hex), std::move(h), r);
		});
}

void VoiceConnection::reconnect_impl(
	asio::any_completion_handler<void(Result<>)> h, CompletionExecutor hex) {
	auto via = m_impl->get_executor();
	m_impl->reconnect(
		[via, h = std::move(h), hex = std::move(hex)](Result<> r) mutable {
			post_completion(via, std::move(hex), std::move(h), r);
		});
}

void VoiceConnection::run_impl(asio::any_completion_handler<void(Result<>)> h,
							   CompletionExecutor hex) {
	auto via = m_impl->get_executor();
	m_impl->run(
		[via, h = std::move(h), hex = std::move(hex)](Result<> r) mutable {
			post_completion(via, std::move(hex), std::move(h), r);
		});
}

void VoiceConnection::send_opus_impl(
	std::vector<std::byte> data, asio::any_completion_handler<void(Result<>)> h,
	CompletionExecutor hex) {
	auto via = m_impl->get_executor();
	m_impl->send_opus(
		std::move(data),
		[via, h = std::move(h), hex = std::move(hex)](Result<> r) mutable {
			post_completion(via, std::move(hex), std::move(h), r);
		});
}

void VoiceConnection::send_raw_impl(
	std::vector<int16_t> data, asio::any_completion_handler<void(Result<>)> h,
	CompletionExecutor hex) {
	auto via = m_impl->get_executor();
	m_impl->send_raw(
		std::move(data),
		[via, h = std::move(h), hex = std::move(hex)](Result<> r) mutable {
			post_completion(via, std::move(hex), std::move(h), r);
		});
}

void VoiceConnection::silence_impl(
	asio::any_completion_handler<void(Result<>)> h, CompletionExecutor hex) {
	auto via = m_impl->get_executor();
	m_impl->silence(
		[via, h = std::move(h), hex = std::move(hex)](Result<> r) mutable {
			post_completion(via, std::move(hex), std::move(h), r);
		});
}

void VoiceConnection::speak_impl(SpeakerFlag flags,
								 asio::any_completion_handler<void(Result<>)> h,
								 CompletionExecutor hex) {
	auto via = m_impl->get_executor();
	m_impl->speak(flags, [via, h = std::move(h),
						  hex = std::move(hex)](Result<> r) mutable {
		post_completion(via, std::move(hex), std::move(h), r);
	});
}

void VoiceConnection::flush_impl(asio::any_completion_handler<void(Result<>)> h,
								 CompletionExecutor hex) {
	auto via = m_impl->get_executor();
	m_impl->flush(
		[via, h = std::move(h), hex = std::move(hex)](Result<> r) mutable {
			post_completion(via, std::move(hex), std::move(h), r);
		});
}

void VoiceConnectionConfig::connect_impl(
	asio::any_io_executor executor,
	asio::any_completion_handler<void(Result<VoiceConnection>)> h,
	CompletionExecutor hex) const {
	// Early validation
	if (!state || !state->guild_id || !endpoint || !token) {
		post_via(executor, std::move(hex), [h = std::move(h)]() mutable {
			std::move(h)(boost::system::errc::invalid_argument);
		});
		return;
	}

	if (sodium_init() < 0) {
		post_via(executor, std::move(hex), [h = std::move(h)]() mutable {
			std::move(h)(boost::system::errc::io_error);
		});
		return;
	}

	auto decoder_res = create_decoder();
	if (!decoder_res) {
		post_via(executor, std::move(hex),
				 [h = std::move(h), ec = decoder_res.error()]() mutable {
					 std::move(h)(ec);
				 });
		return;
	}

	auto encoder_res = create_encoder();
	if (!encoder_res) {
		post_via(executor, std::move(hex),
				 [h = std::move(h), ec = encoder_res.error()]() mutable {
					 std::move(h)(ec);
				 });
		return;
	}

	auto repacketizer_res = create_repacketizer();
	if (!repacketizer_res) {
		post_via(executor, std::move(hex),
				 [h = std::move(h), ec = repacketizer_res.error()]() mutable {
					 std::move(h)(ec);
				 });
		return;
	}

	auto url = fmt::format("wss://{}/?v=8", *endpoint);

	auto url_copy = url;
	auto state_copy = *state;
	auto token_copy = *token;

	net::WebSocketClient::connect(
		executor, url,
		[executor, hex = std::move(hex), url = std::move(url_copy),
		 state_copy = std::move(state_copy), token_copy = std::move(token_copy),
		 decoder = std::move(decoder_res.value()),
		 encoder = std::move(encoder_res.value()),
		 repacketizer = std::move(repacketizer_res.value()),
		 h = std::move(h)](Result<net::WebSocketClient> ws_res) mutable {
			if (!ws_res) {
				post_via(executor, std::move(hex),
						 [h = std::move(h), ec = ws_res.error()]() mutable {
							 std::move(h)(ec);
						 });
				return;
			}

			auto impl = std::make_shared<VoiceConnection::Impl>(
				executor, std::move(ws_res.value()), std::move(state_copy), url,
				token_copy,
				std::make_unique<Codec>(std::move(decoder), std::move(encoder),
										std::move(repacketizer)));

			post_via(executor, std::move(hex),
					 [h = std::move(h), impl = std::move(impl)]() mutable {
						 std::move(h)(VoiceConnection{std::move(impl)});
					 });
		});
}

}  // namespace ekizu
