#include "th05/core/midboss_extra.hpp"

#include "th05/core/bullet.hpp"
#include "th05/core/item.hpp"
#include "th05/core/shot_collision.hpp"
#include "th05/core/state.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_random.hpp"
#include "pc98/th0405_sparks.hpp"

#include <bit>
#include <cstdint>

namespace th05::core {
namespace {

constexpr std::int16_t subpixel_factor = 16;
constexpr std::int16_t midboss_hitbox_radius = 24 * subpixel_factor;
constexpr std::uint8_t phase_explode_big = 254U;
constexpr std::uint8_t phase_none = 255U;
constexpr std::uint8_t pat_enemy_kill = 4U;
constexpr std::uint8_t pat_enemy_kill_last = 11U;
constexpr std::uint8_t pat_bullet16_cross_blue = 47U;

[[nodiscard]] constexpr std::int16_t word(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr std::int16_t word_sub(
	std::int16_t left, std::int16_t right
) noexcept
{
	return word(static_cast<std::int32_t>(left) - right);
}

[[nodiscard]] constexpr std::uint8_t byte_add(
	std::uint8_t left, std::int16_t right
) noexcept
{
	return static_cast<std::uint8_t>(static_cast<std::int16_t>(left) + right);
}

[[nodiscard]] bool midboss_hittest_damage(
	State &state, Presentation &presentation, BossPoint radius, std::uint8_t hit_sound,
	std::uint16_t &damage
) noexcept
{
	const auto hit = shots_hittest(
		state.shots, {state.midboss.pos.cur, radius}, state.shot_hit_spark_parity,
		{
			state.bombing,
			true,
			static_cast<ShotPlaychar>(state.playchar),
			state.stage_id,
			static_cast<std::uint8_t>(state.stage_frame & 3U),
			state.bomb_frame,
			state.player_pos.cur,
		},
		pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset}, state.random,
		state.score
	);
	if(!hit) return false;
	damage = *hit;
	if(damage != 0U) presentation.sound(hit_sound);
	return true;
}

[[nodiscard]] bool midboss_hittest_invincible(
	State &state, Presentation &presentation
) noexcept
{
	std::uint16_t damage{};
	return midboss_hittest_damage(
		state, presentation, {midboss_hitbox_radius, midboss_hitbox_radius}, 10U, damage
	);
}

[[nodiscard]] bool midboss_hittest(State &state, Presentation &presentation) noexcept
{
	std::uint16_t damage{};
	if(!midboss_hittest_damage(
		state, presentation, {midboss_hitbox_radius, midboss_hitbox_radius}, 4U, damage
	)) return false;
	state.midboss.damage_this_frame = static_cast<std::uint8_t>(damage);
	state.midboss.hp = word_sub(
		state.midboss.hp, static_cast<std::int16_t>(state.midboss.damage_this_frame)
	);
	return true;
}

void bullets_clear(State &state) noexcept
{
	if(state.resources.bullet_clear_time < 20U) state.resources.bullet_clear_time = 20U;
}

enum class PatternResult : std::uint8_t { running, complete, unsupported };

[[nodiscard]] PatternResult pattern_wait(const State &state) noexcept
{
	return state.midboss.phase_frame >= 112 ? PatternResult::complete : PatternResult::running;
}

[[nodiscard]] PatternResult pattern_curved_speedup_rings(State &state) noexcept
{
	auto &midboss = state.midboss;
	auto &bullet = state.bullets.bullet_template;
	if(midboss.phase_frame == 94) {
		const auto right = pc98::th0405::randring_next16_and(state.random, 1U);
		if(!right) return PatternResult::unsupported;
		state.boss.statebytes[13U] = *right != 0U ? 1U : 0xFFU;
	}
	if(midboss.phase_frame <= 114) {
		bullet.spawn_type = bullet_spawn_type_no_decelerate;
		state.bullets.special_turns_max = 2U;
		bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::speedup);
		bullet.spread = 18U;
		bullet.spread_angle_delta = 16U;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
		bullet.speed = 8U;
		bullet.patnum = pat_bullet16_cross_blue;
		bullet.angle = byte_add(bullet.angle, state.boss.statebytes[13U]);
		if(!bullets_add_special(state)) return PatternResult::unsupported;
	}
	return midboss.phase_frame >= 128 ? PatternResult::complete : PatternResult::running;
}

[[nodiscard]] PatternResult pattern_random_pellets(State &state) noexcept
{
	if(state.page_back != 0U) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed);
		bullet.speed = 16U;
		bullet.spread = 12U;
		bullet.spread_angle_delta = 10U;
		bullet.patnum = 0U;
		if(!bullets_add_regular(state)) return PatternResult::unsupported;
	}
	return state.midboss.phase_frame >= 128 ? PatternResult::complete : PatternResult::running;
}

