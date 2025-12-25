#ifndef EKIZU_REQUEST_EDIT_CHANNEL_PERMISSIONS_HPP
#define EKIZU_REQUEST_EDIT_CHANNEL_PERMISSIONS_HPP

#include <ekizu/channel.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct EditChannelPermissions {
	EditChannelPermissions(RequestSender sender, Snowflake channel_id,
						   PermissionOverwrite overwrite);

	EKIZU_EXPORT operator net::HttpRequest() const;

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<>)) CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<>)>(
			[this](auto &&handler) {
				m_sender.send(
					*this, [h = std::forward<decltype(handler)>(handler)](
							   Result<net::HttpResponse> res) mutable {
						if (!res) { return std::move(h)(res.error()); }

						if (res.value().result() !=
							net::HttpStatus::no_content) {
							return std::move(h)(
								boost::system::errc::invalid_argument);
						}

						std::move(h)(outcome::success());
					});
			},
			token);
	}

   private:
	Snowflake m_channel_id;
	PermissionOverwrite m_overwrite;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_EDIT_CHANNEL_PERMISSIONS_HPP
