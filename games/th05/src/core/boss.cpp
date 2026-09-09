#include "th05/core/boss.hpp"
#include "th05/core/b6ball.hpp"
#include "th05/core/bullet.hpp"
#include "th05/core/firewave.hpp"
#include "th05/core/item.hpp"
#include "th05/core/laser.hpp"
#include "th05/core/midboss_extra.hpp"
#include "th05/core/stage4_projectiles.hpp"
#include "th05/core/sword.hpp"
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
constexpr std::uint8_t pat_bullet16_outlined_ball_blue = 45U;
constexpr std::uint8_t pat_bullet16_small_ball_blue = 46U;
constexpr std::uint8_t pat_bullet16_cross_blue = 47U;
constexpr std::uint8_t pat_bullet16_ball_red = 48U;
constexpr std::uint8_t pat_bullet16_small_ball_red = 50U;
constexpr std::uint8_t pat_sara_stay = 180U;
constexpr std::uint8_t pat_sara_right = 184U;
constexpr std::uint8_t pat_sara_left = 186U;
constexpr std::uint8_t pat_sara_spin = 188U;
constexpr std::uint8_t pat_sara_spin_last = 191U;
constexpr std::uint8_t pat_midboss1 = 204U;
constexpr std::uint8_t pat_midboss2_entrance = 202U;
constexpr std::uint8_t pat_midboss2_attack = 206U;
constexpr std::uint8_t pat_bullet16_vector_red = 84U;
constexpr std::uint8_t pat_bullet16_vector_blue = 116U;
constexpr std::uint8_t pat_bullet16_directional_blue = 52U;
constexpr std::uint8_t pat_bullet16_directional_green = 68U;
constexpr std::uint8_t pat_yumeko_still = 180U;
constexpr std::uint8_t pat_yumeko_cast = 184U;
constexpr std::uint8_t pat_yumeko_fly = 188U;
constexpr std::uint8_t pat_yumeko_secondary = 192U;
constexpr std::uint8_t pat_midboss5_first = 212U;
constexpr std::uint8_t pat_midboss5_last = 219U;
constexpr std::uint8_t pat_shinki_still = 180U;
constexpr std::uint8_t pat_shinki_cast = 184U;
constexpr std::uint8_t pat_shinki_right = 188U;
constexpr std::uint8_t pat_shinki_left = 190U;
constexpr std::uint8_t pat_shinki_wings_white = 192U;
constexpr std::uint8_t pat_shinki_wings_purple = 196U;
constexpr std::int16_t shinki_hp_total = 22800;
constexpr std::int16_t shinki_phase_2_end_hp = 20600;
constexpr std::int16_t shinki_phase_3_end_hp = 18400;
constexpr std::int16_t shinki_phase_6_end_hp = 14600;
constexpr std::int16_t shinki_phase_7_end_hp = 11600;
constexpr std::int16_t shinki_phase_9_end_hp = 8600;
constexpr std::int16_t shinki_phase_10_devil_laser_hp = 5600;
constexpr std::int16_t shinki_phase_10_devil_fast_hp = 3800;
constexpr std::int16_t shinki_phase_10_end_hp = 2800;
constexpr std::int16_t shinki_phase_12_end_hp = 0;
constexpr std::int16_t shinki_wing_width = 256 * subpixel_factor;
constexpr std::int16_t shinki_wing_height = 96 * subpixel_factor;
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
	// This is an outcome predicate, not a fallible operation: false means the
	// hittest completed and the midboss is still alive. Callers that only need
	// the source side effects must explicitly discard the result.
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
							   std::uint16_t units, std::uint16_t unit_value = bonus_unit_value) noexcept
{
	state.score.score_delta += static_cast<std::uint32_t>(units) * unit_value;
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
							  unit_value, false);
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

[[nodiscard]] bool midboss_score_bonus_impl(State &state, Presentation &presentation,
									   std::uint16_t units) noexcept
{
	return score_bonus(state, presentation, state.midboss.pos.cur, units, 1280U);
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
			if (!midboss_score_bonus_impl(state, presentation, 5U))
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
		state.midboss_callback_active = false;
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

// th05/main/boss/move.cpp: the Alice transitions use the fixed 1/16 fly-to
// helper rather than the random flystep used by Sara and Alice's patterns.
[[nodiscard]] bool boss_flystep_towards(
	State &state, std::int16_t target_x, std::int16_t target_y
) noexcept
{
	auto &boss = state.boss;
	std::uint8_t reached_dimensions = 0U;
	auto distance = word_sub(target_x, boss.pos.cur.x);
	auto step = static_cast<std::int16_t>(distance / 16);
	if(step != 0) {
		boss.pos.cur.x = word_add(boss.pos.cur.x, step);
	} else {
		step = static_cast<std::int16_t>(distance / 4);
		if(step != 0) boss.pos.cur.x = word_add(boss.pos.cur.x, step);
		else {
			boss.pos.cur.x = target_x;
			++reached_dimensions;
		}
	}
	boss.sprite = distance < 0 ? boss.sprite_left : boss.sprite_right;
	distance = word_sub(target_y, boss.pos.cur.y);
	step = static_cast<std::int16_t>(distance / 16);
	if(step != 0) {
		boss.pos.cur.y = word_add(boss.pos.cur.y, step);
	} else {
		step = static_cast<std::int16_t>(distance / 2);
		if(step != 0) boss.pos.cur.y = word_add(boss.pos.cur.y, step);
		else {
			boss.pos.cur.y = target_y;
			++reached_dimensions;
		}
	}
	if(reached_dimensions != 2U) return false;
	boss.sprite = boss.sprite_stay;
	return true;
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
[[nodiscard]] bool boss_defeat_update(
	State &state, Presentation &presentation, std::uint8_t score_bonus = 10U
) noexcept
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
				if (!boss_score_bonus(state, presentation, score_bonus))
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
			if(!midboss_score_bonus_impl(state, presentation, 5U)) return false;
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
			if(!midboss_score_bonus_impl(state, presentation, 15U)) return false;
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
		state.midboss_callback_active = false;
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

constexpr std::uint8_t pat_midboss3 = 208U;
constexpr std::uint8_t pat_midboss3_gather_1 = 210U;
constexpr std::uint8_t pat_midboss3_gather_2 = 211U;
constexpr std::uint8_t pat_midboss3_move_first = 212U;
constexpr std::uint8_t pat_midboss3_move_last = 219U;
constexpr std::uint8_t pat_alice_still = 180U;
constexpr std::uint8_t pat_alice_2 = 182U;
constexpr std::uint8_t pat_alice_cast = 184U;
constexpr std::uint8_t pat_alice_left = 188U;
constexpr std::uint8_t pat_alice_right = 186U;
constexpr std::uint8_t pat_puppet = 190U;
constexpr std::uint8_t pat_puppet_moving = 192U;
constexpr std::uint8_t pat_puppet_cast = 194U;
constexpr std::int16_t midboss3_hp_total = 1400;
constexpr std::int16_t alice_hp_total = 9600;
constexpr std::int16_t alice_phase_2_end_hp = 7400;
constexpr std::int16_t alice_hp_per_phase = 2200;
constexpr std::int16_t puppet_hp = 500;
constexpr std::uint8_t puppet_free = 0U;
constexpr std::uint8_t puppet_alive = 1U;
constexpr std::uint8_t puppet_remove = 2U;

[[nodiscard]] bool midboss3_fire(State &state, Presentation &presentation,
	bool tune
) noexcept
{
	if(tune && !bullet_template_tune(state)) return false;
	if(!bullets_add_regular(state)) return false;
	presentation.sound(15U);
	state.midboss.pos.prev.y = state.midboss.pos.cur.y;
	state.midboss.pos.cur.y = word_add(state.midboss.pos.cur.y, -64);
	return true;
}

void midboss3_reposition(State &state) noexcept
{
	auto &midboss = state.midboss;
	if((midboss.phase_frame & 3) != 0) return;
	auto &moving_down = state.boss.statebytes[15U];
	if(moving_down == 0U) {
		++midboss.sprite;
		if(midboss.sprite < pat_midboss3_move_last + 1U) return;
		midboss.sprite = pat_midboss3_move_last;
		midboss.pos.cur.y = word_add(midboss.pos.cur.y, 192);
		midboss.pos.cur.x = state.player_pos.cur.x;
		if(midboss.pos.cur.x < 1024) midboss.pos.cur.x = 1024;
		else if(midboss.pos.cur.x > 5120) midboss.pos.cur.x = 5120;
		++moving_down;
	} else {
		--midboss.sprite;
		if(midboss.sprite >= pat_midboss3_move_first) return;
		midboss.sprite = pat_midboss3;
		moving_down = 0U;
		midboss.phase_frame = 0;
		state.boss.statebytes[14U] = 0U;
	}
}

void midboss3_gather(State &state, Presentation &presentation) noexcept
{
	auto &midboss = state.midboss;
	gather_add_only_3stack(state, static_cast<std::int16_t>(midboss.phase_frame - 1),
		11U, 10U);
	if(midboss.phase_frame == 1) {
		presentation.sound(8U);
		midboss.sprite = pat_midboss3_gather_1;
	} else if(midboss.phase_frame == 16) {
		midboss.sprite = pat_midboss3_gather_2;
	}
}

[[nodiscard]] bool midboss3_pattern_spread_stacks(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	auto &bullet = state.bullets.bullet_template;
	if(midboss.phase_frame < 16) {
		midboss3_gather(state, presentation);
		return true;
	}
	if(midboss.phase_frame == 32) {
		bullet.spawn_type = bullet_spawn_type_cloud_forwards;
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.speed = 24U;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_stack);
		bullet.spread = 3U;
		bullet.spread_angle_delta = 0x10U;
		bullet.stack = 4U;
		bullet.stack_speed_delta = 8U;
		bullet.angle = 0x40U;
		return midboss3_fire(state, presentation, true);
	}
	if(midboss.phase_frame == 48) {
		bullet.patnum = pat_bullet16_ball_red;
		bullet.speed = 48U;
		bullet.spread = 4U;
		bullet.spread_angle_delta = 5U;
		bullet.stack = 4U;
		bullet.stack_speed_delta = 8U;
		return midboss3_fire(state, presentation, true);
	}
	if(midboss.phase_frame == 64) {
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.speed = 32U;
		bullet.spread = 7U;
		bullet.spread_angle_delta = 10U;
		bullet.stack = 4U;
		bullet.stack_speed_delta = 8U;
		return midboss3_fire(state, presentation, true);
	}
	if(midboss.phase_frame >= 96) midboss3_reposition(state);
	return true;
}

[[nodiscard]] bool midboss3_rank_spread(
	State &state, std::uint8_t &spread, std::uint8_t &angle_delta,
	std::array<std::array<std::uint8_t, 2U>, 4U> values
) noexcept
{
	if(state.rank >= values.size()) return false;
	spread = values[state.rank][0U];
	angle_delta = values[state.rank][1U];
	return true;
}

[[nodiscard]] bool midboss3_pattern_aimed_stacks(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	auto &bullet = state.bullets.bullet_template;
	if(midboss.phase_frame < 16) {
		midboss3_gather(state, presentation);
		return true;
	}
	if(midboss.phase_frame == 32) {
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.speed = 24U;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::stack_aimed);
		bullet.stack = 8U;
		bullet.stack_speed_delta = 8U;
		bullet.angle = 0U;
		return midboss3_fire(state, presentation, false);
	}
	if(midboss.phase_frame == 48) {
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_stack_aimed);
		bullet.patnum = pat_bullet16_ball_red;
		if(!midboss3_rank_spread(state, bullet.spread, bullet.spread_angle_delta,
			{{{1U, 1U}, {2U, 4U}, {3U, 3U}, {4U, 4U}}})) return false;
		bullet.stack = 8U;
		bullet.stack_speed_delta = 8U;
		return midboss3_fire(state, presentation, false);
	}
	if(midboss.phase_frame == 64) {
		bullet.patnum = pat_bullet16_ball_blue;
		if(!midboss3_rank_spread(state, bullet.spread, bullet.spread_angle_delta,
			{{{1U, 1U}, {3U, 5U}, {4U, 5U}, {5U, 4U}}})) return false;
		bullet.stack = 8U;
		bullet.stack_speed_delta = 8U;
		return midboss3_fire(state, presentation, false);
	}
	if(midboss.phase_frame >= 96) midboss3_reposition(state);
	return true;
}

[[nodiscard]] bool midboss3_pattern_wide_spreads(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	auto &bullet = state.bullets.bullet_template;
	if(midboss.phase_frame < 16) {
		midboss3_gather(state, presentation);
		return true;
	}
	if(midboss.phase_frame == 32) {
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.speed = 48U;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		bullet.spread = 16U;
		bullet.spread_angle_delta = 8U;
		bullet.angle = 0x40U;
		return midboss3_fire(state, presentation, true);
	}
	if(midboss.phase_frame == 48) {
		bullet.speed = 48U;
		bullet.spread = 21U;
		bullet.spread_angle_delta = 4U;
		return midboss3_fire(state, presentation, true);
	}
	if(midboss.phase_frame == 64) {
		bullet.spread = 16U;
		bullet.spread_angle_delta = 8U;
		return midboss3_fire(state, presentation, true);
	}
	if(midboss.phase_frame >= 96) midboss3_reposition(state);
	return true;
}

[[nodiscard]] bool midboss3_update(State &state, Presentation &presentation) noexcept
{
	auto &midboss = state.midboss;
	state.bullets.bullet_template.origin = midboss.pos.cur;
	state.bullets.gather_template.center = midboss.pos.cur;
	++midboss.phase_frame;
	switch(midboss.phase) {
	case 0U:
		(void)pc98::th0405::update_motion(midboss.pos);
		if(!midboss_hittest_invincible(state, presentation)) return false;
		if(midboss.phase_frame >= 192) {
			++midboss.phase;
			midboss.phase_frame = 0;
			midboss.angle = 0U;
			state.boss.statebytes[15U] = 0U;
			state.boss.statebytes[14U] = 1U;
			state.boss.statebytes[13U] = 1U;
			midboss.pos.velocity.x = 0;
		}
		break;
	case 1U:
		midboss.pos.prev = midboss.pos.cur;
		switch(state.boss.statebytes[14U]) {
		case 0U:
			if(midboss.phase_frame >= 32) {
				state.boss.statebytes[14U] = static_cast<std::uint8_t>(
					(state.boss.statebytes[13U] % 3U) + 1U
				);
				++state.boss.statebytes[13U];
				midboss.phase_frame = 0;
				if(state.boss.statebytes[13U] >= 13U) goto defeated;
			}
			break;
		case 1U:
			if(!midboss3_pattern_spread_stacks(state, presentation)) return false;
			break;
		case 2U:
			if(!midboss3_pattern_aimed_stacks(state, presentation)) return false;
			break;
		case 3U:
			if(!midboss3_pattern_wide_spreads(state, presentation)) return false;
			break;
		default:
			return false;
		}
		// The shared native helper returns its "HP exhausted" predicate; M3's
		// source call is a side-effect-only hittest, so an alive midboss must
		// continue rather than turn that false predicate into an unsupported run.
		if(midboss.sprite < pat_midboss3_move_first) {
			(void)midboss_hittest(state, presentation);
		}
		if(midboss.hp > 0) break;
		state.bullets.zap.start();
		if(!midboss_score_bonus_impl(state, presentation, 15U)) return false;
		items_add(state, presentation, midboss.pos.cur, ItemType::one_up);
	defeated:
		midboss.phase = phase_explode_big;
		midboss.sprite = pat_enemy_kill;
		midboss.phase_frame = 0;
		if(!pc98::th0405::sparks_add_circle(
			pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
			midboss.pos.cur, 128, 48U
		)) return false;
		presentation.sound(12U);
		break;
	case phase_explode_big:
		if(midboss.phase_frame == 1) {
			state.playfield_shake_anim_time = 10;
			state.midboss_active = false;
		}
		if((midboss.phase_frame % 16) == 0) {
			++midboss.sprite;
			if(midboss.sprite >= pat_enemy_kill_last + 1U) midboss.phase = phase_none;
		}
		presentation.boss_hp(midboss.hp, midboss3_hp_total);
		return true;
	default:
		state.midboss_active = false;
		state.midboss_callback_active = false;
		midboss.hp = 0;
		presentation.boss_hp(midboss.hp, midboss3_hp_total);
		return true;
	}
	presentation.boss_hp(midboss.hp, midboss3_hp_total);
	state.shots.homing_target = midboss.pos.cur;
	return true;
}

[[nodiscard]] bool puppet_player_hittest(const State &state, const PuppetState &puppet) noexcept
{
	const auto hit_x = static_cast<std::uint16_t>(word_add(
		word_sub(puppet.pos.cur.x, state.player_pos.cur.x), 192
	));
	const auto hit_y = static_cast<std::uint16_t>(word_add(
		word_sub(puppet.pos.cur.y, state.player_pos.cur.y), 192
	));
	return hit_x < 384U && hit_y < 384U;
}

[[nodiscard]] bool puppet_hittest(
	State &state, Presentation &presentation, PuppetState &puppet
) noexcept
{
	const auto hit = shots_hittest(state.shots, {puppet.pos.cur, {320, 320}},
		state.shot_hit_spark_parity,
		{state.bombing, false, static_cast<ShotPlaychar>(state.playchar), state.stage_id,
			static_cast<std::uint8_t>(state.stage_frame & 3U), state.bomb_frame,
			state.player_pos.cur},
		pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset}, state.random,
		state.score);
	if(!hit) return false;
	puppet.damage_this_frame = static_cast<std::int16_t>(*hit);
	if(puppet.damage_this_frame != 0) presentation.sound(4U);
	puppet.hp = word_sub(puppet.hp, puppet.damage_this_frame);
	if(puppet.hp < 0) {
		++puppet.flag;
		puppet.patnum = pat_enemy_kill;
		puppet.damage_this_frame = 0;
		presentation.sound(12U);
		state.score.score_delta += 100U;
	}
	return true;
}

[[nodiscard]] bool puppet_pattern_update(
	State &state, PuppetState &puppet, PuppetPattern pattern, bool &done
) noexcept
{
	done = false;
	auto &bullet = state.bullets.bullet_template;
	if(puppet.phase_frame == 16U) {
		// circles_add_shrinking() is presentation-only; its source call has no
		// persistent TH05 gameplay state or RNG effect.
	}
	switch(pattern) {
	case PuppetPattern::random_pellets:
		if(puppet.phase_frame >= 32U) {
			if(puppet.phase_frame < 64U && (state.stage_frame & 1U) != 0U) {
				bullet.patnum = 0U;
				bullet.spawn_type = bullet_spawn_type_no_decelerate;
				bullet.speed = 32U;
				bullet.group = static_cast<std::uint8_t>(BulletGroup::single);
				bullet.origin = puppet.pos.cur;
				const auto angle = pc98::th0405::randring_next16(state.random);
				if(!angle) return false;
				bullet.angle = static_cast<std::uint8_t>(*angle);
				if(!bullet_template_tune(state) || !bullets_add_regular(state)) return false;
				puppet.patnum = pat_puppet_cast;
			} else if(puppet.phase_frame >= 96U) {
				puppet.patnum = pat_puppet;
				done = true;
			}
		}
		return true;
	case PuppetPattern::aimed_spread_stack:
		if(puppet.phase_frame >= 32U) {
			puppet.patnum = pat_puppet_cast;
			if(puppet.phase_frame == 32U) {
				bullet.patnum = 0U;
				bullet.origin = puppet.pos.cur;
				bullet.spawn_type = bullet_spawn_type_no_decelerate;
				bullet.speed = 24U;
				bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_stack_aimed);
				bullet.spread = 3U;
				bullet.spread_angle_delta = 18U;
				bullet.stack = 5U;
				bullet.stack_speed_delta = 5U;
				bullet.angle = 0U;
				if(!bullet_template_tune(state) || !bullets_add_regular(state)) return false;
			} else if(puppet.phase_frame >= 96U) {
				puppet.patnum = pat_puppet;
				done = true;
			}
		}
		return true;
	case PuppetPattern::aimed_spread:
		if(puppet.phase_frame >= 32U) {
			puppet.patnum = pat_puppet_cast;
			if(puppet.phase_frame < 64U && (puppet.phase_frame & 7U) == 0U) {
				bullet.patnum = 0U;
				bullet.origin = puppet.pos.cur;
				bullet.spawn_type = bullet_spawn_type_no_decelerate;
				bullet.speed = 32U;
				bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_aimed);
				bullet.spread = 5U;
				bullet.spread_angle_delta = 16U;
				bullet.angle = 0U;
				if(!bullet_template_tune(state) || !bullets_add_regular(state)) return false;
			} else if(puppet.phase_frame >= 96U) {
				puppet.patnum = pat_puppet;
				done = true;
			}
		}
		return true;
	case PuppetPattern::aimed_ring:
		if(puppet.phase_frame >= 32U) {
			puppet.patnum = pat_puppet_cast;
			if(puppet.phase_frame < 64U && (puppet.phase_frame & 7U) == 0U) {
				bullet.patnum = 0U;
				bullet.origin = puppet.pos.cur;
				bullet.spawn_type = bullet_spawn_type_no_decelerate;
				bullet.speed = 32U;
				bullet.group = static_cast<std::uint8_t>(BulletGroup::ring_aimed);
				bullet.spread = 12U;
				bullet.spread_angle_delta = 8U;
				bullet.angle = 0U;
				if(!bullet_template_tune(state) || !bullets_add_regular(state)) return false;
			} else if(puppet.phase_frame >= 96U) {
				puppet.patnum = pat_puppet;
				done = true;
			}
		}
		return true;
	case PuppetPattern::global_spread:
		if(puppet.phase_frame >= 32U) {
			puppet.patnum = pat_puppet_cast;
			if((state.stage_frame & 31U) == 0U) {
				bullet.patnum = 0U;
				bullet.origin = puppet.pos.cur;
				bullet.spawn_type = bullet_spawn_type_no_decelerate;
				bullet.speed = 24U;
				bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_aimed);
				bullet.spread = 7U;
				bullet.spread_angle_delta = 12U;
				bullet.angle = 0U;
				if(!bullet_template_tune(state) || !bullets_add_regular(state)) return false;
			}
		}
		return true;
	case PuppetPattern::idle_wait:
		done = puppet.phase_frame >= 96U;
		return true;
	case PuppetPattern::never:
		return true;
	}
	return false;
}

[[nodiscard]] bool puppets_update(State &state, Presentation &presentation) noexcept
{
	auto &puppets = state.alice.puppets;
	for(std::size_t i{}; i < puppets.size(); ++i) {
		auto &puppet = puppets[i];
		if(puppet.flag == puppet_free) continue;
		if(puppet.flag == puppet_alive) {
			if(puppet.pos.cur != puppet.pos.prev) {
				const auto move_axis = [](std::int16_t &current, std::int16_t target) noexcept {
					const auto delta = word_sub(target, current);
					if((delta / 32) != 0) current = word_add(current, delta / 32);
					else if((delta / 4) != 0) current = word_add(current, delta / 4);
					else current = target;
				};
				move_axis(puppet.pos.cur.x, puppet.pos.prev.x);
				move_axis(puppet.pos.cur.y, puppet.pos.prev.y);
				puppet.patnum = pat_puppet_moving;
			} else {
				puppet.patnum = pat_puppet;
				bool done{};
				if(!puppet_pattern_update(state, puppet, state.alice.puppet_patterns[i], done)) {
					return false;
				}
				if(done) {
					const auto x = pc98::th0405::randring_next16_mod(state.random, 5120U);
					if(!x) return false;
					const auto y = pc98::th0405::randring_next16_mod(state.random, 2560U);
					if(!y) return false;
					const auto pattern = pc98::th0405::randring_next16_and(state.random, 3U);
					if(!pattern) return false;
					puppet.pos.prev.x = word_add(std::bit_cast<std::int16_t>(*x), 512);
					puppet.pos.prev.y = word_add(std::bit_cast<std::int16_t>(*y), 256);
					state.alice.puppet_patterns[i] = static_cast<PuppetPattern>(*pattern);
					puppet.phase_frame = 0U;
				} else {
					++puppet.phase_frame;
				}
			}
			if(puppet_player_hittest(state, puppet)) state.player_is_hit = true;
			if(state.boss.statebytes[9U] != 3U) {
				if(!puppet_hittest(state, presentation, puppet)) return false;
			} else {
				puppet.damage_this_frame = 0;
			}
		} else {
			if((puppet.flag % 4U) == 0U) {
				++puppet.patnum;
				if(puppet.patnum >= pat_enemy_kill_last + 1U) {
					puppet.flag = puppet_free;
					puppet.pos.cur = {3072, -4096};
					puppet.pos.prev = {3072, -256};
					puppet.hp = puppet_hp;
					puppet.patnum = pat_puppet_moving;
					state.alice.puppet_patterns[i] = PuppetPattern::idle_wait;
					state.boss.hp = word_sub(state.boss.hp, 300);
				}
			}
			++puppet.flag;
		}
	}
	return true;
}

void puppets_spiral_in(State &state) noexcept
{
	auto &puppets = state.alice.puppets;
	const auto update = [](PuppetState &puppet, BossPoint center, std::int16_t angle_delta) noexcept {
		const auto offset = pc98::masterlib::vector2(puppet.angle, puppet.radius);
		puppet.pos.cur = {word_add(center.x, offset.x), word_add(center.y, offset.y)};
		puppet.angle = byte_add(puppet.angle, angle_delta);
		puppet.radius = word_add(puppet.radius, -32);
		puppet.pos.prev = puppet.pos.cur;
	};
	update(puppets[0], {1536, 1536}, -2);
	update(puppets[1], {4608, 1536}, 2);
}

[[nodiscard]] bool alice_pattern_random_balls_and_pellets(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame < 64) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 40), 9U, 8U);
		boss.sprite = pat_alice_cast;
		if(boss.phase_frame == 40) presentation.sound(8U);
		return true;
	}
	if(boss.phase_frame != 64) return true;
	bullet.spawn_type = bullet_spawn_type_cloud_forwards;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed);
	bullet.spread = 2U;
	if(!bullet_template_tune(state)) return false;
	for(std::uint8_t i{}; i < 32U; ++i) {
		bullet.patnum = (i & 1U) != 0U ? pat_bullet16_ball_blue : 0U;
		const auto speed = pc98::th0405::randring_next16_and(state.random, 0x3FU);
		if(!speed) return false;
		bullet.speed = static_cast<std::uint8_t>(*speed + 12U);
		const auto x = pc98::th0405::randring_next16_mod(state.random, 768U);
		if(!x) return false;
		const auto y = pc98::th0405::randring_next16_mod(state.random, 768U);
		if(!y) return false;
		bullet.origin.x = word_add(word_sub(boss.pos.cur.x, 384), std::bit_cast<std::int16_t>(*x));
		bullet.origin.y = word_add(word_sub(boss.pos.cur.y, 512), std::bit_cast<std::int16_t>(*y));
		if(!bullets_add_regular(state)) return false;
	}
	boss.sprite = pat_alice_2;
	presentation.sound(15U);
	return true;
}

void alice_pattern_finish(State &state) noexcept
{
	if(state.boss.phase_frame == 96) {
		state.boss.phase_frame = 0;
		state.boss.mode = 0U;
	}
}

[[nodiscard]] bool alice_pattern_ring_stacks(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame < 64) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 40), 9U, 8U);
		boss.sprite = pat_alice_cast;
		if(boss.phase_frame == 40) presentation.sound(8U);
		return true;
	}
	if(boss.phase_frame == 64) {
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring_stack_aimed);
		bullet.patnum = pat_bullet16_ball_red;
		bullet.spread = 28U;
		bullet.spread_angle_delta = 8U;
		bullet.stack = 4U;
		bullet.stack_speed_delta = 8U;
		bullet.speed = 24U;
		bullet.angle = 0U;
		if(!bullet_template_tune(state) || !bullets_add_regular(state)) return false;
		boss.sprite = pat_alice_still;
		presentation.sound(15U);
		return true;
	}
	alice_pattern_finish(state);
	return true;
}

[[nodiscard]] bool alice_pattern_spread_stacks(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame < 64) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 40), 9U, 8U);
		boss.sprite = pat_alice_cast;
		if(boss.phase_frame == 40) presentation.sound(8U);
		return true;
	}
	if(boss.phase_frame == 64) {
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_stack_aimed);
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.spread = 7U;
		bullet.spread_angle_delta = 8U;
		bullet.stack = 5U;
		bullet.stack_speed_delta = 7U;
		bullet.speed = 24U;
		bullet.angle = 0U;
		if(!bullet_template_tune(state) || !bullets_add_regular(state)) return false;
		boss.sprite = pat_alice_still;
		presentation.sound(15U);
		return true;
	}
	alice_pattern_finish(state);
	return true;
}

[[nodiscard]] bool alice_barrier_update(State &state, Presentation &presentation) noexcept
{
	auto &puppets = state.alice.puppets;
	if(puppets[0].pos.cur != puppets[0].pos.prev || puppets[1].pos.cur != puppets[1].pos.prev) {
		return true;
	}
	++state.alice.barrier_frame;
	if(state.alice.barrier_frame < 16U) return true;
	if(state.alice.barrier_frame == 16U) {
		state.boss.statebytes[9U] = 1U;
		puppets[0].radius = puppets[1].radius = 64;
		state.boss.statebytes[8U] = static_cast<std::uint8_t>(-0x3C);
		state.alice.barrier_fire_frames = static_cast<std::uint16_t>(
			state.alice.barrier_fire_frames + 32U
		);
		return true;
	}
	if(state.alice.barrier_frame < 32U) return true;
	if(state.alice.barrier_frame < 64U) {
		state.boss.statebytes[9U] = 2U;
		puppets[0].radius = puppets[1].radius = static_cast<std::int16_t>(
			(64 - state.alice.barrier_frame) * 2U
		);
		return true;
	}
	if(state.alice.barrier_frame < 80U) {
		if(state.alice.barrier_frame == 64U) presentation.sound(8U);
		state.boss.statebytes[9U] = 3U;
		if(state.alice.barrier_frame == 71U || state.alice.barrier_frame == 79U) {
			state.boss.statebytes[8U] = byte_add(state.boss.statebytes[8U], 2);
		}
		return true;
	}

	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = static_cast<std::uint8_t>(
		bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
	);
	bullet.patnum = 0U;
	bullet.speed = 24U;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::single);
	if(!bullet_template_tune(state)) return false;
	ShotHitbox hitbox{{word_add(puppets[0].pos.cur.x, 1024), puppets[0].pos.cur.y}, {896, 128}};
	RevengeBulletTemplate revenge{};
	bool regular_added = true;
	const auto damage = shots_hittest_revenge(state.shots, hitbox, state.shot_hit_spark_parity,
		{static_cast<ShotRank>(state.rank), static_cast<std::uint8_t>(state.stage_frame & 3U)},
		pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset}, state.random, state.random,
		revenge, state.score,
		[&state, &regular_added](RevengeBulletTemplate request) noexcept {
			state.bullets.bullet_template.origin = request.origin;
			state.bullets.bullet_template.angle = request.angle;
			if(!bullets_add_regular(state)) regular_added = false;
		}
	);
	if(!damage || !regular_added) return false;
	if(*damage != 0U) presentation.sound(9U);
	const auto x = static_cast<std::uint16_t>(word_sub(state.player_pos.cur.x, puppets[0].pos.cur.x));
	const auto y = static_cast<std::uint16_t>(word_add(
		word_sub(puppets[0].pos.cur.y, state.player_pos.cur.y), 64
	));
	if(x < 2048U && y < 128U) state.player_is_hit = true;
	if((state.boss.phase_frame % 16) != 0) return true;
	if((state.boss.phase_frame % 256) >= state.alice.barrier_fire_frames) return true;
	bullet.patnum = pat_bullet16_ball_blue;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::stack_aimed);
	bullet.angle = 0U;
	bullet.origin = {state.boss.pos.cur.x, word_add(state.boss.pos.cur.y, -128)};
	bullet.stack = 8U;
	bullet.stack_speed_delta = 6U;
	bullet.speed = 32U;
	if(!bullet_template_tune(state) || !bullets_add_regular(state)) return false;
	presentation.sound(3U);
	return true;
}

