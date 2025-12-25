#include <boost/asio/use_future.hpp>
#include <ekizu/http_client.hpp>

using namespace ekizu;

int main() {
	const std::string token{std::getenv("DISCORD_TOKEN")};

	asio::io_context ctx;

	// Work guard keeps io_context alive
	auto work = asio::make_work_guard(ctx);

	HttpClient http{ctx.get_executor(), token};

	fmt::println("=== use_future Example ===\n");

	// Run io_context in separate thread
	std::thread io_thread([&ctx]() { ctx.run(); });

	try {
		fmt::println("1. Getting current user...");
		auto user_future = http.get_current_user().send(asio::use_future);
		auto user_result = user_future.get();

		if (user_result) {
			fmt::println("   ✓ Got user: {} (ID: {})",
						 user_result.value().username, user_result.value().id);

			fmt::println("\n2. Creating DM...");
			auto dm_future = http.create_dm(Snowflake{155780111197536256})
								 .send(asio::use_future);
			auto dm_result = dm_future.get();

			if (dm_result) {
				fmt::println("   ✓ Created DM: {}", dm_result.value().id);

				fmt::println("\n3. Sending message...");
				auto msg_future = http.create_message(dm_result.value().id)
									  .content("use_future works!")
									  .send(asio::use_future);
				auto msg_result = msg_future.get();

				if (msg_result) {
					fmt::println(
						"   ✓ Sent message: {}", msg_result.value().id);
				} else {
					fmt::println(
						"   ✗ Failed: {}", msg_result.error().message());
				}
			} else {
				fmt::println("   ✗ Failed: {}", dm_result.error().message());
			}
		} else {
			fmt::println("   ✗ Failed: {}", user_result.error().message());
		}
	} catch (const std::exception &e) {
		fmt::println("Exception: {}", e.what());
	}

	fmt::println("\n=== use_future works! ===\n");

	work.reset();  // Release work guard
	ctx.stop();
	io_thread.join();

	return 0;
}