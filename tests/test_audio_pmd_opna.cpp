#include "th03/audio/pmd_opna.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using th03::audio::PmdOpnaConfig;
using th03::audio::PmdOpnaEngine;
using th03::audio::PmdOpnaErrorCode;
using th03::audio::PmdOpnaFidelity;
using th03::audio::PmdOpnaRegisterTraceSink;
using th03::audio::PmdOpnaRegisterWrite;
using th03::audio::PmdOpnaStereoSample;
using th03::audio::ym2608_rhythm_rom_bytes;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void append(std::vector<std::byte> &bytes, std::uint8_t value)
{
	bytes.push_back(static_cast<std::byte>(value));
}

void set_le16(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::uint16_t value
)
{
	bytes[offset] = static_cast<std::byte>(value);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

// Newly authored from the public PMD header/command layout. This is not an
// original song or a derived PCM fixture. It exercises FM1/FM2/SSG1 and loops
// each part through the ordinary 0x80 part terminator.
std::vector<std::byte> synthetic_song()
{
	constexpr std::size_t file_prefix = 1U;
	constexpr std::size_t header_size = 0x18U;
	std::vector<std::byte> bytes(file_prefix + header_size, std::byte{0U});

	const auto emit = [&bytes](std::initializer_list<std::uint8_t> stream) {
		const auto offset = static_cast<std::uint16_t>(bytes.size() - 1U);
		for(const auto value : stream) {
			append(bytes, value);
		}
		return offset;
	};

	const auto fm1 = emit({
		0x24U, 0x30U, 0x28U, 0x30U, 0x2BU, 0x18U, 0x2FU, 0x18U, 0x80U,
	});
	const auto fm2 = emit({0x30U, 0x60U, 0x3FU, 0x20U, 0x80U});
	const auto ssg1 = emit({
		0x40U, 0x18U, 0x43U, 0x18U, 0x4FU, 0x18U, 0x80U,
	});
	const auto empty = emit({0x80U});
	const auto rhythm_table = emit({0x80U});

	const std::array<std::uint16_t, 11U> parts{
		fm1, fm2, empty, empty, empty, empty,
		ssg1, empty, empty, empty, empty,
	};
	for(std::size_t part = 0U; part < parts.size(); ++part) {
		set_le16(bytes, file_prefix + (part * 2U), parts[part]);
	}
	set_le16(bytes, file_prefix + 0x16U, rhythm_table);
	return bytes;
}

std::vector<std::byte> synthetic_rhythm_song()
{
	constexpr std::size_t file_prefix = 1U;
	constexpr std::size_t header_size = 0x18U;
	std::vector<std::byte> bytes(file_prefix + header_size, std::byte{0U});

	const auto emit = [&bytes](std::initializer_list<std::uint8_t> stream) {
		const auto offset = static_cast<std::uint16_t>(bytes.size() - 1U);
		for(const auto value : stream) {
			append(bytes, value);
		}
		return offset;
	};

	const auto empty = emit({0x80U});
	const auto rhythm_part = emit({0x00U, 0x80U});
	// Pattern 0: key on the bass drum for 24 timer ticks, then terminate.
	const auto rhythm_pattern = emit({0x80U, 0x01U, 0x18U, 0xFFU});
	const auto rhythm_table = static_cast<std::uint16_t>(bytes.size() - 1U);
	append(bytes, static_cast<std::uint8_t>(rhythm_pattern));
	append(bytes, static_cast<std::uint8_t>(rhythm_pattern >> 8U));

	std::array<std::uint16_t, 11U> parts{};
	parts.fill(empty);
	parts.back() = rhythm_part;
	for(std::size_t part = 0U; part < parts.size(); ++part) {
		set_le16(bytes, file_prefix + (part * 2U), parts[part]);
	}
	set_le16(bytes, file_prefix + 0x16U, rhythm_table);
	return bytes;
}

PmdOpnaEngine engine()
{
	auto created = PmdOpnaEngine::create(PmdOpnaConfig{
		// Explicit synthetic-gate configuration, not a hardware authenticity pin.
		.input_clock_hz = 7'987'200U,
		.fidelity = PmdOpnaFidelity::maximum,
	});
	expect(static_cast<bool>(created), "valid explicit engine config is accepted");
	return std::move(*created);
}

std::uint32_t sample_hash(std::span<const PmdOpnaStereoSample> samples)
{
	std::uint32_t hash = 2'166'136'261U;
	for(const auto &sample : samples) {
		for(const auto value : {sample.left, sample.right}) {
			const auto word = static_cast<std::uint16_t>(
				static_cast<std::int16_t>(value)
			);
			hash ^= static_cast<std::uint8_t>(word);
			hash *= 16'777'619U;
			hash ^= static_cast<std::uint8_t>(word >> 8U);
			hash *= 16'777'619U;
		}
	}
	return hash;
}

class Trace final : public PmdOpnaRegisterTraceSink {
public:
	void register_written(const PmdOpnaRegisterWrite &write) noexcept override
	{
		if(count == 0U) {
			first = write;
		}
		last = write;
		++count;
		if(write.address == 0x10U && (write.value & 0x80U) == 0U &&
			(write.value & 0x3FU) != 0U) {
			++rhythm_key_ons;
		}
	}

	std::size_t count = 0U;
	PmdOpnaRegisterWrite first{};
	PmdOpnaRegisterWrite last{};
	std::size_t rhythm_key_ons = 0U;
};

void test_config_and_load_validation()
{
	for(const auto config : {
		PmdOpnaConfig{},
		PmdOpnaConfig{.input_clock_hz = 7U},
		PmdOpnaConfig{
			.input_clock_hz = 7'987'201U,
			.fidelity = PmdOpnaFidelity::maximum,
		},
		PmdOpnaConfig{
			.input_clock_hz = 7'987'200U,
			.fidelity = static_cast<PmdOpnaFidelity>(0xFFU),
		},
	}) {
		const auto created = PmdOpnaEngine::create(config);
		expect(
			!created && created.error().code == PmdOpnaErrorCode::invalid_config,
			"zero, undersized, and nonintegral chip configurations are rejected"
		);
	}
	const std::array<std::byte, ym2608_rhythm_rom_bytes - 1U> short_rom{};
	const auto invalid_rom = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.fidelity = PmdOpnaFidelity::maximum,
		.rhythm_rom = short_rom,
	});
	expect(
		!invalid_rom &&
			invalid_rom.error().code == PmdOpnaErrorCode::invalid_rhythm_rom_size &&
			invalid_rom.error().supplied_size == short_rom.size(),
		"rhythm ROM is either absent or exactly the YM2608 8 KiB domain"
	);

	for(const auto &rate : {
		std::pair{PmdOpnaFidelity::minimum, 166'400U},
		std::pair{PmdOpnaFidelity::medium, 332'800U},
		std::pair{PmdOpnaFidelity::maximum, 998'400U},
	}) {
		auto configured = PmdOpnaEngine::create(PmdOpnaConfig{
			.input_clock_hz = 7'987'200U,
			.fidelity = rate.first,
		});
		expect(
			configured && configured->status().native_sample_rate == rate.second,
			"each explicit ymfm fidelity has the expected integral native rate"
		);
	}
	const std::array invalid_rhythm{std::byte{0U}};
	const auto bad_rhythm = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.rhythm_rom = invalid_rhythm,
	});
	expect(
		!bad_rhythm &&
			bad_rhythm.error().code == PmdOpnaErrorCode::invalid_rhythm_rom_size &&
			bad_rhythm.error().supplied_size == invalid_rhythm.size(),
		"rhythm input is either empty or the exact YM2608 8 KiB domain"
	);
	const std::array<std::byte, th03::audio::ym2608_rhythm_rom_bytes> rhythm{};
	const auto with_rhythm = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.rhythm_rom = rhythm,
	});
	expect(
		with_rhythm && with_rhythm->status().rhythm_rom_present,
		"caller-supplied exact rhythm image is owned and reported"
	);

	auto subject = engine();
	const auto empty = subject.load_song({});
	expect(
		!empty && empty.error().code == PmdOpnaErrorCode::invalid_song_size,
		"empty song is rejected"
	);
	const std::array malformed{std::byte{0xFFU}};
	const auto invalid = subject.load_song(malformed);
	expect(
		!invalid && invalid.error().code == PmdOpnaErrorCode::invalid_song,
		"malformed PMD mode byte is rejected"
	);
	std::vector<std::byte> oversized(65'536U, std::byte{0U});
	const auto too_large = subject.load_song(oversized);
	expect(
		!too_large &&
			too_large.error().code == PmdOpnaErrorCode::invalid_song_size &&
			too_large.error().supplied_size == oversized.size(),
		"PMD driver's 16-bit byte-span limit is explicit"
	);

	const auto no_song = subject.play();
	expect(
		!no_song && no_song.error().code == PmdOpnaErrorCode::no_song_loaded,
		"play requires a validated prepared song"
	);
}

