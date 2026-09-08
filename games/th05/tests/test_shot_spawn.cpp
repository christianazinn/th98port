#include "th05/core/shot_spawn.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {
using namespace th05::core;

struct SoundLog {
	const ShotSpawnState *state{};
	std::uint16_t cursor_when_called{};
	std::uint8_t calls{};
	std::uint8_t sound{};

	void operator()(std::uint8_t effect) noexcept
	{
		cursor_when_called = state->shot_cursor;
		calls = static_cast<std::uint8_t>(calls + 1U);
		sound = effect;
	}
};

constexpr Shot sentinel_shot(std::uint8_t flag, std::uint8_t seed) noexcept
{
	return {
		flag,
		static_cast<std::uint8_t>(seed + 1U),
		{{
			static_cast<std::int16_t>(100 + seed),
			static_cast<std::int16_t>(200 + seed)
		}, {
			static_cast<std::int16_t>(300 + seed),
			static_cast<std::int16_t>(400 + seed)
		}, {
			static_cast<std::int16_t>(500 + seed),
			static_cast<std::int16_t>(600 + seed)
		}},
		static_cast<std::uint8_t>(20U + seed),
		static_cast<std::uint8_t>(30U + seed),
		static_cast<std::int8_t>(40 + seed),
		static_cast<std::uint8_t>(50U + seed),
	};
}

bool assert_initialized(
	const Shot &shot,
	pc98::th0405::SubpixelPoint player_cur,
	const Shot &before
)
{
	return shot.flag == shot_alive && shot.age == 0 &&
		shot.pos.cur == player_cur && shot.pos.velocity == pc98::th0405::SubpixelPoint{0, -192} &&
		shot.patnum_base == 20 && shot.type == shot_normal &&
		shot.pos.prev == before.pos.prev && shot.damage == before.damage &&
		shot.angle == before.angle;
}

bool test_all_free_preincrement_sequence()
{
	ShotUpdateState update{};
	ShotSpawnState state{};
	SoundLog sound{&state};
	state.player_cur = {1234, -567};
	state.shot_time = 18;
	for(std::size_t i = 0; i < update.shots.size(); ++i) {
		update.shots[i] = sentinel_shot(shot_free, static_cast<std::uint8_t>(i));
	}
	const auto before = update.shots;
	if(shot_cycle_init(state, sound).flags != 0xFU || sound.calls != 1 ||
		sound.sound != 1 || sound.cursor_when_called != 0 || state.shot_cursor != 0 ||
		state.shot_last_id != 0) return false;
	for(std::uint16_t expected = 1; expected < shot_count; expected += 2U) {
		const auto allocation = shots_add(state, update);
		if(allocation.status != ShotAddStatus::active_slot ||
			allocation.record_index != expected ||
			allocation.shot != &update.shots[expected] ||
			state.shot_cursor != static_cast<std::uint16_t>(expected + 1U) ||
			!assert_initialized(update.shots[expected], state.player_cur, before[expected])) return false;
	}
	const auto exhausted = shots_add(state, update);
	if(exhausted.status != ShotAddStatus::no_slot || exhausted.allocated() ||
		state.shot_cursor != shot_count || update.shots[0] != before[0]) return false;
	for(std::size_t i = 0; i < state.guard.size(); ++i) {
		if(state.guard[i] != Shot{}) return false;
	}
	return true;
}