[[nodiscard]] bool alice_pattern_lasers_and_random_balls(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame < 64) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 40), 9U, 8U);
		boss.sprite = pat_alice_cast;
		if(boss.phase_frame == 40) presentation.sound(8U);
		return true;
	}
	if(boss.phase_frame == 64) {
		auto &laser = state.lasers.laser_template;
		laser.color = 6U;
		laser.coords.width = 8U;
		laser.coords.origin = state.bullets.bullet_template.origin;
		laser.active_at_age = 40;
		laser.shootout_speed = 80;
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, laser.coords.origin.y),
			word_sub(state.player_pos.cur.x, laser.coords.origin.x)
		);
		if(!angle) return false;
		laser.coords.angle = *angle;
		if(!lasers_shootout_add(state, presentation)) return false;
		laser.coords.angle = byte_add(laser.coords.angle, 0x10);
		if(!lasers_shootout_add(state, presentation)) return false;
		laser.coords.angle = byte_add(laser.coords.angle, -0x20);
		if(!lasers_shootout_add(state, presentation)) return false;

		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed);
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.spread = 20U;
		bullet.spread_angle_delta = 8U;
		bullet.speed = 24U;
		bullet.angle = 0U;
		if(!bullet_template_tune(state) || !bullets_add_regular(state)) return false;
		boss.sprite = pat_alice_still;
		return true;
	}
	alice_pattern_finish(state);
	return true;
}

[[nodiscard]] bool alice_pattern_mirrored_stacks(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame < 64) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 40), 9U, 8U);
		boss.sprite = pat_alice_cast;
		boss.statebytes[15U] = 8U;
		if(boss.phase_frame == 40) presentation.sound(8U);
		return true;
	}
	if((boss.phase_frame % 4) == 0) {
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::stack_aimed);
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.stack = 8U;
		bullet.stack_speed_delta = 7U;
		bullet.speed = 24U;
		if(!bullet_template_tune(state)) return false;
		bullet.angle = boss.statebytes[15U];
		if(!bullets_add_regular(state)) return false;
		bullet.angle = byte_add(0U, -static_cast<std::int16_t>(bullet.angle));
		if(!bullets_add_regular(state)) return false;
		boss.statebytes[15U] = byte_add(boss.statebytes[15U], 5);
		boss.sprite = pat_alice_still;
		presentation.sound(15U);
	}
	alice_pattern_finish(state);
	return true;
}

[[nodiscard]] bool alice_pattern_rotating_ring(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame < 64) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 40), 9U, 8U);
		boss.sprite = pat_alice_cast;
		boss.statebytes[15U] = 0U;
		if(boss.phase_frame == 40) presentation.sound(8U);
		return true;
	}
	if((boss.phase_frame % 4) == 0) {
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
		bullet.patnum = pat_bullet16_ball_red;
		bullet.spread = 20U;
		bullet.spread_angle_delta = 7U;
		bullet.speed = 40U;
		if(!bullet_template_tune(state)) return false;
		bullet.angle = boss.statebytes[15U];
		if(!bullets_add_regular(state)) return false;
		boss.statebytes[15U] = byte_add(boss.statebytes[15U], 2);
		boss.sprite = pat_alice_still;
		presentation.sound(3U);
	}
	alice_pattern_finish(state);
	return true;
}

[[nodiscard]] bool alice_pattern_update(State &state, Presentation &presentation) noexcept
{
	switch(state.alice.boss_pattern) {
	case AlicePattern::random_balls_and_pellets:
		if(!alice_pattern_random_balls_and_pellets(state, presentation)) return false;
		break;
	case AlicePattern::ring_stacks:
		if(!alice_pattern_ring_stacks(state, presentation)) return false;
		break;
	case AlicePattern::spread_stacks:
		if(!alice_pattern_spread_stacks(state, presentation)) return false;
		break;
	case AlicePattern::lasers_and_random_balls:
		if(!alice_pattern_lasers_and_random_balls(state, presentation)) return false;
		break;
	case AlicePattern::mirrored_stacks:
		if(!alice_pattern_mirrored_stacks(state, presentation)) return false;
		break;
	case AlicePattern::rotating_ring:
		if(!alice_pattern_rotating_ring(state, presentation)) return false;
		break;
	}
	return true;
}

[[nodiscard]] bool alice_pattern_red_spreads(
	State &state,
	Presentation &presentation
) noexcept
{
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = bullet_spawn_type_no_decelerate;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
	bullet.angle = 0x40U;
	bullet.patnum = pat_bullet16_vector_red;
	if((state.boss.phase_frame % 32) == 0) {
		bullet.speed = 32U;
		bullet.spread = 13U;
		bullet.spread_angle_delta = 10U;
		if(!bullet_template_tune(state) || !bullets_add_regular(state)) return false;
		presentation.sound(3U);
	}
	return true;
}

[[nodiscard]] bool alice_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	state.shots.homing_target = boss.pos.cur;
	++boss.phase_frame;
	state.bullets.bullet_template.spawn_type = bullet_spawn_type_normal;
	state.bullets.bullet_template.origin = {boss.pos.cur.x, word_add(boss.pos.cur.y, -128)};
	state.bullets.gather_template.center = state.bullets.bullet_template.origin;
	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 1) {
			boss.hp = alice_hp_total;
			boss.phase_end_hp = alice_phase_2_end_hp;
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
			boss.statebytes[9U] = 0U;
		}
		break;
	case 2U:
		if(!alice_pattern_random_balls_and_pellets(state, presentation)) return false;
		(void)boss_hittest(state, presentation);
		if(boss.phase_frame == 128) {
			auto &puppets = state.alice.puppets;
			puppets[0].flag = puppet_alive;
			puppets[0].patnum = pat_puppet;
			puppets[0].radius = 4096;
			puppets[0].angle = 0x60U;
			puppets[0].hp = puppet_hp;
			puppets[0].pos.cur.x = homing_target_none;
			puppets[1].flag = puppet_alive;
			puppets[1].patnum = pat_puppet;
			puppets[1].radius = 4096;
			puppets[1].angle = 0x20U;
			puppets[1].hp = puppet_hp;
			puppets[1].pos.cur.x = homing_target_none;
			state.alice.puppet_patterns = {
				PuppetPattern::random_pellets, PuppetPattern::random_pellets
			};
		} else if(boss.phase_frame > 128) {
			puppets_spiral_in(state);
			if(state.alice.puppets[0].radius == 0) {
				++boss.phase;
				boss.mode = 0U;
				boss.phase_frame = 0;
				boss.phase_state = 0U;
				boss.statebytes[7U] = 0U;
				state.alice.barrier_fire_frames = 160U;
			}
		}
		break;
	case 3U:
	case 6U:
	case 9U:
	case 12U:
		if(boss.mode == 0U) {
			if(boss_flystep_random(state, static_cast<std::int16_t>(boss.phase_frame - 8)) ==
				FlyResult::unsupported) return false;
			if(boss.phase_frame >= 28) {
				boss.mode = 1U;
				boss.phase_frame = 0;
				++boss.phase_state;
				if(boss.phase_state >= 24U) goto phase_timed_out;
				const auto pattern = pc98::th0405::randring_next16_mod(state.random, 3U);
				if(!pattern || boss.statebytes[7U] >= 4U) return false;
				constexpr std::array<std::array<AlicePattern, 3U>, 4U> patterns{{
					{{AlicePattern::random_balls_and_pellets, AlicePattern::ring_stacks,
						AlicePattern::spread_stacks}},
					{{AlicePattern::ring_stacks, AlicePattern::spread_stacks,
						AlicePattern::mirrored_stacks}},
					{{AlicePattern::spread_stacks, AlicePattern::lasers_and_random_balls,
						AlicePattern::mirrored_stacks}},
					{{AlicePattern::lasers_and_random_balls, AlicePattern::mirrored_stacks,
						AlicePattern::rotating_ring}},
				}};
				state.alice.boss_pattern = patterns[boss.statebytes[7U]][*pattern];
			}
		} else if(!alice_pattern_update(state, presentation)) {
			return false;
		}
		if(!boss_hittest(state, presentation)) break;
		if(!boss_score_bonus(state, presentation, 7U) || !boss_items_drop(state, presentation)) {
			return false;
		}
	phase_timed_out:
		++boss.statebytes[7U];
		bullets_clear(state);
		boss_explode_small(state, presentation, 1U);
		++boss.phase;
		boss.phase_frame = 0;
		boss.phase_state = 0U;
		boss.mode = 0U;
		boss.hp = boss.phase_end_hp;
		if(boss.phase < 12U) boss.phase_end_hp = word_sub(boss.phase_end_hp, alice_hp_per_phase);
		else boss.phase_end_hp = 0;
		state.alice.barrier_frame = 0U;
		break;
	case 4U:
	case 7U:
	case 10U:
		state.alice.puppets[0].pos.prev = {2048, 2048};
		state.alice.puppets[1].pos.prev = {4096, 2048};
		state.alice.puppets[0].hp = state.alice.puppets[1].hp = puppet_hp;
		state.alice.puppet_patterns = {PuppetPattern::never, PuppetPattern::never};
		if(boss_flystep_towards(state, 3072, 1024)) goto flystep_next_phase;
		goto flystep_hittest;
	case 5U:
	case 8U:
	case 11U:
		state.alice.puppets[0].pos.prev = {2048, 2048};
		state.alice.puppets[1].pos.prev = {4096, 2048};
		state.alice.puppets[0].hp = state.alice.puppets[1].hp = puppet_hp;
		state.alice.puppet_patterns = {PuppetPattern::never, PuppetPattern::never};
		if(!alice_barrier_update(state, presentation)) return false;
		(void)boss_hittest(state, presentation);
		if(boss.phase_frame >= 600) {
			if(!boss_score_bonus(state, presentation, 5U)) return false;
			boss_explode_small(state, presentation, 1U);
			bullets_clear(state);
			++boss.phase;
			boss.phase_frame = 0;
			boss.statebytes[9U] = 0U;
			state.alice.puppets[0].pos.prev = {5120, 1536};
			state.alice.puppets[1].pos.prev = {1024, 1536};
			state.alice.puppets[0].phase_frame = state.alice.puppets[1].phase_frame = 0U;
			state.alice.puppet_patterns = {
				PuppetPattern::aimed_spread_stack, PuppetPattern::aimed_spread_stack
			};
		}
		break;
	case 13U:
		state.alice.puppets[0].pos.prev = {1024, 2048};
		state.alice.puppets[1].pos.prev = {5120, 2048};
		state.alice.puppets[0].hp = state.alice.puppets[1].hp = puppet_hp;
		state.alice.puppet_patterns = {PuppetPattern::global_spread, PuppetPattern::global_spread};
		if(!boss_flystep_towards(state, 3072, 1024)) goto flystep_hittest;
	flystep_next_phase:
		boss.phase_frame = 0;
		++boss.phase;
	flystep_hittest:
		(void)boss_hittest(state, presentation);
		break;
	case 14U:
		state.alice.puppets[0].pos.prev = {1024, 2048};
		state.alice.puppets[1].pos.prev = {5120, 2048};
		state.alice.puppet_patterns = {PuppetPattern::global_spread, PuppetPattern::global_spread};
		if(!alice_pattern_red_spreads(state, presentation)) return false;
		if(boss.phase_frame < 1000) {
			if(!boss_hittest(state, presentation)) break;
			boss.phase_state = 1U;
		}
		boss.phase_frame = 0;
		boss.phase = phase_explode_small;
		state.alice.puppets[0].flag = puppet_remove;
		state.alice.puppets[1].flag = puppet_remove;
		break;
	default:
		if(!boss_defeat_update(state, presentation)) return false;
		break;
	}
	presentation.boss_hp(boss.hp, alice_hp_total);
	if(boss.phase >= 3U && boss.phase < phase_explode_big &&
		!puppets_update(state, presentation)) return false;
	return true;
}
} // namespace

bool midboss_score_bonus(
	State &state, Presentation &presentation, std::uint16_t units
) noexcept
{
	return midboss_score_bonus_impl(state, presentation, units);
}

// Stage 4 keeps its second character in State::stage4 rather than borrowing
// the single-boss helpers' storage.  The source pair hittest returns an
// outcome (which character exhausted its phase), never a fallible-operation
// result; live combat must therefore keep running on a false outcome.
[[nodiscard]] static bool stage4_yuki_hittest(State &state, Presentation &presentation)
{
	auto &yuki = state.stage4.yuki;
	std::uint16_t damage{};
	if(!shots_hit(state, yuki.pos.cur, {384, 384}, true, damage)) return false;
	yuki.damage_this_frame = static_cast<std::uint8_t>(damage);
	if(damage != 0U) presentation.sound(4U);
	yuki.hp = word_sub(yuki.hp, static_cast<std::int16_t>(yuki.damage_this_frame));
	return yuki.hp <= yuki.phase_end_hp;
}

[[nodiscard]] static bool stage4_midboss_update(State &state, Presentation &presentation)
{
	auto &midboss = state.midboss;
	++midboss.phase_frame;
	state.bullets.bullet_template.origin = midboss.pos.cur;
	state.bullets.gather_template.center = midboss.pos.cur;
	if(midboss.phase == 0U) {
		if(midboss.phase_frame == 1) state.boss.statebytes[15U] = 1U;
		(void)pc98::th0405::update_motion(midboss.pos);
		std::uint16_t damage{};
		(void)midboss_hittest_damage(state, presentation, {384, 384}, 10U, damage);
		if(midboss.phase_frame >= 192) {
			midboss.phase = 1U; midboss.phase_frame = 0; midboss.angle = 0U;
			state.boss.statebytes[14U] = 1U; state.boss.statebytes[13U] = 0U;
			midboss.pos.velocity.x = 0;
		}
	} else if(midboss.phase == 1U) {
		midboss.pos.prev = midboss.pos.cur;
		auto &pattern = state.boss.statebytes[14U];
		const auto frame = midboss.phase_frame;
		if(pattern == 0U) {
			if(frame < 16) {
				// The warp's lead-in intentionally leaves its prior BSS center alone.
			} else if(frame < 48) {
				auto amplitude = static_cast<std::int16_t>(frame - 16);
				if(amplitude == 0) {
					state.boss.statebytes[15U] = 0U;
					presentation.sound(13U);
					state.stage4.midboss_warp_x = midboss.pos.cur.x;
				}
				if(amplitude > 20) amplitude = static_cast<std::int16_t>((amplitude - 20) / 2 + 20);
				midboss.pos.cur.x = word_add(
					state.stage4.midboss_warp_x,
					(state.stage_frame & 1U) != 0U ? static_cast<std::int16_t>(amplitude << 5) :
						static_cast<std::int16_t>(-(amplitude << 5))
				);
			} else if(frame == 48) {
				const auto x = pc98::th0405::randring_next16_mod(state.random, 4096U);
				const auto y = pc98::th0405::randring_next16_mod(state.random, 512U);
				if(!x || !y) return false;
				state.stage4.midboss_warp_x = word_add(std::bit_cast<std::int16_t>(*x), 1024);
				midboss.pos.cur.y = word_add(std::bit_cast<std::int16_t>(*y), 1024);
			} else {
				auto amplitude = static_cast<std::int16_t>(80 - frame);
				if(amplitude > 20) amplitude = static_cast<std::int16_t>((amplitude - 20) / 2 + 20);
				midboss.pos.cur.x = word_add(
					state.stage4.midboss_warp_x,
					(state.stage_frame & 1U) != 0U ? static_cast<std::int16_t>(amplitude << 5) :
						static_cast<std::int16_t>(-(amplitude << 5))
				);
				if(amplitude == 0) {
					state.boss.statebytes[15U] = 1U;
					midboss.phase_frame = 0;
					++state.boss.statebytes[13U];
					pattern = static_cast<std::uint8_t>((state.boss.statebytes[13U] % 3U) + 1U);
					if(state.boss.statebytes[13U] >= 16U) {
						midboss.phase = phase_explode_big;
						midboss.sprite = pat_enemy_kill;
						midboss.phase_frame = 0;
						if(!pc98::th0405::sparks_add_circle(
							pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
							midboss.pos.cur, 128, 48U
						)) return false;
						presentation.sound(12U);
					}
				}
			}
		} else {
			auto &bullet = state.bullets.bullet_template;
			if(frame == 16) {
				presentation.sound(8U);
				midboss.sprite = 212U;
				switch(pattern) {
				case 1U:
					bullet.spawn_type = bullet_spawn_type_cloud_forwards;
					bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed);
					bullet.spread = 7U;
					if(!bullet_template_tune(state)) return false;
					break;
				case 2U:
					bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
					bullet.spread = 16U;
					bullet.patnum = pat_bullet16_vector_red;
					if(!bullet_template_tune(state)) return false;
					break;
				case 3U:
					bullet.spawn_type = static_cast<std::uint8_t>(
						bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
					);
					bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_stack_aimed);
					bullet.angle = 0x20U;
					bullet.spread = 3U;
					bullet.spread_angle_delta = 8U;
					bullet.stack = 4U;
					bullet.stack_speed_delta = 4U;
					bullet.speed = 16U;
					bullet.patnum = pat_bullet16_ball_blue;
					if(!bullet_template_tune(state)) return false;
					break;
				default:
					return false;
				}
			}
			if(frame >= 48 && frame <= 80 && ((frame & (pattern == 1U ? 7 : 3)) == 0)) {
				if(pattern == 1U) {
					bullet.spawn_type = bullet_spawn_type_cloud_forwards; bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed); bullet.spread = 7U;
					if(!bullet_template_tune(state)) return false;
					for(std::uint8_t i{}; i < 4U; ++i) { auto x=pc98::th0405::randring_next16_mod(state.random,768U); auto y=pc98::th0405::randring_next16_mod(state.random,768U); if(!x||!y)return false; bullet.patnum=(i&1U)?pat_bullet16_vector_blue:0U; bullet.speed=static_cast<std::uint8_t>(8U+i*16U); bullet.origin={word_add(midboss.pos.cur.x,word_sub(std::bit_cast<std::int16_t>(*x),384)),word_add(midboss.pos.cur.y,word_sub(std::bit_cast<std::int16_t>(*y),512))}; if(!bullets_add_regular_fixedspeed(state))return false; }
				} else if(pattern == 2U) {
					bullet.group=static_cast<std::uint8_t>(BulletGroup::ring); bullet.patnum=pat_bullet16_vector_red; bullet.spread=16U; bullet.speed=(frame&7)?40U:56U; bullet.spawn_type=(frame&7)?bullet_spawn_type_normal:bullet_spawn_type_cloud_forwards; if(!bullet_template_tune(state)||!bullets_add_regular(state))return false; bullet.angle=byte_add(bullet.angle,4);
				} else {
					bullet.spawn_type=static_cast<std::uint8_t>(bullet_spawn_type_cloud_forwards|bullet_spawn_type_no_decelerate); bullet.group=static_cast<std::uint8_t>(BulletGroup::spread_stack_aimed); bullet.patnum=pat_bullet16_ball_blue; bullet.spread=3U; bullet.spread_angle_delta=8U; bullet.stack=4U; bullet.stack_speed_delta=4U; bullet.speed=16U; bullet.angle=0x20U; if(!bullet_template_tune(state)||!bullets_add_regular(state))return false; bullet.speed=byte_add(bullet.speed,12); bullet.angle=byte_add(bullet.angle,-8);
				}
				presentation.sound(pattern == 2U ? 3U : 15U);
			}
			if(frame >= 96) { midboss.sprite=208U; midboss.phase_frame=0; pattern=0U; }
		}
		if(midboss.phase == 1U && state.boss.statebytes[15U] != 0U) {
			(void)midboss_hittest(state, presentation);
		}
		if(midboss.phase == 1U && midboss.hp <= 0) {
			state.bullets.zap.start();
			if(!th05::core::midboss_score_bonus(state, presentation, 15U)) return false;
			items_add(state, presentation, midboss.pos.cur, ItemType::bomb);
			midboss.phase = phase_explode_big;
			midboss.sprite = pat_enemy_kill;
			midboss.phase_frame = 0;
			if(!pc98::th0405::sparks_add_circle(
				pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset}, midboss.pos.cur,
				128, 48U
			)) return false;
			presentation.sound(12U);
		}
	} else if(midboss.phase == phase_explode_big) {
		if(midboss.phase_frame == 1) {
			state.playfield_shake_anim_time=10;
			state.midboss_active=false;
		}
		if((midboss.phase_frame & 15) == 0) {
			++midboss.sprite;
			if(midboss.sprite >= pat_enemy_kill_last + 1U) midboss.phase = phase_none;
		}
	} else {
		state.midboss_active=false;
		state.midboss_callback_active=false;
		midboss.hp=0;
	}
	presentation.boss_hp(midboss.hp, 1100);
	state.shots.homing_target=midboss.pos.cur;
	return true;
}

enum class Stage4PatternResult : std::uint8_t {
	running,
	completed,
	unsupported,
};

[[nodiscard]] static FlyResult stage4_pair_flystep_one(
	State &state, BossState &actor, std::int16_t frame
) noexcept
{
	if(frame == 0) {
		if(actor.pos.cur.x < boss_fly_field_left) {
			const auto angle = pc98::th0405::randring_next16_mod(state.random, 96U);
			if(!angle) return FlyResult::unsupported;
			actor.angle = static_cast<std::uint8_t>(*angle - 0x30U);
		} else if(actor.pos.cur.x > boss_fly_field_right) {
			const auto angle = pc98::th0405::randring_next16_mod(state.random, 97U);
			if(!angle) return FlyResult::unsupported;
			actor.angle = byte_add(static_cast<std::uint8_t>(*angle), 0x30);
		} else {
			const auto angle = pc98::th0405::randring_next16(state.random);
			if(!angle) return FlyResult::unsupported;
			actor.angle = static_cast<std::uint8_t>(*angle);
		}
		if((actor.flystep_next_y_direction == 1 && actor.angle < 0x80U) ||
			(actor.flystep_next_y_direction == 2 && actor.angle >= 0x80U)) {
			actor.angle = static_cast<std::uint8_t>(-actor.angle);
		}
		actor.flystep_next_y_direction = 0;
	}
	if(frame < 0) return FlyResult::moving;

	const auto velocity = pc98::masterlib::vector2(
		actor.angle, word(100 - static_cast<std::int32_t>(frame) * 2)
	);
	actor.pos.velocity = {velocity.x, velocity.y};
	actor.pos.cur.x = word_add(actor.pos.cur.x, actor.pos.velocity.x);
	actor.pos.cur.y = word_add(actor.pos.cur.y, actor.pos.velocity.y);
	actor.sprite = actor.pos.velocity.x < 0 ? 182U : 181U;
	if(actor.pos.cur.y < 384) {
		actor.pos.cur.y = 384;
		actor.flystep_next_y_direction = 2;
	} else if(actor.pos.cur.y > 2048) {
		actor.pos.cur.y = 2048;
		actor.flystep_next_y_direction = 1;
	}
	if(actor.pos.cur.x < 512) actor.pos.cur.x = 512;
	else if(actor.pos.cur.x > 5632) actor.pos.cur.x = 5632;
	if(frame < 48) return FlyResult::moving;
	actor.sprite = 180U;
	return FlyResult::complete;
}

[[nodiscard]] static FlyResult stage4_pair_flystep(
	State &state, std::int16_t frame
) noexcept
{
	if(stage4_pair_flystep_one(state, state.boss, frame) == FlyResult::unsupported) {
		return FlyResult::unsupported;
	}
	return stage4_pair_flystep_one(state, state.stage4.yuki, frame);
}

