#include <opus/opus.h>

#include <boost/endian/conversion.hpp>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct Args {
	std::string input_path = "recording.opus";
	std::string output_path = "out.wav";
	std::optional<std::uint32_t> ssrc;
	bool list_only = false;
	int sample_rate = 48000;
	int channels = 2;
};

std::optional<std::uint32_t> parse_u32(const std::string &s) {
	try {
		std::size_t idx = 0;
		// base=0 supports 0x... hex too
		unsigned long v = std::stoul(s, &idx, 0);
		if (idx != s.size()) { return std::nullopt; }
		if (v > std::numeric_limits<std::uint32_t>::max()) {
			return std::nullopt;
		}
		return static_cast<std::uint32_t>(v);
	} catch (...) { return std::nullopt; }
}

std::optional<int> parse_i32(const std::string &s) {
	try {
		std::size_t idx = 0;
		long v = std::stol(s, &idx, 0);
		if (idx != s.size()) { return std::nullopt; }
		if (v < std::numeric_limits<int>::min() ||
			v > std::numeric_limits<int>::max()) {
			return std::nullopt;
		}
		return static_cast<int>(v);
	} catch (...) { return std::nullopt; }
}

void print_usage(const char *argv0) {
	std::cerr
		<< "Usage:\n"
		<< "  " << argv0 << " [--input <file>] [--output <wav>]\n"
		<< "           [--ssrc <u32>] [--list] [--rate <hz>] [--channels <n>]\n"
		<< "\n"
		<< "Examples:\n"
		<< "  " << argv0 << " --list\n"
		<< "  " << argv0
		<< " --ssrc 7273 --input recording.opus --output out.wav\n";
}

std::optional<Args> parse_args(int argc, char **argv) {
	Args a;

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];

		auto require_value =
			[&](const char *name) -> std::optional<std::string> {
			if (i + 1 >= argc) {
				std::cerr << "Missing value for " << name << "\n";
				return std::nullopt;
			}
			return std::string(argv[++i]);
		};

		if (arg == "--help" || arg == "-h") {
			return std::nullopt;
		} else if (arg == "--list") {
			a.list_only = true;
		} else if (arg == "--input" || arg == "-i") {
			auto v = require_value("--input");
			if (!v) { return std::nullopt; }
			a.input_path = *v;
		} else if (arg == "--output" || arg == "-o") {
			auto v = require_value("--output");
			if (!v) { return std::nullopt; }
			a.output_path = *v;
		} else if (arg == "--ssrc") {
			auto v = require_value("--ssrc");
			if (!v) { return std::nullopt; }
			auto parsed = parse_u32(*v);
			if (!parsed) {
				std::cerr << "Invalid --ssrc value: " << *v << "\n";
				return std::nullopt;
			}
			a.ssrc = *parsed;
		} else if (arg == "--rate") {
			auto v = require_value("--rate");
			if (!v) { return std::nullopt; }
			auto parsed = parse_i32(*v);
			if (!parsed || *parsed <= 0) {
				std::cerr << "Invalid --rate value: " << *v << "\n";
				return std::nullopt;
			}
			a.sample_rate = *parsed;
		} else if (arg == "--channels") {
			auto v = require_value("--channels");
			if (!v) { return std::nullopt; }
			auto parsed = parse_i32(*v);
			if (!parsed || (*parsed != 1 && *parsed != 2)) {
				std::cerr << "Invalid --channels value (expected 1 or 2): "
						  << *v << "\n";
				return std::nullopt;
			}
			a.channels = *parsed;
		} else {
			std::cerr << "Unknown argument: " << arg << "\n";
			return std::nullopt;
		}
	}

	return a;
}

bool read_exact(std::istream &is, void *dst, std::size_t n) {
	is.read(reinterpret_cast<char *>(dst), static_cast<std::streamsize>(n));
	return static_cast<std::size_t>(is.gcount()) == n;
}

