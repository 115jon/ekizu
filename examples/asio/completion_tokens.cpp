#include <boost/asio/steady_timer.hpp>
#include <ekizu/async_main.hpp>
#include <ekizu/http_client.hpp>

using namespace ekizu;

async_main(const asio::yield_context &yield) {
	const std::string token{std::getenv("DISCORD_TOKEN")};
	HttpClient http{yield.get_executor(), token};

	fmt::println("Testing if CreateDM supports completion tokens...\n");

	// Get user first
	auto user_result = http.get_current_user().send(yield);
	if (!user_result) {
		fmt::println("Failed to get user: {}", user_result.error().message());
		return user_result.error();
	}

	fmt::println("Got user: {}\n", user_result.value().username);

	// Test 1: yield_context (should work with both old and new)
	fmt::println("Test 1: CreateDM with yield_context...");
	auto dm1 = http.create_dm(Snowflake{155780111197536256}).send(yield);
	if (dm1) {
		fmt::println("  ✓ yield_context works: DM ID {}", dm1.value().id);
	} else {
		fmt::println("  ✗ yield_context failed: {}", dm1.error().message());
	}

	// Test 2: callback (ONLY works with updated CreateDM)
	fmt::println("\nTest 2: CreateDM with callback...");
	bool callback_done = false;
	bool callback_success = false;

	http.create_dm(Snowflake{155780111197536256})
		.send([&callback_done, &callback_success](Result<Channel> result) {
			callback_done = true;
			if (result) {
				callback_success = true;
				fmt::println("  ✓ callback works: DM ID {}", result.value().id);
			} else {
				fmt::println(
					"  ✗ callback failed: {}", result.error().message());
			}
		});

	// Wait for callback
	asio::steady_timer t{yield.get_executor()};
	t.expires_after(std::chrono::seconds(2));
	t.async_wait(yield);

	if (!callback_done) {
		fmt::println("  ✗ callback never invoked!");
		fmt::println(
			"\n=== CreateDM does NOT support completion tokens yet ===");
		fmt::println("You need to update CreateDM using the template!");
		return boost::system::errc::operation_not_supported;
	}

	if (callback_success) {
		fmt::println("\n=== CreateDM supports completion tokens! ✓ ===");
	} else {
		fmt::println("\n=== CreateDM callback invoked but failed ===");
	}

	return outcome::success();
}