[[nodiscard]] static Stage4PatternResult stage4_pair_mai_blue_cross_stacks(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if((boss.phase_frame % 6) == 0) {
		auto &bullet = state.bullets.bullet_template;
		const auto angle = pc98::th0405::randring_next16_and(state.random, 0x3FU);
		const auto speed = pc98::th0405::randring_next16_and(state.random, 0x1FU);
		if(!angle || !speed) return Stage4PatternResult::unsupported;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::stack_aimed);
		bullet.angle = byte_add(static_cast<std::uint8_t>(*angle), -0x20);
		bullet.patnum = pat_bullet16_cross_blue;
		bullet.origin = {boss.pos.cur.x, word_add(boss.pos.cur.y, -128)};
		bullet.stack = 2U;
		bullet.stack_speed_delta = 6U;
		bullet.speed = byte_add(static_cast<std::uint8_t>(*speed), 16);
		if(!bullet_template_tune(state) || !bullets_add_regular(state)) {
			return Stage4PatternResult::unsupported;
		}
		presentation.sound(3U);
	}
	return boss.phase_frame == 128 ? Stage4PatternResult::completed
		: Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_mai_turning_blue_spread(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 48) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_aimed);
		bullet.special_motion = static_cast<std::uint8_t>(
			BulletSpecialMotion::decelerate_then_turn_aimed
		);
		bullet.angle = 0U;
		bullet.patnum = pat_bullet16_vector_blue;
		bullet.origin = {boss.pos.cur.x, word_add(boss.pos.cur.y, -128)};
		bullet.spread = 16U;
		bullet.spread_angle_delta = 8U;
		bullet.speed = 60U;
		if(!bullet_template_tune(state) || !bullets_add_special_fixedspeed(state)) {
			return Stage4PatternResult::unsupported;
		}
		presentation.sound(3U);
	}
	return boss.phase_frame == 96 ? Stage4PatternResult::completed
		: Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_mai_rotating_pellets(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 48) boss.statebytes[15U] = 0xF0U;
	if((boss.phase_frame % 4) == 0) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		bullet.angle = boss.statebytes[15U];
		boss.statebytes[15U] = byte_add(boss.statebytes[15U], 5);
		bullet.patnum = 0U;
		bullet.origin = {boss.pos.cur.x, word_add(boss.pos.cur.y, -128)};
		bullet.spread = 5U;
		bullet.spread_angle_delta = 10U;
		bullet.speed = 50U;
		if(!bullet_template_tune(state) || !bullets_add_regular_fixedspeed(state)) {
			return Stage4PatternResult::unsupported;
		}
		presentation.sound(3U);
	}
	return boss.phase_frame == 160 ? Stage4PatternResult::completed
		: Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_mai_blue_ball_spread(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if((boss.phase_frame % 8) == 0) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		bullet.angle = 0x40U;
		bullet.patnum = pat_bullet16_small_ball_blue;
		bullet.origin = {boss.pos.cur.x, word_add(boss.pos.cur.y, -128)};
		bullet.spread = 8U;
		bullet.spread_angle_delta = 12U;
		bullet.speed = 64U;
		if(!bullet_template_tune(state) || !bullets_add_regular(state)) {
			return Stage4PatternResult::unsupported;
		}
		presentation.sound(3U);
	}
	return boss.phase_frame == 128 ? Stage4PatternResult::completed
		: Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_mai_manual_laser(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 48) {
		auto &laser = state.lasers.laser_template;
		laser.coords.origin = boss.pos.cur;
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, laser.coords.origin.y),
			word_sub(state.player_pos.cur.x, laser.coords.origin.x)
		);
		if(!angle) return Stage4PatternResult::unsupported;
		laser.coords.angle = *angle;
		laser.color = 8U;
		laser.coords.width = 8U;
		if(!laser_manual_fixed_spawn(state, 0U, presentation)) {
			return Stage4PatternResult::unsupported;
		}
	}
	if(boss.phase_frame == 80 && !laser_manual_grow(state, 0U, presentation)) {
		return Stage4PatternResult::unsupported;
	}
	if((boss.phase_frame % 4) == 0 &&
		((boss.phase_frame >= 80 && boss.phase_frame < 144) ||
			(boss.phase_frame % 8) == 0)) {
		auto &laser = state.lasers.pool[0U];
		const auto aimed = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, laser.coords.origin.y),
			word_sub(state.player_pos.cur.x, laser.coords.origin.x)
		);
		if(!aimed) return Stage4PatternResult::unsupported;
		const auto delta = static_cast<std::int8_t>(
			static_cast<std::uint8_t>(*aimed - laser.coords.angle)
		);
		if(delta > 0) laser.coords.angle = byte_add(laser.coords.angle, 1);
		else if(delta < 0) laser.coords.angle = byte_add(laser.coords.angle, -1);
	}
	if(boss.phase_frame == 160) {
		if(!laser_stop(state, 0U)) return Stage4PatternResult::unsupported;
		return Stage4PatternResult::completed;
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_mai_blue_ball_ring(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 48) {
		boss.statebytes[15U] = 0x20U;
		boss.statebytes[12U] = 0xFCU;
	}
	if((boss.phase_frame % 4) == 0) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = bullet_spawn_type_no_decelerate;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
		bullet.angle = boss.statebytes[15U];
		boss.statebytes[15U] = byte_add(
			boss.statebytes[15U], static_cast<std::int8_t>(boss.statebytes[12U])
		);
		bullet.patnum = pat_bullet16_small_ball_blue;
		bullet.origin = {boss.pos.cur.x, word_add(boss.pos.cur.y, -128)};
		bullet.spread = 4U;
		bullet.speed = 38U;
		if(!bullet_template_tune(state) || !bullets_add_regular(state)) {
			return Stage4PatternResult::unsupported;
		}
		if((boss.phase_frame % 16) == 0) {
			const auto random = pc98::th0405::randring_next16_and(state.random, 7U);
			if(!random) return Stage4PatternResult::unsupported;
			boss.statebytes[15U] = byte_add(
				boss.statebytes[15U], -static_cast<std::int16_t>(*random + 0x10U)
			);
			boss.statebytes[12U] = static_cast<std::uint8_t>(
				-static_cast<std::int8_t>(boss.statebytes[12U])
			);
		}
		presentation.sound(3U);
	}
	return boss.phase_frame == 256 ? Stage4PatternResult::completed
		: Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_yuki_exact_linear_spread(
	State &state
) noexcept
{
	auto &boss = state.boss;
	if((boss.phase_frame % 16) != 0) return Stage4PatternResult::running;
	auto &bullet = state.bullets.bullet_template;
	const auto angle = pc98::th0405::randring_next16_and(state.random, 0x3FU);
	const auto speed = pc98::th0405::randring_next16_and(state.random, 0x1FU);
	if(!angle || !speed) return Stage4PatternResult::unsupported;
	bullet.spawn_type = bullet_spawn_type_no_decelerate;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_aimed);
	bullet.angle = byte_add(static_cast<std::uint8_t>(*angle), -0x20);
	bullet.patnum = pat_bullet16_small_ball_red;
	bullet.origin = {state.stage4.yuki.pos.cur.x, word_add(state.stage4.yuki.pos.cur.y, -128)};
	bullet.spread = 7U;
	bullet.spread_angle_delta = 5U;
	bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::exact_linear);
	bullet.speed = byte_add(static_cast<std::uint8_t>(*speed), 16);
	if(!bullet_template_tune(state) || !bullets_add_special(state)) {
		return Stage4PatternResult::unsupported;
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_yuki_red_ring(State &state) noexcept
{
	auto &boss = state.boss;
	if((boss.phase_frame % 16) != 0) return Stage4PatternResult::running;
	auto &bullet = state.bullets.bullet_template;
	const auto angle = pc98::th0405::randring_next16(state.random);
	if(!angle) return Stage4PatternResult::unsupported;
	bullet.spawn_type = bullet_spawn_type_no_decelerate;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
	bullet.angle = static_cast<std::uint8_t>(*angle);
	bullet.patnum = pat_bullet16_small_ball_red;
	bullet.origin = {state.stage4.yuki.pos.cur.x, word_add(state.stage4.yuki.pos.cur.y, -128)};
	bullet.spread = 32U;
	bullet.speed = 40U;
	if(!bullet_template_tune(state) || !bullets_add_regular(state)) {
		return Stage4PatternResult::unsupported;
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_yuki_red_stack(State &state) noexcept
{
	auto &boss = state.boss;
	if((boss.phase_frame % 16) != 0) return Stage4PatternResult::running;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = bullet_spawn_type_no_decelerate;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::stack_aimed);
	bullet.angle = 0U;
	bullet.patnum = pat_bullet16_small_ball_red;
	bullet.origin = {state.stage4.yuki.pos.cur.x, word_add(state.stage4.yuki.pos.cur.y, -128)};
	bullet.stack = 8U;
	bullet.stack_speed_delta = 4U;
	bullet.speed = 32U;
	if(!bullet_template_tune(state) || !bullets_add_regular(state)) {
		return Stage4PatternResult::unsupported;
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_yuki_red_ball_spread(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if((boss.phase_frame % 8) != 0) return Stage4PatternResult::running;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = static_cast<std::uint8_t>(
		bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
	);
	bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
	bullet.angle = 0x40U;
	bullet.patnum = pat_bullet16_small_ball_red;
	bullet.origin = {state.stage4.yuki.pos.cur.x, word_add(state.stage4.yuki.pos.cur.y, -128)};
	bullet.spread = 8U;
	bullet.spread_angle_delta = 12U;
	bullet.speed = 64U;
	if(!bullet_template_tune(state) || !bullets_add_regular(state)) {
		return Stage4PatternResult::unsupported;
	}
	presentation.sound(3U);
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_yuki_red_big_stack(State &state) noexcept
{
	auto &boss = state.boss;
	if((boss.phase_frame % 8) != 0) return Stage4PatternResult::running;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = bullet_spawn_type_no_decelerate;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::stack_aimed);
	bullet.angle = 0U;
	bullet.patnum = pat_bullet16_ball_red;
	bullet.origin = {state.stage4.yuki.pos.cur.x, word_add(state.stage4.yuki.pos.cur.y, -128)};
	bullet.stack = 8U;
	bullet.stack_speed_delta = 6U;
	bullet.speed = 32U;
	if(!bullet_template_tune(state) || !bullets_add_regular(state)) {
		return Stage4PatternResult::unsupported;
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_yuki_random_pellets(State &state) noexcept
{
	auto &boss = state.boss;
	if((boss.phase_frame % 2) != 0) return Stage4PatternResult::running;
	auto &bullet = state.bullets.bullet_template;
	const auto angle = pc98::th0405::randring_next16(state.random);
	const auto speed = pc98::th0405::randring_next16_and(state.random, 0x1FU);
	if(!angle || !speed) return Stage4PatternResult::unsupported;
	bullet.spawn_type = bullet_spawn_type_no_decelerate;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
	bullet.angle = static_cast<std::uint8_t>(*angle);
	bullet.patnum = 0U;
	bullet.origin = {state.stage4.yuki.pos.cur.x, word_add(state.stage4.yuki.pos.cur.y, -128)};
	bullet.spread = 2U;
	bullet.spread_angle_delta = 2U;
	bullet.speed = byte_add(static_cast<std::uint8_t>(*speed), 24);
	if(!bullet_template_tune(state) || !bullets_add_regular(state)) {
		return Stage4PatternResult::unsupported;
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_yuki_rotating_red_ring(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 48) {
		boss.statebytes[14U] = 0U;
		boss.statebytes[13U] = 4U;
	}
	if((boss.phase_frame % 4) != 0) return Stage4PatternResult::running;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = bullet_spawn_type_no_decelerate;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
	bullet.angle = boss.statebytes[14U];
	boss.statebytes[14U] = byte_add(
		boss.statebytes[14U], static_cast<std::int8_t>(boss.statebytes[13U])
	);
	bullet.patnum = pat_bullet16_small_ball_red;
	bullet.origin = {state.stage4.yuki.pos.cur.x, word_add(state.stage4.yuki.pos.cur.y, -128)};
	bullet.spread = 4U;
	bullet.speed = 38U;
	if(!bullet_template_tune(state) || !bullets_add_regular(state)) {
		return Stage4PatternResult::unsupported;
	}
	if((boss.phase_frame % 16) == 0) {
		const auto random = pc98::th0405::randring_next16_and(state.random, 7U);
		if(!random) return Stage4PatternResult::unsupported;
		boss.statebytes[14U] = byte_add(
			boss.statebytes[14U], static_cast<std::int16_t>(*random + 0x10U)
		);
		boss.statebytes[13U] = static_cast<std::uint8_t>(
			-static_cast<std::int8_t>(boss.statebytes[13U])
		);
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_cheetos(
	State &state, bool yuki, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame != 48 && boss.phase_frame != 96) {
		return yuki ? Stage4PatternResult::running :
			(boss.phase_frame == 128 ? Stage4PatternResult::completed : Stage4PatternResult::running);
	}
	CheetoTemplate cheeto{};
	cheeto.origin = yuki ? state.stage4.yuki.pos.cur : boss.pos.cur;
	cheeto.color = yuki ? 11 : 9;
	cheeto.speed = 32U;
	const bool point_right = boss.pos.cur.x >= state.stage4.yuki.pos.cur.x;
	const auto side = static_cast<std::int16_t>(yuki ? (point_right ? 0x40 : -0x40) :
		(point_right ? -0x40 : 0x40));
	const auto angle = pc98::masterlib::iatan2_source(
		word_sub(state.player_pos.cur.y, cheeto.origin.y),
		word_sub(state.player_pos.cur.x, cheeto.origin.x)
	);
	if(!angle) return Stage4PatternResult::unsupported;
	cheeto.angle = byte_add(*angle, side);
	stage4_cheetos_add(state, cheeto);
	presentation.sound(15U);
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_mai_pattern(
	State &state, Presentation &presentation
) noexcept
{
	switch(state.stage4.pair_mai_pattern) {
	case Stage4PairMaiPattern::blue_cross_stacks:
		return stage4_pair_mai_blue_cross_stacks(state, presentation);
	case Stage4PairMaiPattern::turning_blue_spread:
		return stage4_pair_mai_turning_blue_spread(state, presentation);
	case Stage4PairMaiPattern::rotating_pellets:
		return stage4_pair_mai_rotating_pellets(state, presentation);
	case Stage4PairMaiPattern::blue_ball_spread:
		return stage4_pair_mai_blue_ball_spread(state, presentation);
	case Stage4PairMaiPattern::manual_laser:
		return stage4_pair_mai_manual_laser(state, presentation);
	case Stage4PairMaiPattern::blue_ball_ring:
		return stage4_pair_mai_blue_ball_ring(state, presentation);
	case Stage4PairMaiPattern::cheetos:
		return stage4_pair_cheetos(state, false, presentation);
	}
	return Stage4PatternResult::unsupported;
}

[[nodiscard]] static Stage4PatternResult stage4_pair_yuki_pattern(
	State &state, Presentation &presentation
) noexcept
{
	switch(state.stage4.pair_yuki_pattern) {
	case Stage4PairYukiPattern::exact_linear_spread:
		return stage4_pair_yuki_exact_linear_spread(state);
	case Stage4PairYukiPattern::red_ring:
		return stage4_pair_yuki_red_ring(state);
	case Stage4PairYukiPattern::red_stack:
		return stage4_pair_yuki_red_stack(state);
	case Stage4PairYukiPattern::red_ball_spread:
		return stage4_pair_yuki_red_ball_spread(state, presentation);
	case Stage4PairYukiPattern::red_big_stack:
		return stage4_pair_yuki_red_big_stack(state);
	case Stage4PairYukiPattern::random_pellets:
		return stage4_pair_yuki_random_pellets(state);
	case Stage4PairYukiPattern::rotating_red_ring:
		return stage4_pair_yuki_rotating_red_ring(state);
	case Stage4PairYukiPattern::cheetos:
		return stage4_pair_cheetos(state, true, presentation);
	}
	return Stage4PatternResult::unsupported;
}

void stage4_pair_select_patterns(State &state) noexcept
{
	const auto index = static_cast<std::uint8_t>(state.boss.phase_state & 3U);
	if(state.boss.phase_state < 10U) {
		constexpr std::array<Stage4PairMaiPattern, 4U> mai{
			Stage4PairMaiPattern::blue_cross_stacks,
			Stage4PairMaiPattern::rotating_pellets,
			Stage4PairMaiPattern::turning_blue_spread,
			Stage4PairMaiPattern::blue_ball_spread,
		};
		constexpr std::array<Stage4PairYukiPattern, 4U> yuki{
			Stage4PairYukiPattern::exact_linear_spread,
			Stage4PairYukiPattern::red_stack,
			Stage4PairYukiPattern::red_ring,
			Stage4PairYukiPattern::red_ball_spread,
		};
		state.stage4.pair_mai_pattern = mai[index];
		state.stage4.pair_yuki_pattern = yuki[index];
	} else if(state.boss.phase_state < 15U) {
		constexpr std::array<Stage4PairYukiPattern, 4U> yuki{
			Stage4PairYukiPattern::red_ring,
			Stage4PairYukiPattern::random_pellets,
			Stage4PairYukiPattern::red_big_stack,
			Stage4PairYukiPattern::exact_linear_spread,
		};
		state.stage4.pair_mai_pattern = Stage4PairMaiPattern::manual_laser;
		state.stage4.pair_yuki_pattern = yuki[index];
	} else {
		constexpr std::array<Stage4PairMaiPattern, 4U> mai{
			Stage4PairMaiPattern::cheetos,
			Stage4PairMaiPattern::rotating_pellets,
			Stage4PairMaiPattern::blue_ball_ring,
			Stage4PairMaiPattern::blue_ball_ring,
		};
		constexpr std::array<Stage4PairYukiPattern, 4U> yuki{
			Stage4PairYukiPattern::cheetos,
			Stage4PairYukiPattern::rotating_red_ring,
			Stage4PairYukiPattern::rotating_red_ring,
			Stage4PairYukiPattern::red_stack,
		};
		state.stage4.pair_mai_pattern = mai[index];
		state.stage4.pair_yuki_pattern = yuki[index];
	}
}

[[nodiscard]] static bool stage4_pair_step_patterns(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &yuki = state.stage4.yuki;
	if(boss.phase_frame < 48) {
		state.bullets.gather_template.center = boss.pos.cur;
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 24), 9U, 8U);
		boss.sprite = 183U;
		if(boss.phase_frame == 32) presentation.sound(8U);
	} else {
		const auto result = stage4_pair_mai_pattern(state, presentation);
		if(result == Stage4PatternResult::unsupported) return false;
		if(result == Stage4PatternResult::running) boss.sprite = 184U;
		else {
			boss.phase_frame = 0;
			boss.mode = 0U;
			boss.sprite = 180U;
			yuki.sprite = 180U;
		}
	}

	if(boss.phase_frame < 48) {
		state.bullets.gather_template.center = yuki.pos.cur;
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 24), 9U, 8U);
		yuki.sprite = 183U;
		return true;
	}
	const auto result = stage4_pair_yuki_pattern(state, presentation);
	if(result == Stage4PatternResult::unsupported) return false;
	if(result == Stage4PatternResult::running) yuki.sprite = 184U;
	return true;
}

void stage4_explode_small_at(
	State &state, Presentation &presentation, BossPoint center, std::uint8_t type
) noexcept
{
	auto &explosion = state.boss_explosions.small[0].alive ?
		state.boss_explosions.small[1] : state.boss_explosions.small[0];
	explosion_init(explosion, center);
	if(type == 3U) explosion.radius_delta = {208, 112};
	else if(type == 4U) explosion.radius_delta = {112, 208};
	presentation.sound(15U);
}

void stage4_explode_big_at(State &state, Presentation &presentation, BossPoint center) noexcept
{
	explosion_init(state.boss_explosions.big, center);
	state.boss_explosions.big.radius_delta = {192, 192};
	presentation.sound(12U);
}

[[nodiscard]] static bool stage4_pair_update(State &state, Presentation &presentation)
{
	auto &boss = state.boss;
	auto &yuki = state.stage4.yuki;
	++boss.phase_frame;
	state.shots.homing_target = boss.pos.cur.y > yuki.pos.cur.y ? boss.pos.cur : yuki.pos.cur;

	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 1) {
			boss.hp = 4500;
			boss.phase_end_hp = 0;
			yuki.hp = 4500;
			yuki.phase_end_hp = 0;
			state.bullets.gather_template = {boss.pos.cur, {}, 1024, 8, 9, 2};
		}
		if(!boss_hittest_invincible(state, presentation)) return false;
		(void)stage4_yuki_hittest(state, presentation);
		if(boss.phase_frame >= 128) {
			++boss.phase;
			boss.phase_frame = 0;
			presentation.sound(13U);
		}
		break;
	case 1U:
		if(!boss_hittest_invincible(state, presentation)) return false;
		(void)stage4_yuki_hittest(state, presentation);
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.phase_frame = 0;
			boss.mode = 1U;
			boss.phase_state = 0U;
			state.stage4.pair_mai_pattern = Stage4PairMaiPattern::blue_cross_stacks;
			state.stage4.pair_yuki_pattern = Stage4PairYukiPattern::exact_linear_spread;
		}
		break;
	case 2U: {
		if(boss.mode == 0U) {
			std::int16_t frame = static_cast<std::int16_t>(boss.phase_frame - 16);
			if(boss.phase_state == 9U || boss.phase_state == 14U) {
				frame = static_cast<std::int16_t>(boss.phase_frame - 64);
				if(boss.phase_frame == 16) {
					bullets_clear(state);
					presentation.sound(15U);
					stage4_explode_small_at(state, presentation, boss.pos.cur, 1U);
					stage4_explode_small_at(state, presentation, yuki.pos.cur, 2U);
				}
			}
			const auto flight = stage4_pair_flystep(state, frame);
			if(flight == FlyResult::unsupported) return false;
			if(flight == FlyResult::complete) {
				boss.phase_frame = 0;
				boss.mode = 1U;
				++boss.phase_state;
				if(boss.phase_state >= 36U) {
					state.stage4.pair_defeated_yuki = false;
					boss.phase_timed_out = true;
					boss.phase = phase_explode_small;
					boss.phase_frame = 0;
					break;
				}
				stage4_pair_select_patterns(state);
			}
		} else if(!stage4_pair_step_patterns(state, presentation)) {
			return false;
		}
		// The flight timeout above has already transferred to the defeat path.
		// In the source this is a jump that bypasses both hittest calls.
		if(boss.phase != 2U) break;

		if((word_add(boss.hp, yuki.hp) < 5500) && boss.phase_state < 9U) {
			boss.mode = 0U;
			boss.phase_frame = 0;
			boss.phase_state = 9U;
		} else if((word_add(boss.hp, yuki.hp) < 2250) && boss.phase_state < 14U) {
			boss.mode = 0U;
			boss.phase_frame = 0;
			boss.phase_state = 14U;
		}
		const auto mai_dead = boss_hittest(state, presentation);
		const auto yuki_dead = stage4_yuki_hittest(state, presentation);
		if(mai_dead || yuki_dead) {
			state.stage4.pair_defeated_yuki = yuki_dead;
			boss.phase_timed_out = false;
			boss.phase = phase_explode_small;
			boss.phase_frame = 0;
			if(state.lasers.pool[0U].flag != LaserFlag::free && !laser_stop(state, 0U)) {
				return false;
			}
		}
		break;
	}
	case phase_explode_small:
		if(boss.phase_frame == 16) {
			stage4_explode_small_at(
				state, presentation, state.stage4.pair_defeated_yuki ? yuki.pos.cur : boss.pos.cur, 3U
			);
		}
		if(boss.phase_frame == 32) {
			stage4_explode_big_at(
				state, presentation, state.stage4.pair_defeated_yuki ? yuki.pos.cur : boss.pos.cur
			);
			if(state.stage4.pair_defeated_yuki) yuki.sprite = pat_enemy_kill;
			else boss.sprite = pat_enemy_kill;
			boss.phase = phase_explode_big;
			boss.phase_frame = 0;
			if(!boss.phase_timed_out) state.bullets.zap.start();
			state.player_invincibility_time = 255U;
		}
		break;
	case phase_explode_big:
		if(boss.phase_frame < 12) {
			state.playfield_shake_x = (state.stage_frame & 1U) != 0U ? 4 : -4;
			state.playfield_shake_y = (state.stage_frame & 3U) <= 1U ? -4 : 4;
		}
		state.slowdown_factor = 2U;
		if((boss.phase_frame % 8) == 0) {
			if(state.stage4.pair_defeated_yuki) ++yuki.sprite;
			else ++boss.sprite;
			if(boss.phase_frame >= 64) {
				boss.phase = phase_none;
				boss.phase_frame = 0;
			}
		}
		break;
	case phase_none:
		if(boss.phase_frame == 1) {
			if(state.stage4.pair_defeated_yuki) {
				state.stage4.fight = Stage4Fight::mai_solo;
			} else {
				boss.pos.cur = yuki.pos.cur;
				state.stage4.fight = Stage4Fight::yuki_solo;
			}
			boss.phase = 0U;
			boss.phase_frame = 0;
			boss.hp = 7900;
			state.dialog_pending = true;
		}
		break;
	default:
		return false;
	}

	if(!stage4_cheetos_update(state)) return false;
	presentation.boss_hp(word_add(boss.hp, yuki.hp), 9000);
	return true;
}

[[nodiscard]] static Stage4PatternResult stage4_solo_flystep_random(
	State &state, std::int16_t frame
) noexcept
{
	return boss_flystep_random(state, frame) == FlyResult::unsupported ?
		Stage4PatternResult::unsupported : Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_yuki_ring(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 48) {
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return Stage4PatternResult::unsupported;
		boss.statebytes[15U] = static_cast<std::uint8_t>(*angle);
		boss.statebytes[14U] = 1U;
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = bullet_spawn_type_no_decelerate;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
		bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::exact_linear);
		bullet.patnum = pat_bullet16_small_ball_red;
		bullet.spread = byte_add(state.rank, 5);
		bullet.speed = 54U;
		boss.sprite = 208U;
		if(!bullet_template_tune(state)) return Stage4PatternResult::unsupported;
	} else if(boss.phase_frame > 48) {
		if((boss.phase_frame % 2) == 0) {
			auto &bullet = state.bullets.bullet_template;
			bullet.angle = boss.statebytes[15U];
			boss.statebytes[15U] = byte_add(
				boss.statebytes[15U], static_cast<std::int8_t>(boss.statebytes[14U])
			);
			if(!bullets_add_special(state)) return Stage4PatternResult::unsupported;
			if((boss.phase_frame % 16) == 0) {
				boss.statebytes[15U] = byte_add(
					boss.statebytes[15U],
					static_cast<std::int8_t>(boss.statebytes[14U]) * 12
				);
			}
			presentation.sound(3U);
		}
		if(boss.phase_frame == 160) {
			boss.statebytes[14U] = static_cast<std::uint8_t>(
				-static_cast<std::int8_t>(boss.statebytes[14U])
			);
		}
		if(boss.phase_frame == 256) {
			boss.phase_frame = 0;
			boss.mode = 0U;
			return Stage4PatternResult::completed;
		}
		if(boss.phase_state >= 2U && boss.phase_frame >= 64) {
			return stage4_solo_flystep_random(
				state, static_cast<std::int16_t>(boss.phase_frame % 64)
			);
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_yuki_random_spread(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 8) {
		auto &bullet = state.bullets.bullet_template;
		bullet.angle = 0x80U;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		bullet.patnum = pat_bullet16_ball_red;
		boss.sprite = 208U;
	} else if(boss.phase_frame > 8) {
		if((boss.phase_frame % 2) == 0) {
			const auto speed = pc98::th0405::randring_next16_and(state.random, 0x1FU);
			const auto spread = pc98::th0405::randring_next16_and(state.random, 7U);
			if(!speed || !spread) return Stage4PatternResult::unsupported;
			auto &bullet = state.bullets.bullet_template;
			bullet.speed = byte_add(static_cast<std::uint8_t>(*speed), 8);
			bullet.spread = static_cast<std::uint8_t>(*spread);
			bullet.spread_angle_delta = 6U;
			if(!bullet_template_tune(state) || !bullets_add_regular(state)) {
				return Stage4PatternResult::unsupported;
			}
			bullet.angle = byte_add(bullet.angle, 6);
			presentation.sound(3U);
		}
		if(boss.phase_frame == 256) {
			boss.phase_frame = 0;
			boss.mode = 0U;
			return Stage4PatternResult::completed;
		}
		if(boss.phase_state >= 2U && boss.phase_frame >= 64) {
			return stage4_solo_flystep_random(
				state, static_cast<std::int16_t>(boss.phase_frame % 64)
			);
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_yuki_pellet_ring(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 8) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
		bullet.patnum = 0U;
		bullet.spread = 24U;
		boss.sprite = 208U;
		if(!bullet_template_tune(state)) return Stage4PatternResult::unsupported;
	} else if(boss.phase_frame > 8) {
		if((boss.phase_frame % 16) == 0) {
			const auto angle = pc98::th0405::randring_next16(state.random);
			const auto speed = pc98::th0405::randring_next16_and(state.random, 0x1FU);
			if(!angle || !speed) return Stage4PatternResult::unsupported;
			auto &bullet = state.bullets.bullet_template;
			bullet.angle = static_cast<std::uint8_t>(*angle);
			bullet.speed = byte_add(static_cast<std::uint8_t>(*speed), 32);
			if(!bullets_add_regular(state)) return Stage4PatternResult::unsupported;
			presentation.sound(15U);
		}
		if(boss.phase_frame == 160) {
			boss.phase_frame = 0;
			boss.mode = 0U;
			return Stage4PatternResult::completed;
		}
		if(boss.phase_state >= 2U && boss.phase_frame >= 64) {
			return stage4_solo_flystep_random(
				state, static_cast<std::int16_t>((boss.phase_frame % 64) - 32)
			);
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_yuki_spread_balls(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &b4ball = state.stage4.custom.b4ball_template;
	if(boss.phase_frame < 32) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 16), 9U, 8U);
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_aimed);
		bullet.patnum = pat_bullet16_ball_red;
		bullet.spread = 5U;
		bullet.spread_angle_delta = 12U;
		bullet.angle = 0U;
		boss.sprite = 208U;
		b4ball.speed = 64U;
		if(!bullet_template_tune(state)) return Stage4PatternResult::unsupported;
	} else {
		if((boss.phase_frame % 32) == 0) {
			const auto speed = pc98::th0405::randring_next16_and(state.random, 0x1FU);
			if(!speed) return Stage4PatternResult::unsupported;
			auto &bullet = state.bullets.bullet_template;
			bullet.speed = byte_add(static_cast<std::uint8_t>(*speed), 16);
			if(!bullets_add_regular(state)) return Stage4PatternResult::unsupported;
			const auto angle = pc98::masterlib::iatan2_source(
				word_sub(state.player_pos.cur.y, b4ball.origin.y),
				word_sub(state.player_pos.cur.x, b4ball.origin.x)
			);
			if(!angle) return Stage4PatternResult::unsupported;
			b4ball.angle = *angle;
			stage4_b4balls_add(state);
			b4ball.angle = byte_add(b4ball.angle, -12);
			stage4_b4balls_add(state);
			b4ball.angle = byte_add(b4ball.angle, -12);
			stage4_b4balls_add(state);
			b4ball.angle = byte_add(b4ball.angle, 36);
			stage4_b4balls_add(state);
			b4ball.angle = byte_add(b4ball.angle, 12);
			stage4_b4balls_add(state);
			presentation.sound(15U);
		}
		if(boss.phase_frame == 160) {
			boss.phase_frame = 0;
			boss.mode = 0U;
			return Stage4PatternResult::completed;
		}
		if(boss.phase_state >= 2U && boss.phase_frame >= 64) {
			return stage4_solo_flystep_random(
				state, static_cast<std::int16_t>((boss.phase_frame % 64) - 32)
			);
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_yuki_ball_cross(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &b4ball = state.stage4.custom.b4ball_template;
	if(boss.phase_frame < 32) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 16), 9U, 8U);
		boss.sprite = 208U;
		b4ball.speed = 32U;
	} else {
		if(boss.phase_frame == 32) presentation.sound(15U);
		if((boss.phase_frame % 4) == 0) {
			stage4_b4balls_add(state);
			b4ball.angle = byte_add(b4ball.angle, 0x40);
			stage4_b4balls_add(state);
			b4ball.angle = byte_add(b4ball.angle, 0x40);
			stage4_b4balls_add(state);
			b4ball.angle = byte_add(b4ball.angle, 0x40);
			stage4_b4balls_add(state);
			b4ball.angle = byte_add(b4ball.angle, 0x40);
			b4ball.angle = byte_add(b4ball.angle, static_cast<std::int8_t>(boss.statebytes[13U]));
		}
		if(boss.phase_frame == 64) {
			boss.phase_frame = 0;
			boss.mode = 0U;
			boss.statebytes[13U] = static_cast<std::uint8_t>(
				-static_cast<std::int8_t>(boss.statebytes[13U])
			);
			return Stage4PatternResult::completed;
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_yuki_sweeping_stacks(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 24) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::stack);
		bullet.patnum = pat_bullet16_vector_red;
		bullet.stack = 8U;
		bullet.stack_speed_delta = 6U;
		bullet.angle = 0x10U;
		bullet.speed = 32U;
		boss.sprite = 208U;
		if(!bullet_template_tune(state)) return Stage4PatternResult::unsupported;
	} else if(boss.phase_frame >= 24) {
		if((boss.phase_frame % 2) == 0) {
			auto &bullet = state.bullets.bullet_template;
			bullet.angle = boss.statebytes[15U];
			if(!bullets_add_regular(state)) return Stage4PatternResult::unsupported;
			boss.statebytes[15U] = byte_add(
				boss.statebytes[15U], static_cast<std::int8_t>(boss.statebytes[14U])
			);
			presentation.sound(15U);
		}
		if(boss.phase_frame == 64) {
			boss.statebytes[14U] = static_cast<std::uint8_t>(
				-static_cast<std::int8_t>(boss.statebytes[14U])
			);
			boss.statebytes[15U] = static_cast<std::int8_t>(boss.statebytes[14U]) < 0x7F ?
				0x10U : 0x70U;
			boss.phase_frame = 0;
			boss.mode = 0U;
			return Stage4PatternResult::completed;
		}
		if(boss.phase_state > 2U) {
			return stage4_solo_flystep_random(
				state, static_cast<std::int16_t>((boss.phase_frame % 64) - 25)
			);
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_yuki_speedup_cloud(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 8) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = bullet_spawn_type_no_decelerate;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle);
		bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::speedup);
		state.bullets.special_turns_max = 1U;
		bullet.patnum = pat_bullet16_vector_red;
		bullet.spread = byte_add(static_cast<std::uint8_t>(state.rank * 2U), 4);
		bullet.speed = 32U;
		boss.sprite = 208U;
	} else if(boss.phase_frame > 8) {
		if((boss.phase_frame % 4) == 0 && !bullets_add_special(state)) {
			return Stage4PatternResult::unsupported;
		}
		if(boss.phase_frame >= 128) {
			const auto frame = boss.phase_frame < 320 ?
				static_cast<std::int16_t>((boss.phase_frame % 64) - 32) :
				static_cast<std::int16_t>(boss.phase_frame % 64);
			return stage4_solo_flystep_random(state, frame);
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_yuki_pattern(
	State &state, Presentation &presentation
) noexcept
{
	switch(state.stage4.solo_pattern) {
	case Stage4SoloPattern::yuki_ring:
		return stage4_yuki_ring(state, presentation);
	case Stage4SoloPattern::yuki_random_spread:
		return stage4_yuki_random_spread(state, presentation);
	case Stage4SoloPattern::yuki_pellet_ring:
		return stage4_yuki_pellet_ring(state, presentation);
	case Stage4SoloPattern::yuki_spread_balls:
		return stage4_yuki_spread_balls(state, presentation);
	case Stage4SoloPattern::yuki_ball_cross:
		return stage4_yuki_ball_cross(state, presentation);
	case Stage4SoloPattern::yuki_sweeping_stacks:
		return stage4_yuki_sweeping_stacks(state, presentation);
	case Stage4SoloPattern::yuki_speedup_cloud:
		return stage4_yuki_speedup_cloud(state);
	default:
		return Stage4PatternResult::unsupported;
	}
}

[[nodiscard]] static Stage4PatternResult stage4_mai_spread_pairs(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 32) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = bullet_spawn_type_no_decelerate;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		bullet.patnum = pat_bullet16_small_ball_blue;
		bullet.spread = 2U;
		bullet.angle = 0x40U;
		boss.sprite = 192U;
	} else if(boss.phase_frame > 32) {
		if((boss.phase_frame % 16) == 0) {
			auto &bullet = state.bullets.bullet_template;
			bullet.spread_angle_delta = 12U;
			bullet.speed = 48U;
			for(std::uint8_t i{}; i < 8U; ++i) {
				if(!bullets_add_regular(state)) return Stage4PatternResult::unsupported;
				bullet.spread_angle_delta = byte_add(bullet.spread_angle_delta, 8);
				bullet.speed = byte_add(bullet.speed, 6);
			}
			presentation.sound(3U);
		}
		if(boss.phase_frame == 160) {
			boss.phase_frame = 0;
			boss.mode = 0U;
			return Stage4PatternResult::completed;
		}
		if(boss.phase_state >= 2U && boss.phase_frame >= 64) {
			return stage4_solo_flystep_random(
				state, static_cast<std::int16_t>(boss.phase_frame % 64)
			);
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_mai_turning_stacks(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 32) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = bullet_spawn_type_no_decelerate;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::stack);
		state.bullets.special_turns_max = 1U;
		bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::decelerate_then_turn);
		bullet.patnum = pat_bullet16_vector_blue;
		bullet.stack = byte_add(state.rank, 4);
		bullet.stack_speed_delta = 16U;
		bullet.speed = 32U;
		boss.sprite = 192U;
		boss.statebytes[15U] = 0x60U;
	} else if(boss.phase_frame > 32) {
		if((boss.phase_frame % 16) == 0) {
			auto &bullet = state.bullets.bullet_template;
			if((boss.phase_frame % 32) == 0) {
				bullet.angle = boss.statebytes[15U];
				state.bullets.bullet_template_special_angle = 0xC0U;
			} else {
				bullet.angle = byte_add(0x80U, -static_cast<std::int16_t>(boss.statebytes[15U]));
				state.bullets.bullet_template_special_angle = 0x40U;
			}
			if(!bullets_add_special_fixedspeed(state)) return Stage4PatternResult::unsupported;
			boss.statebytes[15U] = byte_add(boss.statebytes[15U], 9);
			presentation.sound(3U);
		}
		if(boss.phase_frame == 192) {
			boss.phase_frame = 0;
			boss.mode = 0U;
			return Stage4PatternResult::completed;
		}
		if(boss.phase_state >= 2U && boss.phase_frame >= 64) {
			return stage4_solo_flystep_random(
				state, static_cast<std::int16_t>(boss.phase_frame % 64)
			);
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_mai_laser_bullet_pattern(
	State &state
) noexcept
{
	auto &boss = state.boss;
	const auto frame_mod = static_cast<std::int16_t>((boss.phase_frame - 32) % 256);
	auto &bullet = state.bullets.bullet_template;
	switch(state.stage4.mai_laser_pattern) {
	case Stage4MaiLaserPattern::spread:
		if(frame_mod == 0) {
			bullet.spawn_type = static_cast<std::uint8_t>(
				bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
			);
			bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
			bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::decelerate_then_turn);
			bullet.patnum = pat_bullet16_vector_blue;
			bullet.spread = 3U;
			bullet.spread_angle_delta = 12U;
			bullet.speed = 48U;
			bullet.angle = 0U;
			if(!bullet_template_tune(state)) return Stage4PatternResult::unsupported;
		}
		if(frame_mod <= 128 && (frame_mod % 4) == 0) {
			if(!bullets_add_regular(state)) return Stage4PatternResult::unsupported;
			bullet.angle = byte_add(bullet.angle, 6);
		}
		return Stage4PatternResult::running;
	case Stage4MaiLaserPattern::ring:
		if(frame_mod == 0) {
			bullet.spawn_type = static_cast<std::uint8_t>(
				bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
			);
			bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
			bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::decelerate_then_turn);
			bullet.patnum = pat_bullet16_vector_blue;
			bullet.spread = 24U;
			bullet.speed = 48U;
			bullet.angle = 0U;
			if(!bullet_template_tune(state)) return Stage4PatternResult::unsupported;
		}
		if(frame_mod <= 128 && (frame_mod % 16) == 0) {
			if(!bullets_add_regular(state)) return Stage4PatternResult::unsupported;
			bullet.angle = byte_add(bullet.angle, 1);
		}
		return Stage4PatternResult::running;
	case Stage4MaiLaserPattern::ball_triplet:
		if(frame_mod == 0) {
			auto &b4ball = state.stage4.custom.b4ball_template;
			b4ball.speed = 48U;
			b4ball.angle = 0x80U;
			b4ball.hp = 24;
			b4ball.revenge = true;
			b4ball.patnum_tiny_base = 212;
			bullet.spawn_type = static_cast<std::uint8_t>(
				bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
			);
			bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle);
			bullet.patnum = 0U;
			bullet.spread = 4U;
			bullet.speed = 32U;
			if(!bullet_template_tune(state)) return Stage4PatternResult::unsupported;
		}
		if(frame_mod <= 128 && (frame_mod % 16) == 0) {
			auto &b4ball = state.stage4.custom.b4ball_template;
			b4ball.angle = byte_add(b4ball.angle, 0x10);
			stage4_b4balls_add(state);
			b4ball.angle = byte_add(b4ball.angle, -0x10);
			stage4_b4balls_add(state);
			b4ball.angle = byte_add(b4ball.angle, -0x10);
			stage4_b4balls_add(state);
			b4ball.angle = byte_add(b4ball.angle, 4);
		}
		return Stage4PatternResult::running;
	}
	return Stage4PatternResult::unsupported;
}

[[nodiscard]] static Stage4PatternResult stage4_mai_laser_fan(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 32) {
		auto &laser = state.lasers.laser_template;
		laser.color = 8U;
		laser.coords.width = 8U;
		const auto base_angle = pc98::th0405::randring_next16(state.random);
		if(!base_angle) return Stage4PatternResult::unsupported;
		const auto count = static_cast<std::int16_t>(state.rank + 5U);
		if(count < 1 || count > 10) return Stage4PatternResult::unsupported;
		state.stage4.mai_laser_count = count;
		for(std::int16_t i{}; i < count; ++i) {
			laser.coords.angle = byte_add(
				static_cast<std::uint8_t>((static_cast<std::int32_t>(i) << 8) / count),
				static_cast<std::uint8_t>(*base_angle)
			);
			if(!laser_manual_fixed_spawn(state, static_cast<std::uint8_t>(i), presentation)) {
				return Stage4PatternResult::unsupported;
			}
		}
		state.stage4.mai_laser_angle_progress = 0;
		state.stage4.mai_laser_angle_speed = 1;
		state.stage4.mai_laser_pattern = Stage4MaiLaserPattern::spread;
		boss.statebytes[15U] = 1U;
		boss.statebytes[10U] = 0U;
	} else if(boss.phase_frame > 32) {
		const auto frame_mod = static_cast<std::int16_t>((boss.phase_frame - 32) % 256);
		for(std::int16_t i{}; i < state.stage4.mai_laser_count; ++i) {
			state.lasers.pool[static_cast<std::size_t>(i)].coords.origin = boss.pos.cur;
		}
		if(frame_mod == 160) {
			for(std::int16_t i{}; i < state.stage4.mai_laser_count; ++i) {
				if(!laser_manual_grow(state, static_cast<std::uint8_t>(i), presentation)) {
					return Stage4PatternResult::unsupported;
				}
			}
		} else if(frame_mod == 212) {
			for(std::int16_t i{}; i < state.stage4.mai_laser_count; ++i) {
				state.lasers.pool[static_cast<std::size_t>(i)].flag =
					LaserFlag::fixed_shrink_and_wait_to_grow;
			}
		} else if(frame_mod == 180) {
			boss.statebytes[15U] = static_cast<std::uint8_t>(
				-static_cast<std::int8_t>(boss.statebytes[15U])
			);
		}

		if(frame_mod <= 112) {
			state.stage4.mai_laser_angle_speed += static_cast<std::int16_t>(state.stage_frame & 1U);
			if(state.stage4.mai_laser_angle_speed > 0x80) state.stage4.mai_laser_angle_speed = 0x80;
		} else if(frame_mod <= 192) {
			state.stage4.mai_laser_angle_speed -= 2;
			if(state.stage4.mai_laser_angle_speed < 0) state.stage4.mai_laser_angle_speed = 0;
		} else {
			state.stage4.mai_laser_angle_speed += static_cast<std::int16_t>(state.stage_frame & 1U);
			if(frame_mod > 212) {
				const auto random = pc98::th0405::randring_next16_and(state.random, 0x1FU);
				if(!random) return Stage4PatternResult::unsupported;
				if(*random == 0U) boss.phase_frame = word_add(boss.phase_frame, 255 - frame_mod);
			}
		}

		if(frame_mod == 0) {
			switch(boss.statebytes[10U] % 3U) {
			case 0U: state.stage4.mai_laser_pattern = Stage4MaiLaserPattern::spread; break;
			case 1U: state.stage4.mai_laser_pattern = Stage4MaiLaserPattern::ring; break;
			default: state.stage4.mai_laser_pattern = Stage4MaiLaserPattern::ball_triplet; break;
			}
			++boss.statebytes[10U];
		}
		if(boss.phase_frame >= 288) {
			const auto pattern = stage4_mai_laser_bullet_pattern(state);
			if(pattern == Stage4PatternResult::unsupported) return pattern;
			if(boss.phase_frame >= 544 && frame_mod <= 96) {
				const auto flight = stage4_solo_flystep_random(state, frame_mod - 32);
				if(flight == Stage4PatternResult::unsupported) return flight;
			}
		}

		state.stage4.mai_laser_angle_progress += state.stage4.mai_laser_angle_speed;
		if(state.stage4.mai_laser_angle_progress >= 0x10) {
			const auto angle_delta = static_cast<std::int16_t>(
				(state.stage4.mai_laser_angle_progress / 0x10) *
				static_cast<std::int8_t>(boss.statebytes[15U])
			);
			state.stage4.mai_laser_angle_progress %= 0x10;
			for(std::int16_t i{}; i < state.stage4.mai_laser_count; ++i) {
				auto &laser = state.lasers.pool[static_cast<std::size_t>(i)];
				laser.coords.angle = byte_add(laser.coords.angle, angle_delta);
			}
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_mai_spread_balls(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &b4ball = state.stage4.custom.b4ball_template;
	if(boss.phase_frame == 32) {
		boss.sprite = 192U;
		b4ball.speed = 48U;
		b4ball.hp = 24;
		b4ball.revenge = true;
		b4ball.patnum_tiny_base = 212;
	} else if(boss.phase_frame > 32) {
		if((boss.phase_frame % 16) == 0) {
			auto &bullet = state.bullets.bullet_template;
			bullet.spawn_type = bullet_spawn_type_no_decelerate;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
			bullet.patnum = pat_bullet16_outlined_ball_blue;
			bullet.spread = 2U;
			bullet.angle = 0x40U;
			bullet.spread_angle_delta = 12U;
			bullet.speed = 48U;
			for(std::uint8_t i{}; i < 8U; ++i) {
				if(!bullets_add_regular(state)) return Stage4PatternResult::unsupported;
				bullet.spread_angle_delta = byte_add(bullet.spread_angle_delta, 8);
				bullet.speed = byte_add(bullet.speed, 6);
			}
			presentation.sound(3U);
			bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle);
			bullet.patnum = 0U;
			bullet.spread = 3U;
			bullet.speed = 32U;
			if((boss.phase_frame % 32) == 0) {
				const auto angle = pc98::masterlib::iatan2_source(
					word_sub(state.player_pos.cur.y, b4ball.origin.y),
					word_sub(state.player_pos.cur.x, b4ball.origin.x)
				);
				if(!angle) return Stage4PatternResult::unsupported;
				b4ball.angle = byte_add(*angle, 0x20);
				stage4_b4balls_add(state);
				b4ball.angle = byte_add(b4ball.angle, -0x20);
				stage4_b4balls_add(state);
				b4ball.angle = byte_add(b4ball.angle, -0x20);
				stage4_b4balls_add(state);
			}
		}
		if(boss.phase_frame == 224) {
			boss.phase_frame = 0;
			boss.mode = 0U;
			return Stage4PatternResult::completed;
		}
		if(boss.phase_state >= 2U && boss.phase_frame >= 64) {
			return stage4_solo_flystep_random(
				state, static_cast<std::int16_t>(boss.phase_frame % 64)
			);
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_mai_spread_stacks(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 32) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = bullet_spawn_type_no_decelerate;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_stack);
		state.bullets.special_turns_max = 1U;
		bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::decelerate_then_turn);
		bullet.patnum = pat_bullet16_vector_blue;
		bullet.spread = 2U;
		bullet.spread_angle_delta = 8U;
		bullet.stack = 2U;
		bullet.stack_speed_delta = 8U;
		bullet.speed = 48U;
		boss.sprite = 192U;
		state.bullets.bullet_template_special_angle = 0xB6U;
	} else if(boss.phase_frame > 32) {
		if((boss.phase_frame % 16) == 0) {
			auto &bullet = state.bullets.bullet_template;
			bullet.angle = 0x80U;
			if(!bullets_add_special_fixedspeed(state)) return Stage4PatternResult::unsupported;
			state.bullets.bullet_template_special_angle = static_cast<std::uint8_t>(
				-static_cast<std::int8_t>(state.bullets.bullet_template_special_angle)
			);
			bullet.angle = 0U;
			if(!bullets_add_special_fixedspeed(state)) return Stage4PatternResult::unsupported;
			state.bullets.bullet_template_special_angle = byte_add(
				static_cast<std::uint8_t>(-static_cast<std::int8_t>(state.bullets.bullet_template_special_angle)), 8
			);
			presentation.sound(3U);
		}
		if(boss.phase_frame == 192) {
			boss.phase_frame = 0;
			boss.mode = 0U;
			return Stage4PatternResult::completed;
		}
		if(boss.phase_state >= 2U && boss.phase_frame >= 64) {
			return stage4_solo_flystep_random(
				state, static_cast<std::int16_t>(boss.phase_frame % 64)
			);
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_mai_revenge_balls(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &b4ball = state.stage4.custom.b4ball_template;
	if(boss.phase_frame == 32) {
		boss.sprite = 192U;
		b4ball.speed = 28U;
		b4ball.hp = 6;
		b4ball.revenge = true;
		b4ball.angle = 0x80U;
		b4ball.patnum_tiny_base = 212;
	} else if(boss.phase_frame > 32) {
		if(boss.phase_frame <= 128) {
			auto &bullet = state.bullets.bullet_template;
			bullet.spawn_type = bullet_spawn_type_no_decelerate;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed);
			bullet.patnum = pat_bullet16_small_ball_blue;
			bullet.spread = 8U;
			bullet.speed = 24U;
			if((boss.phase_frame % 4) == 0) {
				b4ball.angle = boss.statebytes[10U];
				stage4_b4balls_add(state);
				boss.statebytes[10U] = byte_add(
					boss.statebytes[10U], static_cast<std::int8_t>(boss.statebytes[11U])
				);
				presentation.sound(3U);
			}
		} else {
			boss.statebytes[11U] = static_cast<std::uint8_t>(
				-static_cast<std::int8_t>(boss.statebytes[11U])
			);
			boss.statebytes[10U] = static_cast<std::int8_t>(boss.statebytes[11U]) < 0x7F ?
				0U : 0x80U;
			boss.phase_frame = 0;
			boss.mode = 0U;
			return Stage4PatternResult::completed;
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_mai_fixed_lasers(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 16) {
		boss.sprite = 192U;
		auto &laser = state.lasers.laser_template;
		const auto random = pc98::th0405::randring_next16_and(state.random, 0x0FU);
		if(!random) return Stage4PatternResult::unsupported;
		const auto angle_delta = static_cast<std::int16_t>(*random + 0x10U);
		laser.active_at_age = 32;
		laser.shrink_at_age = 90;
		laser.coords.angle = static_cast<std::uint8_t>(64 - angle_delta - (angle_delta / 2));
		for(std::uint8_t slot{}; slot < 4U; ++slot) {
			if(!laser_fixed_spawn(state, slot, presentation)) return Stage4PatternResult::unsupported;
			laser.coords.angle = byte_add(laser.coords.angle, angle_delta);
		}
		boss.statebytes[13U] = 0xF8U;
	} else if(boss.phase_frame > 32) {
		if(boss.phase_frame <= 128) {
			if((boss.phase_frame % 8) == 0) {
				auto &bullet = state.bullets.bullet_template;
				bullet.spawn_type = bullet_spawn_type_no_decelerate;
				bullet.group = static_cast<std::uint8_t>(BulletGroup::stack);
				bullet.patnum = pat_bullet16_vector_blue;
				bullet.stack = 8U;
				bullet.stack_speed_delta = 6U;
				bullet.speed = 24U;
				bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::bounce_left_right_top);
				state.bullets.special_turns_max = 1U;
				bullet.angle = boss.statebytes[13U];
				if(!bullets_add_special(state)) return Stage4PatternResult::unsupported;
				bullet.angle = byte_add(0x80U, -static_cast<std::int16_t>(boss.statebytes[13U]));
				if(!bullets_add_special(state)) return Stage4PatternResult::unsupported;
				boss.statebytes[13U] = byte_add(boss.statebytes[13U], 8);
				if(boss.statebytes[13U] > 0x30U) boss.statebytes[13U] = 0xF8U;
				presentation.sound(3U);
			}
			auto &bullet = state.bullets.bullet_template;
			bullet.spawn_type = bullet_spawn_type_no_decelerate;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed);
			bullet.patnum = pat_bullet16_small_ball_blue;
			bullet.spread = 6U;
			bullet.speed = 24U;
		} else {
			boss.phase_frame = 0;
			boss.mode = 0U;
			return Stage4PatternResult::completed;
		}
	}
	return Stage4PatternResult::running;
}

[[nodiscard]] static Stage4PatternResult stage4_mai_pattern(
	State &state, Presentation &presentation
) noexcept
{
	switch(state.stage4.solo_pattern) {
	case Stage4SoloPattern::mai_spread_pairs:
		return stage4_mai_spread_pairs(state, presentation);
	case Stage4SoloPattern::mai_turning_stacks:
		return stage4_mai_turning_stacks(state, presentation);
	case Stage4SoloPattern::mai_spread_balls:
		return stage4_mai_spread_balls(state, presentation);
	case Stage4SoloPattern::mai_spread_stacks:
		return stage4_mai_spread_stacks(state, presentation);
	case Stage4SoloPattern::mai_revenge_balls:
		return stage4_mai_revenge_balls(state, presentation);
	case Stage4SoloPattern::mai_fixed_lasers:
		return stage4_mai_fixed_lasers(state, presentation);
	default:
		return Stage4PatternResult::unsupported;
	}
}

[[nodiscard]] static bool stage4_solo_update(State &state, Presentation &presentation)
{
	auto &boss = state.boss;
	const bool yuki = state.stage4.fight == Stage4Fight::yuki_solo;
	const auto hp_total = static_cast<std::int16_t>(yuki ? 7900 : 7800);
	++boss.phase_frame;
	state.shots.homing_target = boss.pos.cur;
	state.bullets.bullet_template.origin = boss.pos.cur;
	state.bullets.gather_template.center = boss.pos.cur;
	state.stage4.custom.b4ball_template.origin = boss.pos.cur;
	state.lasers.laser_template.coords.origin = boss.pos.cur;

	if(!yuki) {
		switch(boss.phase) {
		case 0U:
			if(boss.phase_frame == 1) {
				boss.hp = 7800;
				boss.phase_end_hp = 5800;
				state.bullets.gather_template = {boss.pos.cur, {}, 1024, 8, 9, 2};
				boss.sprite = 180U;
				boss.sprite_left = 182U;
				boss.sprite_right = 181U;
				boss.sprite_stay = 180U;
				state.stage4.custom.b4ball_template.patnum_tiny_base = 212;
			}
			if(!boss_hittest_invincible(state, presentation)) return false;
			if(boss.phase_frame >= 64) {
				++boss.phase;
				boss.phase_frame = 0;
				presentation.sound(13U);
			}
			break;
		case 1U:
			if(!boss_hittest_invincible(state, presentation)) return false;
			if(boss.phase_frame >= 64) {
				boss.sprite = 204U;
				++boss.phase;
				boss.phase_frame = 0;
			}
			break;
		case 2U:
			if(!boss_hittest_invincible(state, presentation)) return false;
			if(boss_flystep_towards(state, 3072, 1536)) {
				boss.sprite = 188U;
				++boss.phase;
				boss.phase_frame = 0;
				boss.mode = 1U;
				boss.phase_state = 0U;
				boss_explode_small(state, presentation, 3U);
				state.stage4.solo_pattern = Stage4SoloPattern::mai_spread_pairs;
				boss.sprite_left = 190U;
				boss.sprite_right = 189U;
				boss.sprite_stay = 188U;
			}
			break;
		case 3U: {
			if(boss.mode == 0U) {
				const auto flight = boss_flystep_random(
					state, static_cast<std::int16_t>(boss.phase_frame - 32)
				);
				if(flight == FlyResult::unsupported) return false;
				if(flight == FlyResult::complete) {
					boss.phase_frame = 0;
					boss.mode = 1U;
					++boss.phase_state;
					if(boss.phase_state >= 36U) {
						if(!boss_phase_next(state, presentation, 1U, 2800)) return false;
						break;
					}
					state.stage4.solo_pattern = (boss.phase_state & 1U) != 0U ?
						Stage4SoloPattern::mai_turning_stacks : Stage4SoloPattern::mai_spread_pairs;
				}
			} else {
				if(stage4_mai_pattern(state, presentation) == Stage4PatternResult::unsupported) {
					return false;
				}
			}
			if(boss.phase != 3U) break;
			if(boss_hittest(state, presentation)) {
				if(!boss_score_bonus(state, presentation, 10U) ||
					!boss_phase_next(state, presentation, 1U, 2800)) return false;
			}
			break;
		}
		case 4U:
			(void)boss_hittest(state, presentation);
			if(boss_flystep_towards(state, 3072, 2048)) {
				++boss.phase;
				boss.phase_frame = 0;
			}
			break;
		case 5U:
			if(stage4_mai_laser_fan(state, presentation) == Stage4PatternResult::unsupported) {
				return false;
			}
			if(boss.phase_frame < 5000) {
				if(!boss_hittest(state, presentation)) break;
				if(!boss_score_bonus(state, presentation, 10U)) return false;
			}
			for(std::uint8_t slot{}; slot < 10U; ++slot) {
				if(!laser_stop(state, slot)) return false;
			}
			if(!boss_phase_next(state, presentation, 4U, 1200)) return false;
			break;
		case 6U:
			(void)boss_hittest(state, presentation);
			if(boss_flystep_towards(state, 3072, 1536)) {
				++boss.phase;
				boss.phase_frame = 0;
				boss.statebytes[10U] = 0x80U;
				boss.statebytes[11U] = 0xFCU;
			}
			break;
		case 7U: {
			if(boss.mode == 0U) {
				const auto flight = boss_flystep_random(
					state, static_cast<std::int16_t>(boss.phase_frame - 32)
				);
				if(flight == FlyResult::unsupported) return false;
				if(flight == FlyResult::complete) {
					boss.phase_frame = 0;
					boss.mode = 1U;
					++boss.phase_state;
					if(boss.phase_state >= 36U) {
						if(!boss_phase_next(state, presentation, 1U, 0)) return false;
						break;
					}
					state.stage4.solo_pattern = (boss.phase_state & 1U) != 0U ?
						Stage4SoloPattern::mai_spread_stacks : Stage4SoloPattern::mai_spread_balls;
				}
			} else {
				if(stage4_mai_pattern(state, presentation) == Stage4PatternResult::unsupported) {
					return false;
				}
			}
			if(boss.phase != 7U) break;
			if(boss_hittest(state, presentation)) {
				if(!boss_score_bonus(state, presentation, 10U) ||
					!boss_phase_next(state, presentation, 1U, 0)) return false;
			}
			break;
		}
		case 8U:
			(void)boss_hittest(state, presentation);
			if(boss_flystep_towards(state, 3072, 1536)) {
				++boss.phase;
				boss.phase_frame = 0;
				boss.statebytes[10U] = 0x80U;
				boss.statebytes[11U] = 0xFCU;
			}
			break;
		case 9U: {
			if(boss.mode == 0U) {
				const auto flight = boss_flystep_random(
					state, static_cast<std::int16_t>(boss.phase_frame - 4)
				);
				if(flight == FlyResult::unsupported) return false;
				if(flight == FlyResult::complete) {
					boss.phase_frame = 0;
					boss.mode = 1U;
					++boss.phase_state;
					if(boss.phase_state >= 20U) {
						boss.phase_timed_out = true;
						boss_explode_small(state, presentation, 3U);
						boss.phase_frame = 0;
						boss.phase = phase_explode_small;
						stage4_b4balls_reset(state);
						break;
					}
					state.stage4.solo_pattern = (boss.phase_state & 1U) != 0U ?
						Stage4SoloPattern::mai_fixed_lasers : Stage4SoloPattern::mai_revenge_balls;
				}
			} else {
				if(stage4_mai_pattern(state, presentation) == Stage4PatternResult::unsupported) {
					return false;
				}
			}
			if(boss.phase != 9U) break;
			if(boss_hittest(state, presentation)) {
				boss.phase_timed_out = false;
				boss_explode_small(state, presentation, 3U);
				boss.phase_frame = 0;
				boss.phase = phase_explode_small;
				stage4_b4balls_reset(state);
			}
			break;
		}
		default:
			return boss_defeat_update(state, presentation);
		}
	} else {
		switch(boss.phase) {
		case 0U:
			if(boss.phase_frame == 1) {
				boss.hp = 7900;
				boss.phase_end_hp = 4600;
				state.bullets.gather_template = {boss.pos.cur, {}, 1024, 8, 9, 2};
				boss.sprite = 196U;
				boss.sprite_left = 198U;
				boss.sprite_right = 197U;
				boss.sprite_stay = 196U;
				state.stage4.custom.b4ball_template.patnum_tiny_base = 216;
			}
			if(!boss_hittest_invincible(state, presentation)) return false;
			if(boss.phase_frame >= 64) {
				++boss.phase;
				boss.phase_frame = 0;
				presentation.sound(13U);
			}
			break;
		case 1U:
			if(!boss_hittest_invincible(state, presentation)) return false;
			if(boss.phase_frame >= 64) {
				boss.sprite = 204U;
				++boss.phase;
				boss.phase_frame = 0;
			}
			break;
		case 2U:
			if(!boss_hittest_invincible(state, presentation)) return false;
			if(boss_flystep_towards(state, 3072, 1536)) {
				boss.sprite = 204U;
				++boss.phase;
				boss.phase_frame = 0;
				boss.mode = 1U;
				boss.phase_state = 0U;
				state.stage4.yuki.pos.cur = {boss.pos.cur.x, word_add(boss.pos.cur.y, 256)};
				boss_explode_small(state, presentation, 3U);
				state.stage4.solo_pattern = Stage4SoloPattern::yuki_ring;
				boss.sprite_left = 206U;
				boss.sprite_right = 205U;
				boss.sprite_stay = 204U;
			}
			break;
		case 3U: {
			if(boss.mode == 0U) {
				if(boss_flystep_towards(state, state.stage4.yuki.pos.cur.x, state.stage4.yuki.pos.cur.y)) {
					state.stage4.yuki.pos.cur = {boss.pos.cur.x, 1536};
					boss.phase_frame = 0;
					++boss.mode;
					++boss.phase_state;
					if(boss.phase_state >= 12U) {
						if(!boss_phase_next(state, presentation, 1U, 3200)) return false;
						break;
					}
					state.stage4.solo_pattern = (boss.phase_state & 1U) != 0U ?
						Stage4SoloPattern::yuki_random_spread : Stage4SoloPattern::yuki_ring;
				}
			} else if(stage4_yuki_pattern(state, presentation) == Stage4PatternResult::unsupported) {
				return false;
			}
			if(boss.phase != 3U) break;
			if(boss_hittest(state, presentation)) {
				if(!boss_score_bonus(state, presentation, 10U) ||
					!boss_phase_next(state, presentation, 1U, 3200)) return false;
			}
			break;
		}
		case 4U:
			(void)boss_hittest(state, presentation);
			if(boss_flystep_towards(state, 3072, 1536)) {
				++boss.phase;
				boss.phase_frame = 0;
				state.stage4.solo_pattern = Stage4SoloPattern::yuki_pellet_ring;
				boss.mode = 1U;
			}
			break;
		case 5U: {
			if(boss.mode == 0U) {
				if(boss_flystep_towards(state, boss.pos.cur.x, 1536)) {
					boss.phase_frame = 0;
					++boss.mode;
					++boss.phase_state;
					if(boss.phase_state >= 24U) {
						if(!boss_phase_next(state, presentation, 2U, 1200)) return false;
						break;
					}
					state.stage4.solo_pattern = (boss.phase_state & 1U) != 0U ?
						Stage4SoloPattern::yuki_spread_balls : Stage4SoloPattern::yuki_pellet_ring;
				}
			} else if(stage4_yuki_pattern(state, presentation) == Stage4PatternResult::unsupported) {
				return false;
			}
			if(boss.phase != 5U) break;
			if(boss_hittest(state, presentation)) {
				if(!boss_score_bonus(state, presentation, 10U) ||
					!boss_phase_next(state, presentation, 2U, 1200)) return false;
			}
			break;
		}
		case 6U:
			(void)boss_hittest(state, presentation);
			if(boss_flystep_towards(state, 3072, 1536)) {
				++boss.phase;
				boss.phase_frame = 0;
			}
			break;
		case 7U:
			if(stage4_yuki_pattern(state, presentation) == Stage4PatternResult::unsupported) {
				return false;
			}
			if(boss.phase_frame < 2000) {
				if(!boss_hittest(state, presentation)) break;
				if(!boss_score_bonus(state, presentation, 10U)) return false;
			}
			if(!boss_phase_next(state, presentation, 4U, 0)) return false;
			break;
		case 8U:
			(void)boss_hittest(state, presentation);
			if(boss_flystep_towards(state, 3072, 1536)) {
				++boss.phase;
				boss.phase_frame = 0;
				state.stage4.solo_pattern = Stage4SoloPattern::yuki_ball_cross;
				boss.mode = 1U;
				boss.statebytes[14U] = 8U;
				boss.statebytes[15U] = 0x10U;
				boss.statebytes[13U] = 0xF8U;
			}
			break;
		case 9U:
			if(boss.mode == 0U) {
				if(boss.phase_frame >= 8) {
					boss.phase_frame = 0;
					++boss.mode;
					++boss.phase_state;
					if(boss.phase_state >= 36U) {
						boss.phase_timed_out = true;
						boss_explode_small(state, presentation, 3U);
						boss.phase_frame = 0;
						boss.phase = phase_explode_small;
						stage4_b4balls_reset(state);
						break;
					}
					state.stage4.solo_pattern = (boss.phase_state % 5U) == 0U ?
						Stage4SoloPattern::yuki_ball_cross : Stage4SoloPattern::yuki_sweeping_stacks;
				}
			} else if(stage4_yuki_pattern(state, presentation) == Stage4PatternResult::unsupported) {
				return false;
			}
			if(boss.phase != 9U) break;
			if(boss_hittest(state, presentation)) {
				boss.phase_timed_out = false;
				boss_explode_small(state, presentation, 3U);
				boss.phase_frame = 0;
				boss.phase = phase_explode_small;
				stage4_b4balls_reset(state);
			}
			break;
		default:
			return boss_defeat_update(state, presentation);
		}
	}

	if(!stage4_b4balls_update(state, presentation)) return false;
	presentation.boss_hp(boss.hp, hp_total);
	return true;
}

[[nodiscard]] static std::uint8_t stage5_rank_select(
	const State &state, std::uint8_t easy, std::uint8_t normal,
	std::uint8_t hard, std::uint8_t lunatic
) noexcept
{
	switch(state.rank) {
	case 0U: return easy;
	case 1U:
	case 4U: return normal;
	case 2U: return hard;
	case 3U: return lunatic;
	default: return easy;
	}
}

[[nodiscard]] static bool stage5_midboss_pattern_blue_spreads(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	if((midboss.phase_frame % 4) == 0) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.patnum = pat_bullet16_vector_blue;
		bullet.speed = 88U;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		bullet.spread = 13U;
		bullet.spread_angle_delta = 7U;
		if(!bullets_add_regular(state)) return false;
		presentation.sound(3U);
	}
	return true;
}

[[nodiscard]] static bool stage5_midboss_pattern_random_green_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	if((midboss.phase_frame % 4) == 0) {
		const auto x = pc98::th0405::randring_next16_mod(state.random, 1024U);
		const auto y = pc98::th0405::randring_next16_mod(state.random, 1024U);
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!x || !y || !angle) return false;
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = bullet_spawn_type_no_decelerate;
		bullet.patnum = pat_bullet16_directional_green;
		bullet.speed = 32U;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
		bullet.spread = 16U;
		bullet.origin.x = word_add(midboss.pos.cur.x, word_sub(std::bit_cast<std::int16_t>(*x), 512));
		bullet.origin.y = word_add(midboss.pos.cur.y, word_sub(std::bit_cast<std::int16_t>(*y), 512));
		bullet.angle = static_cast<std::uint8_t>(*angle);
		if(!bullets_add_regular(state)) return false;
		presentation.sound(3U);
	}
	return true;
}

[[nodiscard]] static bool stage5_midboss_pattern_aimed_pellet_stacks(
	State &state, Presentation &presentation
) noexcept
{
	if(state.midboss.phase_frame != 32) return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.spawn_type = static_cast<std::uint8_t>(
		bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
	);
	bullet.patnum = 0U;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_stack_aimed);
	bullet.angle = 0U;
	bullet.spread = 5U;
	bullet.spread_angle_delta = 10U;
	bullet.stack = 5U;
	bullet.stack_speed_delta = 6U;
	bullet.speed = 32U;
	if(!bullets_add_regular(state)) return false;
	bullet.group = static_cast<std::uint8_t>(BulletGroup::ring_stack_aimed);
	bullet.stack_speed_delta = 4U;
	bullet.spread = 32U;
	if(!bullets_add_regular(state)) return false;
	presentation.sound(15U);
	return true;
}

