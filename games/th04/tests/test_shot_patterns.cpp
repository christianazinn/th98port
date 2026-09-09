#include "th04/core/shot_patterns.hpp"

#include "pc98/masterlib_vector.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace {
using namespace th04::core;

constexpr pc98::th0405::SubpixelPoint player{3000, 4000};
constexpr pc98::th0405::SubpixelPoint option{2800, 3800};

struct Fixture {
	ShotSpawnState spawn{};
	ShotState shots{};
	RandomState random{};
	ReimuAPatternState reimu{};

	Fixture()
	{
		spawn.player_cur = player;
		reimu.homing_target.y = homing_target_none;
	}
};

[[nodiscard]] bool alive_prefix(const ShotState &shots, std::size_t count)
{
	for(std::size_t i = 0; i < shots.shots.size(); ++i) {
		if((shots.shots[i].flag == shot_alive) != (i < count)) return false;
	}
	return true;
}

[[nodiscard]] bool has_velocity(const Shot &shot, std::uint8_t angle, std::int16_t length = 192)
{
	const auto expected = pc98::masterlib::vector2(angle, length);
	return shot.pos.velocity == pc98::th0405::SubpixelPoint{expected.x, expected.y};
}

bool reimu_b_all_levels()
{
	{
		Fixture f;
		f.spawn.shot_time = 18;
		f.random.ring[0] = 9;
		const auto result = shot_reimu_b_l2(f.reimu, f.spawn, f.shots, f.random);
		if(!result.complete() || result.shots_allocated != 3 || result.allocation.record_index != 2 ||
			f.reimu.cycle != 1 || f.random.cursor != 1 || !alive_prefix(f.shots, 3) ||
			f.shots.shots[0].patnum_base != 32 || f.shots.shots[0].pos.cur.x != player.x - 384 ||
			!has_velocity(f.shots.shots[0], 0xB8U) ||
			f.shots.shots[1].pos.cur.x != player.x + 384 || !has_velocity(f.shots.shots[1], 0xC8U) ||
			f.shots.shots[2].patnum_base != pat_shot_reimu || !has_velocity(f.shots.shots[2], 0xC1U)) {
			return false;
		}
	}

	using Pattern = ShotPatternResult (*)(ReimuAPatternState &, ShotSpawnState &, ShotState &) noexcept;
	constexpr std::array<std::pair<Pattern, std::size_t>, 7> patterns{{
		{shot_reimu_b_l3, 4}, {shot_reimu_b_l4, 5}, {shot_reimu_b_l5, 7},
		{shot_reimu_b_l6, 7}, {shot_reimu_b_l7, 7}, {shot_reimu_b_l8, 9},
		{shot_reimu_b_l9, 11},
	}};
	for(const auto &[pattern, count] : patterns) {
		Fixture f;
		f.spawn.shot_time = 18;
		const auto result = pattern(f.reimu, f.spawn, f.shots);
		if(!result.complete() || result.shots_allocated != count ||
			result.allocation.record_index != count - 1 || f.reimu.cycle != 1 ||
			!alive_prefix(f.shots, count) || f.shots.shots[0].patnum_base != 32 ||
			f.shots.shots[0].pos.cur.x != player.x - 384 ||
			f.shots.shots[count - 1].patnum_base != pat_shot_reimu) {
			return false;
		}
	}
	return true;
}

bool reimu_b_cycle_cadence()
{
	Fixture f;
	f.spawn.shot_time = 12;
	f.reimu.cycle = 1;
	const auto later = shot_reimu_b_l5(f.reimu, f.spawn, f.shots);
	if(!later.complete() || later.shots_allocated != 3 || f.reimu.cycle != 2 ||
		!alive_prefix(f.shots, 3) || f.shots.shots[0].patnum_base != pat_shot_reimu) {
		return false;
	}
	f.shots = {};
	f.spawn.shot_time = 18;
	const auto reset = shot_reimu_b_l5(f.reimu, f.spawn, f.shots);
	return reset.complete() && reset.shots_allocated == 7 && f.reimu.cycle == 1 &&
		f.shots.shots[0].patnum_base == 32;
}

