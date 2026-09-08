#include "th05/core/shot.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <iostream>

namespace {
using namespace th05::core;

bool test_free_and_occupied()
{
	for(unsigned index = 0; index < hitshot_count; ++index) {
		for(unsigned age = 0; age < 256; ++age) {
			HitShotState state{};
			for(auto &entry : state.hitshots) entry = {9, 71, {{1, 2}, {3, 4}, {5, 6}}};
			state.next_free_id = static_cast<std::uint16_t>(index);
			state.hitshots[index].age = static_cast<std::uint8_t>(age);
			auto expected = state;
			expected.next_free_id = static_cast<std::uint16_t>((index + 1) % 24);
			Shot shot{77, 81, {{101, -102}, {201, -202}, {-13, 17}}, 20, 3, -7, 99};
			auto shot_expected = shot;
			shot_expected.flag = shot_remove;
			if(age == 0) expected.hitshots[index] = {1, 28, {{101, -102}, {201, -202}, {-2, 2}}};
			if(!hitshot_from(state, shot) || state != expected || shot != shot_expected) return false;
		}
	}
	return true;
}

bool test_sprite_and_division()
{
	for(int value = -32768; value < 32768; ++value) {
		HitShotState state{};
		Shot shot{};
		shot.patnum_base = static_cast<std::uint8_t>(value);
		shot.pos.velocity = {static_cast<std::int16_t>(value), static_cast<std::int16_t>(-1 - value)};
		if(!hitshot_from(state, shot)) return false;
		const auto &result = state.hitshots[0];
		// Independent sign/magnitude model makes rounding toward zero explicit.
		const auto divide = [](int v) { return v < 0 ? -((-v) / 6) : v / 6; };
		if(result.patnum != (shot.patnum_base == 20 ? 28 : 32) ||
			result.pos.velocity.x != divide(value) ||
			result.pos.velocity.y != divide(-1 - value)) return false;
	}
	return true;
}

bool test_invalid_cursor()
{
	for(unsigned cursor : {24U, 25U, 65535U}) {
		HitShotState state{};
		state.next_free_id = static_cast<std::uint16_t>(cursor);
		Shot shot{};
		shot.flag = shot_alive;
		const auto before = state;
		const auto shot_before = shot;
		if(hitshot_from(state, shot) || state != before || shot != shot_before) return false;
	}
	return true;
}

std::int16_t word_add(int left, int right)
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(left + right));
}

bool test_update_flag_lifetime_and_cache()
{
	for(unsigned flag = 0; flag < 256; ++flag) {
		ShotUpdateState state{};
		for(auto &entry : state.shots_alive) entry = {{-999, 999}, 63};
		state.shots_alive_count = 65535;
		auto &shot = state.shots[17];
		shot = {static_cast<std::uint8_t>(flag), 127,
			{{100, 200}, {300, 400}, {1, -2}}, 9, 99, -7, 42};
		const auto before = state;
		if(shots_update(state) != ShotUpdateResult::complete) return false;
		auto expected = before;
		expected.shots_alive_count = 0;
		auto &e = expected.shots[17];
		if(flag == shot_remove) {
			e.flag = shot_free;
		} else if(flag != shot_free) {
			e.pos.prev = {100, 200};
			e.pos.cur = {101, 198};
			e.age = 128;
			expected.shots_alive_count = 1;
			expected.shots_alive[0] = {{101, 198}, 17};
		}
		if(state != expected) return false;
	}
	return true;
}

