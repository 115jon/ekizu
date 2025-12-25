#include <boost/asio/experimental/channel.hpp>
#include <boost/endian/conversion.hpp>
#include <ekizu/async_main.hpp>
#include <ekizu/http_client.hpp>
#include <ekizu/lru_cache.hpp>
#include <ekizu/shard.hpp>
#include <ekizu/voice_connection.hpp>

using namespace ekizu;

using VoiceStates =
	ekizu::SnowflakeLruCache<ekizu::SnowflakeLruCache<ekizu::VoiceState>>;

// I do not recommend global variables, but this is an example.

// The buffers found in the airhorn.dca file.
std::vector<std::vector<std::byte>> samples;
// Channel for use in waiting for the voice connection to be ready.
std::optional<asio::experimental::channel<void(
	boost::system::error_code, const VoiceConnectionConfig *)>>
	channel;

// State variables one would normally track when running a bot.
// Voice States, Voice Configs, Voice Connections, etc.
VoiceStates voice_states{10};
std::unordered_map<Snowflake, VoiceConnectionConfig> voice_configs{10};
ekizu::SnowflakeLruCache<VoiceConnection> voice_conns{10};

Result<> handle_event(const Event &ev, HttpClient &http, Shard &shard,
					  const asio::yield_context &yield);
Result<> start_voice_connection(const VoiceConnectionConfig &voice_conn_config,
								const asio::yield_context &yield, Shard &shard);

Result<> load_sound() {
	boost::system::error_code ec;
	boost::beast::file file;

	file.open("airhorn.dca", boost::beast::file_mode::read, ec);
	if (ec) {
		fmt::println(
			stderr, "Failed to open airhorn.dca file: {}", ec.message());
		return ec;
	}

	int16_t opuslen{};

	while (true) {
		auto read = file.read(&opuslen, sizeof(opuslen), ec);
		if (ec) {
			fmt::println(stderr, "Failed to read file: {}", ec.message());
			return ec;
		}
		// EOF
		if (read == 0) { break; }

		boost::endian::little_to_native_inplace(opuslen);
		std::vector<std::byte> buffer(static_cast<size_t>(opuslen));

		read = file.read(buffer.data(), buffer.size(), ec);
		if (ec) {
			fmt::println(stderr, "Failed to read file: {}", ec.message());
			return ec;
		}
		// EOF
		if (read == 0) { break; }

		samples.emplace_back(std::move(buffer));
	}

	fmt::println("Loaded {} samples from airhorn.dca!", samples.size());
	return outcome::success();
}

async_main(const asio::yield_context &yield) {
	const std::string token{std::getenv("DISCORD_TOKEN")};
	HttpClient http{yield.get_executor(), token};
	Shard shard{yield.get_executor(), ShardId::ONE, token, Intents::AllIntents};

	EKIZU_TRY(load_sound());

	while (true) {
		auto res = shard.next_event(yield);
		if (!res) {
			if (res.error().failed()) {
				fmt::println(stderr, "Failed to get next event: {}",
							 res.error().message());
				return res.error();
			}
			// Could be handling a non-dispatch event.
			continue;
		}

		asio::spawn(
			yield,
			[e = std::move(res.value()), &http, &shard](auto y) {
				auto res = handle_event(e, http, shard, y);
				if (!res) {
					fmt::println(stderr, "Failed to handle event: {}",
								 res.error().message());
				}
			},
			asio::detached);
	}

	return outcome::success();
}

Result<> handle_event(const Event &ev, HttpClient &http, Shard &shard,
					  const asio::yield_context &yield) {
	if (!channel) { channel.emplace(yield.get_executor()); }

	return std::visit(
		[&](const auto &event) -> Result<> {
			using T = std::decay_t<decltype(event)>;

			if constexpr (std::is_same_v<T, GuildCreate>) {
				const auto &[guild_a] = event;
				const Guild &guild = guild_a;

				if (!voice_states.has(guild.id)) {
					voice_states.put(
						guild.id,
						ekizu::SnowflakeLruCache<ekizu::VoiceState>{10});
				}

				for (const auto &voice_state : guild.voice_states) {
					voice_states[guild.id]->put(
						voice_state.user_id, voice_state);
				}
			} else if constexpr (std::is_same_v<T, Ready>) {
				fmt::println("{} is ready!", event.user.username);

				EKIZU_TRY(shard.update_presence(
					{
						{

						},
						{{
							">airhorn",
							ActivityType::Playing,
						}},
						Status::DoNotDisturb,
					},
					yield));
			} else if constexpr (std::is_same_v<T, VoiceStateUpdate>) {
				if (event.voice_state.guild_id) {
					voice_states[*event.voice_state.guild_id]->put(
						event.voice_state.user_id, event.voice_state);
					voice_configs[*event.voice_state.guild_id].state =
						event.voice_state;
				}
			} else if constexpr (std::is_same_v<T, VoiceServerUpdate>) {
				voice_configs[event.guild_id].endpoint = event.endpoint;
				voice_configs[event.guild_id].token = event.token;

				channel->async_send(
					boost::system::error_code{}, &voice_configs[event.guild_id],
					[](const boost::system::error_code &) {});
			} else if constexpr (std::is_same_v<T, MessageCreate>) {
				const auto &[msg_a] = event;
				const Message &msg = msg_a;

				if (msg.content != ">airhorn" || !msg.guild_id) {
					return outcome::success();
				}

				// Get the user's voice state.
				boost::optional<VoiceState &> voice_state =
					voice_states.get(*msg.guild_id).flat_map([&](auto &users) {
						return users.get(msg.author.id);
					});

				if (!voice_state) {
					EKIZU_TRY(http.create_message(msg.channel_id)
								  .content("You are not in a voice channel!")
								  .reply(msg.id)
								  .send(yield));
					return outcome::success();
				}

				EKIZU_TRY(shard.join_voice_channel(
					*msg.guild_id, *voice_state->channel_id, yield));

				const auto *config = channel->async_receive(yield);

				asio::spawn(
					yield,
					[config, &shard](auto y) {
						auto res = start_voice_connection(*config, y, shard);

						if (!res) {
							auto ec = res.error();
							fmt::println(
								stderr, "ec: {} (value={}, category={})",
								ec.message(), ec.value(), ec.category().name());

							fmt::println(
								stderr, "Failed to start voice connection: {}",
								res.error().message());
						}
					},
					asio::detached);
			}

			return outcome::success();
		},
		ev);
}

Result<> start_voice_connection(const VoiceConnectionConfig &config,
								const asio::yield_context &yield,
								Shard &shard) {
	EKIZU_TRY(auto conn, config.connect(yield.get_executor(), yield));

	// Runs/Waits until its ready to start sending/receiving audio.
	EKIZU_TRY(conn.run(yield));
	EKIZU_TRY(conn.speak(SpeakerFlag::Microphone, yield));
	for (const auto &s : samples) { EKIZU_TRY(conn.send_opus(s, yield)); }
	EKIZU_TRY(conn.flush(yield));
	EKIZU_TRY(conn.speak(SpeakerFlag::None, yield));
	EKIZU_TRY(shard.leave_voice_channel(*config.state->guild_id, yield));
	EKIZU_TRY(conn.close(yield));

	return outcome::success();
}
