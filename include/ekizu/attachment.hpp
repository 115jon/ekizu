#ifndef EKIZU_ATTACHMENT_HPP
#define EKIZU_ATTACHMENT_HPP

#include <boost/asio/spawn.hpp>
#include <ekizu/http.hpp>
#include <ekizu/snowflake.hpp>
#include <optional>

namespace ekizu {
/**
 * @brief This structure represents an attachment to a message.
 *
 * @see https://discord.com/developers/docs/resources/channel#attachment-object
 */
struct Attachment {
	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<std::string>))
				  CompletionToken>
	[[nodiscard]] Result<std::string> download(boost::asio::any_io_executor ex,
											   CompletionToken &&token) const {
		EKIZU_TRY(auto res, net::HttpConnection::get(ex, url, token));
		return res.body();
	}

	/// The id of the attachment.
	Snowflake id;
	/// The filename of the attachment.
	std::string filename;
	/// The description of the attachment.
	std::string description;
	/// The content type of the attachment (e.g. image/png).
	std::string content_type;
	/// The size of the attachment in bytes.
	uint64_t size;
	/// The url of the attachment.
	std::string url;
	/// The proxy url of the attachment.
	std::string proxy_url;
	/// The height of the attachment (if image).
	uint16_t height;
	/// The width of the attachment (if image).
	uint16_t width;
	/// Whether the attachment is ephemeral.
	bool ephemeral{};
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Attachment &a);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Attachment &a);

/**
 * @brief Represents a partial attachment.
 */
struct PartialAttachment {
	/// ID of the attachment.
	std::optional<Snowflake> id;
	/// Name of the file attached.
	std::optional<std::string> filename;
	/// Description for the file (max 1024 characters).
	std::optional<std::string> description;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const PartialAttachment &a);
EKIZU_EXPORT void from_json(const nlohmann::json &j, PartialAttachment &a);
}  // namespace ekizu

#endif	// EKIZU_ATTACHMENT_HPP