bool test_update_missiles_and_bounds()
{
	struct Case {
		std::uint8_t type;
		pc98::th0405::SubpixelPoint velocity;
		pc98::th0405::SubpixelPoint expected_velocity;
	};
	for(const auto test : std::array<Case, 3>{{
		{shot_missile_left, {10, 20}, {11, 16}},
		{shot_missile_right, {10, 20}, {9, 16}},
		{shot_missile_straight, {10, 20}, {10, 16}},
	}}) {
		ShotUpdateState state{};
		auto &shot = state.shots[0];
		shot = {shot_alive, 7, {{100, 200}, {-1, -2}, test.velocity}, 5,
			test.type, 3, 4};
		if(shots_update(state) != ShotUpdateResult::complete ||
			shot.pos.velocity != test.expected_velocity ||
			shot.pos.prev != pc98::th0405::SubpixelPoint{100, 200} ||
			shot.pos.cur != pc98::th0405::SubpixelPoint{
				word_add(100, test.expected_velocity.x),
				word_add(200, test.expected_velocity.y)
			} || shot.age != 8 || state.shots_alive_count != 1) return false;
	}

	ShotUpdateState wrapping{};
	wrapping.shots[0] = {shot_alive, 1, {{100, 200}, {}, {32767, -32768}}, 0,
		shot_missile_left, 0, 0};
	if(shots_update(wrapping) != ShotUpdateResult::complete ||
		wrapping.shots[0].pos.velocity != pc98::th0405::SubpixelPoint{-32768, 32764} ||
		wrapping.shots[0].pos.cur != pc98::th0405::SubpixelPoint{-32668, -32572} ||
		wrapping.shots[0].flag != shot_remove || wrapping.shots[0].age != 1 ||
		wrapping.shots_alive_count != 0) return false;

	ShotUpdateState outside{};
	outside.shots[0] = {99, 19, {{-129, 201}, {4, 5}, {1, 0}}, 8, 0, 7, 6};
	if(shots_update(outside) != ShotUpdateResult::complete ||
		outside.shots[0].flag != shot_remove || outside.shots[0].age != 19 ||
		outside.shots[0].pos.prev != pc98::th0405::SubpixelPoint{-129, 201} ||
		outside.shots[0].pos.cur != pc98::th0405::SubpixelPoint{-128, 201} ||
		outside.shots_alive_count != 0) return false;
	const auto retired = outside.shots[0];
	if(shots_update(outside) != ShotUpdateResult::complete ||
		outside.shots[0].flag != shot_free || outside.shots[0].pos != retired.pos ||
		outside.shots[0].age != retired.age) return false;
	return true;
}

bool test_all_update_bounds()
{
	struct Bound {
		bool x_axis;
		std::int16_t lower;
		std::int16_t upper;
	};
	for(const auto bound : std::array<Bound, 2>{{
		{true, -128, 6272},
		{false, -128, 6016},
	}}) {
		for(const auto [position, in_bounds] : std::array{
			std::pair{bound.lower, false},
			std::pair{static_cast<std::int16_t>(bound.lower + 1), true},
			std::pair{static_cast<std::int16_t>(bound.upper - 1), true},
			std::pair{bound.upper, false},
		}) {
			const auto cur = bound.x_axis
				? pc98::th0405::SubpixelPoint{position, 100}
				: pc98::th0405::SubpixelPoint{100, position};
			ShotUpdateState shot_state{};
			shot_state.shots[0] = {shot_alive, 255, {cur, {3, 4}, {}}, 0, 0, 0, 0};
			if(shots_update(shot_state) != ShotUpdateResult::complete ||
				shot_state.shots[0].pos.prev != cur || shot_state.shots[0].pos.cur != cur ||
				shot_state.shots[0].flag != (in_bounds ? shot_alive : shot_remove) ||
				shot_state.shots[0].age != (in_bounds ? 0 : 255) ||
				shot_state.shots_alive_count != (in_bounds ? 1 : 0)) return false;

			ShotUpdateState hitshot_state{};
			hitshot_state.hitshot_state.hitshots[0] = {1, 99, {cur, {5, 6}, {}}};
			if(shots_update(hitshot_state) != ShotUpdateResult::complete ||
				hitshot_state.hitshot_state.hitshots[0].pos.prev != cur ||
				hitshot_state.hitshot_state.hitshots[0].pos.cur != cur ||
				hitshot_state.hitshot_state.hitshots[0].age != (in_bounds ? 2 : 18) ||
				hitshot_state.hitshot_state.hitshots[0].patnum != 99) return false;
		}
	}

	ShotUpdateState patnum_wrap{};
	patnum_wrap.hitshot_state.hitshots[0] = {12, 255, {{100, 200}, {}, {}}};
	if(shots_update(patnum_wrap) != ShotUpdateResult::complete ||
		patnum_wrap.hitshot_state.hitshots[0].age != 13 ||
		patnum_wrap.hitshot_state.hitshots[0].patnum != 0) return false;
	return true;
}