bool test_occupied_scan_and_guard()
{
	ShotUpdateState update{};
	ShotSpawnState state{};
	state.player_cur = {-22, 44};
	update.shots[1] = sentinel_shot(99, 1);
	update.shots[2] = sentinel_shot(shot_free, 2);
	const auto active_before = update.shots[2];
	const auto active = shots_add(state, update);
	if(active.status != ShotAddStatus::active_slot || active.record_index != 2 ||
		state.shot_cursor != 3 ||
		!assert_initialized(update.shots[2], state.player_cur, active_before)) return false;

	state.shot_cursor = 63;
	update.shots[63] = sentinel_shot(99, 63);
	for(std::size_t i = 0; i < state.guard.size(); ++i) {
		state.guard[i] = sentinel_shot(shot_free, static_cast<std::uint8_t>(70U + i));
	}
	const auto guard_before = state.guard;
	const auto guard = shots_add(state, update);
	if(guard.status != ShotAddStatus::guard_slot || guard.record_index != 64 ||
		guard.shot != &state.guard[0] || state.shot_cursor != 65 ||
		!assert_initialized(state.guard[0], state.player_cur, guard_before[0]) ||
		state.guard[1] != guard_before[1] || state.guard[2] != guard_before[2] ||
		state.guard[3] != guard_before[3]) return false;
	if(shots_add(state, update).status != ShotAddStatus::no_slot || state.shot_cursor != 65) return false;
	return true;
}

bool test_no_slot_and_invalid_cursor_are_nonmutating()
{
	ShotUpdateState update{};
	ShotSpawnState state{};
	for(std::size_t i = 0; i < update.shots.size(); ++i) update.shots[i] = sentinel_shot(2, 3);
	state.guard[0] = sentinel_shot(7, 4);
	state.shot_cursor = 0;
	const auto update_before = update;
	const auto guard_before = state.guard;
	if(shots_add(state, update).status != ShotAddStatus::no_slot || state.shot_cursor != 0 ||
		update != update_before || state.guard != guard_before) return false;
	for(const auto cursor : std::array<std::uint16_t, 2>{66, 0xFFFFU}) {
		state.shot_cursor = cursor;
		if(shots_add(state, update).status != ShotAddStatus::invalid_cursor ||
			state.shot_cursor != cursor || update != update_before ||
			state.guard != guard_before) return false;
	}
	return true;
}

bool test_cycle_masks_and_reset()
{
	struct Expected {
		std::uint8_t time, flags;
		bool sound;
	};
	for(const auto expected : std::array<Expected, 6>{{
		{18, 0xF, true}, {15, 0x4, false}, {12, 0x5, true},
		{9, 0x6, false}, {6, 0x5, true}, {3, 0x4, false},
	}}) {
		ShotSpawnState state{};
		state.shot_time = expected.time;
		state.shot_cursor = 31;
		state.shot_last_id = 91;
		SoundLog sound{&state};
		const auto result = shot_cycle_init(state, sound);
		if(result.flags != expected.flags || result.sound_1 != expected.sound ||
			state.shot_cursor != 0 || state.shot_last_id != 0 ||
			sound.calls != (expected.sound ? 1U : 0U) ||
			(expected.sound && (sound.sound != 1 || sound.cursor_when_called != 31))) return false;
	}
	for(const auto time : std::array<std::uint8_t, 5>{0, 1, 2, 17, 0xFF}) {
		ShotSpawnState state{};
		state.shot_time = time;
		state.shot_cursor = 31;
		state.shot_last_id = 91;
		SoundLog sound{&state};
		if(shot_cycle_init(state, sound).flags != 0 || state.shot_cursor != 31 ||
			state.shot_last_id != 91 || sound.calls != 0) return false;
	}
	return true;
}

