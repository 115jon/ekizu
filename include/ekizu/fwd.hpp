/**
 * @file fwd.hpp
 * @brief Forward declarations for ekizu types
 *
 * Include this header when you only need type declarations (e.g., for
 * function parameters, return types, or pointer/reference members) without
 * needing the full type definition.
 *
 * This is useful for:
 * - Reducing compile time in headers that only use pointers/references
 * - Breaking circular dependencies
 * - Speeding up compilation for downstream consumers
 *
 * Note: For nlohmann::json forward declarations, use <nlohmann/json_fwd.hpp>
 * which is provided by the nlohmann_json library.
 */
#ifndef EKIZU_FWD_HPP
#define EKIZU_FWD_HPP

#include <cstdint>

namespace ekizu {

// Core types
struct Snowflake;
struct User;
struct CurrentUser;
struct GuildMember;
struct PartialMember;
struct Guild;
struct PartialGuild;
struct GuildPreview;
struct Channel;
struct Message;
struct Embed;
struct Emoji;
struct PartialEmoji;
struct Role;
struct Attachment;
struct PartialAttachment;
struct Sticker;
struct Invite;
struct Ban;
struct StageInstance;
struct GuildScheduledEvent;
struct VoiceState;
struct Presence;
struct Activity;

// Interaction types
struct Interaction;
struct GuildInteraction;
struct PartialApplication;
struct Entitlement;

// Application command types
struct ApplicationCommand;
struct ApplicationCommandData;
struct ApplicationCommandDataResolved;

// Message component types
struct Button;
struct SelectMenu;
struct SelectOptions;
struct TextInput;
struct ActionRow;
struct MessageComponent;

// Gateway types
struct Shard;
struct ShardId;
struct ShardManager;
struct Event;  // Note: Event is a std::variant, forward declare as struct

// HTTP/Network types
struct HttpClient;
struct RateLimiter;
struct RequestSender;
struct DiscordApiRequest;

// Voice types
struct VoiceConnection;
struct VoiceConnectionConfig;
struct Packet;
enum class VoiceOpcode : uint8_t;
enum class SpeakerFlag : uint8_t;

// Request types (forward declare common ones)
struct CreateMessage;
struct EditMessage;
struct GetChannel;
struct GetGuild;
struct GetUser;
struct CreateDM;

}  // namespace ekizu

namespace ekizu::net {

struct HttpConnection;
struct HttpRequest;
struct HttpResponse;
struct WebSocketClient;
struct WebSocketMessage;
struct UdpSocket;

}  // namespace ekizu::net

#endif	// EKIZU_FWD_HPP