bool read_record_header(std::istream &is, std::uint32_t &ssrc,
						std::uint16_t &seq, std::uint32_t &ts,
						std::uint32_t &size) {
	std::uint32_t ssrc_be = 0;
	std::uint16_t seq_be = 0;
	std::uint32_t ts_be = 0;
	std::uint32_t size_be = 0;

	if (!read_exact(is, &ssrc_be, 4)) { return false; }
	if (!read_exact(is, &seq_be, 2)) { return false; }
	if (!read_exact(is, &ts_be, 4)) { return false; }
	if (!read_exact(is, &size_be, 4)) { return false; }

	ssrc = boost::endian::big_to_native(ssrc_be);
	seq = boost::endian::big_to_native(seq_be);
	ts = boost::endian::big_to_native(ts_be);
	size = boost::endian::big_to_native(size_be);
	return true;
}

void write_le_u16(std::ostream &os, std::uint16_t v) {
	auto le = boost::endian::native_to_little(v);
	os.write(reinterpret_cast<const char *>(&le), 2);
}

void write_le_u32(std::ostream &os, std::uint32_t v) {
	auto le = boost::endian::native_to_little(v);
	os.write(reinterpret_cast<const char *>(&le), 4);
}

bool write_wav_header_placeholder(std::ofstream &out, int sample_rate,
								  int channels,
								  std::uint32_t data_bytes_placeholder) {
	// Minimal PCM WAV header (44 bytes).
	// RIFF chunk
	out.write("RIFF", 4);
	write_le_u32(out, 36 + data_bytes_placeholder);	 // chunk size
	out.write("WAVE", 4);

	// fmt subchunk
	out.write("fmt ", 4);
	write_le_u32(out, 16);	// PCM fmt size
	write_le_u16(out, 1);	// audio format = PCM
	write_le_u16(out, (std::uint16_t)channels);
	write_le_u32(out, (std::uint32_t)sample_rate);

	const std::uint16_t bits_per_sample = 16;
	const std::uint16_t block_align =
		(std::uint16_t)(channels * (bits_per_sample / 8));
	const std::uint32_t byte_rate = (std::uint32_t)(sample_rate * block_align);

	write_le_u32(out, byte_rate);
	write_le_u16(out, block_align);
	write_le_u16(out, bits_per_sample);

	// data subchunk
	out.write("data", 4);
	write_le_u32(out, data_bytes_placeholder);

	return static_cast<bool>(out);
}

bool patch_wav_sizes(std::ofstream &out, std::uint32_t data_bytes) {
	// RIFF chunk size at offset 4
	out.seekp(4, std::ios::beg);
	if (!out) { return false; }
	write_le_u32(out, 36 + data_bytes);

	// data chunk size at offset 40
	out.seekp(40, std::ios::beg);
	if (!out) { return false; }
	write_le_u32(out, data_bytes);

	return static_cast<bool>(out);
}

}  // namespace

