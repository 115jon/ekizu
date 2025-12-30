#ifndef EKIZU_INTERACTION_CONTEXT_TYPE_HPP
#define EKIZU_INTERACTION_CONTEXT_TYPE_HPP

#include <cstdint>

namespace ekizu {

/**
 * @brief Interaction context where the command can be used.
 * @see
 * https://discord.com/developers/docs/interactions/application-commands#interaction-contexts
 */
enum class InteractionContextType : uint8_t {
	Guild = 0,
	BotDm = 1,
	PrivateChannel = 2,
};

}  // namespace ekizu

#endif	// EKIZU_INTERACTION_CONTEXT_TYPE_HPP