[[nodiscard]] PatternResult pattern_symmetric_turning_spread_stacks(
	State &state, Presentation &presentation
) noexcept
{
	if((state.midboss.phase_frame % 16) == 0) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_backwards | bullet_spawn_type_no_decelerate
		);
		state.bullets.special_turns_max = 1U;
		bullet.special_motion = static_cast<std::uint8_t>(
			BulletSpecialMotion::decelerate_then_turn
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_stack);
		bullet.spread = 5U;
		bullet.spread_angle_delta = 8U;
		bullet.stack = 4U;
		bullet.stack_speed_delta = 16U;
		bullet.speed = 24U;
		bullet.patnum = pat_bullet16_cross_blue;

		bullet.angle = 0x80U;
		state.bullets.bullet_template_special_angle = 0xC8U;
		if(!bullets_add_special(state)) return PatternResult::unsupported;

		bullet.angle = 0U;
		state.bullets.bullet_template_special_angle = 0x38U;
		if(!bullets_add_special(state)) return PatternResult::unsupported;
		presentation.sound(3U);
	}
	return state.midboss.phase_frame >= 128 ? PatternResult::complete : PatternResult::running;
}

[[nodiscard]] PatternResult midboss_pattern_update(
	State &state, Presentation &presentation
) noexcept
{
	if(state.midboss.phase == 0U) return pattern_wait(state);
	if(state.boss.statebytes[12U] == 0U) {
		if((state.boss.statebytes[14U] % 2U) == 0U) {
			return pattern_curved_speedup_rings(state);
		}
		return pattern_random_pellets(state);
	}
	return pattern_symmetric_turning_spread_stacks(state, presentation);
}

[[nodiscard]] PatternResult flystep_and_pattern(
	State &state, Presentation &presentation, std::int16_t frame
) noexcept
{
	auto &midboss = state.midboss;
	if(frame < -8) return PatternResult::running;
	if(frame < 0) {
		midboss.sprite = 221U;
		return PatternResult::running;
	}
	if(frame < 30) {
		if(frame == 0) presentation.sound(8U);
		frame = static_cast<std::int16_t>(frame * 2);
		const auto velocity = pc98::masterlib::vector2(
			midboss.angle, static_cast<std::int16_t>(64 - frame)
		);
		midboss.pos.velocity = {velocity.x, velocity.y};
		(void)pc98::th0405::update_motion(midboss.pos);
		midboss.sprite = ((frame < 16) | (frame > 48)) ? 222U : 223U;
		return PatternResult::running;
	}
	if(frame < 40) {
		if(frame == 30) presentation.sound(15U);
		midboss.pos.prev = midboss.pos.cur;
		midboss.sprite = 221U;
		return midboss_pattern_update(state, presentation) == PatternResult::unsupported
			? PatternResult::unsupported
			: PatternResult::running;
	}
	midboss.sprite = 220U;
	return midboss_pattern_update(state, presentation);
}