// The pattern callback's completion predicate is distinct from a refused
// concrete operation.  m5_updt.cpp returns to flight only after its frame
// predicate; a failed RNG or bullet-pool operation must propagate instead.
enum class Stage5MidbossPatternResult : std::uint8_t {
	running,
	completed,
	unsupported,
};

[[nodiscard]] static Stage5MidbossPatternResult stage5_midboss_pattern_update(
	State &state, Presentation &presentation
) noexcept
{
	switch(state.stage5.midboss_pattern) {
	case Stage5MidbossPattern::blue_spreads:
		if(!stage5_midboss_pattern_blue_spreads(state, presentation)) {
			return Stage5MidbossPatternResult::unsupported;
		}
		return (state.midboss.phase_frame < 96) ?
			Stage5MidbossPatternResult::running : Stage5MidbossPatternResult::completed;
	case Stage5MidbossPattern::random_green_rings:
		if(!stage5_midboss_pattern_random_green_rings(state, presentation)) {
			return Stage5MidbossPatternResult::unsupported;
		}
		return (state.midboss.phase_frame < 96) ?
			Stage5MidbossPatternResult::running : Stage5MidbossPatternResult::completed;
	case Stage5MidbossPattern::aimed_pellet_stacks:
		if(!stage5_midboss_pattern_aimed_pellet_stacks(state, presentation)) {
			return Stage5MidbossPatternResult::unsupported;
		}
		return (state.midboss.phase_frame < 64) ?
			Stage5MidbossPatternResult::running : Stage5MidbossPatternResult::completed;
	}
	return Stage5MidbossPatternResult::unsupported;
}

