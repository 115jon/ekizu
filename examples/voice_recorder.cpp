#include <boost/asio/experimental/channel.hpp>
#include <boost/endian/conversion.hpp>
#include <ekizu/async_main.hpp>
#include <ekizu/http_client.hpp>
#include <ekizu/lru_cache.hpp>
#include <ekizu/shard.hpp>
#include <ekizu/voice_connection.hpp>
#include <fstream>

using namespace ekizu;

using VoiceStates =
	ekizu::SnowflakeLruCache<ekizu::SnowflakeLruCache<ekizu::VoiceState>>;

// Voice state tracking
std::optional<asio::experimental::channel<void(
	boost::system::error_code, const VoiceConnectionConfig *)>>
	channel;

VoiceStates voice_states{10};
std::unordered_map<Snowflake, VoiceConnectionConfig> voice_configs{10};

// Recording state
std::ofstream recording_file;
int total_packets = 0;
std::unordered_map<uint32_t, int> packets_by_ssrc;
std::unordered_map<Snowflake, int> packets_by_user;

Result<> save_packet(const Packet &pkt) {
	if (pkt.opus.empty()) {
		// If DAVE is enabled and decrypt isn't ready yet, packets may arrive
		// with no opus.
		return outcome::success();
	}

	if (!recording_file.is_open()) {
		recording_file.open("recording.opus", std::ios::binary);
		if (!recording_file) { return boost::system::errc::io_error; }
		fmt::println("Recording file opened");
	}

	// Format: [SSRC:4][Seq:2][Timestamp:4][Size:4][OpusData:N]
	const uint32_t ssrc_be = boost::endian::native_to_big(pkt.ssrc);
	const uint16_t seq_be = boost::endian::native_to_big(pkt.sequence);
	const uint32_t ts_be = boost::endian::native_to_big(pkt.timestamp);
	const uint32_t size_be =
		boost::endian::native_to_big(static_cast<uint32_t>(pkt.opus.size()));

	recording_file.write(reinterpret_cast<const char *>(&ssrc_be), 4);
	recording_file.write(reinterpret_cast<const char *>(&seq_be), 2);
	recording_file.write(reinterpret_cast<const char *>(&ts_be), 4);
	recording_file.write(reinterpret_cast<const char *>(&size_be), 4);
	recording_file.write(reinterpret_cast<const char *>(pkt.opus.data()),
						 static_cast<std::streamsize>(pkt.opus.size()));

	packets_by_ssrc[pkt.ssrc]++;
	packets_by_user[pkt.user_id]++;
	total_packets++;

	if (total_packets % 50 == 0) {
		fmt::println("Recorded {} packets", total_packets);
	}

	return outcome::success();
}

Result<> start_recording(VoiceConnection conn, const asio::yield_context &yield,
						 Shard &shard, Snowflake guild_id) {
	conn.attach_logger([](const ekizu::Log &log) {
		fmt::println("{}", log.message);
	});

	EKIZU_TRY(conn.run(yield));

	fmt::println("Recording started; speak in the voice channel...");
	fmt::println("Recording for 30 seconds (or until you leave)");

	for (int i = 0; i < 1500; ++i) {
		boost::system::error_code ec;
		auto pkt = conn.receive_packet(yield[ec]);
		if (ec) {
			if (ec == asio::error::operation_aborted) { break; }
			fmt::println(stderr, "Receive error: {}", ec.message());
			break;
		}

		EKIZU_TRY(save_packet(pkt));
	}

	recording_file.close();

	fmt::println("\nRecording complete!");
	fmt::println("Total packets: {}", total_packets);
	fmt::println("Unique speakers: {}", packets_by_ssrc.size());
	for (const auto &[ssrc, count] : packets_by_ssrc) {
		fmt::println(" - SSRC {}: {} packets", ssrc, count);
	}
	for (const auto &[user, count] : packets_by_user) {
		fmt::println(" - User {}: {} packets", user, count);
	}

	EKIZU_TRY(shard.leave_voice_channel(guild_id, yield));
	EKIZU_TRY(conn.close(yield));
	return outcome::success();
}

Result<> handle_event(const Event &ev, HttpClient &http, Shard &shard,
					  const asio::yield_context &yield);

async_main(const asio::yield_context &yield) {
	const std::string token{std::getenv("DISCORD_TOKEN")};
	HttpClient http{yield.get_executor(), token};
	Shard shard{yield.get_executor(), ShardId::ONE, token, Intents::AllIntents};

	fmt::println("Voice Recorder Bot Starting...\n");

	while (true) {
		auto res = shard.next_event(yield);
		if (!res) {
			if (res.error().failed()) {
				fmt::println(stderr, "Failed to get next event: {}",
							 res.error().message());
				return res.error();
			}
			continue;
		}

		asio::spawn(
			yield,
			[e = std::move(res.value()), &http, &shard](auto y) {
				auto r = handle_event(e, http, shard, y);
				if (!r) {
					fmt::println(stderr, "Failed to handle event: {}",
								 r.error().message());
				}
			},
			asio::detached);
	}
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
				fmt::println("Use: >record in a text channel\n");

				EKIZU_TRY(shard.update_presence(
					{{},
					 {{">record", ActivityType::Listening}},
					 Status::Online,
					 {}},
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

				if (msg.content != ">record" || !msg.guild_id) {
					return outcome::success();
				}

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

				EKIZU_TRY(http.create_message(msg.channel_id)
							  .content("Joining voice channel to record...")
							  .reply(msg.id)
							  .send(yield));

				EKIZU_TRY(shard.join_voice_channel(
					*msg.guild_id, *voice_state->channel_id, yield));

				const auto *config = channel->async_receive(yield);

				asio::spawn(
					yield,
					[config, &shard, guild_id = *msg.guild_id](auto y) {
						auto conn_res = config->connect(y.get_executor(), y);
						if (!conn_res) {
							fmt::println(stderr, "Voice connect failed: {}",
										 conn_res.error().message());
							return;
						}

						auto r = start_recording(
							std::move(conn_res.value()), y, shard, guild_id);
						if (!r) {
							fmt::println(stderr, "Recording failed: {}",
										 r.error().message());
						}
					},
					asio::detached);
			}

			return outcome::success();
		},
		ev);
}
