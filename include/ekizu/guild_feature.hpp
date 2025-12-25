#ifndef EKIZU_GUILD_FEATURE_HPP
#define EKIZU_GUILD_FEATURE_HPP

#include <ekizu/export.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ekizu {
enum class GuildFeature : uint8_t {
	/// Has access to set an animated guild banner image.
	AnimatedBanner,
	/// Has access to set an animated guild icon.
	AnimatedIcon,
	/// Has set up auto moderation rules.
	AutoModeration,
	/// Has access to set a guild banner image.
	Banner,
	/// Has access to use commerce features (create store channels).
	Commerce,
	/// Can enable welcome screen, membership screening, stage channels,
	/// discovery, and receives community updates.
	Community,
	/// Guild has enabled monetization.
	CreatorMonetizableProvisional,
	/// Guild has enabled the role subscription promotional page.
	CreatorStorePage,
	/// Guild has been set as a support server on the App Directory.
	DeveloperSupportServer,
	/// Is able to be discovered in the directory.
	Discoverable,
	/// Is able to be featured in the directory.
	Featurable,
	/// Invites have been paused, this prevents new users from joining.
	InvitesDisabled,
	/// Has access to set an invite splash background.
	InviteSplash,
	/// Has enabled membership screening.
	MemberVerificationGateEnabled,
	/// Has enabled monetization.
	MonetizationEnabled,
	/// Has increased custom sticker slots.
	MoreStickers,
	/// Has access to create news channels.
	News,
	/// Is partnered.
	Partnered,
	/// Can be previewed before joining via membership screening or the
	/// directory.
	PreviewEnabled,
	/// Has access to create private threads.
	PrivateThreads,
	/// Is able to set role icons.
	RoleIcons,
	/// Guild has role subscriptions that can be purchased.
	RoleSubscriptionsAvailableForPurchase,
	/// Guild has enabled role subscriptions.
	RoleSubscriptionsEnabled,
	/// Has enabled ticketed events.
	TicketedEventsEnabled,
	/// Has access to set a vanity URL.
	VanityUrl,
	/// Is verified.
	Verified,
	/// Has access to set 384kps bitrate in voice (previously VIP voice
	/// servers).
	VipRegions,
	/// Has enabled the welcome screen.
	WelcomeScreenEnabled,
	/// Variant value is unknown to the library.
	Unknown
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const GuildFeature &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j, GuildFeature &f);
}  // namespace ekizu

#endif	// EKIZU_GUILD_FEATURE_HPP