[[nodiscard]] static bool stage5_midboss_update(State &state, Presentation &presentation)
{
	auto &midboss = state.midboss;
	++midboss.phase_frame;
	state.bullets.bullet_template.origin = midboss.pos.cur;
	state.bullets.gather_template.center = midboss.pos.cur;
	if(midboss.phase == 0U) {
		(void)pc98::th0405::update_motion(midboss.pos);
		std::uint16_t damage{};
		(void)midboss_hittest_damage(state, presentation, {384, 384}, 10U, damage);
		if(midboss.phase_frame >= 192) {
			midboss.phase = 1U;
			midboss.phase_frame = 0;
			midboss.angle = 0U;
			state.boss.statebytes[13U] = 0U;
			state.boss.statebytes[14U] = 1U;
			state.boss.statebytes[15U] = 0U;
			state.stage5.midboss_pattern = Stage5MidbossPattern::blue_spreads;
			midboss.pos.velocity.y = 0;
		}
	} else if(midboss.phase == 1U) {
		midboss.pos.prev = midboss.pos.cur;
		if(state.boss.statebytes[14U] == 0U) {
			if(midboss.phase_frame == 1) {
				midboss.pos.velocity.x = (state.boss.statebytes[15U] == 0U ||
					state.boss.statebytes[15U] == 3U) ? -32 : 32;
				state.boss.statebytes[15U] = static_cast<std::uint8_t>(
					(state.boss.statebytes[15U] + 1U) & 3U
				);
			}
			(void)pc98::th0405::update_motion(midboss.pos);
			if(midboss.phase_frame >= 32) {
				midboss.phase_frame = 0;
				++state.boss.statebytes[13U];
				switch(state.boss.statebytes[13U] % 3U) {
				case 0U: state.stage5.midboss_pattern = Stage5MidbossPattern::blue_spreads; break;
				case 1U: state.stage5.midboss_pattern = Stage5MidbossPattern::random_green_rings; break;
				default: state.stage5.midboss_pattern = Stage5MidbossPattern::aimed_pellet_stacks; break;
				}
				state.boss.statebytes[14U] = 1U;
				if(state.boss.statebytes[13U] >= 16U) goto defeated;
			}
		} else {
			if(midboss.phase_frame < 32) {
				gather_add_only_3stack(state, static_cast<std::int16_t>(midboss.phase_frame - 16), 3U, 2U);
				if(midboss.phase_frame == 16) presentation.sound(8U);
				if(midboss.phase_frame == 20 || midboss.phase_frame == 24 || midboss.phase_frame == 28) {
					++midboss.sprite;
				}
				const auto angle = pc98::masterlib::iatan2_source(
					word_sub(state.player_pos.cur.y, midboss.pos.cur.y),
					word_sub(state.player_pos.cur.x, midboss.pos.cur.x)
				);
				if(!angle) return false;
				state.bullets.bullet_template.angle = *angle;
			} else {
				if(midboss.sprite < pat_midboss5_last && (midboss.phase_frame % 4) == 0) {
					++midboss.sprite;
				}
				const auto pattern_result = stage5_midboss_pattern_update(state, presentation);
				if(pattern_result == Stage5MidbossPatternResult::unsupported) return false;
				if(pattern_result == Stage5MidbossPatternResult::completed) {
					midboss.phase_frame = 0;
					state.boss.statebytes[14U] = 0U;
					midboss.sprite = pat_midboss5_first;
				}
			}
		}
		if(midboss.phase == 1U && !midboss_hittest(state, presentation)) {
			// A false predicate means the normal, still-alive source hittest path.
			if(midboss.hp > 0) goto midboss_done;
		}
		if(midboss.hp > 0) goto midboss_done;
		state.bullets.zap.start();
		if(!midboss_score_bonus_impl(state, presentation, 30U)) return false;
		items_add(state, presentation, midboss.pos.cur, ItemType::one_up);
	defeated:
		midboss.phase = phase_explode_big;
		midboss.sprite = pat_enemy_kill;
		midboss.phase_frame = 0;
		if(!pc98::th0405::sparks_add_circle(
			pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset}, midboss.pos.cur, 128, 48U
		)) return false;
		presentation.sound(12U);
	}
midboss_done:
	if(midboss.phase == phase_explode_big) {
		if(midboss.phase_frame == 1) {
			state.playfield_shake_anim_time = 10;
			state.midboss_active = false;
		}
		if((midboss.phase_frame % 16) == 0) {
			++midboss.sprite;
			if(midboss.sprite >= pat_enemy_kill_last + 1U) midboss.phase = phase_none;
		}
	} else if(midboss.phase != 0U && midboss.phase != 1U) {
		state.midboss_active = false;
		state.midboss_callback_active = false;
		midboss.hp = 0;
	}
	presentation.boss_hp(midboss.hp, 1550);
	state.shots.homing_target = midboss.pos.cur;
	return true;
}

void stage5_vector_at(
	BossPoint &destination, BossPoint center, std::int16_t distance, std::uint8_t angle
) noexcept
{
	const auto offset = pc98::masterlib::vector2(angle, distance);
	destination = {word_add(center.x, offset.x), word_add(center.y, offset.y)};
}

// b5.cpp's flystep result is a completion predicate.  Keep it distinct from
// a failed random-angle read, because both occur on the same first-frame path.
enum class Stage5FlystepResult : std::uint8_t {
	running,
	completed,
	unsupported,
};

[[nodiscard]] static Stage5FlystepResult stage5_yumeko_flystep_bounce(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 1) {
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return Stage5FlystepResult::unsupported;
		const auto velocity = pc98::masterlib::vector2(static_cast<std::uint8_t>(*angle), 32);
		boss.pos.velocity = {velocity.x, velocity.y};
		boss.sprite = pat_yumeko_fly;
	}
	boss.pos.cur.x = word_add(boss.pos.cur.x, boss.pos.velocity.x);
	boss.pos.cur.y = word_add(boss.pos.cur.y, boss.pos.velocity.y);
	if(boss.pos.cur.x <= 768 || boss.pos.cur.x >= 5376) {
		boss.pos.velocity.x = word(-static_cast<std::int32_t>(boss.pos.velocity.x));
	}
	if(boss.pos.cur.y <= 768 || boss.pos.cur.y >= 2048) {
		boss.pos.velocity.y = word(-static_cast<std::int32_t>(boss.pos.velocity.y));
	}
	if(boss.phase_frame < 64) return Stage5FlystepResult::running;
	boss.sprite = pat_yumeko_still;
	return Stage5FlystepResult::completed;
}

[[nodiscard]] static bool stage5_yumeko_phase2_sword_fan(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &sword = state.swords.sword_template;
	if(boss.phase_frame == 16) {
		boss.sprite = pat_yumeko_cast;
		sword.twirl_time = 48U;
		sword.speed = 80U;
		sword.angle = 0x70U;
		const auto mirror = pc98::th0405::randring_next16_and(state.random, 1U);
		if(!mirror) return false;
		boss.statebytes[15U] = static_cast<std::uint8_t>(*mirror);
	} else if(boss.phase_frame > 32 && (boss.phase_frame % 2) == 0) {
		if(boss.statebytes[15U] != 0U) sword.angle = byte_add(0x80U, -sword.angle);
		stage5_vector_at(sword.origin, boss.pos.cur, 768, sword.angle);
		swords_add(state);
		if(boss.statebytes[15U] != 0U) sword.angle = byte_add(0x80U, -sword.angle);
		sword.angle = byte_add(sword.angle, -6);
		if(sword.angle <= 0x0CU) {
			boss.phase_frame = 0;
			boss.mode = 0U;
		}
	}
	(void)presentation;
	return true;
}

[[nodiscard]] static bool stage5_yumeko_phase2_red_ball_ring(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame < 32) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 16), 7U, 6U);
		if(boss.phase_frame == 16) {
			boss.sprite = pat_yumeko_cast;
			bullet.spawn_type = static_cast<std::uint8_t>(
				bullet_spawn_type_cloud_backwards | bullet_spawn_type_no_decelerate
			);
			bullet.patnum = pat_bullet16_ball_red;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
			bullet.speed = 60U;
			bullet.spread = 16U;
			if(!bullet_template_tune(state)) return false;
			presentation.sound(8U);
		}
	} else {
		if((boss.phase_frame % 4) == 0) {
			const auto angle = pc98::th0405::randring_next16(state.random);
			const auto distance = pc98::th0405::randring_next16_mod(state.random, 512U);
			if(!angle || !distance) return false;
			bullet.angle = static_cast<std::uint8_t>(*angle);
			stage5_vector_at(
				bullet.origin, boss.pos.cur, std::bit_cast<std::int16_t>(*distance), bullet.angle
			);
			if(!bullets_add_regular(state)) return false;
			presentation.sound(3U);
		}
		if(boss.phase_frame == 80) {
			boss.phase_frame = 0;
			boss.mode = 0U;
		}
	}
	return true;
}

[[nodiscard]] static bool stage5_yumeko_phase4_arrowheads_and_sword(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	auto &sword = state.swords.sword_template;
	if(boss.phase_frame < 32) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 16), 7U, 6U);
		if(boss.phase_frame == 16) {
			boss.sprite = pat_yumeko_cast;
			bullet.spawn_type = static_cast<std::uint8_t>(
				bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
			);
			bullet.patnum = pat_bullet16_vector_red;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::single);
			bullet.speed = 96U;
			if(!bullet_template_tune(state)) return false;
			presentation.sound(8U);
			sword.twirl_time = 32U;
			sword.speed = 76U;
			boss.statebytes[13U] = 0U;
			boss.statebytes[12U] = 0x20U;
		}
	} else {
		const auto tick = static_cast<std::uint8_t>(boss.phase_frame % 64);
		if(boss.statebytes[12U] > tick && (tick & 1U) == 0U) {
			bullet.origin.x = word_add(bullet.origin.x, -512);
			bullet.origin.y = word_add(bullet.origin.y, -256);
			if(tick == 0U) {
				const auto left = pc98::masterlib::iatan2_source(
					word_sub(state.player_pos.cur.y, bullet.origin.y),
					word_sub(state.player_pos.cur.x, bullet.origin.x)
				);
				const auto right = pc98::masterlib::iatan2_source(
					word_sub(state.player_pos.cur.y, bullet.origin.y),
					word_sub(state.player_pos.cur.x, word_add(bullet.origin.x, 1024))
				);
				if(!left || !right) return false;
				boss.statebytes[15U] = *left;
				boss.statebytes[14U] = *right;
			}
			bullet.angle = boss.statebytes[15U];
			if(!bullets_add_regular(state)) return false;
			bullet.origin.x = word_add(bullet.origin.x, 1024);
			bullet.angle = boss.statebytes[14U];
			if(!bullets_add_regular(state)) return false;
			presentation.sound(3U);
		}
		if((boss.phase_frame % state.stage5.interval_phase4) == 0) {
			const auto offset = pc98::th0405::randring_next16_and(state.random, 0x1FU);
			const auto y = pc98::th0405::randring_next16_mod(state.random, 1536U);
			const auto x = pc98::th0405::randring_next16_mod(state.random, 5632U);
			if(!offset || !y || !x) return false;
			sword.origin.y = std::bit_cast<std::int16_t>(*y);
			sword.origin.x = word_add(std::bit_cast<std::int16_t>(*x), 256);
			const auto angle = pc98::masterlib::iatan2_source(
				word_sub(state.player_pos.cur.y, sword.origin.y),
				word_sub(state.player_pos.cur.x, sword.origin.x)
			);
			if(!angle) return false;
			sword.angle = byte_add(*angle, static_cast<std::int16_t>(*offset) - 15);
			swords_add(state);
		}
		if(word_sub(boss.hp, boss.phase_end_hp) < 600 && boss.statebytes[13U] == 0U) {
			boss.statebytes[13U] = 1U;
			state.stage5.interval_phase4 = stage5_rank_select(state, 16U, 8U, 4U, 4U);
			boss.statebytes[12U] = stage5_rank_select(state, 40U, 48U, 52U, 52U);
			boss_explode_small(state, presentation, 0U);
			bullets_clear(state);
		}
	}
	return true;
}

[[nodiscard]] static bool stage5_yumeko_phase5_exact_linear_pairs(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame < 16) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 1), 7U, 6U);
		if(boss.phase_frame == 4) {
			boss.sprite = pat_yumeko_cast;
			bullet.spawn_type = bullet_spawn_type_no_decelerate;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
			bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::exact_linear);
			bullet.speed = 56U;
			bullet.spread = 5U;
			bullet.spread_angle_delta = 2U;
			presentation.sound(8U);
			boss.statebytes[15U] = 0x60U;
			boss.statebytes[14U] = 0x40U;
		}
	} else {
		if((boss.phase_frame % 8) == 0) {
			bullet.patnum = pat_bullet16_ball_blue;
			stage5_vector_at(bullet.origin, boss.pos.cur, 768, boss.statebytes[15U]);
			bullet.angle = byte_add(boss.statebytes[15U], boss.statebytes[14U]);
			if(!bullets_add_special(state)) return false;
			stage5_vector_at(bullet.origin, boss.pos.cur, 768, byte_add(boss.statebytes[15U], 0x80));
			bullet.angle = byte_add(bullet.angle, 0x80);
			if(!bullets_add_special(state)) return false;

			bullet.patnum = 0U;
			boss.statebytes[15U] = static_cast<std::uint8_t>(
				-static_cast<std::int8_t>(boss.statebytes[15U])
			);
			stage5_vector_at(bullet.origin, boss.pos.cur, 512, boss.statebytes[15U]);
			bullet.angle = byte_add(boss.statebytes[15U], -static_cast<std::int8_t>(boss.statebytes[14U]));
			if(!bullets_add_special(state)) return false;
			stage5_vector_at(bullet.origin, boss.pos.cur, 512, byte_add(boss.statebytes[15U], 0x80));
			bullet.angle = byte_add(bullet.angle, 0x80);
			if(!bullets_add_special(state)) return false;

			boss.statebytes[15U] = static_cast<std::uint8_t>(
				-static_cast<std::int8_t>(boss.statebytes[15U])
			);
			boss.statebytes[15U] = byte_add(boss.statebytes[15U], -8);
			boss.statebytes[14U] = byte_add(boss.statebytes[14U], -6);
			presentation.sound(3U);
		}
		if(boss.phase_frame == 256) {
			boss.phase_frame = 0;
			boss.mode = 0U;
		}
	}
	return true;
}

[[nodiscard]] static bool stage5_yumeko_phase5_green_arrowheads(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame < 16) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 1), 7U, 6U);
		if(boss.phase_frame == 4) {
			boss.sprite = pat_yumeko_cast;
			bullet.spawn_type = bullet_spawn_type_no_decelerate;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_aimed);
			bullet.speed = 56U;
			bullet.spread_angle_delta = 20U;
			bullet.angle = 0U;
			bullet.patnum = pat_bullet16_directional_green;
			if(!bullet_template_tune(state)) return false;
			presentation.sound(8U);
			boss.statebytes[15U] = 0x80U;
		}
	} else {
		if((boss.phase_frame % 8) == 0) {
			stage5_vector_at(bullet.origin, boss.pos.cur, 768, boss.statebytes[15U]);
			bullet.spread = 5U;
			if(!bullets_add_regular(state)) return false;
			stage5_vector_at(bullet.origin, boss.pos.cur, 768, byte_add(0x80U, -boss.statebytes[15U]));
			bullet.spread = 4U;
			if(!bullets_add_regular(state)) return false;
			boss.statebytes[15U] = byte_add(boss.statebytes[15U], 8);
			presentation.sound(3U);
		}
		if(boss.phase_frame == 192) {
			boss.phase_frame = 0;
			boss.mode = 0U;
		}
	}
	return true;
}

[[nodiscard]] static bool stage5_yumeko_phase7_arrowheads_laser_swords(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	auto &laser = state.lasers.laser_template;
	auto &sword = state.swords.sword_template;
	if(boss.phase_frame < 32) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 16), 7U, 6U);
		if(boss.phase_frame == 16) {
			boss.sprite = pat_yumeko_cast;
			bullet.spawn_type = static_cast<std::uint8_t>(
				bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
			);
			bullet.patnum = pat_bullet16_vector_blue;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::single);
			bullet.speed = 96U;
			if(!bullet_template_tune(state)) return false;
			presentation.sound(8U);
			laser.age = 24;
			laser.shootout_speed = 100;
			laser.coords.width = 6U;
			laser.color = 8U;
			laser.active_at_age = 28;
			boss.statebytes[13U] = 0U;
			boss.statebytes[12U] = 0x20U;
			sword.twirl_time = 32U;
			sword.speed = 76U;
		}
	} else {
		const auto tick = static_cast<std::uint8_t>(boss.phase_frame % 64);
		if(boss.statebytes[12U] > tick && (tick & 1U) == 0U) {
			bullet.origin.x = word_add(bullet.origin.x, -512);
			bullet.origin.y = word_add(bullet.origin.y, -256);
			if(tick == 0U) {
				const auto left = pc98::masterlib::iatan2_source(
					word_sub(state.player_pos.cur.y, bullet.origin.y),
					word_sub(state.player_pos.cur.x, bullet.origin.x)
				);
				const auto right = pc98::masterlib::iatan2_source(
					word_sub(state.player_pos.cur.y, bullet.origin.y),
					word_sub(state.player_pos.cur.x, word_add(bullet.origin.x, 1024))
				);
				if(!left || !right) return false;
				boss.statebytes[15U] = *left;
				boss.statebytes[14U] = *right;
			}
			bullet.angle = boss.statebytes[15U];
			if(!bullets_add_regular(state)) return false;
			bullet.origin.x = word_add(bullet.origin.x, 1024);
			bullet.angle = boss.statebytes[14U];
			if(!bullets_add_regular(state)) return false;
			presentation.sound(3U);
		}
		if((boss.phase_frame % state.stage5.interval_phase7) == 0) {
			const auto laser_x = pc98::th0405::randring_next16_mod(state.random, 4096U);
			if(!laser_x) return false;
			laser.coords.origin.y = 512;
			laser.coords.origin.x = word_add(std::bit_cast<std::int16_t>(*laser_x), 1024);
			const auto laser_angle = pc98::masterlib::iatan2_source(
				word_sub(state.player_pos.cur.y, laser.coords.origin.y),
				word_sub(state.player_pos.cur.x, laser.coords.origin.x)
			);
			if(!laser_angle) return false;
			laser.coords.angle = *laser_angle;
			if(!lasers_shootout_add(state, presentation)) return false;
			for(std::uint8_t i{}; i < 2U; ++i) {
				const auto offset = pc98::th0405::randring_next16_and(state.random, 0x1FU);
				const auto y = pc98::th0405::randring_next16_mod(state.random, 1536U);
				const auto x = pc98::th0405::randring_next16_mod(state.random, 5632U);
				if(!offset || !y || !x) return false;
				sword.origin.y = std::bit_cast<std::int16_t>(*y);
				sword.origin.x = word_add(std::bit_cast<std::int16_t>(*x), 256);
				const auto sword_angle = pc98::masterlib::iatan2_source(
					word_sub(state.player_pos.cur.y, sword.origin.y),
					word_sub(state.player_pos.cur.x, sword.origin.x)
				);
				if(!sword_angle) return false;
				sword.angle = byte_add(*sword_angle, static_cast<std::int16_t>(*offset) - 15);
				swords_add(state);
			}
		}
		if(word_sub(boss.hp, boss.phase_end_hp) < 500 && boss.statebytes[13U] == 0U) {
			boss.statebytes[13U] = 1U;
			state.stage5.interval_phase7 = stage5_rank_select(state, 34U, 28U, 20U, 20U);
			boss.statebytes[12U] = stage5_rank_select(state, 40U, 48U, 52U, 48U);
			boss_explode_small(state, presentation, 0U);
			bullets_clear(state);
		}
	}
	return true;
}

[[nodiscard]] static bool stage5_yumeko_phase8_swords_and_spreads(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &secondary = state.stage4.yuki;
	auto &bullet = state.bullets.bullet_template;
	auto &sword = state.swords.sword_template;
	if(boss.phase_frame < 32) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 16), 7U, 6U);
		if(boss.phase_frame != 16) return true;
		boss.sprite = pat_yumeko_cast;
		bullet.spawn_type = bullet_spawn_type_no_decelerate;
		bullet.patnum = pat_bullet16_directional_blue;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		bullet.spread = 5U;
		bullet.spread_angle_delta = stage5_rank_select(state, 24U, 16U, 12U, 10U);
		bullet.speed = 128U;
		presentation.sound(8U);
		sword.twirl_time = 32U;
		sword.speed = 64U;
		const auto y = pc98::th0405::randring_next16_mod(state.random, 512U);
		if(!y) return false;
		secondary.pos.cur.y = std::bit_cast<std::int16_t>(*y);
		boss.statebytes[15U] = stage5_rank_select(state, 0x28U, 0x1EU, 0x18U, 0x10U);
		boss.statebytes[14U] = 0U;
		boss.statebytes[13U] = 0U;
		return true;
	}
	if(boss.statebytes[13U] == 0U) {
		if((boss.phase_frame % 8) != 0) return true;
		sword.angle = boss.statebytes[14U];
		sword.origin.x = sword.angle == 0U ? 256 : 5888;
		sword.origin.y = secondary.pos.cur.y;
		swords_add(state);
		secondary.pos.cur.y = word_add(
			secondary.pos.cur.y, static_cast<std::int16_t>(boss.statebytes[15U] << 4U)
		);
		boss.statebytes[14U] = byte_add(boss.statebytes[14U], 0x80);
		if(secondary.pos.cur.y >= 6016) {
			const auto y = pc98::th0405::randring_next16_mod(state.random, 512U);
			if(!y) return false;
			secondary.pos.cur.y = std::bit_cast<std::int16_t>(*y);
			++boss.statebytes[13U];
		}
		return true;
	}
	if((boss.phase_frame % 4) != 0) return true;
	if(bullet.speed > 96U) {
		bullet.speed = 8U;
		presentation.sound(15U);
		++boss.statebytes[13U];
		if(boss.statebytes[13U] > 5U) {
			boss.statebytes[13U] = 0U;
			return true;
		}
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, bullet.origin.y),
			word_sub(state.player_pos.cur.x, bullet.origin.x)
		);
		if(!angle) return false;
		bullet.angle = *angle;
	}
	bullet.speed = byte_add(bullet.speed, 8);
	return bullets_add_regular(state);
}

[[nodiscard]] static bool stage5_yumeko_phase10_turning_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame < 32) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 16), 7U, 6U);
		if(boss.phase_frame == 16) {
			boss.sprite = pat_yumeko_cast;
			bullet.spawn_type = bullet_spawn_type_no_decelerate;
			bullet.patnum = pat_bullet16_vector_blue;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
			bullet.spread = 18U;
			bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::decelerate_then_turn);
			bullet.speed = 38U;
			if(!bullet_template_tune(state)) return false;
			presentation.sound(8U);
			boss.statebytes[15U] = 0U;
			state.bullets.special_turns_max = 2U;
			boss.statebytes[14U] = 0U;
		}
	} else if((boss.phase_frame % 16) == 0) {
		bullet.angle = boss.statebytes[14U];
		if((boss.statebytes[15U] & 1U) != 0U) {
			state.bullets.bullet_template_special_angle = 0x40U;
			bullet.angle = byte_add(0x80U, -bullet.angle);
		} else {
			state.bullets.bullet_template_special_angle = 0xC0U;
		}
		if(!bullets_add_special_fixedspeed(state)) return false;
		++boss.statebytes[14U];
		++boss.statebytes[15U];
		presentation.sound(3U);
	}
	return true;
}

[[nodiscard]] static bool stage5_yumeko_pattern_update(
	State &state, Presentation &presentation
) noexcept
{
	switch(state.stage5.yumeko_pattern) {
	case Stage5YumekoPattern::phase2_sword_fan:
		return stage5_yumeko_phase2_sword_fan(state, presentation);
	case Stage5YumekoPattern::phase2_red_ball_ring:
		return stage5_yumeko_phase2_red_ball_ring(state, presentation);
	case Stage5YumekoPattern::phase4_arrowheads_and_sword:
		return stage5_yumeko_phase4_arrowheads_and_sword(state, presentation);
	case Stage5YumekoPattern::phase5_exact_linear_pairs:
		return stage5_yumeko_phase5_exact_linear_pairs(state, presentation);
	case Stage5YumekoPattern::phase5_green_arrowheads:
		return stage5_yumeko_phase5_green_arrowheads(state, presentation);
	case Stage5YumekoPattern::phase7_arrowheads_laser_swords:
		return stage5_yumeko_phase7_arrowheads_laser_swords(state, presentation);
	case Stage5YumekoPattern::phase8_swords_and_spreads:
		return stage5_yumeko_phase8_swords_and_spreads(state, presentation);
	case Stage5YumekoPattern::phase10_turning_rings:
		return stage5_yumeko_phase10_turning_rings(state, presentation);
	}
	return false;
}

