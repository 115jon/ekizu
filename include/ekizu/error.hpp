#ifndef EKIZU_ERROR_HPP
#define EKIZU_ERROR_HPP

#include <boost/system/error_category.hpp>
#include <boost/system/error_code.hpp>
#include <string>
#include <type_traits>

namespace ekizu {

enum class errc : int {
	// JSON
	json_parse_failed = 1,
	json_deserialize_failed = 2,
	json_schema_mismatch = 3,
	invalid_outgoing_payload = 4,

	// HTTP (transport / connection layer)
	http_unsupported_scheme = 100,
	http_not_connected = 101,

	// HTTP client (Discord API wrapper)
	http_not_authenticated = 120,

	// HTTP status (Discord API semantics)
	http_redirection = 130,
	http_client_error = 131,
	http_rate_limited = 132,
	http_server_error = 133,

	// Rate limiter
	rate_limiter_stopped = 140,
	rate_limiter_send_disabled = 141,

	// UDP
	udp_invalid_address = 160,
	udp_not_initialized = 161,
	udp_not_connected = 162,
	udp_closed = 163,

	// Reserved
	http_error = 200,
};

namespace detail {
class ekizu_error_category final : public boost::system::error_category {
   public:
	const char *name() const noexcept override { return "ekizu"; }

	std::string message(int ev) const override {
		switch (static_cast<errc>(ev)) {
			case errc::json_parse_failed: return "JSON parse failed";
			case errc::json_deserialize_failed:
				return "JSON deserialize failed";
			case errc::json_schema_mismatch: return "JSON schema mismatch";
			case errc::invalid_outgoing_payload:
				return "Invalid outgoing payload";

			case errc::http_unsupported_scheme:
				return "HTTP unsupported scheme";
			case errc::http_not_connected: return "HTTP not connected";
			case errc::http_not_authenticated: return "HTTP not authenticated";

			case errc::http_redirection: return "HTTP redirection";
			case errc::http_client_error: return "HTTP client error";
			case errc::http_rate_limited: return "HTTP rate limited";
			case errc::http_server_error: return "HTTP server error";

			case errc::rate_limiter_stopped: return "Rate limiter stopped";
			case errc::rate_limiter_send_disabled:
				return "Rate limiter send disabled";

			case errc::udp_invalid_address: return "UDP invalid address";
			case errc::udp_not_initialized: return "UDP not initialized";
			case errc::udp_not_connected: return "UDP not connected";
			case errc::udp_closed: return "UDP closed";

			case errc::http_error: return "HTTP error";
			default: return "Unknown ekizu error";
		}
	}
};
}  // namespace detail

inline const boost::system::error_category &error_category() noexcept {
	static detail::ekizu_error_category cat;
	return cat;
}

inline boost::system::error_code make_error_code(errc e) noexcept {
	return boost::system::error_code(static_cast<int>(e), error_category());
}

}  // namespace ekizu

namespace boost::system {
template <>
struct is_error_code_enum<::ekizu::errc> : std::true_type {};
}  // namespace boost::system

#endif	// EKIZU_ERROR_HPP
