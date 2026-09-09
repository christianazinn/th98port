#include "th05/core/boss.hpp"
#include "th05/core/bullet.hpp"
#include "th05/core/item.hpp"
#include "th05/core/laser.hpp"
#include "th05/core/shot_collision.hpp"
#include "th05/core/state.hpp"
#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_random.hpp"
#include "pc98/th0405_sparks.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
namespace th05::core
{
namespace
{
constexpr std::int16_t subpixel_factor = 16;
constexpr std::int16_t playfield_width = 384 * subpixel_factor;
constexpr std::int16_t playfield_height = 368 * subpixel_factor;
constexpr std::int16_t boss_fly_left = 32 * subpixel_factor;
constexpr std::int16_t boss_fly_right = (384 - 32) * subpixel_factor;
constexpr std::int16_t boss_fly_top = 24 * subpixel_factor;
constexpr std::int16_t boss_fly_bottom = 128 * subpixel_factor;
constexpr std::int16_t boss_fly_field_left = 144 * subpixel_factor;
constexpr std::int16_t boss_fly_field_right = 240 * subpixel_factor;
constexpr std::uint8_t phase_explode_small = 253U;
constexpr std::uint8_t phase_explode_big = 254U;
constexpr std::uint8_t phase_none = 255U;
constexpr std::uint8_t pat_enemy_kill = 4U;
constexpr std::uint8_t pat_enemy_kill_last = 11U;
constexpr std::uint8_t pat_bullet16_ball_blue = 44U;
constexpr std::uint8_t pat_bullet16_ball_red = 48U;
constexpr std::uint8_t pat_sara_stay = 180U;
constexpr std::uint8_t pat_sara_right = 184U;
constexpr std::uint8_t pat_sara_left = 186U;
constexpr std::uint8_t pat_sara_spin = 188U;
constexpr std::uint8_t pat_sara_spin_last = 191U;
constexpr std::uint8_t pat_midboss1 = 204U;
constexpr std::uint8_t pat_midboss2_entrance = 202U;
constexpr std::uint8_t pat_midboss2_attack = 206U;
constexpr std::uint8_t pat_bullet16_vector_red = 84U;
constexpr std::uint8_t pat_louise = 180U;
constexpr std::uint8_t pat_louise_charge = 184U;
constexpr std::uint8_t pat_louise_fire = 188U;
constexpr std::int16_t midboss1_hp_total = 1000;
constexpr std::int16_t midboss2_hp_total = 1400;
constexpr std::int16_t sara_hp_total = 4650;
constexpr std::int16_t sara_phase_2_end_hp = 2550;
constexpr std::int16_t sara_phase_3_end_hp = 450;
constexpr std::int16_t sara_phase_4_end_hp = 0;
constexpr std::int16_t louise_hp_total = 4400;
constexpr std::int16_t louise_phase_2_end_hp = 3000;
constexpr std::int16_t louise_phase_3_end_hp = 1900;
constexpr std::int16_t louise_phase_4_end_hp = 500;
constexpr std::int16_t louise_phase_5_end_hp = 0;
constexpr std::int16_t boss_bonus_area = (384 * subpixel_factor) / 3;
constexpr std::uint16_t bonus_unit_value = 1000U;
constexpr std::int16_t bullet_clear_frames = 20;
constexpr std::int16_t louise_fly_left = 48 * subpixel_factor;
constexpr std::int16_t louise_fly_right = 336 * subpixel_factor;
constexpr std::int16_t louise_fly_top = 48 * subpixel_factor;
constexpr std::int16_t louise_fly_bottom = 96 * subpixel_factor;
constexpr std::uint8_t x_right = 0U;
constexpr std::uint8_t x_left = 1U;
enum class FlyResult : std::uint8_t
{
	moving,
	complete,
	unsupported
};
enum class PatternSelectResult : std::uint8_t
{
	waiting,
	selected,
	timed_out,
	unsupported,
};
enum class LouisePatternStage : std::uint8_t
{
	charging,
	fire_first,
	firing,
	done,
};
[[nodiscard]] constexpr std::int16_t word(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
[[nodiscard]] constexpr std::int16_t word_add(std::int16_t left, std::int16_t right) noexcept
{
	return word(static_cast<std::int32_t>(left) + right);
}
[[nodiscard]] constexpr std::int16_t word_sub(std::int16_t left, std::int16_t right) noexcept
{
	return word(static_cast<std::int32_t>(left) - right);
}
[[nodiscard]] constexpr std::uint8_t byte_add(std::uint8_t left, std::int16_t right) noexcept
{
	return static_cast<std::uint8_t>(static_cast<std::int16_t>(left) + right);
}
[[nodiscard]] constexpr bool overlap(std::int16_t delta, std::int16_t extent) noexcept
{
	return static_cast<std::uint16_t>(static_cast<std::uint16_t>(delta) +
									  static_cast<std::uint16_t>(extent / 2)) <=
		   static_cast<std::uint16_t>(extent);
}
void bullets_clear(State &state) noexcept
{
	if (state.resources.bullet_clear_time < bullet_clear_frames)
	{
		state.resources.bullet_clear_time = bullet_clear_frames;
	}
}
void explosion_init(BossExplosion &explosion, BossPoint center) noexcept
{
	explosion.alive = true;
	explosion.age = 0U;
	explosion.center = center;
	explosion.radius_cur = {8, 8};
	explosion.radius_delta = {176, 176};
	explosion.angle_offset = 0U;
}
void boss_explode_small(State &state, Presentation &presentation, std::uint8_t type) noexcept
{
	auto &explosion = state.boss_explosions.small[0].alive ? state.boss_explosions.small[1]
														   : state.boss_explosions.small[0];
	explosion_init(explosion, state.boss.pos.cur);
	switch (type)
	{
	case 1U:
		explosion.angle_offset = 32U;
		break;
	case 2U:
		explosion.angle_offset = static_cast<std::uint8_t>(-32);
		break;
	case 3U:
		explosion.radius_delta = {208, 112};
		break;
	case 4U:
		explosion.radius_delta = {112, 208};
		break;
	default:
		break;
	}
	presentation.sound(15U);
}
void boss_explode_big(State &state, Presentation &presentation) noexcept
{
	auto &explosion = state.boss_explosions.big;
	explosion_init(explosion, state.boss.pos.cur);
	explosion.radius_delta = {192, 192};
	presentation.sound(12U);
}
[[nodiscard]] bool shots_hit(State &state, BossPoint center, BossPoint radius, bool against_boss,
							 std::uint16_t &damage) noexcept
{
	const auto hit = shots_hittest(state.shots, {center, radius}, state.shot_hit_spark_parity,
								   {
									   state.bombing,
									   against_boss,
									   static_cast<ShotPlaychar>(state.playchar),
									   state.stage_id,
									   static_cast<std::uint8_t>(state.stage_frame & 3U),
									   state.bomb_frame,
									   state.player_pos.cur,
								   },
								   pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
								   state.random, state.score);
	if (!hit)
		return false;
	damage = *hit;
	return true;
}
void boss_hittest_player(State &state) noexcept
{
	const auto delta_x = word_sub(state.boss.pos.cur.x, state.player_pos.cur.x);
	const auto delta_y = word_sub(state.boss.pos.cur.y, state.player_pos.cur.y);
	if (overlap(delta_y, 512) && overlap(delta_x, 512))
	{
		state.player_is_hit = true;
	}
}
[[nodiscard]] bool boss_hittest_damage(State &state, Presentation &presentation, BossPoint radius,
									   std::uint8_t sound, std::uint16_t &damage) noexcept
{
	if (!shots_hit(state, state.boss.pos.cur, radius, true, damage))
		return false;
	if (damage != 0U)
		presentation.sound(sound);
	boss_hittest_player(state);
	return true;
}
[[nodiscard]] bool midboss_hittest_damage(State &state, Presentation &presentation,
										  BossPoint radius, std::uint8_t sound,
										  std::uint16_t &damage) noexcept
{
	if (!shots_hit(state, state.midboss.pos.cur, radius, true, damage))
		return false;
	if (damage != 0U)
		presentation.sound(sound);
	return true;
}
[[nodiscard]] bool boss_hittest(State &state, Presentation &presentation) noexcept
{
	std::uint16_t damage{};
	if (!boss_hittest_damage(state, presentation, state.boss.hitbox_radius, 4U, damage))
		return false;
	state.boss.damage_this_frame = static_cast<std::uint8_t>(damage);
	state.boss.hp =
		word_sub(state.boss.hp, static_cast<std::int16_t>(state.boss.damage_this_frame));
	return state.boss.hp <= state.boss.phase_end_hp;
}
[[nodiscard]] bool boss_hittest_invincible(State &state, Presentation &presentation) noexcept
{
	std::uint16_t damage{};
	return boss_hittest_damage(state, presentation, state.boss.hitbox_radius, 10U, damage);
}
[[nodiscard]] bool midboss_hittest(State &state, Presentation &presentation) noexcept
{
	std::uint16_t damage{};
	if (!midboss_hittest_damage(state, presentation, {384, 384}, 4U, damage))
	{
		return false;
	}
	state.midboss.damage_this_frame = static_cast<std::uint8_t>(damage);
	state.midboss.hp =
		word_sub(state.midboss.hp, static_cast<std::int16_t>(state.midboss.damage_this_frame));
	return state.midboss.hp <= 0;
}
[[nodiscard]] bool midboss_hittest_invincible(State &state, Presentation &presentation) noexcept
{
	std::uint16_t damage{};
	return midboss_hittest_damage(state, presentation, {192, 192}, 10U, damage);
}
[[nodiscard]] bool score_bonus(State &state, Presentation &presentation, BossPoint center,
							   std::uint16_t units) noexcept
{
	state.score.score_delta += static_cast<std::uint32_t>(units) * bonus_unit_value;
	state.pointnum_times_2 = false;
	const auto left = word_sub(center.x, boss_bonus_area / 2);
	const auto top = word_sub(center.y, boss_bonus_area / 2);
	for (std::uint16_t i = 0U; i < units; ++i)
	{
		const auto x = pc98::th0405::randring_next16_mod(
			state.random, static_cast<std::uint16_t>(boss_bonus_area));
		if (!x)
			return false;
		const auto y = pc98::th0405::randring_next16_mod(
			state.random, static_cast<std::uint16_t>(boss_bonus_area));
		if (!y)
			return false;
		auto point_x = word_add(left, std::bit_cast<std::int16_t>(*x));
		if (point_x < 0)
			point_x = 0;
		else if (point_x > playfield_width)
			point_x = playfield_width;
		presentation.pointnum({point_x, word_add(top, std::bit_cast<std::int16_t>(*y))},
							  bonus_unit_value, false);
	}
	return true;
}
[[nodiscard]] bool boss_score_bonus(State &state, Presentation &presentation,
									std::uint16_t units) noexcept
{
	if (!score_bonus(state, presentation, state.boss.pos.cur, units))
		return false;
	state.boss.phase_timed_out = false;
	return true;
}
[[nodiscard]] bool midboss_score_bonus(State &state, Presentation &presentation,
									   std::uint16_t units) noexcept
{
	return score_bonus(state, presentation, state.midboss.pos.cur, units);
}
[[nodiscard]] bool boss_items_drop(State &state, Presentation &presentation) noexcept
{
	const auto left = word_sub(state.boss.pos.cur.x, 1024);
	const auto top = word_sub(state.boss.pos.cur.y, 1024);
	const auto drops = state.power < 128U		? std::array<ItemType, 5U>{
			ItemType::power, ItemType::power, ItemType::big_power,			ItemType::power, ItemType::power,		}
		: std::array<ItemType, 5U>{
			ItemType::point, ItemType::point, ItemType::point,			ItemType::point, ItemType::point,		}
;
	for (const auto drop : drops)
	{
		const auto x = pc98::th0405::randring_next16_mod(state.random, 2048U);
		if (!x)
			return false;
		const auto y = pc98::th0405::randring_next16_mod(state.random, 2048U);
		if (!y)
			return false;
		items_add(state, presentation,
				  {word_add(left, std::bit_cast<std::int16_t>(*x)),
				   word_add(top, std::bit_cast<std::int16_t>(*y))},
				  drop);
	}
	return true;
}
[[nodiscard]] bool boss_phase_next(State &state, Presentation &presentation,
								   std::uint8_t explosion_type, std::int16_t next_end_hp) noexcept
{
	boss_explode_small(state, presentation, explosion_type);
	if (!state.boss.phase_timed_out)
	{
		bullets_clear(state);
		if (!boss_items_drop(state, presentation))
			return false;
	}
	state.boss.phase_timed_out = true;
	++state.boss.phase;
	state.boss.phase_frame = 0;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.hp = state.boss.phase_end_hp;
	state.boss.phase_end_hp = next_end_hp;
	return true;
}
[[nodiscard]] bool tune_and_add_regular(State &state) noexcept
{
	return bullet_template_tune(state) && bullets_add_regular(state);
}
[[nodiscard]] bool tune_and_add_special(State &state) noexcept
{
	return bullet_template_tune(state) && bullets_add_special(state);
}
[[nodiscard]] bool midboss1_pattern(State &state) noexcept
{
	auto &midboss = state.midboss;
	if ((midboss.phase_frame % (midboss.hp <= 200 ? 12 : 16)) != 0)
		return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = bullet_spawn_type_normal;
	bullet.patnum = 0U;
	bullet.speed = 32U;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
	bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::exact_linear);
	bullet.spread = 4U;
	bullet.spread_angle_delta = 2U;
	bullet.angle = state.boss.angle;
	state.boss.angle = byte_add(state.boss.angle, 0x14);
	if (!tune_and_add_special(state))
		return false;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::single);
	bullet.spawn_type = bullet_spawn_type_cloud_forwards;
	bullet.speed = 24U;
	bullet.patnum = pat_bullet16_ball_blue;
	if (!bullet_template_tune(state))
		return false;
	const auto angle = pc98::th0405::randring_next16(state.random);
	if (!angle)
		return false;
	bullet.angle = static_cast<std::uint8_t>(*angle);
	const auto offset = pc98::masterlib::vector2(bullet.angle, 512);
	bullet.origin = {
		word_add(midboss.pos.cur.x, offset.x),
		word_add(midboss.pos.cur.y, offset.y),
	};
	return bullets_add_regular(state);
}
[[nodiscard]] bool midboss1_update(State &state, Presentation &presentation) noexcept
{
	auto &midboss = state.midboss;
	state.bullets.bullet_template.origin = midboss.pos.cur;
	++midboss.phase_frame;
	switch (midboss.phase)
	{
	case 0U:
		if (midboss.phase_frame >= 256)
		{
			++midboss.phase;
			midboss.phase_frame = 0;
			midboss.sprite = pat_midboss1;
		}
		break;
	case 1U:
	case 2U:
		if (!midboss_hittest_invincible(state, presentation))
			return false;
		if (midboss.phase_frame >= 64)
		{
			++midboss.phase;
			midboss.phase_frame = 0;
			midboss.pos.velocity.x = 0;
			state.boss.angle = static_cast<std::uint8_t>(-0x20);
		}
		break;
	case 3U:
	{
		if (!midboss1_pattern(state))
			return false;
		const auto ended = midboss_hittest(state, presentation);
		midboss.pos.velocity.x = word(
			static_cast<std::int32_t>((midboss1_hp_total - midboss.hp) * 64 * subpixel_factor) /
			midboss1_hp_total);
		midboss.pos.prev = midboss.pos.cur;
		const auto offset = pc98::masterlib::vector2(state.boss.angle, midboss.pos.velocity.x);
		midboss.pos.cur = {word_add(3072, offset.x), word_add(1536, offset.y)};
		state.boss.angle = byte_add(state.boss.angle, -2);
		if (midboss.phase_frame < 1500 && !ended)
			break;
		if (midboss.phase_frame < 1500)
		{
			state.bullets.zap.start();
			if (!midboss_score_bonus(state, presentation, 5U))
				return false;
			items_add(state, presentation, midboss.pos.cur, ItemType::big_power);
		}
		midboss.phase = phase_explode_big;
		midboss.sprite = pat_enemy_kill;
		midboss.phase_frame = 0;
		if (!pc98::th0405::sparks_add_circle(
				pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset}, midboss.pos.cur,
				128, 48U))
			return false;
		presentation.sound(12U);
		break;
	}
	case phase_explode_big:
		if (midboss.phase_frame == 1)
		{
			state.playfield_shake_anim_time = 10;
			state.midboss_active = false;
		}
		if ((midboss.phase_frame % 16) == 0)
		{
			++midboss.sprite;
			if (midboss.sprite >= pat_enemy_kill_last + 1U)
				midboss.phase = phase_none;
		}
		presentation.boss_hp(midboss.hp, midboss1_hp_total);
		return true;
	default:
		state.midboss_active = false;
		midboss.hp = 0;
		presentation.boss_hp(midboss.hp, midboss1_hp_total);
		return true;
	}
	presentation.boss_hp(midboss.hp, midboss1_hp_total);
	state.shots.homing_target = midboss.pos.cur;
	return true;
}
[[nodiscard]] FlyResult boss_flystep_random(State &state, std::int16_t frame) noexcept
{
	auto &boss = state.boss;
	if (frame == 0)
	{
		if (boss.pos.cur.x < boss_fly_field_left)
		{
			const auto angle = pc98::th0405::randring_next16_mod(state.random, 96U);
			if (!angle)
				return FlyResult::unsupported;
			boss.angle = static_cast<std::uint8_t>(*angle - 0x30U);
		}
		else if (boss.pos.cur.x > boss_fly_field_right)
		{
			const auto angle = pc98::th0405::randring_next16_and(state.random, 0x60U);
			if (!angle)
				return FlyResult::unsupported;
			boss.angle = static_cast<std::uint8_t>(*angle + 0x30U);
		}
		else
		{
			const auto angle = pc98::th0405::randring_next16(state.random);
			if (!angle)
				return FlyResult::unsupported;
			boss.angle = static_cast<std::uint8_t>(*angle);
		}
		if ((boss.flystep_next_y_direction == 1 && boss.angle < 0x80U) ||
			(boss.flystep_next_y_direction == 2 && boss.angle >= 0x80U))
		{
			boss.angle = static_cast<std::uint8_t>(-boss.angle);
		}
		boss.flystep_next_y_direction = 0;
	}
	if (frame < 0)
		return FlyResult::moving;
	const auto velocity =
		pc98::masterlib::vector2(boss.angle, word(64 - static_cast<std::int32_t>(frame) * 2));
	boss.pos.velocity = {velocity.x, velocity.y};
	boss.pos.cur.x = word_add(boss.pos.cur.x, boss.pos.velocity.x);
	boss.pos.cur.y = word_add(boss.pos.cur.y, boss.pos.velocity.y);
	boss.sprite = boss.pos.velocity.x < 0 ? boss.sprite_left : boss.sprite_right;
	if (boss.pos.cur.y < boss_fly_top)
	{
		boss.pos.cur.y = boss_fly_top;
		boss.flystep_next_y_direction = 2;
	}
	else if (boss.pos.cur.y > boss_fly_bottom)
	{
		boss.pos.cur.y = boss_fly_bottom;
		boss.flystep_next_y_direction = 1;
	}
	if (boss.pos.cur.x < boss_fly_left)
		boss.pos.cur.x = boss_fly_left;
	else if (boss.pos.cur.x > boss_fly_right)
		boss.pos.cur.x = boss_fly_right;
	if (frame < 28)
		return FlyResult::moving;
	boss.sprite = boss.sprite_stay;
	return FlyResult::complete;
}
[[nodiscard]] bool phase2_pattern_blue(State &state, std::size_t angle_byte,
									   std::int16_t angle_delta) noexcept
{
	if ((state.boss.phase_frame % 2) != 0)
		return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = bullet_spawn_type_cloud_forwards;
	bullet.patnum = pat_bullet16_ball_blue;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::single);
	bullet.angle = state.boss.statebytes[angle_byte];
	bullet.speed = 24U;
	if (!tune_and_add_regular(state))
		return false;
	state.boss.statebytes[angle_byte] = byte_add(state.boss.statebytes[angle_byte], angle_delta);
	return true;
}
[[nodiscard]] bool phase2_pattern_aimed_red_spread_stack(State &state,
														 Presentation &presentation) noexcept
{
	if (state.boss.phase_frame != 32)
		return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = static_cast<std::uint8_t>(bullet_spawn_type_cloud_forwards |
												  bullet_spawn_type_no_decelerate);
	bullet.patnum = pat_bullet16_ball_red;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_stack_aimed);
	bullet.angle = 0U;
	bullet.spread = 5U;
	bullet.spread_angle_delta = 0x10U;
	bullet.stack = 5U;
	bullet.stack_speed_delta = 7U;
	bullet.speed = 16U;
	if (!tune_and_add_regular(state))
		return false;
	presentation.sound(15U);
	return true;
}
[[nodiscard]] bool phase2_pattern_red_stacks(State &state, Presentation &presentation) noexcept
{
	if ((state.boss.phase_frame % 8) != 0)
		return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = static_cast<std::uint8_t>(bullet_spawn_type_cloud_forwards |
												  bullet_spawn_type_no_decelerate);
	bullet.patnum = pat_bullet16_ball_red;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::stack);
	bullet.angle = byte_add(byte_add(state.boss.angle, state.boss.statebytes[13]), -0x80);
	bullet.stack = 8U;
	bullet.stack_speed_delta = 6U;
	bullet.speed = 16U;
	if (!tune_and_add_regular(state))
		return false;
	state.boss.statebytes[13] = byte_add(state.boss.statebytes[13], -8);
	presentation.sound(15U);
	return true;
}
[[nodiscard]] bool phase2_with_pattern(State &state, Presentation &presentation) noexcept
{
	if (state.boss.phase_frame < 16)
	{
		gather_add_only_3stack(state, static_cast<std::int16_t>(state.boss.phase_frame - 1), 9U,
							   8U);
		if (state.boss.phase_frame == 1)
		{
			presentation.sound(8U);
			state.boss.statebytes[15] = 0x80U;
			state.boss.statebytes[14] = 0U;
			state.boss.statebytes[13] = 8U;
		}
		return true;
	}
	const auto fly =
		boss_flystep_random(state, static_cast<std::int16_t>(state.boss.phase_frame - 16));
	if (fly == FlyResult::unsupported)
		return false;
	if (fly == FlyResult::complete)
	{
		state.boss.phase_frame = 0;
		state.boss.mode = 0U;
	}
	switch (state.boss.selected_pattern)
	{
	case 1U:
		return phase2_pattern_blue(state, 15U, -10);
	case 2U:
		return phase2_pattern_blue(state, 14U, 10);
	case 3U:
		return phase2_pattern_aimed_red_spread_stack(state, presentation);
	case 4U:
		return phase2_pattern_red_stacks(state, presentation);
	default:
		return false;
	}
}
[[nodiscard]] bool phase3_pattern_pellet_arcs(State &state, Presentation &presentation) noexcept
{
	if ((state.boss.phase_frame % 8) != 0)
		return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = bullet_spawn_type_no_decelerate;
	bullet.patnum = 0U;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
	const auto special_angle = pc98::th0405::randring_next16_mod(
		state.random, static_cast<std::uint16_t>(state.boss.phase_frame * 2));
	if (!special_angle)
		return false;
	bullet.angle = byte_add(static_cast<std::uint8_t>(*special_angle),
							static_cast<std::int16_t>(0x40 - state.boss.phase_frame));
	bullet.spread = 3U;
	bullet.spread_angle_delta = 3U;
	bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::exact_linear);
	const auto special_speed = pc98::th0405::randring_next16_mod(state.random, 32U);
	if (!special_speed)
		return false;
	bullet.speed = byte_add(static_cast<std::uint8_t>(*special_speed), 16);
	if (!tune_and_add_special(state))
		return false;
	bullet.spawn_type = bullet_spawn_type_cloud_backwards;
	bullet.patnum = pat_bullet16_ball_blue;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::single);
	const auto regular_angle = pc98::th0405::randring_next16_mod(
		state.random, static_cast<std::uint16_t>(state.boss.phase_frame * 2));
	if (!regular_angle)
		return false;
	bullet.angle = byte_add(static_cast<std::uint8_t>(*regular_angle),
							static_cast<std::int16_t>(0x40 - state.boss.phase_frame));
	const auto regular_speed = pc98::th0405::randring_next16_mod(state.random, 32U);
	if (!regular_speed)
		return false;
	bullet.speed = byte_add(static_cast<std::uint8_t>(*regular_speed), 24);
	if (!tune_and_add_regular(state))
		return false;
	presentation.sound(3U);
	return true;
}
[[nodiscard]] bool phase3_pattern_random_red_rings(State &state,
												   Presentation &presentation) noexcept
{
	if ((state.boss.phase_frame % 16) != 0)
		return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = bullet_spawn_type_cloud_forwards;
	bullet.patnum = pat_bullet16_ball_red;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
	const auto angle = pc98::th0405::randring_next16(state.random);
	if (!angle)
		return false;
	bullet.angle = static_cast<std::uint8_t>(*angle);
	bullet.speed = 32U;
	bullet.spread = 12U;
	if (!tune_and_add_regular(state))
		return false;
	presentation.sound(3U);
	return true;
}
[[nodiscard]] bool phase3_pattern_accelerating(State &state, Presentation &presentation,
											   std::size_t angle_byte,
											   std::int16_t angle_delta) noexcept
{
	if ((state.boss.phase_frame % 8) != 0)
		return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = bullet_spawn_type_normal;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
	bullet.spread = 3U;
	bullet.patnum = pat_bullet16_ball_blue;
	bullet.angle = state.boss.statebytes[angle_byte];
	bullet.speed = state.boss.statebytes[13];
	if ((state.boss.phase_frame % 16) == 0)
		bullet.speed /= 2U;
	if (!tune_and_add_regular(state))
		return false;
	presentation.sound(3U);
	state.boss.statebytes[angle_byte] = byte_add(state.boss.statebytes[angle_byte], angle_delta);
	state.boss.statebytes[13] = byte_add(state.boss.statebytes[13], 4);
	return true;
}
[[nodiscard]] bool phase3_with_pattern(State &state, Presentation &presentation) noexcept
{
	if (state.boss.phase_frame < 32)
	{
		gather_add_only_3stack(state, static_cast<std::int16_t>(state.boss.phase_frame - 16), 9U,
							   8U);
		if (state.boss.phase_frame == 16)
		{
			presentation.sound(8U);
			state.boss.sprite = pat_sara_spin;
			state.boss.statebytes[15] = 0x38U;
			state.boss.statebytes[14] = 0x48U;
			state.boss.statebytes[13] = 24U;
		}
		return true;
	}
	if (state.boss.phase_frame < 64 ||
		(state.boss.phase_frame < 96 && (state.boss.phase_frame % 2) == 0) ||
		(state.boss.phase_frame < 128 && (state.boss.phase_frame % 4) == 0) ||
		(state.boss.phase_frame < 160 && (state.boss.phase_frame % 8) == 0))
	{
		++state.boss.sprite;
	}
	if (state.boss.sprite >= pat_sara_spin_last + 1U)
		state.boss.sprite = pat_sara_spin;
	bool result{};
	switch (state.boss.selected_pattern)
	{
	case 1U:
		result = phase3_pattern_pellet_arcs(state, presentation);
		break;
	case 2U:
		result = phase3_pattern_random_red_rings(state, presentation);
		break;
	case 3U:
		result = phase3_pattern_accelerating(state, presentation, 15U, 6);
		break;
	case 4U:
		result = phase3_pattern_accelerating(state, presentation, 14U, -6);
		break;
	default:
		return false;
	}
	if (!result)
		return false;
	if (state.boss.phase_frame >= state.boss.statebytes[9])
	{
		state.boss.phase_frame = 0;
		state.boss.mode = 0U;
		state.boss.sprite = pat_sara_stay;
	}
	return true;
}
[[nodiscard]] bool select_phase_pattern(State &state, std::uint8_t phase) noexcept
{
	std::uint8_t &previous = state.boss.statebytes[10];
	do
	{
		const auto mode = pc98::th0405::randring_next16_mod(state.random, 4U);
		if (!mode)
			return false;
		state.boss.mode = static_cast<std::uint8_t>(*mode + 1U);
	} while (state.boss.mode == previous);
	previous = state.boss.mode;
	state.boss.selected_pattern = state.boss.mode;
	(void)phase;
	return true;
}
[[nodiscard]] PatternSelectResult phase2_wait_fly_select(State &state) noexcept
{
	const auto fly = boss_flystep_random(
		state, static_cast<std::int16_t>(state.boss.phase_frame - state.boss.statebytes[9]));
	if (fly == FlyResult::unsupported)
		return PatternSelectResult::unsupported;
	if (fly == FlyResult::moving)
		return PatternSelectResult::waiting;
	state.boss.phase_frame = 0;
	++state.boss.phase_state;
	if (state.boss.phase_state >= 32U)
		return PatternSelectResult::timed_out;
	return select_phase_pattern(state, 2U) ? PatternSelectResult::selected
										   : PatternSelectResult::unsupported;
}
[[nodiscard]] PatternSelectResult phase3_wait_fly_select(State &state) noexcept
{
	const auto fly =
		boss_flystep_random(state, static_cast<std::int16_t>(state.boss.phase_frame - 16));
	if (fly == FlyResult::unsupported)
		return PatternSelectResult::unsupported;
	if (fly == FlyResult::moving)
		return PatternSelectResult::waiting;
	state.boss.phase_frame = 0;
	++state.boss.phase_state;
	if (state.boss.phase_state >= 24U)
		return PatternSelectResult::timed_out;
	return select_phase_pattern(state, 3U) ? PatternSelectResult::selected
										   : PatternSelectResult::unsupported;
}
[[nodiscard]] constexpr std::uint8_t rank_spread(std::uint8_t rank) noexcept
{
	switch (rank)
	{
	case 0U:
		return 5U;
	case 1U:
		return 6U;
	case 2U:
		return 7U;
	case 3U:
		return 8U;
	case 4U:
		return 6U;
	default:
		return 0U;
	}
}
[[nodiscard]] bool phase4_pattern(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if (boss.phase_frame == 1)
		boss.sprite = pat_sara_spin;
	if (boss.phase_frame == 32)
	{
		auto &laser = state.lasers.laser_template;
		laser.coords.origin = boss.pos.cur;
		laser.coords.angle = 0xE0U;
		laser.color = 8U;
		laser.coords.width = 8U;
		if (!laser_manual_fixed_spawn(state, x_right, presentation))
			return false;
		laser.coords.angle = 0xA0U;
		if (!laser_manual_fixed_spawn(state, x_left, presentation))
			return false;
		boss.statebytes[15] = 0U;
		boss.statebytes[14] = 1U;
		boss.statebytes[13] = 1U;
		return true;
	}
	if (boss.phase_frame <= 32)
		return true;
	if (boss.phase_frame == 64)
	{
		if (!laser_manual_grow(state, x_right, presentation) ||
			!laser_manual_grow(state, x_left, presentation))
			return false;
	}
	if ((boss.phase_frame % boss.statebytes[13]) == 0 &&
		state.lasers.pool[x_left].coords.angle > 0x4AU)
	{
		state.lasers.pool[x_right].coords.angle =
			byte_add(state.lasers.pool[x_right].coords.angle, 1);
		state.lasers.pool[x_left].coords.angle =
			byte_add(state.lasers.pool[x_left].coords.angle, -1);
		const auto left_angle = state.lasers.pool[x_left].coords.angle;
		if (left_angle == 0x80U || left_angle == 0x60U || left_angle == 0x58U ||
			left_angle == 0x50U)
		{
			++boss.statebytes[13];
		}
	}
	if ((state.stage_frame & 15U) == 0U)
	{
		const auto spread = rank_spread(state.rank);
		if (spread == 0U)
			return false;
		bullet.spawn_type = bullet_spawn_type_no_decelerate;
		bullet.angle = boss.statebytes[15];
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::exact_linear);
		bullet.spread = spread;
		bullet.spread_angle_delta = 1U;
		bullet.speed = 32U;
		bullet.patnum = 0U;
		if (!bullets_add_special(state))
			return false;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed);
		bullet.speed = 16U;
		bullet.spread = boss.statebytes[14];
		bullet.patnum = pat_bullet16_ball_blue;
		if (!bullets_add_regular(state))
			return false;
		boss.statebytes[15] = byte_add(boss.statebytes[15], 14);
	}
	if ((boss.phase_frame % 64) == 0 && boss.statebytes[14] < 8U)
	{
		++boss.statebytes[14];
	}
	return true;
}
[[nodiscard]] bool boss_defeat_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	if (boss.phase == phase_explode_small)
	{
		if (boss.phase_frame == 1)
		{
			boss.damage_this_frame = 0U;
			boss_explode_small(state, presentation, 0U);
			presentation.sound(13U);
		}
		if (boss.phase_frame == 16)
			boss_explode_small(state, presentation, 4U);
		if (boss.phase_frame == 32)
		{
			boss_explode_big(state, presentation);
			++boss.phase;
			if (boss.phase_state != 0U)
			{
				state.bullets.zap.start();
				if (!boss_score_bonus(state, presentation, 10U))
					return false;
			}
			boss.sprite = pat_enemy_kill;
			boss.phase_frame = 0;
			state.player_invincibility_time = 255U;
		}
		state.shots.homing_target = {homing_target_none, homing_target_none};
		return true;
	}
	if (boss.phase == phase_explode_big)
	{
		if (boss.phase_frame < 12)
		{
			state.playfield_shake_x = (state.stage_frame & 1U) == 0U ? -4 : 4;
			state.playfield_shake_y = (state.stage_frame & 3U) <= 1U ? -4 : 4;
		}
		state.slowdown_factor = 2U;
		if ((boss.phase_frame % 8) == 0)
		{
			++boss.sprite;
			if (boss.sprite >= pat_enemy_kill_last + 1U)
			{
				++boss.phase;
				boss.phase_frame = 0;
				state.bombing_disabled = true;
			}
		}
		return true;
	}
	state.palette_tone = 60U;
	state.palette_changed = true;
	if (boss.phase_frame == 1)
	{
		state.dialog_pending = true;
		state.stage_clear_requested = true;
	}
	state.shots.homing_target = {homing_target_none, homing_target_none};
	return true;
}
[[nodiscard]] bool midboss2_hittest_invincible(
	State &state, Presentation &presentation
) noexcept
{
	std::uint16_t damage{};
	return midboss_hittest_damage(state, presentation, {384, 384}, 10U, damage);
}
[[nodiscard]] bool midboss2_pattern_random_stacks_and_balls(State &state) noexcept
{
	if((state.stage_frame & 15U) != 0U) return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = bullet_spawn_type_no_decelerate;
	bullet.patnum = pat_bullet16_ball_red;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::stack);
	bullet.stack = 3U;
	bullet.stack_speed_delta = 8U;
	bullet.speed = 24U;
	const auto stack_angle = pc98::th0405::randring_next16(state.random);
	if(!stack_angle) return false;
	bullet.angle = static_cast<std::uint8_t>(*stack_angle);
	if(!tune_and_add_regular(state)) return false;

	bullet.spawn_type = static_cast<std::uint8_t>(
		bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
	);
	bullet.patnum = 0U;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed);
	bullet.spread = state.midboss.hp <= 600 ? 6U : (state.midboss.hp <= 800 ? 4U : 3U);
	bullet.speed = 24U;
	const auto ball_angle = pc98::th0405::randring_next16(state.random);
	if(!ball_angle) return false;
	bullet.angle = static_cast<std::uint8_t>(*ball_angle);
	return tune_and_add_regular(state);
}
[[nodiscard]] bool midboss2_pattern_blue_stacks(State &state) noexcept
{
	if((state.stage_frame & 3U) != 0U) return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = bullet_spawn_type_no_decelerate;
	bullet.patnum = pat_bullet16_ball_blue;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::stack);
	bullet.stack = 3U;
	bullet.stack_speed_delta = 15U;
	bullet.speed = 24U;
	const auto angle = pc98::th0405::randring_next16(state.random);
	if(!angle) return false;
	bullet.angle = static_cast<std::uint8_t>(*angle);
	return tune_and_add_regular(state);
}
[[nodiscard]] bool midboss2_velocity_towards_top_center(State &state) noexcept
{
	auto &midboss = state.midboss;
	const auto angle = pc98::masterlib::iatan2_source(
		word_sub(1024, midboss.pos.cur.y), word_sub(3072, midboss.pos.cur.x)
	);
	if(!angle) return false;
	const auto velocity = pc98::masterlib::vector2(*angle, 1);
	midboss.pos.velocity = {velocity.x, velocity.y};
	return true;
}
void midboss2_orbit(State &state) noexcept
{
	auto &midboss = state.midboss;
	midboss.pos.prev = midboss.pos.cur;
	const auto offset = pc98::masterlib::vector2(midboss.angle, midboss.pos.velocity.x);
	midboss.pos.cur = {
		word_add(midboss.stage2_center.x, offset.x),
		word_add(midboss.stage2_center.y, offset.y),
	};

	auto &center_velocity = state.boss.statebytes[14U];
	midboss.stage2_center.x = word_add(
		midboss.stage2_center.x, static_cast<std::int8_t>(center_velocity)
	);
	if(midboss.stage2_center.x < 2048) {
		center_velocity = byte_add(center_velocity, 2);
	} else if(midboss.stage2_center.x > 4096) {
		center_velocity = byte_add(center_velocity, -2);
	}

	auto &radius_shrinking = state.boss.statebytes[15U];
	if(radius_shrinking == 0U) {
		midboss.pos.velocity.x = word_add(midboss.pos.velocity.x, 16);
		if(midboss.pos.velocity.x > 1024) radius_shrinking = 1U;
	} else {
		midboss.pos.velocity.x = word_add(midboss.pos.velocity.x, -16);
		if(midboss.pos.velocity.x <= 16) radius_shrinking = 0U;
	}
	midboss.angle = byte_add(midboss.angle, -2);
}
[[nodiscard]] bool midboss2_update(State &state, Presentation &presentation) noexcept
{
	auto &midboss = state.midboss;
	state.bullets.bullet_template.origin = midboss.pos.cur;
	++midboss.phase_frame;
	switch(midboss.phase) {
	case 0U:
		(void)pc98::th0405::update_motion(midboss.pos);
		if(!midboss2_hittest_invincible(state, presentation)) return false;
		if(midboss.phase_frame >= 256) {
			++midboss.phase;
			midboss.phase_frame = 0;
			midboss.angle = 0U;
			state.boss.statebytes[15U] = 0U;
			state.boss.statebytes[14U] = 0x20U;
			midboss.pos.velocity.x = 0;
			midboss.stage2_center.x = 3072;
			midboss.stage2_center.y = midboss.pos.cur.y;
		}
		break;
	case 1U: {
		if(!midboss2_pattern_random_stacks_and_balls(state)) return false;
		midboss2_orbit(state);
		(void)midboss_hittest(state, presentation);
		if(midboss.phase_frame < 1000 && midboss.hp > 400) break;
		if(midboss.phase_frame < 1000) {
			if(!midboss_score_bonus(state, presentation, 5U)) return false;
			bullets_clear(state);
		}
		++midboss.phase;
		midboss.sprite = pat_midboss2_attack;
		midboss.phase_frame = 0;
		if(!pc98::th0405::sparks_add_circle(
			pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset}, midboss.pos.cur,
			64, 32U
		)) return false;
		presentation.sound(15U);
		if(!midboss2_velocity_towards_top_center(state)) return false;
		break;
	}
	case 2U: {
		if(!midboss2_pattern_blue_stacks(state)) return false;
		(void)pc98::th0405::update_motion(midboss.pos);
		(void)midboss_hittest(state, presentation);
		if(midboss.phase_frame < 800 && midboss.hp > 0) break;
		if(midboss.phase_frame < 800) {
			state.bullets.zap.start();
			if(!midboss_score_bonus(state, presentation, 15U)) return false;
			items_add(state, presentation, midboss.pos.cur, ItemType::bomb);
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
	case phase_explode_big:
		if(midboss.phase_frame == 1) {
			state.playfield_shake_anim_time = 10;
			state.midboss_active = false;
		}
		if((midboss.phase_frame % 16) == 0) {
			++midboss.sprite;
			if(midboss.sprite >= pat_enemy_kill_last + 1U) midboss.phase = phase_none;
		}
		presentation.boss_hp(midboss.hp, midboss2_hp_total);
		return true;
	default:
		state.midboss_active = false;
		midboss.hp = 0;
		presentation.boss_hp(midboss.hp, midboss2_hp_total);
		return true;
	}
	presentation.boss_hp(midboss.hp, midboss2_hp_total);
	state.shots.homing_target = midboss.pos.cur;
	return true;
}
[[nodiscard]] FlyResult louise_flystep_random(
	State &state, std::int16_t speed, std::int16_t frames
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 1) {
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return FlyResult::unsupported;
		boss.angle = static_cast<std::uint8_t>(*angle);
		const auto velocity = pc98::masterlib::vector2(boss.angle, speed);
		boss.pos.velocity = {velocity.x, velocity.y};
	}
	boss.pos.cur.x = word_add(boss.pos.cur.x, boss.pos.velocity.x);
	boss.pos.cur.y = word_add(boss.pos.cur.y, boss.pos.velocity.y);
	if(boss.pos.cur.x <= louise_fly_left || boss.pos.cur.x >= louise_fly_right) {
		boss.pos.velocity.x = word(-static_cast<std::int32_t>(boss.pos.velocity.x));
	}
	if(boss.pos.cur.y <= louise_fly_top || boss.pos.cur.y >= louise_fly_bottom) {
		boss.pos.velocity.y = word(-static_cast<std::int32_t>(boss.pos.velocity.y));
	}
	return boss.phase_frame >= frames ? FlyResult::complete : FlyResult::moving;
}
[[nodiscard]] LouisePatternStage louise_pattern_stage(
	State &state, std::int16_t frames, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame < 16) return LouisePatternStage::charging;
	if(boss.phase_frame < 32) {
		boss.sprite = pat_louise_charge;
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 16), 9U, 8U);
		if(boss.phase_frame == 16) presentation.sound(8U);
		return LouisePatternStage::charging;
	}
	if(boss.phase_frame == 32) {
		boss.sprite = pat_louise_fire;
		return LouisePatternStage::fire_first;
	}
	if(boss.phase_frame < frames) return LouisePatternStage::firing;
	return LouisePatternStage::done;
}
void louise_pattern_done(State &state) noexcept
{
	state.boss.mode = 0U;
	state.boss.phase_frame = 0;
	state.boss.sprite = pat_louise;
}
[[nodiscard]] bool louise_pattern_blue_fans(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	switch(louise_pattern_stage(state, 64, presentation)) {
	case LouisePatternStage::fire_first:
		bullet.spawn_type = bullet_spawn_type_normal;
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		bullet.speed = boss.statebytes[10U];
		bullet.spread = 5U;
		bullet.spread_angle_delta = 6U;
		bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::exact_linear);
		boss.statebytes[15U] = static_cast<std::uint8_t>(1U - boss.statebytes[15U]);
		bullet.angle = boss.statebytes[15U] != 0U ? 0x20U : 0x60U;
		if(!bullet_template_tune(state)) return false;
		presentation.sound(15U);
		if(boss.statebytes[10U] < 0x24U) boss.statebytes[10U] = byte_add(boss.statebytes[10U], 4);
		[[fallthrough]];
	case LouisePatternStage::firing:
		if((boss.phase_frame & 1) == 0) {
			if(!bullets_add_regular(state)) return false;
			bullet.angle = byte_add(bullet.angle, 0x80);
			if(!bullets_add_regular(state)) return false;
			bullet.angle = byte_add(bullet.angle, boss.statebytes[15U] != 0U ? 0x87 : 0x79);
			bullet.speed = byte_add(bullet.speed, 4);
		}
		return true;
	case LouisePatternStage::done:
		louise_pattern_done(state);
		return true;
	case LouisePatternStage::charging:
		return true;
	}
	return false;
}
[[nodiscard]] bool louise_pattern_red_stacks(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	switch(louise_pattern_stage(state, 96, presentation)) {
	case LouisePatternStage::fire_first:
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.patnum = pat_bullet16_vector_red;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::stack);
		bullet.speed = 32U;
		switch(state.rank) {
		case 0U: bullet.stack = 10U; bullet.stack_speed_delta = 5U; break;
		case 1U:
		case 4U: bullet.stack = 12U; bullet.stack_speed_delta = 5U; break;
		case 2U: bullet.stack = 12U; bullet.stack_speed_delta = 6U; break;
		case 3U: bullet.stack = 14U; bullet.stack_speed_delta = 6U; break;
		default: return false;
		}
		bullet.angle = 0xC0U;
		return true;
	case LouisePatternStage::firing:
		if((boss.phase_frame & 1) == 0) {
			if(!bullets_add_regular(state)) return false;
			bullet.angle = byte_add(bullet.angle, boss.statebytes[15U] != 0U ? -8 : 8);
			presentation.sound(15U);
		}
		return true;
	case LouisePatternStage::done:
		louise_pattern_done(state);
		return true;
	case LouisePatternStage::charging:
		return true;
	}
	return false;
}
[[nodiscard]] bool louise_pattern_symmetric_bursts(State &state) noexcept
{
	const auto frame = state.boss.phase_frame;
	if((frame % 32) != 0) return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = bullet_spawn_type_cloud_backwards;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::single);
	bullet.patnum = 0U;
	if(!bullet_template_tune(state)) return false;
	bullet.speed = 48U;
	bullet.angle = (frame % 64) == 0 ? 0x24U : 0x64U;
	for(std::uint8_t i{}; i < 8U; ++i) {
		const auto angle_first = bullet.angle;
		if(!bullets_add_regular(state)) return false;
		bullet.angle = byte_add(bullet.angle, 0x80);
		if(!bullets_add_regular(state)) return false;
		bullet.angle = (frame % 64) == 0
			? byte_add(0x40U, -static_cast<std::int16_t>(bullet.angle))
			: byte_add(0xC0U, -static_cast<std::int16_t>(bullet.angle));
		if(!bullets_add_regular(state)) return false;
		bullet.angle = byte_add(bullet.angle, 0x80);
		if(!bullets_add_regular(state)) return false;
		bullet.angle = angle_first;
		bullet.speed = byte_add(bullet.speed, -5);
		bullet.angle = byte_add(bullet.angle, 8);
	}
	return true;
}
[[nodiscard]] bool louise_pattern_sweeping_lasers(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &laser = state.lasers.laser_template;
	switch(louise_pattern_stage(state, 128, presentation)) {
	case LouisePatternStage::fire_first: {
		laser.coords.origin = boss.pos.cur;
		laser.coords.width = 6U;
		laser.color = 8U;
		laser.coords.angle = 0xF0U;
		laser.active_at_age = 30;
		laser.shootout_speed = 104;
		const auto flip = pc98::th0405::randring_next16_and(state.random, 1U);
		if(!flip) return false;
		boss.statebytes[15U] = static_cast<std::uint8_t>(*flip);
		[[fallthrough]];
	}
	case LouisePatternStage::firing:
		if(laser.coords.angle >= 0xF0U || laser.coords.angle <= 128U) {
			if((boss.phase_frame & 3) == 0) {
				if(boss.statebytes[15U] != 0U) {
					if(!lasers_shootout_add(state, presentation)) return false;
				} else {
					laser.coords.angle = byte_add(128U, -static_cast<std::int16_t>(laser.coords.angle));
					if(!lasers_shootout_add(state, presentation)) return false;
					laser.coords.angle = byte_add(128U, -static_cast<std::int16_t>(laser.coords.angle));
				}
				laser.coords.angle = byte_add(laser.coords.angle, 10);
			}
		}
		return true;
	case LouisePatternStage::done:
		louise_pattern_done(state);
		return true;
	case LouisePatternStage::charging:
		return true;
	}
	return false;
}
[[nodiscard]] bool louise_pattern_turning_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	switch(louise_pattern_stage(state, 64, presentation)) {
	case LouisePatternStage::fire_first: {
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.patnum = pat_bullet16_vector_red;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring_stack_aimed);
		bullet.speed = boss.statebytes[10U];
		if(boss.statebytes[10U] < 0x20U) boss.statebytes[10U] = byte_add(boss.statebytes[10U], 4);
		bullet.spread = 24U;
		bullet.spread_angle_delta = 8U;
		bullet.stack = 5U;
		bullet.stack_speed_delta = 9U;
		bullet.angle = 0U;
		bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::decelerate_then_turn);
		state.bullets.special_turns_max = 1U;
		const auto turns_right = pc98::th0405::randring_next16_and(state.random, 1U);
		if(!turns_right) return false;
		state.bullets.bullet_template_special_angle = *turns_right != 0U ? 0x20U : 0xE0U;
		if(!bullet_template_tune(state)) return false;
		presentation.sound(15U);
		return bullets_add_special(state);
	}
	case LouisePatternStage::firing:
		return true;
	case LouisePatternStage::done:
		louise_pattern_done(state);
		return true;
	case LouisePatternStage::charging:
		return true;
	}
	return false;
}
[[nodiscard]] bool louise_pattern_aimed_pairs(
	State &state, Presentation &presentation
) noexcept
{
	if((state.boss.phase_frame % 32) != 0) return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = static_cast<std::uint8_t>(
		bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
	);
	bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_aimed);
	bullet.patnum = 0U;
	bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::exact_linear);
	bullet.speed = 32U;
	bullet.spread = 32U;
	bullet.spread_angle_delta = 2U;
	if(!bullet_template_tune(state)) return false;
	bullet.angle = (state.boss.phase_frame % 64) == 0 ? 0x1FU : 0xE1U;
	if(!bullets_add_special(state)) return false;
	presentation.sound(3U);
	return true;
}
[[nodiscard]] bool louise_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	state.shots.homing_target = boss.pos.cur;
	++boss.phase_frame;
	state.bullets.bullet_template.spawn_type = bullet_spawn_type_normal;
	state.bullets.bullet_template.origin = boss.pos.cur;
	state.bullets.gather_template.center = boss.pos.cur;
	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 1) {
			boss.hp = louise_hp_total;
			boss.phase_end_hp = louise_phase_2_end_hp;
			state.bullets.gather_template.radius = 1024;
			state.bullets.gather_template.angle_delta = 2U;
			state.bullets.gather_template.ring_points = 8;
		}
		if(!boss_hittest_invincible(state, presentation)) return false;
		if(boss.phase_frame >= 128) {
			++boss.phase;
			boss.phase_frame = 0;
			presentation.sound(13U);
		}
		break;
	case 1U:
		if(!boss_hittest_invincible(state, presentation)) return false;
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.phase_frame = 0;
			boss.mode = 1U;
			boss.phase_state = 0U;
			boss.statebytes[10U] = 0x18U;
			boss.statebytes[15U] = 0U;
		}
		break;
	case 2U:
		switch(boss.mode) {
		case 0U:
			if(louise_flystep_random(state, 16, 96) == FlyResult::unsupported) return false;
			if(boss.phase_frame >= 96) {
				boss.phase_frame = 0;
				++boss.phase_state;
				boss.mode = static_cast<std::uint8_t>((boss.phase_state & 1U) + 1U);
				if(boss.phase_state >= 20U) {
					if(!boss_phase_next(state, presentation, 1U, louise_phase_3_end_hp)) return false;
					break;
				}
			}
			break;
		case 1U:
			if(!louise_pattern_blue_fans(state, presentation)) return false;
			break;
		case 2U:
			if(!louise_pattern_red_stacks(state, presentation)) return false;
			break;
		default:
			return false;
		}
		if(boss.phase != 2U || !boss_hittest(state, presentation)) break;
		if(!boss_score_bonus(state, presentation, 10U) ||
			!boss_phase_next(state, presentation, 1U, louise_phase_3_end_hp)) return false;
		break;
	case 3U:
		(void)boss_hittest(state, presentation);
		if(boss.phase_frame >= 64) {
			boss.phase_frame = 0;
			++boss.phase;
			boss.sprite = pat_louise_fire;
		}
		break;
	case 4U:
		if(!louise_pattern_symmetric_bursts(state)) return false;
		if(louise_flystep_random(state, 14, 128) == FlyResult::unsupported) return false;
		if(boss.phase_frame >= 128) {
			boss.phase_frame = 0;
			++boss.phase_state;
			if(boss.phase_state >= 20U) {
				if(!boss_phase_next(state, presentation, 2U, louise_phase_4_end_hp)) return false;
				boss.statebytes[14U] = 0U;
				boss.statebytes[10U] = 0x14U;
				break;
			}
		}
		if(boss.phase != 4U || !boss_hittest(state, presentation)) break;
		if(!boss_score_bonus(state, presentation, 10U) ||
			!boss_phase_next(state, presentation, 2U, louise_phase_4_end_hp)) return false;
		boss.statebytes[14U] = 0U;
		boss.statebytes[10U] = 0x14U;
		break;
	case 5U:
		switch(boss.mode) {
		case 0U:
			if(louise_flystep_random(state, 48, 48) == FlyResult::unsupported) return false;
			if(boss.phase_frame >= 48) {
				boss.mode = static_cast<std::uint8_t>(boss.statebytes[14U] + 1U);
				boss.statebytes[14U] = static_cast<std::uint8_t>(1U - boss.statebytes[14U]);
				boss.phase_frame = 0;
				++boss.phase_state;
				if(boss.phase_state >= 20U) {
					if(!boss_phase_next(state, presentation, 1U, louise_phase_5_end_hp)) return false;
					break;
				}
			}
			break;
		case 1U:
			if(!louise_pattern_sweeping_lasers(state, presentation)) return false;
			break;
		case 2U:
			if(!louise_pattern_turning_rings(state, presentation)) return false;
			break;
		default:
			return false;
		}
		if(boss.phase != 5U || !boss_hittest(state, presentation)) break;
		if(!boss_score_bonus(state, presentation, 10U) ||
			!boss_phase_next(state, presentation, 1U, louise_phase_5_end_hp)) return false;
		break;
	case 6U:
		(void)boss_hittest(state, presentation);
		if(boss.phase_frame >= 64) {
			boss.phase_frame = 0;
			++boss.phase;
			boss.sprite = pat_louise_fire;
		}
		break;
	case 7U:
		if(!louise_pattern_aimed_pairs(state, presentation)) return false;
		if(louise_flystep_random(state, 8, 128) == FlyResult::unsupported) return false;
		if(boss.phase_frame >= 128) {
			boss.phase_frame = 0;
			++boss.phase_state;
			if(boss.phase_state >= 10U) {
				boss.phase_state = 0U;
				boss.phase = phase_explode_small;
				break;
			}
		}
		if(!boss_hittest(state, presentation)) break;
		boss.phase_state = 1U;
		boss.phase_frame = 0;
		boss.phase = phase_explode_small;
		break;
	default:
		if(!boss_defeat_update(state, presentation)) return false;
		break;
	}
	presentation.boss_hp(boss.hp, louise_hp_total);
	return true;
}
[[nodiscard]] bool sara_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	state.shots.homing_target = boss.pos.cur;
	++boss.phase_frame;
	state.bullets.bullet_template.spawn_type = bullet_spawn_type_normal;
	state.bullets.bullet_template.origin = boss.pos.cur;
	state.bullets.gather_template.center = boss.pos.cur;
	switch (boss.phase)
	{
	case 0U:
		if (boss.phase_frame == 1)
		{
			boss.hp = sara_hp_total;
			boss.phase_end_hp = sara_phase_2_end_hp;
		}
		if (!boss_hittest_invincible(state, presentation))
			return false;
		if (boss.phase_frame >= 224)
		{
			if (boss.phase_frame == 224)
			{
				state.bullets.gather_template.center = boss.pos.cur;
				state.bullets.gather_template.radius = 3072;
				state.bullets.gather_template.ring_points = 32;
				state.bullets.gather_template.angle_delta = 3U;
				state.bullets.gather_template.color = 9U;
			}
			if ((boss.phase_frame & 7) == 0)
				gather_add_only(state);
			if (boss.phase_frame == 224)
				state.bullets.gather_template.color = 8U;
		}
		if (boss.phase_frame >= 256)
		{
			state.bullets.gather_template.radius = 1024;
			state.bullets.gather_template.angle_delta = 2U;
			state.bullets.gather_template.ring_points = 8;
			++boss.phase;
			boss.phase_frame = 0;
			presentation.sound(13U);
		}
		break;
	case 1U:
		if (!boss_hittest_invincible(state, presentation))
			return false;
		if (boss.phase_frame >= 32)
		{
			++boss.phase;
			boss.phase_frame = 0;
			boss.mode = 0U;
			boss.phase_state = 0U;
			boss.statebytes[10] = 0xFFU;
			boss.statebytes[9] = 64U;
		}
		break;
	case 2U:
		if (boss.mode == 0U)
		{
			const auto selection = phase2_wait_fly_select(state);
			if (selection == PatternSelectResult::unsupported)
				return false;
			if (selection == PatternSelectResult::timed_out)
			{
				if (!boss_phase_next(state, presentation, 1U, sara_phase_3_end_hp))
					return false;
				boss.statebytes[9] = 80U;
				break;
			}
		}
		else
		{
			if (!phase2_with_pattern(state, presentation))
				return false;
			if (boss.phase_frame == 0 && boss.statebytes[9] > 12U)
			{
				boss.statebytes[9] = byte_add(boss.statebytes[9], -12);
			}
		}
		if (!boss_hittest(state, presentation))
			break;
		if (!boss_score_bonus(state, presentation, 5U) ||
			!boss_phase_next(state, presentation, 1U, sara_phase_3_end_hp))
			return false;
		boss.statebytes[9] = 80U;
		break;
	case 3U:
		if (boss.mode == 0U)
		{
			const auto selection = phase3_wait_fly_select(state);
			if (selection == PatternSelectResult::unsupported)
				return false;
			if (selection == PatternSelectResult::timed_out)
			{
				if (!boss_phase_next(state, presentation, 2U, sara_phase_4_end_hp))
					return false;
				break;
			}
		}
		else
		{
			if (!phase3_with_pattern(state, presentation))
				return false;
			if (boss.phase_frame == 0 && boss.statebytes[9] < 180U)
			{
				boss.statebytes[9] = byte_add(boss.statebytes[9], 24);
			}
		}
		if (!boss_hittest(state, presentation))
			break;
		if (!boss_score_bonus(state, presentation, 5U) ||
			!boss_phase_next(state, presentation, 2U, sara_phase_4_end_hp))
			return false;
		break;
	case 4U:
		if (!phase4_pattern(state, presentation))
			return false;
		if (boss.phase_frame >= 1300)
		{
			boss.phase_state = 0U;
		}
		else
		{
			if (!boss_hittest(state, presentation))
				break;
			boss.phase_state = 1U;
		}
		if (!laser_stop(state, x_right) || !laser_stop(state, x_left))
			return false;
		boss.phase_frame = 0;
		boss.phase = phase_explode_small;
		break;
	default:
		if (!boss_defeat_update(state, presentation))
			return false;
		break;
	}
	presentation.boss_hp(boss.hp, sara_hp_total);
	return true;
}
void explosion_update(BossExplosion &explosion) noexcept
{
	explosion.radius_cur.x = word_add(explosion.radius_cur.x, explosion.radius_delta.x);
	explosion.radius_cur.y = word_add(explosion.radius_cur.y, explosion.radius_delta.y);
	explosion.age = static_cast<std::uint8_t>(explosion.age + 1U);
	if (explosion.age >= 32U)
		explosion.alive = false;
}
void fights_render_tail_explosions(State &state) noexcept
{
	for (auto &explosion : state.boss_explosions.small)
	{
		if (explosion.alive)
			explosion_update(explosion);
	}
	auto &big = state.boss_explosions.big;
	if (big.alive)
	{
		explosion_update(big);
		++state.boss_explosions.big_frame;
		const auto frame = state.boss_explosions.big_frame;
		if (frame < 8 && (frame & 1) != 0)
			state.palette_tone = 150U;
		else if (frame >= 8 && frame < 16)
		{
			state.palette_tone = static_cast<std::uint16_t>(196 - frame * 6);
		}
		else
			state.palette_tone = 100U;
		state.palette_changed = true;
	}
	else
	{
		state.boss_explosions.big_frame = 0;
	}
}
} // namespace