[[nodiscard]] static bool stage5_yumeko_update(State &state, Presentation &presentation)
{
	auto &boss = state.boss;
	auto &secondary = state.stage4.yuki;
	++boss.phase_frame;
	state.shots.homing_target = boss.pos.cur;
	state.bullets.bullet_template.origin = boss.pos.cur;
	state.bullets.gather_template.center = boss.pos.cur;
	state.swords.sword_template.origin = boss.pos.cur;

	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 1) {
			boss.hp = 8300;
			boss.phase_end_hp = 7500;
			state.bullets.gather_template = {boss.pos.cur, {}, 1024, 8, 9, 2};
			boss.sprite = pat_yumeko_still;
			boss.pos.velocity.x = 64;
			boss.sprite_left = pat_yumeko_still;
			boss.sprite_right = pat_yumeko_still;
			boss.sprite_stay = pat_yumeko_still;
		}
		if(secondary.pos.cur.y >= -512) {
			secondary.pos.cur.y = word_add(secondary.pos.cur.y, -16);
			std::uint16_t damage{};
			if(shots_hit(state, secondary.pos.cur, {384, 384}, false, damage) && damage != 0U) {
				presentation.sound(10U);
			}
		}
		if(boss.phase_frame < 64) {
			(void)boss_hittest_invincible(state, presentation);
		} else {
			boss.sprite = pat_yumeko_fly;
			boss.pos.cur.x = word_add(boss.pos.cur.x, 32);
			if(boss.pos.cur.x >= 3072) {
				boss.sprite = pat_yumeko_still;
				++boss.phase;
				boss.phase_frame = 0;
				presentation.sound(13U);
			}
		}
		break;
	case 1U:
		(void)boss_hittest_invincible(state, presentation);
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.phase_frame = 0;
			boss.mode = 1U;
			boss.phase_state = 0U;
			state.stage5.yumeko_pattern = Stage5YumekoPattern::phase2_sword_fan;
		}
		break;
	case 2U:
	case 5U: {
		const auto phase = boss.phase;
		if(boss.mode == 0U) {
			const auto flystep_result = stage5_yumeko_flystep_bounce(state);
			if(flystep_result == Stage5FlystepResult::unsupported) return false;
			if(flystep_result == Stage5FlystepResult::completed) {
				boss.phase_frame = 0;
				++boss.mode;
				++boss.phase_state;
				if(boss.phase_state >= 20U) {
					if(!boss_phase_next(
						state, presentation, phase == 2U ? 0U : 3U,
						phase == 2U ? 5700 : 2700
					)) return false;
					break;
				}
				if(phase == 2U) {
					state.stage5.yumeko_pattern = (boss.phase_state & 1U) == 0U ?
						Stage5YumekoPattern::phase2_sword_fan : Stage5YumekoPattern::phase2_red_ball_ring;
				} else {
					state.stage5.yumeko_pattern = (boss.phase_state & 1U) == 0U ?
						Stage5YumekoPattern::phase5_exact_linear_pairs : Stage5YumekoPattern::phase5_green_arrowheads;
				}
			}
		} else if(!stage5_yumeko_pattern_update(state, presentation)) {
			return false;
		}
		if(boss.phase != phase) break;
		if(boss.mode != 0U && boss_hittest(state, presentation)) {
			if(!boss_score_bonus(state, presentation, 10U) ||
				!boss_phase_next(state, presentation, phase == 2U ? 0U : 3U,
					phase == 2U ? 5700 : 2700)) return false;
		}
		break;
	}
	case 3U:
	case 6U:
		(void)boss_hittest(state, presentation);
		if(boss_flystep_towards(state, 3072, 1024)) {
			++boss.phase;
			boss.phase_frame = 0;
			state.stage5.yumeko_pattern = boss.phase == 4U ?
				Stage5YumekoPattern::phase4_arrowheads_and_sword :
				Stage5YumekoPattern::phase7_arrowheads_laser_swords;
		}
		break;
	case 4U:
	case 7U: {
		const auto phase = boss.phase;
		if(!stage5_yumeko_pattern_update(state, presentation)) return false;
		if(boss.phase_frame < 2000) {
			if(!boss_hittest(state, presentation)) break;
			if(!boss_score_bonus(state, presentation, 10U)) return false;
		}
		if(!boss_phase_next(state, presentation, 1U, phase == 4U ? 4500 : 1200)) return false;
		if(phase == 4U) state.stage5.yumeko_pattern = Stage5YumekoPattern::phase5_exact_linear_pairs;
		boss.mode = 1U;
		break;
	}
	case 8U:
		if(!stage5_yumeko_phase8_swords_and_spreads(state, presentation)) return false;
		if(boss.phase_frame < 2000) {
			if(!boss_hittest(state, presentation)) break;
			if(!boss_score_bonus(state, presentation, 10U)) return false;
		}
		if(!boss_phase_next(state, presentation, 1U, 0)) return false;
		break;
	case 9U:
		(void)boss_hittest(state, presentation);
		if(boss_flystep_towards(state, 3072, 1536)) {
			++boss.phase;
			boss.phase_frame = 0;
			boss.phase_state = 0U;
		}
		break;
	case 10U:
		if(!stage5_yumeko_phase10_turning_rings(state, presentation)) return false;
		if(boss.phase_frame < 1200) {
			if(!boss_hittest(state, presentation)) break;
			boss.phase_state = 1U;
		}
		boss_explode_small(state, presentation, 4U);
		boss.phase_frame = 0;
		boss.phase = phase_explode_small;
		break;
	default:
		return boss_defeat_update(state, presentation);
	}

	swords_update(state, presentation);
	presentation.boss_hp(boss.hp, 8300);
	return true;
}

void stage1_fights_setup(State &state) noexcept
{
	state.boss_active = false;
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
	state.midboss_callback_active = true;
}
void stage1_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}
bool stage1_fights_update(State &state, Presentation &presentation) noexcept
{
	if (state.midboss_callback_active && !midboss1_update(state, presentation))
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
	state.boss_active = false;
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
	state.midboss_callback_active = true;
}
void stage2_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}
bool stage2_fights_update(State &state, Presentation &presentation) noexcept
{
	if (state.midboss_callback_active && !midboss2_update(state, presentation))
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

void stage3_fights_setup(State &state) noexcept
{
	state.boss_active = false;
	state.midboss.frames_until = 5750U;
	state.midboss.pos.cur = {3072, -512};
	state.midboss.pos.prev = state.midboss.pos.cur;
	state.midboss.pos.velocity = {0, 8};
	state.midboss.hp = midboss3_hp_total;
	state.midboss.sprite = pat_midboss3;
	state.boss.phase = 0U;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.phase_frame = 0U;
	state.boss.pos.velocity = {};
	state.boss.damage_this_frame = 0U;
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
	state.boss.phase_timed_out = true;
	state.boss.pos.cur = {3072, 1024};
	state.boss.pos.prev = state.boss.pos.cur;
	state.boss.sprite = pat_alice_still;
	state.boss.hitbox_radius = {384, 384};
	state.boss.sprite_left = pat_alice_left;
	state.boss.sprite_right = pat_alice_right;
	state.boss.sprite_stay = pat_alice_still;
}

void stage3_midboss_activate(State &state) noexcept
{
	if(state.midboss.frames_until != state.stage_frame) return;
	state.midboss.phase = 0U;
	state.midboss.phase_frame = 0U;
	state.midboss_active = true;
	state.midboss_callback_active = true;
}

void stage3_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}

bool stage3_fights_update(State &state, Presentation &presentation) noexcept
{
	if(state.midboss_callback_active && !midboss3_update(state, presentation)) return false;
	if(state.boss_active && !alice_update(state, presentation)) return false;
	return true;
}

void stage3_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	// m3_render clears a visible regular-phase hit flash.  Its big explosion
	// deliberately retains the final hit state, unlike the Stage 2 branch.
	if(state.midboss_active && state.midboss.phase < phase_explode_small &&
		state.midboss.pos.cur.y >= 0 && state.midboss.damage_this_frame != 0U)
	{
		state.midboss.damage_this_frame = 0U;
	}
	fights_render_tail_explosions(state);
	(void)presentation;
}

void stage4_fights_setup(State &state) noexcept
{
	state.boss_active = false;
	state.midboss.frames_until = 3900U;
	state.midboss.pos.cur = {3072, -512};
	state.midboss.pos.prev = state.midboss.pos.cur;
	state.midboss.pos.velocity = {0, 8};
	state.midboss.hp = 1100;
	state.midboss.sprite = 208U;
	state.boss.phase = 0U;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.phase_frame = 0;
	state.boss.pos.velocity = {};
	state.boss.damage_this_frame = 0U;
	state.boss.pos.cur = {1536, 1024};
	state.boss.pos.prev = state.boss.pos.cur;
	state.boss.sprite = 180U;
	state.boss.hitbox_radius = {384, 384};
	state.stage4.yuki.pos.cur = {4608, 1024};
	state.stage4.yuki.pos.prev = state.stage4.yuki.pos.cur;
	state.stage4.yuki.pos.velocity = {};
	state.stage4.yuki.sprite = 180U;
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
	state.boss.phase_timed_out = true;
	state.stage4.fight = Stage4Fight::pair;
}
void stage4_midboss_activate(State &state) noexcept
{
	if(state.midboss.frames_until != state.stage_frame) return;
	state.midboss.phase = 0U;
	state.midboss.phase_frame = 0;
	state.midboss_active = true;
	state.midboss_callback_active = true;
}
void stage4_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}
bool stage4_fights_update(State &state, Presentation &presentation) noexcept
{
	if(state.midboss_callback_active && !stage4_midboss_update(state, presentation)) return false;
	if(state.boss_active) {
		const auto updated = state.stage4.fight == Stage4Fight::pair
			? stage4_pair_update(state, presentation)
			: stage4_solo_update(state, presentation);
		if(!updated) return false;
	}
	return true;
}
void stage4_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	if(state.midboss_active && state.midboss.phase < phase_explode_big &&
		state.midboss.pos.cur.y >= 0 && state.midboss.damage_this_frame != 0U) {
		state.midboss.damage_this_frame = 0U;
	}
	if(state.stage4.fight == Stage4Fight::pair) {
		state.boss.damage_this_frame = 0U;
		state.stage4.yuki.damage_this_frame = 0U;
	}
	stage4_b4balls_render_tail(state);
	fights_render_tail_explosions(state);
	(void)presentation;
}

void stage5_fights_setup(State &state) noexcept
{
	state.boss_active = false;
	state.midboss.frames_until = 4800U;
	state.midboss.pos.cur = {3072, -512};
	state.midboss.pos.prev = state.midboss.pos.cur;
	state.midboss.pos.velocity = {0, 8};
	state.midboss.hp = 1550;
	state.midboss.sprite = pat_midboss5_first;

	state.boss.phase = 0U;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.phase_frame = 0;
	state.boss.damage_this_frame = 0U;
	state.boss.phase_timed_out = true;
	state.boss.pos.cur = {1536, 1024};
	state.boss.pos.prev = state.boss.pos.cur;
	state.boss.pos.velocity = {};
	state.boss.sprite = pat_yumeko_still;
	state.boss.hitbox_radius = {384, 384};
	state.stage4.yuki.pos.cur = {3072, 1024};
	state.stage4.yuki.pos.prev = state.stage4.yuki.pos.cur;
	state.stage4.yuki.pos.velocity = {};
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
	state.stage5.interval_phase4 = stage5_rank_select(state, 20U, 10U, 6U, 6U);
	state.stage5.interval_phase7 = stage5_rank_select(state, 48U, 32U, 24U, 24U);
}

void stage5_midboss_activate(State &state) noexcept
{
	if(state.midboss.frames_until != state.stage_frame) return;
	state.midboss.phase = 0U;
	state.midboss.phase_frame = 0;
	state.midboss_active = true;
	state.midboss_callback_active = true;
}

void stage5_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}

bool stage5_fights_update(State &state, Presentation &presentation) noexcept
{
	if(state.midboss_callback_active && !stage5_midboss_update(state, presentation)) return false;
	if(state.boss_active && !stage5_yumeko_update(state, presentation)) return false;
	return true;
}

void stage5_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	// m5.cpp uses midboss_put_generic() after the large top clip.  The generic
	// branch clears only a hit flash that reached a visible blit (y >= 16 px).
	if(state.midboss_active && state.midboss.phase < phase_explode_big &&
		state.midboss.pos.cur.y >= 256 && state.midboss.damage_this_frame != 0U) {
		state.midboss.damage_this_frame = 0U;
	}
	// b5_fg clears Yumeko's flash in its non-explosion branch regardless of
	// clipping, unlike the earlier stage boss renderers.
	if(state.boss_active && state.boss.phase != phase_explode_big) {
		state.boss.damage_this_frame = 0U;
	}
	swords_render_tail(state);
	fights_render_tail_explosions(state);
	(void)presentation;
}

enum class Stage6PatternResult : std::uint8_t {
	running,
	completed,
	unsupported,
};

[[nodiscard]] static std::uint8_t stage6_rank_select(
	const State &state, std::uint8_t easy, std::uint8_t normal,
	std::uint8_t hard, std::uint8_t lunatic
) noexcept
{
	return stage5_rank_select(state, easy, normal, hard, lunatic);
}

[[nodiscard]] static bool stage6_wing_random_x(State &state, std::int16_t &x) noexcept
{
	const auto random = pc98::th0405::randring_next16_mod(
		state.random, static_cast<std::uint16_t>(shinki_wing_width)
	);
	if(!random) return false;
	x = word_add(
		word_add(state.boss.pos.cur.x, static_cast<std::int16_t>(*random)),
		static_cast<std::int16_t>(-(shinki_wing_width / 2))
	);
	return true;
}

[[nodiscard]] static bool stage6_wing_random_y(State &state, std::int16_t &y) noexcept
{
	const auto random = pc98::th0405::randring_next16_mod(
		state.random, static_cast<std::uint16_t>((shinki_wing_height / 3) * 2)
	);
	if(!random) return false;
	y = word_add(
		word_sub(state.boss.pos.cur.y, static_cast<std::int16_t>(*random)),
		static_cast<std::int16_t>((shinki_wing_height - 1024) / 2)
	);
	return true;
}

[[nodiscard]] static bool stage6_player_angle(
	const State &state, BossPoint origin, std::uint8_t &angle
) noexcept
{
	const auto result = pc98::masterlib::iatan2_source(
		word_sub(state.player_pos.cur.y, origin.y), word_sub(state.player_pos.cur.x, origin.x)
	);
	if(!result) return false;
	angle = *result;
	return true;
}

[[nodiscard]] static Stage6PatternResult stage6_pattern_curved_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame == 32) {
		const auto angle = pc98::th0405::randring_next16(state.random);
		const auto direction = pc98::th0405::randring_next16_and(state.random, 1U);
		if(!angle || !direction) return Stage6PatternResult::unsupported;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.patnum = pat_bullet16_outlined_ball_blue;
		bullet.speed = 32U;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
		bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::exact_linear);
		bullet.angle = static_cast<std::uint8_t>(*angle);
		bullet.spread = 16U;
		boss.statebytes[15U] = *direction == 0U ? static_cast<std::uint8_t>(-2) : 2U;
		if(!bullet_template_tune(state)) return Stage6PatternResult::unsupported;
		presentation.sound(15U);
	}
	if((boss.phase_frame % 4) == 0) {
		if(!bullets_add_special(state)) return Stage6PatternResult::unsupported;
		bullet.speed = byte_add(bullet.speed, 8);
		bullet.angle = byte_add(bullet.angle, static_cast<std::int8_t>(boss.statebytes[15U]));
	}
	return boss.phase_frame == 60 ? Stage6PatternResult::completed : Stage6PatternResult::running;
}

[[nodiscard]] static Stage6PatternResult stage6_pattern_dualspeed_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame == 32) {
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return Stage6PatternResult::unsupported;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
		bullet.angle = static_cast<std::uint8_t>(*angle);
		bullet.spread = 16U;
		boss.statebytes[15U] = stage6_rank_select(state, 16U, 12U, 8U, 4U);
	}
	if((boss.phase_frame % boss.statebytes[15U]) == 0) {
		const auto distance = pc98::th0405::randring_next16_mod(state.random, 512U);
		if(!distance) return Stage6PatternResult::unsupported;
		const auto offset = pc98::masterlib::vector2(
			bullet.angle, static_cast<std::int16_t>(*distance)
		);
		bullet.origin = {
			word_add(boss.pos.cur.x, offset.x), word_add(boss.pos.cur.y, offset.y)
		};
		bullet.speed = 32U;
		if(!bullets_add_regular(state)) return Stage6PatternResult::unsupported;
		bullet.speed = 64U;
		bullet.angle = byte_add(bullet.angle, 8);
		if(!bullets_add_regular(state)) return Stage6PatternResult::unsupported;
		presentation.sound(3U);
	}
	return boss.phase_frame == 96 ? Stage6PatternResult::completed : Stage6PatternResult::running;
}

[[nodiscard]] static Stage6PatternResult stage6_pattern_random_directional_and_kunai(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame == 32) {
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.speed = 28U;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed);
		bullet.spread = 3U;
		if(!bullet_template_tune(state)) return Stage6PatternResult::unsupported;
		presentation.sound(15U);
	}
	if((boss.phase_frame & 1) == 0) {
		bullet.patnum = pat_bullet16_directional_blue;
		bullet.origin.x = word_sub(bullet.origin.x, 256);
		if(!bullets_add_regular(state)) return Stage6PatternResult::unsupported;
		bullet.patnum = pat_bullet16_vector_blue;
		bullet.origin.x = word_add(bullet.origin.x, 512);
		if(!bullets_add_regular(state)) return Stage6PatternResult::unsupported;
	}
	const auto flight = boss_flystep_random(state, static_cast<std::int16_t>(boss.phase_frame - 64));
	if(flight == FlyResult::unsupported) return Stage6PatternResult::unsupported;
	return flight == FlyResult::complete ? Stage6PatternResult::completed : Stage6PatternResult::running;
}

[[nodiscard]] static Stage6PatternResult stage6_pattern_dense_blue_stacks(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame == 32) {
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_stack_aimed);
		bullet.angle = 0U;
		bullet.spread = 2U;
		bullet.spread_angle_delta = 4U;
		bullet.stack = stage6_rank_select(state, 5U, 6U, 6U, 7U);
		bullet.stack_speed_delta = stage6_rank_select(state, 12U, 12U, 13U, 13U);
		bullet.speed = 32U;
	}
	if((boss.phase_frame % 4) == 0) {
		if(!bullets_add_regular(state)) return Stage6PatternResult::unsupported;
		presentation.sound(15U);
	}
	const auto flight = boss_flystep_random(state, static_cast<std::int16_t>(boss.phase_frame - 64));
	if(flight == FlyResult::unsupported) return Stage6PatternResult::unsupported;
	return flight == FlyResult::complete ? Stage6PatternResult::completed : Stage6PatternResult::running;
}

[[nodiscard]] static Stage6PatternResult stage6_phase_2_3_with_pattern(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame < 32) {
		gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 16), 7U, 6U);
		if(boss.phase_frame == 2) {
			auto &bullet = state.bullets.bullet_template;
			boss.sprite = pat_shinki_cast;
			bullet.spawn_type = static_cast<std::uint8_t>(
				bullet_spawn_type_cloud_backwards | bullet_spawn_type_no_decelerate
			);
			bullet.patnum = pat_bullet16_ball_red;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
			bullet.speed = 60U;
			bullet.spread = 16U;
			if(!bullet_template_tune(state)) return Stage6PatternResult::unsupported;
			presentation.sound(8U);
		}
		return Stage6PatternResult::running;
	}
	Stage6PatternResult result{};
	switch(state.stage6.phase_2_3_pattern) {
	case Stage6Phase23Pattern::curved_rings:
		result = stage6_pattern_curved_rings(state, presentation);
		break;
	case Stage6Phase23Pattern::dualspeed_rings:
		result = stage6_pattern_dualspeed_rings(state, presentation);
		break;
	case Stage6Phase23Pattern::random_directional_and_kunai:
		result = stage6_pattern_random_directional_and_kunai(state, presentation);
		break;
	case Stage6Phase23Pattern::dense_blue_stacks:
		result = stage6_pattern_dense_blue_stacks(state, presentation);
		break;
	default:
		return Stage6PatternResult::unsupported;
	}
	if(result == Stage6PatternResult::completed) {
		boss.sprite = pat_shinki_still;
		boss.phase_frame = 0;
		boss.mode = 0U;
	}
	return result;
}

[[nodiscard]] static Stage6PatternResult stage6_pattern_wing_preparation(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &laser = state.lasers.laser_template;
	if(boss.phase_frame == 16) {
		constexpr std::array<std::uint8_t, 6U> angles{0x50U, 0x48U, 0x40U, 0x40U, 0x38U, 0x30U};
		for(std::uint8_t slot{}; slot < angles.size(); ++slot) {
			laser.coords.angle = angles[slot];
			if(!laser_manual_fixed_spawn(state, slot, presentation)) return Stage6PatternResult::unsupported;
		}
		boss.sprite = pat_shinki_cast;
		boss.statebytes[15U] = 0U;
		boss.statebytes[14U] = 100U;
		presentation.sound(8U);
	}
	if(boss.phase_frame <= 16) return Stage6PatternResult::running;
	if(state.lasers.pool[2U].coords.angle < 0x80U) {
		if((state.stage_frame & 1U) != 0U) {
			for(std::uint8_t slot{}; slot < 3U; ++slot) {
				state.lasers.pool[slot].coords.angle = byte_add(state.lasers.pool[slot].coords.angle, 1);
			}
			for(std::uint8_t slot{3U}; slot < 6U; ++slot) {
				state.lasers.pool[slot].coords.angle = byte_add(state.lasers.pool[slot].coords.angle, -1);
			}
			state.palette_tone = boss.statebytes[14U];
			state.palette_changed = true;
			++boss.statebytes[14U];
		}
		return Stage6PatternResult::running;
	}
	if(boss.statebytes[15U] == 0U) {
		auto &bullet = state.bullets.bullet_template;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::single);
		if(!bullet_template_tune(state)) return Stage6PatternResult::unsupported;
		for(std::uint8_t i{}; i < 50U; ++i) {
			if(!stage6_wing_random_x(state, bullet.origin.x) || !stage6_wing_random_y(state, bullet.origin.y)) {
				return Stage6PatternResult::unsupported;
			}
			const auto angle = pc98::th0405::randring_next16_mod(state.random, 0x60U);
			const auto speed = pc98::th0405::randring_next16_and(state.random, 63U);
			if(!angle || !speed) return Stage6PatternResult::unsupported;
			bullet.angle = static_cast<std::uint8_t>(*angle + 0x10U);
			bullet.speed = static_cast<std::uint8_t>(*speed + 24U);
			if(!bullets_add_regular(state)) return Stage6PatternResult::unsupported;
		}
		boss.sprite = pat_shinki_wings_white;
		for(std::uint8_t slot{}; slot < 6U; ++slot) {
			if(!laser_manual_grow(state, slot, presentation)) return Stage6PatternResult::unsupported;
		}
		presentation.sound(15U);
		state.playfield_shake_anim_time = 8;
	} else {
		state.palette_tone = (state.stage_frame & 1U) != 0U ? 150U : 100U;
		state.palette_changed = true;
		if(boss.statebytes[15U] >= 8U) {
			state.palette_tone = 100U;
			for(std::uint8_t slot{}; slot < 6U; ++slot) {
				if(!laser_stop(state, slot)) return Stage6PatternResult::unsupported;
			}
			return Stage6PatternResult::completed;
		}
	}
	++boss.statebytes[15U];
	return Stage6PatternResult::running;
}

[[nodiscard]] static bool stage6_pattern_random_rain_and_spreads(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame <= 128) return true;
	if((boss.phase_frame % 8) == 0) {
		auto &b6ball = state.b6balls.b6ball_template;
		const auto x = pc98::th0405::randring_next16_mod(state.random, 6144U);
		const auto y = pc98::th0405::randring_next16_mod(state.random, 1024U);
		const auto speed = pc98::th0405::randring_next16_and(state.random, 31U);
		if(!x || !y || !speed) return false;
		b6ball.origin.x = static_cast<std::int16_t>(*x);
		b6ball.origin.y = static_cast<std::int16_t>(*y + 512U);
		b6ball.angle = 0x40U;
		b6ball.speed = static_cast<std::uint8_t>(*speed + 48U);
		b6ball.patnum_tiny = 200;
		b6balls_add(state);
		presentation.sound(3U);
	}
	if((boss.phase_frame % 24) == 0) {
		auto &bullet = state.bullets.bullet_template;
		if(!stage6_wing_random_x(state, bullet.origin.x)) return false;
		const auto y = pc98::th0405::randring_next16_mod(state.random, 1024U);
		if(!y) return false;
		bullet.origin.y = word_sub(boss.pos.cur.y, static_cast<std::int16_t>(*y));
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_aimed);
		bullet.spread = 5U;
		bullet.spread_angle_delta = stage6_rank_select(state, 16U, 12U, 10U, 8U);
		bullet.angle = 0U;
		bullet.speed = 48U;
		if(!bullet_template_tune(state) || !bullets_add_regular(state)) return false;
	}
	return true;
}

[[nodiscard]] static bool stage6_pattern_cheetos_within_spread_walls(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame < 128) return true;
	if(boss.phase_frame == 128) {
		boss.statebytes[14U] = stage6_rank_select(state, 128U, 48U, 32U, 24U);
		boss.statebytes[15U] = stage6_rank_select(state, 32U, 40U, 48U, 56U);
	}
	const auto frame_in_cycle = static_cast<std::uint16_t>(
		boss.phase_frame % static_cast<std::int16_t>(boss.statebytes[14U])
	);
	if((frame_in_cycle & 7U) != 0U) return true;
	auto &bullet = state.bullets.bullet_template;
	const auto random_speed = pc98::th0405::randring_next16_and(state.random, 31U);
	if(!random_speed) return false;
	bullet.patnum = pat_bullet16_outlined_ball_blue;
	bullet.spawn_type = static_cast<std::uint8_t>(
		bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
	);
	bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
	bullet.speed = static_cast<std::uint8_t>(*random_speed + 48U);
	bullet.spread = 6U;
	bullet.spread_angle_delta = 8U;
	bullet.angle = 0x68U;
	if(!bullets_add_regular(state)) return false;
	bullet.angle = 0x18U;
	if(!bullets_add_regular(state)) return false;
	if(frame_in_cycle != 0U) return true;
	const auto direction = pc98::th0405::randring_next16_mod(state.random, 3U);
	if(!direction) return false;
	CheetoTemplate cheeto{};
	cheeto.color = 11U;
	cheeto.speed = 64U;
	cheeto.angle = static_cast<std::uint8_t>(*direction * 0x40U);
	cheeto.origin = state.b6balls.b6ball_template.origin;
	stage6_cheetos_add(state, cheeto);
	presentation.sound(3U);
	return true;
}

[[nodiscard]] static Stage6PatternResult stage6_pattern_wings_to_purple(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame < 160) return Stage6PatternResult::running;
	if(boss.phase_frame < 192) {
		boss.pos.cur.y = word_add(boss.pos.cur.y, (boss.phase_frame & 1) != 0 ? 32 : -32);
		return Stage6PatternResult::running;
	}
	if(boss.phase_frame == 192) {
		auto &b6ball = state.b6balls.b6ball_template;
		b6ball.patnum_tiny = 201;
		for(std::uint8_t i{}; i < 16U; ++i) {
			if(!stage6_wing_random_x(state, b6ball.origin.x) || !stage6_wing_random_y(state, b6ball.origin.y)) {
				return Stage6PatternResult::unsupported;
			}
			const auto angle = pc98::th0405::randring_next16_mod(state.random, 0x40U);
			const auto speed = pc98::th0405::randring_next16_and(state.random, 63U);
			if(!angle || !speed) return Stage6PatternResult::unsupported;
			b6ball.angle = static_cast<std::uint8_t>(*angle + 0x20U);
			b6ball.speed = static_cast<std::uint8_t>(*speed + 32U);
			b6balls_add(state);
		}
		boss.sprite = pat_shinki_wings_purple;
		presentation.sound(15U);
		state.playfield_shake_anim_time = 8;
	}
	return boss.phase_frame == 200 ? Stage6PatternResult::completed : Stage6PatternResult::running;
}

[[nodiscard]] static bool stage6_pattern_aimed_b6balls_and_symmetric_spreads(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame <= 128) return true;
	if(boss.phase_frame == 129) {
		boss.statebytes[15U] = stage6_rank_select(state, 96U, 32U, 28U, 24U);
	}
	if((boss.phase_frame % boss.statebytes[15U]) == 0) {
		auto &b6ball = state.b6balls.b6ball_template;
		if(!stage6_player_angle(state, b6ball.origin, b6ball.angle)) return false;
		b6ball.speed = 64U;
		b6ball.patnum_tiny = 201;
		b6balls_add(state);
	}
	if((boss.phase_frame % 4) != 0) return true;
	auto &bullet = state.bullets.bullet_template;
	bullet.origin.x = word_sub(bullet.origin.x, shinki_wing_width / 2);
	bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
	bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::exact_linear);
	bullet.spread = 3U;
	bullet.spread_angle_delta = 2U;
	bullet.spawn_type = static_cast<std::uint8_t>(
		bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
	);
	bullet.speed = 40U;
	bullet.patnum = pat_bullet16_vector_red;
	const auto spread_cycle = static_cast<std::uint16_t>(boss.phase_frame % 128);
	bullet.angle = spread_cycle < 64U ? static_cast<std::uint8_t>(spread_cycle * 3U) :
		static_cast<std::uint8_t>(64U - (spread_cycle * 3U));
	if(!bullets_add_special(state)) return false;
	bullet.angle = static_cast<std::uint8_t>(0x80U - bullet.angle);
	bullet.origin.x = word_add(bullet.origin.x, shinki_wing_width);
	if(!bullets_add_special(state)) return false;
	presentation.sound(15U);
	return true;
}

