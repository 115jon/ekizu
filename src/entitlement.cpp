#include <ekizu/entitlement.hpp>
#include <ekizu/json_util.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const Entitlement &e) {
	serialize(j, "id", e.id);
	serialize(j, "sku_id", e.sku_id);
	serialize(j, "application_id", e.application_id);
	serialize(j, "user_id", e.user_id);
	serialize(j, "type", e.type);
	serialize(j, "deleted", e.deleted);
	serialize(j, "starts_at", e.starts_at);
	serialize(j, "ends_at", e.ends_at);
	serialize(j, "guild_id", e.guild_id);
	serialize(j, "consumed", e.consumed);

	serialize(j, "promotion_id", e.promotion_id);
	serialize(j, "gift_code_flags", e.gift_code_flags);
	serialize(j, "subscription_id", e.subscription_id);
}

void from_json(const nlohmann::json &j, Entitlement &e) {
	deserialize(j, "id", e.id);
	deserialize(j, "sku_id", e.sku_id);
	deserialize(j, "application_id", e.application_id);
	deserialize(j, "user_id", e.user_id);
	deserialize(j, "type", e.type);
	deserialize(j, "deleted", e.deleted);
	deserialize(j, "starts_at", e.starts_at);
	deserialize(j, "ends_at", e.ends_at);
	deserialize(j, "guild_id", e.guild_id);
	deserialize(j, "consumed", e.consumed);

	deserialize(j, "promotion_id", e.promotion_id);
	deserialize(j, "gift_code_flags", e.gift_code_flags);
	deserialize(j, "subscription_id", e.subscription_id);
}
}  // namespace ekizu
