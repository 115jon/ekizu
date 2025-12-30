#ifndef EKIZU_REQUEST_UPLOAD_ATTACHMENT_HPP
#define EKIZU_REQUEST_UPLOAD_ATTACHMENT_HPP

#include <optional>
#include <string>

namespace ekizu {

/**
 * @brief Represents a file upload attachment for requests.
 *
 * This is an outgoing-only helper type used by request builders.
 */
struct UploadAttachment {
	/// Filename to report to Discord.
	std::string filename;

	/// Raw file bytes. May contain null bytes.
	std::string data;

	/// Optional content type (e.g. "image/png"). Defaults to
	/// "application/octet-stream" when absent.
	std::optional<std::string> content_type;

	/// Optional description (max 1024 characters).
	std::optional<std::string> description;
};

}  // namespace ekizu

#endif	// EKIZU_REQUEST_UPLOAD_ATTACHMENT_HPP
