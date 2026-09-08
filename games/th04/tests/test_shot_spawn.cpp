#include "th04/core/shot_spawn.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace {
using namespace th04::core;

bool initialized(const Shot &shot, pc98::th0405::SubpixelPoint player) noexcept
{
	return shot.flag == shot_alive && shot.age == 0 && shot.pos.cur == player &&
		shot.pos.velocity == pc98::th0405::SubpixelPoint{0, -192};
}

bool allocator_active_guard_and_tail()
{
	ShotSpawnState spawn{};
	ShotState shots{};
	spawn.player_cur = {123, -456};
	for(std::size_t i = 0; i < shot_count; ++i) {
		shots.shots[i] = {shot_free, -3, {{1, 2}, {3, 4}, {5, 6}}, 77, 8, 9};
		const auto result = shots_add(spawn, shots);
		if(result.status != ShotAddStatus::active_slot || result.record_index != i ||
			!initialized(shots.shots[i], spawn.player_cur) || shots.shots[i].pos.prev != pc98::th0405::SubpixelPoint{3, 4} ||
			shots.shots[i].patnum_base != 77 || shots.shots[i].damage != 8 || shots.shots[i].angle != 9 ||
			spawn.shot_cursor_bytes != (i + 1U) * sizeof(Shot) || spawn.shot_last_id != 0) return false;
	}
	for(std::size_t i = 0; i < shot_spawn_guard_count; ++i) {
		spawn.guard[i] = {shot_free, -3, {{1, 2}, {3, 4}, {5, 6}}, 77, 8, 9};
		const auto result = shots_add(spawn, shots);
		if(result.status != ShotAddStatus::guard_slot || result.record_index != shot_count + i ||
			!initialized(spawn.guard[i], spawn.player_cur) || spawn.guard[i].pos.prev != pc98::th0405::SubpixelPoint{3, 4} ||
			spawn.guard[i].patnum_base != 77 || spawn.guard[i].damage != 8 || spawn.guard[i].angle != 9) return false;
	}
	const auto before_tail = spawn.tail;
	const auto result = shots_add(spawn, shots);
	return result.status == ShotAddStatus::unsupported_tail_access &&
		spawn.shot_cursor_bytes == shot_spawn_tail_offset + sizeof(Shot) && spawn.shot_last_id == 0 &&
		spawn.tail == before_tail;
}

bool occupied_counts_and_cursor_prefix()
{
	ShotSpawnState spawn{};
	ShotState shots{};
	spawn.player_cur = {10, 20};
	shots.shots[0].flag = shot_alive;
	shots.shots[1] = {shot_free, -1, {{1, 2}, {3, 4}, {5, 6}}, 7, 8, 9};
	const auto mixed = shots_add(spawn, shots);
	if(mixed.status != ShotAddStatus::active_slot || mixed.record_index != 1 ||
		spawn.shot_cursor_bytes != 2U * sizeof(Shot) || spawn.shot_last_id != 1 ||
		!initialized(shots.shots[1], spawn.player_cur)) return false;
	spawn = {};
	for(auto &shot : shots.shots) shot.flag = shot_alive;
	const auto before_guards = spawn.guard;
	const auto full = shots_add(spawn, shots);
	if(full.status != ShotAddStatus::no_slot || spawn.shot_cursor_bytes != shot_count * sizeof(Shot) ||
		spawn.shot_last_id != shot_count || spawn.guard != before_guards) return false;
	spawn = {};
	spawn.shot_cursor_bytes = shot_count * sizeof(Shot);
	spawn.guard[0].flag = shot_alive;
	spawn.guard[1].flag = shot_free;
	const auto guard = shots_add(spawn, shots);
	return guard.status == ShotAddStatus::guard_slot && guard.record_index == shot_count + 1U &&
		spawn.shot_cursor_bytes == (shot_count + 2U) * sizeof(Shot) && spawn.shot_last_id == 1;
}

bool invalid_host_cursor_and_source_early_no_slot()
{
	ShotSpawnState spawn{};
	ShotState shots{};
	spawn.shot_cursor_bytes = 1;
	const auto before = spawn;
	if(shots_add(spawn, shots).status != ShotAddStatus::invalid_cursor || spawn != before) return false;
	spawn.shot_cursor_bytes = static_cast<std::uint16_t>(shot_spawn_tail_offset + sizeof(Shot));
	const auto after_tail = spawn;
	if(shots_add(spawn, shots).status != ShotAddStatus::invalid_cursor || spawn != after_tail) return false;
	spawn.shot_last_id = static_cast<std::uint8_t>(shot_count);
	const auto before_early_no_slot = spawn;
	return shots_add(spawn, shots).status == ShotAddStatus::no_slot && spawn == before_early_no_slot;
}

