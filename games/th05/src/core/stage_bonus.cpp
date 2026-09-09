#include "th05/core/state.hpp"
#include "th05/core/stage_bonus.hpp"

namespace th05::core {
namespace {
std::uint32_t multipliers(const State &s, std::uint32_t points) noexcept
{
	if (s.boss.phase_state == 0) return 0;
	switch (s.credit_lives) {
	case 6: points = (points * 3U) / 10U; break;
	case 5: points = (points * 5U) / 10U; break;
	case 4: points = (points * 7U) / 10U; break;
	}
	switch (s.score.score[0]) {
	case 1: points = (points * 8U) / 10U; break;
	case 2: points = (points * 6U) / 10U; break;
	case 3: points = (points * 4U) / 10U; break;
	}
	switch (s.rank) {
	case 0: points = (points * 5U) / 10U; break;
	case 1: points = (points * 10U) / 10U; break;
	case 2: points = (points * 12U) / 10U; break;
	case 3: points = (points * 14U) / 10U; break;
	}
	return points;
}
}
std::uint32_t stage_clear_bonus(State &s, Presentation &p) noexcept
{
	const bool no_miss = s.miss_count == s.bonus_previous_miss_count;
	s.bonus_previous_miss_count = s.miss_count;
	const bool no_bomb = s.bombs_used == s.bonus_previous_bombs_used && no_miss;
	s.bonus_previous_bombs_used = s.bombs_used;
	s.palette_tone = 60;
	p.palette_show(s.palette_tone);
	std::uint32_t points = static_cast<std::uint16_t>(s.stage_id * 100U + 100U);
	points += static_cast<std::uint16_t>(s.items.dream * 10U);
	points += static_cast<std::uint16_t>(s.bullets.stage_graze * 5U);
	points *= s.items.stage_point_items_collected;
	const auto bonus = static_cast<std::uint16_t>(s.stage_id * 5000U + 10000U);
	if (no_miss) points += bonus;
	if (no_bomb) points += bonus;
	if (points >= 1200000U) playperf_raise(s, 4);
	else if (points >= 800000U) playperf_raise(s, 2);
	else if (points >= 500000U) playperf_raise(s, 1);
	else if (points <= 100000U) playperf_lower(s, 2);
	else if (points <= 200000U) playperf_lower(s, 1);
	points = multipliers(s, points);
	s.score.score_delta += points;
	return points;
}
std::uint32_t stage_allclear_bonus(State &s, Presentation &p) noexcept
{
	const bool no_miss = s.miss_count == s.bonus_previous_miss_count;
	s.bonus_previous_miss_count = s.miss_count;
	const bool no_bomb = s.bombs_used == s.bonus_previous_bombs_used && no_miss;
	s.bonus_previous_bombs_used = s.bombs_used;
	s.palette_tone = 60;
	p.palette_show(s.palette_tone);
	s.items.extends_gained = 10;
	std::uint32_t points = 1000;
	points += static_cast<std::uint16_t>(s.items.dream * 10U);
	points += static_cast<std::uint16_t>(s.bullets.stage_graze * 5U);
	points += static_cast<std::uint16_t>(s.resources.rem_lives * 1000U - 1000U);
	points *= s.items.stage_point_items_collected;
	if (no_miss) points += 50000U;
	if (no_bomb) points += 50000U;
	points += s.items.extend_point_items_collected * 250U;
	points = multipliers(s, points);
	s.score.score_delta += points;
	return points;
}
}
