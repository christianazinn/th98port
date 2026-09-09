#include "th05/core/stage4_projectiles.hpp"
#include "th05/core/state.hpp"

#include <array>
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
	state.playchar = 0U;
	state.stage_id = 4U;
	return state;
}

void add_hitting_shot(State &state, Point position, std::int8_t damage) noexcept
{
	auto &shot = state.shots.shots[0];
	shot.flag = shot_alive;
	shot.damage = damage;
	shot.pos.cur = position;
	state.shots.shots_alive[0] = {position, 0U};
	state.shots.shots_alive_count = 1U;
}

[[nodiscard]] bool b4ball_spawn_reset_and_clip()
{
	auto state = source_state();
	auto &template_ = state.stage4.custom.b4ball_template;
	template_.origin = {2000, 1000};
	template_.angle = 0U;
	template_.speed = 32U;
	template_.patnum_tiny_base = 216;
	template_.hp = 17;
	template_.revenge = true;
	auto &ball = state.stage4.custom.b4balls[0];
	ball.age = 0xA55AU;
	ball.pos.prev = {111, 222};
	stage4_b4balls_add(state);
	if(
		ball.flag != 1U || ball.angle != 0U || ball.pos.cur != template_.origin ||
		ball.pos.velocity != Point{40, 0} || ball.age != 0xA55AU ||
		ball.pos.prev != Point{111, 222} || ball.speed != 32U ||
		ball.patnum_tiny_base != 216 || ball.hp != 17 || !ball.revenge ||
		ball.damage_this_frame != 0
	) return false;

	ball.damage_this_frame = 9;
	stage4_b4balls_reset(state);
	if(ball.flag != 0U || ball.age != 0xA55AU || ball.damage_this_frame != 9) return false;

	state = source_state();
	ball = state.stage4.custom.b4balls[0];
	ball.flag = 1U;
	ball.age = 9U;
	ball.pos.cur = {-256, 1000};
	Trace trace{};
	if(!stage4_b4balls_update(state, trace)) return false;
	return ball.flag == 0U && ball.age == 10U && ball.pos.prev == Point{-256, 1000};
}

[[nodiscard]] bool b4ball_hit_decay_and_render_tail()
{
	auto state = source_state();
	Trace trace{};
	auto &ball = state.stage4.custom.b4balls[0];
	ball.flag = 1U;
	ball.pos.cur = {2000, 1000};
	ball.pos.velocity = {7, -7};
	ball.patnum_tiny_base = 212;
	ball.hp = 4;
	ball.revenge = true;
	add_hitting_shot(state, {2007, 993}, 5);
	state.bullets.bullet_template.spawn_type = bullet_spawn_type_normal;
	state.bullets.bullet_template.group = static_cast<std::uint8_t>(BulletGroup::single);
	state.bullets.bullet_template.speed = 32U;
	if(!stage4_b4balls_update(state, trace)) return false;
	if(
		ball.flag != 2U || ball.age != 0U || ball.patnum_tiny_base != 224 ||
		ball.pos.velocity != Point{1, -1} || ball.damage_this_frame != 1 ||
		state.score.score_delta != 555U || trace.sounds != std::vector<std::uint8_t>{4U, 3U} ||
		state.shots.shots[0].flag != shot_remove ||
		state.bullets.pool.pellets[179].flag != bullet_alive ||
		state.bullets.pool.pellets[179].pos.cur != Point{2007, 993}
	) return false;
	stage4_b4balls_render_tail(state);
	if(ball.damage_this_frame != 1) return false;

	ball.age = 3U;
	ball.patnum_tiny_base = 227;
	if(!stage4_b4balls_update(state, trace)) return false;
	if(ball.flag != 0U || ball.patnum_tiny_base != 228) return false;

	state = source_state();
	trace = {};
	ball = state.stage4.custom.b4balls[0];
	ball.flag = 1U;
	ball.pos.cur = {2000, 1000};
	ball.patnum_tiny_base = 216;
	ball.hp = 100;
	add_hitting_shot(state, ball.pos.cur, 5);
	if(!stage4_b4balls_update(state, trace)) return false;
	if(
		ball.flag != 1U || ball.hp != 100 || ball.damage_this_frame != 0 ||
		state.score.score_delta != 5U || trace.sounds != std::vector<std::uint8_t>{10U} ||
		state.shots.shots[0].flag != shot_remove
	) return false;

	state = source_state();
	auto &snow = state.stage4.custom.b4balls[0];
	auto &flash = state.stage4.custom.b4balls[1];
	auto &decay = state.stage4.custom.b4balls[2];
	snow.flag = flash.flag = decay.flag = 1U;
	snow.patnum_tiny_base = 212;
	flash.patnum_tiny_base = 220;
	decay.patnum_tiny_base = 224;
	snow.damage_this_frame = 1;
	flash.damage_this_frame = 2;
	decay.damage_this_frame = 3;
	stage4_b4balls_render_tail(state);
	return snow.damage_this_frame == 0 && flash.damage_this_frame == 2 &&
		decay.damage_this_frame == 3;
}

