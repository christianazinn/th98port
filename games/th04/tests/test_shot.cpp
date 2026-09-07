#include "th04/core/shot.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace {
using namespace th04::core;

std::int16_t signed_word(int value)
{
	value %= 65536;
	if(value < -32768) value += 65536;
	if(value > 32767) value -= 65536;
	return static_cast<std::int16_t>(value);
}

bool test_every_flag_and_age()
{
	for(unsigned flag = 0; flag < 256; ++flag) {
		for(int age = -128; age < 128; ++age) {
			ShotState state{};
			auto &shot = state.shots[67];
			shot = {static_cast<std::uint8_t>(flag), static_cast<std::int8_t>(age),
				{{100, 200}, {10, 20}, {1, -2}}, 32767, -3, -4};
			const auto before = state;
			shots_update(state, {});
			auto expected = before;
			auto &e = expected.shots[67];
			if(flag >= 18) e.flag = 0;
			if(flag > 0 && flag < 18) {
				e.pos.prev = {100, 200};
				e.pos.cur = {101, 198};
				if(flag == 1) {
					e.age = static_cast<std::int8_t>(age == 127 ? -128 : age + 1);
					expected.shots_alive_count = 1;
					expected.shots_alive[0] = {{101, 198}, 67};
				} else {
					e.flag = static_cast<std::uint8_t>(flag + 1);
					if(flag == 5 || flag == 9 || flag == 13 || flag == 17) e.patnum_base = -32768;
				}
			}
			if(state != expected) return false;
		}
	}
	return true;
}

bool test_motion_bounds_and_retirement()
{
	for(int axis = 0; axis < 2; ++axis) {
		for(int position = -32768; position < 32768; ++position) {
			for(int delta : {-32768, -193, -1, 0, 1, 192, 32767}) {
				ShotState state{};
				auto &shot = state.shots[0];
				shot.flag = shot_alive;
				shot.age = 7;
				shot.patnum_base = 40;
				if(axis == 0) {
					shot.pos.cur.x = static_cast<std::int16_t>(position);
					shot.pos.velocity.x = static_cast<std::int16_t>(delta);
				} else {
					shot.pos.cur.y = static_cast<std::int16_t>(position);
					shot.pos.velocity.y = static_cast<std::int16_t>(delta);
				}
				const auto old = shot.pos.cur;
				const auto advanced = signed_word(position + delta);
				const bool in_bounds = advanced > -128 && advanced < (axis == 0 ? 6272 : 6016);
				shots_update(state, {});
				if(shot.pos.prev != old || (axis == 0 ? shot.pos.cur.x : shot.pos.cur.y) != advanced ||
					shot.flag != (in_bounds ? 1 : 18) || shot.age != (in_bounds ? 8 : 7) ||
					state.shots_alive_count != (in_bounds ? 1 : 0) || shot.patnum_base != 40) return false;
				if(!in_bounds) {
					const auto outside = shot.pos;
					shots_update(state, {});
					if(shot.flag != 0 || shot.pos != outside || shot.age != 7) return false;
				}
			}
		}
	}
	return true;
}

bool test_cache_order_and_stale_tail()
{
	ShotState state{};
	for(auto &entry : state.shots_alive) entry = {{-234, 345}, 42};
	state.shots_alive_count = 65535; // overwritten, never used as an input index
	for(std::size_t i = 0; i < state.shots.size(); ++i) {
		state.shots[i].flag = shot_alive;
		state.shots[i].pos.cur = {static_cast<std::int16_t>(i), 10};
	}
	shots_update(state, {});
	if(state.shots_alive_count != 68) return false;
	for(std::size_t i = 0; i < 68; ++i) {
		if(state.shots_alive[i] != ShotAlive{{static_cast<std::int16_t>(i), 10},
			static_cast<std::uint16_t>(i)}) return false;
	}
	for(std::size_t i = 0; i < 68; ++i) {
		state.shots[i].flag = (i % 3 == 0) ? shot_alive : shot_free;
	}
	const auto cache_before = state.shots_alive;
	shots_update(state, {});
	if(state.shots_alive_count != 23) return false;
	for(std::size_t i = 0; i < 23; ++i) if(state.shots_alive[i].shot_index != i * 3) return false;
	for(std::size_t i = 23; i < 68; ++i) if(state.shots_alive[i] != cache_before[i]) return false;
	return true;
}

bool test_hit_animation_and_laser()
{
	for(std::uint8_t flag = 2; flag < 18; ++flag) {
		ShotState outside{};
		outside.shots[0] = {flag, 5, {{6271, 0}, {}, {1, 0}}, 40, 9, 10};
		shots_update(outside, {});
		if(outside.shots[0].flag != 18 || outside.shots[0].patnum_base != 40 ||
			outside.shots[0].age != 5 || outside.shots_alive_count != 0) return false;
	}
	ShotState state{};
	auto &shot = state.shots[0];
	shot.flag = shot_hit;
	shot.patnum_base = 40;
	for(unsigned tick = 1; tick <= 16; ++tick) {
		shots_update(state, {});
		if(shot.flag != 2 + tick || shot.patnum_base != 40 + static_cast<int>(tick / 4) ||
			state.shots_alive_count != 0 || shot.age != 0) return false;
	}
	shots_update(state, {});
	if(shot.flag != 0 || shot.patnum_base != 44) return false;
	// All timer values, including the visually inactive cooldown, and stale velocity.
	for(unsigned time = 0; time < 65536; ++time) {
		state.shot_laser_time = static_cast<std::uint16_t>(time);
		state.shot_laser_bottomcenter = {{100, 200}, {300, 400}, {500, 600}};
		const auto before = state.shot_laser_bottomcenter;
		shots_update(state, {123, -456});
		if(state.shot_laser_time != (time == 0 ? 0 : time - 1)) return false;
		if(time == 0) {
			if(state.shot_laser_bottomcenter != before) return false;
		} else if(state.shot_laser_bottomcenter != pc98::th0405::Motion{{123, -456}, {100, 200}, {500, 600}}) {
			return false;
		}
	}
	return true;
}
} // namespace

int main()
{
	for(const auto &[name, check] : std::array{
		std::pair{"flags/ages", test_every_flag_and_age},
		std::pair{"motion/bounds", test_motion_bounds_and_retirement},
		std::pair{"cache", test_cache_order_and_stale_tail},
		std::pair{"animation/laser", test_hit_animation_and_laser}}) {
		if(!check()) {
			std::cerr << "FAIL " << name << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
