#include "th05/core/bullet.hpp"
#include "th05/core/state.hpp"

#include "pc98/masterlib_vector.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using namespace th05::core;
using Point = pc98::th0405::SubpixelPoint;

struct Trace final : Presentation {
	std::vector<HudField> huds{};
	std::vector<std::uint16_t> pointnums{};

	void hud(HudField field) noexcept override { huds.push_back(field); }
	void pointnum(Point, std::uint16_t value, bool) noexcept override
	{
		pointnums.push_back(value);
	}
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.player_pos.cur = {5000, 5000};
	state.resources.playperf = 32U;
	state.resources.playperf_max = 48U;
	state.rank = 1U;
	state.graze_score = 100U;
	return state;
}

[[nodiscard]] BulletTemplate base_template() noexcept
{
	return {
		bullet_spawn_type_normal, 0U, {2000, 2000},
		static_cast<std::uint8_t>(BulletGroup::single), 0U, 1U, 0U,
		1U, 0U, 0U, 32U,
	};
}

[[nodiscard]] bool template_tuning_and_spread_order()
{
	auto state = source_state();
	state.bullets.bullet_template = base_template();
	state.bullets.bullet_template.group = static_cast<std::uint8_t>(
		BulletGroup::spread
	);
	state.bullets.bullet_template.spread = 3U;
	state.bullets.bullet_template.spread_angle_delta = 16U;
	if(!bullet_template_tune(state) || !bullets_add_regular(state)) return false;
	const auto &pool = state.bullets.pool.pellets;
	return pool[179].flag == bullet_alive && pool[178].flag == bullet_alive &&
		pool[177].flag == bullet_alive && pool[176].flag == bullet_free &&
		pool[179].angle == 0U && pool[178].angle == 16U &&
		pool[177].angle == 240U && pool[179].speed_cur == 32U &&
		pool[178].speed_cur == 32U && pool[177].speed_cur == 32U &&
		state.bullets.bullet_template.angle == 0U &&
		state.bullets.bullet_template.speed == 32U;
}

[[nodiscard]] bool random_group_draw_order_and_full_pool()
{
	auto state = source_state();
	state.random.ring[0] = 0x10U;
	state.random.ring[1] = 0x01U;
	state.random.ring[2] = 0x00U;
	state.bullets.bullet_template = base_template();
	state.bullets.bullet_template.group = static_cast<std::uint8_t>(
		BulletGroup::random_angle_and_speed
	);
	state.bullets.bullet_template.spread = 1U;
	if(!bullets_add_regular(state)) return false;
	const auto &random = state.bullets.pool.pellets[179];
	if(random.speed_cur != 48U || random.angle != 1U || state.random.cursor != 2U) {
		return false;
	}
	for(auto &bullet : state.bullets.pool.pellets) bullet.flag = bullet_alive;
	state.random.cursor = 19U;
	if(!bullets_add_regular(state)) return false;
	return state.random.cursor == 19U;
}

[[nodiscard]] bool exact_linear_motion_and_gather_release()
{
	auto state = source_state();
	Trace trace{};
	state.bullets.bullet_template = base_template();
	state.bullets.bullet_template.special_motion = static_cast<std::uint8_t>(
		BulletSpecialMotion::exact_linear
	);
	state.bullets.bullet_template.speed = 16U;
	if(!bullets_add_special(state) || !bullets_update(state, trace)) return false;
	const auto &linear = state.bullets.pool.pellets[179];
	if(linear.move_flag != bullet_move_special || linear.distance != 16 ||
		linear.pos.cur != Point{2016, 2000}) return false;

	state = source_state();
	state.bullets.bullet_template = base_template();
	state.bullets.bullet_template.spawn_type = bullet_spawn_type_gather_pellet;
	if(!bullets_add_regular(state) || state.bullets.gathers[0].flag != bullet_alive ||
		state.bullets.bullet_template.spawn_type != bullet_spawn_type_gather_pellet) {
		return false;
	}
	for(std::size_t frame{}; frame < 32U; ++frame) {
		if(!gathers_update(state)) return false;
	}
	return state.bullets.gathers[0].flag == bullet_remove &&
		state.bullets.bullet_template.spawn_type == bullet_spawn_type_normal &&
		state.bullets.pool.pellets[179].flag == bullet_alive;
}