[[nodiscard]] bool cheeto_add_update_and_clip()
{
	auto state = source_state();
	CheetoTemplate template_{};
	template_.origin = {1000, 1000};
	template_.angle = 0x79U;
	template_.speed = 5U;
	template_.color = 11;
	state.stage4.custom.b4ball_template.patnum_tiny_base = 0x7A00;
	auto &head = state.stage4.custom.b4balls[0];
	auto &trail = state.stage4.cheeto_trails[0];
	head.age = 19U;
	head.pos.prev = {123, 456};
	head.revenge = true;
	head.hp = 99;
	head.damage_this_frame = 17;
	stage4_cheetos_add(state, template_);
	if(
		trail.flag != CheetoFlag::decelerate || trail.color != 11 ||
		head.flag != 0U || head.angle != 0x79U || head.speed != 5U ||
		head.patnum_tiny_base != 0 || head.pos.cur != template_.origin || head.age != 19U ||
		head.pos.prev != Point{123, 456} || trail.nodes.front() != template_.origin ||
		trail.nodes.back() != template_.origin || trail.sprites.front() != 0U ||
		trail.sprites.back() != 0U || !head.revenge || head.hp != 99 ||
		head.damage_this_frame != 17 ||
		state.stage4.custom.b4ball_template.patnum_tiny_base != 0x7A0B
	) return false;

	state = source_state();
	state.player_pos.cur = {2000, 1000};
	auto &moving_head = state.stage4.custom.b4balls[0];
	auto &moving_trail = state.stage4.cheeto_trails[0];
	moving_head.pos.cur = {1000, 1000};
	moving_head.pos.velocity = {5, 0};
	moving_head.angle = 0U;
	moving_head.speed = 5U;
	moving_trail.flag = CheetoFlag::decelerate;
	moving_trail.nodes.fill(moving_head.pos.cur);
	moving_trail.sprites.fill(0U);
	if(!stage4_cheetos_update(state)) return false;
	if(
		moving_head.age != 1U || moving_head.pos.prev != Point{1000, 1000} ||
		moving_head.pos.cur != Point{1005, 1000} || moving_head.speed != 4U ||
		moving_trail.flag != CheetoFlag::speedup || moving_head.angle != 0U ||
		moving_head.pos.velocity != Point{4, 0} || moving_head.patnum_tiny_base != 0 ||
		moving_trail.nodes.front() != Point{1000, 1000}
	) return false;

	state = source_state();
	head = state.stage4.custom.b4balls[0];
	trail = state.stage4.cheeto_trails[0];
	head.flag = static_cast<std::uint8_t>(CheetoFlag::speedup);
	trail.flag = CheetoFlag::decelerate;
	trail.nodes.fill({-256, 1000});
	if(!stage4_cheetos_update(state)) return false;
	if(trail.flag != CheetoFlag::free || head.flag != 0U || head.age != 1U) return false;

	state = source_state();
	state.player_pos.cur = {1000, 500};
	head = state.stage4.custom.b4balls[0];
	trail = state.stage4.cheeto_trails[0];
	head.pos.cur = {1000, 1000};
	head.angle = 0U;
	head.speed = 223U;
	trail.flag = CheetoFlag::speedup;
	trail.nodes.fill(head.pos.cur);
	if(stage4_cheetos_update(state) || head.age != 1U || head.speed != 224U) return false;
	return true;
}

[[nodiscard]] bool stage6_cheeto_overlay()
{
	auto state = source_state();
	CheetoTemplate template_{};
	template_.origin = {1000, 1000};
	template_.angle = 0x79U;
	template_.speed = 5U;
	template_.color = 11;
	state.b6balls.b6ball_template.patnum_tiny = 0x7A00;
	auto &head = state.b6balls.pool[0];
	auto &trail = state.stage4.cheeto_trails[0];
	head.age = 19U;
	head.pos.prev = {123, 456};
	head.cloud_radius = 456;
	head.decay_frame = 17;
	head.unused_2 = {1, 2, 3, 4};
	stage6_cheetos_add(state, template_);
	if(
		trail.flag != CheetoFlag::decelerate || trail.color != 11 ||
		head.flag != B6BallFlag::free || head.unused_1 != 0x79 || head.age != 19U ||
		head.pos.cur != template_.origin || head.pos.prev != Point{123, 456} ||
		head.patnum_tiny != 0 || head.unused_2 != std::array<std::int8_t, 4U>{1, 2, 5, 4} ||
		head.cloud_radius != 456 || head.decay_frame != 17 ||
		state.b6balls.b6ball_template.patnum_tiny != 0x7A0B
	) return false;

	state = source_state();
	state.player_pos.cur = {2000, 1000};
	auto &moving_head = state.b6balls.pool[0];
	auto &moving_trail = state.stage4.cheeto_trails[0];
	moving_head.pos.cur = {1000, 1000};
	moving_head.pos.velocity = {5, 0};
	moving_head.unused_1 = 0U;
	moving_head.unused_2[2] = 5;
	moving_trail.flag = CheetoFlag::decelerate;
	moving_trail.nodes.fill(moving_head.pos.cur);
	moving_trail.sprites.fill(0U);
	if(!stage6_cheetos_update(state)) return false;
	return moving_head.age == 1U && moving_head.pos.prev == Point{1000, 1000} &&
		moving_head.pos.cur == Point{1005, 1000} && moving_head.unused_2[2] == 4 &&
		moving_trail.flag == CheetoFlag::speedup && moving_head.patnum_tiny == 0;
}

} // namespace

int main()
{
	if(!b4ball_spawn_reset_and_clip()) {
		std::cerr << "b4ball spawn/reset/clip\n";
		return EXIT_FAILURE;
	}
	if(!b4ball_hit_decay_and_render_tail()) {
		std::cerr << "b4ball hit/decay/render\n";
		return EXIT_FAILURE;
	}
	if(!cheeto_add_update_and_clip()) {
		std::cerr << "cheeto add/update/clip\n";
		return EXIT_FAILURE;
	}
	if(!stage6_cheeto_overlay()) {
		std::cerr << "stage6 cheeto overlay\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
