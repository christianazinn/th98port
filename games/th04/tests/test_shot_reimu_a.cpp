#include "th04/core/shot_reimu_a.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace {
using namespace th04::core;

constexpr pc98::th0405::SubpixelPoint player{1600, 3200};

[[nodiscard]] bool shot_is(
	const Shot &shot,
	pc98::th0405::SubpixelPoint pos,
	pc98::th0405::SubpixelPoint velocity,
	std::int16_t patnum,
	std::int8_t damage
) noexcept
{
	return shot.flag == shot_alive && shot.age == 0 && shot.pos.cur == pos &&
		shot.pos.velocity == velocity && shot.patnum_base == patnum && shot.damage == damage;
}

template <typename Pattern>
[[nodiscard]] bool cycles_and_counts(
	Pattern pattern_fn, std::uint8_t divisor, int base, std::uint16_t expected_draws
)
{
	for(unsigned cycle = 0; cycle < 256U; ++cycle) {
		ReimuAPatternState pattern{static_cast<std::uint8_t>(cycle), {0, homing_target_none}};
		ShotSpawnState spawn{};
		ShotState shots{};
		pc98::th0405::RandomState random{};
		spawn.player_cur = player;
		const auto result = pattern_fn(pattern, spawn, shots, random);
		const auto count = base + ((cycle % divisor == 0U) ? 2 : 0);
		if(result != ReimuAPatternResult{
				ReimuAPatternStatus::complete,
				{ShotAddStatus::active_slot, static_cast<std::uint16_t>(count - 1)},
				static_cast<std::uint8_t>(count)} ||
			pattern.cycle != static_cast<std::uint8_t>(cycle + 1U) ||
			random.cursor != expected_draws) {
			return false;
		}
	}
	return true;
}

bool l2_l3_and_fans()
{
	ReimuAPatternState pattern{0, {7, homing_target_none}};
	ShotSpawnState spawn{};
	ShotState shots{};
	pc98::th0405::RandomState random{};
	spawn.player_cur = player;
	random.ring[0] = 0;
	random.ring[1] = 0;
	const auto l2 = shot_reimu_a_l2(pattern, spawn, shots, random);
	if(l2.shots_allocated != 3 || pattern.cycle != 1 || random.cursor != 1 ||
		!shot_is(shots.shots[0], {1216, 3200}, {0, -192}, pat_shot_reimu_sub_a, 10) ||
		!shot_is(shots.shots[1], {1984, 3200}, {0, -192}, pat_shot_reimu_sub_a, 10) ||
		!shot_is(shots.shots[2], player, pc98::th0405::SubpixelPoint{
			pc98::masterlib::vector2(0xB8U, 192).x,
			pc98::masterlib::vector2(0xB8U, 192).y}, pat_shot_reimu, 10)) return false;

	pattern = {0, {0, homing_target_none}};
	spawn = {};
	shots = {};
	random = {};
	spawn.player_cur = player;
	const auto l3 = shot_reimu_a_l3(pattern, spawn, shots, random);
	if(l3.shots_allocated != 4 || random.cursor != 0 ||
		!shot_is(shots.shots[0], {1216, 3200}, {0, -192}, pat_shot_reimu_sub_a, 10) ||
		!shot_is(shots.shots[1], {1984, 3200}, {0, -192}, pat_shot_reimu_sub_a, 10) ||
		!shot_is(shots.shots[2], {1472, 3200}, {0, -192}, pat_shot_reimu, 9) ||
		!shot_is(shots.shots[3], {1728, 3200}, {0, -192}, pat_shot_reimu, 9)) return false;

	const auto check_fan = [&](auto fn, std::uint8_t first_angle, std::int8_t own_damage,
		std::int8_t option_damage) {
		pattern = {0, {0, homing_target_none}};
		spawn = {};
		shots = {};
		random = {};
		spawn.player_cur = player;
		const auto result = fn(pattern, spawn, shots, random);
		if(result.shots_allocated != 5 || random.cursor != 0) return false;
		for(std::size_t index = 0; index < 3U; ++index) {
			const auto velocity = pc98::masterlib::vector2(
				static_cast<std::uint8_t>(first_angle + index *
					(first_angle == 0xBAU ? 6U : 8U)), 192
			);
			if(!shot_is(shots.shots[index + 2U], player,
					{velocity.x, velocity.y}, pat_shot_reimu, own_damage)) return false;
		}
		return shot_is(shots.shots[0], {1216, 3200}, {0, -192},
				pat_shot_reimu_sub_a, option_damage) &&
			shot_is(shots.shots[1], {1984, 3200}, {0, -192},
				pat_shot_reimu_sub_a, option_damage);
	};
	return check_fan(shot_reimu_a_l4, 0xBAU, 8, 9) &&
		check_fan(shot_reimu_a_l5, 0xB8U, 8, 9) &&
		check_fan(shot_reimu_a_l6, 0xB8U, 7, 8);
}