void test_load_is_chip_silent_and_play_is_traced()
{
	auto subject = engine();
	const auto song = synthetic_song();
	expect(static_cast<bool>(subject.load_song(song)), "synthetic PMD loads");
	const auto loaded = subject.status();
	expect(loaded.song_loaded, "load publishes prepared-song state");
	expect(!loaded.playing, "load does not start sequencing");
	expect(loaded.register_writes == 0U, "load performs no chip writes");
	expect(
		loaded.native_sample_rate == 998'400U,
		"maximum-fidelity native rate is the explicit clock divided by eight"
	);

	Trace trace;
	subject.set_register_trace_sink(&trace);
	expect(static_cast<bool>(subject.play()), "prepared song starts");
	const auto started = subject.status();
	expect(started.playing, "play publishes sequencer state");
	expect(started.register_writes == 17U, "PMD start emits exact reset prefix");
	expect(trace.count == started.register_writes, "trace observes every chip write");
	expect(
		trace.first.sequence == 1U && trace.first.chip_clock == 0U &&
			trace.first.address == 0x29U && trace.first.value == 0x83U,
		"trace begins with PMD's OPNA enable/reset write"
	);
	expect(trace.last.sequence == trace.count, "trace sequence is contiguous");
}

void test_pcm_and_fresh_replay_are_deterministic()
{
	auto subject = engine();
	const auto song = synthetic_song();
	const auto caller_song = song;
	expect(static_cast<bool>(subject.load_song(song)), "determinism song loads");
	expect(static_cast<bool>(subject.play()), "first deterministic play starts");
	std::vector<PmdOpnaStereoSample> first(199'680U);
	subject.render_native(first);
	const auto first_pcm_hash = sample_hash(first);
	const auto first_status = subject.status();
	expect(first_status.samples_rendered == first.size(), "render count is exact");
	expect(
		first_status.chip_clocks == first.size() * 8U,
		"native samples advance an exact integral chip-clock count"
	);
	expect(first_status.timer_b_count == 12U, "ymfm timers drive exact PMD ticks");
	expect(
		first_status.register_writes == 61U &&
			first_status.register_hash == 0x85023B32U,
		"generated-song register trace matches the locked cross-compiler hash"
	);
	expect(
		first_pcm_hash == 0x1DF60DC5U,
		"generated-song signed-16 PCM matches the locked cross-compiler hash"
	);
	expect(song == caller_song, "sequencing never mutates caller-owned song bytes");

	expect(static_cast<bool>(subject.play()), "second play starts from fresh image");
	std::vector<PmdOpnaStereoSample> second(first.size());
	subject.render_native(second);
	const auto second_status = subject.status();
	expect(sample_hash(second) == first_pcm_hash, "fresh replay PCM is identical");
	expect(
		second_status.register_hash == first_status.register_hash &&
			second_status.register_writes == first_status.register_writes &&
			second_status.timer_b_count == first_status.timer_b_count,
		"fresh replay register and timer state is identical"
	);

	const std::array malformed{std::byte{0xFFU}};
	expect(!subject.load_song(malformed), "invalid replacement is rejected");
	expect(
		static_cast<bool>(subject.play()),
		"invalid replacement preserves the prior prepared song"
	);
	std::vector<PmdOpnaStereoSample> after_rejection(first.size());
	subject.render_native(after_rejection);
	expect(
		sample_hash(after_rejection) == first_pcm_hash,
		"preserved prepared song remains byte-deterministic"
	);
}

void test_rhythm_rom_is_owned_and_audible()
{
	std::array<std::byte, ym2608_rhythm_rom_bytes> source_rom{};
	for(std::size_t index = 0U; index < source_rom.size(); ++index) {
		source_rom[index] = static_cast<std::byte>((index * 37U + 11U) & 0xFFU);
	}
	const auto retained_rom = source_rom;
	auto copied = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.fidelity = PmdOpnaFidelity::maximum,
		.rhythm_rom = source_rom,
	});
	expect(static_cast<bool>(copied), "exact rhythm ROM is accepted");
	expect(copied->status().rhythm_rom_present, "owned rhythm ROM is reported");
	source_rom.fill(std::byte{0U});

	auto reference = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.fidelity = PmdOpnaFidelity::maximum,
		.rhythm_rom = retained_rom,
	});
	auto silent = engine();
	expect(!silent.status().rhythm_rom_present, "missing ROM mode is explicit");

	const auto song = synthetic_rhythm_song();
	expect(copied->load_song(song) && copied->play(), "copied-ROM rhythm starts");
	expect(reference->load_song(song) && reference->play(), "reference rhythm starts");
	expect(silent.load_song(song) && silent.play(), "silent-rhythm mode still sequences");

	std::vector<PmdOpnaStereoSample> copied_pcm(80'000U);
	std::vector<PmdOpnaStereoSample> reference_pcm(copied_pcm.size());
	std::vector<PmdOpnaStereoSample> silent_pcm(copied_pcm.size());
	copied->render_native(copied_pcm);
	reference->render_native(reference_pcm);
	silent.render_native(silent_pcm);
	const auto copied_hash = sample_hash(copied_pcm);
	expect(
		copied_hash == sample_hash(reference_pcm),
		"engine owns the ROM after caller storage changes"
	);
	expect(
		copied_hash != sample_hash(silent_pcm),
		"ADPCM-A rhythm output reads the supplied ROM rather than silent fallback"
	);
}