bool test_velocity_and_homing()
{
	for(unsigned angle = 0; angle < 256; ++angle) {
		Shot shot{};
		shot_velocity_set(shot, static_cast<std::uint8_t>(angle));
		const auto expected = pc98::masterlib::vector2(static_cast<std::uint8_t>(angle), 192);
		if(shot.angle != angle || shot.pos.velocity != pc98::th0405::SubpixelPoint{
			expected.x, expected.y
		}) return false;
	}

	ShotUpdateState turn{};
	turn.homing_target = {400, 160};
	turn.shots[0] = {shot_alive, 15, {{160, 160}, {}, {}}, 0, shot_homing, 0, 128};
	if(shots_update(turn) != ShotUpdateResult::complete || turn.shots[0].type != shot_homing ||
		turn.shots[0].angle != 160 || turn.shots[0].age != 16 || turn.shots_alive_count != 1) return false;
	const auto expected_turn = pc98::masterlib::vector2(160, 192);
	if(turn.shots[0].pos.velocity != pc98::th0405::SubpixelPoint{
		expected_turn.x, expected_turn.y
	}) return false;

	ShotUpdateState expires{};
	expires.homing_target = {400, 160};
	expires.shots[0] = {shot_alive, 16, {{160, 160}, {}, {}}, 0, shot_homing, 0, 2};
	if(shots_update(expires) != ShotUpdateResult::complete ||
		expires.shots[0].type != shot_normal || expires.shots[0].angle != 0 ||
		expires.shots[0].age != 17 || expires.shots_alive_count != 1) return false;

	ShotUpdateState no_target{};
	no_target.homing_target = {homing_target_none, 777};
	no_target.shots[0] = {shot_alive, 16, {{100, 200}, {}, {5, -6}}, 0,
		shot_homing, 0, 99};
	if(shots_update(no_target) != ShotUpdateResult::complete ||
		no_target.shots[0].type != shot_normal || no_target.shots[0].angle != 99 ||
		no_target.shots[0].pos.velocity != pc98::th0405::SubpixelPoint{5, -6} ||
		no_target.shots[0].pos.cur != pc98::th0405::SubpixelPoint{105, 194}) return false;
	return true;
}

bool test_homing_all_headings()
{
	for(unsigned heading = 0; heading < 256; ++heading) {
		ShotUpdateState state{};
		state.homing_target = {4000, 3000}; // Cardinal right: source iatan2 angle 0.
		state.shots[0] = {shot_alive, 0, {{3000, 3000}, {1, 2}, {}}, 0,
			shot_homing, 0, static_cast<std::uint8_t>(heading)};
		if(shots_update(state) != ShotUpdateResult::complete ||
			state.shots[0].type != shot_homing || state.shots[0].age != 1 ||
			state.shots_alive_count != 1) return false;
		std::uint8_t expected{};
		if(heading >= 128U) {
			const auto turn = (256U - heading) / 4U;
			expected = turn < 3U ? 0U : static_cast<std::uint8_t>(heading + turn);
		} else {
			const auto turn = heading / 4U;
			expected = turn < 3U ? 0U : static_cast<std::uint8_t>(heading - turn);
		}
		if(state.shots[0].angle != expected ||
			state.shots[0].pos.prev != pc98::th0405::SubpixelPoint{3000, 3000}) return false;
	}
	return true;
}

bool test_iatan2_fault_preserves_prefix()
{
	ShotUpdateState state{};
	for(auto &entry : state.shots_alive) entry = {{-333, 444}, 62};
	state.homing_target = {-32768, 128};
	state.shots[0] = {shot_alive, 5, {{100, 200}, {1, 2}, {3, 4}}, 0, 0, 0, 0};
	state.shots[1] = {shot_alive, 16, {{0, 0}, {8, 9}, {10, 11}}, 0, shot_homing, 0, 7};
	state.shots[2] = {shot_alive, 6, {{300, 300}, {12, 13}, {14, 15}}, 0, 0, 0, 0};
	state.hitshot_state.hitshots[0] = {1, 66, {{100, 100}, {2, 3}, {4, 5}}};
	const auto before = state;
	if(shots_update(state) != ShotUpdateResult::iatan2_fault) return false;
	if(state.shots_alive_count != 1 || state.shots_alive[0] != ShotAlive{{103, 204}, 0} ||
		state.shots_alive[1] != before.shots_alive[1] ||
		state.shots[0].pos != pc98::th0405::Motion{{103, 204}, {100, 200}, {3, 4}} ||
		state.shots[0].age != 6 || state.shots[1].type != shot_normal ||
		state.shots[1].pos != before.shots[1].pos || state.shots[1].age != 16 ||
		state.shots[2] != before.shots[2] ||
		state.hitshot_state != before.hitshot_state) return false;
	return true;
}

