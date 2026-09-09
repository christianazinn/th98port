#include "th04/core/state.hpp"
#include "th04/core/stage_bonus.hpp"

namespace th04::core {
namespace {
std::uint32_t multipliers(const State &s, std::uint32_t points) noexcept
{
	if (s.boss.phase_state == 0) return 0;
	// Each source multiplication stores a 32-bit result before division.
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
	s.palette_tone = 60;
	p.palette_show(s.palette_tone);
	std::uint32_t points = static_cast<std::uint16_t>(s.stage_id * 100U + 100U);
	points += static_cast<std::uint16_t>(s.power * 5U);
	points += s.items.dream_score;
	points += static_cast<std::uint16_t>(s.bullets.stage_graze * 5U);
	points *= s.items.stage_point_items_collected;
	const auto before = points;
	points = multipliers(s, points);
	s.score.score_delta += points;
	if (before >= 1200000U) playperf_raise(s, 4);
	else if (before >= 800000U) playperf_raise(s, 2);
	else if (before >= 500000U) playperf_raise(s, 1);
	else if (before <= 100000U) playperf_lower(s, 2);
	else if (before <= 200000U) playperf_lower(s, 1);
	++s.bombs;
	p.hud(HudField::bombs);
	if (s.miss_count <= s.stage_id) playperf_raise(s, 2);
	if (s.bombs_used <= s.stage_id * 2U) playperf_raise(s, 2);
	return points;
}
std::uint32_t stage_allclear_bonus(State &s, Presentation &p) noexcept
{
	s.palette_tone = 60;
	p.palette_show(s.palette_tone);
	s.score.extends_gained = 10;
	std::uint32_t points = 1000;
	points += static_cast<std::uint16_t>(s.power * 5U);
	points += s.items.dream_score;
	points += static_cast<std::uint16_t>(s.bullets.stage_graze * 5U);
	const auto value = s.rank == 4 ? 3000U : 1000U;
	points += static_cast<std::uint16_t>(s.resources.rem_lives * value - value);
	points *= s.items.stage_point_items_collected;
	points = multipliers(s, points);
	s.score.score_delta += points;
	return points;
}
}
