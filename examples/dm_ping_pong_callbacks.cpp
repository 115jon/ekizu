#include <ekizu/http_client.hpp>
#include <ekizu/shard.hpp>

using namespace ekizu;

// Example using callbacks instead of coroutines
// This demonstrates the completion token flexibility

void handle_event(const Event &ev, HttpClient &http);

void handle_message(const Message &msg, HttpClient &http);

void next_event_loop(Shard &shard, HttpClient &http);

int main() {
	const std::string token{std::getenv("DISCORD_TOKEN")};

	asio::io_context ctx;

	// NEW: HttpClient now requires executor
	HttpClient http{ctx.get_executor(), token};
	Shard shard{ctx.get_executor(), ShardId::ONE, token, Intents::AllIntents};

	// Start the event loop using callbacks
	next_event_loop(shard, http);

	ctx.run();
	return 0;
}

void next_event_loop(Shard &shard, HttpClient &http) {
	// Using callback completion token instead of yield_context!
	shard.next_event([&shard, &http](Result<Event> result) {
		if (!result) {
			if (result.error().failed()) {
				fmt::println(
					"Failed to get next event: {}", result.error().message());
				return;
			}
			// Not a dispatch event, continue
			next_event_loop(shard, http);
			return;
		}

		// Handle the event
		handle_event(result.value(), http);

		// Continue the loop
		next_event_loop(shard, http);
	});
}

void handle_event(const Event &ev, HttpClient &http) {
	std::visit(
		[&](auto &&event) {
			using T = std::decay_t<decltype(event)>;
			if constexpr (std::is_same_v<T, MessageCreate>) {
				const auto &[msg_a] = event;
				handle_message(msg_a, http);
			}
		},
		ev);
}

void handle_message(const Message &msg, HttpClient &http) {
	if (msg.content != "ping") { return; }

	fmt::println("Received ping from {}", msg.author.username);

	// Using callbacks for HTTP operations too!
	// The .send() method now accepts any completion token
	http.create_dm(msg.author.id).send([&http, msg](Result<Channel> dm_result) {
		if (!dm_result) {
			fmt::println(
				"Failed to create DM: {}", dm_result.error().message());
			return;
		}

		// Nested callback for sending the message
		http.create_message(dm_result.value().id)
			.content("pong")
			.send([](Result<Message> send_result) {
				if (!send_result) {
					fmt::println("Failed to send pong: {}",
								 send_result.error().message());
					return;
				}
				fmt::println("Sent pong!");
			});
	});
}