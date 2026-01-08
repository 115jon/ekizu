#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <ekizu/async_main.hpp>
#include <ekizu/http_client.hpp>
#include <ekizu/shard.hpp>
#include <nlohmann/json.hpp>

using namespace ekizu;

const std::shared_ptr<spdlog::logger> &get_logger() {
	static const auto ret = [] {
		auto logger = spdlog::stdout_color_mt("logger");
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
		spdlog::set_level(spdlog::level::debug);

		return logger;
	}();

	return ret;
}

template <typename... Func>
struct overload : Func... {
	using Func::operator()...;
};

template <typename... Func>
overload(Func...) -> overload<Func...>;

Result<> handle_event(Snowflake &bot_id, const Event &ev, HttpClient &http,
					  const asio::yield_context &yield);

async_main(const asio::yield_context &yield) {
	const std::string token{std::getenv("DISCORD_TOKEN")};

	if (token.empty()) {
		get_logger()->error("No token :(");
		return boost::system::errc::invalid_argument;
	}

	Snowflake bot_id;
	HttpClient http{yield.get_executor(), token};
	Shard shard{yield.get_executor(), ShardId::ONE, token, Intents::AllIntents};

	// Attach logger to shard.
	shard.attach_logger([](const Log &log) {
		const auto &msg = log.message;

		switch (log.level) {
			case LogLevel::Debug: return get_logger()->debug(msg);
			case LogLevel::Info: return get_logger()->info(msg);
			case LogLevel::Warn: return get_logger()->warn(msg);
			case LogLevel::Error: return get_logger()->error(msg);
			case LogLevel::Trace: return get_logger()->trace(msg);
			case LogLevel::Critical: return get_logger()->critical(msg);
		}
	});

	while (true) {
		auto res = shard.next_event(yield);

		if (!res) {
			if (res.error().failed()) {
				get_logger()->error(
					"Failed to get next event: {}", res.error().message());
				return res.error();
			}
			// Could be handling a non-dispatch event.
			continue;
		}

		asio::spawn(
			yield,
			[&bot_id, e = std::move(res.value()), &http](auto y) {
				boost::ignore_unused(handle_event(bot_id, e, http, y));
			},
			asio::detached);
	}

	return outcome::success();
}

Result<> handle_event(Snowflake &bot_id, const Event &ev, HttpClient &http,
					  const asio::yield_context &yield) {
	const auto &logger = get_logger();

	std::visit(
		overload{
			[&logger, &bot_id](const Ready &payload) {
				const auto &user = payload.user;
				logger->info("Logged in as {}", user.username);
				bot_id = user.id;
				logger->info("API version: {}", payload.v);
				logger->info("Guilds: {}", payload.guilds.size());
			},
			[&http, &yield, &bot_id](const MessageCreate &payload) {
				const auto &[msg] = payload;

				if (msg.author.id == bot_id) {
					get_logger()->info("Ignoring own message");
					return;
				}

				auto m =
					http.create_message(msg.channel_id)
						.content(fmt::format(
							"{} said: {}\nAvatar: {}", msg.author.username,
							msg.content,
							msg.author.avatar ? *msg.author.avatar : "null"))
						.send(yield);

				if (msg.content == "delete me") {
					(void)http.delete_message(msg.channel_id, msg.id)
						.send(yield);
				}
			},
			[&logger](const Log &log) { logger->debug(log.message); },
			[&logger](const auto &e) {
				logger->info("Uncaught {} event: {}", typeid(e).name(),
							 nlohmann::json{e}.dump());
			}},
		ev);

	return outcome::success();
}