[[nodiscard]] bool hit_graze_and_zap_side_effects()
{
	auto state = source_state();
	Trace trace{};
	state.player_pos.cur = {1000, 1000};
	auto &graze = state.bullets.pool.pellets[179];
	graze.flag = bullet_alive;
	graze.move_flag = bullet_move_regular;
	graze.spawn_flag = bullet_spawn_grazeable;
	graze.pos.cur = {1256, 1000};
	if(!bullets_update(state, trace) || graze.spawn_flag != bullet_spawn_grazed ||
		state.bullets.stage_graze != 1U || state.score.score_delta != 100U ||
		trace.huds != std::vector<HudField>{HudField::graze} ||
		state.random.cursor != 2U) return false;

	state = source_state();
	state.player_pos.cur = {1000, 1000};
	auto &hit = state.bullets.pool.bullets16[219];
	hit.flag = bullet_alive;
	hit.move_flag = bullet_move_regular;
	hit.spawn_flag = bullet_spawn_grazed;
	hit.pos.cur = state.player_pos.cur;
	if(!bullets_update(state, trace) || hit.flag != bullet_remove || !state.player_is_hit) {
		return false;
	}

	state = source_state();
	Trace zap_trace{};
	auto &zapped = state.bullets.pool.pellets[179];
	zapped.flag = bullet_alive;
	zapped.pos.cur = {2000, 2000};
	state.bullets.zap.start();
	if(!bullets_update(state, zap_trace)) return false;
	return zapped.flag == bullet_remove && !state.bullets.zap.active() &&
		state.score.score_delta == 1U && state.bullets.popup_bonus == 1U &&
		zap_trace.pointnums == std::vector<std::uint16_t>{1U};
}

[[nodiscard]] bool gather_only_three_stack()
{
	auto state = source_state();
	state.bullets.gather_template.center = {1800, 1900};
	state.bullets.gather_template.radius = 256;
	state.bullets.gather_template.ring_points = 6;
	gather_add_only_3stack(state, 0, 3U, 7U);
	gather_add_only_3stack(state, 2, 3U, 7U);
	gather_add_only_3stack(state, 4, 3U, 7U);
	gather_add_only_3stack(state, 1, 3U, 7U);
	return state.bullets.gathers[0].flag == bullet_alive &&
		state.bullets.gathers[0].color == 3U &&
		state.bullets.gathers[1].color == 7U &&
		state.bullets.gathers[2].color == 7U &&
		state.bullets.gathers[0].bullet_template.spawn_type == 0xFFU &&
		state.bullets.gathers[3].flag == bullet_free;
}