void stage1_fights_setup(State &state) noexcept
{
	state.midboss_active = false;
	state.boss_active = false;
	state.midboss.phase = 0U;
	state.midboss.phase_frame = 0;
	state.midboss.hp = 0;
	state.midboss.frames_until = 2500U;
	state.midboss.pos.cur = {3072, 1536};
	state.midboss.pos.prev = state.midboss.pos.cur;
	state.midboss.pos.velocity = {};
	state.midboss.hp = midboss1_hp_total;
	state.midboss.sprite = 200U;
	state.boss.phase = 0U;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.phase_frame = 0;
	state.boss.pos.velocity = {};
	state.boss.damage_this_frame = 0U;
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
	state.boss.phase_timed_out = true;
	state.boss.pos.cur = {3072, 1024};
	state.boss.pos.prev = state.boss.pos.cur;
	state.boss.sprite = pat_sara_stay;
	state.boss.hitbox_radius = {384, 384};
	state.boss.sprite_left = pat_sara_left;
	state.boss.sprite_right = pat_sara_right;
	state.boss.sprite_stay = pat_sara_stay;
}
void stage1_midboss_activate(State &state) noexcept
{
	if (state.midboss.frames_until != state.stage_frame)
		return;
	state.midboss.phase = 0U;
	state.midboss.phase_frame = 0;
	state.midboss_active = true;
}
void stage1_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}
bool stage1_fights_update(State &state, Presentation &presentation) noexcept
{
	if (state.midboss_active && !midboss1_update(state, presentation))
		return false;
	if (state.boss_active && !sara_update(state, presentation))
		return false;
	return true;
}
void stage1_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	// m1_render only clears the hit flash after rendering the vulnerable
	// phase-3 sprite.  In particular, the defeat hit remains live through the
	// big-explosion phase, matching the shared midboss-state carry-over.
	if (state.midboss_active && state.midboss.phase == 3U && state.midboss.damage_this_frame != 0U)
	{
		state.midboss.damage_this_frame = 0U;
	}
	fights_render_tail_explosions(state);
	(void)presentation;
}