bool test_l0_and_l1_ordering()
{
	ShotUpdateState update{};
	ShotSpawnState state{};
	pc98::th0405::RandomState random{};
	SoundLog sound{&state};
	state.player_cur = {99, 77};
	state.shot_time = 18;
	random.seed = 0x12345678U;
	random.cursor = 7;
	random.ring[7] = 0x05;
	random.ring[8] = 0xAA;
	const auto random_before = random;
	const auto l0 = shot_l0(state, update, sound);
	if(l0.cycle.flags != 0xF || !l0.cycle.sound_1 ||
		l0.allocation.status != ShotAddStatus::active_slot ||
		l0.allocation.record_index != 1 || l0.random_refused ||
		random != random_before || update.shots[1].damage != 10 ||
		sound.calls != 1 || sound.cursor_when_called != 0) return false;

	state.shot_time = 12;
	random.ring[7] = 0x06;
	const auto l1 = shot_l1(state, update, random, sound);
	const auto expected_angle = static_cast<std::uint8_t>(0xBCU + 6U);
	const auto expected_velocity = pc98::masterlib::vector2(expected_angle, 192);
	if(l1.cycle.flags != 0x5 || !l1.cycle.sound_1 ||
		l1.allocation.status != ShotAddStatus::active_slot ||
		l1.allocation.record_index != 2 || l1.random_refused ||
		random.cursor != 8 || update.shots[2].angle != expected_angle ||
		update.shots[2].pos.velocity != pc98::th0405::SubpixelPoint{
			expected_velocity.x, expected_velocity.y
		} || update.shots[2].damage != 10) return false;

	ShotUpdateState blocked_update{};
	ShotSpawnState blocked{};
	pc98::th0405::RandomState blocked_random{};
	SoundLog blocked_sound{&blocked};
	blocked.shot_time = 15;
	blocked.shot_cursor = 55;
	blocked.shot_last_id = 3;
	const auto no_trigger = shot_l1(
		blocked, blocked_update, blocked_random, blocked_sound
	);
	if(no_trigger.cycle.flags != 0x4 || no_trigger.allocation.allocated() ||
		blocked.shot_cursor != 0 || blocked.shot_last_id != 0 ||
		blocked_random.cursor != 0 || blocked_sound.calls != 0) return false;

	for(auto &shot : blocked_update.shots) shot = sentinel_shot(9, 1);
	blocked.guard[0] = sentinel_shot(9, 2);
	blocked.shot_time = 18;
	blocked_random.cursor = 11;
	const auto no_slot = shot_l1(
		blocked, blocked_update, blocked_random, blocked_sound
	);
	if(no_slot.allocation.status != ShotAddStatus::no_slot || no_slot.random_refused ||
		blocked_random.cursor != 11) return false;
	return true;
}

bool test_l1_random_refusal_preserves_allocation_prefix()
{
	ShotUpdateState update{};
	ShotSpawnState state{};
	pc98::th0405::RandomState random{};
	SoundLog sound{&state};
	state.shot_time = 18;
	state.player_cur = {111, 222};
	random.cursor = 256;
	update.shots[1] = sentinel_shot(shot_free, 9);
	const auto before = update.shots[1];
	const auto result = shot_l1(state, update, random, sound);
	if(!result.random_refused || result.allocation.status != ShotAddStatus::active_slot ||
		state.shot_cursor != 2 || random.cursor != 256 ||
		!assert_initialized(update.shots[1], state.player_cur, before) ||
		update.shots[1].damage != before.damage || update.shots[1].angle != before.angle) return false;
	return true;
}

bool test_spawn_state_copy_has_no_borrowed_aliases()
{
	ShotSpawnState original{};
	original.guard[0] = sentinel_shot(7, 8);
	original.player_cur = {19, 23};
	original.shot_cursor = 31;
	original.shot_last_id = 9;
	original.shot_time = 12;
	auto copy = original;
	copy.guard[0].damage = 99;
	copy.player_cur.x = -100;
	copy.shot_cursor = 2;
	copy.shot_last_id = 3;
	copy.shot_time = 6;
	return original.guard[0].damage != copy.guard[0].damage &&
		original.player_cur != copy.player_cur && original.shot_cursor == 31 &&
		original.shot_last_id == 9 && original.shot_time == 12;
}

} // namespace

int main()
{
	if(!test_all_free_preincrement_sequence() ||
		!test_occupied_scan_and_guard() ||
		!test_no_slot_and_invalid_cursor_are_nonmutating() ||
		!test_cycle_masks_and_reset() ||
		!test_l0_and_l1_ordering() ||
		!test_l1_random_refusal_preserves_allocation_prefix() ||
		!test_spawn_state_copy_has_no_borrowed_aliases()) {
		std::cerr << "th05 shot spawn test failure\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
