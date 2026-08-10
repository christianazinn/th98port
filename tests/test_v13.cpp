#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/core/game.hpp"
#include "th03/error.hpp"
#include "th03/formats/replay.hpp"
#include "th03/formats/v13.hpp"

namespace {

enum class Scenario {
	story,
	versus,
	practice,
};

int checks = 0;
int failures = 0;

void check(bool condition, const std::string &message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void put_u8(std::vector<std::byte> &bytes, std::size_t offset, std::uint8_t value)
{
	bytes[offset] = static_cast<std::byte>(value);
}

void put_u16(std::vector<std::byte> &bytes, std::size_t offset, std::uint16_t value)
{
	put_u8(bytes, offset, static_cast<std::uint8_t>(value));
	put_u8(bytes, offset + 1U, static_cast<std::uint8_t>(value >> 8U));
}

void put_u32(std::vector<std::byte> &bytes, std::size_t offset, std::uint32_t value)
{
	put_u8(bytes, offset, static_cast<std::uint8_t>(value));
	put_u8(bytes, offset + 1U, static_cast<std::uint8_t>(value >> 8U));
	put_u8(bytes, offset + 2U, static_cast<std::uint8_t>(value >> 16U));
	put_u8(bytes, offset + 3U, static_cast<std::uint8_t>(value >> 24U));
}

std::uint32_t get_u32(const std::vector<std::byte> &bytes, std::size_t offset)
{
	return std::to_integer<std::uint32_t>(bytes[offset]) |
		(std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 8U) |
		(std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 16U) |
		(std::to_integer<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

void put_magic(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::string_view magic
)
{
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		put_u8(bytes, offset + index, static_cast<std::uint8_t>(magic[index]));
	}
}

void put_packed_decimal(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::uint32_t value
)
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		put_u8(bytes, offset + index, static_cast<std::uint8_t>(
			(value % 10U) | (((value / 10U) % 10U) << 4U)
		));
		value /= 100U;
	}
}

std::size_t expected_input_offset(Scenario scenario)
{
	const auto capacity = (scenario == Scenario::story)
		? 15U : (scenario == Scenario::practice ? 5U : 3U);
	return th03::formats::replay_v13_header_size +
		(capacity * th03::formats::replay_v13_checkpoint_size);
}

std::vector<std::byte> make_v13(Scenario scenario)
{
	const bool story = scenario == Scenario::story;
	const bool practice = scenario == Scenario::practice;
	const auto mode = story ? th03::game_mode::story : th03::game_mode::vs_1p_cpu;
	const auto stage = practice ? 6U : 0U;
	const auto input_offset = expected_input_offset(scenario);
	constexpr std::size_t input_size = 11U;
	std::vector<std::byte> bytes(input_offset + input_size, std::byte{0});
	put_magic(bytes, 0U, "T3RPLY13");
	put_u16(bytes, 0x08U, 13U);
	put_u16(bytes, 0x0AU, 622U);
	put_u16(bytes, 0x0CU, 0U);
	put_u16(bytes, 0x0EU, practice ? 0x0007U : 0x0003U);
	put_u8(bytes, 0x10U, 2U);
	put_u8(bytes, 0x11U, 2U);
	put_u8(bytes, 0x12U, mode);
	put_u8(bytes, 0x13U, story ? 2U : 0U);
	put_u8(bytes, 0x14U, 0U);
	put_u8(bytes, 0x15U, 1U);
	put_u8(bytes, 0x16U, 4U);
	put_u8(bytes, 0x17U, static_cast<std::uint8_t>(stage));
	put_u8(bytes, 0x18U, 0U);
	put_u8(bytes, 0x19U, 1U);
	put_u32(bytes, 0x1AU, 2U);
	put_u32(bytes, 0x1EU, 2U);
	put_u32(bytes, 0x22U, 29U);
	put_u32(bytes, 0x26U, 0x89ABCDEFU);
	put_u32(bytes, 0x2AU, static_cast<std::uint32_t>(input_offset));
	put_u32(bytes, 0x2EU, static_cast<std::uint32_t>(input_size));
	put_u32(bytes, 0x32U, 622U);
	put_u32(bytes, 0x36U, 289U);
	put_u16(bytes, 0x3AU, 0x007FU);
	put_u8(bytes, 0x3CU, 2U);
	put_u8(bytes, 0x3DU, mode);
	put_u8(bytes, 0x3EU, static_cast<std::uint8_t>(stage));
	put_u8(bytes, 0x3FU, 0U);
	put_u8(bytes, 0x40U, 0U);
	put_u8(bytes, 0x41U, practice ? 2U : 0U);
	put_u8(bytes, 0x42U, 0xFFU);
	put_u8(bytes, 0x43U, story ? 1U : 0U);

	if(practice) {
		put_u8(bytes, 0x44U, 1U);
		put_u8(bytes, 0x45U, 6U);
		put_u8(bytes, 0x46U, 0U);
		put_u8(bytes, 0x47U, 2U);
		put_u8(bytes, 0x48U, 0U);
		put_u8(bytes, 0x49U, 1U);
		put_u8(bytes, 0x4AU, 0U);
		put_u8(bytes, 0x4BU, 0U);
		put_u8(bytes, 0x4CU, 7U);
		put_u8(bytes, 0x4DU, 8U);
		put_u8(bytes, 0x4EU, 3U);
		put_u8(bytes, 0x4FU, 2U);
		put_u16(bytes, 0x50U, 333U);
		put_u8(bytes, 0x52U, 64U);
		put_u8(bytes, 0x53U, 80U);
	} else {
		for(std::size_t index = 0U; index < 9U; ++index) {
			const auto opponent = story ? (index == stage ? 4U : 1U) : 0U;
			put_u8(bytes, 0x44U + index, static_cast<std::uint8_t>(opponent));
			put_packed_decimal(bytes, 0x4DU + (index * 4U), 0U);
		}
	}
	put_packed_decimal(bytes, 0x71U, 12'345'678U);
	put_u8(bytes, 0x75U, 0U);
	put_u16(bytes, 0x76U, static_cast<std::uint16_t>(
		((2026U - 1980U) << 9U) | (8U << 5U) | 9U
	));
	put_magic(bytes, 0x78U, "SYNTH   ");

	put_u8(bytes, 0x80U, 0x7FU);
	put_u8(bytes, 0x81U, 1U);
	const auto stage_round = static_cast<std::uint8_t>(
		practice ? 0x06U : (story ? 0x00U : 0x0FU)
	);
	put_u8(bytes, 0x82U, stage_round);
	put_u8(bytes, 0x83U, 0x20U);
	put_packed_decimal(bytes, 0x84U, 1'234U);
	put_packed_decimal(bytes, 0x88U, 5'678U);
	put_u32(bytes, 0x8CU, 1'234U);
	if(story) {
		put_packed_decimal(bytes, 0x1FCU, 12'000U);
		put_u8(bytes, 0x200U, 2U);
		put_u8(bytes, 0x201U, 1U);
	}
	put_u32(bytes, 0x256U, 100U);
	put_u32(bytes, 0x25AU, 25U);
	put_u8(bytes, 0x25EU, 1U);
	put_u8(bytes, 0x25FU, stage_round);

	constexpr std::size_t checkpoint = 622U;
	constexpr std::size_t compact = checkpoint + 12U;
	put_u32(bytes, compact + 0U, 29U);
	put_u32(bytes, compact + 4U, 0x89ABCDEFU);
	put_u8(bytes, compact + 8U, story ? 2U : 0U);
	put_u8(bytes, compact + 9U, 0U);
	put_u8(bytes, compact + 10U, mode);
	put_u8(bytes, compact + 11U, static_cast<std::uint8_t>(stage));
	put_u8(bytes, compact + 12U, practice ? 2U : 0U);
	put_u8(bytes, compact + 19U, 0U);
	put_u8(bytes, compact + 20U, 1U);
	put_u8(bytes, compact + 21U, 1U);
	put_u8(bytes, compact + 22U, 4U);
	if(story) {
		for(std::size_t index = 0U; index < 9U; ++index) {
			put_u8(bytes, compact + 23U + index, index == stage ? 4U : 1U);
		}
	}
	put_u8(bytes, compact + 48U, 0U);
	put_u16(bytes, compact + 49U, 0x1234U);
	put_u16(bytes, compact + 51U, 0x5678U);
	put_u16(bytes, compact + 67U, practice ? 1'024U : 0U);
	put_u16(bytes, compact + 69U, practice ? 1'280U : 0U);
	put_u32(bytes, compact + 79U, 29U);
	put_u8(bytes, compact + 83U, 1U);

	constexpr std::size_t round = compact + 84U;
	put_u8(bytes, round + 0U, 0U);
	put_u8(bytes, round + 19U, 0U);
	put_u8(bytes, round + 20U, 0U);
	put_u8(bytes, round + 21U, practice ? 7U : 1U);
	put_u8(bytes, round + 22U, practice ? 8U : 1U);
	put_u8(bytes, round + 23U, practice ? 3U : 0U);
	put_u8(bytes, round + 24U, practice ? 2U : 0U);
	put_u8(bytes, round + 25U, practice ? 0U : 0xFFU);
	put_u16(bytes, round + 26U, practice ? 333U : 0U);
	put_u16(bytes, round + 28U, practice ? 333U : 0U);

	const auto input = input_offset;
	put_u8(bytes, input + 0U, 0x01U);
	put_u8(bytes, input + 1U, 0x0FU);
	put_u16(bytes, input + 2U, 0x0020U);
	put_u16(bytes, input + 4U, 0x0040U);
	put_u16(bytes, input + 6U, 0x1000U);
	put_u8(bytes, input + 8U, 0U);
	put_u8(bytes, input + 9U, 0x80U);
	put_u8(bytes, input + 10U, 0xA5U);
	return bytes;
}

std::vector<std::byte> make_v13_two_checkpoints()
{
	auto bytes = make_v13(Scenario::story);
	const auto input = expected_input_offset(Scenario::story);
	bytes.resize(input + 20U, std::byte{0});
	put_u32(bytes, 0x2EU, 20U);
	put_u8(bytes, input + 0U, 0x00U);
	put_u8(bytes, input + 1U, 0x0FU);
	put_u16(bytes, input + 2U, 0x0020U);
	put_u16(bytes, input + 4U, 0x0040U);
	put_u16(bytes, input + 6U, 0x1000U);
	put_u8(bytes, input + 8U, 0U);
	put_u8(bytes, input + 9U, 0x00U);
	put_u8(bytes, input + 10U, 0x0FU);
	put_u16(bytes, input + 11U, 0x0020U);
	put_u16(bytes, input + 13U, 0x0040U);
	put_u16(bytes, input + 15U, 0x1000U);
	put_u8(bytes, input + 17U, 0U);
	put_u8(bytes, input + 18U, 0x80U);
	put_u8(bytes, input + 19U, 0xA5U);

	constexpr std::size_t first_checkpoint = 622U;
	constexpr std::size_t second_checkpoint =
		first_checkpoint + th03::formats::replay_v13_checkpoint_size;
	std::copy_n(
		bytes.begin() + static_cast<std::ptrdiff_t>(first_checkpoint),
		th03::formats::replay_v13_checkpoint_size,
		bytes.begin() + static_cast<std::ptrdiff_t>(second_checkpoint)
	);
	put_u32(bytes, second_checkpoint + 0U, 1U);
	put_u32(bytes, second_checkpoint + 4U, 1U);
	put_u32(bytes, second_checkpoint + 8U, 9U);
	// Story changes P2 to this stage's scenario opponent before capturing the
	// checkpoint; the base header character pair only describes the initial stage.
	put_u8(bytes, 0x44U + 1U, 5U);
	put_u8(bytes, first_checkpoint + 12U + 23U + 1U, 5U);
	put_u8(bytes, second_checkpoint + 12U + 23U + 1U, 5U);
	put_u8(bytes, second_checkpoint + 12U + 11U, 1U);
	put_u8(bytes, second_checkpoint + 12U + 22U, 5U);
	put_u32(bytes, second_checkpoint + 12U + 79U, 0x76543210U);
	put_u8(bytes, second_checkpoint + 12U + 84U, 0U);
	put_u8(bytes, 0x25EU, 2U);
	put_u8(bytes, 0x260U, 0x01U);
	return bytes;
}

std::vector<std::byte> make_v13_practice_two_checkpoints()
{
	auto bytes = make_v13(Scenario::practice);
	const auto input = expected_input_offset(Scenario::practice);
	bytes.resize(input + 20U, std::byte{0});
	put_u32(bytes, 0x2EU, 20U);
	for(const auto packet : {input, input + 9U}) {
		put_u8(bytes, packet + 0U, 0x00U);
		put_u8(bytes, packet + 1U, 0x0FU);
		put_u16(bytes, packet + 2U, 0x0020U);
		put_u16(bytes, packet + 4U, 0x0040U);
		put_u16(bytes, packet + 6U, 0x1000U);
		put_u8(bytes, packet + 8U, 0U);
	}
	put_u8(bytes, input + 18U, 0x80U);
	put_u8(bytes, input + 19U, 0xA5U);

	constexpr std::size_t first_checkpoint = 622U;
	constexpr std::size_t second_checkpoint =
		first_checkpoint + th03::formats::replay_v13_checkpoint_size;
	std::copy_n(
		bytes.begin() + static_cast<std::ptrdiff_t>(first_checkpoint),
		th03::formats::replay_v13_checkpoint_size,
		bytes.begin() + static_cast<std::ptrdiff_t>(second_checkpoint)
	);
	put_u32(bytes, second_checkpoint + 0U, 1U);
	put_u32(bytes, second_checkpoint + 4U, 1U);
	put_u32(bytes, second_checkpoint + 8U, 9U);
	put_u8(bytes, second_checkpoint + 12U + 84U, 1U);
	put_u8(bytes, 0x25EU, 2U);
	put_u8(bytes, 0x260U, 0x16U);
	return bytes;
}

void put_full_input_packet(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::uint8_t tag,
	std::uint16_t system
)
{
	put_u8(bytes, offset + 0U, tag);
	put_u8(bytes, offset + 1U, 0x0FU);
	put_u16(bytes, offset + 2U, 0x0020U);
	put_u16(bytes, offset + 4U, 0x0040U);
	put_u16(bytes, offset + 6U, system);
	put_u8(bytes, offset + 8U, 0U);
}

std::vector<std::byte> make_v13_process_sequence()
{
	auto bytes = make_v13(Scenario::story);
	const auto input = expected_input_offset(Scenario::story);
	bytes.resize(input + 33U, std::byte{0});
	put_u32(bytes, 0x1AU, 3U);
	put_u32(bytes, 0x1EU, 3U);
	put_u32(bytes, 0x2EU, 33U);
	put_full_input_packet(bytes, input + 0U, 0x00U, 0U);
	put_u8(bytes, input + 9U, 0x80U);
	put_u8(bytes, input + 10U, 0xA5U);
	put_full_input_packet(bytes, input + 11U, 0x40U, 0x1000U);
	put_u8(bytes, input + 20U, 0x81U);
	put_u8(bytes, input + 21U, 0xA5U);
	put_full_input_packet(bytes, input + 22U, 0x00U, 0U);
	put_u8(bytes, input + 31U, 0x80U);
	put_u8(bytes, input + 32U, 0xA5U);
	return bytes;
}

std::vector<std::byte> make_v13_pending(Scenario scenario)
{
	auto bytes = make_v13(scenario);
	const auto input = expected_input_offset(scenario);
	// A periodic flush seals and commits the current RLE packet, but does not
	// emit a process-boundary control for the process that is still running.
	bytes.resize(input + 9U);
	put_u8(bytes, 0x10U, static_cast<std::uint8_t>(
		th03::formats::ReplayStatus::recording
	));
	put_u8(bytes, 0x11U, static_cast<std::uint8_t>(
		th03::formats::ReplayEndReason::partial
	));
	put_u32(bytes, 0x2EU, 9U);
	return bytes;
}

void set_versus_mode(std::vector<std::byte> &bytes, std::uint8_t mode)
{
	put_u8(bytes, 0x12U, mode);
	put_u8(bytes, 0x3DU, mode);
	put_u8(bytes, 622U + 12U + 10U, mode);
	const auto p1_cpu = mode == th03::game_mode::vs_cpu_cpu ? 1U : 0U;
	const auto p2_cpu = mode == th03::game_mode::vs_1p_2p ? 0U : 1U;
	put_u8(bytes, 0x18U, static_cast<std::uint8_t>(p1_cpu));
	put_u8(bytes, 0x19U, static_cast<std::uint8_t>(p2_cpu));
	put_u8(bytes, 622U + 12U + 19U, static_cast<std::uint8_t>(p1_cpu));
	put_u8(bytes, 622U + 12U + 20U, static_cast<std::uint8_t>(p2_cpu));
}

void expect_rejected(
	std::vector<std::byte> bytes,
	std::string_view message,
	std::optional<th03::ErrorCode> code = std::nullopt
)
{
	const auto parsed = th03::formats::parse_v13_replay(bytes);
	check(!parsed, std::string(message));
	if(!parsed && code) {
		check(parsed.error().code == *code, std::string(message) + " error code");
	}
}

void test_valid_scenarios()
{
	check(
		th03::formats::replay_can_be_verified(th03::formats::ReplayDocument{}),
		"a version-neutral document without status metadata remains verifiable"
	);
	auto story = make_v13(Scenario::story);
	check(
		get_u32(story, 0x2AU) == 4'957U,
		"V13 Story uses source-backed input offset 622 + 15*289 = 4957"
	);
	auto parsed = th03::formats::parse_v13_replay(story);
	check(parsed.has_value(), "synthetic V13 Story replay parses");
	if(parsed) {
		check(
			parsed->session.source_version == 13U &&
			parsed->session.sample_count == 2U && parsed->session.records.size() == 3U,
			"V13 Story produces a neutral replay session"
		);
		check(
			parsed->metadata &&
			parsed->metadata->status == th03::formats::ReplayStatus::finalized &&
			parsed->metadata->end_reason ==
				th03::formats::ReplayEndReason::menu_return &&
			parsed->metadata->story.has_value() &&
			!parsed->metadata->practice.has_value(),
			"V13 preserves base Story metadata outside GameInit"
		);
		check(
			parsed->expected_round_splits.size() == 1U &&
			parsed->expected_round_splits[0].real_frames == 1'234U,
			"V13 preserves per-round real-frame telemetry"
		);
		if(!parsed->expected_round_splits.empty()) {
			auto simulated = parsed->expected_round_splits[0];
			simulated.real_frames.reset();
			check(
				th03::formats::round_splits_match(
					parsed->expected_round_splits[0], simulated
				),
				"host real-time telemetry is excluded from gameplay split matching"
			);
		}
		check(
			parsed->stage_clear_bonuses &&
			(*parsed->stage_clear_bonuses)[0].max_combo == 2U &&
			(*parsed->stage_clear_bonuses)[0].gauge_attacks == 1U &&
			parsed->slowdown && parsed->slowdown->timed_frames == 100U &&
			parsed->slowdown->slow_frames == 25U,
			"V13 preserves clear-bonus and slowdown telemetry"
		);
	}
	const auto dispatched = th03::formats::parse_replay(story);
	check(
		dispatched && dispatched->session.source_version == 13U &&
		th03::formats::replay_can_be_verified(*dispatched),
		"version-neutral replay dispatch accepts T3RPLY13"
	);
	auto with_tail = story;
	with_tail.insert(with_tail.end(), 17U, std::byte{0x5A});
	check(
		th03::formats::parse_v13_replay(with_tail).has_value(),
		"V13 ignores optional bytes appended after the logical input end"
	);
	check(
		[&]() {
			const auto two = th03::formats::parse_v13_replay(
				make_v13_two_checkpoints()
			);
			return two && two->checkpoints.size() == 2U &&
				two->checkpoints[1].stage == 1U &&
				two->checkpoints[1].round == 0U &&
				two->checkpoints[1].sample_index == 1U &&
				two->checkpoints[1].global_frame == 1U &&
				two->checkpoints[1].input_byte_cursor == 9U &&
				two->checkpoints[1].init.compact_runtime_snapshot &&
				two->checkpoints[1].init.compact_runtime_snapshot
					->round_reset_seed == 0x76543210U;
		}(),
		"V13 accepts a later Story-stage checkpoint with its scenario opponent"
	);

	auto versus = make_v13(Scenario::versus);
	check(
		get_u32(versus, 0x2AU) == 1'489U,
		"V13 VS uses source-backed input offset 622 + 3*289 = 1489"
	);
	parsed = th03::formats::parse_v13_replay(versus);
	check(
		parsed && !parsed->practice && parsed->metadata &&
		!parsed->metadata->story && !parsed->metadata->practice &&
		parsed->expected_round_splits[0].story_stage == 0x0FU,
		"V13 ordinary VS preserves no scenario-union interpretation"
	);
	for(const auto mode : {
		th03::game_mode::vs_1p_2p,
		th03::game_mode::vs_cpu_cpu,
	}) {
		auto variant = versus;
		set_versus_mode(variant, mode);
		const auto variant_parsed = th03::formats::parse_v13_replay(variant);
		check(
			variant_parsed && get_u32(variant, 0x2AU) == 1'489U,
			"every ordinary V13 VS mode uses the three-checkpoint reservation"
		);
	}

	auto practice = make_v13(Scenario::practice);
	check(
		get_u32(practice, 0x2AU) == 2'067U,
		"V13 Practice uses source-backed input offset 622 + 5*289 = 2067"
	);
	parsed = th03::formats::parse_v13_replay(practice);
	check(
		parsed && parsed->practice && parsed->metadata &&
		parsed->metadata->practice &&
		parsed->metadata->practice->p1_gauge == 64U &&
		parsed->metadata->practice->cpu_gauge == 80U &&
		parsed->session.init.practice_rules.has_value(),
		"V13 Practice metadata becomes a neutral retry policy"
	);
	if(parsed && parsed->session.init.compact_runtime_snapshot) {
		check(
			parsed->session.init.compact_runtime_snapshot->players[0].gauge_avail ==
				1'024U &&
			parsed->session.init.compact_runtime_snapshot->players[1].gauge_avail ==
				1'280U,
			"V13 Practice gauges agree with checkpoint 0"
		);
	}
	const auto practice_two = th03::formats::parse_v13_replay(
		make_v13_practice_two_checkpoints()
	);
	check(
		practice_two && practice_two->checkpoints.size() == 2U &&
		practice_two->checkpoints[0].init.practice_rules &&
		practice_two->checkpoints[1].init.practice_rules &&
		practice_two->checkpoints[1].init.practice_rules->uses_stock &&
		practice_two->checkpoints[1].init.practice_rules->retry_cpu_timer ==
			th03::CpuTimerPolicy::story_native,
		"V13 attaches the validated Practice retry policy to every checkpoint"
	);

	const auto process_sequence = th03::formats::parse_replay(
		make_v13_process_sequence()
	);
	check(
		process_sequence && process_sequence->session.records.size() == 6U &&
		process_sequence->session.sample_count == 3U,
		"V13 dispatch decodes MAIN-to-MAINL-to-MAIN controls and interstitial input"
	);

	auto pending = make_v13_pending(Scenario::story);
	const auto pending_parsed = th03::formats::parse_replay(pending);
	check(
		pending_parsed && pending_parsed->metadata &&
		pending_parsed->metadata->status ==
			th03::formats::ReplayStatus::recording &&
		pending_parsed->session.records.size() == 2U &&
		!th03::formats::replay_can_be_verified(*pending_parsed),
		"V13 accepts but cannot verify a periodically flushed recording without a terminal control"
	);
	auto recording_with_control = make_v13(Scenario::story);
	put_u8(recording_with_control, 0x10U, static_cast<std::uint8_t>(
		th03::formats::ReplayStatus::recording
	));
	const auto recording_with_control_parsed =
		th03::formats::parse_v13_replay(recording_with_control);
	check(
		recording_with_control_parsed &&
		!th03::formats::replay_can_be_verified(*recording_with_control_parsed),
		"a control-terminated V13 recording still cannot report verification success"
	);
	put_u8(pending, 0x10U, static_cast<std::uint8_t>(
		th03::formats::ReplayStatus::partial
	));
	expect_rejected(
		pending,
		"V13 partial replay requires a terminal process control"
	);
	auto partial_with_control = make_v13(Scenario::story);
	put_u8(partial_with_control, 0x10U, static_cast<std::uint8_t>(
		th03::formats::ReplayStatus::partial
	));
	put_u8(partial_with_control, 0x11U, static_cast<std::uint8_t>(
		th03::formats::ReplayEndReason::partial
	));
	check(
		th03::formats::parse_v13_replay(partial_with_control).has_value(),
		"V13 accepts a control-terminated partial replay"
	);
	put_u8(pending, 0x10U, static_cast<std::uint8_t>(
		th03::formats::ReplayStatus::finalized
	));
	put_u8(pending, 0x11U, static_cast<std::uint8_t>(
		th03::formats::ReplayEndReason::complete
	));
	expect_rejected(
		std::move(pending),
		"V13 finalized replay still requires a terminal process control"
	);
}

void test_header_and_metadata_corruption()
{
	auto bytes = make_v13(Scenario::story);
	bytes.resize(621U);
	expect_rejected(std::move(bytes), "V13 rejects a truncated fixed header", th03::ErrorCode::truncated);

	bytes = make_v13(Scenario::story);
	put_u16(bytes, 0x08U, 12U);
	expect_rejected(std::move(bytes), "V13 rejects a magic/version mismatch");
	bytes = make_v13(Scenario::story);
	put_u16(bytes, 0x0AU, 614U);
	expect_rejected(std::move(bytes), "V13 rejects a non-622-byte header");
	bytes = make_v13(Scenario::story);
	put_u16(bytes, 0x0EU, 0x000BU);
	expect_rejected(std::move(bytes), "V13 rejects unknown replay flag bits");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x10U, static_cast<std::uint8_t>(
		th03::formats::ReplayStatus::empty
	));
	expect_rejected(std::move(bytes), "V13 rejects an empty index-only status");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x10U, static_cast<std::uint8_t>(
		th03::formats::ReplayStatus::error
	));
	expect_rejected(std::move(bytes), "V13 rejects an error index-only status");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x10U, 5U);
	expect_rejected(std::move(bytes), "V13 rejects an unknown status");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x11U, 7U);
	expect_rejected(std::move(bytes), "V13 rejects an unknown end reason");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x12U, th03::game_mode::demo);
	expect_rejected(std::move(bytes), "V13 rejects Demo mode");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x13U, 4U);
	expect_rejected(std::move(bytes), "V13 rejects rank outside 0..3");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x14U, 3U);
	expect_rejected(std::move(bytes), "V13 rejects key mode outside 0..2");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x15U, 0U);
	expect_rejected(std::move(bytes), "V13 rejects an invalid paletted character");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x16U, 5U);
	expect_rejected(
		std::move(bytes),
		"V13 rejects a base Story P2 differing from the initial stage opponent"
	);
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x18U, 2U);
	expect_rejected(std::move(bytes), "V13 rejects a non-Boolean CPU flag");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x18U, 1U);
	expect_rejected(std::move(bytes), "V13 Story rejects a CPU-controlled P1");
	bytes = make_v13(Scenario::versus);
	put_u8(bytes, 0x19U, 0U);
	expect_rejected(std::move(bytes), "V13 1P-vs-CPU rejects a human P2");
	bytes = make_v13(Scenario::versus);
	set_versus_mode(bytes, th03::game_mode::vs_1p_2p);
	put_u8(bytes, 0x19U, 1U);
	expect_rejected(std::move(bytes), "V13 1P-vs-2P rejects a CPU P2");
	bytes = make_v13(Scenario::versus);
	set_versus_mode(bytes, th03::game_mode::vs_cpu_cpu);
	put_u8(bytes, 0x18U, 0U);
	expect_rejected(std::move(bytes), "V13 CPU-vs-CPU rejects a human P1");
	bytes = make_v13(Scenario::practice);
	put_u8(bytes, 0x19U, 0U);
	expect_rejected(std::move(bytes), "V13 Practice rejects a human P2");
	bytes = make_v13(Scenario::story);
	put_u16(bytes, 0x3AU, 0x003FU);
	expect_rejected(std::move(bytes), "V13 rejects missing base capability bits");
	bytes = make_v13(Scenario::story);
	put_u16(bytes, 0x3AU, 0x017FU);
	expect_rejected(std::move(bytes), "frozen V13 rejects unversioned future capability bits");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x3CU, 3U);
	expect_rejected(std::move(bytes), "V13 rejects an invalid final route");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x71U, 0xFAU);
	expect_rejected(std::move(bytes), "V13 rejects an invalid final packed score");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x75U, 4U);
	expect_rejected(std::move(bytes), "V13 rejects an invalid Autofire mask");
	bytes = make_v13(Scenario::story);
	put_u16(bytes, 0x76U, static_cast<std::uint16_t>((46U << 9U) | (13U << 5U) | 1U));
	expect_rejected(std::move(bytes), "V13 rejects an invalid DOS date");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x78U, static_cast<std::uint8_t>('a'));
	expect_rejected(std::move(bytes), "V13 rejects unsupported saved-name characters");
	bytes = make_v13(Scenario::story);
	for(std::size_t index = 0U; index < 8U; ++index) {
		put_u8(bytes, 0x78U + index, 0U);
	}
	check(
		th03::formats::parse_v13_replay(bytes).has_value(),
		"V13 accepts the all-zero pending-save TH3LAST name"
	);
}

