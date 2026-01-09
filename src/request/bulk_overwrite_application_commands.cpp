#include <fmt/format.h>

#include <ekizu/json_util.hpp>
#include <ekizu/request/bulk_overwrite_application_commands.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

// Helper to serialize variant<int64_t, double> to JSON
static void serialize_numeric_variant(
	nlohmann::json &j, std::string_view key,
	const std::optional<std::variant<int64_t, double>> &value) {
	if (!value) { return; }
	std::visit([&j, key](auto &&v) { j[std::string{key}] = v; }, *value);
}

void to_json(nlohmann::json &j, const ApplicationCommandOption &opt) {
	j["type"] = static_cast<uint8_t>(opt.type);
	j["name"] = opt.name;
	j["description"] = opt.description;

	// Only serialize if true (Discord defaults to false)
	if (opt.required) { j["required"] = opt.required; }
	if (opt.autocomplete) { j["autocomplete"] = opt.autocomplete; }

	serialize(j, "name_localizations", opt.name_localizations);
	serialize(j, "description_localizations", opt.description_localizations);
	serialize(j, "choices", opt.choices);
	serialize(j, "options", opt.options);
	serialize(j, "channel_types", opt.channel_types);

	// Handle variant types specially
	serialize_numeric_variant(j, "min_value", opt.min_value);
	serialize_numeric_variant(j, "max_value", opt.max_value);

	serialize(j, "min_length", opt.min_length);
	serialize(j, "max_length", opt.max_length);
}

void from_json(const nlohmann::json &j, ApplicationCommandOption &opt) {
	if (j.contains("type")) {
		opt.type =
			static_cast<ApplicationCommandOptionType>(j["type"].get<uint8_t>());
	}
	deserialize(j, "name", opt.name);
	deserialize(j, "description", opt.description);
	deserialize(j, "required", opt.required);
	deserialize(j, "autocomplete", opt.autocomplete);
	deserialize(j, "name_localizations", opt.name_localizations);
	deserialize(j, "description_localizations", opt.description_localizations);
	deserialize(j, "choices", opt.choices);
	deserialize(j, "options", opt.options);
}

void to_json(nlohmann::json &j, const ApplicationCommandCreateFields &f) {
	j["name"] = f.name;
	j["type"] = static_cast<uint8_t>(f.type);

	// Description is REQUIRED for CHAT_INPUT commands
	if (f.description) {
		j["description"] = *f.description;
	} else {
		j["description"] = "No description provided";
	}

	// Only serialize nsfw if true
	if (f.nsfw) { j["nsfw"] = f.nsfw; }

	serialize(j, "name_localizations", f.name_localizations);
	serialize(j, "description_localizations", f.description_localizations);
	serialize(j, "options", f.options);
	serialize(j, "default_member_permissions", f.default_member_permissions);
	serialize(j, "integration_types", f.integration_types);
	serialize(j, "contexts", f.contexts);
}

void from_json(const nlohmann::json &j, ApplicationCommandCreateFields &f) {
	deserialize(j, "name", f.name);
	if (j.contains("type")) {
		f.type = static_cast<ApplicationCommandType>(j["type"].get<uint8_t>());
	}
	deserialize(j, "nsfw", f.nsfw);
	deserialize(j, "description", f.description);
	deserialize(j, "options", f.options);
}

void to_json(nlohmann::json &j, const ApplicationCommand &cmd) {
	serialize(j, "id", cmd.id);
	j["type"] = static_cast<uint8_t>(cmd.type);
	serialize(j, "application_id", cmd.application_id);
	j["name"] = cmd.name;
	j["description"] = cmd.description;
	serialize(j, "version", cmd.version);
	if (cmd.nsfw) { j["nsfw"] = cmd.nsfw; }

	serialize(j, "guild_id", cmd.guild_id);
	serialize(j, "options", cmd.options);
	serialize(j, "default_member_permissions", cmd.default_member_permissions);
}

void from_json(const nlohmann::json &j, ApplicationCommand &cmd) {
	deserialize(j, "id", cmd.id);
	if (j.contains("type")) {
		cmd.type =
			static_cast<ApplicationCommandType>(j["type"].get<uint8_t>());
	}
	deserialize(j, "application_id", cmd.application_id);
	deserialize(j, "name", cmd.name);
	deserialize(j, "description", cmd.description);
	deserialize(j, "version", cmd.version);
	deserialize(j, "nsfw", cmd.nsfw);
	deserialize(j, "guild_id", cmd.guild_id);
	deserialize(j, "options", cmd.options);
	deserialize(
		j, "default_member_permissions", cmd.default_member_permissions);
}

BulkOverwriteGlobalApplicationCommands::BulkOverwriteGlobalApplicationCommands(
	RequestSender sender, Snowflake application_id,
	std::vector<ApplicationCommandCreateFields> commands)
	: m_sender{sender},
	  m_application_id{application_id},
	  m_commands{std::move(commands)} {}

BulkOverwriteGlobalApplicationCommands::operator net::HttpRequest() const {
	nlohmann::json body = m_commands;

	net::HttpRequest req{
		net::HttpMethod::put,
		fmt::format("/applications/{}/commands", m_application_id), 11,
		body.dump()};
	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();
	return req;
}

BulkOverwriteGuildApplicationCommands::BulkOverwriteGuildApplicationCommands(
	RequestSender sender, Snowflake application_id, Snowflake guild_id,
	std::vector<ApplicationCommandCreateFields> commands)
	: m_sender{sender},
	  m_application_id{application_id},
	  m_guild_id{guild_id},
	  m_commands{std::move(commands)} {}

BulkOverwriteGuildApplicationCommands::operator net::HttpRequest() const {
	nlohmann::json body = m_commands;

	net::HttpRequest req{net::HttpMethod::put,
						 fmt::format("/applications/{}/guilds/{}/commands",
									 m_application_id, m_guild_id),
						 11, body.dump()};
	req.set(net::http::field::content_type, "application/json");
	req.prepare_payload();
	return req;
}

}  // namespace ekizu
