#include <ekizu/application_command_data.hpp>
#include <ekizu/json_util.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

static void option_value_to_json(nlohmann::json &j,
								 const ApplicationCommandOptionValue &v) {
	std::visit(
		[&j](const auto &alt) {
			using T = std::decay_t<decltype(alt)>;
			if constexpr (std::is_same_v<T, std::monostate>) {
				j = nullptr;
			} else {
				j = alt;
			}
		},
		v);
}

static std::optional<ApplicationCommandOptionValue> option_value_from_json(
	const nlohmann::json &j, ApplicationCommandOptionType type) {
	if (j.is_discarded() || j.is_null()) { return std::nullopt; }

	switch (type) {
		case ApplicationCommandOptionType::String:
			if (j.is_string()) {
				return ApplicationCommandOptionValue(j.get<std::string>());
			}
			break;
		case ApplicationCommandOptionType::Integer:
			if (j.is_number_integer() || j.is_number_unsigned() ||
				j.is_string()) {
				int64_t v{};
				json_util::detail::deserialize_impl<int64_t>(j, v);
				return ApplicationCommandOptionValue(v);
			}
			break;
		case ApplicationCommandOptionType::Number:
			if (j.is_number() || j.is_string()) {
				double v{};
				json_util::detail::deserialize_impl<double>(j, v);
				return ApplicationCommandOptionValue(v);
			}
			break;
		case ApplicationCommandOptionType::Boolean:
			if (j.is_boolean()) {
				return ApplicationCommandOptionValue(j.get<bool>());
			}
			break;
		case ApplicationCommandOptionType::User:
		case ApplicationCommandOptionType::Channel:
		case ApplicationCommandOptionType::Role:
		case ApplicationCommandOptionType::Mentionable:
		case ApplicationCommandOptionType::Attachment: {
			Snowflake id{};
			json_util::detail::deserialize_impl<Snowflake>(j, id);
			return ApplicationCommandOptionValue(id);
		}
		case ApplicationCommandOptionType::Subcommand:
		case ApplicationCommandOptionType::SubcommandGroup:
		case ApplicationCommandOptionType::Unknown: break;
	}

	// Fallback: try string first, then integer, then number, then bool, then
	// snowflake.
	try {
		if (j.is_string()) {
			return ApplicationCommandOptionValue(j.get<std::string>());
		}
		if (j.is_boolean()) {
			return ApplicationCommandOptionValue(j.get<bool>());
		}
		if (j.is_number_integer() || j.is_number_unsigned()) {
			return ApplicationCommandOptionValue(j.get<int64_t>());
		}
		if (j.is_number_float()) {
			return ApplicationCommandOptionValue(j.get<double>());
		}
	} catch (...) {}

	return std::nullopt;
}

void to_json(nlohmann::json &j,
			 const ApplicationCommandInteractionDataOption &c) {
	serialize(j, "name", c.name);
	serialize(j, "type", c.type);
	serialize(j, "focused", c.focused);

	if (!c.options.empty()) { serialize(j, "options", c.options); }

	if (c.value) {
		nlohmann::json v;
		option_value_to_json(v, *c.value);
		j["value"] = std::move(v);
	}
}

void from_json(const nlohmann::json &j,
			   ApplicationCommandInteractionDataOption &c) {
	deserialize(j, "name", c.name);
	deserialize(j, "type", c.type);
	deserialize(j, "focused", c.focused);
	deserialize(j, "options", c.options);

	if (json_util::not_null_all(j, "value")) {
		auto v = option_value_from_json(j["value"], c.type);
		if (v) { c.value = std::move(*v); }
	}
}

void to_json(nlohmann::json &j, const ApplicationCommandData &c) {
	serialize(j, "id", c.id);
	serialize(j, "name", c.name);
	serialize(j, "type", c.type);
	serialize(j, "resolved", c.resolved);
	serialize(j, "options", c.options);
	serialize(j, "guild_id", c.guild_id);
	serialize(j, "target_id", c.target_id);
}

void from_json(const nlohmann::json &j, ApplicationCommandData &c) {
	deserialize(j, "id", c.id);
	deserialize(j, "name", c.name);
	deserialize(j, "type", c.type);
	deserialize(j, "resolved", c.resolved);
	deserialize(j, "options", c.options);
	deserialize(j, "guild_id", c.guild_id);
	deserialize(j, "target_id", c.target_id);
}
}  // namespace ekizu
