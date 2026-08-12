#include "th03/tools/replay_hydrate.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "../formats/bytes.hpp"
#include "th03/formats/replay.hpp"
#include "th03/formats/v13.hpp"
#include "th03/replay/run.hpp"

namespace th03::tools {
namespace {

constexpr std::size_t round_split_offset = 0x82U;
constexpr std::size_t round_split_size = 14U;
constexpr std::size_t round_real_frames_offset = 10U;
constexpr std::size_t stage_bonus_offset = 0x1FCU;
constexpr std::size_t stage_bonus_size = 10U;
constexpr std::size_t stage_count = 9U;
constexpr std::size_t timed_frames_offset = 0x256U;
constexpr std::size_t slow_frames_offset = 0x25AU;

Error hydration_error(std::string message, std::size_t offset = 0U)
{
	return Error{ErrorCode::invalid_value, std::move(message), offset};
}

bool all_zero(std::span<const std::byte> bytes) noexcept
{
	return std::all_of(bytes.begin(), bytes.end(), [](std::byte value) {
		return value == std::byte{0};
	});
}

void put_packed_decimal(
	std::span<std::byte> bytes,
	std::size_t offset,
	std::uint32_t value
) noexcept
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		formats::detail::put_u8(
			bytes, offset + index, static_cast<std::uint8_t>(
				(value % 10U) | (((value / 10U) % 10U) << 4U)
			)
		);
		value /= 100U;
	}
}

formats::ReplayStageClearBonus make_clear_bonus(
	const MainResultState &summary
) noexcept
{
	formats::ReplayStageClearBonus bonus{};
	bonus.max_combo = summary.winner_stats.combo_hits_max;
	bonus.gauge_attacks = summary.winner_stats.gauge_attacks_fired;
	bonus.boss_attacks = summary.winner_stats.boss_attacks_fired;
	bonus.boss_reversals = summary.winner_stats.boss_attacks_reversed;
	bonus.boss_panics = summary.winner_stats.boss_panics_fired;
	bonus.remaining_lives = (summary.story_stage == 8U)
		? summary.story_lives : 0U;
	return bonus;
}

