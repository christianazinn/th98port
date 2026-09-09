#include "th05/core/bullet.hpp"
#include "th05/core/state.hpp"

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
