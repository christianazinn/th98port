#include "th04/core/state.hpp"

#include "pc98/masterlib_vector.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace {
using namespace th04::core;

struct Trace final : Presentation {
	std::array<HudField, 8> huds{};
	std::size_t hud_count{};
	std::array<std::uint8_t, 8> popups{};
	std::size_t popup_count{};
	std::array<std::uint16_t, 8> point_values{};
	std::size_t point_count{};

	void hud(HudField field) noexcept override { huds[hud_count++] = field; }
	void popup(std::uint8_t id) noexcept override { popups[popup_count++] = id; }
	void pointnum(Point, std::uint16_t value, bool) noexcept override {
		point_values[point_count++] = value;
	}
};

struct Fixture {
	State state{};
	Trace trace{};

	Fixture()
	{
		state.rank = 1U;
		state.resources.playperf = 16U;
		state.resources.playperf_max = 20U;
		state.player_pos.cur = {3000, 4000};
		state.bullets.graze_score = 10U;
	}
};

bool tuning_and_regular_spawning()
{
	Fixture f;
	auto &bullet_template = f.state.bullets.template_;
	bullet_template.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
	bullet_template.patnum = 40U;
	bullet_template.origin = {2000, 2000};
	bullet_template.group = BulletGroup::spread;
	bullet_template.speed = 64U;
	bullet_template.count = 3U;
	bullet_template.delta = 6U;
	if(!bullet_template_tune(f.state) || !bullets_add(f.state)) return false;
	if(bullet_template.count != 3U || bullet_template.speed != 64U) return false;
	for(const auto index : std::array<std::size_t, 3>{239U, 238U, 237U}) {
		const auto &bullet = f.state.bullets.pool[index];
		if(bullet.flag != bullet_alive || bullet.pos.cur != Point{2000, 2000} ||
			bullet.speed_final != 64U || bullet.move_flag != BulletMoveFlag::regular ||
			bullet.state_count != 32U || bullet.state_value != 8U) return false;
	}
	if(f.state.bullets.pool[236].flag != bullet_free) return false;

	Fixture extra;
	extra.state.rank = 4U;
	extra.state.bullets.template_.group = BulletGroup::spread;
	extra.state.bullets.template_.count = 5U;
	extra.state.bullets.template_.delta = 6U;
	if(!bullet_template_tune(extra.state) || extra.state.bullets.template_.count != 5U) {
		return false;
	}
	return true;
}

bool fixed_speed_and_shared_random()
{
	Fixture f;
	f.state.resources.playperf = 24U;
	f.state.bullets.template_ = {
		static_cast<std::uint8_t>(BulletSpawnType::pellet), 40U, {2000, 2000}, {},
		BulletGroup::random_angle, 0U, 64U, 1U, 0U, 0U, BulletSpecialMotion::none, 0U,
	};
	f.state.random.ring[0] = 0x34U;
	f.state.random.ring[1] = 0x12U;
	if(!bullets_add(f.state, false, true)) return false;
	const auto &bullet = f.state.bullets.pool[239];
	const auto expected_velocity = pc98::masterlib::vector2(0x34U, 64);
	return f.state.random.cursor == 1U && bullet.speed_final == 64U &&
		bullet.pos.velocity == Point{expected_velocity.x, expected_velocity.y};
}

bool gather_release()
{
	Fixture f;
	f.state.bullets.template_ = {
		static_cast<std::uint8_t>(BulletSpawnType::gather_pellet), 40U, {2000, 2000}, {},
		BulletGroup::single, 0U, 64U, 1U, 0U, 0U, BulletSpecialMotion::none, 0U,
	};
	if(!bullets_add(f.state)) return false;
	if(f.state.bullets.gathers[0].flag != bullet_alive ||
		f.state.bullets.template_.spawn_type != static_cast<std::uint8_t>(BulletSpawnType::pellet)) {
		return false;
	}
	for(int frame = 0; frame < 32; ++frame) {
		if(!gathers_update(f.state)) return false;
	}
	return f.state.bullets.gathers[0].flag == bullet_remove &&
		f.state.bullets.pool[239].flag == bullet_alive;
}