std::uint32_t clear_bonus_total(
	const formats::ReplayStageClearBonus &bonus
) noexcept
{
	return (static_cast<std::uint32_t>(bonus.max_combo) * 1'000U) +
		(static_cast<std::uint32_t>(bonus.gauge_attacks) * 10'000U) +
		(static_cast<std::uint32_t>(bonus.boss_attacks) * 15'000U) +
		(static_cast<std::uint32_t>(bonus.boss_reversals) * 20'000U) +
		(static_cast<std::uint32_t>(bonus.boss_panics) * 30'000U) +
		(static_cast<std::uint32_t>(bonus.remaining_lives) * 100'000U);
}

void write_clear_bonus(
	std::span<std::byte> bytes,
	std::size_t stage,
	const formats::ReplayStageClearBonus &bonus
) noexcept
{
	const auto offset = stage_bonus_offset + (stage * stage_bonus_size);
	put_packed_decimal(bytes, offset, clear_bonus_total(bonus));
	formats::detail::put_u8(bytes, offset + 4U, bonus.max_combo);
	formats::detail::put_u8(bytes, offset + 5U, bonus.gauge_attacks);
	formats::detail::put_u8(bytes, offset + 6U, bonus.boss_attacks);
	formats::detail::put_u8(bytes, offset + 7U, bonus.boss_reversals);
	formats::detail::put_u8(bytes, offset + 8U, bonus.boss_panics);
	formats::detail::put_u8(bytes, offset + 9U, bonus.remaining_lives);
}

std::expected<void, Error> validate_run(
	const formats::ReplayDocument &document,
	const replay::RunResult &run,
	const GameContent &content
)
{
	if(run.status != replay::RunStatus::complete) {
		return std::unexpected(hydration_error(
			"replay simulation did not consume a complete session"
		));
	}
	if(run.completed_rounds.size() != document.expected_round_splits.size()) {
		return std::unexpected(hydration_error(
			"reconstructed round count does not match the V13 summary"
		));
	}
	for(std::size_t index = 0U; index < run.completed_rounds.size(); ++index) {
		const auto actual = formats::make_round_split(
			run.completed_rounds[index].state, document.practice
		);
		if(!formats::round_splits_match(
			document.expected_round_splits[index], actual
		)) {
			return std::unexpected(hydration_error(
				"reconstructed round does not match the V13 summary", index
			));
		}
	}

	std::vector<replay::RoundStartRecord> expected;
	expected.reserve(document.checkpoints.size());
	const auto options = GameOptions{
		.input_protocol = InputProtocol::recorded_replay,
	};
	for(std::size_t index = 0U; index < document.checkpoints.size(); ++index) {
		const auto &checkpoint = document.checkpoints[index];
		auto game = Game::create(checkpoint.init, content, options);
		if(!game) {
			return std::unexpected(hydration_error(
				"could not reconstruct a V13 checkpoint", index
			));
		}
		expected.push_back(replay::RoundStartRecord{
			.sample_index = checkpoint.sample_index,
			.observation = game->round_start_observation(),
		});
	}
	if(const auto mismatch = replay::first_round_start_mismatch(
		expected, run.round_starts
	)) {
		return std::unexpected(hydration_error(
			"reconstructed round start does not match checkpoint field " +
				mismatch->field,
			mismatch->checkpoint_index
		));
	}
	return {};
}

} // namespace

std::expected<HydratedReplay, Error> hydrate_v13_replay(
	std::span<const std::byte> bytes,
	const GameContent &content
)
{
	auto document = formats::parse_v13_replay(bytes);
	if(!document) {
		return std::unexpected(document.error());
	}
	if(!formats::replay_can_be_verified(*document)) {
		return std::unexpected(hydration_error(
			"only finalized V13 replays can be hydrated"
		));
	}
	auto run = replay::run_session(document->session, content);
	if(!run) {
		return std::unexpected(run.error());
	}
	if(auto valid = validate_run(*document, *run, content); !valid) {
		return std::unexpected(valid.error());
	}

	HydratedReplay hydrated{
		.bytes = std::vector<std::byte>(bytes.begin(), bytes.end()),
		.report = ReplayHydrationReport{
			.source_version = document->session.source_version,
			.rounds_verified = run->completed_rounds.size(),
		},
	};
	const auto output = std::span<std::byte>(hydrated.bytes);
	for(std::size_t index = 0U; index < run->completed_rounds.size(); ++index) {
		const auto offset = round_split_offset + (index * round_split_size) +
			round_real_frames_offset;
		if(formats::detail::u32(output, offset) == 0U) {
			formats::detail::put_u32(
				output, offset, run->completed_rounds[index].real_frames
			);
			if(run->completed_rounds[index].real_frames != 0U) {
				hydrated.report.round_timers_filled++;
			}
		}
	}

	const bool clear_bonus_mode = document->practice ||
		document->session.init.game_mode == game_mode::story;
	if(clear_bonus_mode) {
		for(const auto &round : run->completed_rounds) {
			if(!round.result_summary ||
				round.result_summary->winner != PlayerId::p1 ||
				round.result_summary->story_stage >= stage_count) {
				continue;
			}
			const auto stage = round.result_summary->story_stage;
			const auto offset = stage_bonus_offset + (stage * stage_bonus_size);
			if(!all_zero(output.subspan(offset, stage_bonus_size))) {
				continue;
			}
			const auto bonus = make_clear_bonus(*round.result_summary);
			write_clear_bonus(output, stage, bonus);
			if(!all_zero(output.subspan(offset, stage_bonus_size))) {
				hydrated.report.clear_bonuses_filled++;
			}
		}
	}

	if(
		formats::detail::u32(output, timed_frames_offset) == 0U &&
		formats::detail::u32(output, slow_frames_offset) == 0U
	) {
		std::uint32_t timed_frames = 0U;
		for(const auto &round : run->completed_rounds) {
			timed_frames += round.timed_frames;
		}
		formats::detail::put_u32(output, timed_frames_offset, timed_frames);
		hydrated.report.slowdown_filled = timed_frames != 0U;
	}

	hydrated.report.changed = !std::equal(
		hydrated.bytes.begin(), hydrated.bytes.end(), bytes.begin(), bytes.end()
	);
	if(auto reparsed = formats::parse_v13_replay(hydrated.bytes); !reparsed) {
		return std::unexpected(reparsed.error());
	}
	return hydrated;
}

} // namespace th03::tools