bool l7_l8_l9_shapes()
{
	ReimuAPatternState pattern{0, {0, homing_target_none}};
	ShotSpawnState spawn{};
	ShotState shots{};
	pc98::th0405::RandomState random{};
	spawn.player_cur = player;
	const auto l7 = shot_reimu_a_l7(pattern, spawn, shots, random);
	if(l7.shots_allocated != 7 || pattern.cycle != 1 ||
		!shot_is(shots.shots[0], {1216, 3200}, {0, -192}, pat_shot_reimu_sub_a, 7) ||
		!shot_is(shots.shots[1], {1984, 3200}, {0, -192}, pat_shot_reimu_sub_a, 7) ||
		!shot_is(shots.shots[2], {1216, 3200}, pc98::th0405::SubpixelPoint{
			pc98::masterlib::vector2(0xB8U, 192).x, pc98::masterlib::vector2(0xB8U, 192).y},
			pat_shot_reimu, 7) ||
		!shot_is(shots.shots[3], {1984, 3200}, pc98::th0405::SubpixelPoint{
			pc98::masterlib::vector2(0xC8U, 192).x, pc98::masterlib::vector2(0xC8U, 192).y},
			pat_shot_reimu, 7)) return false;

	pattern = {255, {0, homing_target_none}};
	spawn = {};
	shots = {};
	random = {};
	spawn.player_cur = player;
	const auto l8 = shot_reimu_a_l8(pattern, spawn, shots, random);
	if(l8.shots_allocated != 7 || pattern.cycle != 0 || random.cursor != 0 ||
		!shot_is(shots.shots[0], {1216, 3200}, {0, -192}, pat_shot_reimu_sub_a, 7) ||
		!shot_is(shots.shots[1], {1984, 3200}, {0, -192}, pat_shot_reimu_sub_a, 7)) return false;

	pattern = {0, {0, homing_target_none}};
	spawn = {};
	shots = {};
	random = {};
	spawn.player_cur = player;
	const auto l9_even = shot_reimu_a_l9(pattern, spawn, shots, random);
	if(l9_even.shots_allocated != 9 || pattern.cycle != 1 ||
		!shot_is(shots.shots[0], {1216, 3200}, {0, -192}, pat_shot_reimu_sub_a, 7) ||
		!shot_is(shots.shots[1], {1984, 3200}, {0, -192}, pat_shot_reimu_sub_a, 7) ||
		!shot_is(shots.shots[2], {1216, 3200}, pc98::th0405::SubpixelPoint{
			pc98::masterlib::vector2(0xB4U, 192).x, pc98::masterlib::vector2(0xB4U, 192).y},
			pat_shot_reimu_sub_a, 5) ||
		!shot_is(shots.shots[3], {1984, 3200}, pc98::th0405::SubpixelPoint{
			pc98::masterlib::vector2(0xCCU, 192).x, pc98::masterlib::vector2(0xCCU, 192).y},
			pat_shot_reimu_sub_a, 5) ||
		!shot_is(shots.shots[4], {1216, 3200}, pc98::th0405::SubpixelPoint{
			pc98::masterlib::vector2(0xB8U, 192).x, pc98::masterlib::vector2(0xB8U, 192).y},
			pat_shot_reimu, 7) ||
		!shot_is(shots.shots[5], {1984, 3200}, pc98::th0405::SubpixelPoint{
			pc98::masterlib::vector2(0xC8U, 192).x, pc98::masterlib::vector2(0xC8U, 192).y},
			pat_shot_reimu, 7)) return false;

	pattern = {1, {0, homing_target_none}};
	spawn = {};
	shots = {};
	random = {};
	spawn.player_cur = player;
	const auto l9_odd = shot_reimu_a_l9(pattern, spawn, shots, random);
	return l9_odd.shots_allocated == 7 && pattern.cycle == 2 && random.cursor == 0;
}

