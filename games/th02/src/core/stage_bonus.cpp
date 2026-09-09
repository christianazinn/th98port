#include "th02/core/stage_bonus.hpp"

#include "th02/core/bomb.hpp"
#include "th02/core/item.hpp"
#include "th02/core/player.hpp"

#include <bit>

namespace th02::core {
namespace {

inline constexpr std::int16_t stage_clear_bonus_sum_max = 25600;

[[nodiscard]] std::int16_t word_from_i32(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] std::int16_t word_add(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	return word_from_i32(static_cast<std::int32_t>(lhs) + rhs);
}

[[nodiscard]] std::int16_t word_subtract(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	return word_from_i32(static_cast<std::int32_t>(lhs) - rhs);
}

[[nodiscard]] std::int16_t word_multiply(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	return word_from_i32(static_cast<std::int32_t>(lhs) * rhs);
}

[[nodiscard]] std::int16_t word_divide_trunc0(std::int16_t value,
	std::int16_t divisor) noexcept
{
	return static_cast<std::int16_t>(value / divisor);
}

[[nodiscard]] std::int32_t dword_from_i64(std::int64_t value) noexcept
{
	return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

[[nodiscard]] std::int32_t dword_add(std::int32_t lhs,
	std::int32_t rhs) noexcept
{
	return dword_from_i64(static_cast<std::int64_t>(lhs) + rhs);
}

[[nodiscard]] std::int32_t dword_multiply(std::int32_t lhs,
	std::int32_t rhs) noexcept
{
	return dword_from_i64(static_cast<std::int64_t>(lhs) * rhs);
}

[[nodiscard]] std::int16_t nonnegative(std::int16_t value) noexcept
{
	return value < 0 ? 0 : value;
}

void sum_row(StageClearBonusTally &tally, std::size_t index) noexcept
{
	tally.sum = word_add(tally.sum,
		word_divide_trunc0(tally.row_values_x10[index], 10));
}

} // namespace

StageClearBonusResult stage_clear_bonus_apply(
	ScoreState &score, std::int8_t rank, const PlayerMissState &miss,
	const BombState &bomb, const ItemLifecycleState &items,
	std::uint8_t start_lives, StageClearBonusTally &tally
) noexcept
{
	tally = {};
	tally.row_values_x10[0U] = word_multiply(
		static_cast<std::int16_t>(rank), 2000);
	tally.row_values_x10[1U] = word_multiply(
		word_add(miss.playperf, 16), 200);
	tally.row_values_x10[2U] = nonnegative(word_subtract(2500,
		word_multiply(static_cast<std::int16_t>(bomb.stage_bombs_used), 500)));
	tally.row_values_x10[3U] = nonnegative(word_subtract(3000,
		word_multiply(static_cast<std::int16_t>(miss.stage_miss_count), 1000)));
	tally.row_values_x10[4U] = word_multiply(word_subtract(4,
		static_cast<std::int16_t>(miss.start_bombs)), 800);
	tally.row_values_x10[5U] = word_multiply(word_subtract(4,
		static_cast<std::int16_t>(start_lives)), 1000);
	for(std::size_t row = 0U; row < tally.row_values_x10.size(); ++row) {
		sum_row(tally, row);
	}
	if(tally.sum > stage_clear_bonus_sum_max) {
		tally.sum = stage_clear_bonus_sum_max;
	}

	tally.sum_x10 = dword_multiply(tally.sum, 10);
	tally.total_x10 = dword_multiply(tally.sum_x10,
		static_cast<std::int32_t>(items.point_items_collected));
	tally.bonus_delta = tally.total_x10 / 10;
	score.score_delta = dword_add(score.score_delta, tally.bonus_delta);
	const auto score_events = score_grant_current_delta_as_bonus(score);
	if(!score_events.has_value()) {
		return StageClearBonusResult::source_score_grant_fault;
	}
	tally.score_events = *score_events;
	return StageClearBonusResult::completed;
}

} // namespace th02::core