bool test_hitshot_update()
{
	for(unsigned age = 0; age < 256; ++age) {
		ShotUpdateState state{};
		auto &hitshot = state.hitshot_state.hitshots[7];
		hitshot = {static_cast<std::uint8_t>(age), 50, {{100, 200}, {3, 4}, {2, -3}}};
		const auto before = state;
		if(shots_update(state) != ShotUpdateResult::complete) return false;
		auto expected = before;
		auto &e = expected.hitshot_state.hitshots[7];
		if(age >= hitshot_frames + 1U) {
			e.age = 0;
		} else if(age != 0) {
			e.pos.prev = {100, 200};
			e.pos.cur = {102, 197};
			e.age = static_cast<std::uint8_t>(age + 1U);
			if((e.age % hitshot_frames_per_cel) == 1U) ++e.patnum;
		}
		if(state != expected) return false;
	}

	ShotUpdateState clipped{};
	clipped.hitshot_state.hitshots[0] = {1, 70, {{6271, 100}, {1, 2}, {1, 0}}};
	if(shots_update(clipped) != ShotUpdateResult::complete ||
		clipped.hitshot_state.hitshots[0] != HitShot{18, 70, {{6272, 100}, {6271, 100}, {1, 0}}}) return false;
	const auto removed = clipped.hitshot_state.hitshots[0];
	if(shots_update(clipped) != ShotUpdateResult::complete ||
		clipped.hitshot_state.hitshots[0].age != 0 ||
		clipped.hitshot_state.hitshots[0].pos != removed.pos ||
		clipped.hitshot_state.hitshots[0].patnum != removed.patnum) return false;
	return true;
}

bool test_cache_order_and_stale_tail()
{
	ShotUpdateState state{};
	for(auto &entry : state.shots_alive) entry = {{-234, 345}, 42};
	for(std::size_t i = 0; i < state.shots.size(); ++i) {
		state.shots[i] = {shot_alive, 0, {{static_cast<std::int16_t>(i), 10}, {}, {}}, 0, 0, 0, 0};
	}
	if(shots_update(state) != ShotUpdateResult::complete || state.shots_alive_count != shot_count) return false;
	for(std::size_t i = 0; i < state.shots.size(); ++i) {
		if(state.shots_alive[i] != ShotAlive{{static_cast<std::int16_t>(i), 10},
			static_cast<std::uint16_t>(i)}) return false;
		state.shots[i].flag = (i % 3U == 0U) ? shot_alive : shot_free;
	}
	const auto cache_before = state.shots_alive;
	if(shots_update(state) != ShotUpdateResult::complete || state.shots_alive_count != 22) return false;
	for(std::size_t i = 0; i < 22; ++i) {
		if(state.shots_alive[i].shot_index != i * 3U) return false;
	}
	for(std::size_t i = 22; i < state.shots_alive.size(); ++i) {
		if(state.shots_alive[i] != cache_before[i]) return false;
	}
	return true;
}
}

int main()
{
	if(!test_free_and_occupied() || !test_sprite_and_division() || !test_invalid_cursor() ||
		!test_update_flag_lifetime_and_cache() || !test_update_missiles_and_bounds() ||
		!test_all_update_bounds() || !test_velocity_and_homing() ||
		!test_homing_all_headings() || !test_iatan2_fault_preserves_prefix() ||
		!test_hitshot_update() || !test_cache_order_and_stale_tail()) {
		std::cerr << "TH05 hitshot test failed\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