bool l0_l1_and_random_prefix()
{
	ShotSpawnState spawn{};
	ShotState shots{};
	spawn.player_cur = {300, 400};
	spawn.shot_cursor_bytes = 36;
	spawn.shot_last_id = 2;
	const auto reimu = shot_reimu_l0(spawn, shots);
	if(reimu.allocation.status != ShotAddStatus::active_slot || reimu.allocation.record_index != 0 ||
		shots.shots[0].patnum_base != pat_shot_reimu || shots.shots[0].damage != 10 ||
		spawn.shot_cursor_bytes != sizeof(Shot) || spawn.shot_last_id != 0) return false;

	pc98::th0405::RandomState random{};
	random.ring[0] = 5;
	random.ring[1] = 0;
	const auto marisa = shot_marisa_l1(spawn, shots, random);
	const auto expected = pc98::masterlib::vector2(0xC1U, 192);
	if(marisa.allocation.status != ShotAddStatus::active_slot || marisa.allocation.record_index != 1 ||
		marisa.random_refused || shots.shots[1].patnum_base != pat_shot_marisa ||
		shots.shots[1].damage != 10 ||
		shots.shots[1].pos.velocity != pc98::th0405::SubpixelPoint{expected.x, expected.y} ||
		random.cursor != 1) return false;

	spawn = {};
	shots = {};
	random.cursor = 256;
	const auto prefix = shot_reimu_l1(spawn, shots, random);
	if(prefix.allocation.status != ShotAddStatus::active_slot || !prefix.random_refused ||
		!initialized(shots.shots[0], {}) || shots.shots[0].patnum_base != 0 ||
		shots.shots[0].damage != 0 || random.cursor != 256) return false;
	spawn = {};
	shots = {};
	for(auto &shot : shots.shots) shot.flag = shot_alive;
	random = {};
	random.ring[0] = 3;
	const auto random_before = random;
	const auto no_slot = shot_reimu_l1(spawn, shots, random);
	return no_slot.allocation.status == ShotAddStatus::no_slot && !no_slot.random_refused &&
		random == random_before && spawn.shot_last_id == shot_count;
}

bool cadence_order_and_ticks()
{
	ShotSpawnState spawn{};
	std::array<int, 8> order{};
	std::size_t count{};
	auto fire = [&]() noexcept { order[count++] = static_cast<int>(spawn.shot_time); };
	auto sound = [&](std::uint8_t id) noexcept { order[count++] = 100 + id; };
	if(shot_cycle_step(spawn, true, fire, sound) != ShotCycleResult{true, true} ||
		spawn.shot_time != 18 || count != 2 || order[0] != 18 || order[1] != 101) return false;
	spawn.shot_time = 13;
	if(shot_cycle_step(spawn, false, fire, sound) != ShotCycleResult{true, true} ||
		spawn.shot_time != 12 || order[2] != 12 || order[3] != 101) return false;
	spawn.shot_time = 7;
	if(shot_cycle_step(spawn, false, fire, sound) != ShotCycleResult{true, true} ||
		spawn.shot_time != 6 || order[4] != 6 || order[5] != 101) return false;
	spawn.shot_time = 2;
	if(shot_cycle_step(spawn, false, fire, sound) != ShotCycleResult{} || spawn.shot_time != 1) return false;
	return shot_cycle_step(spawn, false, fire, sound) == ShotCycleResult{} && spawn.shot_time == 0 && count == 6;
}

bool cadence_all_byte_states()
{
	for(unsigned initial = 0; initial < 256; ++initial) {
		for(const bool held : {false, true}) {
			ShotSpawnState spawn{};
			spawn.shot_time = static_cast<std::uint8_t>(initial);
			std::array<int, 2> order{};
			std::size_t count{};
			auto fire = [&]() noexcept { order[count++] = static_cast<int>(spawn.shot_time); };
			auto sound = [&](std::uint8_t id) noexcept { order[count++] = 100 + id; };
			const auto result = shot_cycle_step(spawn, held, fire, sound);
			const bool immediate = held && initial <= 1U;
			const auto expected_time = immediate ? 18U : (initial == 0U ? 0U : initial - 1U);
			const bool fire_expected = immediate || expected_time == 6U || expected_time == 12U;
			if(result != ShotCycleResult{fire_expected, fire_expected} ||
				spawn.shot_time != expected_time || count != (fire_expected ? 2U : 0U)) return false;
			if(fire_expected && (order[0] != static_cast<int>(expected_time) || order[1] != 101)) return false;
		}
	}
	return true;
}

} // namespace

int main()
{
	for(const auto &[name, test] : std::array{
		std::pair{"allocator/guard/tail", allocator_active_guard_and_tail},
		std::pair{"occupied prefix", occupied_counts_and_cursor_prefix},
		std::pair{"invalid cursor", invalid_host_cursor_and_source_early_no_slot},
		std::pair{"l0/l1/random", l0_l1_and_random_prefix},
		std::pair{"cadence", cadence_order_and_ticks},
		std::pair{"cadence bytes", cadence_all_byte_states},
	}) {
		if(!test()) {
			std::cerr << "FAIL " << name << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