bool marisa_a_all_levels()
{
	using Pattern = ShotPatternResult (*)(
		ShotSpawnState &, ShotState &, RandomState &, pc98::th0405::SubpixelPoint
	) noexcept;
	struct Expected { Pattern pattern; std::size_t count; std::uint16_t time; std::uint8_t style; };
	constexpr std::array<Expected, 8> patterns{{
		{shot_marisa_a_l2, 3, 64, 0}, {shot_marisa_a_l3, 4, 72, 0},
		{shot_marisa_a_l4, 4, 88, 1}, {shot_marisa_a_l5, 5, 104, 1},
		{shot_marisa_a_l6, 5, 128, 2}, {shot_marisa_a_l7, 5, 144, 3},
		{shot_marisa_a_l8, 7, 168, 3}, {shot_marisa_a_l9, 7, 192, 4},
	}};
	for(const auto expected : patterns) {
		Fixture f;
		f.random.ring[0] = 6;
		const auto result = expected.pattern(f.spawn, f.shots, f.random, option);
		if(!result.complete() || result.shots_allocated != expected.count ||
			!alive_prefix(f.shots, expected.count) || f.shots.shot_laser_time != expected.time ||
			f.shots.shot_laser_style != expected.style || f.shots.shot_laser_ring_cycle != 1 ||
			f.shots.shot_laser_bottomcenter.cur != option || f.shots.shot_laser_bottomcenter.prev != option ||
			f.shots.shots[0].patnum_base != 70 || f.shots.shots[0].damage != 9 ||
			f.shots.shots[0].pos.cur.x != option.x - 384 || f.shots.shots[0].pos.cur.y != player.y ||
			f.shots.shots[0].pos.velocity != pc98::th0405::SubpixelPoint{0, -256} ||
			f.shots.shots[1].pos.cur.x != option.x + 384 || f.shots.shots[1].patnum_base != 70) {
			return false;
		}
		if(expected.pattern == shot_marisa_a_l2) {
			if(f.random.cursor != 1 || f.shots.shots[2].patnum_base != pat_shot_marisa ||
				!has_velocity(f.shots.shots[2], 0xC2U)) return false;
		} else if(f.random.cursor != 0 || f.shots.shots[2].patnum_base != pat_shot_marisa) {
			return false;
		}
	}
	return true;
}

bool marisa_a_laser_cadence_and_random_boundary()
{
	Fixture f;
	f.shots.shot_laser_time = 48;
	f.shots.shot_laser_style = 4;
	f.shots.shot_laser_ring_cycle = 4;
	const auto quiet = shot_marisa_a_l3(f.spawn, f.shots, f.random, option);
	if(!quiet.complete() || quiet.shots_allocated != 2 || f.shots.shot_laser_time != 48 ||
		f.shots.shot_laser_style != 4 || f.shots.shot_laser_ring_cycle != 5 ||
		!alive_prefix(f.shots, 2)) return false;
	f.shots.shot_laser_ring_cycle = 7;
	const auto reset = shot_marisa_a_l3(f.spawn, f.shots, f.random, option);
	if(!reset.complete() || reset.shots_allocated != 2 || f.shots.shot_laser_ring_cycle != 0 ||
		!alive_prefix(f.shots, 4)) return false;

	Fixture refusal;
	refusal.random.cursor = 256;
	const auto result = shot_marisa_a_l2(refusal.spawn, refusal.shots, refusal.random, option);
	return result.status == ShotPatternStatus::random_refused && result.shots_allocated == 3 &&
		result.allocation.record_index == 2 && refusal.random.cursor == 256 &&
		refusal.shots.shot_laser_time == 64 && alive_prefix(refusal.shots, 3) &&
		refusal.shots.shots[2].patnum_base == 0;
}

