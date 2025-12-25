#include <ekizu/dave_manager.hpp>

namespace ekizu {

DaveManager::DaveManager(std::string user_id) : m_user_id(std::move(user_id)) {}

void DaveManager::set_logger(std::function<void(std::string_view)> logger) {
	m_logger = std::move(logger);
}

void DaveManager::log(std::string_view msg) const {
	if (m_logger) { m_logger(msg); }
}

void DaveManager::set_external_sender(std::vector<std::byte> package) {
	// Convert std::byte to uint8_t for the underlying library
	m_external_sender_package.resize(package.size());
	std::memcpy(
		m_external_sender_package.data(), package.data(), package.size());

	if (!m_session) {
		m_session = std::make_unique<discord::dave::mls::Session>(
			nullptr, m_user_id,
			[this](std::string const &src, std::string const &msg) {
				log("MLS failure [" + src + "]: " + msg);
			});
	}

	m_session->SetExternalSender(m_external_sender_package);
	m_have_external_sender = true;
}

Result<> DaveManager::initialize_session(
	int protocol_version, uint64_t group_id,
	std::shared_ptr<mlspp::SignaturePrivateKey> sig_key) {
	if (!m_have_external_sender) {
		return boost::system::errc::operation_not_permitted;
	}

	if (!m_session) {
		m_session = std::make_unique<discord::dave::mls::Session>(
			nullptr, m_user_id,
			[this](std::string const &src, std::string const &msg) {
				log("MLS failure [" + src + "]: " + msg);
			});
		m_session->SetExternalSender(m_external_sender_package);
	}

	m_session->Init(
		static_cast<discord::dave::ProtocolVersion>(protocol_version), group_id,
		m_user_id, sig_key);
	m_mls_initialized = true;
	m_protocol_version = protocol_version;
	return outcome::success();
}

void DaveManager::reset_session() {
	if (m_session) { m_session->Reset(); }

	m_mls_initialized = false;
	m_joined_via_welcome = false;

	m_encryptor.SetPassthroughMode(true);
	m_decryptor.TransitionToPassthroughMode(true);

	if (!m_external_sender_package.empty()) {
		if (!m_session) {
			m_session = std::make_unique<discord::dave::mls::Session>(
				nullptr, m_user_id,
				[this](std::string const &src, std::string const &msg) {
					log("MLS failure [" + src + "]: " + msg);
				});
		}
		m_session->SetExternalSender(m_external_sender_package);
		m_have_external_sender = true;
	}
}

Result<std::vector<std::byte>> DaveManager::get_marshalled_key_package() {
	if (!m_session) { return boost::system::errc::operation_not_permitted; }
	try {
		auto vec = m_session->GetMarshalledKeyPackage();
		std::vector<std::byte> ret(vec.size());
		std::memcpy(ret.data(), vec.data(), vec.size());
		return ret;
	} catch (...) { return boost::system::errc::io_error; }
}

Result<std::vector<std::uint8_t>> DaveManager::process_proposals(
	boost::span<const std::byte> proposals,
	std::set<std::string> const &recognized_users) {
	if (!m_session) { return boost::system::errc::operation_not_permitted; }
	if (!m_joined_via_welcome) {
		return boost::system::errc::operation_not_permitted;
	}
	if (!m_mls_initialized) {
		return boost::system::errc::operation_not_permitted;
	}

	std::vector<uint8_t> prop_vec(proposals.size());
	std::memcpy(prop_vec.data(), proposals.data(), proposals.size());

	auto res =
		m_session->ProcessProposals(std::move(prop_vec), recognized_users);
	if (!res) { return boost::system::errc::io_error; }

	return *res;
}

bool DaveManager::process_commit(boost::span<const std::byte> commit) {
	if (!m_session) { return false; }

	std::vector<uint8_t> commit_vec(commit.size());
	std::memcpy(commit_vec.data(), commit.data(), commit.size());

	auto processed = m_session->ProcessCommit(std::move(commit_vec));
	bool failed = std::holds_alternative<discord::dave::failed_t>(processed);

	return !failed;
}

bool DaveManager::process_welcome(
	boost::span<const std::byte> welcome,
	std::set<std::string> const &recognized_users) {
	if (!m_session) { return false; }

	std::vector<uint8_t> welcome_vec(welcome.size());
	std::memcpy(welcome_vec.data(), welcome.data(), welcome.size());

	auto roster =
		m_session->ProcessWelcome(std::move(welcome_vec), recognized_users);
	if (roster) {
		m_joined_via_welcome = true;
		return true;
	}
	return false;
}

void DaveManager::assign_ssrc_to_codec(uint32_t ssrc,
									   discord::dave::Codec codec) {
	m_encryptor.AssignSsrcToCodec(ssrc, codec);
}

bool DaveManager::install_sender_ratchet() {
	if (!m_session) {
		log("Cannot install ratchet: no MLS session");
		return false;
	}

	auto ratchet = m_session->GetKeyRatchet(m_user_id);
	if (!ratchet) { return false; }

	m_encryptor.SetKeyRatchet(std::move(ratchet));
	m_encryptor.SetPassthroughMode(false);
	return true;
}

void DaveManager::set_passthrough_mode(bool enabled) {
	m_encryptor.SetPassthroughMode(enabled);
	m_decryptor.TransitionToPassthroughMode(enabled);
}

bool DaveManager::is_passthrough_mode() const noexcept {
	return m_encryptor.IsPassthroughMode();
}

bool DaveManager::has_key_ratchet() const noexcept {
	return m_encryptor.HasKeyRatchet();
}

bool DaveManager::ready_to_send() const noexcept {
	if (!is_e2ee_enabled()) { return true; }
	if (m_encryptor.IsPassthroughMode()) { return false; }
	if (!m_encryptor.HasKeyRatchet()) { return false; }
	return true;
}

Result<std::size_t> DaveManager::encrypt_frame(
	discord::dave::MediaType media_type, uint32_t ssrc,
	boost::span<const std::byte> plaintext,
	boost::span<std::byte> ciphertext_out) {
	std::size_t written = 0;
	auto u8_plain = discord::dave::MakeArrayView(
		reinterpret_cast<const uint8_t *>(plaintext.data()), plaintext.size());
	auto u8_cipher = discord::dave::MakeArrayView(
		reinterpret_cast<uint8_t *>(ciphertext_out.data()),
		ciphertext_out.size());

	int rc =
		m_encryptor.Encrypt(media_type, ssrc, u8_plain, u8_cipher, &written);

	if (rc != 0) { return boost::system::errc::io_error; }
	return written;
}

Result<std::size_t> DaveManager::decrypt_frame(
	discord::dave::MediaType media_type,
	boost::span<const std::byte> ciphertext,
	boost::span<std::byte> plaintext_out) {
	auto u8_cipher = discord::dave::MakeArrayView(
		reinterpret_cast<const uint8_t *>(ciphertext.data()),
		ciphertext.size());
	auto u8_plain = discord::dave::MakeArrayView(
		reinterpret_cast<uint8_t *>(plaintext_out.data()),
		plaintext_out.size());

	// Decrypt signature varies, assuming it returns size_t of written bytes or
	// 0 on failure
	size_t res = m_decryptor.Decrypt(media_type, u8_cipher, u8_plain);
	if (res == 0 && ciphertext.size() > 0) {
		return boost::system::errc::io_error;
	}
	return res;
}

std::size_t DaveManager::max_ciphertext_size(
	discord::dave::MediaType media_type, std::size_t plaintext_size) {
	return m_encryptor.GetMaxCiphertextByteSize(media_type, plaintext_size);
}

std::shared_ptr<mlspp::SignaturePrivateKey> DaveManager::get_or_create_sig_key(
	int protocol_version) {
	if (m_cached_sig_key) { return m_cached_sig_key; }

	constexpr const char *kKeyCtx = "ekizu";

	auto key = discord::dave::mls::GetPersistedKeyPair(
		kKeyCtx, m_user_id,
		static_cast<discord::dave::ProtocolVersion>(protocol_version));

	if (key) {
		m_cached_sig_key = key;
		return key;
	}

	if (protocol_version == 1) {
		auto suite = mlspp::CipherSuite(
			mlspp::CipherSuite::ID::P256_AES128GCM_SHA256_P256);
		m_cached_sig_key = std::make_shared<mlspp::SignaturePrivateKey>(
			mlspp::SignaturePrivateKey::generate(suite));
		return m_cached_sig_key;
	}

	return nullptr;
}

bool DaveManager::commit_pending_group() {
	if (!m_session) { return false; }
	return m_session->ActivatePendingGroup();
}

}  // namespace ekizu