void test_summary_and_checkpoint_corruption()
{
	auto bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x80U, 0x3FU);
	expect_rejected(std::move(bytes), "V13 rejects missing round-summary capabilities");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x81U, 28U);
	expect_rejected(std::move(bytes), "V13 rejects more than 27 round splits");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x82U, 0x0FU);
	expect_rejected(std::move(bytes), "V13 rejects a VS-tagged Story round split");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x83U, 0x30U);
	expect_rejected(std::move(bytes), "V13 rejects a round route outside 0..2");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x83U, 0x22U);
	expect_rejected(std::move(bytes), "V13 rejects an invalid round winner");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x84U, 0xFAU);
	expect_rejected(std::move(bytes), "V13 rejects an invalid round packed score");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x1FCU, 0x01U);
	expect_rejected(std::move(bytes), "V13 rejects a clear-bonus total mismatch");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x1FCU + 10U + 9U, 1U);
	expect_rejected(std::move(bytes), "V13 rejects lives in a non-final clear bonus");
	bytes = make_v13(Scenario::story);
	put_u32(bytes, 0x25AU, 101U);
	expect_rejected(std::move(bytes), "V13 rejects slowdown frames above timed frames");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x25EU, 0U);
	expect_rejected(std::move(bytes), "V13 rejects an empty checkpoint directory");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x25EU, 16U);
	expect_rejected(std::move(bytes), "V13 rejects a checkpoint count above Story capacity");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x25EU, 2U);
	put_u8(bytes, 0x260U, 0U);
	expect_rejected(std::move(bytes), "V13 rejects duplicate checkpoint directory entries");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 0x25FU, 0x0FU);
	expect_rejected(std::move(bytes), "V13 rejects a VS-tagged Story checkpoint");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 622U + 12U + 10U, th03::game_mode::vs_1p_cpu);
	expect_rejected(std::move(bytes), "V13 rejects checkpoint/header mode disagreement");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 622U + 12U + 84U, 1U);
	expect_rejected(std::move(bytes), "V13 rejects checkpoint/directory round disagreement");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, 622U + 12U + 84U + 30U + 21U, 3U);
	expect_rejected(std::move(bytes), "V13 rejects an invalid checkpoint background phase");
	bytes = make_v13(Scenario::story);
	put_u32(bytes, 622U, 1U);
	expect_rejected(std::move(bytes), "V13 rejects a nonzero process-anchor cursor");
	bytes = make_v13(Scenario::story);
	put_u32(bytes, 0x2AU, 4'949U);
	expect_rejected(std::move(bytes), "V13 rejects the documented prose's erroneous -8 Story offset");
	bytes = make_v13(Scenario::story);
	put_u32(bytes, 0x2EU, 12U);
	expect_rejected(std::move(bytes), "V13 rejects an input span beyond EOF");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, expected_input_offset(Scenario::story) + 1U, 0x07U);
	expect_rejected(std::move(bytes), "V13 rejects a missing process-start Charge keyframe");
	bytes = make_v13(Scenario::story);
	put_u8(bytes, expected_input_offset(Scenario::story) + 10U, 0U);
	expect_rejected(std::move(bytes), "V13 rejects an invalid terminal process marker");
	bytes = make_v13_two_checkpoints();
	put_u32(bytes, 622U + 289U + 8U, 8U);
	expect_rejected(std::move(bytes), "V13 rejects a checkpoint byte cursor inside a packet");
	bytes = make_v13_two_checkpoints();
	put_u8(bytes, 622U + 289U + 12U + 22U, 4U);
	expect_rejected(
		std::move(bytes),
		"V13 rejects a later Story checkpoint with the initial P2 character"
	);
	bytes = make_v13_two_checkpoints();
	put_u32(bytes, 622U + 289U + 0U, 2U);
	expect_rejected(std::move(bytes), "V13 rejects a checkpoint sample/byte cursor mismatch");
	bytes = make_v13_two_checkpoints();
	put_u8(bytes, expected_input_offset(Scenario::story) + 10U, 0x07U);
	expect_rejected(std::move(bytes), "V13 requires a full keyframe after each checkpoint cursor");
}

void test_practice_corruption()
{
	auto bytes = make_v13(Scenario::practice);
	put_u8(bytes, 0x52U, 63U);
	expect_rejected(std::move(bytes), "V13 Practice rejects a gauge below 64");
	bytes = make_v13(Scenario::practice);
	put_u8(bytes, 0x54U, 1U);
	expect_rejected(std::move(bytes), "V13 Practice rejects nonzero reserved bytes");
	bytes = make_v13(Scenario::practice);
	put_u8(bytes, 0x45U, 5U);
	expect_rejected(std::move(bytes), "V13 Practice rejects header/checkpoint stage disagreement");
	bytes = make_v13(Scenario::practice);
	put_u16(bytes, 622U + 12U + 67U, 1'023U);
	expect_rejected(std::move(bytes), "V13 Practice rejects checkpoint gauge disagreement");
}

} // namespace

int main()
{
	test_valid_scenarios();
	test_header_and_metadata_corruption();
	test_summary_and_checkpoint_corruption();
	test_practice_corruption();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