int main(int argc, char **argv) {
	auto parsed = parse_args(argc, argv);
	if (!parsed) {
		print_usage(argv[0]);
		return 2;
	}
	Args args = *parsed;

	// Pass 1: collect SSRCs
	std::ifstream in(args.input_path, std::ios::binary);
	if (!in) {
		std::cerr << "Failed to open input: " << args.input_path << "\n";
		return 1;
	}

	std::unordered_map<std::uint32_t, std::uint64_t> counts;
	for (;;) {
		std::uint32_t ssrc = 0, ts = 0, size = 0;
		std::uint16_t seq = 0;
		if (!read_record_header(in, ssrc, seq, ts, size)) { break; }

		++counts[ssrc];

		// Skip payload
		in.seekg(static_cast<std::streamoff>(size), std::ios::cur);
		if (!in) {
			std::cerr << "Truncated or corrupt file (failed to skip payload)\n";
			return 1;
		}
	}

	if (counts.empty()) {
		std::cerr << "No records found in " << args.input_path << "\n";
		return 1;
	}

	auto print_ssrcs = [&]() {
		std::cerr << "SSRCs found:\n";
		for (const auto &kv : counts) {
			std::cerr << "  " << kv.first << " (" << kv.second << " packets)\n";
		}
	};

	if (args.list_only) {
		print_ssrcs();
		return 0;
	}

	std::uint32_t chosen_ssrc = 0;
	if (args.ssrc) {
		chosen_ssrc = *args.ssrc;
		if (counts.find(chosen_ssrc) == counts.end()) {
			std::cerr << "Requested SSRC not found: " << chosen_ssrc << "\n";
			print_ssrcs();
			return 1;
		}
	} else {
		if (counts.size() == 1) {
			chosen_ssrc = counts.begin()->first;
		} else {
			std::cerr << "Multiple SSRCs present; pass --ssrc to choose one.\n";
			print_ssrcs();
			return 2;
		}
	}

	// Pass 2: decode chosen SSRC
	std::ifstream in2(args.input_path, std::ios::binary);
	if (!in2) {
		std::cerr << "Failed to reopen input: " << args.input_path << "\n";
		return 1;
	}

	std::ofstream out(args.output_path, std::ios::binary);
	if (!out) {
		std::cerr << "Failed to open output: " << args.output_path << "\n";
		return 1;
	}

	if (!write_wav_header_placeholder(
			out, args.sample_rate, args.channels, 0)) {
		std::cerr << "Failed to write WAV header\n";
		return 1;
	}

	int opus_err = 0;
	OpusDecoder *decoder =
		opus_decoder_create(args.sample_rate, args.channels, &opus_err);
	if (!decoder || opus_err != OPUS_OK) {
		std::cerr << "opus_decoder_create failed: " << opus_err << "\n";
		return 1;
	}

	const int max_frame_samples = 5760;	 // 120ms @ 48kHz (safe upper bound)
	std::vector<opus_int16> pcm(static_cast<std::size_t>(max_frame_samples) *
								static_cast<std::size_t>(args.channels));
	std::vector<unsigned char> opus_packet;

	std::uint32_t total_written_bytes = 0;
	std::uint64_t decoded_packets = 0;
	std::uint64_t skipped_packets = 0;
	std::uint64_t decode_errors = 0;

	for (;;) {
		std::uint32_t ssrc = 0, ts = 0, size = 0;
		std::uint16_t seq = 0;
		if (!read_record_header(in2, ssrc, seq, ts, size)) { break; }

		opus_packet.resize(size);
		if (size > 0) {
			if (!read_exact(in2, opus_packet.data(), size)) {
				std::cerr
					<< "Truncated or corrupt file (payload read failed)\n";
				opus_decoder_destroy(decoder);
				return 1;
			}
		}

		if (ssrc != chosen_ssrc) {
			++skipped_packets;
			continue;
		}

		// Empty payloads can exist; just skip them.
		if (opus_packet.empty()) { continue; }

		int frame_size = opus_decode(
			decoder, opus_packet.data(),
			static_cast<opus_int32>(opus_packet.size()), pcm.data(),
			max_frame_samples, 0);

		if (frame_size < 0) {
			++decode_errors;
			continue;
		}

		const std::size_t samples_total =
			static_cast<std::size_t>(frame_size) *
			static_cast<std::size_t>(args.channels);
		const std::size_t bytes_to_write = samples_total * sizeof(opus_int16);

		out.write(reinterpret_cast<const char *>(pcm.data()),
				  static_cast<std::streamsize>(bytes_to_write));
		if (!out) {
			std::cerr << "Write failed\n";
			opus_decoder_destroy(decoder);
			return 1;
		}

		if (total_written_bytes >
			std::numeric_limits<std::uint32_t>::max() - bytes_to_write) {
			std::cerr << "Output too large for WAV ( > 4GiB data chunk)\n";
			opus_decoder_destroy(decoder);
			return 1;
		}

		total_written_bytes += static_cast<std::uint32_t>(bytes_to_write);
		++decoded_packets;
	}

	if (!patch_wav_sizes(out, total_written_bytes)) {
		std::cerr << "Failed to finalize WAV header sizes\n";
		opus_decoder_destroy(decoder);
		return 1;
	}

	opus_decoder_destroy(decoder);

	std::cerr << "Done.\n";
	std::cerr << "Input:  " << args.input_path << "\n";
	std::cerr << "Output: " << args.output_path << "\n";
	std::cerr << "SSRC:   " << chosen_ssrc << "\n";
	std::cerr << "Decoded packets: " << decoded_packets << "\n";
	std::cerr << "Skipped packets: " << skipped_packets << "\n";
	std::cerr << "Decode errors:   " << decode_errors << "\n";
	std::cerr << "WAV data bytes:  " << total_written_bytes << "\n";

	return 0;
}
