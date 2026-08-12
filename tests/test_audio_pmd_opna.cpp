#include "th03/audio/pmd_opna.hpp"

#include "../src/audio/pmd_opna_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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
using th03::audio::PmdOpnaRhythmPcm;
using th03::audio::PmdOpnaStereoSample;
using th03::audio::pmd_effect_bank_max_bytes;
using th03::audio::pmd_effect_count;
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

std::uint16_t get_le16(
	std::span<const std::byte> bytes,
	std::size_t offset
)
{
	return static_cast<std::uint16_t>(
		std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U)
	);
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

// Newly authored from the bounded TH03 EFC grammar. The 127 callable streams
// are followed by the table's final tone pointer and one 26-byte FM tone. No
// original effect-bank or instrument bytes are present in this fixture.
std::vector<std::byte> synthetic_effect_bank()
{
	constexpr std::size_t pointer_entries = 128U;
	std::vector<std::byte> bytes(pointer_entries * 2U, std::byte{0U});

	for(std::size_t effect = 0U; effect < pmd_effect_count; ++effect) {
		set_le16(
			bytes,
			effect * 2U,
			static_cast<std::uint16_t>(bytes.size())
		);
		switch(effect) {
		case 1U:
			for(const auto value : {
				0xFFU, 0x00U, 0xFDU, 0x7FU, 0x40U, 0x03U, 0xF3U,
				0x43U, 0x02U, 0x80U,
			}) {
				append(bytes, static_cast<std::uint8_t>(value));
			}
			break;
		case 2U:
			for(const auto value : {
				0xFFU, 0x00U, 0xFDU, 0x70U,
				0xF2U, 0x00U, 0x01U, 0x01U, 0x02U,
				0xF1U, 0x01U, 0x47U, 0x04U, 0x80U,
			}) {
				append(bytes, static_cast<std::uint8_t>(value));
			}
			break;
		case 3U:
			for(const auto value : {
				0xFFU, 0x00U, 0xFDU, 0x78U, 0xF5U, 0x01U,
				0x40U, 0x02U, 0xFBU, 0x42U, 0x02U,
				0xDAU, 0x42U, 0x47U, 0x03U, 0x80U,
			}) {
				append(bytes, static_cast<std::uint8_t>(value));
			}
			break;
		case 4U: {
			append(bytes, 0xFFU);
			append(bytes, 0x00U);
			append(bytes, 0xFDU);
			append(bytes, 0x78U);
			const auto repeat_reset = bytes.size();
			append(bytes, 0xF9U);
			append(bytes, 0x00U);
			append(bytes, 0x00U);
			const auto note = bytes.size();
			append(bytes, 0x4AU);
			append(bytes, 0x01U);
			const auto repeat = bytes.size();
			append(bytes, 0xF8U);
			append(bytes, 0x02U);
			append(bytes, 0x00U);
			append(bytes, 0x00U);
			append(bytes, 0x00U);
			append(bytes, 0x80U);
			set_le16(
				bytes,
				repeat_reset + 1U,
				static_cast<std::uint16_t>(repeat + 1U)
			);
			set_le16(
				bytes,
				repeat + 3U,
				static_cast<std::uint16_t>(note - 2U)
			);
			break;
		}
		default:
			append(bytes, 0x80U);
			break;
		}
	}

	set_le16(
		bytes,
		static_cast<std::size_t>(pmd_effect_count) * 2U,
		static_cast<std::uint16_t>(bytes.size())
	);
	// Tone ID, four DT/ML values, four TL values, four KS/AR values,
	// four AM/DR values, four SR values, four SL/RR values, FB/algorithm.
	for(const auto value : {
		0x00U,
		0x01U, 0x01U, 0x01U, 0x01U,
		0x00U, 0x00U, 0x00U, 0x00U,
		0x1FU, 0x1FU, 0x1FU, 0x1FU,
		0x00U, 0x00U, 0x00U, 0x00U,
		0x00U, 0x00U, 0x00U, 0x00U,
		0x0FU, 0x0FU, 0x0FU, 0x0FU,
		0x07U,
	}) {
		append(bytes, static_cast<std::uint8_t>(value));
	}
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
		for(const auto value : {
			static_cast<std::uint32_t>(sample.left),
			static_cast<std::uint32_t>(sample.right),
		}) {
			for(std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
				hash ^= (value >> shift) & 0xFFU;
				hash *= 16'777'619U;
			}
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
		if(write.address == 0x28U && (write.value & 0x07U) == 0x06U) {
			if((write.value & 0xF0U) == 0U) {
				++fm6_key_offs;
			} else {
				++fm6_key_ons;
			}
		}
	}

	std::size_t count = 0U;
	PmdOpnaRegisterWrite first{};
	PmdOpnaRegisterWrite last{};
	std::size_t rhythm_key_ons = 0U;
	std::size_t fm6_key_ons = 0U;
	std::size_t fm6_key_offs = 0U;
};

