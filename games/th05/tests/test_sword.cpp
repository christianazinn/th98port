#include "th05/core/sword.hpp"
#include "th05/core/state.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using namespace th05::core;
using Point = pc98::th0405::SubpixelPoint;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.player_pos.cur = {5000, 5000};
	state.resources.playperf = 48U;
	state.resources.playperf_max = 48U;
	return state;
}

[[nodiscard]] bool spawn_and_full_pool_color_write()
{
	auto state = source_state();
	auto &template_ = state.swords.sword_template;
	template_.origin = {2000, 1000};
	template_.angle = 0xFFU;
	template_.speed = 32U;
	template_.twirl_time = 17U;
	template_.patnum_tiny = 999;
	auto &sword = state.swords.pool[0];
	sword.pos.prev = {123, 456};
	sword.decay_frame = 77;
	state.circles_color = 3U;
	swords_add(state);
	if(
		state.circles_color != 9U || sword.flag != sword_alive ||
		sword.pos.cur != template_.origin || sword.pos.prev != Point{123, 456} ||
		sword.pos.velocity != Point{40, -1} || sword.angle != 0xFFU ||
		sword.patnum_tiny != 193 || sword.speed != 40U || sword.twirl_time != 17U ||
		sword.decay_frame != 77
	) return false;

	for(auto &slot : state.swords.pool) slot.flag = sword_alive;
	const auto before = state.swords.pool;
	state.circles_color = 2U;
	swords_add(state);
	return state.circles_color == 9U && state.swords.pool == before;
}

[[nodiscard]] bool twirl_parity_and_end_frame_motion()
{
	auto state = source_state();
	Trace trace{};
	auto &clockwise = state.swords.pool[0];
	auto &counterclockwise = state.swords.pool[1];
	clockwise.flag = counterclockwise.flag = sword_alive;
	clockwise.patnum_tiny = counterclockwise.patnum_tiny = 193;
	clockwise.twirl_time = counterclockwise.twirl_time = 2U;
	clockwise.angle = counterclockwise.angle = 0U;
	clockwise.pos.cur = {1000, 1000};
	counterclockwise.pos.cur = {2000, 1000};
	clockwise.pos.velocity = {10, 0};
	counterclockwise.pos.velocity = {20, 0};
	swords_update(state, trace);
	if(
		clockwise.twirl_time != 1U || clockwise.patnum_tiny != 195 ||
		counterclockwise.twirl_time != 1U || counterclockwise.patnum_tiny != 223 ||
		clockwise.pos.cur != Point{1000, 1000} || counterclockwise.pos.cur != Point{2000, 1000} ||
		!trace.sounds.empty()
	) return false;

	swords_update(state, trace);
	return clockwise.twirl_time == 0U && counterclockwise.twirl_time == 0U &&
		clockwise.patnum_tiny == 193 && counterclockwise.patnum_tiny == 193 &&
		clockwise.pos.prev == Point{1000, 1000} && clockwise.pos.cur == Point{1010, 1000} &&
		counterclockwise.pos.prev == Point{2000, 1000} && counterclockwise.pos.cur == Point{2020, 1000} &&
		trace.sounds == std::vector<std::uint8_t>{3U, 3U};
}

[[nodiscard]] bool clear_hit_delay_decay_and_clip()
{
	{
		auto state = source_state();
		Trace trace{};
		auto &sword = state.swords.pool[0];
		sword.flag = sword_alive;
		sword.twirl_time = 99U;
		sword.patnum_tiny = 193;
		sword.pos.cur = {2000, 1000};
		sword.pos.velocity = {-7, 7};
		state.resources.bullet_clear_time = 1U;
		swords_update(state, trace);
		if(
			sword.flag != sword_remove || sword.twirl_time != 0U ||
			sword.pos.cur != Point{1993, 1007} || sword.patnum_tiny != 225 ||
			sword.pos.velocity != Point{-3, 3} || sword.decay_frame != 0 ||
			!trace.sounds.empty()
		) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		auto &sword = state.swords.pool[0];
		sword.flag = sword_alive;
		sword.patnum_tiny = 193;
		sword.pos.cur = {2000, 1000};
		state.player_pos.cur = sword.pos.cur;
		swords_update(state, trace);
		if(
			!state.player_is_hit || sword.flag != sword_remove ||
			sword.patnum_tiny != 193 || sword.decay_frame != 0
		) return false;
		state.player_is_hit = false;
		state.player_pos.cur = {5000, 5000};
		swords_update(state, trace);
		if(
			sword.flag != sword_remove || sword.patnum_tiny != 225 ||
			sword.decay_frame != 0
		) return false;
		sword.decay_frame = 3;
		swords_update(state, trace);
		if(sword.patnum_tiny != 226 || sword.flag != sword_remove) return false;
		sword.patnum_tiny = 228;
		sword.decay_frame = 3;
		swords_update(state, trace);
		if(sword.flag != sword_free || sword.patnum_tiny != 229) return false;
	}

	{
		auto state = source_state();
		Trace trace{};
		auto &sword = state.swords.pool[0];
		sword.flag = sword_alive;
		sword.pos.cur = {-256, 1000};
		swords_update(state, trace);
		if(sword.flag != sword_free) return false;
	}

	auto state = source_state();
	state.swords.pool[0].flag = sword_alive;
	state.swords.pool[0].patnum_tiny = 193;
	const auto before = state.swords;
	swords_render_tail(state);
	return state.swords == before;
}

} // namespace

int main()
{
	if(!spawn_and_full_pool_color_write()) {
		std::cerr << "sword spawn/full-pool color\n";
		return EXIT_FAILURE;
	}
	if(!twirl_parity_and_end_frame_motion()) {
		std::cerr << "sword twirl\n";
		return EXIT_FAILURE;
	}
	if(!clear_hit_delay_decay_and_clip()) {
		std::cerr << "sword clear/hit/decay/clip\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