[[nodiscard]] static bool stage6_pattern_devil(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &laser = state.lasers.laser_template;
	if(boss.phase_frame < 192) return true;
	if((boss.hp <= shinki_phase_10_devil_laser_hp || boss.phase_frame >= 1800) &&
		state.stage6.devil_laser_grow_delay == 0U) {
		laser.coords.width = 6U;
		laser.coords.angle = 0x40U;
		laser.color = 14U;
		laser.coords.origin.x = word_add(laser.coords.origin.x, 1536);
		for(std::uint8_t slot{}; slot < 4U; ++slot) {
			if(!laser_manual_fixed_spawn(state, slot, presentation)) return false;
			if(slot < 3U) laser.coords.origin.x = word_sub(laser.coords.origin.x, 1024);
		}
		++state.stage6.devil_laser_grow_delay;
		boss_explode_small(state, presentation, 0U);
	}
	if((boss.phase_frame % 4) == 0) {
		auto &bullet = state.bullets.bullet_template;
		if(boss.phase_frame == 192) {
			boss.statebytes[14U] = 0x20U;
			boss.statebytes[15U] = 0U;
			boss.statebytes[13U] = stage6_rank_select(state, 64U, 40U, 32U, 28U);
			boss.statebytes[12U] = 0U;
			boss.statebytes[11U] = 0U;
			boss.statebytes[10U] = 0U;
			boss.statebytes[8U] = 0x40U;
			boss.statebytes[9U] = 0x40U;
			boss.statebytes[7U] = 0U;
		}
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.patnum = pat_bullet16_vector_red;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		if(boss.statebytes[15U] == 0x40U && boss.statebytes[12U] == 0U) {
			++boss.statebytes[12U];
		}
		bullet.spread = boss.statebytes[12U] == 0U ? 1U : 3U;
		bullet.spread_angle_delta = 0x30U;
		bullet.speed = 112U;
		bullet.origin.x = word_add(bullet.origin.x, 1536);
		bullet.angle = boss.statebytes[15U];
		if(!bullets_add_regular_fixedspeed(state)) return false;
		bullet.origin.x = word_sub(bullet.origin.x, 1024);
		bullet.angle = boss.statebytes[14U];
		if(!bullets_add_regular_fixedspeed(state)) return false;
		bullet.origin.x = word_sub(bullet.origin.x, 1024);
		bullet.angle = static_cast<std::uint8_t>(0x80U - boss.statebytes[14U]);
		if(!bullets_add_regular_fixedspeed(state)) return false;
		bullet.origin.x = word_sub(bullet.origin.x, 1024);
		bullet.angle = static_cast<std::uint8_t>(0x80U - boss.statebytes[15U]);
		if(!bullets_add_regular_fixedspeed(state)) return false;
		if(boss.statebytes[12U] == 0U) {
			boss.statebytes[15U] = byte_add(boss.statebytes[15U], 4);
			boss.statebytes[14U] = byte_add(boss.statebytes[14U], 2);
		} else {
			bullet.patnum = 0U;
			bullet.spawn_type = bullet_spawn_type_no_decelerate;
			bullet.spread = 3U;
			bullet.spread_angle_delta = 0x30U;
			bullet.angle = byte_add(boss.statebytes[15U], 0x80);
			bullet.origin.x = word_add(bullet.origin.x, 3072);
			if(!bullets_add_regular_fixedspeed(state)) return false;
			bullet.origin.x = word_sub(bullet.origin.x, 1024);
			bullet.angle = byte_add(boss.statebytes[14U], 0x80);
			if(!bullets_add_regular_fixedspeed(state)) return false;
			bullet.origin.x = word_sub(bullet.origin.x, 1024);
			bullet.angle = byte_add(static_cast<std::uint8_t>(-boss.statebytes[14U]), 0);
			if(!bullets_add_regular_fixedspeed(state)) return false;
			bullet.origin.x = word_sub(bullet.origin.x, 1024);
			bullet.angle = static_cast<std::uint8_t>(-boss.statebytes[15U]);
			if(!bullets_add_regular_fixedspeed(state)) return false;
			if(boss.statebytes[11U] == 0U) {
				boss.statebytes[15U] = byte_add(boss.statebytes[15U], -2);
				boss.statebytes[14U] = byte_add(boss.statebytes[14U], -1);
				if(boss.statebytes[15U] == 0x30U) ++boss.statebytes[11U];
			} else {
				boss.statebytes[15U] = byte_add(boss.statebytes[15U], 2);
				boss.statebytes[14U] = byte_add(boss.statebytes[14U], 1);
				if(boss.statebytes[15U] == 0x40U) --boss.statebytes[11U];
			}
		}
		if(boss.statebytes[12U] != 0U && (boss.phase_frame % boss.statebytes[13U]) == 0) {
			auto &b6ball = state.b6balls.b6ball_template;
			if(!stage6_wing_random_x(state, b6ball.origin.x) || !stage6_wing_random_y(state, b6ball.origin.y) ||
				!stage6_player_angle(state, b6ball.origin, b6ball.angle)) return false;
			b6ball.speed = 60U;
			b6ball.patnum_tiny = 201;
			b6balls_add(state);
		}
		presentation.sound(3U);
	}
	if(state.stage6.devil_laser_grow_delay == 0U) return true;
	if(state.stage6.devil_laser_grow_delay < 64U) {
		++state.stage6.devil_laser_grow_delay;
		return true;
	}
	if(state.stage6.devil_laser_grow_delay == 64U) {
		for(std::uint8_t slot{}; slot < 4U; ++slot) {
			if(!laser_manual_grow(state, slot, presentation)) return false;
		}
		++state.stage6.devil_laser_grow_delay;
		return true;
	}
	if(boss.statebytes[10U] == 0U) {
		if(boss.statebytes[15U] == 0x30U) ++boss.statebytes[10U];
		return true;
	}
	if((boss.phase_frame % 8) != 0) return true;
	state.lasers.pool[0U].coords.angle = boss.statebytes[9U];
	state.lasers.pool[1U].coords.angle = boss.statebytes[8U];
	state.lasers.pool[2U].coords.angle = static_cast<std::uint8_t>(0x80U - boss.statebytes[8U]);
	state.lasers.pool[3U].coords.angle = static_cast<std::uint8_t>(0x80U - boss.statebytes[9U]);
	if(boss.statebytes[7U] == 0U) {
		++boss.statebytes[9U];
		boss.statebytes[8U] = byte_add(boss.statebytes[8U], -1);
		if(boss.statebytes[8U] == 0x38U) ++boss.statebytes[7U];
	} else {
		boss.statebytes[9U] = byte_add(boss.statebytes[9U], -1);
		++boss.statebytes[8U];
		if(boss.statebytes[8U] == 0x41U) --boss.statebytes[7U];
	}
	if((boss.hp <= shinki_phase_10_devil_fast_hp || boss.phase_frame >= 2500) &&
		state.stage6.devil_laser_grow_delay++ == 65U) {
		boss.statebytes[13U] = stage6_rank_select(state, 52U, 20U, 16U, 12U);
		boss_explode_small(state, presentation, 0U);
	}
	return true;
}

[[nodiscard]] static bool stage6_pattern_circles_and_alternating_spirals(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &b6ball = state.b6balls.b6ball_template;
	if(boss.phase_frame < 128) return true;
	if(boss.phase_frame == 128) {
		b6ball.angle = 0U;
		boss.statebytes[15U] = 0U;
		boss.statebytes[14U] = 0U;
	}
	if((boss.phase_frame % 8) == 0) {
		b6ball.patnum_tiny = 200;
		const auto offset = pc98::masterlib::vector2(b6ball.angle, 1024);
		b6ball.origin = {word_add(boss.pos.cur.x, offset.x), word_add(boss.pos.cur.y, offset.y)};
		b6balls_add(state);
		b6ball.angle = byte_add(b6ball.angle, 0x80);
		const auto opposite = pc98::masterlib::vector2(b6ball.angle, 1024);
		b6ball.origin = {word_add(boss.pos.cur.x, opposite.x), word_add(boss.pos.cur.y, opposite.y)};
		b6balls_add(state);
		b6ball.angle = byte_add(b6ball.angle, -0x78);
		presentation.sound(3U);
	}
	if(boss.phase_frame >= 256) {
		const auto frame_in_cycle = static_cast<std::uint16_t>(boss.phase_frame % 256);
		auto &bullet = state.bullets.bullet_template;
		if((boss.phase_frame % 8) == 0 && frame_in_cycle < 128U) {
			bullet.spawn_type = static_cast<std::uint8_t>(
				bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
			);
			bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
			bullet.angle = boss.statebytes[15U];
			bullet.patnum = pat_bullet16_vector_red;
			bullet.speed = 32U;
			bullet.spread = 12U;
			if(!bullets_add_regular(state)) return false;
			boss.statebytes[15U] = byte_add(boss.statebytes[15U], -2);
		}
		if(boss.phase_frame >= 512 && (boss.phase_frame % 8) == 0 && frame_in_cycle >= 128U) {
			bullet.spawn_type = static_cast<std::uint8_t>(
				bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
			);
			bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
			bullet.angle = boss.statebytes[14U];
			bullet.patnum = 0U;
			bullet.speed = 32U;
			bullet.spread = 12U;
			if(!bullets_add_regular(state)) return false;
			boss.statebytes[14U] = byte_add(boss.statebytes[14U], 4);
		}
	}
	if(boss.phase_frame >= 720) {
		const auto flight = boss_flystep_random(
			state, static_cast<std::int16_t>((boss.phase_frame - 720) % 128 - 96)
		);
		if(flight == FlyResult::unsupported) return false;
	}
	return true;
}

static void stage6_move_float(State &state) noexcept
{
	if((state.stage_frame & 3U) != 0U) return;
	auto &boss = state.boss;
	if(!state.stage6.float_direction) {
		boss.pos.velocity.y = word_add(boss.pos.velocity.y, 1);
		if(boss.pos.velocity.y >= 16) state.stage6.float_direction = true;
	} else {
		boss.pos.velocity.y = word_sub(boss.pos.velocity.y, 1);
		if(boss.pos.velocity.y <= -16) state.stage6.float_direction = false;
	}
	boss.pos.cur.y = word_add(boss.pos.cur.y, boss.pos.velocity.y);
}

[[nodiscard]] static bool stage6_phase_2_3_next(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	if(boss.statebytes[10U] == 0U) {
		if(!boss_phase_next(state, presentation, 1U, shinki_phase_3_end_hp)) return false;
		boss.mode = 1U;
		state.stage6.phase_2_3_pattern = Stage6Phase23Pattern::random_directional_and_kunai;
	} else if(!boss_phase_next(state, presentation, 1U, shinki_phase_6_end_hp)) {
		return false;
	}
	++boss.statebytes[10U];
	return true;
}

[[nodiscard]] static bool stage6_shinki_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	state.shots.homing_target = boss.pos.cur;
	state.bullets.bullet_template.origin = boss.pos.cur;
	state.bullets.gather_template.center = boss.pos.cur;
	state.lasers.laser_template.coords.origin = boss.pos.cur;
	state.b6balls.b6ball_template.origin = boss.pos.cur;
	++boss.phase_frame;

	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 1) {
			boss.hp = shinki_hp_total;
			boss.phase_end_hp = shinki_phase_2_end_hp;
			state.bullets.gather_template.radius = 1024;
			state.bullets.gather_template.angle_delta = 2U;
			state.bullets.gather_template.ring_points = 8;
			boss.sprite = pat_shinki_still;
			boss.pos.velocity.y = 0;
			boss.sprite_left = pat_shinki_left;
			boss.sprite_right = pat_shinki_right;
			boss.sprite_stay = pat_shinki_still;
		}
		if(!boss_hittest_invincible(state, presentation)) return false;
		if(boss.phase_frame >= 192) {
			boss.phase_frame = 0;
			++boss.phase;
			presentation.sound(13U);
		}
		break;

	case 1U:
		if(!boss_hittest_invincible(state, presentation)) return false;
		if(boss.phase_frame == 32) {
			state.palette[0U] = {};
			state.palette_changed = true;
		}
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.mode = 1U;
			boss.phase_frame = 0;
			state.stage6.phase_2_3_pattern = Stage6Phase23Pattern::curved_rings;
			boss.statebytes[10U] = 0U;
		}
		break;

	case 2U:
	case 3U:
		if(boss.mode == 0U) {
			const auto flight = boss_flystep_random(
				state, static_cast<std::int16_t>(boss.phase_frame - 32)
			);
			if(flight == FlyResult::unsupported) return false;
			if(flight == FlyResult::complete) {
				boss.phase_frame = 0;
				++boss.phase_state;
				++boss.mode;
				if(boss.statebytes[10U] == 0U) {
					state.stage6.phase_2_3_pattern = (boss.phase_state & 1U) == 0U ?
						Stage6Phase23Pattern::curved_rings : Stage6Phase23Pattern::dualspeed_rings;
				} else {
					state.stage6.phase_2_3_pattern = (boss.phase_state & 1U) == 0U ?
						Stage6Phase23Pattern::random_directional_and_kunai :
						Stage6Phase23Pattern::dense_blue_stacks;
				}
				if(boss.phase_state >= 16U) {
					if(!stage6_phase_2_3_next(state, presentation)) return false;
					break;
				}
			}
		} else if(boss.mode == 1U) {
			const auto result = stage6_phase_2_3_with_pattern(state, presentation);
			if(result == Stage6PatternResult::unsupported) return false;
		}
		if(!boss_hittest(state, presentation)) break;
		if(!boss_score_bonus(state, presentation, 10U)) return false;
		if(!stage6_phase_2_3_next(state, presentation)) return false;
		break;

	case 4U:
		(void)boss_hittest(state, presentation);
		if(boss_flystep_towards(state, 3072, 1280)) {
			++boss.phase;
			boss.phase_frame = 0;
		}
		break;

	case 5U: {
		(void)boss_hittest(state, presentation);
		const auto result = stage6_pattern_wing_preparation(state, presentation);
		if(result == Stage6PatternResult::unsupported) return false;
		if(result == Stage6PatternResult::completed) {
			++boss.phase;
			boss.phase_frame = 0;
			boss.pos.velocity.y = 0;
			switch(state.playchar) {
			case 0U: boss.hitbox_radius.x = 800; break;
			case 1U: boss.hitbox_radius.x = 560; break;
			case 2U: boss.hitbox_radius.x = 720; break;
			case 3U: boss.hitbox_radius.x = 800; break;
			default: return false;
			}
			state.stage6.wing_pattern = Stage6WingPattern::random_rain_and_spreads;
			boss.statebytes[10U] = 0U;
		}
		break;
	}

	case 6U:
	case 7U: {
		if(state.palette[0U].b < 0x80U) {
			if((state.stage_frame & 15U) == 0U) ++state.palette[0U].b;
			state.palette_changed = true;
		}
		stage6_move_float(state);
		if(state.stage6.wing_pattern == Stage6WingPattern::random_rain_and_spreads) {
			if(!stage6_pattern_random_rain_and_spreads(state, presentation)) return false;
		} else if(state.stage6.wing_pattern == Stage6WingPattern::cheetos_within_spread_walls) {
			if(!stage6_pattern_cheetos_within_spread_walls(state, presentation)) return false;
		} else {
			return false;
		}
		if(boss.phase_frame < 3000 && !boss_hittest(state, presentation)) break;
		if(boss.phase_frame < 3000 && !boss_score_bonus(state, presentation, 25U)) return false;
		if(boss.statebytes[10U] == 0U) {
			if(!boss_phase_next(state, presentation, 2U, shinki_phase_7_end_hp)) return false;
			state.stage6.wing_pattern = Stage6WingPattern::cheetos_within_spread_walls;
			boss.phase_frame = 0;
			++boss.statebytes[10U];
		} else if(!boss_phase_next(state, presentation, 3U, shinki_phase_9_end_hp)) {
			return false;
		}
		break;
	}

	case 8U: {
		// b6.cpp's purple-wing callback has a three-way source result: live,
		// complete, or a reached operation failure while populating its B6 pool.
		if(state.palette[0U].b > 0U) {
			if((state.stage_frame & 7U) == 0U) --state.palette[0U].b;
			state.palette_changed = true;
		}
		(void)boss_hittest(state, presentation);
		const auto result = stage6_pattern_wings_to_purple(state, presentation);
		if(result == Stage6PatternResult::unsupported) return false;
		if(result == Stage6PatternResult::completed) {
			++boss.phase;
			boss.phase_frame = 0;
			state.stage6.wing_pattern = Stage6WingPattern::aimed_b6balls_and_symmetric_spreads;
			boss.statebytes[10U] = 0U;
		}
		break;
	}

	case 9U:
	case 10U: {
		if(state.palette[0U].b > 0U) {
			if((state.stage_frame & 7U) == 0U) --state.palette[0U].b;
			state.palette_changed = true;
		} else if(state.palette[0U].r < 0x80U) {
			if((state.stage_frame & 15U) == 0U) ++state.palette[0U].r;
			state.palette_changed = true;
		}
		stage6_move_float(state);
		if(state.stage6.wing_pattern == Stage6WingPattern::aimed_b6balls_and_symmetric_spreads) {
			if(!stage6_pattern_aimed_b6balls_and_symmetric_spreads(state, presentation)) return false;
		} else if(state.stage6.wing_pattern == Stage6WingPattern::devil) {
			if(!stage6_pattern_devil(state, presentation)) return false;
		} else {
			return false;
		}
		if(boss.phase_frame < 3000) {
			const bool phase_done = boss_hittest(state, presentation);
			if(state.bombing) {
				boss.hp = word_add(
					boss.hp, static_cast<std::int16_t>(static_cast<std::int16_t>(boss.damage_this_frame) - 1)
				);
				break;
			}
			if(!phase_done) break;
			if(!boss_score_bonus(state, presentation, 25U)) return false;
		}
		if(boss.statebytes[10U] == 0U) {
			if(!boss_phase_next(state, presentation, 2U, shinki_phase_10_end_hp)) return false;
			state.stage6.wing_pattern = Stage6WingPattern::devil;
			boss.phase_frame = 0;
			++boss.statebytes[10U];
		} else {
			if(!boss_phase_next(state, presentation, 3U, shinki_phase_12_end_hp)) return false;
			state.playfield_shake_anim_time = 16;
			for(std::uint8_t slot{}; slot < 4U; ++slot) {
				if(!laser_stop(state, slot)) return false;
			}
		}
		break;
	}

	case 11U:
		(void)boss_hittest(state, presentation);
		state.palette_tone = (boss.phase_frame & 1) == 0 ? 150U : 100U;
		state.palette_changed = true;
		if(boss.phase_frame > 16) {
			++boss.phase;
			boss.sprite = pat_shinki_still;
			boss.hitbox_radius.x = 384;
			state.palette[0U].r = 0x60U;
			state.palette[0U].b = 0U;
			presentation.sound(13U);
		}
		break;

	case 12U:
		if(!stage6_pattern_circles_and_alternating_spirals(state, presentation)) return false;
		if(boss.phase_frame < 3000 && !boss_hittest(state, presentation)) break;
		if(boss.phase_frame < 3000) boss.phase_state = 1U;
		boss_explode_small(state, presentation, 4U);
		boss.phase_frame = 0;
		boss.phase = phase_explode_small;
		break;

	default:
		if(!boss_defeat_update(state, presentation, 65U)) return false;
		break;
	}
	// b6.cpp advances the native B6 overlay before its Cheeto view.  The
	// order is gameplay-visible because the two views share these records.
	b6balls_update(state);
	if(!stage6_cheetos_update(state)) return false;
	presentation.boss_hp(boss.hp, shinki_hp_total);
	return true;
}

void stage6_fights_setup(State &state) noexcept
{
	state.midboss_active = false;
	state.boss_active = false;
	state.midboss.frames_until = 30000U;
	state.boss.phase = 0U;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.phase_frame = 0;
	state.boss.damage_this_frame = 0U;
	state.boss.phase_timed_out = true;
	state.boss.pos.cur = {3072, 1024};
	state.boss.pos.prev = state.boss.pos.cur;
	state.boss.pos.velocity = {};
	state.boss.sprite = pat_shinki_still;
	state.boss.hitbox_radius = {384, 384};
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
}

void stage6_midboss_activate(State &state) noexcept
{
	(void)state;
}

void stage6_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}

bool stage6_fights_update(State &state, Presentation &presentation) noexcept
{
	return !state.boss_active || stage6_shinki_update(state, presentation);
}

void stage6_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	if(state.boss_active && state.boss.phase != phase_explode_big) {
		state.boss.damage_this_frame = 0U;
	}
	b6balls_render_tail(state);
	fights_render_tail_explosions(state);
	(void)presentation;
}

// Extra Stage boss EX-Alice.  bx_upd.cpp stores each current pattern in a
// source callback pointer; ExtraState keeps the concrete selector instead.
constexpr std::int16_t exalice_hp_total = 26500;
constexpr std::int16_t exalice_phase_last_hp = 3400;
constexpr std::int16_t exalice_fly_left = 32 * subpixel_factor;
constexpr std::int16_t exalice_fly_right = (384 - 32) * subpixel_factor;
constexpr std::int16_t exalice_fly_top = 48 * subpixel_factor;
constexpr std::int16_t exalice_fly_bottom = 96 * subpixel_factor;

enum class ExtraPatternResult : std::uint8_t {
	running,
	completed,
	unsupported,
};

[[nodiscard]] static bool extra_player_angle(
	const State &state, BossPoint origin, std::int16_t offset, std::uint8_t &angle
) noexcept
{
	const auto target = pc98::masterlib::iatan2_source(
		word_sub(state.player_pos.cur.y, origin.y), word_sub(state.player_pos.cur.x, origin.x)
	);
	if(!target) return false;
	angle = byte_add(*target, offset);
	return true;
}

// move.cpp's flystep uses common central direction fields but the mutable
// clamp that bx_upd.cpp installs during EX-Alice's HP-fill phase.
[[nodiscard]] static FlyResult extra_flystep_random(State &state, std::int16_t frame) noexcept
{
	auto &boss = state.boss;
	if(frame == 0) {
		if(boss.pos.cur.x < boss_fly_field_left) {
			const auto random = pc98::th0405::randring_next16_mod(state.random, 96U);
			if(!random) return FlyResult::unsupported;
			boss.angle = byte_add(static_cast<std::uint8_t>(*random), -0x30);
		} else if(boss.pos.cur.x > boss_fly_field_right) {
			const auto random = pc98::th0405::randring_next16_and(state.random, 0x60U);
			if(!random) return FlyResult::unsupported;
			boss.angle = byte_add(static_cast<std::uint8_t>(*random), 0x30);
		} else {
			const auto random = pc98::th0405::randring_next16(state.random);
			if(!random) return FlyResult::unsupported;
			boss.angle = static_cast<std::uint8_t>(*random);
		}
		if((boss.flystep_next_y_direction == 1 && boss.angle < 0x80U) ||
			(boss.flystep_next_y_direction == 2 && boss.angle >= 0x80U)) {
			boss.angle = static_cast<std::uint8_t>(-boss.angle);
		}
		boss.flystep_next_y_direction = 0;
	}
	if(frame < 0) return FlyResult::moving;
	const auto velocity = pc98::masterlib::vector2(
		boss.angle, word(64 - static_cast<std::int32_t>(frame) * 2)
	);
	boss.pos.velocity = {velocity.x, velocity.y};
	boss.pos.cur.x = word_add(boss.pos.cur.x, boss.pos.velocity.x);
	boss.pos.cur.y = word_add(boss.pos.cur.y, boss.pos.velocity.y);
	boss.sprite = boss.pos.velocity.x < 0 ? boss.sprite_left : boss.sprite_right;
	if(boss.pos.cur.y < exalice_fly_top) {
		boss.pos.cur.y = exalice_fly_top;
		boss.flystep_next_y_direction = 2;
	} else if(boss.pos.cur.y > exalice_fly_bottom) {
		boss.pos.cur.y = exalice_fly_bottom;
		boss.flystep_next_y_direction = 1;
	}
	if(boss.pos.cur.x < exalice_fly_left) {
		boss.pos.cur.x = exalice_fly_left;
	} else if(boss.pos.cur.x > exalice_fly_right) {
		boss.pos.cur.x = exalice_fly_right;
	}
	if(frame < 28) return FlyResult::moving;
	boss.sprite = boss.sprite_stay;
	return FlyResult::complete;
}

[[nodiscard]] static bool extra_hittest(State &state, Presentation &presentation) noexcept
{
	if(state.bombing) state.extra.invincibility_frames = 39U;
	if(state.extra.invincibility_frames != 0U) return false;
	return boss_hittest(state, presentation);
}