template <typename Pattern>
[[nodiscard]] bool cycle_reset(Pattern pattern_fn, std::uint8_t expected_count)
{
	ReimuAPatternState pattern{29, {0, homing_target_none}};
	ShotSpawnState spawn{};
	ShotState shots{};
	pc98::th0405::RandomState random{};
	spawn.player_cur = player;
	spawn.shot_time = 18;
	const auto result = pattern_fn(pattern, spawn, shots, random);
	return result.status == ReimuAPatternStatus::complete &&
		result.shots_allocated == expected_count && pattern.cycle == 1;
}

bool cycle_reset_and_l9_homing()
{
	if(!cycle_reset(shot_reimu_a_l2, 3U) || !cycle_reset(shot_reimu_a_l3, 4U) ||
		!cycle_reset(shot_reimu_a_l4, 5U) || !cycle_reset(shot_reimu_a_l5, 5U) ||
		!cycle_reset(shot_reimu_a_l6, 5U) || !cycle_reset(shot_reimu_a_l7, 7U) ||
		!cycle_reset(shot_reimu_a_l9, 9U)) return false;

	ReimuAPatternState l8_pattern{29, {0, homing_target_none}};
	ShotSpawnState l8_spawn{};
	ShotState l8_shots{};
	pc98::th0405::RandomState l8_random{};
	l8_spawn.player_cur = player;
	l8_spawn.shot_time = 18;
	if(shot_reimu_a_l8(l8_pattern, l8_spawn, l8_shots, l8_random).shots_allocated != 7 ||
		l8_pattern.cycle != 30) return false;

	ReimuAPatternState pattern{0, {2000, 3600}};
	ShotSpawnState spawn{};
	ShotState shots{};
	pc98::th0405::RandomState random{};
	spawn.player_cur = player;
	random.ring[0] = 5;
	random.ring[1] = 0;
	const auto result = shot_reimu_a_l9(pattern, spawn, shots, random);
	const auto left_angle = pc98::masterlib::iatan2_source(400, 784);
	const auto right_angle = pc98::masterlib::iatan2_source(400, 16);
	return left_angle && right_angle && result.shots_allocated == 9 && random.cursor == 2 &&
		shots.shots[0].pos.velocity == pc98::th0405::SubpixelPoint{
			pc98::masterlib::vector2(*left_angle, 192).x,
			pc98::masterlib::vector2(*left_angle, 192).y} &&
		shots.shots[1].pos.velocity == pc98::th0405::SubpixelPoint{
			pc98::masterlib::vector2(*right_angle, 192).x,
			pc98::masterlib::vector2(*right_angle, 192).y} &&
		shots.shots[2].pos.velocity == pc98::th0405::SubpixelPoint{
			pc98::masterlib::vector2(static_cast<std::uint8_t>(1U + *left_angle), 192).x,
			pc98::masterlib::vector2(static_cast<std::uint8_t>(1U + *left_angle), 192).y} &&
		shots.shots[3].pos.velocity == pc98::th0405::SubpixelPoint{
			pc98::masterlib::vector2(static_cast<std::uint8_t>(*right_angle - 4U), 192).x,
			pc98::masterlib::vector2(static_cast<std::uint8_t>(*right_angle - 4U), 192).y};
}

