#ifndef EKIZU_ENTITLEMENT_HPP
#define EKIZU_ENTITLEMENT_HPP

#include <cstdint>
#include <ekizu/export.hpp>
#include <ekizu/snowflake.hpp>
#include <optional>
#include <string>

namespace ekizu {
/**
 * @brief Types of entitlement.
 *
 * @see
 * https://discord.com/developers/docs/resources/entitlement#entitlement-types
 */
enum class EntitlementType : uint8_t {
	Purchase = 1,
	PremiumSubscription = 2,
	DeveloperGift = 3,
	TestModePurchase = 4,
	FreePurchase = 5,
	UserGift = 6,
	PremiumPurchase = 7,
	ApplicationSubscription = 8,
};

/**
 * @brief Represents an entitlement.
 *
 * Note: Discord's example payload includes fields like promotion_id,
 * gift_code_flags, and subscription_id, so they are included here as optional.
 *
 * @see
 * https://discord.com/developers/docs/resources/entitlement#entitlement-object
 */
struct Entitlement {
	/// ID of the entitlement.
	Snowflake id;
	/// ID of the SKU.
	Snowflake sku_id;
	/// ID of the parent application.
	Snowflake application_id;
	/// ID of the user that is granted access to the entitlement's sku.
	std::optional<Snowflake> user_id;
	/// ID of the guild that is granted access to the entitlement's sku.
	std::optional<Snowflake> guild_id;

	/// Type of entitlement.
	EntitlementType type{EntitlementType::Purchase};
	/// Entitlement was deleted.
	bool deleted{};

	/// Start date at which the entitlement is valid.
	std::optional<std::string> starts_at;
	/// Date at which the entitlement is no longer valid.
	std::optional<std::string> ends_at;

	/// For consumable items, whether or not the entitlement has been consumed.
	std::optional<bool> consumed;

	/// Promotion ID (present in example payloads).
	std::optional<Snowflake> promotion_id;
	/// Gift code flags (present in example payloads).
	std::optional<uint64_t> gift_code_flags;
	/// Subscription ID (present in example payloads).
	std::optional<Snowflake> subscription_id;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const Entitlement &e);
EKIZU_EXPORT void from_json(const nlohmann::json &j, Entitlement &e);
}  // namespace ekizu

#endif	// EKIZU_ENTITLEMENT_HPP
