#include "persisted_key_pair.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

#include "parameters.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <fcntl.h>

namespace ekizu::dave {

static const std::string SELF_SIGNATURE_LABEL = "DiscordSelfSignature";
static const std::string_view KEY_STORAGE_DIR = "Discord Key Storage";

static std::mutex g_key_mutex;
static std::map<std::string, std::shared_ptr<mlspp::SignaturePrivateKey>>
	g_key_map;

static std::filesystem::path get_key_storage_directory() {
	std::filesystem::path dir;

#if defined(__ANDROID__)
	dir = std::filesystem::path("/data/data");

	{
		std::ifstream idFile("/proc/self/cmdline", std::ios_base::in);
		std::string appId;
		std::getline(idFile, appId, '\0');
		dir /= appId;
	}
#else  // __ANDROID__
#if defined(_WIN32)
	if (const wchar_t *appdata = _wgetenv(L"LOCALAPPDATA")) {
		dir = std::filesystem::path(appdata);
	}
#else	// _WIN32
	if (const char *xdg = getenv("XDG_CONFIG_HOME")) {
		dir = std::filesystem::path(xdg);
	} else if (const char *home = getenv("HOME")) {
		dir = std::filesystem::path(home);
		dir /= ".config";
	}
#endif	// !_WIN32
	else {
		return dir;
	}
#endif	// !__ANDROID__

	return dir / KEY_STORAGE_DIR;
}

static std::string make_key_id(const std::string &session_id,
							   mlspp::CipherSuite suite) {
	return fmt::format(
		"{}-{}-{}", session_id, (uint16_t)suite.cipher_suite(), KEY_VERSION);
}

static std::shared_ptr<mlspp::SignaturePrivateKey>
get_generic_persisted_key_pair(KeyPairContextType /*ctx*/,
							   const std::string &id,
							   mlspp::CipherSuite suite) {
	mlspp::SignaturePrivateKey ret;
	std::filesystem::path dir = get_key_storage_directory();

	if (dir.empty()) { return nullptr; }

	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	if (ec) { return nullptr; }

	std::filesystem::path file = dir / (id + ".key");

	if (std::filesystem::exists(file)) {
		std::ifstream ifs(file, std::ios_base::in | std::ios_base::binary);
		if (!ifs) { return nullptr; }

		std::stringstream s;
		s << ifs.rdbuf();
		std::string curstr = s.str();

		try {
			ret = mlspp::SignaturePrivateKey::from_jwk(suite, curstr);
		} catch (...) { return nullptr; }
	} else {
		try {
			ret = mlspp::SignaturePrivateKey::generate(suite);
			std::string newstr = ret.to_jwk(suite);

			std::filesystem::path tmpfile = file;
			tmpfile += ".tmp";

			{
				std::ofstream ofs(
					tmpfile, std::ios_base::out | std::ios_base::binary |
								 std::ios_base::trunc);
				if (!ofs) { return nullptr; }
				ofs << newstr;
			}

			std::filesystem::rename(tmpfile, file, ec);
			if (ec) { return nullptr; }
		} catch (...) { return nullptr; }
	}

	if (!ret.public_key.data.empty()) {
		return std::make_shared<mlspp::SignaturePrivateKey>(std::move(ret));
	}
	return nullptr;
}

std::shared_ptr<mlspp::SignaturePrivateKey> get_persisted_key_pair(
	KeyPairContextType ctx, const std::string &session_id,
	ProtocolVersion version) {
	auto suite = detail::ciphersuite_for_protocol_version(version);
	std::string id = make_key_id(session_id, suite);

	std::lock_guard lk(g_key_mutex);
	auto it = g_key_map.find(id);
	if (it != g_key_map.end()) { return it->second; }

	auto ret = get_generic_persisted_key_pair(ctx, id, suite);
	if (ret) { g_key_map.emplace(id, ret); }
	return ret;
}

KeyAndSelfSignature get_persisted_public_key(KeyPairContextType ctx,
											 const std::string &session_id,
											 SignatureVersion version) {
	auto suite = detail::ciphersuite_for_signature_version(version);
	auto pair = get_persisted_key_pair(ctx, session_id, version);

	if (!pair) { return {}; }

	auto sign_data =
		mlspp::bytes_ns::from_ascii(session_id + ":") + pair->public_key.data;

	return {pair->public_key.data.as_vec(),
			pair->sign(suite, SELF_SIGNATURE_LABEL, sign_data).as_vec()};
}

bool delete_persisted_key_pair(KeyPairContextType ctx,
							   const std::string &session_id,
							   SignatureVersion version) {
	std::string id = make_key_id(
		session_id, detail::ciphersuite_for_signature_version(version));

	std::lock_guard lk(g_key_mutex);
	g_key_map.erase(id);

	std::filesystem::path dir = get_key_storage_directory();
	if (dir.empty()) { return false; }

	std::filesystem::path file = dir / (id + ".key");
	std::error_code ec;
	return std::filesystem::remove(file, ec);
}

}  // namespace ekizu::dave
