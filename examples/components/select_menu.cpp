#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <ekizu/async_main.hpp>
#include <ekizu/embed_builder.hpp>
#include <ekizu/http_client.hpp>
#include <ekizu/shard.hpp>
#include <nlohmann/json.hpp>

using namespace ekizu;

template <typename... Func>
struct overload : Func... {
	using Func::operator()...;
};

template <typename... Func>
overload(Func...) -> overload<Func...>;

Result<> handle_event(Snowflake &bot_id, const Event &ev, HttpClient &http,
					  const asio::yield_context &yield);

async_main(const asio::yield_context &yield) {
	std::string token{std::getenv("DISCORD_TOKEN")};
	Snowflake bot_id;
	HttpClient http{yield.get_executor(), token};
	Shard shard{yield.get_executor(), ShardId::ONE, token, Intents::AllIntents};

	shard.attach_logger([](const Log &log) {
		fmt::println("{}", log.message);
	});

	while (true) {
		try {
			auto res = shard.next_event(yield);

			if (!res) {
				if (res.error().failed()) { return res.error(); }
				continue;
			}

			asio::spawn(
				yield,
				[&bot_id, e = std::move(res.value()), &http](auto y) {
					(void)handle_event(bot_id, e, http, y);
				},
				asio::detached);
		} catch (const std::exception &e) { fmt::println("{}", e.what()); }
	}

	return outcome::success();
}

const std::unordered_map<std::string_view, std::string_view> SELECT_OPTIONS{
	{"cpp", "C++"},		  {"python", "Python"}, {"rust", "Rust"},
	{"kotlin", "Kotlin"}, {"java", "Java"},		{"go", "Go"},
};

Result<> handle_event(Snowflake &bot_id, const Event &ev, HttpClient &http,
					  const asio::yield_context &yield) {
	std::visit(
		overload{
			[&bot_id](const Ready &payload) {
				const auto &user = payload.user;
				fmt::println("Logged in as {}", user.username);
				bot_id = user.id;
				fmt::println("API version: {}", payload.v);
				fmt::println("Guilds: {}", payload.guilds.size());
			},
			[&http, &yield](const InteractionCreate &payload) {
				const auto &[i] = payload;
				auto app_id = i.application_id;
				auto id = i.id;
				auto token = i.token;

				if (!i.data) { return; }

				std::visit(
					overload{[&http, &yield, app_id, id,
							  token](const MessageComponentData &data) {
								 auto response =
									 InteractionResponseBuilder()
										 .type(InteractionResponseType::
												   ChannelMessageWithSource)
										 .embeds({EmbedBuilder()
													  .set_title(fmt::format(
														  "{} **is** the best!",
														  SELECT_OPTIONS.at(
															  data.values[0])))
													  .build()})
										 .flags(MessageFlags::Ephemeral)
										 .build();

								 if (data.custom_id == "click_me") {
									 (void)http.interaction(app_id)
										 .create_response(
											 id, token, std::move(response))
										 .send(yield);
								 }
							 },
							 [](const auto &) {}},
					*i.data);
			},
			[&http, &yield, &bot_id](const MessageCreate &payload) {
				const auto &[msg] = payload;

				if (msg.author.id == bot_id) { return; }

				std::vector<SelectOptions> options;
				boost::copy(SELECT_OPTIONS |
								boost::adaptors::transformed([](const auto &p) {
									return SelectOptionsBuilder()
										.label(std::string{p.second})
										.value(std::string{p.first})
										.build();
								}),
							std::back_inserter(options));

				auto row = ActionRowBuilder()
							   .components({SelectMenuBuilder()
												.custom_id("click_me")
												.options(std::move(options))
												.build()})
							   .build();

				auto res = http.create_message(msg.channel_id)
							   .content("What is the superior language?")
							   .components({std::move(row)})
							   .reply(msg.id)
							   .send(yield);

				if (!res) {
					fmt::println(
						"Failed to send message: {}", res.error().message());
				}
			},
			[](const auto &e) {
				fmt::println("Uncaught {} event: {}", typeid(e).name(),
							 nlohmann::json{e}.dump());
			}},
		ev);

	return outcome::success();
}