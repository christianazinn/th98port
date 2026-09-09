#include "th04/core/midboss4.hpp"

#include "th04/core/boss.hpp"
#include "th04/core/bullet.hpp"
#include "th04/core/item.hpp"
#include "th04/core/state.hpp"
#include "pc98/masterlib_iatan2.hpp"
#include "pc98/th0405_random.hpp"
#include "pc98/th0405_sparks.hpp"

#include <bit>
#include <cstdint>

namespace th04::core {
namespace {

constexpr std::uint8_t phase_explode_big = 254U;
constexpr std::uint8_t phase_none = 255U;
constexpr std::uint8_t pat_enemy_kill = 4U;
constexpr std::uint8_t pat_bullet16_small_ball_yellow = 58U;
constexpr std::uint8_t pat_bullet16_directional_yellow = 92U;
constexpr std::int16_t playfield_center_x = 3072;
constexpr std::int16_t playfield_width = 6144;
constexpr std::int16_t playfield_height = 5888;

[[nodiscard]] constexpr std::int16_t word_sub(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) - static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::uint8_t byte_add(
	std::uint8_t left, int right
) noexcept
{
	return static_cast<std::uint8_t>(static_cast<int>(left) + right);
}

[[nodiscard]] bool stage4_midboss_pattern_0(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	auto &bullet = state.bullets.template_;
	if(midboss.phase_frame <= 24) {
		midboss.sprite = static_cast<std::uint8_t>(midboss.phase_frame / 8);
		if(midboss.phase_frame == 24) presentation.sound(6U);
	} else if(midboss.phase_frame < 128) {
		if((midboss.phase_frame & 3) == 0) {
			bullet.group = BulletGroup::spread_aimed;
			bullet.count = 2U;
			bullet.delta = 6U;
			const auto pellet_speed = pc98::th0405::randring_next16_mod(
				state.random, 24U
			);
			if(!pellet_speed) return false;
			bullet.speed = byte_add(static_cast<std::uint8_t>(*pellet_speed), 32);
			const auto pellet_angle = pc98::th0405::randring_next16_mod(
				state.random, 0x60U
			);
			if(!pellet_angle) return false;
			bullet.angle = byte_add(static_cast<std::uint8_t>(*pellet_angle), -0x30);
			if(!bullet_template_tune(state) || !bullets_add(state)) return false;

			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			bullet.patnum = pat_bullet16_small_ball_yellow;
			const auto ball_speed = pc98::th0405::randring_next16_mod(
				state.random, 24U
			);
			if(!ball_speed) return false;
			bullet.speed = byte_add(static_cast<std::uint8_t>(*ball_speed), 32);
			const auto ball_angle = pc98::th0405::randring_next16_mod(
				state.random, 0x60U
			);
			if(!ball_angle) return false;
			bullet.angle = byte_add(static_cast<std::uint8_t>(*ball_angle), -0x30);
			if(!bullets_add(state)) return false;
			presentation.sound(3U);
		}
	} else if(midboss.phase_frame < 152) {
		midboss.sprite = static_cast<std::uint8_t>((159 - midboss.phase_frame) / 8);
	} else {
		midboss.phase_frame = 0;
		midboss.stage4_pattern = phase_none;
	}
	return true;
}

[[nodiscard]] bool stage4_midboss_pattern_1(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	auto &bullet = state.bullets.template_;
	if(midboss.phase_frame <= 24) {
		midboss.sprite = static_cast<std::uint8_t>(midboss.phase_frame / 8);
		if(midboss.phase_frame == 24) presentation.sound(6U);
	} else if(midboss.phase_frame <= 120) {
		if((midboss.phase_frame & 7) == 0) {
			bullet.group = BulletGroup::spread_aimed;
			const auto count = pc98::th0405::randring_next16_and(state.random, 3U);
			if(!count) return false;
			bullet.count = static_cast<std::uint8_t>((*count * 2U) + 1U);
			const auto delta = pc98::th0405::randring_next16_and(state.random, 7U);
			if(!delta) return false;
			bullet.delta = byte_add(static_cast<std::uint8_t>(*delta), 10);
			bullet.speed = 50U;
			bullet.angle = 0U;
			if(!bullet_template_tune(state) || !bullets_add(state, false, true)) {
				return false;
			}
		}
		const auto frame_in_volley = static_cast<std::int16_t>(
			(midboss.phase_frame - 25) & 0x1F
		);
		if(frame_in_volley == 0) {
			const auto angle = pc98::masterlib::iatan2_source(
				word_sub(state.player_pos.cur.y, midboss.pos.cur.y),
				word_sub(state.player_pos.cur.x, midboss.pos.cur.x)
			);
			if(!angle) return false;
			midboss.angle = *angle;
		}
		if((frame_in_volley & 3) == 0) {
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			bullet.group = BulletGroup::single;
			bullet.patnum = pat_bullet16_directional_yellow;
			bullet.speed = static_cast<std::uint8_t>(frame_in_volley * 3 + 40);
			bullet.angle = midboss.angle;
			if(!bullet_template_tune(state) || !bullets_add(state, false, true)) {
				return false;
			}
			presentation.sound(3U);
		}
	} else if(midboss.phase_frame < 144) {
		midboss.sprite = static_cast<std::uint8_t>((151 - midboss.phase_frame) / 8);
	} else {
		midboss.phase_frame = 0;
		midboss.stage4_pattern = phase_none;
	}
	return true;
}

[[nodiscard]] bool stage4_midboss_pattern_2(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	auto &bullet = state.bullets.template_;
	if(midboss.phase_frame <= 24) {
		midboss.sprite = static_cast<std::uint8_t>(midboss.phase_frame / 8);
		if(midboss.phase_frame == 24) {
			presentation.sound(6U);
			bullet.angle = midboss.pos.cur.x < playfield_center_x ? 0x36U : 0x56U;
			++midboss.stage4_angle_flip;
		}
	} else if(midboss.phase_frame <= 136) {
		if(((midboss.phase_frame - 25) & 0x0F) == 0) {
			bullet.group = BulletGroup::stack;
			if((midboss.stage4_angle_flip & 1U) != 0U) {
				const auto angle = pc98::masterlib::iatan2_source(
					word_sub(state.player_pos.cur.y, midboss.pos.cur.y),
					word_sub(state.player_pos.cur.x, midboss.pos.cur.x)
				);
				if(!angle) return false;
				bullet.angle = *angle;
			} else {
				bullet.angle = byte_add(bullet.angle, -8);
			}
			const auto angle_before_mirror = bullet.angle;
			presentation.sound(3U);
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			bullet.patnum = pat_bullet16_small_ball_yellow;
			if(!bullet_template_tune(state)) return false;
			bullet.speed = 16U;
			bullet.count = 12U;
			bullet.delta = 7U;
			if(!bullets_add(state, false, true)) return false;
			const auto mirror_axis = static_cast<std::uint8_t>(
				midboss.pos.cur.x > playfield_center_x ? 0xA0U : 0x60U
			);
			bullet.angle = byte_add(mirror_axis, -static_cast<int>(bullet.angle));
			if(!bullets_add(state, false, true)) return false;
			bullet.angle = angle_before_mirror;
		}
	} else if(midboss.phase_frame < 160) {
		midboss.sprite = static_cast<std::uint8_t>((165 - midboss.phase_frame) / 8);
	} else {
		midboss.phase_frame = 0;
		midboss.stage4_pattern = phase_none;
	}
	return true;
}

[[nodiscard]] bool stage4_midboss_pattern_3(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	auto &bullet = state.bullets.template_;
	if(midboss.phase_frame <= 24) {
		midboss.sprite = static_cast<std::uint8_t>(midboss.phase_frame / 8);
		if(midboss.phase_frame == 24) {
			presentation.sound(6U);
			bullet.angle = byte_add(0U, -0x20);
		}
	} else if(midboss.phase_frame < 128) {
		if((midboss.phase_frame & 2) == 0) {
			bullet.group = BulletGroup::spread;
			bullet.count = 3U;
			bullet.delta = 6U;
			bullet.speed = 60U;
			if(midboss.pos.cur.x < playfield_center_x) {
				bullet.angle = byte_add(0x80U, -static_cast<int>(bullet.angle));
			}
			if(!bullet_template_tune(state) || !bullets_add(state)) return false;
			if(midboss.pos.cur.x < playfield_center_x) {
				bullet.angle = byte_add(0x80U, -static_cast<int>(bullet.angle));
			}
			presentation.sound(3U);
			bullet.angle = byte_add(bullet.angle, 6);
		}
	} else if(midboss.phase_frame < 152) {
		midboss.sprite = static_cast<std::uint8_t>((159 - midboss.phase_frame) / 8);
	} else {
		midboss.phase_frame = 0;
		midboss.stage4_pattern = phase_none;
	}
	return true;
}

[[nodiscard]] bool stage4_midboss_pattern_gap(State &state) noexcept
{
	auto &midboss = state.midboss;
	if(midboss.phase_frame == 1) {
		midboss.pos.velocity.x = midboss.pos.cur.x >= 2880 ? -64 : 64;
	} else if(midboss.stage4_passes < 8U) {
		if(midboss.pos.cur.x <= 768 || midboss.pos.cur.x >= 5376) {
			midboss.pos.velocity.x = 0;
			midboss.phase_frame = 0;
			++midboss.stage4_passes;
			midboss.stage4_pattern = static_cast<std::uint8_t>(
				midboss.stage4_passes & 3U
			);
		}
	} else if(midboss.pos.cur.x <= -512 || midboss.pos.cur.x >= 6656) {
		midboss.phase = 3U;
	}
	return true;
}

[[nodiscard]] constexpr bool stage4_midboss_render_clipped(
	Point center
) noexcept
{
	// m4.cpp calls playfield_clip_point_yx_small_roll(center, 64, 64).
	return center.x <= 0 || center.x >= playfield_width ||
		center.y <= 0 || center.y >= playfield_height;
}

} // namespace

void stage4_midboss_setup(State &state) noexcept
{
	auto &midboss = state.midboss;
	midboss.frames_until = 2800U;
	midboss.pos.cur = {2304, -512};
	midboss.pos.prev = midboss.pos.cur;
	midboss.pos.velocity = {64, 32};
	midboss.hp = 1200;
	midboss.sprite = 0U;
	state.midboss_active = false;
}

void stage4_midboss_activate(State &state) noexcept
{
	if(state.midboss.frames_until != state.stage_frame) return;
	state.midboss.phase = 0U;
	state.midboss.phase_frame = 0;
	state.midboss_active = true;
}

bool stage4_midboss_update(State &state, Presentation &presentation) noexcept
{
	auto &midboss = state.midboss;
	state.reimu_a.homing_target = midboss.pos.cur;
	if(midboss.phase == 0U) {
		pc98::th0405::update_motion(midboss.pos);
		++midboss.phase_frame;
		std::uint16_t ignored_damage{};
		if(!midboss_hittest_shots_damage(
			state, presentation, {384, 384}, 10U, ignored_damage
		)) return false;
		if(midboss.phase_frame >= 48) {
			++midboss.phase;
			midboss.phase_frame = 0;
			midboss.pos.velocity = {};
			midboss.stage4_pattern = 0U;
			midboss.stage4_passes = 0U;
		}
		return true;
	}
	if(midboss.phase == 1U) {
		pc98::th0405::update_motion(midboss.pos);
		++midboss.phase_frame;
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.origin = {midboss.pos.cur.x, word_sub(midboss.pos.cur.y, 256)};
		bool pattern_ok = false;
		switch(midboss.stage4_pattern) {
		case 0U: pattern_ok = stage4_midboss_pattern_0(state, presentation); break;
		case 1U: pattern_ok = stage4_midboss_pattern_1(state, presentation); break;
		case 2U: pattern_ok = stage4_midboss_pattern_2(state, presentation); break;
		case 3U: pattern_ok = stage4_midboss_pattern_3(state, presentation); break;
		case phase_none: pattern_ok = stage4_midboss_pattern_gap(state); break;
		default: return false;
		}
		if(!pattern_ok) return false;

		if(midboss.pos.cur.y >= playfield_height || midboss.pos.cur.x <= 0 ||
			midboss.pos.cur.x >= playfield_width) {
			midboss.phase = phase_none;
		}
		std::uint16_t damage{};
		if(!midboss_hittest_shots_damage(
			state, presentation, {384, 384}, 4U, damage
		)) return false;
		if(damage == 0U) return true;
		midboss.hp = static_cast<std::int16_t>(
			static_cast<int>(midboss.hp) - static_cast<int>(damage)
		);
		if(midboss.hp > 0) {
			midboss.damage_this_frame = 1U;
			return true;
		}
		midboss.damage_this_frame = 1U;
		state.bullets.zap.set_active(true);
		const auto bonus_units = static_cast<std::uint16_t>(
			30U - static_cast<std::uint16_t>(midboss.stage4_passes)
		);
		if(!midboss_score_bonus(state, presentation, bonus_units)) return false;
		state.playfield_shake_anim_time = 12;
		midboss.phase = phase_explode_big;
		midboss.sprite = pat_enemy_kill;
		midboss.phase_frame = 0;
		midboss.pos.velocity.x = 0;
		if(!pc98::th0405::sparks_add_circle(
			pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
			midboss.pos.cur, 96, 48U
		)) return false;
		presentation.sound(12U);
		items_add(state, presentation, midboss.pos.cur,
			midboss.frames_until == 2800U ? ItemType::bomb : ItemType::one_up);
		return true;
	}
	if(midboss.phase == phase_explode_big) {
		midboss.pos.velocity = {};
		pc98::th0405::update_motion(midboss.pos);
		++midboss.phase_frame;
		if((midboss.phase_frame % 16) == 0) {
			++midboss.sprite;
			if(midboss.sprite >= 12U) {
				++midboss.phase;
				midboss.hp = 0;
			}
		}
		return true;
	}
	state.midboss_active = false;
	midboss.hp = 0;
	if(midboss.frames_until == 2800U) {
		midboss.frames_until = 5600U;
		midboss.pos.cur = {3840, -512};
		midboss.pos.prev = midboss.pos.cur;
		midboss.pos.velocity = {-64, 32};
		midboss.hp = 1200;
		midboss.sprite = 0U;
		midboss.phase_frame = 0;
	}
	return true;
}

void stage4_midboss_render_tail(State &state, Presentation &presentation) noexcept
{
	const auto &midboss = state.midboss;
	if(!state.midboss_active || midboss.phase > 2U ||
		stage4_midboss_render_clipped(midboss.pos.cur)) {
		return;
	}
	if(state.midboss.damage_this_frame != 0U) {
		state.midboss.damage_this_frame = 0U;
	}
	(void)presentation;
}

} // namespace th04::core