void test_instances_and_preplay_silence_are_independent()
{
	auto first = engine();
	auto second = engine();
	std::array<PmdOpnaStereoSample, 8U> silence{};
	for(auto &sample : silence) {
		sample = PmdOpnaStereoSample{.left = 1, .right = -1};
	}
	first.render_native(silence);
	for(const auto &sample : silence) {
		expect(sample.left == 0 && sample.right == 0, "preplay render is silent");
	}
	expect(first.status().samples_rendered == 0U, "preplay silence has no chip time");

	const auto song = synthetic_song();
	expect(first.load_song(song) && first.play(), "first instance starts");
	expect(second.load_song(song) && second.play(), "second instance starts");
	std::vector<PmdOpnaStereoSample> first_pcm(40'000U);
	std::vector<PmdOpnaStereoSample> second_pcm(40'000U);
	first.render_native(first_pcm);
	second.render_native(second_pcm);
	expect(
		sample_hash(first_pcm) == sample_hash(second_pcm),
		"opaque shim has no cross-instance global state"
	);
}

std::vector<std::byte> read_external_file(const std::filesystem::path &path)
{
	std::error_code error;
	const auto size = std::filesystem::file_size(path, error);
	if(error || size > 65'535U) {
		return {};
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	std::ifstream input(path, std::ios::binary);
	if(!input || !input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	)) {
		return {};
	}
	return bytes;
}

void test_authentic_corpus(const std::filesystem::path &yume1_root)
{
	constexpr std::array names{
		"00MM.M", "01MM.M", "02MM.M", "03MM.M", "04MM.M", "05MM.M",
		"06MM.M", "07MM.M", "08MM.M", "DEC.M", "DEMO1.M", "DEMO2.M",
		"DEMO3.M", "DEMO4.M", "DEMO5.M", "ED.M", "OP.M", "OVER.M",
		"SCORE.M", "SELECT.M", "WIN.M",
	};
	std::size_t songs_passed = 0U;
	std::size_t songs_with_rhythm = 0U;
	std::size_t rhythm_key_ons = 0U;
	for(const std::string_view name : names) {
		const auto bytes = read_external_file(yume1_root / name);
		expect(!bytes.empty(), "authentic PMD role is readable and 16-bit bounded");
		if(bytes.empty()) {
			continue;
		}
		auto created = PmdOpnaEngine::create(PmdOpnaConfig{
			.input_clock_hz = 7'987'200U,
			.fidelity = PmdOpnaFidelity::minimum,
		});
		expect(static_cast<bool>(created), "authentic gate engine is created");
		if(!created) {
			continue;
		}
		auto subject = std::move(*created);
		expect(static_cast<bool>(subject.load_song(bytes)), "authentic PMD role loads");
		Trace trace;
		subject.set_register_trace_sink(&trace);
		expect(static_cast<bool>(subject.play()), "authentic PMD role starts");
		if(!subject.status().playing) {
			continue;
		}

		// Render at least one second, then stop after the first rhythm key-on or
		// one complete loop/end. The 60-second ceiling covers the three shipped
		// songs whose first rhythm event is later than the opening interval.
		const auto one_second =
			static_cast<std::uint64_t>(subject.status().native_sample_rate);
		const auto sample_limit = one_second * 60U;
		std::array<PmdOpnaStereoSample, 4'096U> scratch{};
		while(subject.status().samples_rendered < sample_limit) {
			const auto remaining = sample_limit - subject.status().samples_rendered;
			const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(
				remaining,
				scratch.size()
			));
			subject.render_native(std::span(scratch).first(count));
			const auto state = subject.status();
			if(!state.playing || state.loop_count != 0U ||
				(state.samples_rendered >= one_second &&
					trace.rhythm_key_ons != 0U)) {
				break;
			}
		}
		expect(subject.status().register_writes > 17U, "authentic PMD role advances");
		++songs_passed;
		if(trace.rhythm_key_ons != 0U) {
			++songs_with_rhythm;
			rhythm_key_ons += trace.rhythm_key_ons;
		}
	}
	expect(songs_passed == names.size(), "all 21 authentic PMD roles pass");
	expect(songs_with_rhythm == 20U, "20 authentic songs require ADPCM-A rhythm");
	expect(rhythm_key_ons > 0U, "authentic corpus emits rhythm key-ons");
	std::cout << "authentic_songs_passed=" << songs_passed
		<< " songs_with_rhythm=" << songs_with_rhythm
		<< " rhythm_key_ons=" << rhythm_key_ons << '\n';
}

} // namespace

int main(int argc, char **argv)
{
	test_config_and_load_validation();
	test_load_is_chip_silent_and_play_is_traced();
	test_pcm_and_fresh_replay_are_deterministic();
	test_rhythm_rom_is_owned_and_audible();
	test_instances_and_preplay_silence_are_independent();
	if(argc == 3 && std::string_view(argv[1]) == "--authentic") {
		test_authentic_corpus(argv[2]);
	} else if(argc != 1) {
		std::cerr << "usage: th03-audio-pmd-tests [--authentic <YUME1.DAT>]\n";
		return 2;
	}

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
