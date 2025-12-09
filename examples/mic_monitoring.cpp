#include <portaudio.h>	// for mic recording

#include <boost/asio/experimental/concurrent_channel.hpp>
#include <ekizu/async_main.hpp>
#include <ekizu/http_client.hpp>
#include <ekizu/lru_cache.hpp>
#include <ekizu/shard.hpp>
#include <ekizu/voice_connection.hpp>

using namespace ekizu;

using VoiceStates =
	ekizu::SnowflakeLruCache<ekizu::SnowflakeLruCache<ekizu::VoiceState>>;

// I do not recommend global variables, but this is an example.

// Channel for use in waiting for the voice connection to be ready.
std::optional<asio::experimental::channel<void(
	boost::system::error_code, const VoiceConnectionConfig *)>>
	channel;

// State variables one would normally track when running a bot.
// Voice States, Voice Configs, Voice Connections, etc.
VoiceStates voice_states{10};
std::unordered_map<Snowflake, VoiceConnectionConfig> voice_configs{10};
ekizu::SnowflakeLruCache<VoiceConnection> voice_conns{10};

Result<> handle_event(const Event &ev, const HttpClient &http, Shard &shard,
					  const asio::yield_context &yield);
Result<> start_voice_connection(const VoiceConnectionConfig &voice_conn_config,
								const asio::yield_context &yield);

static constexpr uint8_t CHANNEL_COUNT = 2;		// required by Discord
static constexpr uint32_t SAMPLE_RATE = 48000;	// required by Discord

static constexpr uint16_t FRAME_COUNT = 960;
// since we are sending int16_t samples.
static constexpr uint8_t SAMPLE_SIZE = 2;
static constexpr uint16_t FRAME_SIZE =
	FRAME_COUNT * CHANNEL_COUNT * SAMPLE_SIZE;

struct PortAudioData {
	boost::asio::experimental::concurrent_channel<void(
		boost::system::error_code, std::array<std::byte, FRAME_SIZE>)>
		done;
};

static int pa_callback(
	const void *in, [[maybe_unused]] void *out,
	[[maybe_unused]] unsigned long frames_per_buffer,
	[[maybe_unused]] const PaStreamCallbackTimeInfo *time_info,
	[[maybe_unused]] PaStreamCallbackFlags status_flags, void *user_data) {
	auto &data = *static_cast<PortAudioData *>(user_data);
	const auto *input = reinterpret_cast<const std::byte *>(in);

	std::array<std::byte, FRAME_SIZE> buf{};
	std::copy_n(input, buf.size(), buf.data());

	// Notify our done channel that there's new data.
	data.done.async_send(
		boost::system::error_code{}, buf,
		[]([[maybe_unused]] const boost::system::error_code &) {});
	return paContinue;
}

async_main(const asio::yield_context &yield) {
	const std::string token{std::getenv("DISCORD_TOKEN")};
	HttpClient http{token};
	Shard shard{yield.get_executor(), ShardId::ONE, token, Intents::AllIntents};

	shard.attach_logger([](const Log &log) {
		fmt::println("{}", log.message);
	});

	while (true) {
		auto res = shard.next_event(yield);

		if (!res) {
			if (res.error().failed()) {
				fmt::println(
					"Failed to get next event: {}", res.error().message());
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
					fmt::println(
						"Failed to handle event: {}", res.error().message());
				}
			},
			asio::detached);
	}

	return outcome::success();
}

Result<> handle_event(const Event &ev, const HttpClient &http, Shard &shard,
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

				if (msg.content != "join" || !msg.guild_id) {
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
					[config, gid = *msg.guild_id, &shard](auto y) {
						auto res = start_voice_connection(*config, y);

						if (!res) {
							fmt::println("Failed to start voice connection: {}",
										 res.error().message());
						}

						(void)shard.leave_voice_channel(gid, y);
					},
					asio::detached);
			}

			return outcome::success();
		},
		ev);
}

Result<> start_voice_connection(const VoiceConnectionConfig &voice_conn_config,
								const asio::yield_context &yield) {
	// Uses the config to create a voice connection.
	EKIZU_TRY(auto conn, voice_conn_config.connect(yield));

	conn.attach_logger([](auto log) { fmt::println("{}", log.message); });

	// Starts the connection's run loop. Does not block.
	EKIZU_TRY(conn.run(yield));

	// Standard PortAudio setup for microphone. See `pa_callback` for details as
	// to what actually happens.
	if (Pa_Initialize() != paNoError) {
		fmt::println("Pa_Initialize: {}", Pa_GetErrorText(Pa_Initialize()));
		return boost::system::errc::operation_not_permitted;
	}

	PortAudioData data{{yield.get_executor()}};
	PaError err{};
	PaStream *stream{};
	auto input = Pa_GetDefaultInputDevice();

	if (input == paNoDevice) {
		fmt::println("No default input device.");
		return boost::system::errc::operation_not_permitted;
	}

	PaStreamParameters input_params{
		input, 2, paInt16, Pa_GetDeviceInfo(input)->defaultLowInputLatency,
		nullptr};

	if (err = Pa_OpenStream(&stream, &input_params, nullptr, SAMPLE_RATE,
							FRAME_COUNT, paClipOff, pa_callback, &data);
		err != paNoError) {
		fmt::println("Pa_OpenStream: {}", Pa_GetErrorText(err));
		return boost::system::errc::operation_not_permitted;
	}

	if (err = Pa_StartStream(stream); err != paNoError) {
		fmt::println("Pa_StartStream: {}", Pa_GetErrorText(err));
		return boost::system::errc::operation_not_permitted;
	}

	// Have to notify the gateway we want to speak.
	EKIZU_TRY(conn.speak(SpeakerFlag::Microphone, yield));

	// Loops forever. If you would like to incorporate some logic to stop you
	// would have to return paAbort in `pa_callback`.
	while (Pa_IsStreamActive(stream) != 0) {
		auto buf = data.done.async_receive(yield);

		asio::spawn(
			yield,
			[&conn, buf](auto y) { (void)conn.send_raw(boost::span{buf}, y); },
			asio::detached);
	}

	// Tell it we are done speaking.
	EKIZU_TRY(conn.speak(SpeakerFlag::None, yield));
	Pa_CloseStream(stream);
	EKIZU_TRY(conn.close(yield));

	return outcome::success();
}
