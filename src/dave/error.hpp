#ifndef DAVE_ERROR_HPP
#define DAVE_ERROR_HPP

/// @file error.hpp
/// @brief Custom error codes for DAVE operations

#include <boost/system/error_code.hpp>
#include <string>

namespace ekizu::dave {

/// DAVE-specific error codes
enum class DaveError : uint8_t {
	Success = 0,

	// Frame processing errors
	FrameTooSmall,
	InvalidMagicMarker,
	InvalidSupplementalBytes,
	InvalidNonce,
	InvalidUnencryptedRanges,

	// Codec errors
	InvalidNalUnit,
	InvalidObuHeader,
	UnsupportedCodec,

	// Encryption/Decryption errors
	EncryptionFailed,
	DecryptionFailed,
	RatchetNotReady,
	InvalidGeneration,
	ReplayedPacket,

	// MLS errors
	MlsError,
	InvalidWelcome,
	InvalidCommit,
	InvalidProposal,
	InvalidKeyPackage,
};

/// Error category implementation for DaveError
class DaveErrorCategory final : public boost::system::error_category {
   public:
	[[nodiscard]] const char *name() const noexcept override { return "dave"; }

	[[nodiscard]] std::string message(int ev) const override {
		switch (static_cast<DaveError>(ev)) {
			case DaveError::Success: return "Success";
			case DaveError::FrameTooSmall: return "Frame too small";
			case DaveError::InvalidMagicMarker: return "Invalid magic marker";
			case DaveError::InvalidSupplementalBytes:
				return "Invalid supplemental bytes";
			case DaveError::InvalidNonce: return "Invalid nonce";
			case DaveError::InvalidUnencryptedRanges:
				return "Invalid unencrypted ranges";
			case DaveError::InvalidNalUnit: return "Invalid NAL unit";
			case DaveError::InvalidObuHeader: return "Invalid OBU header";
			case DaveError::UnsupportedCodec: return "Unsupported codec";
			case DaveError::EncryptionFailed: return "Encryption failed";
			case DaveError::DecryptionFailed: return "Decryption failed";
			case DaveError::RatchetNotReady: return "Key ratchet not ready";
			case DaveError::InvalidGeneration: return "Invalid key generation";
			case DaveError::ReplayedPacket: return "Replayed packet detected";
			case DaveError::MlsError: return "MLS protocol error";
			case DaveError::InvalidWelcome: return "Invalid MLS welcome";
			case DaveError::InvalidCommit: return "Invalid MLS commit";
			case DaveError::InvalidProposal: return "Invalid MLS proposal";
			case DaveError::InvalidKeyPackage: return "Invalid MLS key package";
			default: return "Unknown DAVE error";
		}
	}
};

/// Get the singleton error category
inline const boost::system::error_category &dave_error_category() {
	static DaveErrorCategory instance;
	return instance;
}

/// Create an error_code from DaveError
inline boost::system::error_code make_error_code(DaveError e) {
	return {static_cast<int>(e), dave_error_category()};
}

}  // namespace ekizu::dave

// Register DaveError as an error code enum
namespace boost::system {
template <>
struct is_error_code_enum<ekizu::dave::DaveError> : std::true_type {};
}  // namespace boost::system

#endif	// DAVE_ERROR_HPP
