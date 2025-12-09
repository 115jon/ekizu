#include <boost/asio/detached.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/certify/https_verification.hpp>
#include <boost/url/parse.hpp>
#include <ekizu/http.hpp>

namespace {
using boost::asio::ip::tcp;
namespace beast = boost::beast;
namespace ssl = boost::asio::ssl;

template <typename F>
struct defer {
	F f;
	explicit defer(F &&f_) : f(std::move(f_)) {}
	~defer() { f(); }
};
}  // namespace

namespace ekizu::net {
struct HttpConnection::Impl {
	Impl(asio::any_io_executor ex, ssl::context &ctx)
		: m_resolver{ex}, m_stream{ex, ctx} {}

	ekizu::Result<> run(std::string_view host, std::string_view port,
						const asio::yield_context &yield) {
		if (!SSL_set_tlsext_host_name(m_stream.native_handle(), host.data())) {
			beast::error_code ec{static_cast<int>(::ERR_get_error()),
								 asio::error::get_ssl_category()};
			return ec;
		}

		boost::system::error_code ec;
		const auto results = m_resolver.async_resolve(host, port, yield[ec]);

		if (ec) { return ec; }

		beast::get_lowest_layer(m_stream).expires_after(
			std::chrono::seconds(30));
		beast::get_lowest_layer(m_stream).async_connect(results, yield[ec]);

		if (ec) { return ec; }

		m_stream.async_handshake(ssl::stream_base::client, yield[ec]);

		if (ec) { return ec; }

		return outcome::success();
	}

	Result<HttpResponse> do_request(const HttpRequest &req,
									const asio::yield_context &yield) {
		defer timer_d{[this] {
			--m_tasks;
			m_timer->cancel_one();
		}};

		boost::system::error_code ec;
		http::async_write(m_stream, req, yield[ec]);

		if (ec) { return ec; }

		HttpResponse res;
		http::async_read(m_stream, m_buffer, res, yield[ec]);

		if (ec) { return ec; }

		return res;
	}

	Result<HttpResponse> request(const HttpRequest &req,
								 const asio::yield_context &yield) {
		++m_tasks;

		if (m_tasks == 1) {
			m_timer.emplace(yield.get_executor());
			m_timer->expires_at(boost::posix_time::pos_infin);
		} else {
			boost::system::error_code ec;
			m_timer->async_wait(yield[ec]);
		}

		return do_request(req, yield);
	}

   private:
	asio::ip::tcp::resolver m_resolver;
	beast::ssl_stream<beast::tcp_stream> m_stream;
	beast::flat_buffer m_buffer;
	uint64_t m_tasks{};
	std::optional<asio::deadline_timer> m_timer;
};

Result<HttpResponse> HttpConnection::request(const HttpRequest &req,
											 const asio::yield_context &yield) {
	return m_impl->request(req, yield);
}

HttpConnection::HttpConnection(HttpConnection &&) noexcept = default;
HttpConnection &HttpConnection::operator=(HttpConnection &&) noexcept = default;
HttpConnection::~HttpConnection() = default;

Result<HttpConnection> HttpConnection::connect(
	std::string_view url, const asio::yield_context &yield) {
	EKIZU_TRY(auto uri, boost::urls::parse_uri(url));

	if (uri.scheme() != "http" && uri.scheme() != "https") {
		return boost::system::errc::not_supported;
	}

	ssl::context ctx{ssl::context::tlsv12_client};

	if (boost::system::error_code ec;
		ctx.set_verify_mode(ssl::context::verify_peer |
								ssl::context::verify_fail_if_no_peer_cert,
							ec) ||
		ctx.set_default_verify_paths(ec)) {
		return ec;
	}

	boost::certify::enable_native_https_server_verification(ctx);

	auto impl = std::make_shared<Impl>(yield.get_executor(), ctx);

	EKIZU_TRY(
		impl->run(uri.host(), uri.scheme() == "http" ? "80" : "443", yield));

	return HttpConnection{std::move(impl)};
}

Result<HttpResponse> HttpConnection::get(std::string_view url,
										 const asio::yield_context &yield) {
	EKIZU_TRY(auto uri, boost::urls::parse_uri(url));
	EKIZU_TRY(auto conn, connect(url, yield));

	HttpRequest req{
		http::verb::get, uri.path().empty() ? "/" : uri.encoded_path(), 11};
	req.set(http::field::host, uri.host());

	return conn.request(req, yield);
}

HttpConnection::HttpConnection(std::shared_ptr<Impl> impl)
	: m_impl{std::move(impl)} {}
}  // namespace ekizu::net