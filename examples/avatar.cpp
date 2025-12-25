#include <boost/beast/core/detail/base64.hpp>
#include <ekizu/async_main.hpp>
#include <ekizu/http_client.hpp>
#include <ekizu/shard.hpp>
#include <fstream>

using namespace ekizu;

// Do not recommend using global variables.
std::string avatar_filename;
std::string avatar_url;
std::string bot_token;

std::string base64_encode(std::string_view str) {
	std::string ret;
	ret.resize(boost::beast::detail::base64::encoded_size(str.size()));
	boost::beast::detail::base64::encode(ret.data(), str.data(), str.size());
	return ret;
}

std::string_view mime_type(std::string_view path) {
	using boost::beast::iequals;
	const auto ext = [&path] {
		auto const pos = path.rfind('.');
		if (pos == std::string_view::npos) { return std::string_view{}; }
		return path.substr(pos);
	}();
	if (iequals(ext, ".png")) { return "image/png"; }
	if (iequals(ext, ".jpe")) { return "image/jpeg"; }
	if (iequals(ext, ".jpeg")) { return "image/jpeg"; }
	if (iequals(ext, ".jpg")) { return "image/jpeg"; }
	if (iequals(ext, ".gif")) { return "image/gif"; }
	if (iequals(ext, ".bmp")) { return "image/bmp"; }
	if (iequals(ext, ".ico")) { return "image/vnd.microsoft.icon"; }
	if (iequals(ext, ".tiff")) { return "image/tiff"; }
	if (iequals(ext, ".tif")) { return "image/tiff"; }
	if (iequals(ext, ".svg")) { return "image/svg+xml"; }
	if (iequals(ext, ".svgz")) { return "image/svg+xml"; }
	return "application/text";
}

struct Flags {
	Result<> init(std::vector<std::string_view> args) {
		program_name = args[0];

		if (args.size() == 1) { return print_usage(); }

		for (size_t i = 1; i < args.size(); ++i) {
			std::string_view arg = args[i];

			if (arg == "-f") {
				if (++i < args.size()) {
					avatar_filename = args[i];
					continue;
				}

				return print_usage();
			}

			if (arg == "-t") {
				if (++i < args.size()) {
					bot_token = args[i];
					continue;
				}

				return print_usage();
			}

			if (arg == "-u") {
				if (++i < args.size()) {
					avatar_url = args[i];
					continue;
				}

				return print_usage();
			}

			return print_usage();
		}

		if ((avatar_filename.empty() && avatar_url.empty()) ||
			bot_token.empty()) {
			return print_usage();
		}

		return outcome::success();
	}

	Result<> print_usage() const {
		fmt::println("Usage of {}:", program_name);
		fmt::println("-f string\tAvatar File Name");
		fmt::println("-t string\tBot Token");
		fmt::println("-u string\tURL to the avatar image");
		return boost::system::errc::invalid_argument;
	}

	std::string_view program_name;
};

async_main_with_args(int argc, char *argv[], const asio::yield_context &yield) {
	EKIZU_TRY(Flags{}.init({argv, argv + argc}));

	HttpClient http{yield.get_executor(), bot_token};
	std::string base64img;
	std::string content_type;

	if (!avatar_url.empty()) {
		EKIZU_TRY(auto res, net::HttpConnection::get(
								yield.get_executor(), avatar_url, yield));
		auto img = res.body();
		content_type = res.base()["content-type"];
		base64img = base64_encode(img);
	}

	if (!avatar_filename.empty()) {
		std::ifstream file(avatar_filename, std::ios::in | std::ios::binary);
		if (!file) {
			return outcome::failure(
				std::make_error_code(std::errc::no_such_file_or_directory));
		}

		base64img =
			base64_encode(std::string((std::istreambuf_iterator<char>(file)),
									  std::istreambuf_iterator<char>()));
		content_type = mime_type(avatar_filename);
	}

	EKIZU_TRY(
		auto user,
		http.modify_current_user()
			.avatar(fmt::format("data:{};base64,{}", content_type, base64img))
			.send(yield));

	fmt::println("Avatar URL: {}", user.display_avatar_url());
	return outcome::success();
}