bool graze_clear_special_and_zap()
{
	Fixture f;
	auto &bullet = f.state.bullets.pool[439];
	bullet.flag = bullet_alive;
	bullet.pos.cur = f.state.player_pos.cur;
	bullet.spawn_flag = BulletSpawnFlag::grazeable;
	bullet.move_flag = BulletMoveFlag::regular;
	if(!bullets_update(f.state, f.trace) || bullet.spawn_flag != BulletSpawnFlag::grazed ||
		f.state.bullets.stage_graze != 1U || f.state.score.score_delta != 10U ||
		f.state.random.cursor != 2U || f.trace.hud_count != 1U ||
		f.trace.huds[0] != HudField::graze) return false;
	if(!bullets_update(f.state, f.trace) || bullet.flag != bullet_remove ||
		!f.state.player_is_hit) return false;

	Fixture clear;
	auto &decay = clear.state.bullets.pool[439];
	decay.flag = bullet_alive;
	decay.pos.cur = {2000, 2000};
	decay.spawn_flag = BulletSpawnFlag::grazeable;
	decay.move_flag = BulletMoveFlag::regular;
	clear.state.resources.bullet_clear_time = 20U;
	if(!bullets_update(clear.state, clear.trace) ||
		decay.move_flag != BulletMoveFlag::decay || decay.patnum != 112 ||
		clear.state.score.score_delta != 10U || clear.state.resources.bullet_clear_time != 19U) {
		return false;
	}

	Fixture cloud;
	auto &cloud_backwards = cloud.state.bullets.pool[439];
	cloud_backwards.flag = bullet_alive;
	cloud_backwards.pos.cur = {2000, 2000};
	cloud_backwards.pos.velocity = {16, 0};
	cloud_backwards.spawn_flag = BulletSpawnFlag::cloud_backwards;
	cloud_backwards.move_flag = BulletMoveFlag::regular;
	if(!bullets_update(cloud.state, cloud.trace) || cloud_backwards.pos.prev != Point{2000, 2000} ||
		cloud_backwards.pos.cur != Point{1872, 2000} ||
		static_cast<std::uint8_t>(cloud_backwards.spawn_flag) != 5U) return false;

	Fixture special;
	auto &speedup = special.state.bullets.pool[439];
	speedup.flag = bullet_alive;
	speedup.pos.cur = {2000, 2000};
	speedup.spawn_flag = BulletSpawnFlag::grazed;
	speedup.move_flag = BulletMoveFlag::special;
	speedup.special_motion = BulletSpecialMotion::speedup;
	speedup.speed_cur = 16U;
	special.state.bullets.special.value = 1U;
	if(!bullets_update(special.state, special.trace) || speedup.speed_cur != 17U ||
		speedup.pos.cur.x != 2016) return false;

	Fixture zap;
	auto &zapped = zap.state.bullets.pool[439];
	zapped.flag = bullet_alive;
	zapped.pos.cur = {2000, 2000};
	zap.state.bullets.zap.value = 16U;
	if(!bullets_update(zap.state, zap.trace) || zapped.flag != bullet_remove ||
		zap.state.bullets.zap.value != 0U || zap.state.score.score_delta != 1U ||
		zap.state.bullets.overlay_popup_bonus != 1U || zap.trace.popup_count != 1U ||
		zap.trace.popups[0] != 2U || zap.trace.point_count != 1U ||
		zap.trace.point_values[0] != 1U) return false;
	return true;
}

} // namespace

int main()
{
	for(const auto &[name, test] : std::array{
		std::pair{"tuning/regular", tuning_and_regular_spawning},
		std::pair{"fixed speed/shared RNG", fixed_speed_and_shared_random},
		std::pair{"gather release", gather_release},
		std::pair{"graze/clear/special/zap", graze_clear_special_and_zap},
	}) {
		if(!test()) {
			std::cerr << "FAIL " << name << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
