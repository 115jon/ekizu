#ifndef EKIZU_REQUEST_DETAIL_MULTIPART_FORM_DATA_HPP
#define EKIZU_REQUEST_DETAIL_MULTIPART_FORM_DATA_HPP

#include <fmt/format.h>

#include <atomic>
#include <ekizu/request/upload_attachment.hpp>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace ekizu::detail {

inline std::string make_multipart_boundary() {
	static std::atomic_uint64_t ctr{0};
	auto n = ++ctr;
	return fmt::format("----ekizu-multipart-{}", n);
}

inline std::string encode_multipart_form_data(
	std::string_view boundary, std::string_view payload_json,
	const std::vector<UploadAttachment> &files) {
	constexpr std::string_view crlf = "\r\n";

	std::string out;
	out.reserve(payload_json.size() + 1024);

	// payload_json
	fmt::format_to(
		std::back_inserter(out),
		"--{}\r\n"
		"Content-Disposition: form-data; name=\"payload_json\"\r\n"
		"Content-Type: application/json\r\n"
		"\r\n"
		"{}\r\n",
		boundary, payload_json);

	// files[n]
	for (size_t i = 0; i < files.size(); ++i) {
		const auto &f = files[i];

		fmt::format_to(
			std::back_inserter(out),
			"--{}\r\n"
			"Content-Disposition: form-data; name=\"files[{}]\"; "
			"filename=\"{}\"\r\n"
			"Content-Type: {}\r\n"
			"\r\n",
			boundary, i, f.filename,
			f.content_type ? *f.content_type : "application/octet-stream");

		out.append(f.data.data(), f.data.size());
		out.append(crlf.data(), crlf.size());
	}

	// final boundary
	fmt::format_to(std::back_inserter(out), "--{}--\r\n", boundary);

	return out;
}

}  // namespace ekizu::detail

#endif	// EKIZU_REQUEST_DETAIL_MULTIPART_FORM_DATA_HPP