bool cycle_divisors_all_bytes()
{
	return cycles_and_counts(shot_reimu_a_l2, 3U, 1, 1U) &&
		cycles_and_counts(shot_reimu_a_l3, 3U, 2, 0U) &&
		cycles_and_counts(shot_reimu_a_l4, 3U, 3, 0U) &&
		cycles_and_counts(shot_reimu_a_l5, 2U, 3, 0U) &&
		cycles_and_counts(shot_reimu_a_l6, 2U, 3, 0U) &&
		cycles_and_counts(shot_reimu_a_l7, 2U, 5, 0U) &&
		cycles_and_counts(shot_reimu_a_l9, 2U, 7, 0U);
}

bool homing_and_fault_prefixes()
{
	ReimuAPatternState pattern{0, {2000, 3600}};
	ShotSpawnState spawn{};
	ShotState shots{};
	pc98::th0405::RandomState random{};
	spawn.player_cur = player;
	random.ring[0] = 5;
	random.ring[1] = 0;
	const auto result = shot_reimu_a_l2(pattern, spawn, shots, random);
	const auto expected_angle = pc98::masterlib::iatan2_source(400, 784);
	if(!expected_angle || result.shots_allocated != 3 || random.cursor != 3 ||
		shots.shots[0].pos.cur != pc98::th0405::SubpixelPoint{1216, 3200} ||
		shots.shots[0].pos.velocity != pc98::th0405::SubpixelPoint{
			pc98::masterlib::vector2(static_cast<std::uint8_t>(1U + *expected_angle), 192).x,
			pc98::masterlib::vector2(static_cast<std::uint8_t>(1U + *expected_angle), 192).y} ||
		shots.shots[0].patnum_base != pat_shot_reimu_sub_a || shots.shots[0].damage != 10) return false;

	pattern = {0, {-383, -32768}};
	spawn = {};
	shots = {};
	random = {};
	spawn.player_cur = {0, 0};
	const auto fault = shot_reimu_a_l2(pattern, spawn, shots, random);
	if(fault.status != ReimuAPatternStatus::iatan2_fault || fault.shots_allocated != 1 ||
		random.cursor != 1 || shots.shots[0].pos.cur != pc98::th0405::SubpixelPoint{-384, 0} ||
		shots.shots[0].pos.velocity != pc98::th0405::SubpixelPoint{0, -192} ||
		shots.shots[0].patnum_base != 0 || shots.shots[0].damage != 0) return false;

	pattern = {0, {0, homing_target_none}};
	spawn = {};
	shots = {};
	random = {};
	random.cursor = 256;
	spawn.player_cur = player;
	const auto random_fault = shot_reimu_a_l2(pattern, spawn, shots, random);
	if(random_fault.status != ReimuAPatternStatus::random_refused ||
		random_fault.shots_allocated != 3 || random.cursor != 256 ||
		!shot_is(shots.shots[0], {1216, 3200}, {0, -192}, pat_shot_reimu_sub_a, 10) ||
		!shot_is(shots.shots[1], {1984, 3200}, {0, -192}, pat_shot_reimu_sub_a, 10) ||
		!shot_is(shots.shots[2], player, {0, -192}, 0, 0)) return false;

	pattern = {0, {0, homing_target_none}};
	spawn = {};
	shots = {};
	random = {};
	spawn.player_cur = player;
	for(auto &shot : shots.shots) shot.flag = shot_alive;
	const auto exhausted = shot_reimu_a_l7(pattern, spawn, shots, random);
	return exhausted.status == ReimuAPatternStatus::complete &&
		exhausted.allocation.status == ShotAddStatus::no_slot && exhausted.shots_allocated == 0 &&
		spawn.shot_cursor_bytes == shot_count * sizeof(Shot) && spawn.shot_last_id == shot_count;
}

} // namespace

int main()
{
	for(const auto &[name, test] : std::array{
		std::pair{"l2/l3/fans", l2_l3_and_fans},
		std::pair{"l7/l8/l9", l7_l8_l9_shapes},
		std::pair{"cycle bytes", cycle_divisors_all_bytes},
		std::pair{"cycle reset/l9 homing", cycle_reset_and_l9_homing},
		std::pair{"homing prefixes", homing_and_fault_prefixes},
	}) {
		if(!test()) {
			std::cerr << "FAIL " << name << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
