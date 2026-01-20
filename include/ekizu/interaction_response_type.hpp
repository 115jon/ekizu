#ifndef EKIZU_INTERACTION_RESPONSE_TYPE_HPP
#define EKIZU_INTERACTION_RESPONSE_TYPE_HPP

#include <cstdint>

namespace ekizu {
enum class InteractionResponseType : uint8_t {
	Pong = 1,
	ChannelMessageWithSource = 4,
	DeferredChannelMessageWithSource = 5,
	DeferredUpdateMessage = 6,
	UpdateMessage = 7,
	ApplicationCommandAutoCompleteResult = 8,
	Modal = 9,
	PremiumRequired = 10,  // Deprecated: use Premium Buttons instead
	LaunchActivity = 12
};
}  // namespace ekizu

#endif	// EKIZU_INTERACTION_RESPONSE_TYPE_HPP
