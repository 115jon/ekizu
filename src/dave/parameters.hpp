#ifndef DAVE_PARAMETERS_HPP
#define DAVE_PARAMETERS_HPP

#include <mls/core_types.h>
#include <mls/crypto.h>
#include <mls/state.h>

namespace ekizu::dave::detail {

inline mlspp::CipherSuite ciphersuite_for_protocol_version(
	uint16_t /*version*/) {
	return mlspp::CipherSuite{
		mlspp::CipherSuite::ID::P256_AES128GCM_SHA256_P256};
}

inline mlspp::CipherSuite ciphersuite_for_signature_version(
	uint16_t /*version*/) {
	return mlspp::CipherSuite{
		mlspp::CipherSuite::ID::P256_AES128GCM_SHA256_P256};
}

inline mlspp::Capabilities leaf_node_capabilities_for_protocol_version(
	uint16_t version) {
	auto capabilities = mlspp::Capabilities::create_default();
	capabilities.cipher_suites = {
		ciphersuite_for_protocol_version(version).cipher_suite()};
	capabilities.credentials = {mlspp::CredentialType::basic};
	return capabilities;
}

inline mlspp::ExtensionList leaf_node_extensions_for_protocol_version(
	uint16_t /*version*/) {
	return mlspp::ExtensionList{};
}

inline mlspp::ExtensionList group_extensions_for_protocol_version(
	uint16_t /*version*/, mlspp::ExternalSender const &external_sender) {
	auto extension_list = mlspp::ExtensionList{};
	extension_list.add(mlspp::ExternalSendersExtension{{
		{external_sender.signature_key, external_sender.credential},
	}});
	return extension_list;
}

}  // namespace ekizu::dave::detail

#endif	// DAVE_PARAMETERS_HPP