void stage2_fights_setup(State &state) noexcept
{
	state.midboss_active = false;
	state.boss_active = false;
	state.midboss.phase = 0U;
	state.midboss.phase_frame = 0;
	state.midboss.damage_this_frame = 0U;
	state.midboss.frames_until = 2750U;
	state.midboss.pos.cur = {3072, -512};
	state.midboss.pos.prev = state.midboss.pos.cur;
	state.midboss.pos.velocity = {0, 8};
	state.midboss.hp = midboss2_hp_total;
	state.midboss.sprite = pat_midboss2_entrance;
	state.midboss.stage2_center = {};
	state.boss.phase = 0U;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.phase_frame = 0;
	state.boss.pos.velocity = {};
	state.boss.damage_this_frame = 0U;
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
	state.boss.phase_timed_out = true;
	state.boss.pos.cur = {3072, 1024};
	state.boss.pos.prev = state.boss.pos.cur;
	state.boss.sprite = pat_louise;
	state.boss.hitbox_radius = {384, 384};
}
void stage2_midboss_activate(State &state) noexcept
{
	if (state.midboss.frames_until != state.stage_frame)
		return;
	state.midboss.phase = 0U;
	state.midboss.phase_frame = 0;
	state.midboss_active = true;
}
void stage2_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}
bool stage2_fights_update(State &state, Presentation &presentation) noexcept
{
	if (state.midboss_active && !midboss2_update(state, presentation))
		return false;
	if (state.boss_active && !louise_update(state, presentation))
		return false;
	return true;
}
void stage2_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	// midboss2_render() clears any hit flash once its ordinary sprite has
	// been drawn. Its offscreen entrance does not reach that branch, while its
	// big-explosion phase deliberately preserves the existing hit byte.
	if (state.midboss_active && state.midboss.phase < phase_explode_small &&
		state.midboss.pos.cur.y >= 0 && state.midboss.damage_this_frame != 0U)
	{
		state.midboss.damage_this_frame = 0U;
	}
	fights_render_tail_explosions(state);
	(void)presentation;
}
} // namespace th05::core
  // namespace th05::core