[[nodiscard]] static bool extra_phase_next(
	State &state, Presentation &presentation, std::uint8_t explosion_type, std::int16_t next_end_hp
) noexcept
{
	boss_explode_small(state, presentation, explosion_type);
	if(!state.boss.phase_timed_out) {
		state.bullets.zap.start();
		state.bullets.zap_drop_point_items = true;
		if(!boss_items_drop(state, presentation)) return false;
	}
	++state.boss.phase;
	state.boss.phase_timed_out = true;
	state.boss.phase_frame = 0;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.hp = state.boss.phase_end_hp;
	state.boss.phase_end_hp = next_end_hp;
	return true;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_random_red_spreads(
	State &state, Presentation &presentation
) noexcept
{
	auto &bullet = state.bullets.bullet_template;
	const auto speed = pc98::th0405::randring_next16_and(state.random, 0x1FU);
	const auto angle = pc98::th0405::randring_next16_and(state.random, 0x7FU);
	if(!speed || !angle) return ExtraPatternResult::unsupported;
	bullet.spawn_type = bullet_spawn_type_no_decelerate;
	bullet.speed = static_cast<std::uint8_t>(*speed + 40U);
	bullet.angle = byte_add(static_cast<std::uint8_t>(*angle & 0xF8U), -0x40);
	bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_aimed);
	bullet.patnum = pat_bullet16_vector_red;
	bullet.spread = 5U;
	bullet.spread_angle_delta = 12U;
	if(!bullets_add_regular(state)) return ExtraPatternResult::unsupported;
	if((state.boss.phase_frame % 4) == 0) presentation.sound(3U);
	return state.boss.phase_frame == 128 ? ExtraPatternResult::completed : ExtraPatternResult::running;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_rotating_red_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame == 64) boss.statebytes[15U] = static_cast<std::uint8_t>(1U - boss.statebytes[15U]);
	if((boss.phase_frame % 4) == 0) {
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_backwards | bullet_spawn_type_no_decelerate
		);
		bullet.speed = 64U;
		bullet.angle = byte_add(bullet.angle, boss.statebytes[15U] != 0U ? 2 : -2);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
		bullet.special_motion = static_cast<std::uint8_t>(
			BulletSpecialMotion::decelerate_then_turn_aimed
		);
		state.bullets.special_turns_max = 1U;
		bullet.spread = 12U;
		bullet.spread_angle_delta = 10U;
		bullet.patnum = pat_bullet16_ball_red;
		if((boss.phase_frame % 8) == 0) {
			if(!bullets_add_regular(state)) return ExtraPatternResult::unsupported;
		} else {
			bullet.spread = 8U;
			bullet.patnum = pat_bullet16_vector_red;
			if(!bullets_add_special(state)) return ExtraPatternResult::unsupported;
		}
		presentation.sound(3U);
	}
	const auto flight = extra_flystep_random(state, static_cast<std::int16_t>(boss.phase_frame - 100));
	if(flight == FlyResult::unsupported) return ExtraPatternResult::unsupported;
	return boss.phase_frame == 128 ? ExtraPatternResult::completed : ExtraPatternResult::running;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_spreads_and_firewaves(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame == 64) {
		boss.statebytes[14U] = 0x40U;
		boss.statebytes[13U] = 0U;
		state.bullets.pellet_bottom_color = 7U;
	}
	if((boss.phase_frame % 2) == 0) {
		if((boss.phase_frame % 4) == 0) {
			bullet.spawn_type = bullet_spawn_type_no_decelerate;
			bullet.patnum = 0U;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_aimed);
			bullet.spread = 5U;
			bullet.spread_angle_delta = 6U;
			bullet.speed = 80U;
			bullet.angle = boss.statebytes[14U];
			if(!bullets_add_regular(state)) return ExtraPatternResult::unsupported;
			bullet.angle = static_cast<std::uint8_t>(-bullet.angle);
			if(!bullets_add_regular(state)) return ExtraPatternResult::unsupported;
			if(boss.statebytes[13U] == 0U) {
				boss.statebytes[14U] = byte_add(boss.statebytes[14U], -3);
				if(boss.statebytes[14U] <= 0x14U) ++boss.statebytes[13U];
			} else {
				boss.statebytes[14U] = byte_add(boss.statebytes[14U], 3);
				if(boss.statebytes[14U] >= 0x40U) --boss.statebytes[13U];
			}
			if((boss.phase_frame % 16) == 0) {
				const auto x = pc98::th0405::randring_next16_mod(state.random, 1536U);
				const auto y = pc98::th0405::randring_next16_mod(state.random, 1024U);
				if(!x || !y) return ExtraPatternResult::unsupported;
				state.extra.random_origin.x = word_add(
					bullet.origin.x, word_sub(static_cast<std::int16_t>(*x), 768)
				);
				state.extra.random_origin.y = word_add(
					bullet.origin.y, word_sub(static_cast<std::int16_t>(*y), 512)
				);
				if(!extra_player_angle(state, state.extra.random_origin, 0, boss.statebytes[15U])) {
					return ExtraPatternResult::unsupported;
				}
			}
		}
		bullet.origin = state.extra.random_origin;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.angle = boss.statebytes[15U];
		bullet.group = static_cast<std::uint8_t>(BulletGroup::single);
		bullet.speed = 96U;
		bullet.patnum = pat_bullet16_vector_red;
		if(!bullets_add_regular(state)) return ExtraPatternResult::unsupported;
	}
	if((boss.phase_frame % 128) == 0) {
		firewaves_add(state, 128, (boss.phase_frame % 256) == 0);
		presentation.sound(13U);
	}
	if(boss.phase_frame >= 256) {
		const auto flight = extra_flystep_random(
			state, static_cast<std::int16_t>((boss.phase_frame & 0x7F) - 96)
		);
		if(flight == FlyResult::unsupported) return ExtraPatternResult::unsupported;
	}
	return ExtraPatternResult::running;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_gravity_balls_and_stacks(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if((boss.phase_frame % 2) == 0) {
		const auto speed = pc98::th0405::randring_next16_and(state.random, 0x1FU);
		const auto angle = pc98::th0405::randring_next16_and(state.random, 0x7FU);
		if(!speed || !angle) return ExtraPatternResult::unsupported;
		bullet.spawn_type = bullet_spawn_type_no_decelerate;
		bullet.speed = static_cast<std::uint8_t>(*speed + 12U);
		bullet.angle = static_cast<std::uint8_t>(*angle + 0x80U);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::gravity);
		state.bullets.special_turns_max = 1U;
		bullet.patnum = pat_bullet16_small_ball_blue;
		bullet.spread = 3U;
		bullet.spread_angle_delta = 12U;
		if(!bullets_add_special(state)) return ExtraPatternResult::unsupported;
		if((boss.phase_frame % 4) == 0) {
			presentation.sound(3U);
			const auto x = pc98::th0405::randring_next16_mod(state.random, 1536U);
			const auto y = pc98::th0405::randring_next16_mod(state.random, 1024U);
			if(!x || !y) return ExtraPatternResult::unsupported;
			bullet.spawn_type = static_cast<std::uint8_t>(
				bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
			);
			bullet.origin.x = word_add(bullet.origin.x, word_sub(static_cast<std::int16_t>(*x), 768));
			bullet.origin.y = word_add(bullet.origin.y, word_sub(static_cast<std::int16_t>(*y), 512));
			bullet.patnum = 0U;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::stack_aimed);
			bullet.speed = 48U;
			bullet.stack = 4U;
			bullet.stack_speed_delta = 6U;
			bullet.angle = 0U;
			if(!bullets_add_regular(state)) return ExtraPatternResult::unsupported;
		}
	}
	return boss.phase_frame == 128 ? ExtraPatternResult::completed : ExtraPatternResult::running;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_rotating_blue_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame == 64) boss.statebytes[15U] = static_cast<std::uint8_t>(1U - boss.statebytes[15U]);
	if((boss.phase_frame % 8) == 0) {
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_backwards | bullet_spawn_type_no_decelerate
		);
		bullet.speed = 88U;
		bullet.angle = byte_add(bullet.angle, boss.statebytes[15U] != 0U ? 3 : -3);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
		bullet.spread = 22U;
		bullet.spread_angle_delta = 8U;
		bullet.patnum = pat_bullet16_directional_blue;
		if(!bullets_add_regular(state)) return ExtraPatternResult::unsupported;
		presentation.sound(3U);
	}
	return boss.phase_frame == 128 ? ExtraPatternResult::completed : ExtraPatternResult::running;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_bouncing_blue_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if(boss.phase_frame == 64) bullet.spread = 8U;
	if((boss.phase_frame % 16) == 0) {
		bullet.patnum = pat_bullet16_directional_blue;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_backwards | bullet_spawn_type_no_decelerate
		);
		bullet.speed = 32U;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
		bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::bounce_left_right_top);
		if((boss.phase_frame % 128) == 0 && bullet.spread < 14U) ++bullet.spread;
		state.bullets.special_turns_max = 1U;
		bullet.angle = byte_add(bullet.angle, 2);
		if(!bullets_add_special(state)) return ExtraPatternResult::unsupported;
		presentation.sound(3U);
	}
	return ExtraPatternResult::running;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_lasers_and_green_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &laser = state.lasers.laser_template;
	laser.color = 3U;
	laser.coords.width = 8U;
	laser.active_at_age = 47;
	laser.shrink_at_age = 80;
	const auto cycle_frame = static_cast<std::int16_t>(boss.phase_frame % 16);
	if(boss.phase_frame < 144) {
		if(cycle_frame == 0) {
			if(!extra_player_angle(state, laser.coords.origin, 0x30, laser.coords.angle)) {
				return ExtraPatternResult::unsupported;
			}
			if(!laser_fixed_spawn(state, state.extra.laser_slot++, presentation)) {
				return ExtraPatternResult::unsupported;
			}
			const auto random_angle = pc98::th0405::randring_next16(state.random);
			if(!random_angle) return ExtraPatternResult::unsupported;
			auto &bullet = state.bullets.bullet_template;
			bullet.spawn_type = bullet_spawn_type_no_decelerate;
			bullet.patnum = pat_bullet16_directional_green;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::ring);
			bullet.angle = static_cast<std::uint8_t>(*random_angle);
			bullet.speed = 64U;
			bullet.spread = 32U;
			bullet.spread_angle_delta = 10U;
			if(!bullets_add_regular(state)) return ExtraPatternResult::unsupported;
		} else if(cycle_frame == 8) {
			if(!extra_player_angle(state, laser.coords.origin, -0x30, laser.coords.angle)) {
				return ExtraPatternResult::unsupported;
			}
			if(!laser_fixed_spawn(state, state.extra.laser_slot++, presentation)) {
				return ExtraPatternResult::unsupported;
			}
		}
	}
	state.extra.laser_slot &= 0x0FU;
	for(std::uint8_t slot{}; slot < 16U; ++slot) {
		if(state.lasers.pool[slot].flag != LaserFlag::fixed_wait_to_grow) continue;
		state.lasers.pool[slot].coords.angle = byte_add(
			state.lasers.pool[slot].coords.angle, (slot & 1U) != 0U ? 1 : -1
		);
	}
	if(boss.phase_frame != 208) return ExtraPatternResult::running;
	state.extra.laser_slot = 0U;
	return ExtraPatternResult::completed;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_speedup_spreads(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	if((boss.phase_frame % 2) == 0) {
		const auto x = pc98::th0405::randring_next16_mod(state.random, 768U);
		if(!x) return ExtraPatternResult::unsupported;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_aimed);
		bullet.special_motion = static_cast<std::uint8_t>(BulletSpecialMotion::speedup);
		state.bullets.special_turns_max = 2U;
		bullet.speed = 64U;
		bullet.spread = 7U;
		bullet.spread_angle_delta = 9U;
		bullet.patnum = pat_bullet16_directional_green;
		bullet.origin.x = word_add(bullet.origin.x, word_sub(static_cast<std::int16_t>(*x), 384));
		bullet.angle = 0U;
		if(!bullets_add_special(state)) return ExtraPatternResult::unsupported;
		presentation.sound(3U);
	}
	const auto flight = extra_flystep_random(state, static_cast<std::int16_t>(boss.phase_frame - 100));
	if(flight == FlyResult::unsupported) return ExtraPatternResult::unsupported;
	return boss.phase_frame == 128 ? ExtraPatternResult::completed : ExtraPatternResult::running;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_pingpong_lasers(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &laser = state.lasers.laser_template;
	if(boss.phase_frame == 64) {
		laser.color = 2U;
		laser.coords.width = 6U;
		laser.coords.angle = 0U;
		laser.active_at_age = 30;
		laser.shootout_speed = 88;
		boss.statebytes[15U] = 0U;
	}
	if((boss.phase_frame % 4) == 0) {
		if(boss.statebytes[15U] == 0U) {
			laser.coords.angle = byte_add(laser.coords.angle, 8);
			if(laser.coords.angle >= 128U) boss.statebytes[15U] = 1U;
			if(!lasers_shootout_add(state, presentation)) return ExtraPatternResult::unsupported;
		} else if(boss.statebytes[15U] == 1U) {
			laser.coords.angle = byte_add(laser.coords.angle, -8);
			if(laser.coords.angle == 0U) boss.statebytes[15U] = 0U;
			if(!lasers_shootout_add(state, presentation)) return ExtraPatternResult::unsupported;
		}
	}
	if(boss.phase_frame >= 256) {
		const auto flight = extra_flystep_random(
			state, static_cast<std::int16_t>((boss.phase_frame & 0x3F) - 32)
		);
		if(flight == FlyResult::unsupported) return ExtraPatternResult::unsupported;
		if(boss.phase_frame >= 512 && (boss.phase_frame % 16) == 0) {
			auto &bullet = state.bullets.bullet_template;
			bullet.spawn_type = bullet_spawn_type_no_decelerate;
			bullet.group = static_cast<std::uint8_t>(BulletGroup::ring_aimed);
			bullet.angle = 0U;
			bullet.speed = 64U;
			bullet.spread = 32U;
			bullet.spread_angle_delta = 10U;
			bullet.patnum = pat_bullet16_directional_green;
			if(!bullets_add_regular(state)) return ExtraPatternResult::unsupported;
		}
	}
	return ExtraPatternResult::running;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_aimed_crosses(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if((boss.phase_frame % 16) == 0) {
		auto &bullet = state.bullets.bullet_template;
		bullet.speed = 56U;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread_aimed);
		bullet.patnum = pat_bullet16_cross_blue;
		bullet.angle = 0U;
		bullet.spread = 9U;
		bullet.spread_angle_delta = 15U;
		if(!bullets_add_regular(state)) return ExtraPatternResult::unsupported;
		if((boss.phase_frame % 64) == 0) {
			CheetoTemplate cheeto{};
			cheeto.color = 11;
			cheeto.speed = 48U;
			cheeto.origin = state.boss.pos.cur;
			if(!extra_player_angle(
				state, cheeto.origin, (boss.phase_frame % 128) == 0 ? 0x20 : -0x20, cheeto.angle
			)) return ExtraPatternResult::unsupported;
			stage6_cheetos_add(state, cheeto);
			presentation.sound(15U);
		}
	}
	return boss.phase_frame == 256 ? ExtraPatternResult::completed : ExtraPatternResult::running;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_aimed_cheetos(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if((boss.phase_frame % 8) == 0) {
		CheetoTemplate cheeto{};
		cheeto.color = 11;
		cheeto.speed = 64U;
		cheeto.origin = boss.pos.cur;
		if(!extra_player_angle(
			state, cheeto.origin, (boss.phase_frame % 16) == 0 ? 0x40 : -0x40, cheeto.angle
		)) return ExtraPatternResult::unsupported;
		stage6_cheetos_add(state, cheeto);
		presentation.sound(15U);
	}
	const auto flight = extra_flystep_random(state, static_cast<std::int16_t>(boss.phase_frame % 32));
	if(flight == FlyResult::unsupported) return ExtraPatternResult::unsupported;
	return boss.phase_frame == 128 ? ExtraPatternResult::completed : ExtraPatternResult::running;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_mirrored_crosses(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	auto &gather = state.bullets.gather_template;
	bullet.origin.y = word_add(bullet.origin.y, 1664);
	bullet.origin.x = state.extra.pattern_origin_x;
	gather.center.x = state.extra.pattern_origin_x;
	if(boss.phase_frame == 64) {
		// This is a source template write without an immediate cheetos_add().
		state.b6balls.b6ball_template.angle = static_cast<std::uint8_t>(-0x3C);
		state.b6balls.b6ball_template.speed = 80U;
		bullet.angle = 0x20U;
		bullet.speed = 48U;
		bullet.spawn_type = static_cast<std::uint8_t>(
			bullet_spawn_type_cloud_forwards | bullet_spawn_type_no_decelerate
		);
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		bullet.spread = 3U;
		bullet.spread_angle_delta = 8U;
		bullet.patnum = pat_bullet16_cross_blue;
		boss.statebytes[15U] = 0U;
		boss.statebytes[14U] = 0U;
		boss.statebytes[13U] = 0U;
		presentation.sound(8U);
	}
	if(boss.phase_frame < 128) {
		if((boss.phase_frame % 8) == 0) {
			gather.color = 14U;
			gather.center.y = bullet.origin.y;
			gather.radius = 4096;
			gather.angle_delta = (boss.phase_frame % 16) == 0 ? 2U : static_cast<std::uint8_t>(-2);
			gather_add_only(state);
		}
		return ExtraPatternResult::running;
	}
	if((boss.phase_frame % 2) != 0) return ExtraPatternResult::running;
	if((boss.phase_frame % 16) == 0) {
		gather.center.y = bullet.origin.y;
		gather.radius = 2048;
		gather.angle_delta = (boss.phase_frame % 32) == 0 ? 2U : static_cast<std::uint8_t>(-2);
		gather_add_only(state);
	}
	bullet.angle = static_cast<std::uint8_t>(0x80U - bullet.angle);
	if(!bullets_add_regular_fixedspeed(state)) return ExtraPatternResult::unsupported;
	bullet.angle = static_cast<std::uint8_t>(0x80U - bullet.angle);
	if(!bullets_add_regular_fixedspeed(state)) return ExtraPatternResult::unsupported;
	presentation.sound(3U);
	bullet.angle = byte_add(bullet.angle, -static_cast<std::int16_t>(boss.statebytes[15U]));
	if((boss.phase_frame % 32) != 0) return ExtraPatternResult::running;
	boss.statebytes[15U] = byte_add(boss.statebytes[15U], boss.statebytes[14U]);
	if(boss.statebytes[13U] == 0U) {
		++boss.statebytes[14U];
		if(boss.statebytes[14U] >= 0x10U) ++boss.statebytes[13U];
	} else {
		--boss.statebytes[14U];
		if(boss.statebytes[14U] == 0U) {
			state.extra.pattern_origin_x = state.player_pos.cur.x;
			return ExtraPatternResult::completed;
		}
	}
	return ExtraPatternResult::running;
}

[[nodiscard]] static ExtraPatternResult extra_pattern_update(
	State &state, Presentation &presentation
) noexcept
{
	switch(state.extra.pattern) {
	case ExtraPattern::random_red_spreads:
		return extra_pattern_random_red_spreads(state, presentation);
	case ExtraPattern::rotating_red_rings:
		return extra_pattern_rotating_red_rings(state, presentation);
	case ExtraPattern::spreads_and_firewaves:
		return extra_pattern_spreads_and_firewaves(state, presentation);
	case ExtraPattern::gravity_balls_and_stacks:
		return extra_pattern_gravity_balls_and_stacks(state, presentation);
	case ExtraPattern::rotating_blue_rings:
		return extra_pattern_rotating_blue_rings(state, presentation);
	case ExtraPattern::bouncing_blue_rings:
		return extra_pattern_bouncing_blue_rings(state, presentation);
	case ExtraPattern::lasers_and_green_rings:
		return extra_pattern_lasers_and_green_rings(state, presentation);
	case ExtraPattern::speedup_spreads:
		return extra_pattern_speedup_spreads(state, presentation);
	case ExtraPattern::pingpong_lasers:
		return extra_pattern_pingpong_lasers(state, presentation);
	case ExtraPattern::aimed_crosses:
		return extra_pattern_aimed_crosses(state, presentation);
	case ExtraPattern::aimed_cheetos:
		return extra_pattern_aimed_cheetos(state, presentation);
	case ExtraPattern::mirrored_crosses:
		return extra_pattern_mirrored_crosses(state, presentation);
	}
	return ExtraPatternResult::unsupported;
}

[[nodiscard]] static bool extra_select_alternating_pattern(State &state) noexcept
{
	const bool second = (state.boss.phase_state & 1U) != 0U;
	switch(state.boss.statebytes[9U]) {
	case 0U:
		state.extra.pattern = second ? ExtraPattern::rotating_red_rings : ExtraPattern::random_red_spreads;
		return true;
	case 1U:
		state.extra.pattern = second ? ExtraPattern::rotating_blue_rings :
			ExtraPattern::gravity_balls_and_stacks;
		return true;
	case 2U:
		state.extra.pattern = second ? ExtraPattern::speedup_spreads :
			ExtraPattern::lasers_and_green_rings;
		return true;
	case 3U:
		state.extra.pattern = second ? ExtraPattern::aimed_cheetos : ExtraPattern::aimed_crosses;
		return true;
	default:
		return false;
	}
}

[[nodiscard]] static ExtraPatternResult extra_gather_and_pattern(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 48), 6U, 7U);
	if(boss.phase_frame == 48) {
		boss.sprite = 181U;
		presentation.sound(8U);
	}
	if(boss.phase_frame < 64) return ExtraPatternResult::running;
	const auto result = extra_pattern_update(state, presentation);
	if(result == ExtraPatternResult::completed) {
		boss.phase_frame = 0;
		boss.mode = 0U;
	}
	return result;
}

[[nodiscard]] static bool extra_pattern_phase_next(State &state, Presentation &presentation) noexcept
{
	switch(state.boss.statebytes[9U]) {
	case 0U:
		if(!extra_phase_next(state, presentation, 0U, 21000)) return false;
		state.extra.pattern = ExtraPattern::spreads_and_firewaves;
		return true;
	case 1U:
		if(!extra_phase_next(state, presentation, 1U, 15100)) return false;
		state.extra.pattern = ExtraPattern::bouncing_blue_rings;
		return true;
	case 2U:
		if(!extra_phase_next(state, presentation, 2U, 9600)) return false;
		state.extra.pattern = ExtraPattern::pingpong_lasers;
		return true;
	case 3U:
		if(!extra_phase_next(state, presentation, 3U, exalice_phase_last_hp)) return false;
		state.extra.pattern = ExtraPattern::mirrored_crosses;
		return true;
	default:
		return false;
	}
}

[[nodiscard]] static bool extra_colored_phase_next(State &state, Presentation &presentation) noexcept
{
	switch(state.boss.statebytes[9U]) {
	case 0U:
		if(!extra_phase_next(state, presentation, 1U, 18100)) return false;
		state.extra.pattern = ExtraPattern::gravity_balls_and_stacks;
		break;
	case 1U:
		if(!extra_phase_next(state, presentation, 2U, 12600)) return false;
		state.extra.pattern = ExtraPattern::lasers_and_green_rings;
		break;
	case 2U:
		if(!extra_phase_next(state, presentation, 3U, 6800)) return false;
		state.extra.pattern = ExtraPattern::aimed_crosses;
		break;
	case 3U:
		if(!extra_phase_next(state, presentation, 4U, 0)) return false;
		break;
	default:
		return false;
	}
	state.bullets.pellet_bottom_color = 9U;
	state.extra.overlay_patnum = 200U;
	++state.boss.statebytes[9U];
	return true;
}

[[nodiscard]] static bool extra_spreads_and_sweep(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.bullet_template;
	gather_add_only_3stack(state, static_cast<std::int16_t>(boss.phase_frame - 64), 6U, 7U);
	if(boss.phase_frame == 64) {
		boss.sprite = 181U;
		presentation.sound(8U);
		bullet.patnum = pat_bullet16_vector_blue;
		bullet.group = static_cast<std::uint8_t>(BulletGroup::spread);
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return false;
		bullet.angle = static_cast<std::uint8_t>(*angle);
		bullet.spread = 5U;
		bullet.spread_angle_delta = 21U;
		boss.statebytes[15U] = 0U;
		boss.pos.velocity.x = 0;
	}
	if(boss.phase_frame < 128) return true;
	if((boss.phase_frame % 2) == 0) {
		const auto damage = static_cast<std::int32_t>(exalice_phase_last_hp - boss.hp);
		bullet.speed = static_cast<std::uint8_t>((damage * 64) / exalice_phase_last_hp + 40);
		if(!bullets_add_regular(state)) return false;
		bullet.angle = byte_add(bullet.angle, 0x80);
		if(!bullets_add_regular(state)) return false;
		bullet.angle = byte_add(bullet.angle, 0x87);
		presentation.sound(9U);
	}
	const auto offset = pc98::masterlib::vector2(boss.statebytes[15U], boss.pos.velocity.x);
	boss.pos.cur.x = word_add(3072, offset.y);
	boss.statebytes[15U] = byte_add(boss.statebytes[15U], 2);
	const auto damage = static_cast<std::int32_t>(exalice_phase_last_hp - boss.hp);
	boss.pos.velocity.x = static_cast<std::int16_t>(
		(damage * 64 * subpixel_factor) / exalice_phase_last_hp
	);
	return true;
}

[[nodiscard]] static bool extra_alice_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	if(state.extra.invincibility_frames != 0U) --state.extra.invincibility_frames;
	state.shots.homing_target = boss.pos.cur;
	state.bullets.bullet_template.origin = boss.pos.cur;
	state.bullets.gather_template.center = boss.pos.cur;
	state.lasers.laser_template.coords.origin = boss.pos.cur;
	state.b6balls.b6ball_template.origin = boss.pos.cur;
	++boss.phase_frame;

	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 1) {
			boss.hp = exalice_hp_total;
			boss.phase_end_hp = 23800;
			state.bullets.gather_template.radius = 2048;
			state.bullets.gather_template.angle_delta = 2U;
			state.bullets.gather_template.ring_points = 8;
			boss.sprite = 180U;
			boss.sprite_left = 186U;
			boss.sprite_right = 184U;
			boss.sprite_stay = 180U;
			state.extra.invincibility_frames = 0U;
		}
		(void)boss_hittest_invincible(state, presentation);
		if(boss.phase_frame >= 192) {
			boss.phase_frame = 0;
			++boss.phase;
			presentation.sound(13U);
			state.palette[0U] = {};
			state.palette_changed = true;
			state.extra.overlay_patnum = 196U;
		}
		break;

	case 1U:
		(void)boss_hittest_invincible(state, presentation);
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.mode = 1U;
			boss.phase_state = 0U;
			boss.phase_frame = 0;
			state.extra.custom_render_active = true;
			boss.statebytes[15U] = 0U;
			state.extra.pattern = ExtraPattern::random_red_spreads;
			boss.statebytes[9U] = 0U;
		}
		break;

	case 2U:
		if(boss.mode == 0U) {
			const auto flight = extra_flystep_random(
				state, static_cast<std::int16_t>(boss.phase_frame - 32)
			);
			if(flight == FlyResult::unsupported) return false;
			if(flight == FlyResult::complete) {
				boss.phase_frame = 0;
				++boss.phase_state;
				++boss.mode;
				if(!extra_select_alternating_pattern(state)) return false;
				if(boss.phase_state >= 32U) return extra_pattern_phase_next(state, presentation);
			}
		} else if(boss.mode == 1U) {
			const auto result = extra_gather_and_pattern(state, presentation);
			if(result == ExtraPatternResult::unsupported) return false;
		}
		if(!extra_hittest(state, presentation)) break;
		if(!boss_score_bonus(state, presentation, 20U)) return false;
		if(!extra_pattern_phase_next(state, presentation)) return false;
		break;

	case 3U:
		if(state.palette[0U].r < 96U) {
			state.palette[0U].r = byte_add(state.palette[0U].r, 2);
			state.palette_changed = true;
		}
		[[fallthrough]];
	case 7U:
	case 11U:
	case 15U:
		if(boss_flystep_towards(state, 3072, 1024)) {
			boss.phase_frame = 0;
			++boss.phase;
			boss.mode = 1U;
		}
		(void)extra_hittest(state, presentation);
		break;

	case 4U:
		if(state.palette[0U].r < 96U) {
			state.palette[0U].r = byte_add(state.palette[0U].r, 2);
			state.palette_changed = true;
		}
		[[fallthrough]];
	case 8U:
	case 12U: {
		const auto result = extra_gather_and_pattern(state, presentation);
		if(result == ExtraPatternResult::unsupported) return false;
		if(boss.phase_frame > 4000) return extra_colored_phase_next(state, presentation);
		if(!extra_hittest(state, presentation)) break;
		if(!boss_score_bonus(state, presentation, 20U)) return false;
		if(!extra_colored_phase_next(state, presentation)) return false;
		break;
	}

	case 5U:
	case 6U:
		if(state.palette[0U].b < 96U) {
			state.palette[0U].b = byte_add(state.palette[0U].b, 2);
			state.palette[0U].r = byte_add(state.palette[0U].r, -2);
			state.palette_changed = true;
		}
		if(boss.phase == 5U) {
			if(boss_flystep_towards(state, 3072, 1024)) {
				boss.phase_frame = 0;
				++boss.phase;
				boss.mode = 1U;
			}
			(void)extra_hittest(state, presentation);
			break;
		}
		if(boss.mode == 0U) {
			const auto flight = extra_flystep_random(
				state, static_cast<std::int16_t>(boss.phase_frame - 32)
			);
			if(flight == FlyResult::unsupported) return false;
			if(flight == FlyResult::complete) {
				boss.phase_frame = 0;
				++boss.phase_state;
				++boss.mode;
				if(!extra_select_alternating_pattern(state)) return false;
				if(boss.phase_state >= 32U) return extra_pattern_phase_next(state, presentation);
			}
		} else if(boss.mode == 1U) {
			const auto result = extra_gather_and_pattern(state, presentation);
			if(result == ExtraPatternResult::unsupported) return false;
		}
		if(!extra_hittest(state, presentation)) break;
		if(!boss_score_bonus(state, presentation, 20U)) return false;
		if(!extra_pattern_phase_next(state, presentation)) return false;
		break;

	case 9U:
	case 10U:
		if(state.palette[0U].r < 48U) {
			state.palette[0U].r = byte_add(state.palette[0U].r, 1);
			state.palette[0U].b = byte_add(state.palette[0U].b, -1);
			state.palette_changed = true;
		}
		if(boss.phase == 9U) {
			if(boss_flystep_towards(state, 3072, 1024)) {
				boss.phase_frame = 0;
				++boss.phase;
				boss.mode = 1U;
			}
			(void)extra_hittest(state, presentation);
			break;
		}
		if(boss.mode == 0U) {
			const auto flight = extra_flystep_random(
				state, static_cast<std::int16_t>(boss.phase_frame - 32)
			);
			if(flight == FlyResult::unsupported) return false;
			if(flight == FlyResult::complete) {
				boss.phase_frame = 0;
				++boss.phase_state;
				++boss.mode;
				if(!extra_select_alternating_pattern(state)) return false;
				if(boss.phase_state >= 32U) return extra_pattern_phase_next(state, presentation);
			}
		} else if(boss.mode == 1U) {
			const auto result = extra_gather_and_pattern(state, presentation);
			if(result == ExtraPatternResult::unsupported) return false;
		}
		if(!extra_hittest(state, presentation)) break;
		if(!boss_score_bonus(state, presentation, 20U)) return false;
		if(!extra_pattern_phase_next(state, presentation)) return false;
		break;

	case 13U:
	case 14U:
		if(state.palette[0U].g < 64U) {
			if(state.palette[0U].r > 0U) state.palette[0U].r = byte_add(state.palette[0U].r, -1);
			state.palette[0U].g = byte_add(state.palette[0U].g, 1);
			if(state.palette[0U].b > 0U) state.palette[0U].b = byte_add(state.palette[0U].b, -1);
			state.palette_changed = true;
		}
		if(boss.phase == 13U) {
			if(boss_flystep_towards(state, 3072, 1024)) {
				boss.phase_frame = 0;
				++boss.phase;
				boss.mode = 1U;
			}
			(void)extra_hittest(state, presentation);
			break;
		}
		if(boss.mode == 0U) {
			const auto flight = extra_flystep_random(
				state, static_cast<std::int16_t>(boss.phase_frame - 32)
			);
			if(flight == FlyResult::unsupported) return false;
			if(flight == FlyResult::complete) {
				boss.phase_frame = 0;
				++boss.phase_state;
				++boss.mode;
				if(!extra_select_alternating_pattern(state)) return false;
				if(boss.phase_state >= 32U) return extra_pattern_phase_next(state, presentation);
			}
		} else if(boss.mode == 1U) {
			const auto result = extra_gather_and_pattern(state, presentation);
			if(result == ExtraPatternResult::unsupported) return false;
		}
		if(!extra_hittest(state, presentation)) break;
		if(!boss_score_bonus(state, presentation, 20U)) return false;
		if(!extra_pattern_phase_next(state, presentation)) return false;
		break;

	case 16U: {
		const auto result = extra_gather_and_pattern(state, presentation);
		if(result == ExtraPatternResult::unsupported) return false;
		if(result == ExtraPatternResult::completed) {
			++boss.phase_state;
			if(boss.phase_state > 8U) return extra_colored_phase_next(state, presentation);
		}
		if(!extra_hittest(state, presentation)) break;
		if(!boss_score_bonus(state, presentation, 20U)) return false;
		if(!extra_colored_phase_next(state, presentation)) return false;
		break;
	}

	case 17U:
		if(boss.statebytes[10U] == 0U) {
			if(state.palette[0U].g == 0U) {
				boss.statebytes[10U] = 1U;
			} else {
				state.palette[0U].g = byte_add(state.palette[0U].g, -1);
			}
		} else if(boss.statebytes[10U] == 1U) {
			state.palette[0U].r = byte_add(state.palette[0U].r, 2);
			state.palette[0U].g = byte_add(state.palette[0U].g, 1);
			state.palette[0U].b = byte_add(state.palette[0U].b, 2);
			if(state.palette[0U].r >= 128U) boss.statebytes[10U] = 2U;
		} else {
			state.palette[0U].r = byte_add(state.palette[0U].r, -2);
			state.palette[0U].g = byte_add(state.palette[0U].g, -1);
			state.palette[0U].b = byte_add(state.palette[0U].b, -2);
			if(state.palette[0U].r == 0U) boss.statebytes[10U] = 1U;
		}
		state.palette_changed = true;
		if(!extra_spreads_and_sweep(state, presentation)) return false;
		if(boss.phase_frame <= 5000) {
			if(!extra_hittest(state, presentation)) break;
			boss.phase_state = 1U;
		}
		boss_explode_small(state, presentation, 4U);
		boss.phase_frame = 0;
		boss.phase = phase_explode_small;
		state.extra.custom_render_active = false;
		state.bullets.zap_drop_point_items = false;
		break;

	default:
		if(!boss_defeat_update(state, presentation, 200U)) return false;
		break;
	}
	if(!stage6_cheetos_update(state)) return false;
	firewaves_update(state);
	presentation.boss_hp(boss.hp, exalice_hp_total);
	return true;
}

void extra_fights_setup(State &state) noexcept
{
	state.midboss_active = false;
	state.boss_active = false;
	// boss_reset() uninstalls bx_custombullets.cpp's renderer.  Firewaves are
	// separate retained BSS and deliberately survive this callback reset.
	state.extra.custom_render_active = false;
	extra_midboss_setup(state);
	state.boss.phase = 0U;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.phase_frame = 0;
	state.boss.damage_this_frame = 0U;
	state.boss.phase_timed_out = true;
	state.boss.pos.cur = {3072, 1024};
	state.boss.pos.prev = state.boss.pos.cur;
	state.boss.pos.velocity = {};
	state.boss.sprite = 180U;
	state.boss.hitbox_radius = {384, 384};
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
}

void extra_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}

bool extra_fights_update(State &state, Presentation &presentation) noexcept
{
	if(state.midboss_callback_active && !extra_midboss_update(state, presentation)) return false;
	if(state.boss_active && !extra_alice_update(state, presentation)) return false;
	return true;
}

void extra_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	if(state.extra.custom_render_active) firewaves_render_hittest(state);
	if(state.boss_active && state.boss.phase != phase_explode_big) {
		state.boss.damage_this_frame = 0U;
	}
	if(state.midboss_callback_active) extra_midboss_render_tail(state);
	fights_render_tail_explosions(state);
	(void)presentation;
}
} // namespace th05::core
  // namespace th05::core