[[nodiscard]] bool shared_special_motion_transitions()
{
	Trace trace{};
	{
		auto state = source_state();
		auto &bullet = state.bullets.pool.bullets16[219];
		bullet.flag = bullet_alive;
		bullet.move_flag = bullet_move_special;
		bullet.special_motion = static_cast<std::uint8_t>(
			BulletSpecialMotion::decelerate_then_turn
		);
		bullet.pos.cur = {2000, 2000};
		bullet.speed_cur = 1U;
		bullet.speed_final = 16U;
		bullet.angle = 0U;
		bullet.u2.value = 32U;
		bullet.patnum = 52U;
		state.bullets.special_turns_max = 1U;
		if(!bullets_update(state, trace) || bullet.speed_cur != 0U ||
			bullet.pos.cur != Point{2001, 2000}) return false;
		const auto turned = pc98::masterlib::vector2(32U, 16);
		if(!bullets_update(state, trace) || bullet.u1.value != 1U ||
			bullet.move_flag != bullet_move_regular || bullet.angle != 32U ||
			bullet.speed_cur != 16U || bullet.patnum != 56U ||
			bullet.pos.cur != Point{
				static_cast<std::int16_t>(2001 + turned.x),
				static_cast<std::int16_t>(2000 + turned.y)
			}) return false;
	}
	{
		auto state = source_state();
		auto &bullet = state.bullets.pool.bullets16[219];
		bullet.flag = bullet_alive;
		bullet.move_flag = bullet_move_special;
		bullet.special_motion = static_cast<std::uint8_t>(
			BulletSpecialMotion::decelerate_then_turn_aimed
		);
		bullet.pos.cur = {2000, 2000};
		bullet.speed_cur = 0U;
		bullet.speed_final = 16U;
		bullet.patnum = 84U;
		state.player_pos.cur = {2000, 3000};
		state.bullets.special_turns_max = 1U;
		const auto aimed = pc98::masterlib::vector2(64U, 16);
		if(!bullets_update(state, trace) || bullet.u1.value != 1U ||
			bullet.move_flag != bullet_move_regular || bullet.angle != 64U ||
			bullet.patnum != 92U || bullet.pos.cur != Point{
				static_cast<std::int16_t>(2000 + aimed.x),
				static_cast<std::int16_t>(2000 + aimed.y)
			}) return false;
	}
	{
		auto state = source_state();
		auto &speedup = state.bullets.pool.pellets[179];
		speedup.flag = bullet_alive;
		speedup.move_flag = bullet_move_special;
		speedup.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::speedup);
		speedup.pos.cur = {2000, 2000};
		speedup.speed_cur = 16U;
		state.bullets.special_turns_max = 2U;
		if(!bullets_update(state, trace) || speedup.speed_cur != 18U ||
			speedup.pos.cur != Point{2016, 2000}) return false;
	}
	{
		auto state = source_state();
		auto &decelerate = state.bullets.pool.pellets[179];
		decelerate.flag = bullet_alive;
		decelerate.move_flag = bullet_move_special;
		decelerate.special_motion = static_cast<std::uint8_t>(
			BulletSpecialMotion::decelerate_to_angle
		);
		decelerate.pos.cur = {2000, 2000};
		decelerate.speed_cur = 33U;
		decelerate.speed_final = 48U;
		decelerate.u2.value = 64U;
		if(!bullets_update(state, trace) || decelerate.speed_cur != 31U ||
			decelerate.angle != 16U || decelerate.pos.cur != Point{2033, 2000}) {
			return false;
		}
		decelerate.speed_cur = 0U;
		decelerate.pos.cur = {2000, 2000};
		const auto target = pc98::masterlib::vector2(64U, 48);
		if(!bullets_update(state, trace) ||
			decelerate.move_flag != bullet_move_regular ||
			decelerate.angle != 64U || decelerate.speed_cur != 48U ||
			decelerate.pos.cur != Point{
				static_cast<std::int16_t>(2000 + target.x),
				static_cast<std::int16_t>(2000 + target.y)
			}) return false;
	}
	{
		auto state = source_state();
		auto &bounce = state.bullets.pool.bullets16[219];
		bounce.flag = bullet_alive;
		bounce.move_flag = bullet_move_special;
		bounce.special_motion = static_cast<std::uint8_t>(
			BulletSpecialMotion::bounce_left_right_top_bottom
		);
		bounce.pos.cur = {0, 0};
		bounce.speed_cur = 16U;
		bounce.angle = 64U;
		bounce.patnum = 68U;
		state.bullets.special_turns_max = 2U;
		const auto velocity = pc98::masterlib::vector2(192U, 16);
		if(!bullets_update(state, trace) || bounce.u1.value != 2U ||
			bounce.move_flag != bullet_move_regular || bounce.angle != 192U ||
			bounce.patnum != 76U || bounce.pos.cur != Point{velocity.x, velocity.y}) {
			return false;
		}
	}
	{
		auto state = source_state();
		auto &gravity = state.bullets.pool.pellets[179];
		gravity.flag = bullet_alive;
		gravity.move_flag = bullet_move_special;
		gravity.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::gravity);
		gravity.pos.cur = {2000, 2000};
		gravity.pos.velocity = {3, 4};
		state.stage_frame = 1U;
		state.bullets.special_turns_max = 1U;
		if(!bullets_update(state, trace) || gravity.pos.velocity.y != 5 ||
			gravity.pos.cur != Point{2003, 2005}) return false;
		state.stage_frame = 2U;
		if(!bullets_update(state, trace) || gravity.pos.velocity.y != 5 ||
			gravity.pos.cur != Point{2006, 2010}) return false;
	}
	{
		auto state = source_state();
		auto &unknown = state.bullets.pool.pellets[179];
		unknown.flag = bullet_alive;
		unknown.move_flag = bullet_move_special;
		unknown.special_motion = 0xEEU;
		unknown.pos.cur = {2000, 2000};
		unknown.pos.velocity = {7, -3};
		if(!bullets_update(state, trace) || unknown.pos.cur != Point{2007, 1997}) {
			return false;
		}
	}
	return true;
}

} // namespace

int main()
{
	const struct {
		const char *name;
		bool (*run)();
	} tests[] = {
		{"template tuning and spread order", template_tuning_and_spread_order},
		{"random group draw order and full pool", random_group_draw_order_and_full_pool},
		{"exact linear and gather release", exact_linear_motion_and_gather_release},
		{"hit graze and zap side effects", hit_graze_and_zap_side_effects},
		{"gather only three stack", gather_only_three_stack},
		{"shared special motion transitions", shared_special_motion_transitions},
	};
	for(const auto &test : tests) {
		if(!test.run()) {
			std::cerr << "FAIL " << test.name << '\n';
			return 1;
		}
	}
	std::cout << "PASS " << (sizeof(tests) / sizeof(tests[0])) << " bullet tests\n";
	return 0;
}
