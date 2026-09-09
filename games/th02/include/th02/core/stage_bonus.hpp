#pragma once

#include "th02/core/score.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

struct BombState;
struct ItemLifecycleState;
struct PlayerMissState;

inline constexpr std::size_t stage_clear_bonus_row_count = 6U;

// Observable arithmetic from the regular stage-clear tally. row_values_x10
// are the displayed arguments; the source language bridge divides each by ten
// before adding `sum`. The *_x10 totals are the two values displayed after
// that sum, before and after multiplication by point-item count.
struct StageClearBonusTally {
	std::array<std::int16_t, stage_clear_bonus_row_count> row_values_x10{};
	std::int16_t sum{};
	std::int32_t sum_x10{};
	std::int32_t total_x10{};
	std::int32_t bonus_delta{};
	ScoreEvents score_events{};
	bool operator==(const StageClearBonusTally &) const = default;
};

enum class StageClearBonusResult : std::uint8_t {
	completed,
	source_score_grant_fault,
};

// Deterministic arithmetic/state slice of
// th02/main/stage/bonus.cpp::stage_clear_bonus_animate. This borrows the
// source owners for play performance/miss/start-bomb, bomb count, and point
// item count. `start_lives` is the resident field with no current native state
// owner. It updates real ScoreState and returns its actual immediate-grant
// events, but intentionally omits bonus text timing, sound reset/update, and
// the replay input-wait boundary. An unsupported native extend-table read
// keeps the source prefix through score_delta's add and returns the fault.
[[nodiscard]] StageClearBonusResult stage_clear_bonus_apply(
	ScoreState &score, std::int8_t rank, const PlayerMissState &miss,
	const BombState &bomb, const ItemLifecycleState &items,
	std::uint8_t start_lives, StageClearBonusTally &tally
) noexcept;

} // namespace th02::core