void test_config_and_load_validation()
{
	for(const auto &config : {
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
	std::array<std::int16_t, 4U> one_voice{};
	PmdOpnaRhythmPcm partial_pcm{.sample_rate_hz = 44'100U};
	partial_pcm.voices[0] = one_voice;
	const auto invalid_pcm = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.rhythm_pcm = partial_pcm,
	});
	expect(
		!invalid_pcm &&
			invalid_pcm.error().code == PmdOpnaErrorCode::invalid_rhythm_pcm,
		"decoded rhythm requires all six voices and a sample rate"
	);
	const auto rate_without_voices = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.rhythm_pcm = PmdOpnaRhythmPcm{.sample_rate_hz = 44'100U},
	});
	expect(
		!rate_without_voices &&
			rate_without_voices.error().code ==
				PmdOpnaErrorCode::invalid_rhythm_pcm,
		"decoded rhythm cannot declare a sample rate without six voices"
	);
	PmdOpnaRhythmPcm complete_pcm{.sample_rate_hz = 44'100U};
	complete_pcm.voices.fill(one_voice);
	const auto conflicting_pcm = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.rhythm_rom = rhythm,
		.rhythm_pcm = complete_pcm,
	});
	expect(
		!conflicting_pcm &&
			conflicting_pcm.error().code == PmdOpnaErrorCode::invalid_rhythm_pcm,
		"encoded ROM and decoded rhythm profiles are mutually exclusive"
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

void test_effect_bank_validation_is_transactional()
{
	auto subject = engine();
	const auto valid = synthetic_effect_bank();
	const auto caller_bank = valid;
	expect(
		static_cast<bool>(subject.load_effect_bank(valid)),
		"generated EFC bank validates"
	);
	expect(
		subject.status().effect_bank_loaded,
		"validated EFC bank is reported as prepared"
	);
	expect(valid == caller_bank, "EFC validation does not mutate caller bytes");

	const auto empty = subject.load_effect_bank({});
	expect(
		!empty && empty.error().code ==
			PmdOpnaErrorCode::invalid_effect_bank_size,
		"empty EFC bank is rejected"
	);
	std::vector<std::byte> oversized(
		pmd_effect_bank_max_bytes + 1U,
		std::byte{0U}
	);
	const auto too_large = subject.load_effect_bank(oversized);
	expect(
		!too_large && too_large.error().code ==
			PmdOpnaErrorCode::invalid_effect_bank_size,
		"resident driver's two-KiB EFC limit is explicit"
	);

	auto wrong_header = valid;
	set_le16(wrong_header, 0U, 0x00FFU);
	expect(
		!subject.load_effect_bank(wrong_header),
		"EFC pointer table must occupy exactly 256 bytes"
	);
	auto non_increasing = valid;
	set_le16(non_increasing, 2U, get_le16(non_increasing, 0U));
	expect(
		!subject.load_effect_bank(non_increasing),
		"EFC stream pointers must be strictly increasing"
	);
	auto invalid_opcode = valid;
	invalid_opcode[get_le16(invalid_opcode, 0U)] = std::byte{0x81U};
	expect(
		!subject.load_effect_bank(invalid_opcode),
		"unimplemented EFC commands are rejected before playback"
	);
	auto truncated_note = valid;
	truncated_note[get_le16(truncated_note, 0U)] = std::byte{0x40U};
	expect(
		!subject.load_effect_bank(truncated_note),
		"truncated EFC note is rejected"
	);
	auto unresolved_tone = valid;
	const auto effect_one = get_le16(unresolved_tone, 2U);
	unresolved_tone[effect_one + 1U] = std::byte{0xFEU};
	expect(
		!subject.load_effect_bank(unresolved_tone),
		"every selected EFC tone must resolve in the bank-local table"
	);
	auto cross_stream_repeat = valid;
	const auto effect_four = get_le16(cross_stream_repeat, 8U);
	set_le16(cross_stream_repeat, effect_four + 5U, 0U);
	expect(
		!subject.load_effect_bank(cross_stream_repeat),
		"EFC repeat mutation cannot target another stream or the header"
	);
	auto duplicate_tone = valid;
	const auto tone = get_le16(duplicate_tone, 254U);
	duplicate_tone.insert(
		duplicate_tone.end(),
		duplicate_tone.begin() + tone,
		duplicate_tone.begin() + tone + 26U
	);
	expect(
		!subject.load_effect_bank(duplicate_tone),
		"duplicate EFC tone identifiers are rejected"
	);

	expect(
		subject.status().effect_bank_loaded,
		"every rejected EFC replacement preserves the prepared bank"
	);
	expect(
		subject.load_song(synthetic_song()) && subject.play() &&
			subject.play_effect(1U),
		"preserved EFC bank attaches to the next fresh play"
	);
	expect(
		subject.status().active_effect == 1U,
		"preserved EFC bank remains executable"
	);
}

void test_effect_transport_and_timer_a_ownership()
{
	auto subject = engine();
	const auto song = synthetic_song();
	const auto bank = synthetic_effect_bank();
	const auto caller_bank = bank;
	const auto before_song = subject.play_effect(1U);
	expect(
		!before_song && before_song.error().code ==
			PmdOpnaErrorCode::no_song_loaded,
		"EFC playback requires an initialized PMD instance"
	);
	expect(static_cast<bool>(subject.load_song(song)), "effect transport song loads");
	expect(static_cast<bool>(subject.play()), "effect transport song starts without a bank");
	const auto without_bank = subject.play_effect(1U);
	expect(
		!without_bank && without_bank.error().code ==
			PmdOpnaErrorCode::no_effect_bank_loaded,
		"a bank prepared after Play is not retrofitted into active bytes"
	);
	expect(static_cast<bool>(subject.load_effect_bank(bank)), "effect transport bank prepares");
	expect(static_cast<bool>(subject.play()), "fresh Play attaches the prepared EFC bank");

	const auto invalid = subject.play_effect(pmd_effect_count);
	expect(
		!invalid && invalid.error().code == PmdOpnaErrorCode::invalid_effect_id,
		"tone-table slot 127 is not callable as an effect"
	);
	expect(!subject.status().active_effect, "invalid effect start is atomic");

	Trace trace;
	subject.set_register_trace_sink(&trace);
	expect(static_cast<bool>(subject.play_effect(1U)), "effect one starts immediately");
	expect(
		subject.status().active_effect == 1U && trace.fm6_key_ons == 1U,
		"effect start owns and keys on OPNA FM6"
	);
	const auto timer_b_before_pause = subject.status().timer_b_count;
	subject.pause();
	const auto sample_limit = subject.status().samples_rendered + 200'000U;
	std::array<PmdOpnaStereoSample, 2'048U> scratch{};
	while(subject.status().active_effect &&
		subject.status().samples_rendered < sample_limit) {
		subject.render_native(scratch);
	}
	expect(
		!subject.status().active_effect,
		"FM effect advances to natural key-off from Timer A while song is paused"
	);
	expect(
		subject.status().timer_b_count == timer_b_before_pause,
		"paused song sequencing remains frozen while Timer A owns effects"
	);
	expect(trace.fm6_key_offs != 0U, "natural EFC end keys off FM6");
	subject.resume();

	expect(static_cast<bool>(subject.play_effect(4U)), "relocated repeat effect starts");
	const auto repeat_limit = subject.status().samples_rendered + 200'000U;
	while(subject.status().active_effect &&
		subject.status().samples_rendered < repeat_limit) {
		subject.render_native(scratch);
	}
	expect(
		!subject.status().active_effect && trace.fm6_key_ons >= 3U,
		"relocated F9/F8 counters replay the generated note then terminate"
	);

	expect(static_cast<bool>(subject.play_effect(1U)), "replacement source effect starts");
	expect(static_cast<bool>(subject.play_effect(2U)), "replacement effect starts atomically");
	expect(
		subject.status().active_effect == 2U,
		"new effect replaces the old FM6 program"
	);
	subject.stop();
	expect(
		!subject.status().playing && subject.status().active_effect == 2U,
		"song Stop preserves the independently serviced FM effect"
	);
	subject.stop_effect();
	expect(!subject.status().active_effect, "explicit EFC Stop releases FM6");
	expect(
		subject.play_effect(3U) && subject.status().active_effect == 3U,
		"FM effects can start while the song is stopped"
	);
	const auto stopped_song_limit = subject.status().samples_rendered + 200'000U;
	while(subject.status().active_effect &&
		subject.status().samples_rendered < stopped_song_limit) {
		subject.render_native(scratch);
	}
	expect(
		!subject.status().active_effect,
		"tie, transpose, and portamento commands finish after song Stop"
	);
	expect(bank == caller_bank, "EFC repeat counters never mutate caller bytes");
}

void test_effect_pcm_and_registers_are_deterministic()
{
	const auto song = synthetic_song();
	const auto bank = synthetic_effect_bank();
	auto first = engine();
	auto second = engine();
	expect(
		first.load_song(song) && first.load_effect_bank(bank) && first.play() &&
			second.load_song(song) && second.load_effect_bank(bank) && second.play(),
		"isolated effect engines accept equal prepared inputs"
	);
	expect(
		first.play_effect(4U) && second.play_effect(4U),
		"equal relocated repeat effects start"
	);
	std::vector<PmdOpnaStereoSample> first_pcm(120'000U);
	std::vector<PmdOpnaStereoSample> second_pcm(first_pcm.size());
	first.render_native(first_pcm);
	second.render_native(second_pcm);
	const auto first_status = first.status();
	const auto second_status = second.status();
	expect(
		sample_hash(first_pcm) == sample_hash(second_pcm),
		"effect PCM is deterministic across isolated engines"
	);
	expect(
		first_status.register_hash == second_status.register_hash &&
			first_status.register_writes == second_status.register_writes &&
			first_status.timer_b_count == second_status.timer_b_count,
		"effect register and timer state is deterministic"
	);
	expect(
		!first_status.active_effect && !second_status.active_effect,
		"equal repeat effects naturally end at the same Timer-A boundary"
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
	expect(
		th03::audio::detail::pmd_mix_to_q31(32'767, 0) == 0x7FFF0000 &&
		th03::audio::detail::pmd_mix_to_q31(-32'768, 0) ==
			std::numeric_limits<std::int32_t>::min() &&
		th03::audio::detail::pmd_mix_to_q31(0, 2'048) == 543 * 65'536 &&
		th03::audio::detail::pmd_mix_to_q31(0, -2'048) == -543 * 65'536 &&
		th03::audio::detail::pmd_mix_to_q31(0, 32'764) == 8'686 * 65'536 &&
		th03::audio::detail::pmd_mix_to_q31(30'000, 30'000) == 0x7FFF0000 &&
		th03::audio::detail::pmd_mix_to_q31(-30'000, -30'000) ==
			std::numeric_limits<std::int32_t>::min(),
		"T98-weighted signed-16 ymfm lanes saturate and promote exactly to Q31"
	);
	auto subject = engine();
	const auto song = synthetic_song();
	const auto caller_song = song;
	expect(static_cast<bool>(subject.load_song(song)), "determinism song loads");
	expect(static_cast<bool>(subject.play()), "first deterministic play starts");
	std::vector<PmdOpnaStereoSample> first(199'680U);
	subject.render_native(first);
	const auto first_pcm_hash = sample_hash(first);
	const auto peak = std::max_element(
		first.begin(), first.end(), [](const auto &left, const auto &right) {
			return std::max(std::abs(static_cast<std::int64_t>(left.left)),
				std::abs(static_cast<std::int64_t>(left.right))) <
				std::max(std::abs(static_cast<std::int64_t>(right.left)),
					std::abs(static_cast<std::int64_t>(right.right)));
		}
	);
	expect(
		peak != first.end() &&
			std::max(std::abs(static_cast<std::int64_t>(peak->left)),
				std::abs(static_cast<std::int64_t>(peak->right))) > (1LL << 24U) &&
			std::all_of(first.begin(), first.end(), [](const auto &sample) {
				return (static_cast<std::uint32_t>(sample.left) & 0xFFFFU) == 0U &&
					(static_cast<std::uint32_t>(sample.right) & 0xFFFFU) == 0U;
			}),
		"native PMD output occupies the S32 domain with exact Q31 promotion"
	);
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
		first_pcm_hash == 0x17A82FC5U,
		"generated-song Q31 PCM matches the locked cross-compiler hash"
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
	const std::array<std::byte, ym2608_rhythm_rom_bytes> zero_rom{};
	auto zero_decoded = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.fidelity = PmdOpnaFidelity::maximum,
		.rhythm_rom = zero_rom,
	});
	expect(!silent.status().rhythm_rom_present, "missing ROM mode is explicit");

	const auto song = synthetic_rhythm_song();
	expect(copied->load_song(song) && copied->play(), "copied-ROM rhythm starts");
	expect(reference->load_song(song) && reference->play(), "reference rhythm starts");
	expect(silent.load_song(song) && silent.play(), "silent-rhythm mode still sequences");
	expect(
		zero_decoded && zero_decoded->load_song(song) && zero_decoded->play(),
		"explicit zero-nibble ADPCM reference starts"
	);

	std::vector<PmdOpnaStereoSample> copied_pcm(80'000U);
	std::vector<PmdOpnaStereoSample> reference_pcm(copied_pcm.size());
	std::vector<PmdOpnaStereoSample> silent_pcm(copied_pcm.size());
	std::vector<PmdOpnaStereoSample> zero_pcm(copied_pcm.size());
	copied->render_native(copied_pcm);
	reference->render_native(reference_pcm);
	silent.render_native(silent_pcm);
	if(zero_decoded) {
		zero_decoded->render_native(zero_pcm);
	}
	const auto copied_hash = sample_hash(copied_pcm);
	expect(
		copied_hash == sample_hash(reference_pcm),
		"engine owns the ROM after caller storage changes"
	);
	expect(
		!std::equal(
			copied_pcm.begin(), copied_pcm.end(), silent_pcm.begin(),
			[](const auto &left, const auto &right) {
				return left.left == right.left && left.right == right.right;
			}
		),
		"ADPCM-A rhythm output reads the supplied ROM rather than silent fallback"
	);
	expect(
		[](const auto &without_data, const auto &zero_nibbles) {
			const auto magnitude = [](const auto &samples) {
				std::uint64_t sum = 0U;
				for(const auto &sample : samples) {
					sum += static_cast<std::uint64_t>(std::abs(
						static_cast<std::int64_t>(sample.left)
					));
					sum += static_cast<std::uint64_t>(std::abs(
						static_cast<std::int64_t>(sample.right)
					));
				}
				return sum;
			};
			return magnitude(without_data) < magnitude(zero_nibbles);
		}(silent_pcm, zero_pcm),
		"missing rhythm data suppresses the loud zero-nibble ADPCM waveform"
	);
}

void test_decoded_rhythm_is_owned_and_audible()
{
	std::array<std::array<std::int16_t, 256U>, 6U> source{};
	for(std::size_t voice = 0U; voice < source.size(); ++voice) {
		for(std::size_t sample = 0U; sample < source[voice].size(); ++sample) {
			source[voice][sample] = static_cast<std::int16_t>(
				4'000 + static_cast<std::int32_t>(voice * 500U) -
				static_cast<std::int32_t>(sample * 8U)
			);
		}
	}
	const auto retained = source;
	PmdOpnaRhythmPcm profile{.sample_rate_hz = 44'100U};
	for(std::size_t voice = 0U; voice < source.size(); ++voice) {
		profile.voices[voice] = source[voice];
	}
	auto copied = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.fidelity = PmdOpnaFidelity::maximum,
		.rhythm_pcm = profile,
	});
	expect(static_cast<bool>(copied), "decoded six-voice rhythm is accepted");
	expect(
		copied && copied->status().rhythm_pcm_present &&
			!copied->status().rhythm_rom_present,
		"decoded rhythm profile is reported independently from a ROM"
	);
	for(auto &voice : source) {
		voice.fill(0);
	}
	PmdOpnaRhythmPcm reference_profile{.sample_rate_hz = 44'100U};
	for(std::size_t voice = 0U; voice < retained.size(); ++voice) {
		reference_profile.voices[voice] = retained[voice];
	}
	auto reference = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.fidelity = PmdOpnaFidelity::maximum,
		.rhythm_pcm = reference_profile,
	});
	const auto song = synthetic_rhythm_song();
	expect(
		copied && reference && copied->load_song(song) && copied->play() &&
			reference->load_song(song) && reference->play(),
		"decoded-rhythm comparison starts"
	);
	if(!copied || !reference) {
		return;
	}
	std::vector<PmdOpnaStereoSample> copied_pcm(80'000U);
	std::vector<PmdOpnaStereoSample> reference_pcm(copied_pcm.size());
	copied->render_native(copied_pcm);
	reference->render_native(reference_pcm);
	expect(
		sample_hash(copied_pcm) == sample_hash(reference_pcm),
		"engine owns decoded rhythm after caller storage changes"
	);
	expect(
		std::any_of(copied_pcm.begin(), copied_pcm.end(), [](const auto &sample) {
			return sample.left != 0 || sample.right != 0;
		}),
		"decoded rhythm reaches the canonical stereo mix"
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

struct ExternalRhythmPcm final {
	std::array<std::vector<std::int16_t>, 6U> voices{};

	PmdOpnaRhythmPcm view() const
	{
		PmdOpnaRhythmPcm result{.sample_rate_hz = 44'100U};
		for(std::size_t index = 0U; index < voices.size(); ++index) {
			result.voices[index] = voices[index];
		}
		return result;
	}
};

std::optional<ExternalRhythmPcm> read_external_rhythm_pcm(
	const std::filesystem::path &path
)
{
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::nullopt;
	}
	std::vector<std::uint8_t> bytes{
		std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()
	};
	constexpr std::array<std::uint8_t, 8U> magic{
		'T', '9', '8', 'R', 'H', 'Y', '0', 0U,
	};
	constexpr std::array<std::uint32_t, 6U> lengths{
		0x2590U, 0x3B10U, 0x6EEDU, 0x3E00U, 0x2F5EU, 0x2018U,
	};
	if(bytes.size() != 178'730U ||
		!std::equal(magic.begin(), magic.end(), bytes.begin())) {
		return std::nullopt;
	}
	const auto le32 = [&bytes](std::size_t offset) {
		return static_cast<std::uint32_t>(bytes[offset]) |
			(static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
			(static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
			(static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
	};
	if(le32(8U) != 44'100U) {
		return std::nullopt;
	}
	ExternalRhythmPcm result{};
	std::size_t cursor = 36U;
	for(std::size_t voice = 0U; voice < result.voices.size(); ++voice) {
		if(le32(12U + (voice * 4U)) != lengths[voice]) {
			return std::nullopt;
		}
		result.voices[voice].resize(lengths[voice]);
		for(auto &sample : result.voices[voice]) {
			sample = static_cast<std::int16_t>(
				static_cast<std::uint16_t>(bytes[cursor]) |
				(static_cast<std::uint16_t>(bytes[cursor + 1U]) << 8U)
			);
			cursor += 2U;
		}
	}
	return result;
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

void test_authentic_title_mix(
	const std::filesystem::path &yume1_root,
	PmdOpnaRhythmPcm rhythm_pcm = {}
)
{
	const auto song = read_external_file(yume1_root / "OP.M");
	expect(!song.empty(), "authentic title song is readable");
	if(song.empty()) {
		return;
	}
	auto created = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.fidelity = PmdOpnaFidelity::minimum,
		.rhythm_pcm = rhythm_pcm,
	});
	expect(static_cast<bool>(created), "authentic title mix engine is created");
	if(!created) {
		return;
	}
	auto subject = std::move(*created);
	expect(subject.load_song(song) && subject.play(), "authentic title mix starts");
	std::vector<PmdOpnaStereoSample> pcm(subject.status().native_sample_rate);
	subject.render_native(pcm);
	std::int64_t peak = 0;
	std::size_t saturated = 0U;
	for(const auto &sample : pcm) {
		for(const auto value : {sample.left, sample.right}) {
			peak = std::max(peak, std::abs(static_cast<std::int64_t>(value)));
			if(value == std::numeric_limits<std::int32_t>::min() ||
				value == 0x7FFF0000) {
				++saturated;
			}
		}
	}
	const auto hash = sample_hash(pcm);
	const bool decoded_profile = rhythm_pcm.sample_rate_hz != 0U;
	expect(
		(decoded_profile ?
			(hash == 1'847'911'017U && peak == 1'662'386'176LL) :
			(hash == 50'015'953U && peak == 586'481'664LL)) &&
			saturated == 0U,
		"authentic OP.M mix is profile-specific, deterministic, and unclipped"
	);
}

void test_authentic_complete_voice_routes(
	const std::filesystem::path &yume1_root,
	PmdOpnaRhythmPcm rhythm_pcm
)
{
	const auto song = read_external_file(yume1_root / "DEMO1.M");
	const auto rhythm_song = read_external_file(yume1_root / "OP.M");
	expect(!song.empty(), "complete-voice reference song is readable");
	expect(!rhythm_song.empty(), "complete-rhythm reference song is readable");
	if(song.empty() || rhythm_song.empty()) {
		return;
	}
	std::array<std::uint32_t, 9U> hashes{};
	for(std::size_t route = 0U; route < hashes.size(); ++route) {
		const auto audible_part = static_cast<std::uint8_t>(
			route < 8U ? route : 10U
		);
		auto created = PmdOpnaEngine::create(PmdOpnaConfig{
			.input_clock_hz = 7'987'200U,
			.fidelity = PmdOpnaFidelity::minimum,
			.rhythm_pcm = rhythm_pcm,
		});
		expect(static_cast<bool>(created), "complete-voice engine is created");
		if(!created) {
			continue;
		}
		auto subject = std::move(*created);
		const auto &selected_song = route < 8U ? song : rhythm_song;
		expect(
			subject.load_song(selected_song) && subject.play(),
			"complete-voice reference song starts"
		);
		for(std::uint8_t part = 0U; part <= 13U; ++part) {
			if(part != audible_part) {
				static_cast<void>(subject.set_part_mask(part, true));
			}
		}
		if(audible_part != 10U) {
			static_cast<void>(subject.set_part_mask(14U, true));
		}
		// Play initializes the first source notes before AH=1Eh can mask parts.
		// Let those pre-mask key-ons expire before measuring the isolated route.
		std::vector<PmdOpnaStereoSample> warmup(
			static_cast<std::size_t>(subject.status().native_sample_rate) * 10U
		);
		subject.render_native(warmup);
		std::vector<PmdOpnaStereoSample> pcm(
			static_cast<std::size_t>(subject.status().native_sample_rate) * 10U
		);
		subject.render_native(pcm);
		hashes[route] = sample_hash(pcm);
		const auto route_nonzero = std::count_if(
			pcm.begin(), pcm.end(), [](const auto &sample) {
				return sample.left != 0 || sample.right != 0;
			}
		);
		expect(
			route_nonzero != 0,
			"each authored FM/SSG/rhythm route reaches stereo PCM"
		);
	}
	constexpr std::array<std::uint32_t, 9U> expected_hashes{
		1'961'029'913U, 4'203'322'885U, 1'386'951'077U,
		71'659'365U, 1'708'591'201U, 149'015'673U,
		3'266'068'125U, 3'834'310'849U, 236'522'255U,
	};
	expect(
		hashes == expected_hashes,
		"FM1-6, SSG1-2, and decoded rhythm stems are byte-deterministic"
	);
}

void test_authentic_effect_bank(const std::filesystem::path &yume1_root)
{
	const auto song = read_external_file(yume1_root / "OP.M");
	const auto bank = read_external_file(yume1_root / "YUME.EFC");
	const auto caller_bank = bank;
	expect(!song.empty(), "authentic EFC gate song is readable");
	expect(!bank.empty(), "authentic EFC bank is readable");
	if(song.empty() || bank.empty()) {
		return;
	}
	auto created = PmdOpnaEngine::create(PmdOpnaConfig{
		.input_clock_hz = 7'987'200U,
		.fidelity = PmdOpnaFidelity::minimum,
	});
	expect(static_cast<bool>(created), "authentic EFC gate engine is created");
	if(!created) {
		return;
	}
	auto subject = std::move(*created);
	expect(
		subject.load_song(song) && subject.load_effect_bank(bank) &&
			subject.play(),
		"authentic song and EFC bank load and start"
	);
	Trace trace;
	subject.set_register_trace_sink(&trace);
	std::size_t effects_passed = 0U;
	std::array<PmdOpnaStereoSample, 4'096U> scratch{};
	for(std::uint8_t effect = 1U; effect <= 22U; ++effect) {
		expect(static_cast<bool>(subject.play_effect(effect)), "authentic EFC entry starts");
		const auto deadline = subject.status().samples_rendered +
			(static_cast<std::uint64_t>(subject.status().native_sample_rate) * 5U);
		while(subject.status().active_effect &&
			subject.status().samples_rendered < deadline) {
			subject.render_native(scratch);
		}
		expect(!subject.status().active_effect, "authentic EFC entry terminates");
		if(!subject.status().active_effect) {
			++effects_passed;
		}
	}
	expect(effects_passed == 22U, "all 22 authored TH03 EFC entries pass");
	expect(trace.fm6_key_ons != 0U, "authentic EFC bank keys on OPNA FM6");
	expect(trace.fm6_key_offs != 0U, "authentic EFC bank releases OPNA FM6");
	expect(bank == caller_bank, "authentic EFC playback preserves caller bytes");
	std::cout << "authentic_effects_passed=" << effects_passed
		<< " fm6_key_ons=" << trace.fm6_key_ons << '\n';
}

} // namespace

int main(int argc, char **argv)
{
	test_config_and_load_validation();
	test_effect_bank_validation_is_transactional();
	test_effect_transport_and_timer_a_ownership();
	test_effect_pcm_and_registers_are_deterministic();
	test_load_is_chip_silent_and_play_is_traced();
	test_pcm_and_fresh_replay_are_deterministic();
	test_rhythm_rom_is_owned_and_audible();
	test_decoded_rhythm_is_owned_and_audible();
	test_instances_and_preplay_silence_are_independent();
	if((argc == 3 || argc == 4) &&
		std::string_view(argv[1]) == "--authentic") {
		test_authentic_corpus(argv[2]);
		std::optional<ExternalRhythmPcm> rhythm_pcm{};
		if(argc == 4) {
			rhythm_pcm = read_external_rhythm_pcm(argv[3]);
			expect(rhythm_pcm.has_value(), "external T98 rhythm profile is readable");
		}
		test_authentic_title_mix(
			argv[2], rhythm_pcm ? rhythm_pcm->view() : PmdOpnaRhythmPcm{}
		);
		if(rhythm_pcm) {
			test_authentic_complete_voice_routes(argv[2], rhythm_pcm->view());
		}
		test_authentic_effect_bank(argv[2]);
	} else if(argc != 1) {
		std::cerr << "usage: th03-audio-pmd-tests "
			"[--authentic <YUME1.DAT> [<T98RHYTHM.PCM>]]\n";
		return 2;
	}

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