void midboss_defeat_update(State &state) noexcept
{
	auto &midboss = state.midboss;
	if(midboss.phase == phase_explode_big) {
		if(midboss.phase_frame == 1) {
			state.playfield_shake_anim_time = 10;
			state.midboss_active = false;
		}
		if((midboss.phase_frame % 16) == 0) {
			++midboss.sprite;
			if(midboss.sprite >= pat_enemy_kill_last + 1U) midboss.phase = phase_none;
		}
		return;
	}
	state.midboss_active = false;
	state.midboss_callback_active = false;
	midboss.hp = 0;
}

} // namespace

void extra_midboss_setup(State &state) noexcept
{
	auto &midboss = state.midboss;
	midboss.frames_until = 5800U;
	midboss.pos.cur = {3072, -256};
	midboss.pos.prev = midboss.pos.cur;
	midboss.angle = 0x40U;
	midboss.hp = 3000;
	midboss.sprite = 220U;
}

void extra_midboss_activate(State &state) noexcept
{
	if(state.midboss.frames_until != state.stage_frame) return;
	state.midboss.phase = 0U;
	state.midboss.phase_frame = 0;
	state.midboss_active = true;
	state.midboss_callback_active = true;
}

bool extra_midboss_update(State &state, Presentation &presentation) noexcept
{
	auto &midboss = state.midboss;
	state.bullets.bullet_template.origin = midboss.pos.cur;
	state.bullets.gather_template.center = midboss.pos.cur;
	++midboss.phase_frame;

	switch(midboss.phase) {
	case 0U: {
		if(!midboss_hittest_invincible(state, presentation)) return false;
		midboss.angle = 0x40U;
		const auto pattern = flystep_and_pattern(state, presentation, midboss.phase_frame);
		if(pattern == PatternResult::unsupported) return false;
		if(pattern == PatternResult::complete) {
			++midboss.phase;
			midboss.phase_frame = 0;
			midboss.angle = 0U;
			state.boss.statebytes[15U] = 0U;
			state.boss.statebytes[14U] = 0U;
			state.boss.statebytes[12U] = 0U;
		}
		break;
	}

	case 1U: {
		const auto pattern = flystep_and_pattern(
			state, presentation, static_cast<std::int16_t>(midboss.phase_frame - 64)
		);
		if(pattern == PatternResult::unsupported) return false;
		if(pattern == PatternResult::complete) {
			if(state.boss.statebytes[12U] == 0U && midboss.hp < 1000) {
				if(!midboss_score_bonus(state, presentation, 10U)) return false;
				bullets_clear(state);
				presentation.sound(15U);
				++state.boss.statebytes[12U];
			}
			state.boss.statebytes[15U] = 1U;
			midboss.phase_frame = 0;
			constexpr std::uint8_t fly_angles[] = {0x10U, 0x70U, 0x80U, 0x80U,
				0x90U, 0xF0U, 0U, 0U};
			midboss.angle = fly_angles[state.boss.statebytes[14U] & 7U];
			++state.boss.statebytes[14U];
		}
		if(!midboss_hittest(state, presentation)) return false;
		if(state.boss.statebytes[14U] < 20U && midboss.hp > 0) break;
		if(state.boss.statebytes[14U] < 20U) {
			state.bullets.zap.start();
			if(!midboss_score_bonus(state, presentation, 30U)) return false;
			items_add(state, presentation, midboss.pos.cur, ItemType::one_up);
		}
		midboss.phase = phase_explode_big;
		midboss.sprite = pat_enemy_kill;
		midboss.phase_frame = 0;
		if(!pc98::th0405::sparks_add_circle(
			pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset}, midboss.pos.cur,
			128, 48U
		)) return false;
		presentation.sound(12U);
		break;
	}

	default:
		midboss_defeat_update(state);
		presentation.boss_hp(midboss.hp, 3000);
		return true;
	}

	presentation.boss_hp(midboss.hp, 3000);
	state.shots.homing_target = midboss.pos.cur;
	return true;
}

void extra_midboss_render_tail(State &state) noexcept
{
	if(state.midboss.phase < phase_explode_big && state.midboss.pos.cur.y >= 256 &&
		state.midboss.damage_this_frame != 0U) {
		state.midboss.damage_this_frame = 0U;
	}
}

} // namespace th05::core
