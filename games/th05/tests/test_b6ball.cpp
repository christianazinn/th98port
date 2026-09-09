#include "th05/core/b6ball.hpp"
#include "th05/core/state.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>

namespace {

using namespace th05::core;
using Point = pc98::th0405::SubpixelPoint;

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.player_pos.cur = {5000, 5000};
	state.resources.playperf = 48U;
	state.resources.playperf_max = 48U;
	return state;
}

[[nodiscard]] bool spawn_full_pool_and_cloud_transition()
{
	auto state = source_state();
	auto &template_ = state.b6balls.b6ball_template;
	template_.origin = {2000, 1000};
	template_.angle = 0U;
	template_.speed = 32U;
	template_.patnum_tiny = 201;
	auto &ball = state.b6balls.pool[0];
	ball.age = 0xA55AU;
	ball.pos.prev = {123, 456};
	ball.decay_frame = 77;
	ball.unused_1 = 9;
	ball.unused_2 = {1, 2, 3, 4};
	b6balls_add(state);
	if(
		ball.flag != B6BallFlag::cloud || ball.pos.cur != template_.origin ||
		ball.pos.prev != Point{123, 456} || ball.pos.velocity != Point{40, 0} ||
		ball.patnum_tiny != 201 || ball.cloud_radius != 768 || ball.age != 0xA55AU ||
		ball.decay_frame != 77 || ball.unused_1 != 9 ||
		ball.unused_2 != std::array<std::int8_t, 4U>{1, 2, 3, 4}
	) return false;

	for(auto &slot : state.b6balls.pool) slot.flag = B6BallFlag::alive;
	const auto before = state.b6balls.pool;
	b6balls_add(state);
	if(state.b6balls.pool != before) return false;

	state = source_state();
	ball = state.b6balls.pool[0];
	ball.flag = B6BallFlag::cloud;
	ball.cloud_radius = 256;
	ball.age = 4U;
	ball.pos.cur = {1000, 1000};
	ball.pos.velocity = {10, 0};
	b6balls_update(state);
	return ball.cloud_radius == 224 && ball.flag == B6BallFlag::alive && ball.age == 5U &&
		ball.pos.prev == Point{1000, 1000} && ball.pos.cur == Point{1010, 1000};
}

[[nodiscard]] bool clear_hit_decay_and_clip()
{
	{
		auto state = source_state();
		auto &ball = state.b6balls.pool[0];
		ball.flag = B6BallFlag::alive;
		ball.patnum_tiny = 200;
		ball.pos.cur = {2000, 1000};
		ball.pos.velocity = {-7, 7};
		state.resources.bullet_clear_time = 1U;
		b6balls_update(state);
		if(
			ball.flag != B6BallFlag::decay || ball.patnum_tiny != 204 ||
			ball.pos.cur != Point{1993, 1007} || ball.pos.velocity != Point{-3, 3} ||
			ball.decay_frame != 0
		) return false;
	}

	{
		auto state = source_state();
		auto &ball = state.b6balls.pool[0];
		ball.flag = B6BallFlag::alive;
		ball.patnum_tiny = 200;
		ball.pos.cur = {2000, 1000};
		ball.pos.velocity = {7, -7};
		state.player_pos.cur = ball.pos.cur;
		b6balls_update(state);
		if(
			!state.player_is_hit || ball.flag != B6BallFlag::decay ||
			ball.patnum_tiny != 200 || ball.decay_frame != 0
		) return false;
		state.player_is_hit = false;
		state.player_pos.cur = {5000, 5000};
		b6balls_update(state);
		if(
			ball.patnum_tiny != 204 || ball.pos.velocity != Point{3, -3} ||
			ball.decay_frame != 0
		) return false;
		ball.decay_frame = 3;
		b6balls_update(state);
		if(ball.flag != B6BallFlag::decay || ball.patnum_tiny != 205) return false;
		ball.patnum_tiny = 207;
		ball.decay_frame = 3;
		b6balls_update(state);
		if(ball.flag != B6BallFlag::free || ball.patnum_tiny != 208) return false;
	}

	{
		auto state = source_state();
		auto &ball = state.b6balls.pool[0];
		ball.flag = B6BallFlag::alive;
		ball.pos.cur = {-256, 1000};
		b6balls_update(state);
		if(ball.flag != B6BallFlag::free) return false;
	}

	auto state = source_state();
	state.b6balls.pool[0].flag = B6BallFlag::alive;
	const auto before = state.b6balls;
	b6balls_render_tail(state);
	return state.b6balls == before;
}

} // namespace

int main()
{
	if(!spawn_full_pool_and_cloud_transition()) {
		std::cerr << "b6ball spawn/cloud\n";
		return EXIT_FAILURE;
	}
	if(!clear_hit_decay_and_clip()) {
		std::cerr << "b6ball clear/hit/decay/clip\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