bool marisa_b_all_levels()
{
	{
		Fixture f;
		f.random.ring[0] = 2;
		const auto result = shot_marisa_b_l2(f.spawn, f.shots, f.random);
		if(!result.complete() || result.shots_allocated != 3 || f.random.cursor != 1 ||
			!alive_prefix(f.shots, 3) || f.shots.shots[0].patnum_base != 36 ||
			f.shots.shots[0].pos.cur.x != player.x - 384 || f.shots.shots[0].pos.velocity.y != -256 ||
			f.shots.shots[2].patnum_base != pat_shot_marisa || !has_velocity(f.shots.shots[2], 0xBEU)) {
			return false;
		}
	}
	{
		Fixture f;
		const auto result = shot_marisa_b_l3(f.spawn, f.shots);
		if(!result.complete() || result.shots_allocated != 4 || !alive_prefix(f.shots, 4) ||
			f.shots.shots[0].patnum_base != 36 || f.shots.shots[3].patnum_base != pat_shot_marisa) return false;
	}
	{
		Fixture f;
		f.random.ring[0] = 1;
		f.random.ring[1] = 7;
		const auto result = shot_marisa_b_l4(f.spawn, f.shots, f.random);
		if(!result.complete() || result.shots_allocated != 4 || f.random.cursor != 2 ||
			!has_velocity(f.shots.shots[0], 0xBDU, 256) ||
			!has_velocity(f.shots.shots[1], 0xC3U, 256)) return false;
	}
	{
		Fixture f;
		f.random.ring[0] = 3;
		f.random.ring[1] = 4;
		const auto result = shot_marisa_b_l5(f.spawn, f.shots, f.random);
		if(!result.complete() || result.shots_allocated != 5 || f.random.cursor != 2 ||
			!alive_prefix(f.shots, 5) || f.shots.shots[0].damage != 5 ||
			f.shots.shots[4].patnum_base != pat_shot_marisa) return false;
	}
	struct Expected { std::size_t count; ShotPatternResult (*pattern)(ShotSpawnState &, ShotState &) noexcept; };
	constexpr std::array<Expected, 4> patterns{{
		{7, shot_marisa_b_l6}, {7, shot_marisa_b_l7}, {8, shot_marisa_b_l8}, {10, shot_marisa_b_l9},
	}};
	for(const auto expected : patterns) {
		Fixture f;
		const auto result = expected.pattern(f.spawn, f.shots);
		if(!result.complete() || result.shots_allocated != expected.count ||
			!alive_prefix(f.shots, expected.count) || f.shots.shots[0].patnum_base != 36 ||
			f.shots.shots[expected.count - 1].patnum_base != pat_shot_marisa) return false;
	}
	return true;
}

bool marisa_b_special_fan_and_random_boundary()
{
	Fixture f;
	const auto fan = shot_marisa_b_l8(f.spawn, f.shots);
	if(!fan.complete() || !has_velocity(f.shots.shots[4], 0xB6U) ||
		!has_velocity(f.shots.shots[5], 0xC0U) || !has_velocity(f.shots.shots[6], 0xC0U) ||
		!has_velocity(f.shots.shots[7], 0xCAU) || f.shots.shots[5].pos.cur.x != player.x - 128 ||
		f.shots.shots[6].pos.cur.x != player.x + 128) return false;
	Fixture refusal;
	refusal.random.cursor = 256;
	const auto result = shot_marisa_b_l4(refusal.spawn, refusal.shots, refusal.random);
	return result.status == ShotPatternStatus::random_refused && result.shots_allocated == 1 &&
		result.allocation.record_index == 0 && refusal.random.cursor == 256 &&
		refusal.shots.shots[0].pos.cur.x == player.x - 384 &&
		refusal.shots.shots[0].patnum_base == 36 && alive_prefix(refusal.shots, 1);
}

} // namespace

int main()
{
	for(const auto &[name, test] : std::array{
		std::pair{"reimu B all levels", reimu_b_all_levels},
		std::pair{"reimu B cadence", reimu_b_cycle_cadence},
		std::pair{"marisa A all levels", marisa_a_all_levels},
		std::pair{"marisa A laser cadence/RNG", marisa_a_laser_cadence_and_random_boundary},
		std::pair{"marisa B all levels", marisa_b_all_levels},
		std::pair{"marisa B fan/RNG", marisa_b_special_fan_and_random_boundary},
	}) {
		if(!test()) {
			std::cerr << "FAIL " << name << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
