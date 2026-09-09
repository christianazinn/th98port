#include "th04/core/boss.hpp"

#include "th04/core/bullet.hpp"
#include "th04/core/b6ent.hpp"
#include "th04/core/item.hpp"
#include "th04/core/midboss4.hpp"
#include "th04/core/midboss_extra.hpp"
#include "th04/core/shot_collision.hpp"
#include "th04/core/shot_reimu_a.hpp"
#include "th04/core/state.hpp"
#include "th04/core/thicklaser.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_random.hpp"
#include "pc98/th0405_sparks.hpp"

#include <bit>
#include <cstdint>
#include <optional>

namespace th04::core {
namespace {

constexpr std::int16_t subpixel_factor = 16;
constexpr std::int16_t playfield_width = 384 * subpixel_factor;
constexpr std::int16_t playfield_height = 368 * subpixel_factor;
constexpr std::int16_t playfield_top = 16 * subpixel_factor;
constexpr std::int16_t vram_height = 400;
constexpr std::int16_t tile_size = 16;
constexpr std::uint8_t phase_explode_big = 254U;
constexpr std::uint8_t phase_none = 255U;
constexpr std::uint8_t pat_enemy_kill = 4U;
constexpr std::uint8_t pat_enemy_kill_last = 11U;
constexpr std::uint8_t pat_bullet16_white = 52U;
constexpr std::uint8_t pat_bullet16_directional_blue = 76U;
constexpr std::uint8_t pat_stage = 128U;
constexpr std::uint8_t rank_easy = 0U;
constexpr std::uint8_t rank_lunatic = 3U;
constexpr std::int16_t orange_hp = 3050;
constexpr std::int16_t boss_defeat_invincibility_frames = 255;
constexpr std::int16_t circle_frames = 16;
constexpr std::int16_t bullet_clear_frames = 20;
constexpr std::int16_t bonus_area = playfield_width / 3;
constexpr std::int16_t boss_bonus_value = 1280;
constexpr std::int16_t midboss_bonus_value = 1280;

enum class HitResult : std::uint8_t { none, ended, unsupported };
enum class OrangeMoveResult : std::uint8_t { moving, wait, fire, unsupported };

[[nodiscard]] constexpr std::int16_t word_add(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(left) + right
	));
}

[[nodiscard]] constexpr std::int16_t word_sub(
	std::int16_t left, std::int16_t right
) noexcept
{
	return word_add(left, std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		0U - std::bit_cast<std::uint16_t>(right)
	)));
}

[[nodiscard]] constexpr std::int16_t sar4(std::int16_t value) noexcept
{
	auto bits = static_cast<std::uint16_t>(
		std::bit_cast<std::uint16_t>(value) >> 4U
	);
	if(value < 0) bits = static_cast<std::uint16_t>(bits | 0xF000U);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] constexpr std::uint8_t byte_add(
	std::uint8_t left, std::int16_t right
) noexcept
{
	return static_cast<std::uint8_t>(static_cast<std::int16_t>(left) + right);
}

[[nodiscard]] bool tile_ring_set(
	State &state, std::int16_t source_x, std::int16_t source_y,
	std::uint16_t image_vo
) noexcept
{
	const auto x = sar4(source_x);
	auto y = sar4(word_add(playfield_top, source_y));
	y = word_add(y, state.scroll_line);
	if(y < 0) {
		y = word_add(y, vram_height);
	} else if(y >= vram_height) {
		y = word_add(y, -vram_height);
	}
	if(x < 0 || x >= 32 * tile_size || y < 0 || y >= vram_height) return false;
	state.tile_ring[(static_cast<std::size_t>(y / tile_size) * 32U) +
		static_cast<std::size_t>(x / tile_size)] = image_vo;
	return true;
}

void bullets_clear(State &state) noexcept
{
	if(state.resources.bullet_clear_time < bullet_clear_frames) {
		state.resources.bullet_clear_time = bullet_clear_frames;
	}
}

void circle_add(State &state, Point center, std::int16_t radius_delta) noexcept
{
	for(auto &circle : state.circles.pool) {
		if(circle.flag != circle_free) continue;
		circle.flag = circle_alive;
		circle.age = 0U;
		circle.center_x = sar4(center.x);
		circle.center_y = sar4(center.y);
		circle.radius_cur = static_cast<std::int16_t>(4 + (
			radius_delta > 0 ? 0 : circle_frames * -radius_delta
		));
		circle.radius_delta = radius_delta;
		return;
	}
}

void circle_add_shrinking(State &state, Point center) noexcept
{
	circle_add(state, center, -8);
}

void boss_explode(BossExplosion &explosion, const BossState &boss,
	std::uint8_t type) noexcept
{
	explosion.alive = true;
	explosion.age = 0U;
	explosion.center = boss.pos.cur;
	explosion.radius_cur = {8, 8};
	explosion.radius_delta = {176, 176};
	explosion.angle_offset = 0U;
	switch(type) {
	case 1U: explosion.angle_offset = 32U; break;
	case 2U: explosion.angle_offset = static_cast<std::uint8_t>(-32); break;
	case 3U: explosion.radius_delta = {208, 112}; break;
	case 4U: explosion.radius_delta = {112, 208}; break;
	default: break;
	}
}

void boss_explode_small(State &state, Presentation &presentation,
	std::uint8_t type) noexcept
{
	auto &explosion = state.boss_explosions.small[0].alive
		? state.boss_explosions.small[1]
		: state.boss_explosions.small[0];
	boss_explode(explosion, state.boss, type);
	presentation.sound(15U);
}

void boss_explode_big(State &state, Presentation &presentation,
	std::uint8_t type) noexcept
{
	boss_explode(state.boss_explosions.big, state.boss, type);
	presentation.sound(15U);
}

[[nodiscard]] bool shots_hit(
	State &state, Point center, Point radius, bool against_boss,
	std::uint16_t &damage
) noexcept
{
	state.shot_collision.center = center;
	state.shot_collision.radius = radius;
	state.shot_collision.bombing = state.bombing;
	state.shot_collision.against_boss = against_boss;
	state.shot_collision.stage_frame_mod2 = static_cast<std::uint8_t>(
		state.stage_frame & 1U
	);
	state.shot_collision.stage_frame_mod4 = static_cast<std::uint8_t>(
		state.stage_frame & 3U
	);
	state.shot_collision.score_delta = state.score.score_delta;
	const auto hit = shots_hittest(
		state.shots, state.shot_collision,
		pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset}, state.random
	);
	state.shot_collision.against_boss = false;
	if(!hit) return false;
	state.score.score_delta = state.shot_collision.score_delta;
	damage = hit->damage;
	return true;
}

[[nodiscard]] bool score_bonus(
	State &state, Presentation &presentation, Point center, std::uint16_t units,
	std::uint16_t value
) noexcept
{
	const auto score_delta = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(units) * static_cast<std::uint32_t>(value)
	);
	state.score.score_delta += score_delta;
	state.pointnum_times_2 = false;
	const auto left = word_sub(center.x, static_cast<std::int16_t>(bonus_area / 2));
	const auto top = word_sub(center.y, static_cast<std::int16_t>(bonus_area / 2));
	for(std::uint16_t i = 0U; i < units; ++i) {
		const auto x_offset = pc98::th0405::randring_next16_mod(
			state.random, static_cast<std::uint16_t>(bonus_area)
		);
		if(!x_offset) return false;
		auto x = word_add(left, std::bit_cast<std::int16_t>(*x_offset));
		if(x < 0) {
			x = 0;
		} else if(x > playfield_width) {
			x = playfield_width;
		}
		const auto y_offset = pc98::th0405::randring_next16_mod(
			state.random, static_cast<std::uint16_t>(bonus_area)
		);
		if(!y_offset) return false;
		presentation.pointnum(
			{x, word_add(top, std::bit_cast<std::int16_t>(*y_offset))}, value, false
		);
	}
	return true;
}

[[nodiscard]] bool midboss_score_bonus_impl(
	State &state, Presentation &presentation, std::uint16_t units
) noexcept
{
	return score_bonus(state, presentation, state.midboss.pos.cur, units,
		static_cast<std::uint16_t>(midboss_bonus_value));
}

[[nodiscard]] bool boss_score_bonus(
	State &state, Presentation &presentation, std::uint16_t units
) noexcept
{
	if(!score_bonus(state, presentation, state.boss.pos.cur, units,
		static_cast<std::uint16_t>(boss_bonus_value))) return false;
	state.boss.phase_timed_out = false;
	return true;
}

[[nodiscard]] HitResult boss_hittest(State &state, Presentation &presentation) noexcept
{
	++state.boss.phase_frame;
	std::uint16_t damage{};
	if(!shots_hit(state, state.boss.pos.cur, state.boss.hitbox_radius, true, damage)) {
		return HitResult::unsupported;
	}
	if(damage != 0U) presentation.sound(4U);
	state.boss.damage_this_frame = static_cast<std::uint8_t>(damage);
	state.boss.hp = static_cast<std::int16_t>(
		static_cast<int>(state.boss.hp) - static_cast<int>(state.boss.damage_this_frame)
	);
	return state.boss.hp <= state.boss.phase_end_hp ? HitResult::ended : HitResult::none;
}

[[nodiscard]] bool boss_hittest_invincible(
	State &state, Presentation &presentation, Point radius
) noexcept
{
	std::uint16_t damage{};
	if(!shots_hit(state, state.boss.pos.cur, radius, true, damage)) {
		return false;
	}
	if(damage != 0U) presentation.sound(10U);
	return true;
}

[[nodiscard]] bool boss_items_drop(State &state, Presentation &presentation) noexcept
{
	const auto full_power_drop = state.power <= 123U;
	const auto small_power_drop = state.power < 128U;
	const auto left = word_sub(state.boss.pos.cur.x, 1024);
	const auto top = word_sub(state.boss.pos.cur.y, 1024);
	for(std::uint16_t i = 0U; i < 5U; ++i) {
		const auto x = pc98::th0405::randring_next16_mod(state.random, 2048U);
		if(!x) return false;
		const auto y = pc98::th0405::randring_next16_mod(state.random, 2048U);
		if(!y) return false;
		const auto type = !small_power_drop ? ItemType::point :
			(full_power_drop && i == 2U ? ItemType::big_power : ItemType::power);
		items_add(state, presentation, {
			word_add(left, std::bit_cast<std::int16_t>(*x)),
			word_add(top, std::bit_cast<std::int16_t>(*y)),
		}, type);
	}
	return true;
}

[[nodiscard]] bool boss_phase_next(State &state, Presentation &presentation,
	std::uint8_t explosion_type, std::int16_t next_end_hp
) noexcept
{
	if(explosion_type != 0U) {
		boss_explode_small(state, presentation, explosion_type);
		if(!state.boss.phase_timed_out) {
			bullets_clear(state);
			if(!boss_items_drop(state, presentation)) return false;
		}
	}
	state.boss.phase_timed_out = true;
	state.boss.phase = static_cast<std::uint8_t>(state.boss.phase + 1U);
	state.boss.phase_frame = 0;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.hp = state.boss.phase_end_hp;
	state.boss.phase_end_hp = next_end_hp;
	return true;
}

[[nodiscard]] OrangeMoveResult orange_move_schedule(State &state) noexcept
{
	auto &boss = state.boss;
	gather_add_only_3stack(
		state, static_cast<std::int16_t>(boss.phase_frame - 70), 7U, 6U
	);
	if(boss.phase_frame < 16) return OrangeMoveResult::wait;
	if(boss.phase_frame == 16) {
		const auto x = pc98::th0405::randring_next16_mod(state.random, 5120U);
		if(!x) return OrangeMoveResult::unsupported;
		const auto y = pc98::th0405::randring_next16_mod(state.random, 1536U);
		if(!y) return OrangeMoveResult::unsupported;
		const auto target_x = word_add(std::bit_cast<std::int16_t>(*x), 512);
		const auto target_y = word_add(std::bit_cast<std::int16_t>(*y), 1024);
		boss.pos.velocity.x = static_cast<std::int16_t>(
			(word_sub(target_x, boss.pos.cur.x)) / 64
		);
		boss.pos.velocity.y = static_cast<std::int16_t>(
			(word_sub(target_y, boss.pos.cur.y)) / 64
		);
		state.bullets.gather_template.radius = 1536;
		state.bullets.gather_template.ring_points = 8;
	}
	if(boss.phase_frame < 70) {
		pc98::th0405::update_motion(boss.pos);
		return OrangeMoveResult::moving;
	}
	if(boss.phase_frame == 70) {
		circle_add_shrinking(state, boss.pos.cur);
		state.circles_color = 15U;
		return OrangeMoveResult::wait;
	}
	return boss.phase_frame < 86 ? OrangeMoveResult::wait : OrangeMoveResult::fire;
}

[[nodiscard]] bool orange_pattern_1(State &state, Presentation &presentation) noexcept
{
	if(orange_move_schedule(state) == OrangeMoveResult::unsupported) return false;
	auto &boss = state.boss;
	if(boss.phase_frame < 86) return true;
	if(boss.phase_frame == 86) {
		const auto direction = pc98::th0405::randring_next16_and(state.random, 1U);
		if(!direction) return false;
		boss.phase_state = static_cast<std::uint8_t>(*direction);
		boss.angle = boss.phase_state == 0U ? 0U : 128U;
		boss.phase_state = boss.phase_state == 0U ? 11U : static_cast<std::uint8_t>(-11);
	}
	if((state.stage_frame & 1U) == 0U) {
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.origin = boss.pos.cur;
		bullet.group = BulletGroup::ring;
		bullet.count = 2U;
		bullet.angle = boss.angle;
		bullet.speed = 30U;
		if(!bullets_add(state)) return false;
		bullet.angle = byte_add(bullet.angle, 5);
		bullet.speed = 20U;
		if(!bullets_add(state)) return false;
		boss.angle = byte_add(boss.angle, std::bit_cast<std::int8_t>(boss.phase_state));
		presentation.sound(9U);
	}
	if(boss.phase_frame >= 118) boss.mode = 0U;
	return true;
}

[[nodiscard]] bool orange_pattern_2(State &state, Presentation &presentation) noexcept
{
	if(orange_move_schedule(state) == OrangeMoveResult::unsupported) return false;
	auto &boss = state.boss;
	if(boss.phase_frame < 86) return true;
	if(boss.phase_frame == 86) {
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, boss.pos.cur.y),
			word_sub(state.player_pos.cur.x, boss.pos.cur.x)
		);
		if(!angle) return false;
		boss.angle = *angle;
		state.bullets.template_.speed = 16U;
	}
	if((state.stage_frame & 3U) == 0U) {
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(
			BulletSpawnType::bullet16_cloud_forwards
		);
		bullet.patnum = pat_bullet16_white;
		bullet.origin = boss.pos.cur;
		bullet.group = BulletGroup::spread;
		bullet.count = 3U;
		bullet.delta = 12U;
		bullet.special_motion = BulletSpecialMotion::none;
		bullet.angle = byte_add(boss.angle, -32);
		if(!bullet_template_tune(state) || !bullets_add(state, true, false)) return false;
		bullet.angle = byte_add(bullet.angle, 64);
		if(!bullets_add(state, true, false)) return false;
		presentation.sound(3U);
		bullet.speed = byte_add(bullet.speed, 6);
	}
	if(boss.phase_frame >= 118) boss.mode = 0U;
	return true;
}

[[nodiscard]] bool orange_pattern_3(State &state, Presentation &presentation) noexcept
{
	if(orange_move_schedule(state) == OrangeMoveResult::unsupported) return false;
	auto &boss = state.boss;
	if(boss.phase_frame < 86) return true;
	if((state.stage_frame & 7U) == 0U) {
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = pat_bullet16_white;
		bullet.origin = boss.pos.cur;
		bullet.group = BulletGroup::ring_aimed;
		bullet.count = 16U;
		bullet.speed = 32U;
		bullet.angle = 0U;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		presentation.sound(9U);
	}
	if(boss.phase_frame >= 118) boss.mode = 0U;
	return true;
}

[[nodiscard]] bool orange_pattern_4(State &state, Presentation &presentation) noexcept
{
	if(orange_move_schedule(state) == OrangeMoveResult::unsupported) return false;
	auto &boss = state.boss;
	if(boss.phase_frame < 86) return true;
	if((state.stage_frame & 7U) == 0U) {
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.origin.x = word_sub(boss.pos.cur.x, 512);
		bullet.origin.y = boss.pos.cur.y;
		bullet.group = BulletGroup::ring;
		bullet.count = 8U;
		bullet.speed = 30U;
		bullet.angle = byte_add(bullet.angle, 8);
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		bullet.origin.x = word_add(bullet.origin.x, 1024);
		if(!bullets_add(state)) return false;
		presentation.sound(9U);
	}
	if(boss.phase_frame >= 118) boss.mode = 0U;
	return true;
}

[[nodiscard]] bool orange_pattern_5(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 1) {
		boss.pos.velocity.x = boss.pos.cur.x < 3072 ? 16 : -16;
		boss.angle = 0U;
	}
	// polar(0, TO_SP(1), SinTable8[angle]) is the source Q8 multiply followed
	// by a signed arithmetic shift.
	boss.pos.velocity.y = pc98::masterlib::vector_component(
		16, pc98::masterlib::sine8(boss.angle)
	);
	if(boss.pos.cur.y >= 1536) boss.pos.velocity.y = -16;
	if(boss.pos.cur.y <= 768) boss.pos.velocity.y = 16;
	boss.angle = byte_add(boss.angle, 2);
	const auto x = pc98::th0405::update_motion(boss.pos).x;
	if(x <= 512 || x >= 5632) boss.pos.velocity.x = static_cast<std::int16_t>(
		-1 * boss.pos.velocity.x
	);
	if((state.stage_frame & 3U) != 0U ||
		(state.rank == rank_easy && (state.stage_frame & 7U) == 0U)) return true;
	auto &bullet = state.bullets.template_;
	bullet.patnum = pat_bullet16_white;
	bullet.origin.x = word_sub(boss.pos.cur.x, 512);
	bullet.origin.y = boss.pos.cur.y;
	bullet.group = BulletGroup::random_angle;
	bullet.count = boss.hp <= 700 ? (state.rank < rank_lunatic ? 2U : 4U) : 1U;
	bullet.speed = 32U;
	const auto left_type = pc98::th0405::randring_next16_and(state.random, 1U);
	if(!left_type) return false;
	bullet.spawn_type = *left_type == 0U
		? static_cast<std::uint8_t>(BulletSpawnType::pellet)
		: static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
	const auto left_angle = pc98::th0405::randring_next16(state.random);
	if(!left_angle) return false;
	bullet.angle = static_cast<std::uint8_t>(*left_angle);
	if(!bullet_template_tune(state) || !bullets_add(state)) return false;
	bullet.origin.x = word_add(bullet.origin.x, 1024);
	const auto right_type = pc98::th0405::randring_next16_and(state.random, 1U);
	if(!right_type) return false;
	bullet.spawn_type = *right_type == 0U
		? static_cast<std::uint8_t>(BulletSpawnType::pellet)
		: static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
	const auto right_angle = pc98::th0405::randring_next16(state.random);
	if(!right_angle) return false;
	bullet.angle = static_cast<std::uint8_t>(*right_angle);
	return bullets_add(state);
}

void orange_hover(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.pos.cur.x < 3056) boss.pos.velocity.x = 24;
	else if(boss.pos.cur.x > 3088) boss.pos.velocity.x = -24;
	if(boss.pos.cur.y < 1264) boss.pos.velocity.y = 12;
	else if(boss.pos.cur.y > 1296) boss.pos.velocity.y = -12;
	pc98::th0405::update_motion(boss.pos);
}

[[nodiscard]] bool orange_pattern_6(State &state) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame == 96 || boss.phase_frame == 160 ||
		boss.phase_frame == 224 || boss.phase_frame == 288) {
		state.bullets.gather_template.center = bullet.origin;
		gather_add_only(state);
	}
	if(boss.phase_frame == 112 || boss.phase_frame == 160 ||
		boss.phase_frame == 240 || boss.phase_frame == 304) {
		circle_add_shrinking(state, bullet.origin);
		state.circles_color = 15U;
	}
	if((state.stage_frame & 3U) != 0U) return true;
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
	bullet.group = BulletGroup::single;
	boss.angle = byte_add(boss.angle, -7);
	bullet.speed = 32U;
	bullet.angle = boss.angle;
	if(!bullet_template_tune(state) || !bullets_add(state, false, true)) return false;
	if(boss.phase_frame < 128) return true;
	if(boss.phase_frame >= 320) {
		bullet.angle = byte_add(bullet.angle, 64);
		if(!bullets_add(state, false, true)) return false;
		bullet.angle = byte_add(bullet.angle, 64);
		if(!bullets_add(state, false, true)) return false;
		bullet.angle = byte_add(bullet.angle, 64);
		if(!bullets_add(state, false, true)) return false;
		bullet.speed = byte_add(bullet.speed, 16);
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = pat_bullet16_white;
		bullet.angle = byte_add(static_cast<std::uint8_t>(-bullet.angle), -32);
		if(!bullets_add(state, false, true)) return false;
		bullet.angle = byte_add(bullet.angle, 128);
		return bullets_add(state, false, true);
	}
	if(boss.phase_frame >= 192) {
		if(boss.phase_frame >= 256) {
			bullet.angle = byte_add(bullet.angle, 64);
			if(!bullets_add(state, false, true)) return false;
		}
		bullet.angle = byte_add(bullet.angle, 64);
		if(!bullets_add(state, false, true)) return false;
	}
	bullet.angle = byte_add(bullet.angle, 64);
	if(!bullets_add(state, false, true)) return false;
	return true;
}

[[nodiscard]] bool orange_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	state.bullets.gather_template.center = boss.pos.cur;
	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 0) {
			boss.hp = orange_hp;
			boss.phase_end_hp = 1950;
			state.palette[0] = {0U, 0U, 96U};
			state.palette_changed = true;
		}
		++boss.phase_frame;
		if(boss.phase_frame == 192) {
			++boss.sprite;
			++boss.sprite;
			presentation.sound(8U);
			state.bullets.gather_template.center = {
				word_add(boss.pos.cur.x, 128), word_sub(boss.pos.cur.y, 640)
			};
			state.bullets.gather_template.radius = 5120;
			state.bullets.gather_template.ring_points = 32;
			state.bullets.gather_template.angle_delta = 3U;
			state.bullets.gather_template.col = 7U;
		} else if(boss.phase_frame > 320) {
			if(boss.phase_frame == 336) state.bullets.gather_template.col = 6U;
			if((boss.phase_frame & 7) == 0) gather_add_only(state);
			if(boss.phase_frame >= 352) {
				++boss.phase;
				boss.phase_frame = 0;
				presentation.sound(13U);
				boss.statebytes[15] = 0U;
				boss.statebytes[14] = 0xFFU;
			}
		}
		if(!boss_hittest_invincible(state, presentation, {256, 256})) return false;
		break;

	case 1U:
		++boss.phase_frame;
		if(boss.phase_frame >= 32) {
			state.bullets.gather_template.radius = 1024;
			state.bullets.gather_template.angle_delta = 2U;
			state.bullets.gather_template.ring_points = 8;
			boss.phase = 2U;
			boss.phase_frame = 0;
			boss.mode = 0U;
			++boss.sprite;
			++boss.sprite;
			auto &bullet = state.bullets.template_;
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
			bullet.origin = boss.pos.cur;
			bullet.group = BulletGroup::ring;
			bullet.count = 16U;
			bullet.speed = 64U;
			bullet.angle = 0U;
			if(!bullet_template_tune(state)) return false;
			for(int i = 0; i < 3; ++i) {
				if(!bullets_add(state)) return false;
				bullet.speed = byte_add(bullet.speed, -16);
			}
			presentation.sound(6U);
		}
		if(!boss_hittest_invincible(state, presentation, {256, 256})) return false;
		break;

	case 2U:
		if(boss.mode == 0U) {
			boss.phase_frame = 0;
			for(unsigned attempts = 0U; attempts < 256U; ++attempts) {
				const auto mode = pc98::th0405::randring_next16_and(state.random, 3U);
				if(!mode) return false;
				boss.mode = byte_add(static_cast<std::uint8_t>(*mode), 1);
				if(boss.statebytes[14] != boss.mode) break;
				if(attempts == 255U) return false;
			}
			boss.statebytes[14] = boss.mode;
			++boss.statebytes[15];
			if(boss.statebytes[15] >= 16U) {
				if(!boss_phase_next(state, presentation, 1U, 450)) return false;
				state.palette[0].r = 112U;
				state.palette[0].b = 112U;
				state.palette_changed = true;
				break;
			}
		}
		if(boss.phase == 2U) {
			bool pattern_ok = true;
			switch(boss.mode) {
			case 1U: pattern_ok = orange_pattern_1(state, presentation); break;
			case 2U: pattern_ok = orange_pattern_2(state, presentation); break;
			case 3U: pattern_ok = orange_pattern_3(state, presentation); break;
			case 4U: pattern_ok = orange_pattern_4(state, presentation); break;
			default: return false;
			}
			if(!pattern_ok) return false;
			const auto hit = boss_hittest(state, presentation);
			if(hit == HitResult::unsupported) return false;
			if(hit == HitResult::ended) {
				if(!boss_score_bonus(state, presentation, 5U) ||
					!boss_phase_next(state, presentation, 1U, 450)) return false;
				state.palette[0].r = 112U;
				state.palette[0].b = 112U;
				state.palette_changed = true;
			}
		}
		break;

	case 3U:
		if(boss.mode == 0U) {
			if(boss.phase_frame > 128) {
				boss.phase_frame = 0;
				boss.mode = 1U;
			}
		} else if(boss.mode == 1U && !orange_pattern_5(state)) {
			return false;
		}
		if(boss.phase_frame <= 1500) {
			const auto hit = boss_hittest(state, presentation);
			if(hit == HitResult::unsupported) return false;
			if(hit == HitResult::none) break;
			if(!boss_score_bonus(state, presentation, 5U)) return false;
		}
		if(!boss_phase_next(state, presentation, 1U, 0)) return false;
		boss.sprite = byte_add(boss.sprite, 4);
		state.palette[0].r = 144U;
		state.palette[0].b = 32U;
		state.palette_changed = true;
		state.bullets.gather_template.col = 9U;
		break;

	case 4U: {
		auto &bullet = state.bullets.template_;
		bullet.origin = {word_add(boss.pos.cur.x, 128), word_sub(boss.pos.cur.y, 256)};
		if(boss.mode == 0U) {
			if(boss.phase_frame == 96) {
				state.bullets.gather_template.center = bullet.origin;
				gather_add_only(state);
			}
			if(boss.phase_frame == 112) {
				circle_add_shrinking(state, bullet.origin);
				state.circles_color = 15U;
			}
			if(boss.phase_frame > 128) {
				boss.phase_frame = 0;
				boss.mode = 1U;
			}
			orange_hover(state);
		} else if(boss.mode == 1U) {
			if(!orange_pattern_6(state)) return false;
		} else {
			return false;
		}
		if(boss.phase_frame <= 600) {
			const auto hit = boss_hittest(state, presentation);
			if(hit == HitResult::unsupported) return false;
			if(hit == HitResult::none) break;
		}
		boss.phase_state = boss.phase_frame <= 600 ? 1U : 0U;
		boss_explode_small(state, presentation, 3U);
		++boss.phase;
		boss.phase_frame = 0;
		boss.mode = 0U;
		if(!pc98::th0405::sparks_add_circle(
			pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
			boss.pos.cur, 128, 48U
		)) return false;
		break;
	}

	case 5U:
		orange_hover(state);
		++boss.phase_frame;
		if(boss.phase_frame == 16) boss_explode_small(state, presentation, 4U);
		if(boss.phase_frame == 32) {
			boss_explode_big(state, presentation, 0U);
			boss.phase = phase_explode_big;
			state.bullets.zap.set_active(boss.phase_state != 0U);
			if(boss.phase_state != 0U && !boss_score_bonus(state, presentation, 10U)) {
				return false;
			}
			boss.sprite = pat_enemy_kill;
			boss.phase_frame = 0;
			presentation.sound(12U);
			state.palette[0].r = 0U;
			state.palette[0].b = 0U;
			state.palette_changed = true;
			state.player_invincibility_time = boss_defeat_invincibility_frames;
		}
		break;

	case phase_explode_big:
		if(boss.phase_frame < 12) {
			state.playfield_shake_x = (state.stage_frame & 1U) == 0U ? -4 : 4;
			state.playfield_shake_y = (state.stage_frame & 3U) <= 1U ? -4 : 4;
		}
		state.bullets.slowdown_factor = 2U;
		++boss.phase_frame;
		if((boss.phase_frame % 8) == 0) {
			++boss.sprite;
			if(boss.sprite >= static_cast<std::uint8_t>(pat_enemy_kill_last + 1U)) {
				boss.phase = phase_none;
				boss.phase_frame = 0;
				state.bombing_disabled = true;
			}
		}
		return true;

	case phase_none:
		// boss_defeat_update() enters dialog/clear/overlay/next-stage state here.
		// That source branch is intentionally not a fake successful transition.
		state.palette_tone = 60U;
		state.palette_changed = true;
		state.stage_clear_requested = true;
		return true;

	default:
		return false;
	}
	state.reimu_a.homing_target = state.boss.pos.cur;
	return true;
}

[[nodiscard]] bool midboss_fan(State &state, Presentation &presentation) noexcept
{
	auto &midboss = state.midboss;
	if(midboss.pos.cur.y >= 4096) return true;
	if(midboss.phase_frame == 1) midboss.stage1_fan_angle = 1U;
	if((midboss.phase_frame % 8) != 0) return true;
	auto &bullet = state.bullets.template_;
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
	bullet.patnum = pat_bullet16_directional_blue;
	bullet.angle = midboss.stage1_fan_angle;
	bullet.speed = 32U;
	bullet.group = BulletGroup::single;
	bullet.special_motion = BulletSpecialMotion::none;
	if(!bullet_template_tune(state) || !bullets_add(state, true, false)) return false;
	bullet.angle = byte_add(128U, -static_cast<std::int16_t>(midboss.stage1_fan_angle));
	if(!bullets_add(state, true, false)) return false;
	midboss.stage1_fan_angle = byte_add(midboss.stage1_fan_angle, 12);
	(void)presentation;
	return true;
}

[[nodiscard]] bool midboss_invincible_hittest(
	State &state, Presentation &presentation, Point radius
) noexcept
{
	std::uint16_t damage{};
	if(!shots_hit(state, state.midboss.pos.cur, radius, false, damage)) return false;
	if(damage != 0U) presentation.sound(10U);
	return true;
}

[[nodiscard]] bool midboss_update(State &state, Presentation &presentation) noexcept
{
	auto &midboss = state.midboss;
	if(midboss.phase == 0U) {
		midboss.pos.velocity.y = -16;
		pc98::th0405::update_motion(midboss.pos);
		if(!tile_ring_set(state, word_sub(midboss.pos.cur.x, 256),
			word_sub(midboss.pos.cur.y, 256), 40U) ||
			!tile_ring_set(state, midboss.pos.cur.x,
			word_sub(midboss.pos.cur.y, 256), 41U) ||
			!tile_ring_set(state, word_sub(midboss.pos.cur.x, 256),
			midboss.pos.cur.y, 56U) ||
			!tile_ring_set(state, midboss.pos.cur.x, midboss.pos.cur.y, 57U)) return false;
		++midboss.phase_frame;
		if(midboss.phase_frame >= 288) {
			midboss.phase = 1U;
			midboss.phase_frame = 0;
			midboss.pos.velocity.y = 2;
			if(!tile_ring_set(state, word_sub(midboss.pos.cur.x, 256),
				word_sub(midboss.pos.cur.y, 256), 42U) ||
				!tile_ring_set(state, midboss.pos.cur.x,
				word_sub(midboss.pos.cur.y, 256), 43U) ||
				!tile_ring_set(state, word_sub(midboss.pos.cur.x, 256),
				midboss.pos.cur.y, 58U) ||
				!tile_ring_set(state, midboss.pos.cur.x, midboss.pos.cur.y, 59U)) return false;
			midboss.sprite = 136U;
			midboss.pos.cur.y = word_sub(midboss.pos.cur.y, 64);
			midboss.pos.cur.y = word_add(midboss.pos.cur.y,
				static_cast<std::int16_t>(state.scroll_subpixel_line));
			const auto sparks = pc98::th0405::sparks_add_circle(
				pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
				midboss.pos.cur, 48, 32U
			);
			if(!sparks) return false;
			presentation.sound(9U);
		}
		return true;
	}
	if(midboss.phase == 1U) {
		pc98::th0405::update_motion(midboss.pos);
		state.reimu_a.homing_target = midboss.pos.cur;
		++midboss.phase_frame;
		if(midboss.sprite < 139U) {
			if((midboss.phase_frame % 8) == 0) ++midboss.sprite;
		} else if(midboss.phase_frame >= 96) {
			midboss.phase = 2U;
			midboss.phase_frame = 0;
			midboss.sprite = 140U;
			midboss.pos.cur.y = word_sub(midboss.pos.cur.y, 256);
			const auto sparks = pc98::th0405::sparks_add_circle(
				pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
				midboss.pos.cur, 48, 32U
			);
			if(!sparks) return false;
			presentation.sound(9U);
		}
		return midboss_invincible_hittest(state, presentation, {256, 192});
	}
	if(midboss.phase == 2U) {
		pc98::th0405::update_motion(midboss.pos);
		state.reimu_a.homing_target = midboss.pos.cur;
		++midboss.phase_frame;
		if(midboss.sprite < 146U) {
			if((midboss.phase_frame % 8) == 0) {
				midboss.sprite = byte_add(midboss.sprite, 2);
			}
		} else {
			midboss.phase = 3U;
			midboss.phase_frame = 0;
		}
		return midboss_invincible_hittest(state, presentation, {384, 256});
	}
	if(midboss.phase == 3U) {
		pc98::th0405::update_motion(midboss.pos);
		std::uint16_t damage{};
		if(state.std_data.scroll_speed > 2U) goto leaves;
		state.reimu_a.homing_target = midboss.pos.cur;
		++midboss.phase_frame;
		if(!shots_hit(state, midboss.pos.cur, {384, 256}, false, damage)) return false;
		state.bullets.template_.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		state.bullets.template_.origin = {
			midboss.pos.cur.x, word_sub(midboss.pos.cur.y, 16)
		};
		if(!midboss_fan(state, presentation)) return false;
		if(damage == 0U) return true;
		midboss.hp = static_cast<std::int16_t>(
			static_cast<int>(midboss.hp) - static_cast<int>(damage)
		);
		if(midboss.hp > 0) {
			midboss.damage_this_frame = 1U;
			presentation.sound(4U);
			return true;
		}
		state.bullets.zap.set_active(true);
		if(!midboss_score_bonus(state, presentation, 5U)) return false;
leaves:
		midboss.phase = phase_explode_big;
		midboss.sprite = pat_enemy_kill;
		midboss.phase_frame = 0;
		midboss.pos.velocity.y = 0;
		if(!pc98::th0405::sparks_add_circle(
			pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
			midboss.pos.cur, 128, 48U
		)) return false;
		presentation.sound(12U);
		state.std_data.scroll_speed = 4U;
		return true;
	}
	if(midboss.phase == phase_explode_big) {
		if(midboss.phase_frame == 0) state.playfield_shake_anim_time = 12;
		++midboss.phase_frame;
		if((midboss.phase_frame % 16) == 0) {
			++midboss.sprite;
			if(midboss.sprite >= static_cast<std::uint8_t>(pat_enemy_kill_last + 1U)) {
				midboss.phase = phase_none;
			}
		}
		return true;
	}
	state.midboss_active = false;
	midboss.hp = 0;
	return true;
}

void explosion_update(BossExplosion &explosion) noexcept
{
	explosion.radius_cur.x = word_add(explosion.radius_cur.x, explosion.radius_delta.x);
	explosion.radius_cur.y = word_add(explosion.radius_cur.y, explosion.radius_delta.y);
	explosion.age = static_cast<std::uint8_t>(explosion.age + 1U);
	if(explosion.age >= 32U) explosion.alive = false;
}

} // namespace

void stage1_fights_setup(State &state) noexcept
{
	state.midboss.frames_until = 3100U;
	state.midboss.pos.cur = {3072, 5888};
	state.midboss.pos.prev = state.midboss.pos.cur;
	state.midboss.pos.velocity = {0, 16};
	state.midboss.hp = 800;
	state.midboss_active = false;

	state.boss_active = false;
	state.boss.phase = 0U;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.phase_frame = 0;
	state.boss.pos.cur = {3072, 640};
	state.boss.pos.prev = state.boss.pos.cur;
	state.boss.pos.velocity = {};
	state.boss.damage_this_frame = 0U;
	state.boss.phase_timed_out = true;
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
	state.boss.sprite = pat_stage;
	state.boss.hitbox_radius = {384, 256};
	state.palette[0].r = 255U;
	state.palette[0].g = 255U;
}

void stage1_midboss_activate(State &state) noexcept
{
	if(state.midboss.frames_until != state.stage_frame) return;
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
	if(state.midboss_active && !midboss_update(state, presentation)) return false;
	if(state.boss_active && !orange_update(state, presentation)) return false;
	return true;
}

void circles_update(State &state, Presentation &presentation) noexcept
{
	for(auto &circle : state.circles.pool) {
		if(circle.flag == circle_remove) {
			circle.flag = circle_free;
			continue;
		}
		if(circle.flag != circle_alive) continue;
		circle.radius_cur = word_add(circle.radius_cur, circle.radius_delta);
		circle.age = static_cast<std::uint8_t>(circle.age + 1U);
		if(circle.age > circle_frames) circle.flag = circle_remove;
	}
	(void)presentation;
}

void stage1_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	if(state.midboss_active && state.midboss.phase == 3U &&
		state.midboss.damage_this_frame != 0U) {
		state.midboss.damage_this_frame = 0U;
	}
	if(state.boss_active) {
		for(auto &explosion : state.boss_explosions.small) {
			if(explosion.alive) explosion_update(explosion);
		}
		auto &big = state.boss_explosions.big;
		if(big.alive) {
			explosion_update(big);
			++state.boss_explosions.big_frame;
			const auto frame = state.boss_explosions.big_frame;
			if(frame < 8 && (frame & 1) != 0) {
				state.palette_tone = 150U;
			} else if(frame >= 8 && frame < 16) {
				state.palette_tone = static_cast<std::uint16_t>(196 - frame * 6);
			} else {
				state.palette_tone = 100U;
			}
			state.palette_changed = true;
		} else {
			state.boss_explosions.big_frame = 0;
		}
	}
	(void)presentation;
}

namespace {

constexpr std::uint8_t pat_bullet16_outlined_ball_blue = 55U;
constexpr std::uint8_t pat_bullet16_ball_blue = 57U;
constexpr std::uint8_t stage2_white = 15U;

void circle_add_growing(State &state, Point center) noexcept
{
	circle_add(state, center, 8);
}

[[nodiscard]] bool midboss_hittest_damage(
	State &state, Presentation &presentation, std::uint16_t &damage
) noexcept
{
	if(!shots_hit(state, state.midboss.pos.cur, {384, 384}, false, damage)) {
		return false;
	}
	if(damage != 0U) presentation.sound(10U);
	return true;
}

[[nodiscard]] bool stage2_midboss_pattern_0(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	if((midboss.phase_frame % 8) == 0) {
		presentation.sound(3U);
		auto &bullet = state.bullets.template_;
		bullet.angle = byte_add(static_cast<std::uint8_t>(
			static_cast<unsigned>(midboss.stage2_direction) << 5U
		), 32);
		if(((midboss.phase_frame / 8) & 1) != 0) {
			bullet.spawn_type = static_cast<std::uint8_t>(
				BulletSpawnType::bullet16_cloud_forwards
			);
			bullet.patnum = pat_bullet16_ball_blue;
			bullet.group = BulletGroup::single;
			bullet.speed = 42U;
			if(!bullet_template_tune(state) || !bullets_add(state)) return false;
			bullet.delta = 15U;
		} else {
			bullet.delta = 10U;
		}
		bullet.group = BulletGroup::spread;
		bullet.count = 6U;
		bullet.speed = 36U;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
	}
	if(midboss.phase_frame >= 64) {
		midboss.phase_frame = 0;
		midboss.stage2_pattern = 0xFFU;
	}
	return true;
}

[[nodiscard]] bool stage2_midboss_pattern_1(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	if((midboss.phase_frame % 8) == 0) {
		presentation.sound(3U);
		auto &bullet = state.bullets.template_;
		bullet.angle = byte_add(static_cast<std::uint8_t>(
			static_cast<unsigned>(midboss.stage2_direction) << 5U
		), 32);
		bullet.speed = static_cast<std::uint8_t>((midboss.phase_frame / 2) + 32);
		bullet.group = BulletGroup::ring;
		bullet.count = 32U;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
	}
	if(midboss.phase_frame >= 32) {
		midboss.phase_frame = 0;
		midboss.stage2_pattern = 0xFFU;
	}
	return true;
}

[[nodiscard]] bool stage2_midboss_pattern_2(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	if((midboss.phase_frame % 4) == 0) {
		presentation.sound(3U);
		auto &bullet = state.bullets.template_;
		bullet.speed = 64U;
		bullet.group = BulletGroup::single;
		bullet.angle = byte_add(static_cast<std::uint8_t>(
			static_cast<unsigned>(midboss.stage2_direction) << 5U
		), 8);
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		bullet.angle = byte_add(static_cast<std::uint8_t>(
			static_cast<unsigned>(midboss.stage2_direction) << 5U
		), 16);
		if(!bullets_add(state)) return false;
		bullet.angle = byte_add(static_cast<std::uint8_t>(
			static_cast<unsigned>(midboss.stage2_direction) << 5U
		), 56);
		if(!bullets_add(state)) return false;
		bullet.angle = byte_add(static_cast<std::uint8_t>(
			static_cast<unsigned>(midboss.stage2_direction) << 5U
		), 48);
		if(!bullets_add(state)) return false;
	}
	if(midboss.phase_frame >= 32) {
		midboss.phase_frame = 0;
		midboss.stage2_pattern = 0xFFU;
	}
	return true;
}

[[nodiscard]] bool stage2_midboss_pattern_3(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	const auto frame = static_cast<std::int16_t>(midboss.phase_frame - 1);
	auto &bullet = state.bullets.template_;
	if(frame == 0) {
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, midboss.pos.cur.y),
			word_sub(state.player_pos.cur.x, midboss.pos.cur.x)
		);
		if(!angle) return false;
		midboss.angle = *angle;
	}
	if((frame % 8) == 0) {
		presentation.sound(3U);
		bullet.speed = 40U;
		bullet.group = BulletGroup::spread;
		bullet.count = 5U;
		bullet.delta = 16U;
		bullet.angle = midboss.angle;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		bullet.speed = 10U;
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, midboss.pos.cur.y),
			word_sub(state.player_pos.cur.x, midboss.pos.cur.x)
		);
		if(!angle) return false;
		bullet.angle = *angle;
		bullet.patnum = pat_bullet16_directional_blue;
		bullet.group = BulletGroup::single;
		bullet.special_motion = BulletSpecialMotion::none;
	}
	if((frame % 32) >= 24) {
		bullet.spawn_type = static_cast<std::uint8_t>(
			BulletSpawnType::bullet16_cloud_forwards
		);
		bullet.speed = byte_add(bullet.speed, 10);
		if(!bullets_add(state, true, false)) return false;
	}
	if(frame >= 64) {
		midboss.phase_frame = 0;
		midboss.stage2_pattern = 0xFFU;
	}
	return true;
}

[[nodiscard]] bool stage2_midboss_pattern_gap(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	state.bullets.gather_template.center = midboss.pos.cur;
	gather_add_only_3stack(
		state, static_cast<std::int16_t>(midboss.phase_frame - 48), 7U, stage2_white
	);
	if(midboss.phase_frame == 1) {
		state.bullets.gather_template.ring_points = 8;
		state.bullets.gather_template.radius = 1536;
		switch(midboss.stage2_direction) {
		case 1U: {
			const auto direction = pc98::th0405::randring_next16(state.random);
			if(!direction) return false;
			if((*direction & 1U) != 0U) {
				midboss.sprite = 1U;
				midboss.stage2_direction = 0U;
				midboss.pos.velocity.x = -48;
			} else {
				midboss.sprite = 2U;
				midboss.stage2_direction = 2U;
				midboss.pos.velocity.x = 48;
			}
			break;
		}
		case 0U:
			midboss.stage2_direction = 1U;
			midboss.pos.velocity.x = 48;
			midboss.sprite = 2U;
			break;
		case 2U:
			midboss.stage2_direction = 1U;
			midboss.pos.velocity.x = -48;
			midboss.sprite = 1U;
			break;
		default:
			break;
		}
	}
	if(midboss.phase_frame == 48) {
		midboss.pos.velocity.x = 0;
	} else if(midboss.phase_frame == 52) {
		midboss.phase_frame = 0;
		if(midboss.stage2_direction == 1U) midboss.sprite = 0U;
		++midboss.stage2_cycles;
		midboss.stage2_pattern = static_cast<std::uint8_t>(midboss.stage2_cycles & 3U);
		if(midboss.stage2_cycles > 16U) return false;
	}
	(void)presentation;
	return true;
}

[[nodiscard]] bool stage2_midboss_update(State &state, Presentation &presentation) noexcept
{
	auto &midboss = state.midboss;
	state.reimu_a.homing_target = midboss.pos.cur;
	if(midboss.phase == 0U) {
		pc98::th0405::update_motion(midboss.pos);
		++midboss.phase_frame;
		std::uint16_t ignored_damage{};
		if(!midboss_hittest_damage(state, presentation, ignored_damage)) return false;
		if(midboss.phase_frame >= 96) {
			midboss.phase = 1U;
			midboss.phase_frame = 0;
			midboss.pos.velocity = {};
			midboss.stage2_pattern = 0U;
			midboss.stage2_direction = 1U;
			midboss.stage2_cycles = 0U;
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
		switch(midboss.stage2_pattern) {
		case 0U: pattern_ok = stage2_midboss_pattern_0(state, presentation); break;
		case 1U: pattern_ok = stage2_midboss_pattern_1(state, presentation); break;
		case 2U: pattern_ok = stage2_midboss_pattern_2(state, presentation); break;
		case 3U: pattern_ok = stage2_midboss_pattern_3(state, presentation); break;
		case 0xFFU: pattern_ok = stage2_midboss_pattern_gap(state, presentation); break;
		default: return false;
		}
		if(!pattern_ok && midboss.stage2_cycles <= 16U) return false;

		std::uint16_t damage{};
		if(!midboss_hittest_damage(state, presentation, damage)) return false;
		if(damage == 0U && midboss.stage2_cycles <= 16U) return true;
		if(midboss.stage2_cycles <= 16U) {
			midboss.hp = static_cast<std::int16_t>(
				static_cast<int>(midboss.hp) - static_cast<int>(damage)
			);
			if(midboss.hp > 0) {
				midboss.damage_this_frame = 1U;
				presentation.sound(4U);
				return true;
			}
			midboss.damage_this_frame = 1U;
			state.bullets.zap.set_active(true);
			const auto units = static_cast<std::uint16_t>(
				18U - static_cast<std::uint16_t>(midboss.stage2_cycles)
			);
			if(!midboss_score_bonus(state, presentation, units)) return false;
			items_add(state, presentation, midboss.pos.cur, ItemType::bomb);
			state.playfield_shake_anim_time = 12;
		}
		midboss.phase = 2U;
		midboss.sprite = 0U;
		midboss.phase_frame = 0;
		midboss.pos.velocity = {0, -16};
		if(!pc98::th0405::sparks_add_circle(
			pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
			midboss.pos.cur, 128, 48U
		)) return false;
		presentation.sound(12U);
		return true;
	}
	if(midboss.phase == 2U) {
		pc98::th0405::update_motion(midboss.pos);
		++midboss.phase_frame;
		if(midboss.pos.cur.y <= 0) {
			++midboss.phase;
			midboss.hp = 0;
		}
		if((state.stage_frame & 15U) == 0U &&
			!pc98::th0405::sparks_add_circle(
				pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
				midboss.pos.cur, 128, 16U
			)) {
			return false;
		}
		return true;
	}
	state.midboss_active = false;
	midboss.hp = 0;
	return true;
}

void kurumi_spawnrays_add(State &state, std::int16_t distance_from_center_x,
	std::uint8_t angle, Presentation &presentation) noexcept
{
	for(auto &ray : state.boss.kurumi_spawnrays) {
		if(ray.flag != KurumiSpawnrayFlag::free) continue;
		ray.flag = KurumiSpawnrayFlag::grow;
		ray.target = {
			word_add(state.boss.pos.cur.x, distance_from_center_x),
			word_sub(state.boss.pos.cur.y, 160),
		};
		ray.origin = ray.target;
		const auto velocity = pc98::masterlib::vector2(angle, 256);
		ray.velocity = {velocity.x, velocity.y};
		presentation.sound(5U);
		return;
	}
}

[[nodiscard]] bool kurumi_spawnrays_update(
	State &state, Presentation &presentation
) noexcept
{
	bool all_free = true;
	for(auto &ray : state.boss.kurumi_spawnrays) {
		if(ray.flag == KurumiSpawnrayFlag::free) continue;
		all_free = false;
		if(ray.flag == KurumiSpawnrayFlag::grow) {
			if(ray.target.x < playfield_width && ray.target.y < playfield_height &&
				ray.target.x > 0 && ray.target.y > 0) {
				ray.target.x = word_add(ray.target.x, ray.velocity.x);
				ray.target.y = word_add(ray.target.y, ray.velocity.y);
				continue;
			}
			auto &bullet = state.bullets.template_;
			bullet.origin = {
				word_sub(ray.target.x, ray.velocity.x),
				word_sub(ray.target.y, ray.velocity.y),
			};
			bullet.special_motion = BulletSpecialMotion::speedup;
			state.bullets.special.value = 1U;
			bullet.speed = 32U;
			for(std::uint8_t i = 0U; i < 3U; ++i) {
				if(!bullets_add(state, false, true)) return false;
				bullet.speed = byte_add(bullet.speed, 6);
			}
			ray.flag = KurumiSpawnrayFlag::shrink;
			presentation.sound(6U);
			state.circles_color = 9U;
			circle_add_growing(state, bullet.origin);
			continue;
		}
		if(ray.flag != KurumiSpawnrayFlag::shrink) return false;
		if(ray.origin.x >= playfield_width || ray.origin.y >= playfield_height ||
			ray.origin.x <= 0 || ray.origin.y <= 0) {
			ray.flag = KurumiSpawnrayFlag::free;
			continue;
		}
		ray.origin.x = word_add(ray.origin.x, ray.velocity.x);
		ray.origin.y = word_add(ray.origin.y, ray.velocity.y);
	}
	return all_free;
}

void kurumi_circle_position(State &state, bool clockwise) noexcept
{
	auto &boss = state.boss;
	boss.pos.cur.x = word_add(3072, pc98::masterlib::vector_component(
		1024, pc98::masterlib::cosine8(boss.angle)
	));
	boss.pos.cur.y = word_add(1456, pc98::masterlib::vector_component(
		320, pc98::masterlib::sine8(boss.angle)
	));
	boss.angle = byte_add(boss.angle, clockwise ? 1 : -1);
}

[[nodiscard]] bool kurumi_rays_left(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 16) {
		boss.sprite = 8U;
	} else if(boss.phase_frame == 48) {
		circle_add_shrinking(state, {word_sub(boss.pos.cur.x, 192),
			word_sub(boss.pos.cur.y, 160)});
		state.circles_color = stage2_white;
		presentation.sound(8U);
	} else if(boss.phase_frame == 64) {
		boss.sprite = 0U;
		const auto random = pc98::th0405::randring_next16_and(state.random, 15U);
		if(!random) return false;
		kurumi_spawnrays_add(state, -192, byte_add(24U, -static_cast<std::int16_t>(*random)), presentation);
	} else if(boss.phase_frame > 64) {
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = pat_bullet16_outlined_ball_blue;
		bullet.angle = 0U;
		bullet.group = BulletGroup::ring_aimed;
		bullet.count = 16U;
		if(!bullet_template_tune(state)) return false;
		if(kurumi_spawnrays_update(state, presentation)) {
			boss.phase_frame = 0;
			boss.mode = 0U;
		}
	}
	return true;
}

[[nodiscard]] bool kurumi_rays_right(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 16) {
		boss.sprite = 9U;
	} else if(boss.phase_frame == 48) {
		circle_add_shrinking(state, {word_add(boss.pos.cur.x, 192),
			word_sub(boss.pos.cur.y, 160)});
		state.circles_color = stage2_white;
		presentation.sound(8U);
	} else if(boss.phase_frame == 64) {
		boss.sprite = 0U;
		const auto random = pc98::th0405::randring_next16_and(state.random, 15U);
		if(!random) return false;
		kurumi_spawnrays_add(state, 192, byte_add(104U, static_cast<std::int16_t>(*random)), presentation);
	} else if(boss.phase_frame > 64) {
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = pat_bullet16_outlined_ball_blue;
		bullet.angle = 0U;
		bullet.group = BulletGroup::ring_aimed;
		bullet.count = 16U;
		if(!bullet_template_tune(state)) return false;
		if(kurumi_spawnrays_update(state, presentation)) {
			boss.phase_frame = 0;
			boss.mode = 0U;
		}
	}
	return true;
}

[[nodiscard]] bool kurumi_rays_both(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 16) {
		boss.sprite = 10U;
	} else if(boss.phase_frame == 48) {
		circle_add_shrinking(state, {word_add(boss.pos.cur.x, 192), word_sub(boss.pos.cur.y, 160)});
		circle_add_shrinking(state, {word_sub(boss.pos.cur.x, 192), word_sub(boss.pos.cur.y, 160)});
		state.circles_color = stage2_white;
		presentation.sound(8U);
	} else if(boss.phase_frame == 64) {
		boss.sprite = 0U;
		const auto random = pc98::th0405::randring_next16_and(state.random, 15U);
		if(!random) return false;
		const auto angle = byte_add(104U, static_cast<std::int16_t>(*random));
		kurumi_spawnrays_add(state, 192, angle, presentation);
		kurumi_spawnrays_add(state, -192, byte_add(128U, -static_cast<std::int16_t>(angle)), presentation);
	} else if(boss.phase_frame > 64) {
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = pat_bullet16_outlined_ball_blue;
		bullet.angle = 0U;
		bullet.group = BulletGroup::ring_aimed;
		bullet.count = 8U;
		if(!bullet_template_tune(state)) return false;
		if(kurumi_spawnrays_update(state, presentation)) {
			boss.phase_frame = 0;
			boss.mode = 0U;
		}
	}
	return true;
}

[[nodiscard]] bool kurumi_turning_bullets(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	kurumi_circle_position(state, false);
	boss.sprite = boss.angle > 128U ? 4U : 6U;
	if((state.stage_frame % 57U) != 0U) return true;
	auto &bullet = state.bullets.template_;
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
	bullet.patnum = pat_bullet16_outlined_ball_blue;
	bullet.origin = boss.pos.cur;
	bullet.group = BulletGroup::ring_aimed;
	bullet.count = 16U;
	bullet.special_motion = BulletSpecialMotion::decelerate_then_turn;
	bullet.speed = 48U;
	const auto first_angle = pc98::th0405::randring_next16(state.random);
	if(!first_angle) return false;
	bullet.angle = static_cast<std::uint8_t>(*first_angle);
	state.bullets.special.value = 1U;
	state.bullets.template_special_angle = (boss.stage2_turn_direction & 1U) != 0U
		? 64U : static_cast<std::uint8_t>(-64);
	if(!bullet_template_tune(state) || !bullets_add(state, true, false)) return false;
	state.bullets.template_special_angle = byte_add(
		state.bullets.template_special_angle, 128
	);
	bullet.speed = 32U;
	const auto second_angle = pc98::th0405::randring_next16(state.random);
	if(!second_angle) return false;
	bullet.angle = static_cast<std::uint8_t>(*second_angle);
	if(!bullets_add(state, true, false)) return false;
	++boss.stage2_turn_direction;
	(void)presentation;
	return true;
}

[[nodiscard]] bool kurumi_rays_left_extra(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 16) {
		boss.sprite = 8U;
	} else if(boss.phase_frame == 48) {
		circle_add_shrinking(state, {word_sub(boss.pos.cur.x, 192),
			word_sub(boss.pos.cur.y, 160)});
		state.circles_color = stage2_white;
		presentation.sound(8U);
	} else if(boss.phase_frame == 64) {
		boss.sprite = 0U;
		kurumi_spawnrays_add(state, -192, 24U, presentation);
	} else if(boss.phase_frame > 64) {
		if(boss.phase_frame == 80) {
			kurumi_spawnrays_add(state, -192, 16U, presentation);
		} else if(boss.phase_frame == 96) {
			kurumi_spawnrays_add(state, -192, 8U, presentation);
		}
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = pat_bullet16_outlined_ball_blue;
		bullet.angle = 0U;
		bullet.group = BulletGroup::ring_aimed;
		bullet.count = 12U;
		if(!bullet_template_tune(state)) return false;
		if(kurumi_spawnrays_update(state, presentation)) {
			boss.phase_frame = 0;
			boss.mode = 0U;
		}
	}
	return true;
}

[[nodiscard]] bool kurumi_rays_right_extra(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 16) {
		boss.sprite = 9U;
	} else if(boss.phase_frame == 48) {
		circle_add_shrinking(state, {word_add(boss.pos.cur.x, 192),
			word_sub(boss.pos.cur.y, 160)});
		state.circles_color = stage2_white;
		presentation.sound(8U);
	} else if(boss.phase_frame == 64) {
		boss.sprite = 0U;
		kurumi_spawnrays_add(state, 192, 104U, presentation);
	} else if(boss.phase_frame > 64) {
		if(boss.phase_frame == 80) {
			kurumi_spawnrays_add(state, 192, 112U, presentation);
		} else if(boss.phase_frame == 96) {
			kurumi_spawnrays_add(state, 192, 120U, presentation);
		}
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = pat_bullet16_outlined_ball_blue;
		bullet.angle = 0U;
		bullet.group = BulletGroup::ring_aimed;
		bullet.count = 12U;
		if(!bullet_template_tune(state)) return false;
		if(kurumi_spawnrays_update(state, presentation)) {
			boss.phase_frame = 0;
			boss.mode = 0U;
		}
	}
	return true;
}

[[nodiscard]] bool kurumi_rays_both_extra(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 16) {
		boss.sprite = 10U;
	} else if(boss.phase_frame == 48) {
		circle_add_shrinking(state, {word_sub(boss.pos.cur.x, 192),
			word_sub(boss.pos.cur.y, 160)});
		circle_add_shrinking(state, {word_add(boss.pos.cur.x, 192),
			word_sub(boss.pos.cur.y, 160)});
		state.circles_color = stage2_white;
		presentation.sound(8U);
	} else if(boss.phase_frame == 64) {
		boss.sprite = 0U;
		kurumi_spawnrays_add(state, -192, 24U, presentation);
		kurumi_spawnrays_add(state, 192, 104U, presentation);
	} else if(boss.phase_frame > 64) {
		if(boss.phase_frame == 80) {
			kurumi_spawnrays_add(state, -192, 16U, presentation);
			kurumi_spawnrays_add(state, 192, 112U, presentation);
		} else if(boss.phase_frame == 96) {
			kurumi_spawnrays_add(state, -192, 8U, presentation);
			kurumi_spawnrays_add(state, 192, 120U, presentation);
		}
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = pat_bullet16_outlined_ball_blue;
		bullet.angle = 0U;
		bullet.group = BulletGroup::ring_aimed;
		bullet.count = 6U;
		if(!bullet_template_tune(state)) return false;
		if(kurumi_spawnrays_update(state, presentation)) {
			boss.phase_frame = 0;
			boss.mode = 0U;
		}
	}
	return true;
}

[[nodiscard]] bool kurumi_stacks(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame < 16) return true;
	if(boss.phase_frame == 16) {
		const auto right_random = pc98::th0405::randring_next16_and(state.random, 15U);
		if(!right_random) return false;
		boss.statebytes[15] = byte_add(224U, -static_cast<std::int16_t>(*right_random));
		const auto left_random = pc98::th0405::randring_next16_and(state.random, 15U);
		if(!left_random) return false;
		boss.statebytes[14] = byte_add(160U, static_cast<std::int16_t>(*left_random));
		boss.statebytes[13] = 0U;
		bullet.patnum = pat_bullet16_directional_blue;
	}
	if((boss.phase_frame % 8) == 0) {
		bullet.speed = 16U;
		boss.statebytes[15] = byte_add(boss.statebytes[15], 16);
		boss.statebytes[14] = byte_add(boss.statebytes[14], -16);
		++boss.statebytes[13];
		if(boss.statebytes[13] > 10U) {
			const auto right_random = pc98::th0405::randring_next16_and(state.random, 15U);
			if(!right_random) return false;
			boss.statebytes[15] = byte_add(224U, -static_cast<std::int16_t>(*right_random));
			const auto left_random = pc98::th0405::randring_next16_and(state.random, 15U);
			if(!left_random) return false;
			boss.statebytes[14] = byte_add(160U, static_cast<std::int16_t>(*left_random));
			boss.statebytes[13] = 0U;
		}
		presentation.sound(15U);
	}
	bullet.spawn_type = static_cast<std::uint8_t>(
		BulletSpawnType::bullet16_cloud_forwards
	);
	bullet.group = BulletGroup::single;
	bullet.origin.y = word_sub(boss.pos.cur.y, 160);
	bullet.origin.x = word_add(boss.pos.cur.x, 192);
	bullet.angle = boss.statebytes[15];
	if(!bullets_add(state, false, true)) return false;
	bullet.origin.x = word_sub(bullet.origin.x, 384);
	bullet.angle = boss.statebytes[14];
	if(!bullets_add(state, false, true)) return false;
	bullet.speed = byte_add(bullet.speed, 10);
	if(boss.statebytes[0] == 0U || (boss.phase_frame % boss.statebytes[0]) != 0) {
		return true;
	}
	bullet.group = BulletGroup::spread_aimed;
	bullet.count = 5U;
	bullet.delta = 9U;
	bullet.angle = 0U;
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
	const auto speed_random = pc98::th0405::randring_next16_and(state.random, 15U);
	if(!speed_random) return false;
	bullet.speed = byte_add(32U, static_cast<std::int16_t>(*speed_random));
	bullet.special_motion = BulletSpecialMotion::none;
	return bullet_template_tune(state) && bullets_add(state, true, false);
}

void kurumi_hover(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.pos.cur.x < 3056) {
		boss.pos.velocity.x = 24;
	} else if(boss.pos.cur.x > 3088) {
		boss.pos.velocity.x = -24;
	}
	if(boss.pos.cur.y < 1264) {
		boss.pos.velocity.y = 12;
	} else if(boss.pos.cur.y > 1296) {
		boss.pos.velocity.y = -12;
	}
	pc98::th0405::update_motion(boss.pos);
}

[[nodiscard]] bool kurumi_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	state.bullets.gather_template.center = boss.pos.cur;
	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 0) {
			boss.hp = 4800;
			boss.phase_end_hp = 4800;
			state.palette[0] = {96U, 0U, 0U};
			state.palette_changed = true;
			for(auto &ray : boss.kurumi_spawnrays) ray.flag = KurumiSpawnrayFlag::free;
			state.bullets.gather_template.center = boss.pos.cur;
			state.bullets.gather_template.ring_points = 32;
			state.bullets.gather_template.radius = 5120;
			state.bullets.gather_template.angle_delta = static_cast<std::uint8_t>(-3);
			state.bullets.gather_template.col = stage2_white;
		} else if(boss.phase_frame >= 288) {
			if(boss.phase_frame == 296) state.bullets.gather_template.col = 9U;
			if((boss.phase_frame & 7) == 0) gather_add_only(state);
			if(boss.phase_frame >= 320) {
				++boss.phase;
				boss.phase_frame = 0;
				presentation.sound(13U);
				boss.stage2_unused = 0U;
			}
		} else if(boss.phase_frame == 128) {
			presentation.sound(8U);
		}
		++boss.phase_frame;
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		break;

	case 1U:
		++boss.phase_frame;
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(boss.phase_frame >= 32 &&
			!boss_phase_next(state, presentation, 0U, 3300)) return false;
		if(boss.phase == 2U) {
			boss.mode = 3U;
			boss.angle = 192U;
			presentation.sound(6U);
		}
		break;

	case 2U: {
		switch(boss.mode) {
		case 0U: {
			kurumi_circle_position(state, true);
			if(boss.phase_frame >= 96) {
				boss.phase_frame = 0;
				const auto next_mode = pc98::th0405::randring_next16_and(state.random, 3U);
				if(!next_mode) return false;
				boss.mode = static_cast<std::uint8_t>(*next_mode + 1U);
				++boss.phase_state;
				if(boss.phase_state > 10U) {
					if(!boss_score_bonus(state, presentation, 10U) ||
						!boss_phase_next(state, presentation, 1U, 2050)) return false;
					state.bullets.template_special_angle = 64U;
					state.reimu_a.homing_target = boss.pos.cur;
					return true;
				}
				auto &bullet = state.bullets.template_;
				bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
				bullet.origin = boss.pos.cur;
				bullet.patnum = pat_bullet16_outlined_ball_blue;
				bullet.group = BulletGroup::ring;
				bullet.count = 22U;
				const auto angle = pc98::th0405::randring_next16(state.random);
				if(!angle) return false;
				bullet.angle = static_cast<std::uint8_t>(*angle);
				if(!bullet_template_tune(state)) return false;
				bullet.speed = 16U;
				const auto count = boss.phase_state <= 5U ? boss.phase_state : 5U;
				for(std::uint8_t i = 0U; i < count; ++i) {
					if(!bullets_add(state, false, true)) return false;
					bullet.speed = byte_add(bullet.speed, 8);
					bullet.angle = byte_add(bullet.angle, -3);
				}
			}
			break;
		}
		case 1U:
			if(!kurumi_rays_left(state, presentation)) return false;
			break;
		case 2U:
			if(!kurumi_rays_right(state, presentation)) return false;
			break;
		case 3U:
		case 4U:
			if(!kurumi_rays_both(state, presentation)) return false;
			break;
		default:
			break;
		}
		const auto hit = boss_hittest(state, presentation);
		if(hit == HitResult::unsupported) return false;
		if(hit == HitResult::ended) {
			if(!boss_score_bonus(state, presentation, 10U) ||
				!boss_phase_next(state, presentation, 1U, 2050)) return false;
			state.bullets.template_special_angle = 64U;
		}
		break;
	}

	case 3U:
		(void)kurumi_spawnrays_update(state, presentation);
		if(boss.mode == 0U) {
			if(boss.phase_frame >= 128) {
				boss.phase_frame = 0;
				boss.mode = 1U;
			}
		} else if(boss.mode == 1U && !kurumi_turning_bullets(state, presentation)) {
			return false;
		}
		if(boss.phase_frame <= 2000) {
			const auto hit = boss_hittest(state, presentation);
			if(hit == HitResult::unsupported) return false;
			if(hit == HitResult::none) break;
			if(!boss_score_bonus(state, presentation, 10U)) return false;
		}
		if(!boss_phase_next(state, presentation, 2U, 550)) return false;
		break;

	case 4U: {
		if(boss.mode == 0U) {
			kurumi_circle_position(state, true);
			if(boss.phase_frame >= 96) {
				boss.phase_frame = 0;
				const auto next_mode = pc98::th0405::randring_next16_mod(
					state.random, 3U
				);
				if(!next_mode) return false;
				boss.mode = static_cast<std::uint8_t>(*next_mode + 1U);
				++boss.phase_state;
				if(boss.phase_state > 10U) {
					if(!boss_score_bonus(state, presentation, 10U) ||
						!boss_phase_next(state, presentation, 3U, 0)) return false;
					state.reimu_a.homing_target = boss.pos.cur;
					return true;
				}
				auto &bullet = state.bullets.template_;
				bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
				bullet.origin = boss.pos.cur;
				bullet.patnum = pat_bullet16_outlined_ball_blue;
				bullet.group = BulletGroup::ring;
				bullet.count = 22U;
				const auto angle = pc98::th0405::randring_next16(state.random);
				if(!angle) return false;
				bullet.angle = static_cast<std::uint8_t>(*angle);
				if(!bullet_template_tune(state)) return false;
				bullet.speed = 16U;
				const auto count = boss.phase_state <= 5U ? boss.phase_state : 5U;
				for(std::uint8_t i = 0U; i < count; ++i) {
					if(!bullets_add(state, false, true)) return false;
					bullet.speed = byte_add(bullet.speed, 8);
					bullet.angle = byte_add(bullet.angle, 3);
				}
			}
		} else if(boss.mode == 1U) {
			if(!kurumi_rays_left_extra(state, presentation)) return false;
		} else if(boss.mode == 2U) {
			if(!kurumi_rays_right_extra(state, presentation)) return false;
		} else if(boss.mode == 3U) {
			if(!kurumi_rays_both_extra(state, presentation)) return false;
		}
		const auto hit = boss_hittest(state, presentation);
		if(hit == HitResult::unsupported) return false;
		if(hit == HitResult::ended) {
			if(!boss_score_bonus(state, presentation, 10U) ||
				!boss_phase_next(state, presentation, 3U, 0)) return false;
		}
		break;
	}

	case 5U:
		(void)kurumi_spawnrays_update(state, presentation);
		if(boss.mode == 0U) {
			if(boss.phase_frame == 144) {
				circle_add_shrinking(state, boss.pos.cur);
				state.circles_color = stage2_white;
			}
			if(boss.phase_frame > 64) {
				boss.phase_frame = 0;
				boss.mode = 1U;
				boss.sprite = 12U;
				boss.angle = 128U;
			}
			kurumi_hover(state);
		} else if(boss.mode == 1U) {
			if(!kurumi_stacks(state, presentation)) return false;
		}
		if(const auto hit = boss_hittest(state, presentation);
			hit == HitResult::unsupported) return false;
		if(boss.damage_this_frame == 0U && boss.phase_frame < 700) break;
		++boss.phase;
		if(!pc98::th0405::sparks_add_circle(
			pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
			boss.pos.cur, 128, 48U
		)) return false;
		boss_explode_small(state, presentation, 4U);
		boss.phase_state = boss.phase_frame < 600 ? 1U : 0U;
		boss.phase_frame = 0;
		break;

	case 6U:
		(void)kurumi_spawnrays_update(state, presentation);
		kurumi_hover(state);
		++boss.phase_frame;
		if(boss.phase_frame == 16) {
			boss_explode_small(state, presentation, 4U);
		} else if(boss.phase_frame == 32) {
			boss_explode_big(state, presentation, 0U);
			boss.phase = phase_explode_big;
			state.bullets.zap.set_active(boss.phase_state != 0U);
			if(boss.phase_state != 0U && !boss_score_bonus(state, presentation, 20U)) {
				return false;
			}
			boss.sprite = pat_enemy_kill;
			boss.phase_frame = 0;
			presentation.sound(12U);
			state.palette[0].r = 0U;
			state.palette_changed = true;
			state.player_invincibility_time = boss_defeat_invincibility_frames;
		}
		break;

	case phase_explode_big:
		if(boss.phase_frame < 12) {
			state.playfield_shake_x = (state.stage_frame & 1U) == 0U ? -4 : 4;
			state.playfield_shake_y = (state.stage_frame & 3U) <= 1U ? -4 : 4;
		}
		state.bullets.slowdown_factor = 2U;
		++boss.phase_frame;
		if((boss.phase_frame % 8) == 0) {
			++boss.sprite;
			if(boss.sprite >= static_cast<std::uint8_t>(pat_enemy_kill_last + 1U)) {
				boss.phase = phase_none;
				boss.phase_frame = 0;
				state.bombing_disabled = true;
			}
		}
		return true;

	case phase_none:
		state.palette_tone = 60U;
		state.palette_changed = true;
		state.stage_clear_requested = true;
		return true;

	default:
		return false;
	}
	state.reimu_a.homing_target = boss.pos.cur;
	return true;
}

} // namespace

void stage2_fights_setup(State &state) noexcept
{
	state.midboss.frames_until = 2600U;
	state.midboss.pos.cur = {3072, -512};
	state.midboss.pos.prev = state.midboss.pos.cur;
	state.midboss.pos.velocity = {0, 16};
	state.midboss.hp = 750;
	state.midboss.sprite = 0U;
	state.midboss_active = false;

	state.boss_active = false;
	state.boss.phase = 0U;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.phase_frame = 0;
	state.boss.pos.cur = {3072, 1296};
	state.boss.pos.prev = state.boss.pos.cur;
	state.boss.pos.velocity = {};
	state.boss.damage_this_frame = 0U;
	state.boss.phase_timed_out = true;
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
	state.boss.sprite = 0U;
	state.boss.hitbox_radius = {384, 384};
	switch(state.rank) {
	case 0U: state.boss.statebytes[0] = 255U; break;
	case 1U: state.boss.statebytes[0] = 128U; break;
	case 2U: state.boss.statebytes[0] = 32U; break;
	case 3U: state.boss.statebytes[0] = 8U; break;
	default: state.boss.statebytes[0] = 8U; break;
	}
}

void stage2_midboss_activate(State &state) noexcept
{
	if(state.midboss.frames_until != state.stage_frame) return;
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
	if(state.midboss_active && !stage2_midboss_update(state, presentation)) return false;
	if(state.boss_active && !kurumi_update(state, presentation)) return false;
	return true;
}

void stage2_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	if(state.midboss_active && state.midboss.damage_this_frame != 0U) {
		state.midboss.damage_this_frame = 0U;
	}
	stage1_fights_render_tail(state, presentation);
}

namespace {

constexpr std::uint8_t stage3_white = 15U;
constexpr std::uint8_t pat_elly = 134U;
constexpr std::uint8_t pat_bullet16_directional_yellow = 92U;
constexpr std::uint8_t pat_bullet16_outlined_ball_white = 52U;
constexpr std::array<std::uint8_t, 12U> stage3_midboss_fly_angles{
	0x18U, 0x68U, 0x98U, 0xE8U, 0x00U, 0x60U,
	0xA0U, 0x40U, 0xE0U, 0x80U, 0x20U, 0x60U,
};

[[nodiscard]] bool stage3_midboss_hittest(
	State &state, Presentation &presentation, std::uint16_t &damage,
	std::uint8_t hit_sound
) noexcept
{
	return midboss_hittest_shots_damage(
		state, presentation, {384, 384}, hit_sound, damage
	);
}

[[nodiscard]] bool stage3_midboss_pattern_0(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	auto &bullet = state.bullets.template_;
	if(midboss.phase_frame == 2) {
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, midboss.pos.cur.y),
			word_sub(state.player_pos.cur.x, midboss.pos.cur.x)
		);
		if(!angle) return false;
		midboss.angle = *angle;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.group = BulletGroup::ring;
		bullet.count = 32U;
		bullet.speed = 40U;
		bullet.angle = 0U;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
	}
	if((midboss.phase_frame % 4) == 0) {
		bullet.group = BulletGroup::spread;
		bullet.count = 2U;
		bullet.delta = 12U;
		bullet.speed = 52U;
		bullet.angle = midboss.angle;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		presentation.sound(3U);
	}
	if(midboss.phase_frame >= 32) {
		midboss.stage3_pattern = 0xFFU;
		midboss.phase_frame = 0;
	}
	return true;
}

[[nodiscard]] bool stage3_midboss_pattern_1(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	if(midboss.phase_frame >= 32) {
		midboss.stage3_pattern = 0xFFU;
		midboss.phase_frame = 0;
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(
			BulletSpawnType::bullet16_cloud_backwards
		);
		bullet.patnum = pat_bullet16_ball_blue;
		bullet.group = BulletGroup::ring;
		bullet.count = 32U;
		bullet.speed = 40U;
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return false;
		bullet.angle = static_cast<std::uint8_t>(*angle);
		if(!bullet_template_tune(state) || !bullets_add(state, false, true)) return false;
		presentation.sound(9U);
	}
	return true;
}

[[nodiscard]] bool stage3_midboss_pattern_2(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	if(midboss.phase_frame == 1) midboss.angle = 128U;
	if((midboss.phase_frame % 2) == 0) {
		auto &bullet = state.bullets.template_;
		bullet.group = BulletGroup::spread;
		bullet.count = 2U;
		bullet.delta = 18U;
		bullet.speed = 46U;
		bullet.angle = midboss.angle;
		midboss.angle = byte_add(midboss.angle, -8);
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		presentation.sound(3U);
	}
	if(midboss.phase_frame >= 32) {
		midboss.stage3_pattern = 0xFFU;
		midboss.phase_frame = 0;
	}
	return true;
}

[[nodiscard]] bool stage3_midboss_pattern_3(
	State &state, Presentation &presentation
) noexcept
{
	auto &midboss = state.midboss;
	if(midboss.phase_frame == 1) {
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return false;
		midboss.angle = static_cast<std::uint8_t>(*angle);
	}
	if((midboss.phase_frame % 6) == 0) {
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = pat_bullet16_directional_blue;
		bullet.group = BulletGroup::ring;
		bullet.speed = 32U;
		bullet.angle = midboss.angle;
		bullet.count = 24U;
		if(!bullet_template_tune(state) || !bullets_add(state, false, true)) return false;
		presentation.sound(9U);
		midboss.angle = byte_add(midboss.angle, 6);
	}
	if(midboss.phase_frame >= 32) {
		midboss.stage3_pattern = 0xFFU;
		midboss.phase_frame = 0;
	}
	return true;
}

[[nodiscard]] bool stage3_midboss_update(State &state, Presentation &presentation) noexcept
{
	auto &midboss = state.midboss;
	state.reimu_a.homing_target = midboss.pos.cur;
	if(midboss.phase == 0U) {
		pc98::th0405::update_motion(midboss.pos);
		++midboss.phase_frame;
		std::uint16_t ignored_damage{};
		if(!stage3_midboss_hittest(state, presentation, ignored_damage, 10U)) return false;
		if(midboss.phase_frame >= 20) {
			midboss.phase = 1U;
			midboss.phase_frame = 0;
			midboss.pos.velocity = {};
			const auto pattern = pc98::th0405::randring_next16_and(state.random, 3U);
			if(!pattern) return false;
			midboss.stage3_pattern = static_cast<std::uint8_t>(*pattern);
			const auto mirrored = pc98::th0405::randring_next16_and(state.random, 1U);
			if(!mirrored) return false;
			midboss.stage3_mirrored = static_cast<std::uint8_t>(*mirrored);
			midboss.stage3_patterns_done = 0U;
		}
	} else if(midboss.phase == 1U) {
		pc98::th0405::update_motion(midboss.pos);
		++midboss.phase_frame;
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.origin = {midboss.pos.cur.x, word_sub(midboss.pos.cur.y, 256)};
		switch(midboss.stage3_pattern) {
		case 0U:
			if(!stage3_midboss_pattern_0(state, presentation)) return false;
			break;
		case 1U:
			if(!stage3_midboss_pattern_1(state, presentation)) return false;
			break;
		case 2U:
			if(!stage3_midboss_pattern_2(state, presentation)) return false;
			break;
		case 3U:
			if(!stage3_midboss_pattern_3(state, presentation)) return false;
			break;
		case 0xFFU:
			if(midboss.stage3_patterns_done <= 11U) {
				state.bullets.gather_template.center = midboss.pos.cur;
				gather_add_only_3stack(
					state, static_cast<std::int16_t>(midboss.phase_frame - 64),
					stage3_white, 9U
				);
				if(midboss.phase_frame == 64) {
					midboss.pos.velocity = {};
				} else if(midboss.phase_frame == 68) {
					midboss.phase_frame = 0;
					midboss.stage3_pattern = static_cast<std::uint8_t>(
						midboss.stage3_patterns_done & 3U
					);
					midboss.sprite = 0U;
				}
			}
			// th04_main.asm::loc_145A5 reaches this after the guarded gather body.
			// The valid twelfth flight reads entry 11, then leaves phase_frame
			// monotonically advancing; any later frame-1 state has no bounded
			// source table entry.
			if(midboss.phase_frame == 1) {
				const auto index = static_cast<std::size_t>(midboss.stage3_patterns_done);
				if(index >= stage3_midboss_fly_angles.size()) return false;
				auto angle = stage3_midboss_fly_angles[index];
				if(midboss.stage3_mirrored != 0U) angle = byte_add(128U, -angle);
				const auto velocity = pc98::masterlib::vector2(angle, 32);
				midboss.pos.velocity = {velocity.x, velocity.y};
				++midboss.stage3_patterns_done;
				midboss.sprite = 1U;
				state.bullets.gather_template.ring_points = 8;
			}
			break;
		default:
			return false;
		}
		if(midboss.pos.cur.y >= playfield_height || midboss.pos.cur.x <= 0 ||
			midboss.pos.cur.x >= playfield_width) {
			midboss.phase = 3U;
		}
		std::uint16_t damage{};
		if(!stage3_midboss_hittest(state, presentation, damage, 4U)) return false;
		if(damage != 0U) {
			midboss.hp = static_cast<std::int16_t>(
				static_cast<int>(midboss.hp) - static_cast<int>(damage)
			);
			if(midboss.hp > 0) {
				midboss.damage_this_frame = 1U;
			} else {
				midboss.damage_this_frame = 1U;
				state.bullets.zap.set_active(true);
				if(!midboss_score_bonus(state, presentation, static_cast<std::uint16_t>(
					20U - midboss.stage3_patterns_done
				))) return false;
				midboss.phase = phase_explode_big;
				midboss.sprite = pat_enemy_kill;
				midboss.phase_frame = 0;
				midboss.pos.velocity.x = 0;
				if(!pc98::th0405::sparks_add_circle(
					pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
					midboss.pos.cur, 96, 48U
				)) return false;
				presentation.sound(12U);
				items_add(state, presentation, midboss.pos.cur, ItemType::one_up);
			}
		}
	} else if(midboss.phase == phase_explode_big) {
		if(midboss.phase_frame == 0) state.playfield_shake_anim_time = 12;
		++midboss.phase_frame;
		if((midboss.phase_frame % 16) == 0) {
			++midboss.sprite;
			if(midboss.sprite >= static_cast<std::uint8_t>(pat_enemy_kill_last + 1U)) {
				midboss.phase = phase_none;
			}
		}
	} else {
		state.midboss_active = false;
		midboss.hp = 0;
	}
	return true;
}

[[nodiscard]] bool elly_boomerang_update(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &boomerang = boss.elly_boomerang;
	if(boomerang.flag == EllyBoomerangFlag::caught) {
		boomerang.flag = EllyBoomerangFlag::free;
	}
	switch(boomerang.state) {
	case 1U:
		if(boomerang.state_frame < 64U && (boomerang.state_frame & 7U) == 0U) {
			boss.sprite = static_cast<std::uint8_t>(
				pat_elly + static_cast<std::uint8_t>(boomerang.state_frame >> 3U)
			);
		}
		if(boomerang.state_frame == 0U) {
			presentation.sound(8U);
			++boomerang.state_frame;
			break;
		}
		if(boomerang.state_frame == 56U) {
			presentation.sound(9U);
			boomerang.flag = EllyBoomerangFlag::thrown;
			boomerang.pos.cur = boss.pos.cur;
			++boomerang.state_frame;
			break;
		}
		if(boomerang.state_frame < 64U) {
			++boomerang.state_frame;
			break;
		}
		boss.sprite = static_cast<std::uint8_t>(pat_elly + 7U);
		boomerang.state = 2U;
		[[fallthrough]];

	case 2U: {
		if(boomerang.state_frame < 80U) {
			const auto aimed = pc98::masterlib::iatan2_source(
				word_sub(state.player_pos.cur.y, boomerang.pos.cur.y),
				word_sub(state.player_pos.cur.x, boomerang.pos.cur.x)
			);
			if(!aimed) return false;
			const auto angle_delta = byte_add(*aimed, -static_cast<std::int16_t>(
				boomerang.angle
			));
			if(angle_delta < 0x80U && angle_delta >= 0x10U) {
				boomerang.angle_delta = 1U;
				boomerang.speed = byte_add(boomerang.speed,
					static_cast<std::int16_t>(state.stage_frame & 1U));
			} else if(angle_delta >= 0x80U && angle_delta <= 0xF0U) {
				boomerang.angle_delta = static_cast<std::uint8_t>(-1);
				boomerang.speed = byte_add(boomerang.speed,
					static_cast<std::int16_t>(state.stage_frame & 1U));
			} else {
				boomerang.speed = byte_add(boomerang.speed, 1);
				boomerang.angle_delta = 0U;
			}
			boomerang.angle = byte_add(boomerang.angle,
				static_cast<std::int16_t>(boomerang.angle_delta));
		} else {
			boomerang.speed = byte_add(boomerang.speed,
				static_cast<std::int16_t>(state.stage_frame & 1U));
		}
		if(boomerang.pos.cur.x <= 1024) {
			boomerang.state = 3U;
		} else if(boomerang.pos.cur.x >= 5120) {
			boomerang.state = 4U;
		} else if(boomerang.pos.cur.y >= 4864) {
			boomerang.state = 5U;
		} else if(boomerang.pos.cur.y <= 512) {
			boomerang.state = 6U;
		}
		++boomerang.state_frame;
		break;
	}

	case 3U:
		boomerang.speed = byte_add(boomerang.speed, -4);
		boomerang.angle = byte_add(boomerang.angle,
			static_cast<std::int16_t>(boomerang.angle_delta));
		if(boomerang.speed <= 4U) boomerang.state = 7U;
		break;

	case 4U:
		boomerang.speed = byte_add(boomerang.speed, -4);
		boomerang.angle = byte_add(boomerang.angle,
			static_cast<std::int16_t>(boomerang.angle_delta));
		if(boomerang.angle_delta != 0U) {
			boomerang.angle = byte_add(boomerang.angle,
				static_cast<std::int16_t>(boomerang.angle_delta));
		} else {
			boomerang.angle = byte_add(boomerang.angle, 1);
		}
		if(boomerang.speed <= 4U) boomerang.state = 7U;
		break;

	case 5U:
	case 6U:
		boomerang.speed = byte_add(boomerang.speed, -4);
		boomerang.angle = byte_add(boomerang.angle,
			static_cast<std::int16_t>(boomerang.angle_delta));
		if(boomerang.speed <= 4U) boomerang.state = 7U;
		break;

	case 7U: {
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(boss.pos.cur.y, boomerang.pos.cur.y),
			word_sub(boss.pos.cur.x, boomerang.pos.cur.x)
		);
		if(!angle) return false;
		boomerang.angle = *angle;
		boomerang.speed = byte_add(boomerang.speed, 8);
		if(word_sub(boss.pos.cur.x, 256) < boomerang.pos.cur.x &&
			word_add(boss.pos.cur.x, 256) > boomerang.pos.cur.x &&
			word_sub(boss.pos.cur.y, 256) < boomerang.pos.cur.y &&
			word_add(boss.pos.cur.y, 256) > boomerang.pos.cur.y) {
			boomerang.state = 8U;
			boomerang.flag = EllyBoomerangFlag::caught;
			boomerang.state_frame = 0U;
		}
		break;
	}

	case 8U:
		if(boomerang.state_frame < 32U && (boomerang.state_frame & 7U) == 0U) {
			boss.sprite = static_cast<std::uint8_t>(pat_elly +
				static_cast<std::uint8_t>((31U - boomerang.state_frame) >> 2U));
		}
		if(boomerang.state_frame >= 32U) {
			boss.sprite = pat_elly;
			boomerang.state = 0U;
		}
		++boomerang.state_frame;
		break;

	default:
		break;
	}
	if(boomerang.flag == EllyBoomerangFlag::thrown) {
		const auto velocity = pc98::masterlib::vector2(boomerang.angle,
			static_cast<std::int16_t>(boomerang.speed));
		boomerang.pos.velocity = {velocity.x, velocity.y};
		std::uint16_t damage{};
		if(!shots_hit(state, boomerang.pos.cur, {512, 512}, false, damage)) return false;
		boomerang.pos.velocity.y = word_sub(boomerang.pos.velocity.y,
			static_cast<std::int16_t>(static_cast<unsigned>(damage) >> 1U));
		pc98::th0405::update_motion(boomerang.pos);
		if(word_sub(boomerang.pos.cur.x, 384) < state.player_pos.cur.x &&
			word_add(boomerang.pos.cur.x, 384) > state.player_pos.cur.x &&
			word_sub(boomerang.pos.cur.y, 384) < state.player_pos.cur.y &&
			word_add(boomerang.pos.cur.y, 384) > state.player_pos.cur.y) {
			state.player_is_hit = true;
		}
	}
	return true;
}

[[nodiscard]] bool elly_boomerang_rearm(State &state) noexcept
{
	auto &boss = state.boss;
	auto &boomerang = boss.elly_boomerang;
	const auto angle = pc98::masterlib::iatan2_source(
		word_sub(state.player_pos.cur.y, boss.pos.cur.y),
		word_sub(state.player_pos.cur.x, boss.pos.cur.x)
	);
	if(!angle) return false;
	boomerang.speed = 8U;
	boomerang.angle = *angle;
	boomerang.state = 1U;
	boomerang.state_frame = 0U;
	boomerang.flag = EllyBoomerangFlag::free;
	boomerang.angle_delta = 0U;
	return true;
}

void elly_scythe_update(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.elly_scythe_tick < 128) {
		boss.pos.prev.x = word_add(boss.pos.prev.x, 8);
		boss.angle = 96U;
	} else if(boss.elly_scythe_tick < 256) {
		boss.angle = byte_add(boss.angle, -1);
	} else if(boss.elly_scythe_tick < 384) {
		boss.pos.prev.x = word_add(boss.pos.prev.x, -8);
	} else if(boss.elly_scythe_tick < 512) {
		boss.pos.prev.x = word_add(boss.pos.prev.x, 8);
		boss.angle = 32U;
	} else if(boss.elly_scythe_tick < 640) {
		boss.angle = byte_add(boss.angle, 1);
	} else if(boss.elly_scythe_tick < 768) {
		boss.pos.prev.x = word_add(boss.pos.prev.x, -8);
	} else {
		boss.pos.prev.x = word_add(boss.pos.prev.x, 8);
		boss.angle = 96U;
		boss.elly_scythe_tick = 0;
	}
	boss.pos.cur.x = word_add(3072, pc98::masterlib::vector_component(
		boss.pos.prev.x, pc98::masterlib::cosine8(boss.angle)
	));
	boss.pos.cur.y = word_add(1536, pc98::masterlib::vector_component(
		boss.pos.prev.x, pc98::masterlib::sine8(boss.angle)
	));
}

[[nodiscard]] std::uint8_t elly_gather_opening(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame > 32) return 0U;
	switch(boss.phase_frame) {
	case 1:
		state.bullets.gather_template.center = boss.pos.cur;
		state.bullets.gather_template.ring_points = 8;
		state.bullets.gather_template.radius = 3072;
		state.bullets.gather_template.col = stage3_white;
		state.bullets.gather_template.angle_delta = static_cast<std::uint8_t>(-2);
		gather_add_only(state);
		state.bullets.gather_template.angle_delta = 2U;
		gather_add_only(state);
		break;
	case 16:
		circle_add_shrinking(state, boss.pos.cur);
		state.circles_color = stage3_white;
		[[fallthrough]];
	case 8:
		state.bullets.gather_template.col = 7U;
		state.bullets.gather_template.angle_delta = static_cast<std::uint8_t>(-2);
		gather_add_only(state);
		state.bullets.gather_template.angle_delta = 2U;
		gather_add_only(state);
		break;
	case 32:
		return 2U;
	default:
		break;
	}
	return 1U;
}

[[nodiscard]] bool elly_big_pattern_end(State &state) noexcept
{
	auto &boss = state.boss;
	boss.mode = 0xFFU;
	boss.phase_frame = 0;
	auto &bullet = state.bullets.template_;
	bullet.angle = 0U;
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_backwards);
	bullet.patnum = pat_bullet16_ball_blue;
	bullet.speed = 32U;
	bullet.group = BulletGroup::ring_aimed;
	bullet.count = 48U;
	return bullet_template_tune(state) && bullets_add(state, false, true);
}

[[nodiscard]] bool elly_pattern_0(State &state) noexcept
{
	auto &boss = state.boss;
	elly_scythe_update(state);
	++boss.elly_scythe_tick;
	if(boss.phase_frame == 16) {
		if(!elly_boomerang_rearm(state)) return false;
		state.bullets.template_.angle = 192U;
	}
	if(boss.phase_frame > 16) {
		if((state.stage_frame & 15U) == 0U) {
			auto &bullet = state.bullets.template_;
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			bullet.patnum = pat_bullet16_directional_yellow;
			bullet.speed = 48U;
			bullet.group = BulletGroup::spread;
			bullet.count = 5U;
			bullet.delta = 16U;
			if(!bullet_template_tune(state) || !bullets_add(state)) return false;
			bullet.angle = byte_add(bullet.angle, -16);
		}
		if(boss.elly_boomerang.state == 0U) {
			boss.mode = 0xFFU;
			boss.phase_frame = 0;
		}
	}
	return true;
}

[[nodiscard]] bool elly_pattern_1(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	const auto opening = elly_gather_opening(state);
	if(opening != 0U) {
		if(opening != 2U) return true;
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, boss.pos.cur.y),
			word_sub(state.player_pos.cur.x, boss.pos.cur.x)
		);
		if(!angle) return false;
		state.bullets.template_.angle = byte_add(*angle, -64);
		return true;
	}
	if((boss.phase_frame % 4) == 0) presentation.sound(9U);
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame < 72) {
		if((boss.phase_frame % 2) != 0) return true;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.speed = 64U;
		bullet.group = BulletGroup::spread;
		bullet.count = 2U;
		bullet.delta = 12U;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		bullet.angle = byte_add(bullet.angle, 4);
		return true;
	}
	if(boss.phase_frame == 72) {
		bullet.angle = byte_add(bullet.angle, 64);
		return true;
	}
	if(boss.phase_frame < 144) {
		if((boss.phase_frame % 2) != 0) return true;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.speed = 64U;
		bullet.group = BulletGroup::spread;
		bullet.count = 2U;
		bullet.delta = 12U;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		bullet.angle = byte_add(bullet.angle, -2);
		return true;
	}
	return elly_big_pattern_end(state);
}

[[nodiscard]] bool elly_pattern_2(State &state) noexcept
{
	auto &boss = state.boss;
	elly_scythe_update(state);
	++boss.elly_scythe_tick;
	if(boss.phase_frame == 16 && !elly_boomerang_rearm(state)) return false;
	if(boss.phase_frame > 16) {
		if((boss.phase_frame % 16) == 0) {
			auto &bullet = state.bullets.template_;
			bullet.angle = 0U;
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
			bullet.speed = 32U;
			bullet.group = BulletGroup::ring_aimed;
			bullet.count = 8U;
			if(!bullets_add(state)) return false;
		}
		if(boss.elly_boomerang.state == 0U) {
			boss.mode = 0xFFU;
			boss.phase_frame = 0;
		}
	}
	return true;
}

[[nodiscard]] bool elly_pattern_3(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	const auto opening = elly_gather_opening(state);
	if(opening != 0U) {
		if(opening != 2U) return true;
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, boss.pos.cur.y),
			word_sub(state.player_pos.cur.x, boss.pos.cur.x)
		);
		if(!angle) return false;
		state.bullets.template_.angle = byte_add(*angle, -64);
		return true;
	}
	if((boss.phase_frame % 4) == 0) presentation.sound(9U);
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame < 80) {
		if((boss.phase_frame % 4) != 0) return true;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
		bullet.speed = static_cast<std::uint8_t>((boss.phase_frame / 8) + 44);
		bullet.group = BulletGroup::spread;
		bullet.count = 4U;
		bullet.delta = 12U;
		bullet.patnum = pat_bullet16_outlined_ball_white;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		bullet.angle = byte_add(bullet.angle, 11);
		return true;
	}
	return elly_big_pattern_end(state);
}

[[nodiscard]] bool elly_pattern_4(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	elly_scythe_update(state);
	++boss.elly_scythe_tick;
	elly_scythe_update(state);
	++boss.elly_scythe_tick;
	if(boss.phase_frame == 16) {
		if(!elly_boomerang_rearm(state)) return false;
		auto &bullet = state.bullets.template_;
		bullet.angle = 64U;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
		bullet.speed = 64U;
		bullet.group = BulletGroup::single_aimed;
		bullet.patnum = pat_bullet16_outlined_ball_blue;
		if(!bullet_template_tune(state)) return false;
	}
	if(boss.phase_frame > 16) {
		auto &bullet = state.bullets.template_;
		if((state.stage_frame & 7U) == 0U) {
			if(!bullets_add(state)) return false;
			bullet.angle = byte_add(0U, -static_cast<std::int16_t>(bullet.angle));
			if(!bullets_add(state)) return false;
			bullet.angle = byte_add(0U, -static_cast<std::int16_t>(bullet.angle));
			bullet.angle = byte_add(bullet.angle, -3);
			presentation.sound(3U);
		}
		if(boss.elly_boomerang.state == 0U) {
			boss.mode = 0xFFU;
			boss.phase_frame = 0;
		}
	}
	return true;
}

[[nodiscard]] bool elly_pattern_5(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	const auto opening = elly_gather_opening(state);
	if(opening != 0U) {
		if(opening != 2U) return true;
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, boss.pos.cur.y),
			word_sub(state.player_pos.cur.x, boss.pos.cur.x)
		);
		if(!angle) return false;
		state.bullets.template_.angle = byte_add(*angle, 64);
		return true;
	}
	if((boss.phase_frame % 4) == 0) presentation.sound(9U);
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame < 80) {
		if((boss.phase_frame % 4) != 0) return true;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
		bullet.speed = static_cast<std::uint8_t>((boss.phase_frame / 8) + 44);
		bullet.group = BulletGroup::spread;
		bullet.count = 4U;
		bullet.delta = 12U;
		bullet.patnum = pat_bullet16_outlined_ball_white;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		bullet.angle = byte_add(bullet.angle, -11);
		return true;
	}
	if(boss.phase_frame == 80) {
		bullet.angle = byte_add(bullet.angle, 64);
		return true;
	}
	return elly_big_pattern_end(state);
}

[[nodiscard]] bool elly_pattern_6(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	elly_scythe_update(state);
	++boss.elly_scythe_tick;
	elly_scythe_update(state);
	++boss.elly_scythe_tick;
	if(boss.phase_frame == 16) {
		if(!elly_boomerang_rearm(state)) return false;
		auto &bullet = state.bullets.template_;
		bullet.angle = 0U;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.speed = 32U;
		bullet.group = BulletGroup::ring_aimed;
		bullet.count = 16U;
		if(!bullet_template_tune(state)) return false;
	}
	if(boss.phase_frame > 16) {
		if((boss.phase_frame % 16) == 0) {
			if(!bullets_add(state)) return false;
			presentation.sound(3U);
		}
		if(boss.elly_boomerang.state == 0U) {
			boss.mode = 0xFFU;
			boss.phase_frame = 0;
		}
	}
	return true;
}

[[nodiscard]] bool elly_pattern_7(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	const auto opening = elly_gather_opening(state);
	if(opening != 0U) {
		if(opening != 2U) return true;
		auto &bullet = state.bullets.template_;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.speed = 32U;
		bullet.group = BulletGroup::ring;
		bullet.count = 16U;
		auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return false;
		bullet.angle = static_cast<std::uint8_t>(*angle);
		bullet.origin.x = word_sub(bullet.origin.x, 512);
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return false;
		bullet.angle = static_cast<std::uint8_t>(*angle);
		bullet.origin.x = word_add(bullet.origin.x, 1024);
		if(!bullets_add(state)) return false;
		angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return false;
		bullet.angle = static_cast<std::uint8_t>(*angle);
		bullet.origin.x = word_sub(bullet.origin.x, 512);
		bullet.origin.y = word_sub(bullet.origin.y, 512);
		if(!bullets_add(state)) return false;
		angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return false;
		bullet.angle = static_cast<std::uint8_t>(*angle);
		bullet.origin.y = word_add(bullet.origin.y, 1024);
		if(!bullets_add(state)) return false;
		presentation.sound(9U);
	}
	if(boss.phase_frame >= 80) return elly_big_pattern_end(state);
	return true;
}

[[nodiscard]] bool elly_pattern_8(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	elly_scythe_update(state);
	++boss.elly_scythe_tick;
	elly_scythe_update(state);
	++boss.elly_scythe_tick;
	if(boss.phase_frame == 16 && !elly_boomerang_rearm(state)) return false;
	if(boss.phase_frame > 16) {
		auto &bullet = state.bullets.template_;
		if((boss.phase_frame % 16) == 0) {
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			bullet.patnum = pat_bullet16_directional_blue;
			bullet.speed = 56U;
			bullet.group = BulletGroup::ring_aimed;
			bullet.count = 16U;
			bullet.angle = 0U;
			if(!bullet_template_tune(state) || !bullets_add(state)) return false;
			presentation.sound(3U);
		}
		if(boss.hp <= 200) {
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
			bullet.speed = 32U;
			bullet.group = BulletGroup::random_angle;
			bullet.count = 2U;
			if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		}
		if(boss.elly_boomerang.state == 0U) {
			boss.mode = 0xFFU;
			boss.phase_frame = 0;
		}
	}
	return true;
}

[[nodiscard]] bool elly_update(State &state, Presentation &presentation) noexcept
{
	if(!elly_boomerang_update(state, presentation)) return false;
	auto &boss = state.boss;
	switch(boss.phase) {
	case 0U:
		boss.elly_boomerang.flag = EllyBoomerangFlag::free;
		boss.elly_boomerang.state = 0U;
		++boss.phase;
		boss.mode = 0U;
		state.palette[0].b = 128U;
		state.palette_changed = true;
		boss.hp = 6000;
		boss.phase_end_hp = 6000;
		break;

	case 1U:
		pc98::th0405::update_motion(boss.pos);
		if(boss.mode == 0U) {
			if(boss.phase_frame == 32 && !elly_boomerang_rearm(state)) return false;
			if(boss.phase_frame > 32 && boss.elly_boomerang.state == 0U) {
				boss.mode = 0xFFU;
				boss.phase_frame = 0;
			}
		} else if(boss.mode == 0xFFU) {
			if(boss.phase_frame <= 64) {
				if(boss.phase_state == 0U || boss.phase_state == 3U) {
					boss.pos.velocity.x = -16;
				} else if(boss.phase_state == 1U || boss.phase_state == 2U) {
					boss.pos.velocity.x = 16;
				}
			} else {
				boss.phase_state = boss.phase_state < 3U
					? static_cast<std::uint8_t>(boss.phase_state + 1U) : 0U;
				boss.mode = 0U;
				boss.phase_frame = 0;
				boss.pos.velocity.x = 0;
			}
		}
		// Both the visible entry body and boss_hittest_shots_invincible() own an
		// increment in TH04. The preexisting helper models only its collision.
		boss.phase_frame = static_cast<std::int16_t>(boss.phase_frame + 2);
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(state.stage_frame >= 9240U) {
			++boss.phase;
			boss.phase_frame = 0;
			presentation.sound(13U);
			boss.pos.velocity.y = 8;
		}
		break;

	case 2U:
		pc98::th0405::update_motion(boss.pos);
		if(boss.pos.cur.x < 3072) {
			boss.pos.velocity.x = 32;
		} else if(boss.pos.cur.x >= 3088) {
			boss.pos.velocity.x = -32;
		} else {
			boss.pos.velocity.x = 0;
		}
		++boss.phase_frame;
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(boss.phase_frame >= 32) {
			boss.pos.velocity.x = 0;
			state.palette[0].b = 0U;
			state.palette_changed = true;
			boss.elly_scythe_tick = 0;
			boss.pos.cur = {3072, 1536};
			boss.pos.prev.x = 0;
			if(!boss_phase_next(state, presentation, 0U, 0)) return false;
			boss.elly_pattern_set = 0U;
		}
		break;

	case 3U: {
		auto &bullet = state.bullets.template_;
		bullet.origin = boss.pos.cur;
		bool pattern_ok = false;
		switch(boss.mode) {
		case 0U: pattern_ok = elly_pattern_0(state); break;
		case 1U: pattern_ok = elly_pattern_1(state, presentation); break;
		case 2U: pattern_ok = elly_pattern_2(state); break;
		case 3U: pattern_ok = elly_pattern_3(state, presentation); break;
		case 4U: pattern_ok = elly_pattern_4(state, presentation); break;
		case 5U: pattern_ok = elly_pattern_5(state, presentation); break;
		case 6U: pattern_ok = elly_pattern_6(state, presentation); break;
		case 7U: pattern_ok = elly_pattern_7(state, presentation); break;
		case 8U: pattern_ok = elly_pattern_8(state, presentation); break;
		case 0xFFU:
			if(boss.phase_frame < 32) {
				elly_scythe_update(state);
				++boss.elly_scythe_tick;
			} else {
				++boss.phase_state;
				switch(boss.elly_pattern_set) {
				case 0U:
					boss.mode = static_cast<std::uint8_t>(boss.phase_state % 2U);
					if(boss.phase_state < 8U) break;
					[[fallthrough]];
				case 1U:
					if(boss.elly_pattern_set == 1U) {
						boss.mode = static_cast<std::uint8_t>(boss.phase_state % 4U);
						if(boss.phase_state < 16U) break;
					}
					[[fallthrough]];
				case 2U:
					if(boss.elly_pattern_set == 2U) {
						boss.mode = static_cast<std::uint8_t>((boss.phase_state % 4U) + 2U);
						if(boss.phase_state < 24U) break;
					}
					[[fallthrough]];
				case 3U:
					if(boss.elly_pattern_set == 3U) {
						boss.mode = static_cast<std::uint8_t>((boss.phase_state % 4U) + 4U);
						if(boss.phase_state < 32U) break;
					}
					[[fallthrough]];
				case 4U:
					if(boss.elly_pattern_set == 4U) {
						boss.mode = static_cast<std::uint8_t>((boss.phase_state % 4U) + 5U);
						if(boss.phase_state >= 40U) {
							boss.phase_state = 0U;
							++boss.phase;
							if(!pc98::th0405::sparks_add_circle(
								pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
								boss.pos.cur, 128, 48U
							)) return false;
							boss_explode_small(state, presentation, 4U);
							boss.phase_frame = 0;
							break;
						}
						break;
					}
					[[fallthrough]];
				default:
					boss_explode_small(state, presentation, boss.elly_pattern_set);
					boss.mode = 0xFFU;
					++boss.elly_pattern_set;
					boss.hp = static_cast<std::int16_t>(6000 -
						static_cast<std::int16_t>(boss.elly_pattern_set * 1500U));
					break;
				}
				boss.phase_frame = 0;
			}
			pattern_ok = true;
			break;
		default:
			return false;
		}
		if(!pattern_ok) return false;
		const auto hit = boss_hittest(state, presentation);
		if(hit == HitResult::unsupported) return false;
		if(hit == HitResult::ended) {
			boss.phase_state = 1U;
			++boss.phase;
			if(!pc98::th0405::sparks_add_circle(
				pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
				boss.pos.cur, 128, 48U
			)) return false;
			boss_explode_small(state, presentation, 4U);
			boss.phase_frame = 0;
		}
		if((boss.elly_pattern_set == 0U && boss.hp <= 4700) ||
			(boss.elly_pattern_set == 1U && boss.hp <= 3300) ||
			(boss.elly_pattern_set == 2U && boss.hp <= 2100) ||
			(boss.elly_pattern_set == 3U && boss.hp <= 700)) {
			if(!boss_items_drop(state, presentation)) return false;
			bullets_clear(state);
			if(!boss_score_bonus(state, presentation, 10U)) return false;
			boss_explode_small(state, presentation, boss.elly_pattern_set);
			boss.mode = 0xFFU;
			boss.phase_frame = 0;
			++boss.elly_pattern_set;
		}
		break;
	}

	case 4U:
		++boss.phase_frame;
		if(boss.phase_frame == 16) {
			boss_explode_small(state, presentation, 4U);
		}
		if(boss.phase_frame == 32) {
			boss_explode_big(state, presentation, 3U);
			boss.phase = phase_explode_big;
			state.bullets.zap.set_active(boss.phase_state != 0U);
			if(boss.phase_state != 0U && !boss_score_bonus(state, presentation, 40U)) {
				return false;
			}
			boss.sprite = pat_enemy_kill;
			boss.phase_frame = 0;
			presentation.sound(12U);
			state.player_invincibility_time = boss_defeat_invincibility_frames;
			boss.elly_boomerang.state = 0U;
			boss.elly_boomerang.flag = EllyBoomerangFlag::free;
		}
		break;

	case phase_explode_big:
		if(boss.phase_frame < 12) {
			state.playfield_shake_x = (state.stage_frame & 1U) == 0U ? -4 : 4;
			state.playfield_shake_y = (state.stage_frame & 3U) <= 1U ? -4 : 4;
		}
		state.bullets.slowdown_factor = 2U;
		++boss.phase_frame;
		if((boss.phase_frame % 8) == 0) {
			++boss.sprite;
			if(boss.sprite >= static_cast<std::uint8_t>(pat_enemy_kill_last + 1U)) {
				boss.phase = phase_none;
				boss.phase_frame = 0;
				state.bombing_disabled = true;
			}
		}
		return true;

	case phase_none:
		state.palette_tone = 60U;
		state.palette_changed = true;
		state.stage_clear_requested = true;
		return true;

	default:
		return false;
	}
	state.reimu_a.homing_target = boss.pos.cur;
	return true;
}

} // namespace

bool midboss_hittest_shots_damage(
	State &state, Presentation &presentation, Point radius, std::uint8_t hit_sound,
	std::uint16_t &damage
) noexcept
{
	if(!shots_hit(state, state.midboss.pos.cur, radius, false, damage)) return false;
	if(damage != 0U) presentation.sound(hit_sound);
	return true;
}

bool midboss_score_bonus(
	State &state, Presentation &presentation, std::uint16_t units
) noexcept
{
	return midboss_score_bonus_impl(state, presentation, units);
}

void stage3_fights_setup(State &state) noexcept
{
	state.midboss.frames_until = 1600U;
	state.midboss.pos.cur = {3072, -512};
	state.midboss.pos.prev = state.midboss.pos.cur;
	state.midboss.pos.velocity = {0, 64};
	state.midboss.hp = 850;
	state.midboss.sprite = 0U;
	state.midboss_active = false;

	state.boss_active = false;
	state.boss.phase = 0U;
	state.boss.mode = 0U;
	state.boss.phase_state = 0U;
	state.boss.phase_frame = 0;
	state.boss.pos.cur = {3072, 1024};
	state.boss.pos.prev = state.boss.pos.cur;
	state.boss.pos.velocity = {};
	state.boss.damage_this_frame = 0U;
	state.boss.phase_timed_out = true;
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
	state.boss.sprite = pat_elly;
	state.boss.hitbox_radius = {384, 384};
}

void stage3_midboss_activate(State &state) noexcept
{
	if(state.midboss.frames_until != state.stage_frame) return;
	state.midboss.phase = 0U;
	state.midboss.phase_frame = 0;
	state.midboss_active = true;
}

void stage3_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}

bool stage3_fights_update(State &state, Presentation &presentation) noexcept
{
	if(state.midboss_active && !stage3_midboss_update(state, presentation)) return false;
	if(state.boss_active && !elly_update(state, presentation)) return false;
	return true;
}

void stage3_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	// elly_fg_render() updates the same boss explosions as the Stage 1 tail;
	// unlike Orange/Kurumi it deliberately leaves damage_this_frame untouched.
	stage1_fights_render_tail(state, presentation);
}

} // namespace th04::core

namespace th04::core {
namespace {

constexpr std::int16_t stage6_yuuka_hp = 13300;
constexpr std::int16_t stage6_fly_target_y = 1280;
constexpr std::uint8_t stage6_mode_fly = 255U;
constexpr std::uint8_t stage6_phase_explode_big = 254U;
constexpr std::uint8_t stage6_phase_none = 255U;
constexpr std::uint8_t stage6_pat_back_open = 128U;
constexpr std::uint8_t stage6_pat_back_halfopen = 130U;
constexpr std::uint8_t stage6_pat_back_halfclosed = 132U;
constexpr std::uint8_t stage6_pat_back_closed = 134U;
constexpr std::uint8_t stage6_pat_left_pull = 136U;
constexpr std::uint8_t stage6_pat_forward_closed = 138U;
constexpr std::uint8_t stage6_pat_forward_open = 140U;
constexpr std::uint8_t stage6_pat_shield_0 = 142U;
constexpr std::uint8_t stage6_pat_shield_1 = 144U;
constexpr std::uint8_t stage6_pat_shield_2 = 146U;
constexpr std::uint8_t stage6_pat_left = 150U;
constexpr std::uint8_t stage6_pat_left_forward_pull = 152U;
constexpr std::uint8_t stage6_pat_spin_back_0 = 154U;
constexpr std::uint8_t stage6_pat_vanish_0 = 174U;
constexpr std::uint8_t stage6_pat_vanish_1 = 176U;
constexpr std::uint8_t stage6_pat_vanish_2 = 178U;
constexpr std::uint8_t stage6_pat_vanish_3 = 180U;
constexpr std::uint8_t stage6_pat_bullet16_ball_blue = 57U;
constexpr std::uint8_t stage6_pat_bullet16_cross_yellow = 59U;
constexpr std::uint8_t stage6_pat_bullet16_small_ball_red = 60U;
constexpr std::uint8_t stage6_pat_bullet16_ball_red = 61U;
constexpr std::uint8_t stage6_pat_bullet16_directional_blue = 76U;
constexpr std::uint8_t stage6_pat_bullet16_directional_yellow = 92U;

constexpr std::array<std::array<std::uint8_t, 5U>, 2U> stage6_fly_angles{{
	{{96U, 0U, 112U, 224U, 128U}},
	{{32U, 112U, 144U, 240U, 16U}},
}};

[[nodiscard]] bool yuuka6_anim_parasol_back_close(State &state) noexcept
{
	auto &boss = state.boss;
	boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_back_open;
	++boss.yuuka6_anim_frame;
	switch(boss.yuuka6_anim_frame) {
	case 1: boss.sprite = stage6_pat_back_open; break;
	case 7: boss.sprite = stage6_pat_back_halfopen; break;
	case 13: boss.sprite = stage6_pat_back_halfclosed; break;
	case 19: boss.sprite = stage6_pat_back_closed; break;
	case 25:
		boss.yuuka6_anim_frame = 0;
		boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_back_closed;
		return true;
	default:
		break;
	}
	return false;
}

[[nodiscard]] bool yuuka6_anim_parasol_back_open(State &state) noexcept
{
	auto &boss = state.boss;
	boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_back_closed;
	++boss.yuuka6_anim_frame;
	switch(boss.yuuka6_anim_frame) {
	case 1: boss.sprite = stage6_pat_back_closed; break;
	case 7: boss.sprite = stage6_pat_back_halfclosed; break;
	case 13: boss.sprite = stage6_pat_back_halfopen; break;
	case 19: boss.sprite = stage6_pat_back_open; break;
	case 25:
		boss.yuuka6_anim_frame = 0;
		boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_back_open;
		return true;
	default:
		break;
	}
	return false;
}

[[nodiscard]] bool yuuka6_anim_parasol_back_pull_forward(State &state) noexcept
{
	auto &boss = state.boss;
	++boss.yuuka6_anim_frame;
	switch(boss.yuuka6_anim_frame) {
	case 1: boss.sprite = stage6_pat_back_closed; break;
	case 7: boss.sprite = stage6_pat_left_pull; break;
	case 13: boss.sprite = stage6_pat_left_forward_pull; break;
	case 19: boss.sprite = stage6_pat_forward_closed; break;
	case 25:
		boss.yuuka6_anim_frame = 0;
		boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_forward;
		return true;
	default:
		break;
	}
	return false;
}

[[nodiscard]] bool yuuka6_anim_parasol_back_pull_left(State &state) noexcept
{
	auto &boss = state.boss;
	++boss.yuuka6_anim_frame;
	switch(boss.yuuka6_anim_frame) {
	case 1: boss.sprite = stage6_pat_back_closed; break;
	case 7: boss.sprite = stage6_pat_left_pull; break;
	case 13: boss.sprite = stage6_pat_left; break;
	case 19:
		boss.yuuka6_anim_frame = 0;
		boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_left;
		return true;
	default:
		break;
	}
	return false;
}

[[nodiscard]] bool yuuka6_anim_parasol_left_spin_back(State &state) noexcept
{
	auto &boss = state.boss;
	++boss.yuuka6_anim_frame;
	switch(boss.yuuka6_anim_frame) {
	case 1: boss.sprite = stage6_pat_left; break;
	case 4: boss.sprite = stage6_pat_left_forward_pull; break;
	case 7: boss.sprite = stage6_pat_spin_back_0; break;
	case 10: boss.sprite = stage6_pat_spin_back_0 + 2U; break;
	case 13: boss.sprite = stage6_pat_spin_back_0 + 4U; break;
	case 16: boss.sprite = stage6_pat_spin_back_0 + 6U; break;
	case 19: boss.sprite = stage6_pat_spin_back_0 + 8U; break;
	case 22: boss.sprite = stage6_pat_spin_back_0 + 10U; break;
	case 25: boss.sprite = stage6_pat_spin_back_0 + 12U; break;
	case 28: boss.sprite = stage6_pat_spin_back_0 + 14U; break;
	case 31: boss.sprite = stage6_pat_spin_back_0 + 16U; break;
	case 34: boss.sprite = stage6_pat_spin_back_0 + 18U; break;
	case 37: boss.sprite = stage6_pat_back_closed; break;
	case 40:
		boss.yuuka6_anim_frame = 0;
		boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_back_closed;
		return true;
	default:
		break;
	}
	return false;
}

[[nodiscard]] bool yuuka6_anim_vanish(State &state) noexcept
{
	auto &boss = state.boss;
	++boss.yuuka6_anim_frame;
	switch(boss.yuuka6_anim_frame) {
	case 1: boss.sprite = stage6_pat_vanish_0; break;
	case 8: boss.sprite = stage6_pat_vanish_1; break;
	case 15: boss.sprite = stage6_pat_vanish_2; break;
	case 22: boss.sprite = stage6_pat_vanish_3; break;
	case 29: boss.sprite = 0U; break;
	case 36:
		boss.yuuka6_anim_frame = 0;
		boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::vanished;
		return true;
	default:
		break;
	}
	return false;
}

[[nodiscard]] bool yuuka6_anim_appear(State &state) noexcept
{
	auto &boss = state.boss;
	++boss.yuuka6_anim_frame;
	switch(boss.yuuka6_anim_frame) {
	case 1: boss.sprite = stage6_pat_vanish_3; break;
	case 8: boss.sprite = stage6_pat_vanish_2; break;
	case 15: boss.sprite = stage6_pat_vanish_1; break;
	case 22: boss.sprite = stage6_pat_vanish_0; break;
	case 29: boss.sprite = stage6_pat_back_open; break;
	case 36:
		boss.yuuka6_anim_frame = 0;
		boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_back_open;
		return true;
	default:
		break;
	}
	return false;
}

[[nodiscard]] bool yuuka6_anim_parasol_shield(State &state) noexcept
{
	auto &boss = state.boss;
	++boss.yuuka6_anim_frame;
	switch(boss.yuuka6_anim_frame) {
	case 1: boss.sprite = stage6_pat_back_open; break;
	case 7: boss.sprite = stage6_pat_shield_0; break;
	case 13: boss.sprite = stage6_pat_shield_1; break;
	case 19:
		boss.yuuka6_anim_frame = 0;
		boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_shield;
		return true;
	default:
		break;
	}
	return false;
}

void yuuka6_gather_mirrored_wide(State &state) noexcept
{
	auto &boss = state.boss;
	auto &gather = state.bullets.gather_template;
	switch(boss.phase_frame) {
	case 16:
		gather.radius = 5120;
		gather.center.y = word_sub(boss.pos.cur.y, 64);
		gather.center.x = word_add(boss.pos.cur.x, 384);
		gather.ring_points = 16;
		gather.col = 9U;
		gather.angle_delta = static_cast<std::uint8_t>(-2);
		[[fallthrough]];
	case 20:
		gather_add_only(state);
		gather.center.x = word_sub(gather.center.x, 704);
		gather.angle_delta = 2U;
		gather_add_only(state);
		break;
	case 18:
		gather.col = 8U;
		gather_add_only(state);
		gather.center.x = word_add(gather.center.x, 704);
		gather.angle_delta = static_cast<std::uint8_t>(-2);
		gather_add_only(state);
		break;
	case 32:
		circle_add_shrinking(state, gather.center);
		circle_add_shrinking(state, {word_add(gather.center.x, 704), gather.center.y});
		state.circles_color = 15U;
		break;
	default:
		break;
	}
}

void yuuka6_gather_crossed(State &state) noexcept
{
	auto &gather = state.bullets.gather_template;
	gather.angle_delta = static_cast<std::uint8_t>(-2);
	gather_add_only(state);
	gather.angle_delta = 2U;
	gather_add_only(state);
}

void yuuka6_gather_single(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &gather = state.bullets.gather_template;
	switch(boss.phase_frame) {
	case 48:
		presentation.sound(8U);
		gather.radius = 5120;
		gather.center.y = word_add(boss.pos.cur.y, 512);
		gather.center.x = boss.pos.cur.x;
		gather.ring_points = 8;
		gather.col = 9U;
		[[fallthrough]];
	case 52:
		yuuka6_gather_crossed(state);
		break;
	case 50:
		gather.col = 8U;
		yuuka6_gather_crossed(state);
		break;
	case 64:
		circle_add_shrinking(state, gather.center);
		state.circles_color = 15U;
		break;
	default:
		break;
	}
}

void yuuka6_gather_mirrored(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &gather = state.bullets.gather_template;
	switch(boss.phase_frame) {
	case 32:
		presentation.sound(8U);
		gather.radius = 5120;
		gather.ring_points = 8;
		gather.col = 9U;
		[[fallthrough]];
	case 36:
		gather.center.y = word_add(boss.pos.cur.y, 512);
		gather.center.x = boss.pos.cur.x;
		yuuka6_gather_crossed(state);
		gather.center.y = word_add(boss.yuuka6_mirror_pos.y, 512);
		gather.center.x = boss.yuuka6_mirror_pos.x;
		yuuka6_gather_crossed(state);
		break;
	case 34:
		gather.col = 8U;
		gather.center.y = word_add(boss.pos.cur.y, 512);
		gather.center.x = boss.pos.cur.x;
		yuuka6_gather_crossed(state);
		gather.center.y = word_add(boss.yuuka6_mirror_pos.y, 512);
		gather.center.x = boss.yuuka6_mirror_pos.x;
		yuuka6_gather_crossed(state);
		break;
	case 48:
		circle_add_shrinking(state, {
			boss.pos.cur.x, word_add(boss.pos.cur.y, 512)
		});
		circle_add_shrinking(state, {
			boss.yuuka6_mirror_pos.x, word_add(boss.yuuka6_mirror_pos.y, 512)
		});
		state.circles_color = 15U;
		break;
	default:
		break;
	}
}

void yuuka6_gather_shield(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &gather = state.bullets.gather_template;
	switch(boss.phase_frame) {
	case 16:
		presentation.sound(8U);
		gather.radius = 5120;
		gather.center.y = boss.pos.cur.y;
		gather.center.x = boss.pos.cur.x;
		gather.ring_points = 16;
		gather.col = 7U;
		[[fallthrough]];
	case 20:
		yuuka6_gather_crossed(state);
		break;
	case 18:
		gather.col = 6U;
		yuuka6_gather_crossed(state);
		break;
	case 32:
		circle_add_shrinking(state, gather.center);
		state.circles_color = 15U;
		break;
	default:
		break;
	}
}

[[nodiscard]] bool yuuka6_pattern_1ab5d(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_open) {
		(void)yuuka6_anim_parasol_back_close(state);
	}
	yuuka6_gather_mirrored_wide(state);
	switch(boss.phase_frame) {
	case 48:
		bullet.speed = 40U;
		[[fallthrough]];
	case 64:
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = stage6_pat_bullet16_cross_yellow;
		bullet.origin.x = boss.pos.cur.x;
		bullet.origin.y = word_sub(boss.pos.cur.y, 64);
		bullet.angle = 0U;
		bullet.group = BulletGroup::ring;
		bullet.special_motion = BulletSpecialMotion::decelerate_then_turn;
		bullet.count = 20U;
		state.bullets.template_special_angle = static_cast<std::uint8_t>(-64);
		if(!bullet_template_tune(state) || !bullets_add(state, true, true)) return false;
		state.bullets.template_special_angle = 64U;
		if(!bullets_add(state, true, true)) return false;
		presentation.sound(9U);
		bullet.speed = byte_add(bullet.speed, 16);
		break;
	case 80:
		boss.phase_frame = 0;
		boss.mode = stage6_mode_fly;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool yuuka6_pattern_1abe5(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame < 48) {
		if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_closed) {
			(void)yuuka6_anim_parasol_back_pull_left(state);
		}
	} else if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_left) {
		(void)yuuka6_anim_parasol_left_spin_back(state);
		if(boss.phase_frame <= 80) {
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
			bullet.group = BulletGroup::ring;
			bullet.count = boss.statebytes[1U];
			const auto angle = static_cast<std::uint8_t>(-
				static_cast<std::int16_t>(static_cast<std::uint8_t>(boss.phase_frame << 3U))
			);
			bullet.speed = 40U;
			bullet.angle = angle;
			const auto first = pc98::masterlib::vector2(angle, 544);
			bullet.origin = {
				word_add(boss.pos.cur.x, first.x), word_add(boss.pos.cur.y, first.y)
			};
			if(!bullets_add(state)) return false;
			const auto mirror = static_cast<std::uint8_t>(128U - angle);
			const auto second = pc98::masterlib::vector2(mirror, 544);
			bullet.origin = {
				word_add(boss.pos.cur.x, second.x), word_add(boss.pos.cur.y, second.y)
			};
			bullet.speed = static_cast<std::uint8_t>(
				60 - static_cast<std::uint8_t>(boss.phase_frame / 2)
			);
			if(!bullets_add(state)) return false;
			if((state.stage_frame & 3U) == 0U) presentation.sound(9U);
		}
	}
	switch(boss.phase_frame) {
	case 32:
		circle_add_shrinking(state, {
			word_sub(boss.pos.cur.x, 640), word_add(boss.pos.cur.y, 640)
		});
		state.circles_color = 15U;
		break;
	case 96:
		boss.phase_frame = 0;
		boss.mode = stage6_mode_fly;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool yuuka6_pattern_1accc(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_closed) {
		(void)yuuka6_anim_parasol_back_open(state);
	}
	if(boss.phase_frame >= 48 && boss.phase_frame <= 80) {
		if((state.stage_frame & 3U) == 0U) {
			bullet.spawn_type = static_cast<std::uint8_t>(
				((state.stage_frame & 7U) / 4U) + 1U
			);
			bullet.patnum = stage6_pat_bullet16_small_ball_red;
			bullet.origin.y = word_sub(boss.pos.cur.y, 64);
			bullet.group = BulletGroup::random_angle_and_speed;
			bullet.count = 4U;
			bullet.special_motion = BulletSpecialMotion::gravity;
			const auto speed = pc98::th0405::randring_next16_mod(state.random, 24U);
			if(!speed) return false;
			bullet.speed = static_cast<std::uint8_t>(*speed + 8U);
			bullet.origin.x = word_sub(boss.pos.cur.x, 320);
			const auto first_angle = pc98::th0405::randring_next16(state.random);
			if(!first_angle) return false;
			bullet.angle = static_cast<std::uint8_t>(*first_angle);
			state.bullets.special.value = 1U;
			if(!bullet_template_tune(state) || !bullets_add(state, true, true)) return false;
			const auto second_angle = pc98::th0405::randring_next16(state.random);
			if(!second_angle) return false;
			bullet.angle = static_cast<std::uint8_t>(*second_angle);
			bullet.origin.x = word_add(bullet.origin.x, 704);
			if(!bullets_add(state, true, true)) return false;
			presentation.sound(9U);
		}
	} else if(boss.phase_frame > 80) {
		boss.phase_frame = 0;
		boss.mode = stage6_mode_fly;
	}
	yuuka6_gather_mirrored_wide(state);
	return true;
}

void yuuka6_pattern_1ad6f(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame < 64) {
		if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_open) {
			(void)yuuka6_anim_parasol_back_close(state);
		} else if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_closed) {
			(void)yuuka6_anim_parasol_back_pull_forward(state);
		}
	} else if(boss.phase_frame <= 112) {
		boss.sprite = stage6_pat_forward_open;
	} else if(boss.phase_frame >= 288 &&
		boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_closed) {
		if(yuuka6_anim_parasol_back_open(state)) {
			boss.phase_frame = 0;
			boss.mode = stage6_mode_fly;
		}
	} else if(boss.phase_frame >= 288) {
		(void)yuuka6_anim_parasol_left_spin_back(state);
	}
	yuuka6_gather_single(state, presentation);
	if(boss.phase_frame == 64) yuuka6_safetycircle_open(state, presentation);
}

[[nodiscard]] bool yuuka6_pattern_1addb(
	State &state, Presentation &presentation
) noexcept
{
	if((state.stage_frame & 15U) != 0U) return true;
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	bullet.origin = boss.pos.cur;
	if(boss.phase == 6U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = stage6_pat_bullet16_small_ball_red;
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return false;
		bullet.angle = static_cast<std::uint8_t>(*angle);
		bullet.group = BulletGroup::ring;
		bullet.count = 16U;
		bullet.speed = 30U;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		bullet.spawn_type = static_cast<std::uint8_t>(
			BulletSpawnType::bullet16_cloud_forwards
		);
		bullet.patnum = stage6_pat_bullet16_ball_blue;
		bullet.count = static_cast<std::uint8_t>(boss.phase_frame / 16 + 2);
		bullet.speed = 34U;
	} else {
		bullet.spawn_type = static_cast<std::uint8_t>(
			BulletSpawnType::bullet16_cloud_forwards
		);
		bullet.patnum = stage6_pat_bullet16_directional_blue;
		bullet.angle = 0U;
		bullet.group = BulletGroup::stack_aimed;
		bullet.count = 7U;
		bullet.speed = 32U;
		bullet.delta = 10U;
		if(!bullet_template_tune(state) || !bullets_add(state, false, true)) return false;
		presentation.sound(15U);
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.group = BulletGroup::random_angle_and_speed;
		bullet.count = 4U;
		bullet.speed = 24U;
	}
	if(!bullet_template_tune(state) || !bullets_add(state)) return false;
	return true;
}

[[nodiscard]] bool yuuka6_pattern_1ae8f(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame < 64) {
		if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_open) {
			(void)yuuka6_anim_parasol_back_close(state);
		} else if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_closed) {
			(void)yuuka6_anim_parasol_back_pull_forward(state);
		}
		bullet.speed = 16U;
	} else if(boss.phase_frame <= 128) {
		boss.sprite = stage6_pat_forward_open;
		if((state.stage_frame & 7U) == 0U) {
			bullet.origin = boss.pos.cur;
			bullet.spawn_type = static_cast<std::uint8_t>(
				BulletSpawnType::bullet16_cloud_forwards
			);
			bullet.patnum = stage6_pat_bullet16_directional_blue;
			bullet.group = BulletGroup::spread;
			bullet.delta = 8U;
			bullet.count = 3U;
			if(!bullet_template_tune(state)) return false;
			bullet.angle = 96U;
			if(!bullets_add(state)) return false;
			bullet.angle = 32U;
			if(!bullets_add(state)) return false;
			bullet.origin.x = boss.yuuka6_mirror_pos.x;
			if(!bullets_add(state)) return false;
			bullet.angle = 96U;
			if(!bullets_add(state)) return false;
			bullet.speed = byte_add(bullet.speed, 12);
			presentation.sound(3U);
		}
	} else if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_closed) {
		if(yuuka6_anim_parasol_back_open(state)) {
			boss.phase_frame = 0;
			boss.mode = stage6_mode_fly;
		}
	} else {
		(void)yuuka6_anim_parasol_left_spin_back(state);
	}
	yuuka6_gather_mirrored(state, presentation);
	if(boss.phase_frame == 64) {
		auto &laser = state.thicklasers.template_;
		laser.radius_max = static_cast<std::int16_t>(boss.statebytes[0U]);
		laser.radius_speed = 4;
		laser.line_frames = 36;
		laser.static_frames = 40;
		laser.col_outline = 8U;
		laser.origin = {boss.pos.cur.x, word_add(boss.pos.cur.y, 512)};
		thicklaser_add(state, presentation);
		laser.origin = {
			boss.yuuka6_mirror_pos.x, word_add(boss.yuuka6_mirror_pos.y, 640)
		};
		thicklaser_add(state, presentation);
	}
	return true;
}

[[nodiscard]] bool yuuka6_pattern_1afa8(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame < 64) {
		if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_open) {
			(void)yuuka6_anim_parasol_back_close(state);
		} else if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_closed) {
			(void)yuuka6_anim_parasol_back_pull_forward(state);
		}
		boss.statebytes[15U] = 2U;
	} else if(boss.phase_frame <= 128) {
		boss.sprite = stage6_pat_forward_open;
		if((state.stage_frame & 3U) == 0U) {
			bullet.origin.y = word_add(boss.pos.cur.y, 512);
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			bullet.patnum = stage6_pat_bullet16_directional_yellow;
			bullet.group = BulletGroup::spread;
			bullet.count = 5U;
			const auto speed = pc98::th0405::randring_next16_and(state.random, 31U);
			if(!speed) return false;
			bullet.speed = static_cast<std::uint8_t>(*speed + 12U);
			bullet.delta = 16U;
			const auto range = boss.statebytes[15U];
			const auto first = pc98::th0405::randring_next16_mod(
				state.random, static_cast<std::uint16_t>(range) * 2U
			);
			if(!first) return false;
			bullet.angle = static_cast<std::uint8_t>(*first + (64U - range));
			bullet.origin.x = boss.pos.cur.x;
			if(!bullets_add(state)) return false;
			const auto second = pc98::th0405::randring_next16_mod(
				state.random, static_cast<std::uint16_t>(range) * 2U
			);
			if(!second) return false;
			bullet.angle = static_cast<std::uint8_t>(*second + (64U - range));
			bullet.origin.x = boss.yuuka6_mirror_pos.x;
			if(!bullets_add(state)) return false;
			presentation.sound(9U);
			boss.statebytes[15U] = byte_add(range, 6);
		}
	} else if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_closed) {
		if(yuuka6_anim_parasol_back_open(state)) {
			boss.phase_frame = 0;
			boss.mode = stage6_mode_fly;
		}
	} else {
		(void)yuuka6_anim_parasol_left_spin_back(state);
	}
	yuuka6_gather_mirrored(state, presentation);
	return true;
}

[[nodiscard]] bool yuuka6_pattern_1b099(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame <= 48) {
		if(boss.yuuka6_sprite_flag != Yuuka6SpriteFlag::parasol_shield) {
			(void)yuuka6_anim_parasol_shield(state);
		}
	} else if(boss.phase_frame < 136) {
		boss.sprite = static_cast<std::uint8_t>(
			stage6_pat_shield_2 + ((state.stage_frame & 3U) / 2U) * 2U
		);
		if((state.stage_frame & 1U) != 0U && !bullets_add(state, false, true)) {
			return false;
		}
		if((state.stage_frame & 3U) == 0U) presentation.sound(3U);
		if(boss.phase_frame >= 112) bullet.angle = byte_add(bullet.angle, boss.angle);
	} else if(state.rank >= 2U && boss.phase_frame < 150) {
		bullet.angle = byte_add(bullet.angle, -static_cast<std::int16_t>(boss.angle));
		if((state.stage_frame & 1U) != 0U && !bullets_add(state, false, true)) {
			return false;
		}
		if((state.stage_frame & 3U) == 0U) presentation.sound(3U);
	} else {
		boss.sprite = stage6_pat_shield_2;
	}
	yuuka6_gather_shield(state, presentation);
	switch(boss.phase_frame) {
	case 48: {
		const auto aimed = pc98::masterlib::iatan2_source(
			word_sub(boss.pos.cur.y, state.player_pos.cur.y),
			word_sub(boss.pos.cur.x, state.player_pos.cur.x)
		);
		if(!aimed) return false;
		bullet.angle = byte_add(*aimed, 16);
		bullet.spawn_type = static_cast<std::uint8_t>(
			BulletSpawnType::bullet16_cloud_forwards
		);
		bullet.patnum = stage6_pat_bullet16_ball_red;
		bullet.origin = boss.pos.cur;
		bullet.group = BulletGroup::ring;
		bullet.count = 8U;
		bullet.speed = 144U;
		if(state.rank != rank_easy) {
			const auto direction = pc98::th0405::randring_next16_and(state.random, 1U);
			if(!direction) return false;
			boss.angle = *direction != 0U ? 1U : static_cast<std::uint8_t>(-1);
		} else {
			boss.angle = 0U;
		}
		break;
	}
	case 144:
		if(state.rank >= 2U) break;
		[[fallthrough]];
	case 156:
		boss.phase_frame = 0;
		boss.mode = stage6_mode_fly;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool yuuka6_pattern_1b1b1(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame > 48) {
		boss.sprite = static_cast<std::uint8_t>(
			stage6_pat_shield_2 + ((state.stage_frame & 3U) / 2U) * 2U
		);
		if((state.stage_frame & 7U) == 0U) {
			presentation.sound(3U);
			bullet.angle = byte_add(bullet.angle, 2);
			bullet.group = BulletGroup::ring;
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			bullet.patnum = stage6_pat_bullet16_directional_blue;
			bullet.speed = 48U;
			bullet.count = static_cast<std::uint8_t>(boss.phase_frame / 4 + 4);
			if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		}
	} else {
		boss.sprite = stage6_pat_shield_2;
	}
	yuuka6_gather_shield(state, presentation);
	if(boss.phase_frame == 144) {
		boss.phase_frame = 0;
		boss.mode = stage6_mode_fly;
	}
	return true;
}

[[nodiscard]] bool yuuka6_pattern_1b22b(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame > 48) {
		boss.sprite = static_cast<std::uint8_t>(
			stage6_pat_shield_2 + ((state.stage_frame & 3U) / 2U) * 2U
		);
		if((state.stage_frame & 7U) == 0U) {
			const auto first = pc98::th0405::randring_next16(state.random);
			if(!first) return false;
			const auto second = pc98::th0405::randring_next16(state.random);
			if(!second) return false;
			yuuka6_chasecrosses_add(state, static_cast<std::uint8_t>(*first), 32U);
			yuuka6_chasecrosses_add(state, static_cast<std::uint8_t>(*second), 32U);
			presentation.sound(3U);
		}
	} else {
		boss.sprite = stage6_pat_shield_2;
	}
	if(boss.phase_frame == 144) {
		boss.phase_frame = 0;
		boss.mode = stage6_mode_fly;
	}
	return true;
}

[[nodiscard]] bool yuuka6_pattern_1b282(
	State &state
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto subframe = static_cast<std::uint8_t>(boss.phase_frame & 31);
	boss.sprite = static_cast<std::uint8_t>(
		stage6_pat_shield_2 + ((state.stage_frame & 3U) / 2U) * 2U
	);
	if((subframe & 3U) == 0U) {
		bullet.origin = boss.pos.cur;
		bullet.spawn_type = static_cast<std::uint8_t>(
			static_cast<std::uint8_t>(BulletSpawnType::gather_pellet) - bullet.spawn_type
		);
		bullet.patnum = stage6_pat_bullet16_small_ball_red;
		bullet.group = BulletGroup::ring;
		bullet.count = 8U;
		bullet.speed = static_cast<std::uint8_t>(subframe + 32U);
		if(!bullet_template_tune(state)) return false;
		bullet.angle = byte_add(130U, -static_cast<std::int16_t>(bullet.angle));
		if(!bullets_add(state, false, true)) return false;
		bullet.angle = static_cast<std::uint8_t>(128U - bullet.angle);
		if(!bullets_add(state, false, true)) return false;
	}
	if(subframe != 0U) return true;
	bullet.angle = byte_add(bullet.angle, 8);
	auto &gather = state.bullets.gather_template;
	gather.ring_points = 8;
	gather.col = 9U;
	gather.angle_delta = static_cast<std::uint8_t>(-
		static_cast<std::int16_t>(gather.angle_delta)
	);
	gather_add_only(state);
	return true;
}

[[nodiscard]] bool yuuka6_pattern_1b313(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame < 64) {
		if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_open) {
			(void)yuuka6_anim_parasol_back_close(state);
		} else if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_closed) {
			(void)yuuka6_anim_parasol_back_pull_forward(state);
		}
		yuuka6_gather_mirrored(state, presentation);
		return true;
	}
	if(boss.phase_frame <= 192) {
		boss.sprite = stage6_pat_forward_open;
		if((state.stage_frame & 15U) == 0U) {
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			bullet.patnum = stage6_pat_bullet16_directional_blue;
			bullet.speed = 40U;
			bullet.delta = 14U;
			if((state.stage_frame & 31U) == 0U) {
				bullet.count = 10U;
				bullet.group = BulletGroup::spread;
				bullet.angle = 64U;
			} else {
				bullet.count = 7U;
				bullet.group = BulletGroup::spread_aimed;
				bullet.angle = 0U;
			}
			bullet.origin.y = word_add(boss.pos.cur.y, 512);
			bullet.origin.x = boss.pos.cur.x;
			if(!bullets_add(state)) return false;
			bullet.origin.x = boss.yuuka6_mirror_pos.x;
			if(!bullets_add(state)) return false;
			presentation.sound(3U);
		}
		yuuka6_gather_mirrored(state, presentation);
		return true;
	}
	if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_closed) {
		if(yuuka6_anim_parasol_back_open(state)) {
			boss.phase_frame = 0;
			boss.mode = stage6_mode_fly;
		}
	} else {
		(void)yuuka6_anim_parasol_left_spin_back(state);
	}
	yuuka6_gather_mirrored(state, presentation);
	return true;
}

void yuuka6_phase_next(
	State &state, Presentation &presentation, std::uint8_t explosion_type,
	std::int16_t next_end_hp
) noexcept
{
	bullets_clear(state);
	boss_explode_small(state, presentation, explosion_type);
	auto &boss = state.boss;
	++boss.phase;
	boss.phase_frame = 0;
	boss.phase_state = 0U;
	boss.mode = 0U;
	boss.hp = boss.phase_end_hp;
	boss.phase_end_hp = next_end_hp;
	boss.yuuka6_anim_frame = 0;
	boss.sprite = stage6_pat_back_open;
	boss.yuuka6_anim_frame = 0;
	boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_back_open;
}

[[nodiscard]] bool yuuka6_teleport_to(
	State &state, std::int16_t x, std::int16_t y
) noexcept;

[[nodiscard]] bool yuuka6_phase2_fly(State &state) noexcept
{
	auto &boss = state.boss;
	if((boss.phase_state % 6U) < 5U) {
		switch(boss.phase_frame) {
		case 1: {
			const auto angle = stage6_fly_angles[boss.yuuka6_phase2_fly_path][
				boss.phase_state % 6U
			];
			const auto velocity = pc98::masterlib::vector2(angle, 8);
			boss.pos.velocity = {velocity.x, velocity.y};
			break;
		}
		case 112:
			boss.phase_frame = 0;
			++boss.phase_state;
			return true;
		default:
			break;
		}
		boss.pos.cur.x = word_add(boss.pos.cur.x, boss.pos.velocity.x);
		boss.pos.cur.y = word_add(boss.pos.cur.y, boss.pos.velocity.y);
		return false;
	}
	return yuuka6_teleport_to(state, playfield_width / 2, stage6_fly_target_y);
}

[[nodiscard]] bool yuuka6_teleport_to(
	State &state, std::int16_t x, std::int16_t y
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame < 64) {
		if(boss.yuuka6_sprite_flag != Yuuka6SpriteFlag::vanished) {
			(void)yuuka6_anim_vanish(state);
		}
	} else if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::vanished) {
		(void)yuuka6_anim_appear(state);
	}
	switch(boss.phase_frame) {
	case 64:
		boss.pos.cur = {x, y};
		if(boss.yuuka6_mirror_state != 0U) {
			boss.yuuka6_mirror_state = 2U;
			boss.yuuka6_mirror_pos = {word_sub(playfield_width, x), y};
		}
		break;
	case 128:
		boss.phase_frame = 0;
		++boss.phase_state;
		return true;
	default:
		break;
	}
	return false;
}

void yuuka6_fly_sine(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 1) {
		boss.pos.velocity.x = 32;
		boss.angle = 0U;
	}
	boss.pos.cur.x = word_add(boss.pos.cur.x, boss.pos.velocity.x);
	if(boss.pos.cur.x <= 768 || boss.pos.cur.x >= 5376) {
		boss.pos.velocity.x = static_cast<std::int16_t>(-boss.pos.velocity.x);
	}
	boss.pos.cur.y = word_add(1280, pc98::masterlib::vector_component(
		768, pc98::masterlib::sine8(boss.angle)
	));
	boss.angle = byte_add(boss.angle, 2);
}

[[nodiscard]] bool yuuka6_return_to_center(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::vanished) {
		(void)yuuka6_anim_appear(state);
		return false;
	}
	boss.yuuka6_aura_enabled = 0U;
	if(boss.pos.cur.x < 3072) {
		boss.pos.cur.x = word_add(boss.pos.cur.x, 16);
	} else if(boss.pos.cur.x >= 3088) {
		boss.pos.cur.x = word_sub(boss.pos.cur.x, 16);
	} else {
		return true;
	}
	return false;
}

[[nodiscard]] bool yuuka6_mirror_hittest(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.yuuka6_mirror_state != 2U) return true;
	std::uint16_t damage{};
	if(!shots_hit(state, boss.yuuka6_mirror_pos, {384, 768}, false, damage)) {
		return false;
	}
	boss.yuuka6_mirror_damage = static_cast<std::uint8_t>(damage);
	if(boss.yuuka6_mirror_damage != 0U) presentation.sound(4U);
	boss.hp = static_cast<std::int16_t>(
		static_cast<int>(boss.hp) - static_cast<int>(boss.yuuka6_mirror_damage)
	);
	return true;
}

[[nodiscard]] bool yuuka6_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 0) {
			state.midboss.frames_until = 0U;
			boss.yuuka6_aura_enabled = 0U;
			boss.yuuka6_mirror_state = 0U;
		}
		++boss.phase_frame;
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(boss.phase_frame > 128) {
			++boss.phase;
			boss.phase_frame = 0;
			presentation.sound(13U);
			boss.yuuka6_last_pattern = 0U;
		}
		break;

	case 1U: {
		++boss.phase_frame;
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.pos.velocity.x = 0;
			boss.phase_state = 0U;
			boss.mode = 0U;
			boss.hp = stage6_yuuka_hp;
			boss.phase_end_hp = 10600;
			boss.phase_frame = 0;
			boss.yuuka6_anim_frame = 0;
			boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_back_open;
			const auto path = pc98::th0405::randring_next16_and(state.random, 1U);
			if(!path) return false;
			boss.yuuka6_phase2_fly_path = static_cast<std::uint8_t>(*path);
		}
		break;
	}

	case 2U: {
		bool phase_over{};
		switch(boss.mode) {
		case 0U:
			if(!yuuka6_pattern_1ab5d(state, presentation)) return false;
			break;
		case 1U:
			if(!yuuka6_pattern_1abe5(state, presentation)) return false;
			break;
		case 2U:
			if(!yuuka6_pattern_1accc(state, presentation)) return false;
			break;
		case stage6_mode_fly:
			if(yuuka6_phase2_fly(state)) {
				boss.mode = static_cast<std::uint8_t>(boss.phase_state % 3U);
				phase_over = boss.phase_state >= 10U;
			}
			break;
		default:
			return false;
		}
		if(phase_over) {
			yuuka6_phase_next(state, presentation, 0U, 7600);
			break;
		}
		if(boss.sprite == 0U) {
			++boss.phase_frame;
			break;
		}
		const auto hit = boss_hittest(state, presentation);
		if(hit == HitResult::unsupported) return false;
		if(hit == HitResult::none) break;
		if(!boss_score_bonus(state, presentation, 20U) ||
			!boss_items_drop(state, presentation)) return false;
		yuuka6_phase_next(state, presentation, 0U, 7600);
		break;
	}

	case 3U:
		++boss.phase_frame;
		if(yuuka6_teleport_to(state, 3072, stage6_fly_target_y)) {
			++boss.phase;
			boss.phase_frame = 0;
			boss.phase_state = 0U;
		}
		break;

	case 4U: {
		bool phase_over{};
		switch(boss.mode) {
		case 0U:
			yuuka6_pattern_1ad6f(state, presentation);
			break;
		case stage6_mode_fly: {
			const auto x = pc98::th0405::randring_next16_mod(state.random, 4608U);
			if(!x) return false;
			if(yuuka6_teleport_to(state, static_cast<std::int16_t>(*x + 768U),
				stage6_fly_target_y)) {
				boss.mode = 0U;
				phase_over = boss.phase_state >= 10U;
			}
			break;
		}
		default:
			return false;
		}
		if(phase_over) {
			yuuka6_phase_next(state, presentation, 0U, 5400);
			boss.yuuka6_aura_enabled = 1U;
			break;
		}
		if(boss.sprite == 0U || boss.mode == stage6_mode_fly) {
			++boss.phase_frame;
			break;
		}
		const auto hit = boss_hittest(state, presentation);
		if(hit == HitResult::unsupported) return false;
		if(hit == HitResult::none) break;
		if(!boss_score_bonus(state, presentation, 20U) ||
			!boss_items_drop(state, presentation)) return false;
		yuuka6_phase_next(state, presentation, 0U, 5400);
		boss.yuuka6_aura_enabled = 1U;
		break;
	}

	case 5U:
	case 9U:
		++boss.phase_frame;
		if(boss.yuuka6_sprite_flag != Yuuka6SpriteFlag::vanished) {
			(void)yuuka6_anim_vanish(state);
			break;
		}
		++boss.phase;
		boss.phase_frame = 0;
		break;

	case 6U:
	case 10U:
		if(!yuuka6_pattern_1addb(state, presentation)) return false;
		++boss.phase_frame;
		yuuka6_fly_sine(state);
		if(boss.phase_frame < 320 || boss.pos.cur.y != stage6_fly_target_y) break;
		boss_explode_small(state, presentation, 2U);
		bullets_clear(state);
		++boss.phase;
		boss.phase_frame = 0;
		boss.yuuka6_anim_frame = 0;
		break;

	case 7U:
	case 11U: {
		const auto hit = boss_hittest(state, presentation);
		if(hit == HitResult::unsupported) return false;
		if(!yuuka6_return_to_center(state)) break;
		++boss.phase;
		boss.phase_state = 0U;
		boss.mode = stage6_mode_fly;
		boss.phase_frame = 0;
		boss.yuuka6_anim_frame = 0;
		boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_back_closed;
		boss.yuuka6_mirror_state = 1U;
		boss.yuuka6_last_pattern = 255U;
		break;
	}

	case 8U:
	case 12U: {
		bool phase_over{};
		switch(boss.mode) {
		case 0U:
			if(!yuuka6_pattern_1ae8f(state, presentation)) return false;
			break;
		case 1U:
			if(!yuuka6_pattern_1afa8(state, presentation)) return false;
			break;
		case 2U:
			if(!yuuka6_pattern_1b313(state, presentation)) return false;
			break;
		case stage6_mode_fly: {
			const auto x = pc98::th0405::randring_next16_mod(state.random, 2304U);
			if(!x) return false;
			if(yuuka6_teleport_to(state, static_cast<std::int16_t>(*x + 768U),
				stage6_fly_target_y)) {
				do {
					const auto mode = pc98::th0405::randring_next16_mod(state.random, 3U);
					if(!mode) return false;
					boss.mode = static_cast<std::uint8_t>(*mode);
				} while(boss.yuuka6_last_pattern == boss.mode);
				boss.yuuka6_last_pattern = boss.mode;
				phase_over = boss.phase_state >= 10U;
			}
			break;
		}
		default:
			return false;
		}
		if(phase_over) {
			yuuka6_phase_next(state, presentation, 0U,
				boss.phase == 8U ? 3400 : 1200);
			if(boss.phase == 9U) boss.yuuka6_aura_enabled = 1U;
			boss.yuuka6_mirror_state = 0U;
			break;
		}
		if(boss.sprite == 0U) {
			++boss.phase_frame;
			break;
		}
		if(boss.mode <= 2U) {
			const auto hit = boss_hittest(state, presentation);
			if(hit == HitResult::unsupported || !yuuka6_mirror_hittest(state, presentation)) {
				return false;
			}
		} else {
			++boss.phase_frame;
		}
		if(boss.hp > boss.phase_end_hp) break;
		if(!boss_score_bonus(state, presentation, 20U) ||
			!boss_items_drop(state, presentation)) return false;
		yuuka6_phase_next(state, presentation, 0U,
			boss.phase == 8U ? 3400 : 1200);
		if(boss.phase == 9U) boss.yuuka6_aura_enabled = 1U;
		boss.yuuka6_mirror_state = 0U;
		break;
	}

	case 13U:
		++boss.phase_frame;
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(!yuuka6_return_to_center(state)) break;
		++boss.phase;
		boss.phase_state = 0U;
		boss.mode = 0U;
		boss.phase_frame = 0;
		boss.yuuka6_anim_frame = 0;
		boss.yuuka6_sprite_flag = Yuuka6SpriteFlag::parasol_back_closed;
		boss.yuuka6_mirror_state = 1U;
		break;

	case 14U: {
		bool phase_over{};
		switch(boss.mode) {
		case 0U:
			if(!yuuka6_pattern_1b099(state, presentation)) return false;
			break;
		case 1U:
			if(!yuuka6_pattern_1b1b1(state, presentation)) return false;
			break;
		case 2U:
			if(!yuuka6_pattern_1b22b(state, presentation)) return false;
			break;
		case stage6_mode_fly:
			++boss.phase_state;
			boss.mode = static_cast<std::uint8_t>(boss.phase_state % 3U);
			phase_over = boss.phase_state >= 18U;
			break;
		default:
			return false;
		}
		if(phase_over) {
			yuuka6_phase_next(state, presentation, 3U, 0);
			boss.sprite = stage6_pat_shield_2;
			break;
		}
		const auto hit = boss_hittest(state, presentation);
		if(hit == HitResult::unsupported) return false;
		if(hit == HitResult::none) break;
		if(!boss_score_bonus(state, presentation, 20U) ||
			!boss_items_drop(state, presentation)) return false;
		yuuka6_phase_next(state, presentation, 3U, 0);
		boss.sprite = stage6_pat_shield_2;
		break;
	}

	case 15U: {
		const auto hit = boss_hittest(state, presentation);
		if(hit == HitResult::unsupported) return false;
		if(boss.phase_frame >= 128) {
			++boss.phase;
			state.bullets.template_.spawn_type = static_cast<std::uint8_t>(
				BulletSpawnType::pellet
			);
			state.bullets.template_.angle = 0U;
		}
		break;
	}

	case 16U: {
		if(!yuuka6_pattern_1b282(state)) return false;
		const auto hit = boss_hittest(state, presentation);
		if(hit == HitResult::unsupported) return false;
		if(hit == HitResult::none && boss.phase_frame < 2500) break;
		boss_explode_small(state, presentation, 1U);
		++boss.phase;
		boss.phase_state = boss.phase_frame < 2500 ? 1U : 0U;
		boss.phase_frame = 0;
		boss.mode = 0U;
		state.palette_tone = 100U;
		state.palette_changed = true;
		break;
	}

	case 17U:
		++boss.phase_frame;
		if(boss.phase_frame == 16) boss_explode_small(state, presentation, 4U);
		if(boss.phase_frame == 32) {
			boss_explode_big(state, presentation, 2U);
			boss.phase = stage6_phase_explode_big;
			state.bullets.zap.set_active(boss.phase_state != 0U);
			if(boss.phase_state != 0U && !boss_score_bonus(state, presentation, 70U)) {
				return false;
			}
			boss.sprite = pat_enemy_kill;
			boss.phase_frame = 0;
			presentation.sound(12U);
			state.palette_changed = true;
			state.player_invincibility_time = boss_defeat_invincibility_frames;
		}
		break;

	case stage6_phase_explode_big:
		if(boss.phase_frame < 12) {
			state.playfield_shake_x = (state.stage_frame & 1U) == 0U ? -4 : 4;
			state.playfield_shake_y = (state.stage_frame & 3U) <= 1U ? -4 : 4;
		}
		state.bullets.slowdown_factor = 2U;
		++boss.phase_frame;
		if((boss.phase_frame & 7) == 0) {
			++boss.sprite;
			if(boss.sprite >= static_cast<std::uint8_t>(pat_enemy_kill_last + 1U)) {
				boss.phase = stage6_phase_none;
				boss.phase_frame = 0;
				state.bombing_disabled = true;
			}
		}
		return true;

	case stage6_phase_none:
		state.palette_tone = 60U;
		state.palette_changed = true;
		state.stage_clear_requested = true;
		return true;

	default:
		return false;
	}
	state.reimu_a.homing_target = boss.pos.cur;
	thicklasers_update_and_hittest(state, presentation);
	if(!yuuka6_customs_update(state, presentation)) return false;
	return true;
}

} // namespace

void stage6_fights_setup(State &state) noexcept
{
	auto &boss = state.boss;
	state.midboss_active = false;
	state.midboss.frames_until = 60000U;
	state.boss_active = false;
	boss.phase = 0U;
	boss.mode = 0U;
	boss.phase_state = 0U;
	boss.phase_frame = 0;
	boss.pos.cur = {3072, 1280};
	boss.pos.prev = boss.pos.cur;
	boss.pos.velocity = {};
	boss.damage_this_frame = 0U;
	boss.phase_timed_out = true;
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
	boss.sprite = stage6_pat_back_open;
	boss.hitbox_radius = {384, 768};
	switch(state.rank) {
	case 0U:
		boss.statebytes[0U] = 48U;
		boss.statebytes[1U] = 1U;
		break;
	case 1U:
		boss.statebytes[0U] = 64U;
		boss.statebytes[1U] = 1U;
		break;
	case 2U:
		boss.statebytes[0U] = 80U;
		boss.statebytes[1U] = 2U;
		break;
	default:
		boss.statebytes[0U] = 96U;
		boss.statebytes[1U] = 4U;
		break;
	}
}

void stage6_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}

bool stage6_fights_update(State &state, Presentation &presentation) noexcept
{
	if(!state.boss_active) return true;
	return yuuka6_update(state, presentation);
}

void stage6_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	if(!state.boss_active) return;
	auto &boss = state.boss;
	if(boss.phase == stage6_phase_none || boss.phase == stage6_phase_explode_big) {
		return;
	}
	if(boss.sprite != 0U) {
		boss.yuuka6_damage_flash = byte_add(
			boss.yuuka6_damage_flash, boss.damage_this_frame != 0U ? 1 : 0
		);
		boss.damage_this_frame = 0U;
		if(boss.yuuka6_mirror_state == 2U) {
			boss.yuuka6_mirror_damage_flash = byte_add(
				boss.yuuka6_mirror_damage_flash,
				boss.yuuka6_mirror_damage != 0U ? 1 : 0
			);
			boss.yuuka6_mirror_damage = 0U;
		}
	}
	stage1_fights_render_tail(state, presentation);
	thicklasers_render(state, presentation);
	yuuka6_customs_render_tail(state);
}

} // namespace th04::core

namespace th04::core {
namespace {

constexpr std::uint8_t stage4_pat_reimu_animated = pat_stage + 8U;
constexpr std::uint8_t stage4_pat_reimu_orb_blue = pat_stage + 12U;
constexpr std::uint8_t stage4_pat_reimu_orb_yellow = pat_stage + 16U;
constexpr std::uint8_t stage4_pat_marisa_bit = pat_stage + 8U;
constexpr std::uint8_t stage4_pat_bullet16_ball_blue = 57U;
constexpr std::uint8_t stage4_pat_bullet16_ball_red = 61U;
constexpr std::uint8_t stage4_pat_bullet16_star = 56U;
constexpr std::uint8_t stage4_pat_bullet16_directional_blue = 76U;
constexpr std::uint8_t stage4_pat_bullet16_directional_yellow = 92U;
constexpr std::uint8_t stage4_pat_bullet16_directional_yellow_first = 84U;
constexpr std::uint8_t stage4_pat_reimu_still = pat_stage;
constexpr std::uint8_t stage4_pat_marisa_fight = pat_stage + 1U;
constexpr std::int16_t stage4_reimu_hp = 9100;
constexpr std::int16_t stage4_marisa_hp = 6000;
constexpr std::int16_t stage4_boss_center_x = 3072;
constexpr std::int16_t stage4_boss_center_y = 1536;
constexpr std::uint8_t stage4_mode_interval = 0xFFU;
constexpr std::uint8_t stage4_phase_explode_big = 254U;
constexpr std::uint8_t stage4_phase_none = 255U;

[[nodiscard]] constexpr std::int16_t stage4_word_negate(std::int16_t value) noexcept
{
	return word_sub(0, value);
}

[[nodiscard]] constexpr bool stage4_player_in_box(
	const State &state, Point center, std::int16_t half_extent
) noexcept
{
	const auto left = word_sub(center.x, half_extent);
	const auto top = word_sub(center.y, half_extent);
	const auto extent = static_cast<std::uint16_t>(half_extent * 2);
	return static_cast<std::uint16_t>(word_sub(state.player_pos.cur.x, left)) < extent &&
		static_cast<std::uint16_t>(word_sub(state.player_pos.cur.y, top)) < extent;
}

[[nodiscard]] bool stage4_shots_hit(
	State &state, Point center, Point radius, std::uint16_t &damage
) noexcept
{
	return shots_hit(state, center, radius, false, damage);
}

[[nodiscard]] bool reimu_orbs_add_moving(State &state) noexcept
{
	auto &boss = state.boss;
	for(auto &orb : boss.reimu_orbs) {
		if(orb.flag != ReimuOrbFlag::free) continue;
		orb.flag = ReimuOrbFlag::move;
		orb.center = boss.reimu_orb_template.center;
		orb.origin = boss.reimu_orb_template.origin;
		orb.unknown = boss.reimu_orb_template.unknown;
		orb.move_speed = boss.reimu_orb_template.move_speed;
		orb.angle = boss.reimu_orb_template.angle;
		orb.distance = 0;
		const auto velocity = pc98::masterlib::vector2(orb.angle,
			static_cast<std::int16_t>(orb.move_speed));
		orb.velocity = {velocity.x, velocity.y};
		return true;
	}
	return true;
}

[[nodiscard]] bool reimu_orbs_add_spinning(
	State &state, std::uint8_t angle_offset, std::uint8_t count
) noexcept
{
	if(count == 0U) return false;
	auto &boss = state.boss;
	std::uint8_t spawned{};
	for(auto &orb : boss.reimu_orbs) {
		if(orb.flag != ReimuOrbFlag::free) continue;
		orb.flag = ReimuOrbFlag::moveout_spin;
		orb.spin_time = boss.reimu_orb_template.spin_time;
		orb.center = boss.reimu_orb_template.center;
		orb.origin = boss.reimu_orb_template.origin;
		orb.unknown = boss.reimu_orb_template.unknown;
		orb.move_speed = boss.reimu_orb_template.move_speed;
		orb.angle = byte_add(static_cast<std::uint8_t>(
			(static_cast<unsigned>(spawned) << 8U) / count
		), static_cast<std::int16_t>(angle_offset));
		orb.distance = 0;
		orb.angle_speed = boss.reimu_orb_template.angle_speed;
		++spawned;
		if(spawned >= count) break;
	}
	return true;
}

[[nodiscard]] bool reimu_orbs_update(State &state) noexcept
{
	for(auto &orb : state.boss.reimu_orbs) {
		if(orb.flag == ReimuOrbFlag::free) continue;
		if(orb.flag == ReimuOrbFlag::moveout_spin) {
			orb.center.x = word_add(orb.origin.x, pc98::masterlib::vector_component(
				orb.distance, pc98::masterlib::cosine8(orb.angle)
			));
			orb.center.y = word_add(orb.origin.y, pc98::masterlib::vector_component(
				orb.distance, pc98::masterlib::sine8(orb.angle)
			));
			if(orb.distance < 1024) orb.distance = word_add(orb.distance, 64);
			--orb.spin_time;
			orb.angle = byte_add(orb.angle, static_cast<std::int8_t>(orb.angle_speed));
			if(orb.spin_time == 0U) {
				orb.angle = byte_add(orb.angle,
					static_cast<std::int8_t>(orb.angle_speed) >= 0 ? 64 : -64);
				const auto velocity = pc98::masterlib::vector2(orb.angle,
					static_cast<std::int16_t>(orb.move_speed));
				orb.velocity = {velocity.x, velocity.y};
				orb.flag = ReimuOrbFlag::move;
			}
		} else if(orb.flag == ReimuOrbFlag::move) {
			++orb.spin_time;
			orb.center.x = word_add(orb.center.x, orb.velocity.x);
			if(orb.center.x < 0 || orb.center.x > playfield_width) {
				orb.velocity.x = stage4_word_negate(orb.velocity.x);
			}
			orb.center.y = word_add(orb.center.y, orb.velocity.y);
			if(orb.center.y >= playfield_height) orb.flag = ReimuOrbFlag::free;
			orb.velocity.y = word_add(orb.velocity.y, 1);
		} else {
			return false;
		}
		std::uint16_t ignored_damage{};
		if(!stage4_shots_hit(state, orb.center, {192, 192}, ignored_damage)) return false;
		if(stage4_player_in_box(state, orb.center, 192)) state.player_is_hit = true;
	}
	return true;
}

[[nodiscard]] std::uint8_t reimu_charge_animate(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	switch(boss.phase_frame) {
	case 14:
		state.bullets.gather_template.center = {
			word_add(boss.pos.cur.x, 64), word_sub(boss.pos.cur.y, 448)
		};
		state.bullets.gather_template.ring_points = 16;
		state.bullets.gather_template.radius = 4096;
		state.bullets.gather_template.col = 9U;
		gather_add_only(state);
		boss.sprite = pat_stage + 1U;
		presentation.sound(8U);
		state.circles_color = 15U;
		break;
	case 16:
		state.bullets.gather_template.col = 8U;
		gather_add_only(state);
		break;
	case 18: gather_add_only(state); break;
	case 22: boss.sprite = pat_stage + 2U; break;
	case 26: boss.sprite = pat_stage + 3U; break;
	case 30:
		boss.sprite = pat_stage + 4U;
		circle_add_shrinking(state, state.bullets.gather_template.center);
		break;
	case 34: boss.sprite = pat_stage + 5U; break;
	case 38: boss.sprite = pat_stage + 6U; break;
	case 42: boss.sprite = pat_stage + 7U; break;
	case 46:
		boss.sprite = pat_stage + 1U;
		presentation.sound(3U);
		return 2U;
	default: break;
	}
	return boss.phase_frame < 46 ? 0U : 1U;
}

void reimu_pulse(State &state) noexcept
{
	auto &boss = state.boss;
	if(!boss.reimu_bg_pulse_dimming) {
		++state.palette[0].r;
		if(state.palette[0].r >= 240U) boss.reimu_bg_pulse_dimming = true;
	} else {
		--state.palette[0].r;
		if(state.palette[0].r <= 64U) boss.reimu_bg_pulse_dimming = false;
	}
	state.palette_changed = true;
}

[[nodiscard]] bool reimu_pattern_aimed_sweep(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = reimu_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = stage4_pat_bullet16_directional_yellow_first;
		bullet.speed = 96U;
		const auto aimed = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, boss.pos.cur.y),
			word_sub(state.player_pos.cur.x, boss.pos.cur.x)
		);
		if(!aimed) return false;
		boss.angle = *aimed;
		bullet.group = BulletGroup::spread;
		bullet.count = 6U;
		bullet.delta = boss.statebytes[5U];
		if(!bullet_template_tune(state)) return false;
		boss.reimu_sweep_angle_delta = state.player_pos.cur.x < playfield_width / 2 ?
			static_cast<std::uint8_t>(-2) : 2U;
	}
	if(charge == 1U) {
		if((boss.phase_frame & 3) == 0) {
			bullet.angle = boss.angle;
			if(!bullets_add(state)) return false;
			presentation.sound(3U);
			if(boss.phase_frame >= 64) {
				boss.angle = byte_add(boss.angle,
					static_cast<std::int8_t>(boss.reimu_sweep_angle_delta));
			}
		}
		if(boss.phase_frame >= 112) {
			boss.sprite = stage4_pat_reimu_still;
			boss.phase_frame = 0;
			boss.mode = stage4_mode_interval;
		}
	}
	return true;
}

[[nodiscard]] bool reimu_pattern_turning_spread(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = reimu_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
		bullet.patnum = stage4_pat_bullet16_ball_blue;
		bullet.speed = 85U;
		bullet.angle = 0U;
		bullet.special_motion = BulletSpecialMotion::decelerate_then_turn_aimed;
		state.bullets.special.value = boss.statebytes[2U];
		bullet.group = BulletGroup::spread_aimed;
		bullet.count = 9U;
		bullet.delta = 6U;
		if(!bullet_template_tune(state) || !bullets_add(state, true, true)) return false;
	}
	if(charge == 1U && boss.phase_frame >= 128) {
		boss.sprite = stage4_pat_reimu_still;
		boss.phase_frame = 0;
		boss.mode = stage4_mode_interval;
	}
	return true;
}

[[nodiscard]] bool reimu_pattern_orb_ring(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 32) {
		boss.sprite = stage4_pat_reimu_animated;
		boss.reimu_orb_template.spin_time = 64U;
		boss.reimu_orb_template.move_speed = 56U;
		boss.reimu_orb_template.origin = boss.pos.cur;
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle || !reimu_orbs_add_spinning(state, static_cast<std::uint8_t>(*angle),
			boss.statebytes[0U])) return false;
		presentation.sound(8U);
	}
	if(boss.phase_frame >= 96) {
		boss.phase_frame = 0;
		boss.mode = stage4_mode_interval;
		boss.reimu_orb_template.angle_speed = static_cast<std::uint8_t>(-
			static_cast<std::int8_t>(boss.reimu_orb_template.angle_speed));
	}
	return true;
}

[[nodiscard]] bool reimu_pattern_random_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = reimu_charge_animate(state, presentation);
	if(charge != 1U) return true;
	if((boss.phase_frame % 32) == 0) {
		const auto x = pc98::th0405::randring_next16_mod(state.random, 1024U);
		if(!x) return false;
		const auto y = pc98::th0405::randring_next16_mod(state.random, 1024U);
		if(!y) return false;
		bullet.origin = {word_add(word_sub(boss.pos.cur.x, 512),
			std::bit_cast<std::int16_t>(*x)), word_add(word_sub(boss.pos.cur.y, 512),
			std::bit_cast<std::int16_t>(*y))};
		(void)pc98::th0405::randring_next16(state.random);
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_backwards);
		bullet.patnum = stage4_pat_bullet16_ball_blue;
		bullet.speed = 16U;
		bullet.angle = 0U;
		bullet.special_motion = BulletSpecialMotion::speedup;
		state.bullets.special.value = 1U;
		bullet.group = BulletGroup::ring;
		bullet.count = 16U;
		if(!bullet_template_tune(state) || !bullets_add(state, true, true)) return false;
		presentation.sound(3U);
	}
	if((boss.phase_frame % 32) == 16) {
		const auto x = pc98::th0405::randring_next16_mod(state.random, 1024U);
		if(!x) return false;
		const auto y = pc98::th0405::randring_next16_mod(state.random, 1024U);
		if(!y) return false;
		bullet.origin = {word_add(word_sub(boss.pos.cur.x, 512),
			std::bit_cast<std::int16_t>(*x)), word_add(word_sub(boss.pos.cur.y, 512),
			std::bit_cast<std::int16_t>(*y))};
		(void)pc98::th0405::randring_next16(state.random);
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.speed = 24U;
		bullet.group = BulletGroup::stack;
		bullet.count = 4U;
		bullet.delta = 8U;
		if(!bullet_template_tune(state)) return false;
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return false;
		bullet.angle = static_cast<std::uint8_t>(*angle);
		for(std::uint8_t i{}; i < 12U; ++i) {
			if(!bullets_add(state)) return false;
			bullet.angle = byte_add(bullet.angle, 21);
		}
		presentation.sound(3U);
	}
	if(boss.phase_frame >= 288) {
		boss.sprite = stage4_pat_reimu_still;
		boss.phase_frame = 0;
		boss.mode = stage4_mode_interval;
	}
	return true;
}

[[nodiscard]] bool reimu_pattern_jittered_pellets(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = reimu_charge_animate(state, presentation);
	if(charge == 1U) {
		if((boss.phase_frame & 3) == 0) {
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
			bullet.speed = 128U;
			const auto jitter = pc98::th0405::randring_next16_and(state.random, 7U);
			if(!jitter) return false;
			bullet.angle = byte_add(static_cast<std::uint8_t>(*jitter), -68);
			bullet.group = BulletGroup::spread;
			bullet.count = boss.statebytes[3U];
			bullet.delta = boss.statebytes[4U];
			if(!bullets_add(state, false, true)) return false;
			if((boss.phase_frame & 15) == 0) {
				const auto x = pc98::th0405::randring_next16_mod(state.random, 1024U);
				if(!x) return false;
				const auto y = pc98::th0405::randring_next16_mod(state.random, 1024U);
				if(!y) return false;
				bullet.origin = {word_add(word_sub(boss.pos.cur.x, 512),
					std::bit_cast<std::int16_t>(*x)), word_add(word_sub(boss.pos.cur.y, 512),
					std::bit_cast<std::int16_t>(*y))};
				bullet.group = BulletGroup::stack_aimed;
				bullet.count = static_cast<std::uint8_t>(state.rank + 3U);
				bullet.delta = 16U;
				bullet.spawn_type = static_cast<std::uint8_t>(
					BulletSpawnType::bullet16_cloud_forwards
				);
				bullet.speed = 32U;
				bullet.angle = 0U;
				bullet.patnum = stage4_pat_bullet16_ball_red;
				if(!bullets_add(state)) return false;
				presentation.sound(3U);
			}
		}
		if(boss.phase_frame >= 192) {
			boss.sprite = stage4_pat_reimu_still;
			boss.phase_frame = 0;
			boss.mode = stage4_mode_interval;
		}
	}
	return true;
}

[[nodiscard]] bool reimu_pattern_aimed_random(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = reimu_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.speed = 54U;
		const auto aimed = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, boss.pos.cur.y),
			word_sub(state.player_pos.cur.x, boss.pos.cur.x)
		);
		if(!aimed) return false;
		bullet.angle = *aimed;
		bullet.group = BulletGroup::random_angle_and_speed;
		bullet.count = 3U;
		if(!bullet_template_tune(state)) return false;
	}
	if(charge != 1U) return true;
	if(boss.phase_frame < 96) {
		if((boss.phase_frame & 1) == 0) {
			if(!bullets_add(state)) return false;
			presentation.sound(3U);
		}
	} else if(boss.phase_frame <= 128) {
		if((boss.phase_frame % 16) == 0) {
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_backwards);
			bullet.patnum = stage4_pat_bullet16_ball_blue;
			bullet.group = BulletGroup::ring;
			bullet.count = 32U;
			const auto angle = pc98::th0405::randring_next16(state.random);
			if(!angle) return false;
			bullet.angle = static_cast<std::uint8_t>(*angle);
			bullet.speed = 48U;
			bullet.special_motion = BulletSpecialMotion::speedup;
			state.bullets.special.value = 1U;
			if(!bullet_template_tune(state) || !bullets_add(state, true, false)) return false;
			presentation.sound(15U);
		}
	} else {
		boss.sprite = stage4_pat_reimu_still;
		boss.phase_frame = 0;
		boss.mode = stage4_mode_interval;
	}
	return true;
}

[[nodiscard]] bool reimu_pattern_orb_spiral(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 32) {
		boss.sprite = stage4_pat_reimu_animated;
		boss.reimu_orb_template.angle = 0U;
		boss.reimu_orb_template.move_speed = 56U;
		boss.reimu_orb_template.center = boss.pos.cur;
		presentation.sound(8U);
	}
	if(boss.phase_frame >= 32 && (boss.phase_frame % boss.statebytes[1U]) == 0) {
		boss.reimu_orb_template.angle = byte_add(boss.reimu_orb_template.angle,
			-static_cast<std::int8_t>(boss.reimu_orb_template.angle_speed));
		if(!reimu_orbs_add_moving(state)) return false;
	}
	if(boss.phase_frame >= 180) {
		boss.phase_frame = 0;
		boss.mode = stage4_mode_interval;
		boss.reimu_orb_template.angle_speed = static_cast<std::uint8_t>(-
			static_cast<std::int8_t>(boss.reimu_orb_template.angle_speed));
	}
	return true;
}

[[nodiscard]] bool reimu_pattern_scattered_stacks(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame == 32) {
		boss.sprite = stage4_pat_reimu_animated;
		boss.angle = 0U;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
		bullet.patnum = stage4_pat_bullet16_ball_blue;
		bullet.angle = static_cast<std::uint8_t>(-64);
		bullet.special_motion = BulletSpecialMotion::none;
		bullet.delta = 8U;
	}
	if(boss.phase_frame < 32 || (boss.phase_frame % 16) != 0) return true;
	const auto x = pc98::th0405::randring_next16_mod(state.random, 1024U);
	if(!x) return false;
	const auto y = pc98::th0405::randring_next16_mod(state.random, 1024U);
	if(!y) return false;
	bullet.origin = {word_add(word_sub(boss.pos.cur.x, 512), std::bit_cast<std::int16_t>(*x)),
		word_add(word_sub(boss.pos.cur.y, 512), std::bit_cast<std::int16_t>(*y))};
	(void)pc98::th0405::randring_next16(state.random);
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
	bullet.speed = 24U;
	bullet.group = BulletGroup::stack;
	bullet.count = 4U;
	bullet.delta = 10U;
	if(!bullet_template_tune(state)) return false;
	const auto angle = pc98::th0405::randring_next16(state.random);
	if(!angle) return false;
	bullet.angle = static_cast<std::uint8_t>(*angle);
	for(std::uint8_t i{}; i < 8U; ++i) {
		if(!bullets_add(state)) return false;
		bullet.angle = byte_add(bullet.angle, 32);
	}
	presentation.sound(3U);
	return true;
}

[[nodiscard]] bool reimu_pattern_mirrored_spreads(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = reimu_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
		bullet.patnum = stage4_pat_bullet16_ball_blue;
		bullet.angle = static_cast<std::uint8_t>(-64);
		bullet.special_motion = BulletSpecialMotion::none;
		bullet.delta = 8U;
		boss.reimu_pattern8_angle = boss.reimu_pattern8_angle != 0x78U ?
			0x78U : static_cast<std::uint8_t>(-0x78);
	}
	if(charge == 1U) {
		if((boss.phase_frame & 3) == 0) {
			const auto speed = pc98::th0405::randring_next16_and(state.random, 31U);
			if(!speed) return false;
			bullet.speed = byte_add(static_cast<std::uint8_t>(*speed), 16);
			bullet.group = BulletGroup::spread;
			const auto count = pc98::th0405::randring_next16_and(state.random, 3U);
			if(!count) return false;
			bullet.count = byte_add(static_cast<std::uint8_t>(*count), 2);
			if(!bullet_template_tune(state)) return false;
			const auto x = pc98::th0405::randring_next16_mod(state.random, 1024U);
			if(!x) return false;
			const auto y = pc98::th0405::randring_next16_mod(state.random, 1024U);
			if(!y) return false;
			bullet.origin = {word_add(word_sub(boss.pos.cur.x, 512),
				std::bit_cast<std::int16_t>(*x)), word_add(word_sub(boss.pos.cur.y, 512),
				std::bit_cast<std::int16_t>(*y))};
			bullet.angle = byte_add(bullet.angle,
				static_cast<std::int8_t>(boss.reimu_pattern8_angle));
			if(!bullets_add(state, true, false)) return false;
			bullet.angle = byte_add(bullet.angle, 128);
			if(!bullets_add(state, true, false)) return false;
			presentation.sound(3U);
		}
		if(boss.phase_frame >= 224) {
			boss.sprite = stage4_pat_reimu_still;
			boss.phase_frame = 0;
			boss.mode = stage4_mode_interval;
		}
	}
	return true;
}

[[nodiscard]] bool reimu_pattern_aimed_stacks(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = reimu_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
		bullet.group = BulletGroup::stack_aimed;
		bullet.count = boss.statebytes[6U];
		bullet.delta = 12U;
		bullet.special_motion = BulletSpecialMotion::none;
		bullet.angle = 0U;
		bullet.patnum = stage4_pat_bullet16_ball_blue;
	}
	if(charge == 1U) {
		if((boss.phase_frame % 32) == 16) {
			bullet.angle = 32U;
			for(std::uint8_t i{}; i < 5U; ++i) {
				if(!bullets_add(state, true, true)) return false;
				bullet.angle = byte_add(bullet.angle, -16);
			}
			presentation.sound(15U);
		}
		if(boss.phase_frame >= 128) {
			boss.sprite = stage4_pat_reimu_still;
			boss.phase_frame = 0;
			boss.mode = stage4_mode_interval;
		}
	}
	return true;
}

void reimu_fly_pattern(State &state, bool mirrored) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 1) {
		boss.reimu_afterimage = true;
		const auto leg = static_cast<std::uint8_t>(boss.phase_state % 3U);
		if(!mirrored) {
			boss.pos.velocity = leg == 0U ? Point{-64, 16} :
				leg == 1U ? Point{64, 0} : Point{-64, -16};
		} else {
			boss.pos.velocity = leg == 0U ? Point{64, -16} :
				leg == 1U ? Point{-64, 0} : Point{64, 16};
		}
	}
	pc98::th0405::update_motion(boss.pos);
	const auto frames = (boss.phase_state % 3U) == 1U ? 64 : 32;
	if(boss.phase_frame == frames) {
		++boss.phase_state;
		boss.mode = static_cast<std::uint8_t>(boss.phase_state % 2U);
		boss.phase_frame = 0;
		boss.reimu_afterimage = false;
	}
}

[[nodiscard]] bool reimu_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	bullet.origin = {word_add(boss.pos.cur.x, 64), word_sub(boss.pos.cur.y, 448)};
	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 0) boss.reimu_afterimage = false;
		++boss.phase_frame;
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(boss.phase_frame > 96) {
			++boss.phase;
			state.palette[0] = {128U, 0U, 224U};
			state.palette_changed = true;
			boss.phase_frame = 0;
			presentation.sound(13U);
		}
		break;

	case 1U:
		reimu_pulse(state);
		// The source body and the invincible boss hittest both advance this.
		boss.phase_frame = static_cast<std::int16_t>(boss.phase_frame + 2);
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(boss.phase_frame >= 128) {
			boss.pos.velocity.x = 0;
			boss.phase_end_hp = stage4_reimu_hp;
			if(!boss_phase_next(state, presentation, 0U, 7900)) return false;
		}
		break;

	case 2U: {
		bool pattern_ok = true;
		switch(boss.mode) {
		case 0U: pattern_ok = reimu_pattern_aimed_sweep(state, presentation); break;
		case 1U: pattern_ok = reimu_pattern_turning_spread(state, presentation); break;
		case stage4_mode_interval: reimu_fly_pattern(state, false); break;
		default: return false;
		}
		if(!pattern_ok) return false;
		reimu_pulse(state);
		if(boss.phase_state < 9U) {
			const auto hit = boss_hittest(state, presentation);
			if(hit == HitResult::unsupported) return false;
			if(hit == HitResult::none) break;
			if(!boss_score_bonus(state, presentation, 10U)) return false;
		}
		if(!boss_phase_next(state, presentation, 1U, 6300)) return false;
		boss.sprite = pat_stage + 1U;
		boss.reimu_afterimage = false;
		break;
	}

	case 3U:
		boss.pos.velocity.x = boss.pos.cur.x < stage4_boss_center_x ? 32 :
			boss.pos.cur.x > stage4_boss_center_x ? -32 : 0;
		boss.pos.velocity.y = boss.pos.cur.y < 1536 ? 16 :
			boss.pos.cur.y > 1536 ? -16 : 0;
		pc98::th0405::update_motion(boss.pos);
		reimu_pulse(state);
		if(boss_hittest(state, presentation) == HitResult::unsupported) return false;
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.phase_frame = 0;
			boss.phase_state = 0U;
			boss.mode = 0U;
			boss.sprite = pat_stage + 1U;
			boss.reimu_orb_template.angle_speed = 4U;
			boss.reimu_orb_patnum_base = stage4_pat_reimu_orb_blue;
			boss.statebytes[10U] = 0U;
		}
		break;

	case 4U: {
		bool pattern_ok = true;
		switch(boss.mode) {
		case 0U:
		case 1U:
		case 2U: pattern_ok = reimu_pattern_orb_ring(state, presentation); break;
		case 3U: pattern_ok = reimu_pattern_random_rings(state, presentation); break;
		case stage4_mode_interval:
			++boss.phase_state;
			if(boss.statebytes[10U] <= 2U) {
				const auto up = pc98::th0405::randring_next16_and(state.random, 1U);
				if(!up) return false;
				boss.statebytes[10U] = *up != 0U ?
					static_cast<std::uint8_t>(boss.statebytes[10U] + 1U) : 3U;
			} else {
				boss.statebytes[10U] = 0U;
			}
			boss.mode = boss.statebytes[10U];
			boss.phase_frame = 0;
			break;
		default: return false;
		}
		if(!pattern_ok) return false;
		reimu_pulse(state);
		if(boss.phase_state < 18U) {
			const auto hit = boss_hittest(state, presentation);
			if(hit == HitResult::unsupported) return false;
			if(hit == HitResult::none) break;
			if(!boss_score_bonus(state, presentation, 10U)) return false;
		}
		if(!boss_phase_next(state, presentation, 2U, 4500)) return false;
		boss.pos.velocity.x = 0;
		break;
	}

	case 5U:
		boss.pos.velocity.y = boss.pos.cur.y < 2048 ? 16 :
			boss.pos.cur.y > 2048 ? -16 : 0;
		pc98::th0405::update_motion(boss.pos);
		reimu_pulse(state);
		if(boss_hittest(state, presentation) == HitResult::unsupported) return false;
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.phase_frame = 0;
			boss.phase_state = 0U;
			boss.mode = 0U;
			boss.sprite = pat_stage + 1U;
			boss.reimu_orb_template.angle_speed = 4U;
		}
		break;

	case 6U: {
		bool pattern_ok = true;
		switch(boss.mode) {
		case 0U: pattern_ok = reimu_pattern_jittered_pellets(state, presentation); break;
		case 1U: pattern_ok = reimu_pattern_aimed_random(state, presentation); break;
		case stage4_mode_interval: reimu_fly_pattern(state, true); break;
		default: return false;
		}
		if(!pattern_ok) return false;
		reimu_pulse(state);
		if(boss.phase_state < 11U) {
			const auto hit = boss_hittest(state, presentation);
			if(hit == HitResult::unsupported) return false;
			if(hit == HitResult::none) break;
			if(!boss_score_bonus(state, presentation, 10U)) return false;
		}
		if(!boss_phase_next(state, presentation, 3U, 2700)) return false;
		boss.sprite = pat_stage + 1U;
		boss.reimu_afterimage = false;
		break;
	}

	case 7U:
		boss.pos.velocity.x = boss.pos.cur.x < stage4_boss_center_x ? 32 :
			boss.pos.cur.x > stage4_boss_center_x ? -32 : 0;
		boss.pos.velocity.y = boss.pos.cur.y < stage4_boss_center_y ? 16 :
			boss.pos.cur.y > stage4_boss_center_y ? -16 : 0;
		pc98::th0405::update_motion(boss.pos);
		reimu_pulse(state);
		if(boss_hittest(state, presentation) == HitResult::unsupported) return false;
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.phase_frame = 0;
			boss.phase_state = 0U;
			boss.mode = 0U;
			boss.sprite = pat_stage + 1U;
			boss.reimu_orb_template.angle_speed = 18U;
			boss.reimu_orb_patnum_base = stage4_pat_reimu_orb_yellow;
		}
		break;

	case 8U: {
		bool pattern_ok = true;
		switch(boss.mode) {
		case 0U: pattern_ok = reimu_pattern_orb_spiral(state, presentation); break;
		case 1U: pattern_ok = reimu_pattern_mirrored_spreads(state, presentation); break;
		case stage4_mode_interval:
			++boss.phase_state;
			boss.mode = static_cast<std::uint8_t>(boss.phase_state & 1U);
			boss.phase_frame = 0;
			break;
		default: return false;
		}
		if(!pattern_ok) return false;
		reimu_pulse(state);
		if(boss.phase_state < 10U) {
			const auto hit = boss_hittest(state, presentation);
			if(hit == HitResult::unsupported) return false;
			if(hit == HitResult::none) break;
			if(!boss_score_bonus(state, presentation, 10U)) return false;
		}
		if(!boss_phase_next(state, presentation, 4U, 900)) return false;
		boss.pos.velocity.x = 0;
		boss.reimu_orb_template.angle_speed = 3U;
		break;
	}

	case 9U: {
		bool pattern_ok = true;
		switch(boss.mode) {
		case 0U: pattern_ok = reimu_pattern_jittered_pellets(state, presentation); break;
		case 1U: pattern_ok = reimu_pattern_aimed_stacks(state, presentation); break;
		case stage4_mode_interval: reimu_fly_pattern(state, false); break;
		default: return false;
		}
		if(!pattern_ok) return false;
		reimu_pulse(state);
		if(boss.phase_state < 12U) {
			const auto hit = boss_hittest(state, presentation);
			if(hit == HitResult::unsupported) return false;
			if(hit == HitResult::none) break;
			if(!boss_score_bonus(state, presentation, 10U)) return false;
		}
		if(!boss_phase_next(state, presentation, 5U, 0)) return false;
		boss.pos.velocity.x = 0;
		boss.reimu_orb_template.angle_speed = 3U;
		state.palette[0].r = 60U;
		break;
	}

	case 10U:
		boss.pos.velocity.x = boss.pos.cur.x < stage4_boss_center_x ? 32 :
			boss.pos.cur.x > stage4_boss_center_x ? -32 : 0;
		boss.pos.velocity.y = boss.pos.cur.y < stage4_boss_center_y ? 16 :
			boss.pos.cur.y > stage4_boss_center_y ? -16 : 0;
		pc98::th0405::update_motion(boss.pos);
		state.palette[0].r = byte_add(state.palette[0].r, 3);
		state.palette[0].b = byte_add(state.palette[0].b, -2);
		state.palette_changed = true;
		if(boss_hittest(state, presentation) == HitResult::unsupported) return false;
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.phase_frame = 0;
			boss.phase_state = 0U;
			boss.mode = 0U;
			boss.sprite = pat_stage + 1U;
			boss.reimu_orb_template.angle_speed = 4U;
		}
		break;

	case 11U: {
		bullet.origin = boss.pos.cur;
		if(!reimu_pattern_scattered_stacks(state, presentation)) return false;
		const auto hit = boss_hittest(state, presentation);
		if(hit == HitResult::unsupported) return false;
		if(hit == HitResult::none && boss.phase_frame < 1000) break;
		boss_explode_small(state, presentation, 4U);
		++boss.phase;
		boss.phase_state = boss.phase_frame < 1000 ? 1U : 0U;
		boss.phase_frame = 0;
		break;
	}

	case 12U:
		++boss.phase_frame;
		if(boss.phase_frame == 16) boss_explode_small(state, presentation, 5U);
		if(boss.phase_frame == 32) {
			boss_explode_big(state, presentation, 3U);
			boss.phase = stage4_phase_explode_big;
			state.bullets.zap.set_active(boss.phase_state != 0U);
			if(boss.phase_state != 0U && !boss_score_bonus(state, presentation, 40U)) {
				return false;
			}
			boss.sprite = pat_enemy_kill;
			boss.phase_frame = 0;
			presentation.sound(12U);
			state.palette[0].r = 0U;
			state.palette[0].b = 0U;
			state.palette_changed = true;
			state.player_invincibility_time = boss_defeat_invincibility_frames;
		}
		break;

	case stage4_phase_explode_big:
		if(boss.phase_frame < 12) {
			state.playfield_shake_x = (state.stage_frame & 1U) == 0U ? -4 : 4;
			state.playfield_shake_y = (state.stage_frame & 3U) <= 1U ? -4 : 4;
		}
		state.bullets.slowdown_factor = 2U;
		++boss.phase_frame;
		if((boss.phase_frame % 8) == 0) {
			++boss.sprite;
			if(boss.sprite >= static_cast<std::uint8_t>(pat_enemy_kill_last + 1U)) {
				boss.phase = stage4_phase_none;
				boss.phase_frame = 0;
				state.bombing_disabled = true;
			}
		}
		return true;

	case stage4_phase_none:
			state.stage_clear_requested = true;
			return true;

	default:
		return false;
	}
	state.reimu_a.homing_target = boss.pos.cur;
	if(!reimu_orbs_update(state)) return false;
	return true;
}

[[nodiscard]] std::uint8_t marisa_charge_animate(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	if(boss.mode >= 1U && boss.mode <= 6U) {
		switch(boss.phase_frame) {
		case 32:
			state.bullets.gather_template.center = {
				word_sub(boss.pos.cur.x, 320), word_sub(boss.pos.cur.y, 128)
			};
			state.bullets.gather_template.ring_points = 16;
			state.bullets.gather_template.angle_delta = static_cast<std::uint8_t>(-2);
			state.bullets.gather_template.col = 3U;
			state.bullets.gather_template.radius = 4096;
			gather_add_only(state);
			break;
		case 34:
			state.bullets.gather_template.col = 2U;
			gather_add_only(state);
			break;
		case 36: gather_add_only(state); break;
		default: break;
		}
	}
	if(boss.phase_frame == 16) {
		boss.sprite = pat_stage + 2U;
		presentation.sound(8U);
	} else if(boss.phase_frame == 30) {
		circle_add_shrinking(state, {word_sub(boss.pos.cur.x, 320),
			word_sub(boss.pos.cur.y, 128)});
	} else if(boss.phase_frame >= 44 && boss.phase_frame < 64) {
		boss.sprite = static_cast<std::uint8_t>(pat_stage +
			static_cast<std::uint8_t>((boss.phase_frame - 32) / 4));
	} else if(boss.phase_frame == 64) {
		boss.sprite = pat_stage + 2U;
		presentation.sound(15U);
		return 2U;
	}
	return boss.phase_frame < 64 ? 0U : 1U;
}

[[nodiscard]] bool marisa_flystep_random(State &state) noexcept
{
	auto &boss = state.boss;
	if((boss.phase_frame & 31) == 1) {
		if(boss.pos.cur.x <= 1792) {
			boss.pos.velocity.x = 32;
		} else if(boss.pos.cur.x >= 4352) {
			boss.pos.velocity.x = -32;
		} else {
			const auto direction = pc98::th0405::randring_next16_and(state.random, 1U);
			if(!direction) return false;
			boss.pos.velocity.x = *direction != 0U ? 16 : -16;
		}
		if(boss.pos.cur.y <= 1280) {
			boss.pos.velocity.y = 16;
		} else if(boss.pos.cur.y >= 2304) {
			boss.pos.velocity.y = -16;
		} else {
			const auto direction = pc98::th0405::randring_next16_and(state.random, 3U);
			if(!direction) return false;
			switch(*direction) {
			case 0U: boss.pos.velocity.y = 16; break;
			case 1U: boss.pos.velocity.y = -16; break;
			case 2U: boss.pos.velocity.y = 24; break;
			default: boss.pos.velocity.y = -24; break;
			}
		}
	}
	pc98::th0405::update_motion(boss.pos);
	return true;
}

[[nodiscard]] bool marisa_flystep_pointreflected(State &state, std::int16_t duration) noexcept
{
	auto &boss = state.boss;
	if(boss.marisa_flystep_pointreflected_tick == 0U) {
		const auto divisor = static_cast<std::int16_t>((duration / 2) - 6);
		if(divisor == 0) return false;
		boss.pos.velocity.x = static_cast<std::int16_t>(
			word_sub(3072, boss.pos.cur.x) / divisor
		);
		boss.pos.velocity.y = static_cast<std::int16_t>(
			word_sub(1792, boss.pos.cur.y) / divisor
		);
	}
	++boss.marisa_flystep_pointreflected_tick;
	if(boss.marisa_flystep_pointreflected_tick >= static_cast<std::uint8_t>(duration - 12)) {
		boss.pos.velocity.x = static_cast<std::int16_t>(boss.pos.velocity.x / 2);
		boss.pos.velocity.y = static_cast<std::int16_t>(boss.pos.velocity.y / 2);
	}
	if(boss.marisa_flystep_pointreflected_tick >= static_cast<std::uint8_t>(duration)) return true;
	pc98::th0405::update_motion(boss.pos);
	return false;
}

[[nodiscard]] bool marisa_bits_respawn(State &state) noexcept
{
	static constexpr std::array<std::int16_t, marisa_bit_count> hit_points{
		220, 400, 280, 450,
	};
	auto &boss = state.boss;
	const auto random_angle = pc98::th0405::randring_next16(state.random);
	if(!random_angle) return false;
	auto angle = static_cast<std::uint8_t>(*random_angle);
	for(std::size_t i{}; i < boss.marisa_bits.size(); ++i) {
		auto &bit = boss.marisa_bits[i];
		bit.flag = MarisaBitFlag::moveout_spin;
		bit.angle_speed = boss.marisa_bit_angle_speed;
		bit.angle = angle;
		angle = byte_add(angle, 64);
		bit.patnum = static_cast<std::uint8_t>(stage4_pat_marisa_bit + i);
		bit.distance = 0;
		bit.hp = hit_points[i];
		bit.moveout_speed = 32;
	}
	return true;
}

[[nodiscard]] bool marisa_bits_update_and_hittest(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	boss.marisa_bits_alive = 0U;
	for(auto &bit : boss.marisa_bits) {
		if(bit.flag == MarisaBitFlag::free) continue;
		const auto raw_flag = static_cast<std::uint8_t>(bit.flag);
		if(raw_flag >= static_cast<std::uint8_t>(MarisaBitFlag::kill_anim)) {
			const auto next = static_cast<std::uint8_t>(raw_flag + 1U);
			bit.flag = static_cast<MarisaBitFlag>(next);
			bit.patnum = static_cast<std::uint8_t>(((next -
				static_cast<std::uint8_t>(MarisaBitFlag::kill_anim)) / 4U) + pat_enemy_kill);
			if(bit.patnum >= static_cast<std::uint8_t>(pat_enemy_kill_last + 1U)) {
				bit.flag = MarisaBitFlag::free;
			}
			continue;
		}
		bit.angle = byte_add(bit.angle, static_cast<std::int8_t>(bit.angle_speed));
		bit.distance = word_add(bit.distance, bit.moveout_speed);
		bit.center.x = word_add(boss.pos.cur.x, pc98::masterlib::vector_component(
			bit.distance, pc98::masterlib::cosine8(bit.angle)
		));
		bit.center.y = word_add(boss.pos.cur.y, pc98::masterlib::vector_component(
			bit.distance, pc98::masterlib::sine8(bit.angle)
		));
		if(bit.flag == MarisaBitFlag::moveout_spin && bit.distance >= 1024) {
			bit.flag = MarisaBitFlag::spin;
			bit.moveout_speed = 0;
		} else if(bit.flag == MarisaBitFlag::spin && bit.distance >= 64) {
			bit.flag = static_cast<MarisaBitFlag>(3U);
		}
		std::uint16_t damage{};
		if(!stage4_shots_hit(state, bit.center, {192, 192}, damage)) return false;
		bit.damage_this_frame = static_cast<std::uint8_t>(damage);
		bit.hp = static_cast<std::int16_t>(static_cast<int>(bit.hp) -
			static_cast<int>(bit.damage_this_frame));
		if(bit.hp <= 0) {
			bit.patnum = pat_enemy_kill;
			bit.flag = MarisaBitFlag::kill_anim;
			presentation.sound(3U);
			state.score.score_delta += 5120U;
			if(!pc98::th0405::sparks_add_random(
				pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
				state.random, bit.center, 64, 8U
			)) return false;
			continue;
		}
		if(stage4_player_in_box(state, bit.center, 192)) state.player_is_hit = true;
		if(bit.center.y < state.reimu_a.homing_target.y) {
			state.reimu_a.homing_target = bit.center;
		}
		++boss.marisa_bits_alive;
	}
	return true;
}

[[nodiscard]] bool marisa_bits_fire(State &state) noexcept
{
	for(auto &bit : state.boss.marisa_bits) {
		const auto flag = static_cast<std::uint8_t>(bit.flag);
		if(bit.flag == MarisaBitFlag::free ||
			flag >= static_cast<std::uint8_t>(MarisaBitFlag::kill_anim)) continue;
		auto &bullet = state.bullets.template_;
		switch(state.boss.marisa_bit_fire) {
		case MarisaBitFire::sideways_spread:
			if(state.boss.marisa_bits_alive <= 2U) bullet.count = 5U;
			bullet.angle = byte_add(bit.angle,
				static_cast<std::int8_t>(bit.angle_speed) >= 0 ? -64 : 64);
			bullet.origin = bit.center;
			if(!bullets_add(state)) return false;
			break;
		case MarisaBitFire::single:
			bullet.origin = bit.center;
			if(!bullets_add(state)) return false;
			break;
		default: return false;
		}
	}
	return true;
}

[[nodiscard]] bool marisa_pattern_bitless_clouds(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = marisa_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
		bullet.patnum = stage4_pat_bullet16_ball_blue;
		bullet.group = BulletGroup::spread;
		bullet.angle = 128U;
		bullet.delta = 8U;
		if(!bullet_template_tune(state)) return false;
	} else if(charge == 1U) {
		if((boss.phase_frame & 1) == 0) {
			const auto count1 = pc98::th0405::randring_next16_and(state.random, 3U);
			if(!count1) return false;
			bullet.count = byte_add(static_cast<std::uint8_t>(*count1), 1);
			const auto speed1 = pc98::th0405::randring_next16_and(state.random, 31U);
			if(!speed1) return false;
			bullet.speed = byte_add(static_cast<std::uint8_t>(*speed1), 32);
			bullet.angle = byte_add(bullet.angle, -8);
			bullet.origin.x = word_sub(bullet.origin.x, 96);
			if(!bullets_add(state)) return false;
			const auto count2 = pc98::th0405::randring_next16_and(state.random, 3U);
			if(!count2) return false;
			bullet.count = byte_add(static_cast<std::uint8_t>(*count2), 1);
			const auto speed2 = pc98::th0405::randring_next16_and(state.random, 31U);
			if(!speed2) return false;
			bullet.speed = byte_add(static_cast<std::uint8_t>(*speed2), 32);
			bullet.origin.x = word_add(bullet.origin.x, 192);
			if(!bullets_add(state)) return false;
		}
		if(bullet.angle == 0U) {
			boss.phase_frame = 0;
			boss.mode = stage4_mode_interval;
			boss.sprite = stage4_pat_marisa_fight;
		}
	}
	return true;
}

[[nodiscard]] bool marisa_pattern_respawn_bits(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	(void)marisa_charge_animate(state, presentation);
	switch(boss.phase_frame) {
	case 32:
		state.bullets.gather_template.center = boss.pos.cur;
		state.bullets.gather_template.ring_points = 32;
		state.bullets.gather_template.angle_delta = static_cast<std::uint8_t>(-2);
		state.bullets.gather_template.col = 9U;
		state.bullets.gather_template.radius = 4096;
		gather_add_only(state);
		break;
	case 34:
		state.bullets.gather_template.col = 8U;
		gather_add_only(state);
		break;
	case 36: gather_add_only(state); break;
	case 64:
		if(!marisa_bits_respawn(state)) return false;
		boss.marisa_bit_angle_speed = static_cast<std::uint8_t>(-
			static_cast<std::int8_t>(boss.marisa_bit_angle_speed));
		break;
	case 96:
		boss.phase_frame = 0;
		boss.mode = stage4_mode_interval;
		boss.sprite = stage4_pat_marisa_fight;
		break;
	default: break;
	}
	return true;
}

[[nodiscard]] bool marisa_pattern_sideways_bits(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = marisa_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.speed = 56U;
		bullet.group = BulletGroup::spread;
		bullet.count = 3U;
		bullet.delta = 8U;
		if(!bullet_template_tune(state)) return false;
		boss.marisa_bit_fire = MarisaBitFire::sideways_spread;
		boss.statebytes[15U] = static_cast<std::uint8_t>(boss.phase_frame);
	} else if(charge == 1U) {
		if((boss.phase_frame & 3) == 0) {
			if(boss.marisa_bits_alive != 0U) {
				if(!marisa_bits_fire(state)) return false;
				boss.statebytes[15U] = static_cast<std::uint8_t>(boss.phase_frame);
			} else {
				(void)marisa_flystep_pointreflected(state, static_cast<std::int16_t>(
					160 - boss.statebytes[15U]
				));
				bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
				bullet.patnum = stage4_pat_bullet16_directional_blue;
				bullet.speed = 52U;
				bullet.group = BulletGroup::spread;
				bullet.count = 3U;
				bullet.delta = 6U;
				bullet.angle = byte_add(bullet.angle, 6);
				if(!bullet_template_tune(state)) return false;
				for(std::uint8_t i{}; i < 4U; ++i) {
					if(!bullets_add(state)) return false;
					bullet.angle = byte_add(bullet.angle, 64);
				}
			}
			presentation.sound(9U);
		}
		if(boss.phase_frame >= 160) {
			for(auto &bit : boss.marisa_bits) {
				bit.angle_speed = static_cast<std::uint8_t>(-
					static_cast<std::int8_t>(bit.angle_speed));
			}
			boss.phase_frame = 0;
			boss.mode = stage4_mode_interval;
			boss.sprite = stage4_pat_marisa_fight;
		}
	}
	return true;
}

[[nodiscard]] bool marisa_pattern_aimed_bits(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = marisa_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.speed = 16U;
		bullet.group = BulletGroup::single;
		if(!bullet_template_tune(state)) return false;
		boss.marisa_bit_fire = MarisaBitFire::single;
		for(auto &bit : boss.marisa_bits) {
			bit.angle_speed = byte_add(bit.angle_speed, static_cast<std::int8_t>(bit.angle_speed));
		}
		boss.statebytes[15U] = static_cast<std::uint8_t>(boss.phase_frame);
	} else if(charge == 1U) {
		if((boss.phase_frame & 3) == 0) {
			const auto aimed = pc98::masterlib::iatan2_source(
				word_sub(state.player_pos.cur.y, boss.pos.cur.y),
				word_sub(state.player_pos.cur.x, boss.pos.cur.x)
			);
			if(!aimed) return false;
			bullet.angle = *aimed;
			if(boss.marisa_bits_alive != 0U) {
				if(!marisa_bits_fire(state)) return false;
				boss.statebytes[15U] = static_cast<std::uint8_t>(boss.phase_frame);
			} else {
				(void)marisa_flystep_pointreflected(state, static_cast<std::int16_t>(
					160 - boss.statebytes[15U]
				));
				bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
				bullet.patnum = stage4_pat_bullet16_star;
				bullet.group = BulletGroup::spread_aimed;
				bullet.count = 3U;
				bullet.delta = 12U;
				bullet.angle = 0U;
				if(!bullet_template_tune(state) || !bullets_add(state)) return false;
			}
			bullet.speed = byte_add(bullet.speed, 4);
			presentation.sound(9U);
		}
		if(boss.phase_frame >= 160) {
			for(auto &bit : boss.marisa_bits) {
				bit.angle_speed = static_cast<std::uint8_t>(
					static_cast<std::int8_t>(bit.angle_speed) / 2
				);
			}
			boss.phase_frame = 0;
			boss.mode = stage4_mode_interval;
			boss.sprite = stage4_pat_marisa_fight;
		}
	}
	return true;
}

void marisa_pattern_end(BossState &boss) noexcept
{
	boss.phase_frame = 0;
	boss.mode = stage4_mode_interval;
	boss.sprite = stage4_pat_marisa_fight;
}

[[nodiscard]] bool marisa_pattern_spiral_bits(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = marisa_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		boss.marisa_bit_fire = MarisaBitFire::sideways_spread;
		boss.statebytes[15U] = 0U;
	} else if(charge == 1U) {
		if(boss.marisa_bits_alive != 0U) {
			if(boss.phase_frame <= 192) {
				for(auto &bit : boss.marisa_bits) bit.distance = word_add(bit.distance, 24);
			} else if(boss.phase_frame <= 256) {
				if((boss.phase_frame & 3) == 0) {
					if((boss.phase_frame % 32) == 0) {
						bullet.speed = 32U;
						bullet.group = BulletGroup::ring_aimed;
						bullet.count = 16U;
						if(!bullet_template_tune(state) || !bullets_add(state)) return false;
					}
					bullet.group = BulletGroup::spread;
					bullet.count = 3U;
					bullet.delta = 6U;
					if(!bullet_template_tune(state)) return false;
					bullet.speed = 64U;
					presentation.sound(9U);
					if(!marisa_bits_fire(state)) return false;
				}
			} else if(boss.phase_frame <= 384) {
				for(auto &bit : boss.marisa_bits) bit.distance = word_add(bit.distance, -24);
			} else {
				for(std::size_t i{}; i < boss.marisa_bits.size(); i += 2U) {
					auto &bit = boss.marisa_bits[i];
					bit.angle_speed = static_cast<std::uint8_t>(-
						static_cast<std::int8_t>(bit.angle_speed));
				}
				marisa_pattern_end(boss);
			}
		} else {
			(void)marisa_flystep_pointreflected(state, 96);
			if(boss.statebytes[15U] == 0U) {
				boss.statebytes[15U] = 1U;
				bullet.spawn_type = static_cast<std::uint8_t>(
					BulletSpawnType::bullet16_cloud_forwards
				);
				bullet.patnum = stage4_pat_bullet16_ball_blue;
				bullet.group = BulletGroup::spread;
				bullet.angle = 128U;
				bullet.delta = 8U;
				presentation.sound(15U);
			}
			if((boss.phase_frame & 1) == 0) {
				const auto count1 = pc98::th0405::randring_next16_and(state.random, 3U);
				if(!count1) return false;
				bullet.count = byte_add(static_cast<std::uint8_t>(*count1), 1);
				const auto speed1 = pc98::th0405::randring_next16_and(state.random, 31U);
				if(!speed1) return false;
				bullet.speed = byte_add(static_cast<std::uint8_t>(*speed1), 16);
				bullet.angle = (boss.statebytes[15U] & 1U) != 0U ?
					byte_add(bullet.angle, -8) : byte_add(bullet.angle, 8);
				bullet.origin.x = word_sub(bullet.origin.x, 96);
				if(!bullets_add(state)) return false;
				const auto count2 = pc98::th0405::randring_next16_and(state.random, 3U);
				if(!count2) return false;
				bullet.count = byte_add(static_cast<std::uint8_t>(*count2), 1);
				const auto speed2 = pc98::th0405::randring_next16_and(state.random, 31U);
				if(!speed2) return false;
				bullet.speed = byte_add(static_cast<std::uint8_t>(*speed2), 16);
				bullet.origin.x = word_add(bullet.origin.x, 192);
				if(!bullets_add(state)) return false;
				if(bullet.angle == 0U || bullet.angle >= 128U) {
					++boss.statebytes[15U];
					if(boss.statebytes[15U] >= 4U) {
						marisa_pattern_end(boss);
					} else {
						presentation.sound(15U);
					}
				}
			}
		}
	}
	return true;
}

[[nodiscard]] bool marisa_pattern_aimed_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = marisa_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_backwards);
		bullet.patnum = stage4_pat_bullet16_ball_blue;
		bullet.speed = 50U;
		bullet.group = BulletGroup::ring_aimed;
		bullet.angle = 0U;
		if(!bullet_template_tune(state)) return false;
		boss.marisa_bit_fire = MarisaBitFire::single;
	} else if(charge == 1U) {
		if(boss.marisa_bits_alive != 0U) {
			if((boss.phase_frame % 32) == 0) {
				bullet.count = static_cast<std::uint8_t>(24U - boss.marisa_bits_alive * 2U);
				if(boss.marisa_explode_milestone == 2U) bullet.count = 28U;
				if(!marisa_bits_fire(state)) return false;
				presentation.sound(9U);
			}
			if(boss.phase_frame >= 160) {
				for(std::size_t i{}; i < boss.marisa_bits.size(); i += 2U) {
					auto &bit = boss.marisa_bits[i];
					bit.angle_speed = static_cast<std::uint8_t>(-
						static_cast<std::int8_t>(bit.angle_speed));
				}
				marisa_pattern_end(boss);
			}
		} else {
			const auto done = marisa_flystep_pointreflected(state, 64);
			if(done) {
				marisa_pattern_end(boss);
				return true;
			}
			if((boss.phase_frame % 16) == 0) {
				bullet.spawn_type = static_cast<std::uint8_t>(
					BulletSpawnType::bullet16_cloud_forwards
				);
				bullet.patnum = stage4_pat_bullet16_ball_red;
				bullet.group = BulletGroup::single_aimed;
				bullet.special_motion = BulletSpecialMotion::none;
				if(!bullet_template_tune(state)) return false;
				for(std::uint8_t i{}; i < 32U; ++i) {
					const auto x = pc98::th0405::randring_next16_mod(state.random, 1024U);
					if(!x) return false;
					const auto y = pc98::th0405::randring_next16_mod(state.random, 1024U);
					if(!y) return false;
					bullet.origin = {word_add(word_sub(boss.pos.cur.x, 832),
						std::bit_cast<std::int16_t>(*x)), word_add(word_sub(boss.pos.cur.y, 640),
						std::bit_cast<std::int16_t>(*y))};
					const auto raw_speed = pc98::th0405::randring_next16_mod(state.random, 96U);
					if(!raw_speed) return false;
					std::int16_t speed = word_add(std::bit_cast<std::int16_t>(*raw_speed), 16);
					bullet.speed = static_cast<std::uint8_t>(speed);
					if(speed >= 64) {
						bullet.angle = 0U;
					} else {
						speed = word_sub(64, speed);
						const auto angle = pc98::th0405::randring_next16_mod(state.random,
							static_cast<std::uint16_t>(speed));
						if(!angle) return false;
						bullet.angle = byte_add(static_cast<std::uint8_t>(*angle),
							-static_cast<std::int16_t>(speed / 2));
					}
					if(!bullets_add(state, true, false)) return false;
				}
				presentation.sound(15U);
			}
		}
	}
	return true;
}

[[nodiscard]] bool marisa_pattern_turning_stars(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = marisa_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = stage4_pat_bullet16_star;
		bullet.speed = 92U;
		bullet.group = BulletGroup::single;
		bullet.special_motion = BulletSpecialMotion::decelerate_to_angle;
		if(!bullet_template_tune(state)) return false;
		boss.marisa_bit_fire = MarisaBitFire::single;
		boss.statebytes[15U] = 0U;
	} else if(charge == 1U) {
		if(boss.marisa_bits_alive != 0U) {
			if((boss.phase_frame & 3) == 0 && boss.phase_frame <= 224) {
				auto fire_quad = [&](std::uint8_t first_angle, std::uint8_t target) noexcept {
					state.bullets.template_special_angle = target;
					bullet.angle = first_angle;
					for(std::uint8_t i{}; i < 4U; ++i) {
						if(!marisa_bits_fire(state)) return false;
						bullet.angle = byte_add(bullet.angle, 32);
					}
					return true;
				};
				bool fired{};
				if(boss.phase_frame <= 96) {
					fired = fire_quad(16U, 64U);
				} else if(boss.phase_frame <= 128) {
					state.bullets.template_special_angle = 48U;
					bullet.angle = 144U;
					if(!marisa_bits_fire(state)) return false;
					state.bullets.template_special_angle = 80U;
					bullet.angle = 176U;
					if(!marisa_bits_fire(state)) return false;
					state.bullets.template_special_angle = 48U;
					bullet.angle = 208U;
					if(!marisa_bits_fire(state)) return false;
					state.bullets.template_special_angle = 80U;
					bullet.angle = 240U;
					if(!marisa_bits_fire(state)) return false;
					fired = true;
				} else if(boss.phase_frame <= 160) {
					fired = fire_quad(16U, 112U);
				} else if(boss.phase_frame <= 192) {
					fired = fire_quad(16U, 16U);
				} else {
					fired = fire_quad(16U, 64U);
				}
				if(!fired) return false;
				presentation.sound(3U);
			}
		} else {
			(void)marisa_flystep_pointreflected(state, 128);
			if(boss.statebytes[15U] == 0U) {
				const auto random = pc98::th0405::randring_next16_and(state.random, 31U);
				if(!random) return false;
				bullet.angle = byte_add(128U, -static_cast<std::int16_t>(*random));
				boss.statebytes[15U] = 1U;
			}
			if((boss.phase_frame & 7) == 0) {
				bullet.spawn_type = static_cast<std::uint8_t>(
					BulletSpawnType::bullet16_cloud_forwards
				);
				bullet.patnum = stage4_pat_bullet16_ball_blue;
				bullet.group = BulletGroup::stack;
				bullet.count = 16U;
				bullet.delta = 5U;
				bullet.speed = 16U;
				if(!bullet_template_tune(state) || !bullets_add(state, false, true)) return false;
				bullet.angle = byte_add(bullet.angle, -8);
				presentation.sound(15U);
			}
			if(bullet.angle > 128U && bullet.angle <= 224U) marisa_pattern_end(boss);
		}
		if(boss.phase_frame >= 256) marisa_pattern_end(boss);
	}
	return true;
}

[[nodiscard]] bool marisa_pattern_curtain(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = marisa_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = stage4_pat_bullet16_star;
		bullet.group = BulletGroup::single;
		bullet.special_motion = BulletSpecialMotion::none;
		bullet.angle = static_cast<std::uint8_t>(-64);
		bullet.speed = 96U;
		boss.marisa_bit_fire = MarisaBitFire::single;
		boss.statebytes[15U] = 0U;
		return true;
	}
	if(charge != 1U) return true;
	if(boss.marisa_bits_alive != 0U) {
		if(boss.phase_frame <= 128) {
			if((state.stage_frame & 3U) == 0U) {
				if(!marisa_bits_fire(state)) return false;
				presentation.sound(9U);
			}
			return true;
		}
		if(boss.phase_frame <= 192) {
			if((state.stage_frame & 3U) != 0U) return true;
			const auto y1 = pc98::th0405::randring_next16_mod(state.random, 3072U);
			if(!y1) return false;
			const auto speed1 = pc98::th0405::randring_next16_and(state.random, 31U);
			if(!speed1) return false;
			const auto angle1 = pc98::th0405::randring_next16_and(state.random, 31U);
			if(!angle1) return false;
			bullet.origin = {0, std::bit_cast<std::int16_t>(*y1)};
			bullet.speed = byte_add(static_cast<std::uint8_t>(*speed1), 16);
			bullet.angle = byte_add(48U, -static_cast<std::int16_t>(*angle1));
			if(!bullets_add(state, true, false)) return false;
			const auto y2 = pc98::th0405::randring_next16_mod(state.random, 3072U);
			if(!y2) return false;
			const auto speed2 = pc98::th0405::randring_next16_and(state.random, 31U);
			if(!speed2) return false;
			const auto angle2 = pc98::th0405::randring_next16_and(state.random, 31U);
			if(!angle2) return false;
			bullet.origin = {6144, std::bit_cast<std::int16_t>(*y2)};
			bullet.speed = byte_add(static_cast<std::uint8_t>(*speed2), 16);
			bullet.angle = byte_add(static_cast<std::uint8_t>(*angle2), 80);
			if(!bullets_add(state, true, false)) return false;
			const auto x3 = pc98::th0405::randring_next16_mod(state.random, 6144U);
			if(!x3) return false;
			const auto speed3 = pc98::th0405::randring_next16_and(state.random, 31U);
			if(!speed3) return false;
			const auto angle3 = pc98::th0405::randring_next16_and(state.random, 31U);
			if(!angle3) return false;
			bullet.origin = {std::bit_cast<std::int16_t>(*x3), 0};
			bullet.speed = byte_add(static_cast<std::uint8_t>(*speed3), 16);
			bullet.angle = byte_add(static_cast<std::uint8_t>(*angle3), 48);
			return bullets_add(state, true, false);
		}
		if(boss.phase_frame >= 256) marisa_pattern_end(boss);
		return true;
	}
	(void)marisa_flystep_pointreflected(state, 160);
	if(boss.statebytes[15U] == 0U) {
		const auto angle = pc98::th0405::randring_next16_and(state.random, 31U);
		if(!angle) return false;
		bullet.angle = static_cast<std::uint8_t>(*angle);
		boss.statebytes[15U] = 1U;
	}
	if((boss.phase_frame & 7) == 0) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
		bullet.patnum = stage4_pat_bullet16_ball_blue;
		bullet.group = BulletGroup::stack;
		bullet.count = 16U;
		bullet.delta = 5U;
		bullet.speed = 16U;
		if(!bullet_template_tune(state) || !bullets_add(state, false, true)) return false;
		bullet.angle = byte_add(bullet.angle, 8);
		presentation.sound(15U);
	}
	if(boss.phase_frame >= 192) marisa_pattern_end(boss);
	return true;
}

[[nodiscard]] bool marisa_pattern_bitless_ring(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto charge = marisa_charge_animate(state, presentation);
	if(charge == 2U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		bullet.count = 32U;
		bullet.group = BulletGroup::ring;
		bullet.speed = 56U;
		if(!bullet_template_tune(state)) return false;
		const auto direction = pc98::th0405::randring_next16_and(state.random, 1U);
		if(!direction) return false;
		boss.statebytes[15U] = *direction == 0U ? 1U : static_cast<std::uint8_t>(-1);
		return true;
	}
	if(charge == 1U) {
		if((boss.phase_frame & 7) == 0) {
			if(!bullets_add(state)) return false;
			bullet.angle = byte_add(bullet.angle, static_cast<std::int8_t>(boss.statebytes[15U]));
			presentation.sound(9U);
		}
		if(boss.phase_frame >= 128) marisa_pattern_end(boss);
	}
	return true;
}

[[nodiscard]] bool marisa_pattern_bit_sweep(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	const auto initial_charge = marisa_charge_animate(state, presentation);
	if(initial_charge == 2U) {
		boss.statebytes[14U] = 32U;
		const auto mirror = pc98::th0405::randring_next16_and(state.random, 1U);
		if(!mirror) return false;
		boss.statebytes[15U] = static_cast<std::uint8_t>(*mirror);
	}
	// The source makes a second, independently effectful charge call here.
	if(marisa_charge_animate(state, presentation) != 1U) return true;
	if(boss.marisa_bits_alive != 0U) {
		if((boss.phase_frame & 3) == 0) {
			boss.marisa_bit_fire = MarisaBitFire::single;
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
			bullet.speed = 32U;
			bullet.group = BulletGroup::single;
			bullet.angle = static_cast<std::uint8_t>(state.stage_frame << 3U);
			if(boss.statebytes[15U] != 0U) bullet.angle = byte_add(0U, -static_cast<std::int16_t>(bullet.angle));
			if(!bullet_template_tune(state) || !marisa_bits_fire(state)) return false;
			if((boss.phase_frame & 7) == 0) {
				boss.marisa_bit_fire = MarisaBitFire::sideways_spread;
				bullet.spawn_type = static_cast<std::uint8_t>(
					BulletSpawnType::bullet16_cloud_forwards
				);
				bullet.patnum = stage4_pat_bullet16_ball_blue;
				bullet.speed = boss.statebytes[14U];
				bullet.group = BulletGroup::single;
				bullet.angle = 0U;
				if(!bullet_template_tune(state) || !marisa_bits_fire(state)) return false;
				boss.statebytes[14U] = byte_add(boss.statebytes[14U], 2);
			}
			presentation.sound(9U);
		}
		if(boss.phase_frame < 160) return true;
		for(auto &bit : boss.marisa_bits) {
			bit.angle_speed = static_cast<std::uint8_t>(-
				static_cast<std::int8_t>(bit.angle_speed));
		}
	} else {
		const auto done = marisa_flystep_pointreflected(state, 72);
		if(!done) {
			if((boss.phase_frame & 7) == 0) {
				bullet.spawn_type = static_cast<std::uint8_t>(
					BulletSpawnType::bullet16_cloud_forwards
				);
				bullet.patnum = stage4_pat_bullet16_ball_blue;
				bullet.group = BulletGroup::stack_aimed;
				bullet.count = 16U;
				bullet.delta = 5U;
				bullet.angle = 0U;
				bullet.speed = 16U;
				if(!bullet_template_tune(state) || !bullets_add(state, false, true)) return false;
				presentation.sound(15U);
			}
			return true;
		}
	}
	marisa_pattern_end(boss);
	return true;
}

[[nodiscard]] HitResult marisa_hittest(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	++boss.phase_frame;
	std::uint16_t damage{};
	if(!shots_hit(state, boss.pos.cur, {384, 384}, true, damage)) return HitResult::unsupported;
	if(damage != 0U) presentation.sound(4U);
	boss.damage_this_frame = static_cast<std::uint8_t>(damage);
	boss.hp = static_cast<std::int16_t>(static_cast<int>(boss.hp) -
		static_cast<int>(boss.damage_this_frame / (boss.marisa_bits_alive + 1U)));
	return boss.hp <= boss.phase_end_hp ? HitResult::ended : HitResult::none;
}

[[nodiscard]] bool marisa_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	bullet.origin = {word_sub(boss.pos.cur.x, 320), word_sub(boss.pos.cur.y, 128)};
	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 0) {
			boss.hp = stage4_marisa_hp;
			boss.marisa_bit_angle_speed = 2U;
		}
		++boss.phase_frame;
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(boss.phase_frame > 96) {
			++boss.phase;
			state.palette[0] = {0U, 0U, 7U};
			state.palette_changed = true;
			boss.phase_frame = 0;
			presentation.sound(13U);
			boss.marisa_pulse_dimming = false;
		}
		break;

	case 1U:
		// The visible phase and the invincible hittest each own one increment.
		boss.phase_frame = static_cast<std::int16_t>(boss.phase_frame + 2);
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(boss.phase_frame >= 128) {
			++boss.phase;
			boss.pos.velocity.x = 0;
			boss.phase_state = 0U;
			boss.mode = 10U;
			boss.phase_frame = 0;
			boss.sprite = stage4_pat_marisa_fight;
			boss.marisa_patterns_without_bits = 1U;
			boss.marisa_explode_milestone = 0U;
			boss.marisa_pattern_prev = 10U;
			boss.marisa_bits_at_pattern_start = 0U;
			boss.marisa_flystep_pointreflected_tick = 0U;
		}
		break;

	case 2U: {
		bool pattern_ok = true;
		switch(boss.mode) {
		case 0U: pattern_ok = marisa_pattern_respawn_bits(state, presentation); break;
		case 1U: pattern_ok = marisa_pattern_sideways_bits(state, presentation); break;
		case 2U: pattern_ok = marisa_pattern_aimed_bits(state, presentation); break;
		case 3U: pattern_ok = marisa_pattern_spiral_bits(state, presentation); break;
		case 4U: pattern_ok = marisa_pattern_aimed_rings(state, presentation); break;
		case 5U: pattern_ok = marisa_pattern_turning_stars(state, presentation); break;
		case 6U: pattern_ok = marisa_pattern_curtain(state, presentation); break;
		case 7U: pattern_ok = marisa_pattern_bit_sweep(state, presentation); break;
		case 10U: pattern_ok = marisa_pattern_bitless_clouds(state, presentation); break;
		case 11U: pattern_ok = marisa_pattern_bitless_ring(state, presentation); break;
		case stage4_mode_interval:
			if(!marisa_flystep_random(state)) return false;
			if(boss.phase_frame >= 64) {
				++boss.phase_state;
				boss.marisa_flystep_pointreflected_tick = 0U;
				if(boss.marisa_bits_at_pattern_start == 0U && boss.marisa_bits_alive == 0U) {
					++boss.marisa_patterns_without_bits;
					if(boss.marisa_patterns_without_bits >= 2U) {
						boss.mode = 0U;
						boss.marisa_patterns_without_bits = 0U;
					} else {
						const auto pattern = pc98::th0405::randring_next16_and(state.random, 1U);
						if(!pattern) return false;
						boss.mode = byte_add(static_cast<std::uint8_t>(*pattern), 10);
					}
				} else {
					std::uint8_t attempts{};
					for(;;) {
						const auto pattern = pc98::th0405::randring_next16_mod(state.random, 7U);
						if(!pattern) return false;
						const auto candidate = byte_add(static_cast<std::uint8_t>(*pattern), 1);
						if(candidate != boss.marisa_pattern_prev) {
							boss.mode = candidate;
							boss.marisa_pattern_prev = candidate;
							break;
						}
						++attempts;
						if(attempts == 0U) return false;
					}
					boss.marisa_bits_at_pattern_start = boss.marisa_bits_alive;
				}
				boss.phase_frame = 0;
				if(boss.phase_state >= 52U) {
					boss.phase_state = 0U;
					++boss.phase;
					boss_explode_small(state, presentation, 4U);
					boss.phase_frame = 0;
				}
			}
			break;
		default: return false;
		}
		if(!pattern_ok) return false;
		const auto hit = boss.phase == 2U ? marisa_hittest(state, presentation) :
			HitResult::none;
		if(hit == HitResult::unsupported) return false;
		if(hit == HitResult::ended) {
			boss.phase_state = 1U;
			boss_explode_small(state, presentation, 4U);
			++boss.phase;
			boss.phase_frame = 0;
		}
		if((state.stage_frame & 3U) == 0U) {
			if(!boss.marisa_pulse_dimming) {
				state.palette[0].b = byte_add(state.palette[0].b, 2);
				if(state.palette[0].b >= 192U) boss.marisa_pulse_dimming = true;
			} else {
				state.palette[0].b = byte_add(state.palette[0].b, -2);
				if(state.palette[0].b <= 38U) boss.marisa_pulse_dimming = false;
			}
			state.palette_changed = true;
		}
		if((boss.hp <= 4500 && boss.marisa_explode_milestone == 0U) ||
			(boss.hp <= 2500 && boss.marisa_explode_milestone == 1U) ||
			(boss.hp <= 1000 && boss.marisa_explode_milestone == 2U)) {
			if(!boss_items_drop(state, presentation)) return false;
			bullets_clear(state);
			if(!boss_score_bonus(state, presentation, 10U)) return false;
			boss_explode_small(state, presentation, boss.marisa_explode_milestone);
			++boss.marisa_explode_milestone;
		}
		break;
	}

	case 3U:
		++boss.phase_frame;
		if(boss.phase_frame == 16) boss_explode_small(state, presentation, 5U);
		if(boss.phase_frame == 32) {
			boss_explode_big(state, presentation, 3U);
			boss.phase = stage4_phase_explode_big;
			state.bullets.zap.set_active(boss.phase_state != 0U);
			if(boss.phase_state != 0U && !boss_score_bonus(state, presentation, 40U)) {
				return false;
			}
			boss.sprite = pat_enemy_kill;
			boss.phase_frame = 0;
			presentation.sound(12U);
			state.palette[0].r = 0U;
			state.palette[0].b = 0U;
			state.palette_changed = true;
			state.player_invincibility_time = boss_defeat_invincibility_frames;
		}
		break;

	case stage4_phase_explode_big:
		if(boss.phase_frame < 12) {
			state.playfield_shake_x = (state.stage_frame & 1U) == 0U ? -4 : 4;
			state.playfield_shake_y = (state.stage_frame & 3U) <= 1U ? -4 : 4;
		}
		state.bullets.slowdown_factor = 2U;
		++boss.phase_frame;
		if((boss.phase_frame & 7) == 0) {
			++boss.sprite;
			if(boss.sprite >= static_cast<std::uint8_t>(pat_enemy_kill_last + 1U)) {
				boss.phase = stage4_phase_none;
				boss.phase_frame = 0;
				state.bombing_disabled = true;
			}
		}
		return true;

	case stage4_phase_none:
		state.stage_clear_requested = true;
		return true;

	default: return false;
	}
	state.reimu_a.homing_target = boss.pos.cur;
	return marisa_bits_update_and_hittest(state, presentation);
}

} // namespace

void stage4_fights_setup(State &state) noexcept
{
	stage4_midboss_setup(state);

	auto &boss = state.boss;
	state.boss_active = false;
	boss.phase = 0U;
	boss.mode = 0U;
	boss.phase_state = 0U;
	boss.phase_frame = 0;
	boss.pos.cur = {3072, 1024};
	boss.pos.prev = boss.pos.cur;
	boss.pos.velocity = {};
	boss.damage_this_frame = 0U;
	boss.phase_timed_out = true;
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
	boss.sprite = pat_stage;
	boss.hitbox_radius = {384, 384};
	if(state.playchar != 0U) {
		switch(state.rank) {
		case 0U:
			boss.statebytes[0U] = 4U;
			boss.statebytes[1U] = 16U;
			boss.statebytes[2U] = 1U;
			boss.statebytes[3U] = 23U;
			boss.statebytes[4U] = 8U;
			boss.statebytes[5U] = 18U;
			boss.statebytes[6U] = 6U;
			break;
		case 1U:
			boss.statebytes[0U] = 6U;
			boss.statebytes[1U] = 12U;
			boss.statebytes[2U] = 2U;
			boss.statebytes[3U] = 23U;
			boss.statebytes[4U] = 9U;
			boss.statebytes[5U] = 16U;
			boss.statebytes[6U] = 8U;
			break;
		case 2U:
			boss.statebytes[0U] = 8U;
			boss.statebytes[1U] = 8U;
			boss.statebytes[2U] = 3U;
			boss.statebytes[3U] = 24U;
			boss.statebytes[4U] = 9U;
			boss.statebytes[5U] = 14U;
			boss.statebytes[6U] = 9U;
			break;
		default:
			boss.statebytes[0U] = 12U;
			boss.statebytes[1U] = 6U;
			boss.statebytes[2U] = 4U;
			boss.statebytes[3U] = 24U;
			boss.statebytes[4U] = 10U;
			boss.statebytes[5U] = 10U;
			boss.statebytes[6U] = 10U;
			break;
		}
	} else {
		boss.hp = stage4_marisa_hp;
	}
}

void stage4_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}

bool stage4_fights_update(State &state, Presentation &presentation) noexcept
{
	if(state.midboss_active && !stage4_midboss_update(state, presentation)) return false;
	if(!state.boss_active) return true;
	return state.playchar != 0U ? reimu_update(state, presentation) :
		marisa_update(state, presentation);
}

void stage4_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	stage4_midboss_render_tail(state, presentation);
	if(state.boss_active && state.boss.phase < stage4_phase_explode_big) {
		if(state.boss.damage_this_frame != 0U) state.boss.damage_this_frame = 0U;
		if(state.playchar == 0U) {
			for(auto &bit : state.boss.marisa_bits) {
				if(bit.flag == MarisaBitFlag::free) continue;
				if(bit.center.x <= -256 || bit.center.x >= playfield_width ||
					bit.center.y <= -256 || bit.center.y >= playfield_height) continue;
				bit.damage_this_frame = 0U;
			}
		}
	}
	// reimu_fg_render()/marisa_fg_render() both age the common explosion pools.
	stage1_fights_render_tail(state, presentation);
}

namespace {

constexpr std::uint8_t stage5_pat_bullet16_cross_yellow = 59U;
constexpr std::uint8_t stage5_pat_bullet16_directional_blue = 76U;
constexpr std::uint8_t stage5_pat_bullet16_ball_blue = 57U;
constexpr std::uint8_t stage5_pat_bullet16_outlined_ball_blue = 55U;
constexpr std::int16_t stage5_yuuka_hp = 9000;
constexpr std::uint8_t stage5_warp_none = 0U;
constexpr std::uint8_t stage5_warp_out = 1U;
constexpr std::uint8_t stage5_warp_traveling = 2U;
constexpr std::uint8_t stage5_warp_in = 3U;
constexpr std::uint8_t stage5_mode_next_pattern = 254U;
constexpr std::uint8_t stage5_mode_warp = 255U;

enum class Yuuka5WarpResult : std::uint8_t { running, done, unsupported };

[[nodiscard]] Yuuka5WarpResult yuuka5_warp(
	State &state, bool to_fixed_point
) noexcept
{
	auto &boss = state.boss;
	if(boss.yuuka5_warp_phase == stage5_warp_none) {
		boss.yuuka5_warp_phase = stage5_warp_out;
		boss.damage_this_frame = 0U;
		boss.pos.cur.y = word_add(boss.pos.cur.y, 256);
	}
	if(boss.yuuka5_warp_phase == stage5_warp_out) {
		if(boss.phase_frame < 32) return Yuuka5WarpResult::running;
		boss.phase_frame = 0;
		boss.yuuka5_warp_phase = stage5_warp_traveling;
		std::int16_t target_x{};
		std::int16_t target_y{};
		if(!to_fixed_point) {
			const auto x = pc98::th0405::randring_next16_mod(state.random, 4096U);
			if(!x) return Yuuka5WarpResult::unsupported;
			const auto y = pc98::th0405::randring_next16_mod(state.random, 1024U);
			if(!y) return Yuuka5WarpResult::unsupported;
			target_x = word_add(1024, static_cast<std::int16_t>(*x));
			target_y = word_add(1024, static_cast<std::int16_t>(*y));
		} else {
			target_x = 3072;
			target_y = 1280;
		}
		boss.pos.velocity.x = word_sub(target_x, boss.pos.cur.x) / 64;
		boss.pos.velocity.y = word_sub(target_y, boss.pos.cur.y) / 64;
	} else if(boss.yuuka5_warp_phase == stage5_warp_traveling) {
		pc98::th0405::update_motion(boss.pos);
		if(boss.phase_frame < 64) return Yuuka5WarpResult::running;
		boss.phase_frame = 0;
		boss.yuuka5_warp_phase = stage5_warp_in;
	} else if(boss.yuuka5_warp_phase == stage5_warp_in) {
		if(boss.phase_frame < 8) return Yuuka5WarpResult::running;
		boss.phase_frame = 0;
		boss.yuuka5_warp_phase = stage5_warp_none;
		boss.mode = stage5_mode_next_pattern;
		boss.pos.cur.y = word_sub(boss.pos.cur.y, 256);
		return Yuuka5WarpResult::done;
	} else {
		return Yuuka5WarpResult::unsupported;
	}
	return Yuuka5WarpResult::running;
}

[[nodiscard]] bool yuuka5_pattern_sweeping_spreads(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame == 1) {
		boss.yuuka5_spread_origin_x = word_sub(boss.pos.cur.x, 512);
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = stage5_pat_bullet16_cross_yellow;
		bullet.group = BulletGroup::spread;
		bullet.count = 11U;
		bullet.delta = 5U;
		bullet.speed = 40U;
		if(!bullet_template_tune(state)) return false;
	} else if(boss.phase_frame == 15) {
		bullet.angle = 0U;
	} else if(boss.phase_frame == 31) {
		boss.yuuka5_spread_origin_x = word_add(boss.yuuka5_spread_origin_x, 1024);
		bullet.angle = 128U;
	} else if(boss.phase_frame == 47) {
		boss.yuuka5_spread_origin_x = word_sub(boss.yuuka5_spread_origin_x, 1024);
		bullet.angle = 16U;
	} else if(boss.phase_frame == 63) {
		boss.yuuka5_spread_origin_x = word_add(boss.yuuka5_spread_origin_x, 1024);
		bullet.angle = 112U;
	} else if(boss.phase_frame == 79) {
		boss.yuuka5_spread_origin_x = word_sub(boss.yuuka5_spread_origin_x, 1024);
		bullet.angle = 32U;
	} else if(boss.phase_frame == 95) {
		boss.yuuka5_spread_origin_x = word_add(boss.yuuka5_spread_origin_x, 1024);
		bullet.angle = 96U;
	} else if(boss.phase_frame == 111) {
		boss.yuuka5_spread_origin_x = word_sub(boss.yuuka5_spread_origin_x, 1024);
		bullet.angle = 48U;
	} else if(boss.phase_frame == 127) {
		boss.yuuka5_spread_origin_x = word_add(boss.yuuka5_spread_origin_x, 1024);
		bullet.angle = 80U;
	} else if(boss.phase_frame == 140) {
		boss.mode = stage5_mode_warp;
		boss.phase_frame = 0;
	}
	if((boss.phase_frame & 15) == 15) {
		bullet.origin.x = boss.yuuka5_spread_origin_x;
		bullet.origin.y = boss.pos.cur.y;
		if(!bullets_add(state)) return false;
		presentation.sound(3U);
	}
	return true;
}

[[nodiscard]] bool yuuka5_pattern_accelerating_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame == 1) {
		bullet.angle = 0U;
		bullet.spawn_type = static_cast<std::uint8_t>(
			BulletSpawnType::bullet16_cloud_forwards
		);
		bullet.patnum = stage5_pat_bullet16_directional_blue;
		bullet.group = BulletGroup::ring;
		bullet.count = static_cast<std::uint8_t>(state.rank + 1U);
		bullet.speed = 64U;
		boss.yuuka5_ring_rate = 1U;
		boss.yuuka5_ring_accumulator = 0U;
	} else if(boss.phase_frame < 128) {
		if(boss.yuuka5_ring_accumulator >= 16U) {
			bullet.angle = byte_add(bullet.angle, 7);
			if(!bullets_add(state)) return false;
			presentation.sound(3U);
			++boss.yuuka5_ring_rate;
			boss.yuuka5_ring_accumulator = byte_add(
				boss.yuuka5_ring_accumulator, -16
			);
		}
		boss.yuuka5_ring_accumulator = byte_add(
			boss.yuuka5_ring_accumulator, boss.yuuka5_ring_rate
		);
	} else if(boss.phase_frame == 128) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = stage5_pat_bullet16_cross_yellow;
		bullet.angle = 128U;
		bullet.speed = 16U;
		bullet.special_motion = BulletSpecialMotion::speedup;
		state.bullets.special.value = 1U;
		bullet.count = 32U;
		if(!bullet_template_tune(state) || !bullets_add(state, true, true)) return false;
		bullet.spawn_type = static_cast<std::uint8_t>(
			BulletSpawnType::bullet16_cloud_forwards
		);
		bullet.speed = 64U;
		bullet.count = static_cast<std::uint8_t>(state.rank + 1U);
		boss.yuuka5_ring_rate = 1U;
		boss.yuuka5_ring_accumulator = 0U;
		presentation.sound(9U);
		bullet.patnum = stage5_pat_bullet16_directional_blue;
	} else if(boss.phase_frame < 256) {
		if(boss.yuuka5_ring_accumulator >= 16U) {
			bullet.angle = byte_add(bullet.angle, -7);
			if(!bullets_add(state)) return false;
			presentation.sound(3U);
			++boss.yuuka5_ring_rate;
			boss.yuuka5_ring_accumulator = byte_add(
				boss.yuuka5_ring_accumulator, -16
			);
		}
		boss.yuuka5_ring_accumulator = byte_add(
			boss.yuuka5_ring_accumulator, boss.yuuka5_ring_rate
		);
	} else if(boss.phase_frame == 256) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = stage5_pat_bullet16_cross_yellow;
		bullet.angle = 0U;
		bullet.speed = 16U;
		bullet.special_motion = BulletSpecialMotion::speedup;
		state.bullets.special.value = 1U;
		bullet.count = 32U;
		if(!bullet_template_tune(state) || !bullets_add(state, true, true)) return false;
		presentation.sound(9U);
	} else if(boss.phase_frame == 288) {
		boss.mode = stage5_mode_warp;
		boss.phase_frame = 0;
	}
	return true;
}

[[nodiscard]] bool yuuka5_pattern_bouncing_spread(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	auto &gather = state.bullets.gather_template;
	switch(boss.phase_frame) {
	case 1:
		gather.center = bullet.origin;
		gather.ring_points = 32;
		gather.col = 11U;
		gather.radius = 4096;
		gather.angle_delta = 3U;
		gather_add_only(state);
		break;

	case 3:
		gather.col = 10U;
		gather_add_only(state);
		break;

	case 5:
		gather_add_only(state);
		break;

	case 17:
		circle_add_shrinking(state, bullet.origin);
		state.circles_color = 15U;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.speed = 16U;
		bullet.special_motion = BulletSpecialMotion::bounce_left_right_top;
		bullet.group = BulletGroup::spread;
		bullet.count = 7U;
		bullet.delta = 8U;
		bullet.patnum = stage5_pat_bullet16_cross_yellow;
		bullet.speed = 32U;
		if(!bullet_template_tune(state)) return false;
		state.bullets.special.value = 1U;
		break;

	default:
		break;
	}
	if(boss.phase_frame >= 32 && (boss.phase_frame & 15) == 0) {
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return false;
		bullet.angle = static_cast<std::uint8_t>(*angle);
		if(!bullets_add(state, true, false)) return false;
		presentation.sound(9U);
	}
	return true;
}

[[nodiscard]] bool yuuka5_pattern_speedup_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame == 1) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = stage5_pat_bullet16_cross_yellow;
		bullet.group = BulletGroup::ring_aimed;
		bullet.delta = 6U;
		bullet.speed = 16U;
		bullet.special_motion = BulletSpecialMotion::speedup;
		bullet.count = 8U;
		state.bullets.special.value = 1U;
	} else if(boss.phase_frame == 170) {
		boss.mode = stage5_mode_warp;
		boss.phase_frame = 0;
	}
	if((boss.phase_frame & 15) == 15) {
		if(!bullets_add(state, true, false)) return false;
		bullet.count = byte_add(bullet.count, 3);
		presentation.sound(3U);
	}
	return true;
}

[[nodiscard]] bool yuuka5_pattern_aimed_cloud_spreads(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame == 1) {
		const auto aimed = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, bullet.origin.y),
			word_sub(state.player_pos.cur.x, bullet.origin.x)
		);
		if(!aimed) return false;
		bullet.angle = *aimed;
		bullet.spawn_type = static_cast<std::uint8_t>(
			BulletSpawnType::bullet16_cloud_forwards
		);
		bullet.patnum = stage5_pat_bullet16_ball_blue;
		bullet.group = BulletGroup::spread;
		bullet.count = 5U;
		bullet.delta = 66U;
		bullet.speed = 80U;
		if(!bullet_template_tune(state)) return false;
	} else if(boss.phase_frame == 128) {
		boss.mode = stage5_mode_warp;
		boss.phase_frame = 0;
	}
	if((boss.phase_frame & 7) == 7) {
		bullet.delta = byte_add(bullet.delta, -4);
		if(!bullets_add(state)) return false;
		presentation.sound(3U);
	}
	return true;
}

[[nodiscard]] bool yuuka5_pattern_laser_rings(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	auto &gather = state.bullets.gather_template;
	switch(boss.phase_frame) {
	case 16:
		gather.angle_delta = 3U;
		break;

	case 48:
		bullet.angle = 0U;
		presentation.sound(8U);
		boss.yuuka5_palette_tone = 100U;
		[[fallthrough]];
	case 56:
	case 64:
	case 72:
	case 80:
		circle_add_shrinking(state, bullet.origin);
		state.circles_color = 15U;
		[[fallthrough]];
	case 40:
		gather.angle_delta = static_cast<std::uint8_t>(-
			static_cast<std::int8_t>(gather.angle_delta)
		);
		gather.center = bullet.origin;
		gather.ring_points = 8;
		gather.col = 9U;
		gather.radius = 4096;
		[[fallthrough]];
	case 44:
	case 52:
	case 60:
	case 68:
	case 76:
	case 84:
		gather_add_only(state);
		break;

	case 42:
	case 50:
	case 58:
	case 66:
	case 74:
	case 82:
		gather.col = 8U;
		gather_add_only(state);
		break;

	case 96:
		state.thicklasers.template_.origin = bullet.origin;
		state.thicklasers.template_.radius_max = static_cast<std::int16_t>(
			boss.statebytes[0U]
		);
		state.thicklasers.template_.radius_speed = 6;
		state.thicklasers.template_.line_frames = 32;
		state.thicklasers.template_.static_frames = 144;
		state.thicklasers.template_.col_outline = 8U;
		thicklaser_add(state, presentation);
		break;

	default:
		break;
	}
	if(boss.phase_frame < 128) return true;
	if(boss.phase_frame <= 160) {
		boss.yuuka5_palette_tone = byte_add(boss.yuuka5_palette_tone, 2);
		state.palette_tone = static_cast<std::uint16_t>(boss.yuuka5_palette_tone);
		state.palette_changed = true;
	} else if(boss.yuuka5_palette_tone > 100U) {
		boss.yuuka5_palette_tone = byte_add(boss.yuuka5_palette_tone,
			-static_cast<std::int16_t>(boss.phase_frame & 1));
		state.palette_tone = static_cast<std::uint16_t>(boss.yuuka5_palette_tone);
		state.palette_changed = true;
	}
	if((boss.phase_frame & 31) == 0) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.group = BulletGroup::ring;
		bullet.count = 32U;
		bullet.patnum = stage5_pat_bullet16_directional_blue;
		bullet.speed = 72U;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		bullet.angle = byte_add(bullet.angle, 2);
		presentation.sound(9U);
	}
	if(boss.phase_frame >= 192 && (state.stage_frame & 1U) != 0U) {
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.group = BulletGroup::random_angle;
		bullet.patnum = stage5_pat_bullet16_outlined_ball_blue;
		bullet.speed = 32U;
		bullet.count = 2U;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
	}
	return true;
}

[[nodiscard]] bool yuuka5_pattern_mirrored_pairs(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame == 48) {
		circle_add_shrinking(state, bullet.origin);
		state.circles_color = 15U;
		boss.angle = 16U;
		boss.statebytes[15U] = 16U;
		bullet.special_motion = BulletSpecialMotion::none;
		return true;
	}
	if(boss.phase_frame < 64 || (boss.phase_frame & 7) != 0) return true;
	bullet.spawn_type = static_cast<std::uint8_t>(
		BulletSpawnType::bullet16_cloud_forwards
	);
	bullet.count = 5U;
	bullet.delta = 1U;
	bullet.group = BulletGroup::spread;
	bullet.patnum = stage5_pat_bullet16_ball_blue;
	bullet.speed = 40U;
	if(!bullet_template_tune(state)) return false;
	bullet.origin.x = word_add(bullet.origin.x, 512);
	bullet.angle = boss.angle;
	if(!bullets_add(state, true, false)) return false;
	bullet.origin.x = word_sub(bullet.origin.x, 1024);
	bullet.angle = byte_add(128U, -static_cast<std::int16_t>(boss.angle));
	if(!bullets_add(state, true, false)) return false;
	boss.angle = byte_add(boss.angle, -16);
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
	bullet.count = 3U;
	bullet.speed = 24U;
	if(!bullet_template_tune(state)) return false;
	bullet.angle = boss.statebytes[15U];
	if(!bullets_add(state, true, false)) return false;
	bullet.origin.x = word_add(bullet.origin.x, 1024);
	bullet.angle = byte_add(128U, -static_cast<std::int16_t>(boss.statebytes[15U]));
	if(!bullets_add(state, true, false)) return false;
	boss.statebytes[15U] = byte_add(boss.statebytes[15U], 9);
	presentation.sound(3U);
	return true;
}

[[nodiscard]] bool yuuka5_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	bullet.origin.x = boss.pos.cur.x;
	bullet.origin.y = word_add(boss.pos.cur.y, 256);
	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 0) state.midboss.frames_until = 0U;
		++boss.phase_frame;
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(boss.phase_frame > 128) {
			++boss.phase;
			boss.phase_frame = 0;
			presentation.sound(13U);
			boss.yuuka5_warp_phase = stage5_warp_none;
		}
		break;

	case 1U:
		++boss.phase_frame;
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(boss.phase_frame == 32) {
			state.palette[0] = {64U, 64U, 64U};
			state.palette_changed = true;
		}
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.pos.velocity.x = 0;
			boss.phase_state = 0U;
			boss.mode = 0U;
			boss.hp = stage5_yuuka_hp;
			boss.phase_end_hp = 7900;
			boss.phase_frame = 0;
			boss.pos.cur.y = word_sub(boss.pos.cur.y, 256);
		}
		break;

	case 2U:
	case 5U:
	case 8U: {
		bool pattern_ok = true;
		switch(boss.mode) {
		case 0U: pattern_ok = yuuka5_pattern_sweeping_spreads(state, presentation); break;
		case 1U: pattern_ok = yuuka5_pattern_accelerating_rings(state, presentation); break;
		case stage5_mode_next_pattern:
			boss.phase_frame = 0;
			++boss.phase_state;
			boss.mode = static_cast<std::uint8_t>(boss.phase_state & 1U);
			break;
		case stage5_mode_warp:
			if(yuuka5_warp(state, false) == Yuuka5WarpResult::unsupported) return false;
			break;
		default:
			return false;
		}
		if(!pattern_ok) return false;
		if(boss.yuuka5_warp_phase == stage5_warp_none) {
			if(boss.phase_state < 4U) {
				const auto hit = boss_hittest(state, presentation);
				if(hit == HitResult::unsupported) return false;
				if(hit == HitResult::none) break;
				if(!boss_score_bonus(state, presentation, 15U) ||
					!boss_items_drop(state, presentation)) return false;
			}
			bullets_clear(state);
			boss_explode_small(state, presentation, 0U);
			++boss.phase;
			boss.hp = boss.phase_end_hp;
			boss.phase_end_hp = static_cast<std::int16_t>(boss.phase_end_hp - 800);
		} else {
			++boss.phase_frame;
		}
		break;
	}

	case 3U:
	case 6U:
	case 9U:
		++boss.phase_frame;
		if(yuuka5_warp(state, true) == Yuuka5WarpResult::unsupported) return false;
		if(boss.yuuka5_warp_phase == stage5_warp_none) {
			++boss.phase;
			boss.phase_frame = 0;
			boss.phase_state = 0U;
			boss.mode = 0U;
		}
		break;

	case 4U:
	case 7U:
	case 10U: {
		if(!yuuka5_pattern_bouncing_spread(state, presentation)) return false;
		if(boss.phase_frame < 500) {
			const auto hit = boss_hittest(state, presentation);
			if(hit == HitResult::unsupported) return false;
			if(hit == HitResult::none) break;
			if(!boss_score_bonus(state, presentation, 15U) ||
				!boss_items_drop(state, presentation)) return false;
		}
		bullets_clear(state);
		boss_explode_small(state, presentation, 1U);
		++boss.phase;
		boss.phase_frame = 0;
		boss.phase_state = 0U;
		boss.mode = 0U;
		boss.hp = boss.phase_end_hp;
		boss.phase_end_hp = static_cast<std::int16_t>(boss.phase_end_hp -
			(boss.phase < 10U ? 1100 : 1200));
		break;
	}

	case 11U:
	case 14U: {
		bool pattern_ok = true;
		switch(boss.mode) {
		case 0U: pattern_ok = yuuka5_pattern_speedup_rings(state, presentation); break;
		case 1U: pattern_ok = yuuka5_pattern_aimed_cloud_spreads(state, presentation); break;
		case stage5_mode_next_pattern:
			boss.phase_frame = 0;
			++boss.phase_state;
			boss.mode = static_cast<std::uint8_t>(boss.phase_state & 1U);
			break;
		case stage5_mode_warp:
			if(yuuka5_warp(state, false) == Yuuka5WarpResult::unsupported) return false;
			break;
		default:
			return false;
		}
		if(!pattern_ok) return false;
		if(boss.yuuka5_warp_phase == stage5_warp_none) {
			if(boss.phase_state < 4U) {
				const auto hit = boss_hittest(state, presentation);
				if(hit == HitResult::unsupported) return false;
				if(hit == HitResult::none) break;
				if(!boss_score_bonus(state, presentation, 15U) ||
					!boss_items_drop(state, presentation)) return false;
			}
			bullets_clear(state);
			boss_explode_small(state, presentation, 0U);
			++boss.phase;
			boss.hp = boss.phase_end_hp;
		} else {
			++boss.phase_frame;
		}
		break;
	}

	case 12U:
	case 15U:
		++boss.phase_frame;
		if(yuuka5_warp(state, true) == Yuuka5WarpResult::unsupported) return false;
		if(boss.yuuka5_warp_phase == stage5_warp_none) {
			++boss.phase;
			boss.phase_frame = 0;
			boss.phase_state = 0U;
			boss.mode = 0U;
		}
		break;

	case 13U:
	case 16U:
		if(!yuuka5_pattern_laser_rings(state, presentation)) return false;
		++boss.phase_frame;
		if(!boss_hittest_invincible(state, presentation, boss.hitbox_radius)) return false;
		if(boss.phase_frame >= 288) {
			boss_explode_small(state, presentation, 4U);
			bullets_clear(state);
			++boss.phase;
			boss.hp = boss.phase_end_hp;
			if(boss.phase == 17U) {
				boss.phase_end_hp = 0;
				state.palette[0] = {128U, 64U, 64U};
				state.palette_changed = true;
			} else {
				boss.phase_end_hp = static_cast<std::int16_t>(boss.phase_end_hp - 1200);
			}
			boss.phase_frame = 0;
			boss.phase_state = 0U;
			boss.mode = 0U;
			state.palette_tone = 100U;
			state.palette_changed = true;
		}
		break;

	case 17U: {
		if(!yuuka5_pattern_mirrored_pairs(state, presentation)) return false;
		const auto hit = boss_hittest(state, presentation);
		if(hit == HitResult::unsupported) return false;
		if(hit == HitResult::none && boss.phase_frame < 1000) break;
		boss_explode_small(state, presentation, 1U);
		++boss.phase;
		boss.phase_state = boss.phase_frame < 1000 ? 1U : 0U;
		boss.phase_frame = 0;
		boss.mode = 0U;
		state.palette_tone = 100U;
		state.palette_changed = true;
		break;
	}

	case 18U:
		++boss.phase_frame;
		if(boss.phase_frame == 16) boss_explode_small(state, presentation, 4U);
		if(boss.phase_frame == 32) {
			boss_explode_big(state, presentation, 2U);
			boss.phase = phase_explode_big;
			state.bullets.zap.set_active(boss.phase_state != 0U);
			if(boss.phase_state != 0U && !boss_score_bonus(state, presentation, 60U)) {
				return false;
			}
			boss.sprite = pat_enemy_kill;
			boss.phase_frame = 0;
			presentation.sound(12U);
			state.palette[0] = {};
			state.palette_changed = true;
			state.player_invincibility_time = boss_defeat_invincibility_frames;
		}
		break;

	case phase_explode_big:
		if(boss.phase_frame < 12) {
			state.playfield_shake_x = (state.stage_frame & 1U) == 0U ? -4 : 4;
			state.playfield_shake_y = (state.stage_frame & 3U) <= 1U ? -4 : 4;
		}
		state.bullets.slowdown_factor = 2U;
		++boss.phase_frame;
		if((boss.phase_frame & 7) == 0) {
			++boss.sprite;
			if(boss.sprite >= static_cast<std::uint8_t>(pat_enemy_kill_last + 1U)) {
				boss.phase = phase_none;
				boss.phase_frame = 0;
				state.bombing_disabled = true;
			}
		}
		return true;

	case phase_none:
		state.palette_tone = 60U;
		state.palette_changed = true;
		state.stage_clear_requested = true;
		return true;

	default:
		return false;
	}
	state.reimu_a.homing_target = boss.pos.cur;
	thicklasers_update_and_hittest(state, presentation);
	return true;
}

} // namespace

void stage5_fights_setup(State &state) noexcept
{
	auto &boss = state.boss;
	state.midboss_active = false;
	state.midboss.frames_until = 60000U;
	state.boss_active = false;
	boss.phase = 0U;
	boss.mode = 0U;
	boss.phase_state = 0U;
	boss.phase_frame = 0;
	boss.pos.cur = {3072, 1024};
	boss.pos.prev = boss.pos.cur;
	boss.pos.velocity = {};
	boss.damage_this_frame = 0U;
	boss.phase_timed_out = true;
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
	boss.sprite = pat_stage;
	boss.hitbox_radius = {416, 416};
	switch(state.rank) {
	case 0U:
		boss.statebytes[0U] = 144U;
		break;
	case 1U:
		boss.statebytes[0U] = 160U;
		break;
	case 2U:
		boss.statebytes[0U] = 168U;
		break;
	default:
		boss.statebytes[0U] = 180U;
		break;
	}
}

void stage5_boss_activate(State &state) noexcept
{
	state.boss_active = true;
}

bool stage5_fights_update(State &state, Presentation &presentation) noexcept
{
	if(!state.boss_active) return true;
	return yuuka5_update(state, presentation);
}

void stage5_fights_render_tail(State &state, Presentation &presentation) noexcept
{
	if(state.boss_active) {
		const auto &boss = state.boss;
		if(boss.phase <= 1U || (boss.phase < phase_explode_big &&
			boss.yuuka5_warp_phase == stage5_warp_none)) {
			state.boss.damage_this_frame = 0U;
		}
	}
	stage1_fights_render_tail(state, presentation);
	if(state.boss.phase < phase_none) thicklasers_render(state, presentation);
}

} // namespace th04::core

namespace th04::core {
namespace {

constexpr std::uint8_t extra_pat_mugetsu_idle = 128U;
constexpr std::uint8_t extra_pat_mugetsu_flap_closed = 129U;
constexpr std::uint8_t extra_pat_mugetsu_flap_open = 130U;
constexpr std::uint8_t extra_pat_mugetsu_turn_1 = 131U;
constexpr std::uint8_t extra_pat_mugetsu_turn_2 = 132U;
constexpr std::uint8_t extra_pat_mugetsu_turn_3 = 133U;
constexpr std::uint8_t extra_pat_mugetsu_turn_4 = 134U;
constexpr std::uint8_t extra_pat_mugetsu_turn_5 = 135U;
constexpr std::uint8_t extra_pat_gengetsu_tipping = 128U;
constexpr std::uint8_t extra_pat_gengetsu_flap_a = 130U;
constexpr std::uint8_t extra_pat_gengetsu_fire = 132U;
constexpr std::uint8_t extra_pat_gengetsu_flap_b = 134U;
constexpr std::uint8_t extra_pat_bullet16_white = 52U;
constexpr std::uint8_t extra_pat_bullet16_ball_blue = 57U;
constexpr std::uint8_t extra_pat_bullet16_small_ball_yellow = 58U;
constexpr std::uint8_t extra_pat_bullet16_cross_yellow = 59U;
constexpr std::uint8_t extra_pat_bullet16_heart_red = 62U;
constexpr std::uint8_t extra_pat_bullet16_directional_blue = 76U;
constexpr std::uint8_t extra_mode_teleport = 255U;
constexpr std::uint8_t extra_phase_explode_big = 254U;
constexpr std::uint8_t extra_phase_none = 255U;
constexpr std::int16_t extra_mugetsu_hp = 9400;
constexpr std::int16_t extra_gengetsu_hp = 18700;
constexpr std::int16_t extra_bomb_immunity_frames = 32;

[[nodiscard]] bool extra_random_mod(
	State &state, std::uint16_t modulus, std::uint16_t &result
) noexcept
{
	const auto value = pc98::th0405::randring_next16_mod(state.random, modulus);
	if(!value) return false;
	result = *value;
	return true;
}

[[nodiscard]] bool extra_random_byte(State &state, std::uint8_t &result) noexcept
{
	const auto value = pc98::th0405::randring_next16(state.random);
	if(!value) return false;
	result = static_cast<std::uint8_t>(*value);
	return true;
}

void extra_bomb_immunity_update(State &state) noexcept
{
	auto &boss = state.boss;
	if(state.bombing) boss.extra_boss_bomb_immunity = extra_bomb_immunity_frames;
	if(boss.extra_boss_bomb_immunity != 0U) --boss.extra_boss_bomb_immunity;
}

[[nodiscard]] bool mugetsu_hittest(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	if(boss.extra_boss_bomb_immunity != 0U) {
		if(!boss_hittest_invincible(state, presentation, {768, 768})) return false;
	} else if(boss.sprite <= extra_pat_mugetsu_flap_open && boss.sprite != 0U) {
		return boss_hittest(state, presentation) == HitResult::ended;
	}
	++boss.phase_frame;
	return false;
}

[[nodiscard]] bool extra_boss_defeat_update(State &state) noexcept;

[[nodiscard]] bool gengetsu_hittest(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	if(boss.extra_boss_bomb_immunity != 0U && boss.gengetsu_wave_amplitude == 0U) {
		if(!boss_hittest_invincible(state, presentation, {768, 768})) return false;
	} else if(boss.sprite != 0U && boss.gengetsu_wave_amplitude == 0U) {
		return boss_hittest(state, presentation) == HitResult::ended;
	}
	++boss.phase_frame;
	return false;
}

void mugetsu_gather_pair(State &state) noexcept
{
	state.bullets.gather_template.angle_delta = static_cast<std::uint8_t>(-2);
	gather_add_only(state);
	state.bullets.gather_template.angle_delta = 2U;
	gather_add_only(state);
}

void mugetsu_gather_update(State &state) noexcept
{
	auto &boss = state.boss;
	auto &gather = state.bullets.gather_template;
	switch(static_cast<std::int16_t>(boss.phase_frame + boss.mugetsu_gather_frame_offset)) {
	case 32:
		gather.radius = 5120;
		gather.center.y = word_sub(boss.mugetsu_gather_center.y, 160);
		gather.center.x = boss.mugetsu_gather_center.x;
		gather.ring_points = 16;
		gather.col = 14U;
		[[fallthrough]];
	case 36:
		mugetsu_gather_pair(state);
		break;
	case 34:
		gather.col = 7U;
		mugetsu_gather_pair(state);
		break;
	case 48:
		circle_add_shrinking(state, gather.center);
		state.circles_color = 15U;
		break;
	default:
		break;
	}
}

[[nodiscard]] std::uint8_t mugetsu_pose_short(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	boss.mugetsu_gather_frame_offset = 16;
	mugetsu_gather_update(state);
	if(boss.phase_frame < 16) return 0U;
	if(boss.phase_frame == 16) boss.sprite = extra_pat_mugetsu_flap_closed;
	if(boss.sprite < 24U) return 0U;
	if(boss.phase_frame < 48) {
		if(boss.phase_frame == 24) presentation.sound(8U);
		boss.sprite = (state.stage_frame & 1U) != 0U
			? extra_pat_mugetsu_flap_open : extra_pat_mugetsu_flap_closed;
		return 0U;
	}
	if(boss.phase_frame == 48) {
		boss.sprite = extra_pat_mugetsu_idle;
		return 1U;
	}
	return boss.phase_frame < 128 ? 2U : 3U;
}

[[nodiscard]] std::uint8_t mugetsu_pose_turn(
	State &state, Presentation &presentation, bool long_timeline
) noexcept
{
	auto &boss = state.boss;
	boss.mugetsu_gather_frame_offset = long_timeline ? -80 : 0;
	mugetsu_gather_update(state);
	switch(boss.phase_frame) {
	case 34: boss.sprite = 0U; boss.pos.cur = boss.mugetsu_gather_center; break;
	case 32: case 38: boss.sprite = extra_pat_mugetsu_turn_5; break;
	case 30: case 40: boss.sprite = extra_pat_mugetsu_turn_4; break;
	case 28: case 42: boss.sprite = extra_pat_mugetsu_turn_3; break;
	case 26: case 44: boss.sprite = extra_pat_mugetsu_turn_2; break;
	case 24: case 46: boss.sprite = extra_pat_mugetsu_turn_1; break;
	case 16: case 48: boss.sprite = extra_pat_mugetsu_flap_closed; break;
	default: break;
	}
	const auto flap_end = long_timeline ? 128 : 64;
	const auto pattern_end = long_timeline ? 192 : 144;
	if(boss.phase_frame < 48) return 0U;
	if(boss.phase_frame < flap_end) {
		if(long_timeline && boss.phase_frame == 48) presentation.sound(8U);
		boss.sprite = (state.stage_frame & 1U) != 0U
			? extra_pat_mugetsu_flap_open : extra_pat_mugetsu_flap_closed;
		return 0U;
	}
	if(boss.phase_frame == flap_end) {
		boss.sprite = extra_pat_mugetsu_idle;
		return 1U;
	}
	return boss.phase_frame < pattern_end ? 2U : 3U;
}

[[nodiscard]] std::uint8_t mugetsu_pose(State &state, Presentation &presentation) noexcept
{
	switch(state.boss.mugetsu_pose) {
	case ExtraMugetsuPose::short_180bb:
		return mugetsu_pose_short(state, presentation);
	case ExtraMugetsuPose::medium_1812a:
		return mugetsu_pose_turn(state, presentation, false);
	case ExtraMugetsuPose::long_1821e:
		return mugetsu_pose_turn(state, presentation, true);
	}
	return 0U;
}

[[nodiscard]] bool mugetsu_pattern_ring(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	switch(mugetsu_pose(state, presentation)) {
	case 1U: {
		std::uint8_t angle{};
		if(!extra_random_byte(state, angle)) return false;
		const auto direction = pc98::th0405::randring_next16_and(state.random, 1U);
		if(!direction) return false;
		bullet.group = BulletGroup::ring;
		bullet.count = 8U;
		bullet.speed = 32U;
		bullet.angle = angle;
		boss.statebytes[15U] = static_cast<std::uint8_t>(*direction);
		break;
	}
	case 2U:
		if((state.stage_frame & 3U) == 0U) {
			if(!bullets_add(state)) return false;
			bullet.speed = byte_add(bullet.speed, 3);
			bullet.angle = byte_add(bullet.angle,
				boss.statebytes[15U] != 0U ? 4 : -4);
			presentation.sound(3U);
		}
		break;
	case 3U:
		boss.phase_frame = 0;
		boss.mode = extra_mode_teleport;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool mugetsu_pattern_crosses(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	switch(mugetsu_pose(state, presentation)) {
	case 1U: {
		const auto aimed = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, boss.pos.cur.y),
			word_sub(state.player_pos.cur.x, boss.pos.cur.x)
		);
		if(!aimed) return false;
		presentation.sound(15U);
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = extra_pat_bullet16_cross_yellow;
		bullet.angle = 0U;
		bullet.group = BulletGroup::ring;
		bullet.count = 32U;
		bullet.special_motion = BulletSpecialMotion::decelerate_then_turn;
		bullet.speed = 40U;
		state.bullets.special.value = 2U;
		state.bullets.template_special_angle = static_cast<std::uint8_t>(-32);
		if(!bullet_template_tune(state) || !bullets_add(state, true, true)) return false;
		state.bullets.template_special_angle = 32U;
		if(!bullets_add(state, true, true)) return false;
		bullet.angle = *aimed;
		bullet.delta = 66U;
		break;
	}
	case 2U:
		if((boss.phase_frame & 7) == 0U) bullet.delta = byte_add(bullet.delta, -8);
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.patnum = extra_pat_bullet16_directional_blue;
		bullet.speed = static_cast<std::uint8_t>((boss.phase_frame & 7) * 12 + 32);
		bullet.group = BulletGroup::spread;
		bullet.count = 2U;
		if(!bullets_add(state)) return false;
		if((state.stage_frame & 3U) == 0U) presentation.sound(3U);
		break;
	case 3U:
		boss.phase_frame = 0;
		boss.mode = extra_mode_teleport;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool mugetsu_pattern_cloud(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	switch(mugetsu_pose(state, presentation)) {
	case 1U: {
		std::uint8_t angle{};
		if(!extra_random_byte(state, angle)) return false;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_backwards);
		bullet.patnum = extra_pat_bullet16_ball_blue;
		bullet.group = BulletGroup::ring;
		bullet.count = 40U;
		bullet.speed = 16U;
		bullet.angle = angle;
		if(!bullets_add(state)) return false;
		presentation.sound(15U);
		break;
	}
	case 2U:
		boss.phase_frame = 0;
		boss.mode = extra_mode_teleport;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool mugetsu_pattern_turning(State &state, Presentation &presentation,
	bool rings) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	switch(mugetsu_pose(state, presentation)) {
	case 1U:
		bullet.group = rings ? BulletGroup::ring : BulletGroup::single;
		if(rings) {
			if(!bullet_template_tune(state)) return false;
		} else {
			bullet.special_motion = BulletSpecialMotion::decelerate_then_turn;
			state.bullets.special.value = 1U;
		}
		break;
	case 2U:
		if((state.stage_frame & (rings ? 7U : 1U)) == 0U) {
			std::uint16_t speed{};
			std::uint16_t x{};
			std::uint16_t y{};
			if(rings) {
				const auto spawn_type = pc98::th0405::randring_next16_and(state.random, 1U);
				if(!spawn_type || !extra_random_mod(state, 64U, speed) ||
					!extra_random_mod(state, 1024U, x) || !extra_random_mod(state, 512U, y)) return false;
				bullet.spawn_type = static_cast<std::uint8_t>(*spawn_type);
			} else if(!extra_random_mod(state, 64U, speed) ||
				!extra_random_mod(state, 1024U, x) || !extra_random_mod(state, 512U, y)) return false;
			if(rings) {
				bullet.patnum = extra_pat_bullet16_ball_blue;
			} else {
				bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
				bullet.patnum = extra_pat_bullet16_small_ball_yellow;
			}
			bullet.speed = static_cast<std::uint8_t>(speed + 16U);
			bullet.origin.x = word_add(word_sub(boss.pos.cur.x, 512), static_cast<std::int16_t>(x));
			bullet.origin.y = word_add(word_sub(boss.pos.cur.y, rings ? 416 : 416), static_cast<std::int16_t>(y));
			if(rings) {
				std::uint8_t angle{};
				if(!extra_random_byte(state, angle)) return false;
				bullet.angle = angle;
				if(!bullets_add(state)) return false;
				presentation.sound(3U);
			} else {
				bullet.angle = 0U;
				state.bullets.template_special_angle = 64U;
				if(!bullets_add(state, true, false)) return false;
				const auto speed_second = pc98::th0405::randring_next16_and(state.random, 63U);
				if(!speed_second) return false;
				bullet.speed = static_cast<std::uint8_t>(*speed_second + 16U);
				bullet.angle = 128U;
				state.bullets.template_special_angle = static_cast<std::uint8_t>(-64);
				if(!bullets_add(state, true, false)) return false;
				presentation.sound(9U);
			}
		}
		break;
	case 3U:
		boss.phase_frame = 0;
		boss.mode = extra_mode_teleport;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool mugetsu_phase_next(
	State &state, Presentation &presentation, std::uint8_t explosion,
	std::int16_t next_end_hp
) noexcept
{
	if(!boss_items_drop(state, presentation)) return false;
	boss_explode_small(state, presentation, explosion);
	auto &boss = state.boss;
	++boss.phase;
	boss.phase_frame = 0;
	boss.mode = 0U;
	boss.phase_state = 0U;
	boss.hp = boss.phase_end_hp;
	boss.phase_end_hp = next_end_hp;
	boss.mugetsu_phase2_mode = 0U;
	return true;
}

[[nodiscard]] bool mugetsu_final_pattern(State &state) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if((state.stage_frame & 7U) != 0U) return true;
	bullet.angle = boss.angle;
	bullet.group = BulletGroup::ring;
	bullet.count = 16U;
	bullet.speed = static_cast<std::uint8_t>(boss.phase_frame / 256 + 32);
	if(!bullets_add(state)) return false;
	bullet.patnum = extra_pat_bullet16_directional_blue;
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
	bullet.angle = static_cast<std::uint8_t>(-static_cast<std::int16_t>(bullet.angle) * 2);
	bullet.count = 4U;
	bullet.speed = byte_add(bullet.speed, 16);
	if(!bullets_add(state)) return false;
	boss.angle = byte_add(boss.angle, (boss.phase_frame % 1024) < 512 ? 3 : -3);
	return true;
}

[[nodiscard]] bool mugetsu_fast_ring(State &state) noexcept
{
	if((state.stage_frame & 7U) != 0U) return true;
	auto &bullet = state.bullets.template_;
	std::uint8_t angle{};
	if(!extra_random_byte(state, angle)) return false;
	bullet.group = BulletGroup::ring;
	bullet.count = 32U;
	bullet.patnum = extra_pat_bullet16_directional_blue;
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
	bullet.angle = angle;
	bullet.speed = 112U;
	return bullets_add(state);
}

[[nodiscard]] bool mugetsu_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	extra_bomb_immunity_update(state);
	bullet.origin = {boss.pos.cur.x, word_sub(boss.pos.cur.y, 160)};
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
	switch(boss.phase) {
	case 0U:
		if(boss.phase_frame == 0) {
			state.midboss.frames_until = 0U;
			boss.mugetsu_pose = ExtraMugetsuPose::short_180bb;
			boss.extra_boss_bomb_immunity = 0U;
			boss.hp = extra_mugetsu_hp;
			boss.phase_end_hp = 3700;
			boss.mugetsu_gather_center = boss.pos.cur;
		}
		(void)mugetsu_hittest(state, presentation);
		if(boss.phase_frame > 128) {
			++boss.phase;
			boss.phase_frame = 0;
			presentation.sound(13U);
		}
		break;
	case 1U:
		(void)mugetsu_hittest(state, presentation);
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.mode = 0U;
			boss.phase_state = 2U;
			boss.phase_frame = 0;
			boss.pos.velocity.x = 0;
			boss.mugetsu_phase2_mode = 0U;
		}
		break;
	case 2U:
		switch(boss.mode) {
		case 0U: if(!mugetsu_pattern_ring(state, presentation)) return false; break;
		case 1U: case 4U: case 5U:
			if(!mugetsu_pattern_cloud(state, presentation)) return false;
			break;
		case 2U: case 7U:
			if(!mugetsu_pattern_crosses(state, presentation)) return false;
			break;
		case 3U:
			if(!mugetsu_pattern_turning(state, presentation, false)) return false;
			break;
		case 6U:
			if(!mugetsu_pattern_ring(state, presentation)) return false;
			break;
		case extra_mode_teleport:
			if(boss.phase_frame > 16) {
				const auto centered = pc98::th0405::randring_next16_and(state.random, 3U);
				if(!centered) return false;
				if(*centered == 0U) {
					boss.mugetsu_pose = ExtraMugetsuPose::short_180bb;
				} else {
					boss.mugetsu_pose = ExtraMugetsuPose::medium_1812a;
					std::uint16_t pattern{};
					do {
						if(!extra_random_mod(state, 5U, pattern)) return false;
					} while(boss.phase_state == static_cast<std::uint8_t>(pattern));
					boss.phase_state = static_cast<std::uint8_t>(pattern);
					boss.mugetsu_gather_center = {
						static_cast<std::int16_t>(pattern * 1024U + 1024U), boss.pos.cur.y
					};
				}
				boss.phase_frame = 0;
				boss.mugetsu_phase2_mode = static_cast<std::uint8_t>(boss.mugetsu_phase2_mode + 1U);
				boss.mode = static_cast<std::uint8_t>(boss.mugetsu_phase2_mode & 7U);
			}
			break;
		default:
			return false;
		}
		if(boss.mugetsu_phase2_mode >= 32U && boss.mode != extra_mode_teleport &&
			boss.phase_frame > 24) {
			if(!mugetsu_fast_ring(state)) return false;
		}
		if(boss.mugetsu_phase2_mode < 36U) {
			if(!mugetsu_hittest(state, presentation)) break;
			if(!boss_score_bonus(state, presentation, 100U)) return false;
		}
		if(state.resources.bullet_clear_time < 20) state.resources.bullet_clear_time = 20;
		if(!mugetsu_phase_next(state, presentation, 0U, 0)) return false;
		boss.mugetsu_gather_center = {3072, boss.mugetsu_gather_center.y};
		break;
	case 3U:
		(void)mugetsu_hittest(state, presentation);
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.mugetsu_pose = ExtraMugetsuPose::long_1821e;
			boss.phase_frame = 0;
		}
		break;
	case 4U:
		(void)mugetsu_hittest(state, presentation);
		if(!mugetsu_pattern_turning(state, presentation, true)) return false;
		if(boss.phase_frame == 0) {
			boss_explode_small(state, presentation, 4U);
			++boss.phase;
			boss.phase_frame = 0;
			boss.sprite = extra_pat_mugetsu_flap_closed;
		}
		break;
	case 5U:
		(void)mugetsu_hittest(state, presentation);
		if(boss.phase_frame >= 128) {
			++boss.phase;
			boss.phase_frame = 0;
		}
		break;
	case 6U:
		if(!mugetsu_final_pattern(state)) return false;
		if(boss.phase_frame >= 3000 && !mugetsu_fast_ring(state)) return false;
		if(!mugetsu_hittest(state, presentation) && boss.phase_frame < 4000) break;
		boss_explode_small(state, presentation, 3U);
		++boss.phase;
		boss.phase_state = boss.phase_frame < 4000 ? 1U : 0U;
		boss.phase_frame = 0;
		boss.mode = 0U;
		state.palette_tone = 100U;
		state.palette_changed = true;
		break;
	case 7U:
		++boss.phase_frame;
		if(boss.phase_frame == 16) boss_explode_small(state, presentation, 4U);
		if(boss.phase_frame == 32) {
			boss_explode_big(state, presentation, 2U);
			boss.phase = extra_phase_explode_big;
			state.bullets.zap.set_active(boss.phase_state != 0U);
			if(boss.phase_state != 0U && !boss_score_bonus(state, presentation, 200U)) {
				return false;
			}
			boss.sprite = pat_enemy_kill;
			boss.phase_frame = 0;
			presentation.sound(12U);
			state.palette_changed = true;
			state.player_invincibility_time = boss_defeat_invincibility_frames;
		}
		break;
	case extra_phase_explode_big:
	case extra_phase_none:
		return extra_boss_defeat_update(state);
	default:
		return false;
	}
	return true;
}

[[nodiscard]] bool extra_boss_defeat_update(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.phase == extra_phase_explode_big) {
		if(boss.phase_frame < 12) {
			state.playfield_shake_x = (state.stage_frame & 1U) == 0U ? -4 : 4;
			state.playfield_shake_y = (state.stage_frame & 3U) <= 1U ? -4 : 4;
		}
		state.bullets.slowdown_factor = 2U;
		++boss.phase_frame;
		if((boss.phase_frame % 8) != 0) return true;
		++boss.sprite;
		if(boss.sprite <= pat_enemy_kill_last) return true;
		boss.phase = extra_phase_none;
		boss.phase_frame = 0;
		state.bombing_disabled = true;
		return true;
	}
	state.palette_tone = 60U;
	state.palette_changed = true;
	if(boss.statebytes[0U] == 0U) state.dialog_pending = true;
	else state.stage_clear_requested = true;
	return true;
}

[[nodiscard]] bool extra_gather_add_bullets(State &state) noexcept
{
	for(auto &gather : state.bullets.gathers) {
		if(gather.flag != bullet_free) continue;
		const auto &template_ = state.bullets.gather_template;
		gather.flag = bullet_alive;
		gather.bullet_template = state.bullets.template_;
		gather.center.cur = template_.center;
		gather.center.prev = template_.center;
		gather.center.velocity = template_.velocity;
		gather.radius_cur = template_.radius;
		gather.radius_prev = template_.radius;
		gather.ring_points = template_.ring_points;
		gather.angle_cur = 0U;
		gather.angle_delta = template_.angle_delta;
		gather.col = template_.col;
		gather.radius_delta = static_cast<std::int16_t>(template_.radius / 16);
		return true;
	}
	return true;
}

void gengetsu_gather_pair(State &state) noexcept
{
	state.bullets.gather_template.angle_delta = static_cast<std::uint8_t>(-2);
	gather_add_only(state);
	state.bullets.gather_template.angle_delta = 2U;
	gather_add_only(state);
}

void gengetsu_charge_gather(State &state) noexcept
{
	auto &boss = state.boss;
	auto &gather = state.bullets.gather_template;
	switch(boss.phase_frame) {
	case 48:
		gather.radius = 5120;
		gather.center = {word_sub(boss.pos.cur.x, 208), word_sub(boss.pos.cur.y, 768)};
		gather.ring_points = 16;
		gather.col = 15U;
		gengetsu_gather_pair(state);
		break;
	case 50:
		gather.col = 9U;
		gengetsu_gather_pair(state);
		break;
	case 52:
		gengetsu_gather_pair(state);
		break;
	case 64:
		circle_add_shrinking(state, gather.center);
		state.circles_color = 15U;
		break;
	default:
		break;
	}
}

[[nodiscard]] std::uint8_t gengetsu_charge_animate(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	gengetsu_charge_gather(state);
	if(boss.phase_frame < 8) return 0U;
	if(boss.phase_frame == 8) {
		boss.sprite = extra_pat_gengetsu_flap_a;
		return 0U;
	}
	if(boss.sprite < 32U) return 0U;
	if(boss.phase_frame < 80) {
		if(boss.phase_frame == 32) presentation.sound(8U);
		boss.sprite = (state.stage_frame & 1U) != 0U
			? extra_pat_gengetsu_flap_b : extra_pat_gengetsu_flap_a;
		return 1U;
	}
	if(boss.phase_frame == 80) {
		boss.sprite = extra_pat_gengetsu_fire;
		return 2U;
	}
	if(boss.phase_frame < 144) return 3U;
	boss.sprite = extra_pat_gengetsu_fire;
	return 4U;
}

[[nodiscard]] bool gengetsu_teleport(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 1) {
		boss.pos.velocity.x = word_sub(boss.gengetsu_wave_target_x, boss.pos.cur.x) / 64;
	}
	pc98::th0405::update_motion(boss.pos);
	if(boss.phase_frame <= 32) {
		boss.gengetsu_wave_amplitude = byte_add(boss.gengetsu_wave_amplitude, 2);
	} else {
		boss.gengetsu_wave_amplitude = byte_add(boss.gengetsu_wave_amplitude, -2);
	}
	if(boss.phase_frame != 64) return false;
	boss.gengetsu_wave_amplitude = 0U;
	return true;
}

[[nodiscard]] bool gengetsu_return_center(State &state) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 1) boss.pos.velocity.x = boss.pos.cur.x < 3072 ? 32 : -32;
	if((boss.pos.velocity.x < 0 && boss.pos.cur.x >= 3088) ||
		(boss.pos.velocity.x > 0 && boss.pos.cur.x <= 3072)) {
		pc98::th0405::update_motion(boss.pos);
	}
	if(boss.phase_frame <= 64) ++boss.gengetsu_wave_amplitude;
	else --boss.gengetsu_wave_amplitude;
	if(boss.phase_frame != 128) return false;
	boss.pos.cur.x = 3072;
	boss.gengetsu_wave_amplitude = 0U;
	return true;
}

[[nodiscard]] bool gengetsu_pattern_hearts(State &state, Presentation &presentation) noexcept
{
	auto &bullet = state.bullets.template_;
	switch(gengetsu_charge_animate(state, presentation)) {
	case 2U: {
		std::uint8_t angle{};
		if(!extra_random_byte(state, angle)) return false;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_backwards);
		bullet.patnum = extra_pat_bullet16_heart_red;
		bullet.speed = 70U;
		bullet.group = BulletGroup::ring;
		bullet.count = 90U;
		bullet.angle = angle;
		if(!bullets_add(state)) return false;
		presentation.sound(9U);
		break;
	}
	case 4U:
		state.boss.phase_frame = 0;
		state.boss.mode = extra_mode_teleport;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool gengetsu_pattern_spreads(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	switch(gengetsu_charge_animate(state, presentation)) {
	case 2U: {
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, bullet.origin.y),
			word_sub(state.player_pos.cur.x, bullet.origin.x)
		);
		if(!angle) return false;
		bullet.group = BulletGroup::spread;
		bullet.delta = 8U;
		boss.statebytes[15U] = byte_add(*angle, -64);
		break;
	}
	case 3U:
		if((state.stage_frame & 1U) == 0U) {
			const auto speed = pc98::th0405::randring_next16_and(state.random, 63U);
			const auto count = pc98::th0405::randring_next16_and(state.random, 3U);
			if(!speed || !count) return false;
			bullet.speed = static_cast<std::uint8_t>(*speed + 8U);
			bullet.count = static_cast<std::uint8_t>(*count + 5U);
			bullet.angle = boss.statebytes[15U];
			if(!bullets_add(state)) return false;
			bullet.angle = static_cast<std::uint8_t>(128U - boss.statebytes[15U]);
			if(!bullets_add(state)) return false;
			boss.statebytes[15U] = byte_add(boss.statebytes[15U], 7);
			presentation.sound(9U);
		}
		break;
	case 4U:
		boss.phase_frame = 0;
		boss.mode = extra_mode_teleport;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool gengetsu_pattern_bouncing_crosses(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	switch(gengetsu_charge_animate(state, presentation)) {
	case 2U:
		bullet.special_motion = BulletSpecialMotion::bounce_left_right_top_bottom;
		state.bullets.special.value = 4U;
		break;
	case 3U:
		if((state.stage_frame & 1U) != 0U) {
			const auto speed_a = pc98::th0405::randring_next16_and(state.random, 63U);
			const auto x = pc98::th0405::randring_next16_mod(state.random, 1024U);
			const auto y = pc98::th0405::randring_next16_mod(state.random, 512U);
			const auto speed_b = pc98::th0405::randring_next16_and(state.random, 63U);
			if(!speed_a || !x || !y || !speed_b) return false;
			bullet.group = BulletGroup::single;
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			bullet.patnum = extra_pat_bullet16_cross_yellow;
			bullet.speed = static_cast<std::uint8_t>(*speed_a + 42U);
			bullet.origin = {word_add(word_sub(boss.pos.cur.x, 512), static_cast<std::int16_t>(*x)),
				word_add(word_sub(boss.pos.cur.y, 512), static_cast<std::int16_t>(*y))};
			bullet.angle = static_cast<std::uint8_t>(-32);
			if(!bullets_add(state, true, false)) return false;
			bullet.speed = static_cast<std::uint8_t>(*speed_b + 42U);
			bullet.angle = static_cast<std::uint8_t>(-96);
			if(!bullets_add(state, true, false)) return false;
			presentation.sound(9U);
			if((state.stage_frame & 15U) == 1U) {
				bullet.patnum = extra_pat_bullet16_directional_blue;
				bullet.group = BulletGroup::ring_aimed;
				bullet.count = 32U;
				bullet.speed = 80U;
				if(!bullets_add(state)) return false;
			}
		}
		break;
	case 4U:
		boss.phase_frame = 0;
		boss.mode = extra_mode_teleport;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool gengetsu_pattern_turning_and_gather(
	State &state, Presentation &presentation
) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	auto &gather = state.bullets.gather_template;
	switch(gengetsu_charge_animate(state, presentation)) {
	case 2U:
		state.bullets.special.value = 1U;
		break;
	case 3U:
		if((state.stage_frame & 1U) != 0U) {
			const auto dead_speed = pc98::th0405::randring_next16_and(state.random, 63U);
			const auto x = pc98::th0405::randring_next16_mod(state.random, 1024U);
			const auto y = pc98::th0405::randring_next16_mod(state.random, 512U);
			const auto speed = pc98::th0405::randring_next16_and(state.random, 63U);
			if(!dead_speed || !x || !y || !speed) return false;
			bullet.special_motion = BulletSpecialMotion::decelerate_then_turn;
			bullet.group = BulletGroup::single;
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			bullet.patnum = extra_pat_bullet16_small_ball_yellow;
			bullet.origin = {word_add(word_sub(boss.pos.cur.x, 512), static_cast<std::int16_t>(*x)),
				word_add(word_sub(boss.pos.cur.y, 416), static_cast<std::int16_t>(*y))};
			bullet.speed = static_cast<std::uint8_t>(*speed + 16U);
			bullet.angle = 128U;
			state.bullets.template_special_angle = static_cast<std::uint8_t>(-64);
			if(!bullets_add(state, true, false)) return false;
			bullet.angle = 0U;
			state.bullets.template_special_angle = 64U;
			if(!bullets_add(state, true, false)) return false;
			presentation.sound(9U);
		}
		if((state.stage_frame & 3U) == 0U) {
			std::uint16_t x{};
			if(!extra_random_mod(state, 5120U, x)) return false;
			gather.ring_points = 8;
			gather.radius = 1024;
			gather.col = 14U;
			gather.center = {word_add(512, static_cast<std::int16_t>(x)), bullet.origin.y};
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
			bullet.group = BulletGroup::ring_aimed;
			bullet.count = 16U;
			bullet.speed = 64U;
			if(!extra_gather_add_bullets(state)) return false;
		}
		break;
	case 4U:
		boss.phase_frame = 0;
		boss.mode = extra_mode_teleport;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool gengetsu_pattern_cloud_fans(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	switch(gengetsu_charge_animate(state, presentation)) {
	case 0U:
		bullet.patnum = extra_pat_bullet16_heart_red;
		bullet.group = BulletGroup::spread_aimed;
		bullet.special_motion = BulletSpecialMotion::none;
		bullet.delta = 16U;
		bullet.angle = 0U;
		bullet.count = 2U;
		break;
	case 1U:
		if((boss.phase_frame & 15) == 8) {
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
			bullet.speed = 16U;
			for(std::uint8_t i{}; i < 4U; ++i) {
				if(!bullets_add(state, true, true)) return false;
				bullet.speed = byte_add(bullet.speed, 20);
			}
			presentation.sound(15U);
			bullet.count = byte_add(bullet.count, 2);
			bullet.delta = byte_add(bullet.delta, -2);
		}
		break;
	case 2U:
		bullet.group = BulletGroup::stack_aimed;
		bullet.count = 8U;
		bullet.delta = 10U;
		bullet.speed = 32U;
		bullet.patnum = extra_pat_bullet16_ball_blue;
		bullet.angle = 0U;
		break;
	case 3U:
		if((boss.phase_frame & 3) == 0) {
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards);
			if(!bullets_add(state)) return false;
			presentation.sound(15U);
		}
		break;
	case 4U:
		boss.phase_frame = 0;
		boss.mode = extra_mode_teleport;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool gengetsu_pattern_growing_spread(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	switch(gengetsu_charge_animate(state, presentation)) {
	case 2U: {
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, bullet.origin.y),
			word_sub(state.player_pos.cur.x, bullet.origin.x)
		);
		if(!angle) return false;
		bullet.speed = 80U;
		bullet.group = BulletGroup::spread;
		bullet.count = 1U;
		bullet.delta = 6U;
		bullet.angle = *angle;
		break;
	}
	case 3U:
		if((state.stage_frame & 3U) == 0U) {
			if(!bullets_add(state)) return false;
			++bullet.count;
			presentation.sound(9U);
		}
		break;
	case 4U:
		boss.phase_frame = 0;
		boss.mode = extra_mode_teleport;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool gengetsu_pattern_mirrored_fans(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	switch(gengetsu_charge_animate(state, presentation)) {
	case 2U:
		bullet.patnum = extra_pat_bullet16_directional_blue;
		bullet.group = BulletGroup::spread;
		bullet.count = 5U;
		bullet.delta = 11U;
		bullet.angle = 0U;
		bullet.speed = 90U;
		break;
	case 3U:
		if((state.stage_frame & 1U) == 0U) {
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			if(!bullets_add(state)) return false;
			bullet.angle = static_cast<std::uint8_t>(128U - bullet.angle);
			if(!bullets_add(state)) return false;
			bullet.angle = static_cast<std::uint8_t>(120U - bullet.angle);
			presentation.sound(3U);
		}
		break;
	case 4U:
		boss.phase_frame = 0;
		boss.mode = extra_mode_teleport;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool gengetsu_pattern_columns(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	if(boss.phase_frame == 1) {
		const auto aligned = pc98::th0405::randring_next16_and(state.random, 1U);
		if(!aligned) return false;
		for(std::size_t i{}; i < boss.gengetsu_spawncolumns.size(); ++i) {
			if(*aligned == 0U) {
				boss.gengetsu_spawncolumns[i] = {
					static_cast<std::int16_t>(i * 384U + 192U), 0
				};
			} else {
				std::uint16_t wobble{};
				if(!extra_random_mod(state, 192U, wobble)) return false;
				boss.gengetsu_spawncolumns[i] = {
					static_cast<std::int16_t>(i * 384U + 96U + wobble), 0
				};
			}
		}
	}
	switch(gengetsu_charge_animate(state, presentation)) {
	case 1U: {
		const auto angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, bullet.origin.y),
			word_sub(state.player_pos.cur.x, bullet.origin.x)
		);
		if(!angle) return false;
		bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
		bullet.group = BulletGroup::spread;
		bullet.count = 5U;
		bullet.delta = 24U;
		bullet.angle = 192U;
		bullet.patnum = extra_pat_bullet16_white;
		bullet.speed = 127U;
		if(!bullet_template_tune(state) || !bullets_add(state)) return false;
		presentation.sound(3U);
		boss.statebytes[15U] = byte_add(*angle, -48);
		break;
	}
	case 2U:
		state.thicklasers.template_.origin = {bullet.origin.x, boss.pos.cur.y};
		state.thicklasers.template_.radius_max = 64;
		state.thicklasers.template_.radius_speed = 6;
		state.thicklasers.template_.line_frames = 32;
		state.thicklasers.template_.static_frames = 48;
		state.thicklasers.template_.col_outline = 8U;
		thicklaser_add(state, presentation);
		break;
	case 3U:
		if((state.stage_frame & 3U) == 0U) {
			if((state.stage_frame & 7U) == 0U) {
				bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
				bullet.angle = boss.statebytes[15U];
				bullet.group = BulletGroup::stack;
				bullet.count = 12U;
				bullet.speed = 32U;
				bullet.delta = 8U;
				boss.statebytes[15U] = byte_add(boss.statebytes[15U], 12);
				if(!bullets_add(state)) return false;
			}
			bullet.speed = 128U;
			bullet.angle = 64U;
			bullet.patnum = extra_pat_bullet16_white;
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			bullet.origin.y = 0;
			bullet.group = BulletGroup::single;
			for(const auto &column : boss.gengetsu_spawncolumns) {
				bullet.origin.x = column.x;
				if(!bullets_add(state, false, true)) return false;
			}
			presentation.sound(3U);
		}
		break;
	case 4U:
		boss.phase_frame = 0;
		boss.mode = extra_mode_teleport;
		break;
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool gengetsu_phase7_pattern(State &state, Presentation &presentation) noexcept
{
	if((state.stage_frame & 7U) != 0U) return true;
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	std::uint8_t angle{};
	const auto x = pc98::th0405::randring_next16_mod(state.random, 1024U);
	const auto y = pc98::th0405::randring_next16_mod(state.random, 512U);
	const auto speed = pc98::th0405::randring_next16_and(state.random, 63U);
	if(!extra_random_byte(state, angle) || !x || !y || !speed) return false;
	boss.sprite = extra_pat_gengetsu_tipping;
	bullet.angle = angle;
	bullet.origin = {word_add(word_sub(boss.pos.cur.x, 512), static_cast<std::int16_t>(*x)),
		word_add(word_sub(boss.pos.cur.y, 416), static_cast<std::int16_t>(*y))};
	bullet.group = BulletGroup::ring;
	bullet.count = 16U;
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
	bullet.patnum = extra_pat_bullet16_directional_blue;
	bullet.speed = static_cast<std::uint8_t>(*speed + 16U);
	if(!bullets_add(state)) return false;
	presentation.sound(3U);
	return true;
}

[[nodiscard]] bool gengetsu_phase8_early(State &state, Presentation &presentation) noexcept
{
	if((state.stage_frame & 3U) != 0U) {
		state.boss.sprite = extra_pat_gengetsu_flap_a;
		return true;
	}
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	std::uint16_t y{};
	if(!extra_random_mod(state, 512U, y)) return false;
	boss.sprite = extra_pat_gengetsu_flap_b;
	bullet.group = BulletGroup::spread;
	bullet.delta = 9U;
	bullet.count = 8U;
	bullet.speed = 56U;
	bullet.angle = static_cast<std::uint8_t>(state.stage_frame * 2U);
	if((state.stage_frame % 512U) >= 256U) {
		bullet.angle = static_cast<std::uint8_t>(-static_cast<std::int16_t>(bullet.angle));
	}
	if(!bullets_add(state)) return false;
	bullet.angle = byte_add(bullet.angle, 128);
	if(!bullets_add(state)) return false;
	bullet.speed = 32U;
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_backwards);
	bullet.delta = 1U;
	bullet.count = 3U;
	bullet.patnum = extra_pat_bullet16_ball_blue;
	bullet.origin.y = word_add(word_sub(boss.pos.cur.y, 416), static_cast<std::int16_t>(y));
	bullet.origin.x = word_add(boss.pos.cur.x, static_cast<std::int16_t>(boss.statebytes[15U] * 16U));
	bullet.angle = boss.statebytes[14U];
	if(!bullets_add(state)) return false;
	bullet.origin.x = word_sub(boss.pos.cur.x, static_cast<std::int16_t>(boss.statebytes[15U] * 16U));
	bullet.angle = static_cast<std::uint8_t>(-static_cast<std::int16_t>(boss.statebytes[14U]));
	if(!bullets_add(state)) return false;
	boss.statebytes[15U] = byte_add(boss.statebytes[15U], 16);
	if(boss.statebytes[15U] > 176U) boss.statebytes[15U] = 16U;
	boss.statebytes[14U] = byte_add(boss.statebytes[14U], 11);
	presentation.sound(3U);
	return true;
}

[[nodiscard]] bool gengetsu_phase8_late(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	boss.sprite = (state.stage_frame & 1U) != 0U
		? extra_pat_gengetsu_flap_b : extra_pat_gengetsu_flap_a;
	if((state.stage_frame & 3U) != 0U) return true;
	const auto spawn_type = pc98::th0405::randring_next16_and(state.random, 1U);
	const auto x = pc98::th0405::randring_next16_mod(state.random, 1024U);
	const auto y = pc98::th0405::randring_next16_mod(state.random, 512U);
	std::uint8_t angle{};
	if(!spawn_type || !x || !y || !extra_random_byte(state, angle)) return false;
	bullet.spawn_type = static_cast<std::uint8_t>(*spawn_type);
	bullet.origin = {word_add(word_sub(boss.pos.cur.x, 512), static_cast<std::int16_t>(*x)),
		word_add(word_sub(boss.pos.cur.y, 416), static_cast<std::int16_t>(*y))};
	bullet.group = BulletGroup::ring;
	bullet.count = 32U;
	bullet.angle = angle;
	bullet.speed = 100U;
	if(!bullets_add(state)) return false;
	presentation.sound(3U);
	return true;
}

[[nodiscard]] bool gengetsu_teleport_target(
	State &state, bool increment_before_mode
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_frame == 1) {
		if((boss.phase_state & 1U) != 0U) {
			boss.gengetsu_wave_target_x = state.player_pos.cur.x;
			if(boss.gengetsu_wave_target_x < 512) boss.gengetsu_wave_target_x = 512;
			if(boss.gengetsu_wave_target_x > 5632) boss.gengetsu_wave_target_x = 5632;
		} else {
			std::uint16_t x{};
			if(!extra_random_mod(state, 4096U, x)) return false;
			boss.gengetsu_wave_target_x = static_cast<std::int16_t>(x + 1024U);
		}
	}
	if(!gengetsu_teleport(state)) return true;
	if(increment_before_mode) {
		++boss.phase_state;
		boss.mode = static_cast<std::uint8_t>(boss.phase_state & 1U);
	} else {
		boss.mode = static_cast<std::uint8_t>(boss.phase_state & 1U);
		++boss.phase_state;
	}
	boss.phase_frame = 0;
	return true;
}

[[nodiscard]] bool gengetsu_finish_pattern_phase(
	State &state, Presentation &presentation, std::uint8_t explosion,
	std::int16_t next_end_hp
) noexcept
{
	auto &boss = state.boss;
	if(boss.phase_state >= 16U && boss.mode != extra_mode_teleport) {
		if((state.stage_frame & 7U) == 0U) {
			auto &bullet = state.bullets.template_;
			std::uint8_t angle{};
			if(!extra_random_byte(state, angle)) return false;
			bullet.group = BulletGroup::ring;
			bullet.count = 32U;
			bullet.patnum = extra_pat_bullet16_directional_blue;
			bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
			bullet.angle = angle;
			bullet.speed = 112U;
			if(!bullets_add(state)) return false;
		}
	}
	if(boss.phase_state < 32U) {
		if(!gengetsu_hittest(state, presentation)) return true;
		if(!boss_score_bonus(state, presentation, 100U)) return false;
	}
	if(!boss_phase_next(state, presentation, explosion, next_end_hp)) return false;
	boss.mode = extra_mode_teleport;
	boss.gengetsu_wave_target_x = 3072;
	return true;
}

[[nodiscard]] bool gengetsu_update(State &state, Presentation &presentation) noexcept
{
	auto &boss = state.boss;
	auto &bullet = state.bullets.template_;
	extra_bomb_immunity_update(state);
	bullet.origin = {word_sub(boss.pos.cur.x, 208), word_sub(boss.pos.cur.y, 768)};
	bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
	switch(boss.phase) {
	case 0U:
		(void)gengetsu_hittest(state, presentation);
		boss.hp = extra_gengetsu_hp;
		if(boss.phase_frame > 128) {
			boss.phase_end_hp = 14700;
			++boss.phase;
			boss.phase_frame = 0;
			presentation.sound(13U);
		}
		break;
	case 1U:
		(void)gengetsu_hittest(state, presentation);
		if(boss.phase_frame >= 64) {
			++boss.phase;
			boss.mode = 0U;
			boss.phase_state = 0U;
			boss.phase_frame = 0;
			boss.pos.velocity.x = 0;
		}
		break;
	case 2U:
		switch(boss.mode) {
		case 0U: if(!gengetsu_pattern_hearts(state, presentation)) return false; break;
		case 1U: if(!gengetsu_pattern_spreads(state, presentation)) return false; break;
		case extra_mode_teleport:
			if(!gengetsu_teleport_target(state, true)) return false;
			break;
		default: return false;
		}
		if(!gengetsu_finish_pattern_phase(state, presentation, 1U, 12700)) return false;
		break;
	case 3U:
		switch(boss.mode) {
		case 0U: if(!gengetsu_pattern_bouncing_crosses(state, presentation)) return false; break;
		case 1U: if(!gengetsu_pattern_turning_and_gather(state, presentation)) return false; break;
		case extra_mode_teleport:
			if(!gengetsu_teleport_target(state, false)) return false;
			break;
		default: return false;
		}
		if(!gengetsu_finish_pattern_phase(state, presentation, 3U, 8900)) return false;
		break;
	case 4U:
		switch(boss.mode) {
		case 0U: if(!gengetsu_pattern_cloud_fans(state, presentation)) return false; break;
		case 1U: if(!gengetsu_pattern_growing_spread(state, presentation)) return false; break;
		case extra_mode_teleport:
			if(!gengetsu_teleport_target(state, false)) return false;
			break;
		default: return false;
		}
		if(!gengetsu_finish_pattern_phase(state, presentation, 3U, 5500)) return false;
		break;
	case 5U:
		switch(boss.mode) {
		case 0U: if(!gengetsu_pattern_mirrored_fans(state, presentation)) return false; break;
		case 1U: if(!gengetsu_pattern_columns(state, presentation)) return false; break;
		case extra_mode_teleport:
			if(!gengetsu_teleport_target(state, false)) return false;
			break;
		default: return false;
		}
		if(!gengetsu_finish_pattern_phase(state, presentation, 3U, 3000)) return false;
		break;
	case 6U:
		(void)gengetsu_hittest(state, presentation);
		if(gengetsu_return_center(state)) {
			++boss.phase;
			boss.phase_frame = 32;
		}
		break;
	case 7U:
		if(!gengetsu_phase7_pattern(state, presentation)) return false;
		if(boss.phase_frame < 1500) {
			if(!gengetsu_hittest(state, presentation)) break;
			if(!boss_score_bonus(state, presentation, 100U)) return false;
		}
		if(!boss_phase_next(state, presentation, 4U, 0)) return false;
		boss.mode = extra_mode_teleport;
		boss.gengetsu_wave_target_x = 3072;
		boss.statebytes[15U] = 16U;
		break;
	case 8U:
		if(boss.phase_frame <= 3000) {
			if(!gengetsu_phase8_early(state, presentation)) return false;
		} else {
			if(!gengetsu_phase8_late(state, presentation)) return false;
		}
		if(!gengetsu_hittest(state, presentation) && boss.phase_frame < 5000) break;
		boss_explode_small(state, presentation, 3U);
		++boss.phase;
		boss.phase_state = boss.phase_frame < 5000 ? 1U : 0U;
		boss.phase_frame = 0;
		boss.mode = 0U;
		state.palette_tone = 100U;
		state.palette_changed = true;
		break;
	case 9U:
		++boss.phase_frame;
		if(boss.phase_frame == 16) boss_explode_small(state, presentation, 4U);
		if(boss.phase_frame == 32) {
			boss_explode_big(state, presentation, 3U);
			boss.phase = extra_phase_explode_big;
			state.bullets.zap.set_active(boss.phase_state != 0U);
			if(boss.phase_state != 0U && !boss_score_bonus(state, presentation, 200U)) {
				return false;
			}
			boss.sprite = pat_enemy_kill;
			boss.phase_frame = 0;
			presentation.sound(12U);
			state.palette_changed = true;
			state.player_invincibility_time = boss_defeat_invincibility_frames;
		}
		break;
	case extra_phase_explode_big:
	case extra_phase_none:
		return extra_boss_defeat_update(state);
	default:
		return false;
	}
	thicklasers_update_and_hittest(state, presentation);
	return true;
}

void extra_mugetsu_setup(State &state) noexcept
{
	auto &boss = state.boss;
	boss.phase = 0U;
	boss.mode = 0U;
	boss.phase_state = 0U;
	boss.phase_frame = 0;
	boss.pos.cur = {3072, 1280};
	boss.pos.prev = boss.pos.cur;
	boss.pos.velocity = {};
	boss.damage_this_frame = 0U;
	boss.phase_timed_out = true;
	boss.sprite = extra_pat_mugetsu_idle;
	boss.hitbox_radius = {384, 768};
	boss.statebytes[0U] = 0U;
}

void extra_gengetsu_setup(State &state) noexcept
{
	auto &boss = state.boss;
	boss.phase = 0U;
	boss.mode = 0U;
	boss.phase_state = 0U;
	boss.phase_frame = 0;
	boss.pos.cur = {3072, 1792};
	boss.pos.prev = boss.pos.cur;
	boss.pos.velocity = {};
	boss.damage_this_frame = 0U;
	boss.phase_timed_out = true;
	boss.sprite = extra_pat_gengetsu_tipping;
	boss.hitbox_radius = {384, 768};
	boss.gengetsu_wave_amplitude = 0U;
	boss.gengetsu_wave_target_x = 3072;
}

} // namespace

void extra_fights_setup(State &state) noexcept
{
	extra_midboss_setup(state);
	state.boss_active = false;
	state.boss_explosions.small[0].alive = false;
	state.boss_explosions.small[1].alive = false;
	extra_mugetsu_setup(state);
}

void extra_boss_activate(State &state) noexcept
{
	if(state.boss.phase == extra_phase_none && state.boss.statebytes[0U] == 0U) {
		state.boss.statebytes[0U] = 1U;
		extra_gengetsu_setup(state);
		state.boss_active = true;
		return;
	}
	state.boss_active = true;
}

bool extra_fights_update(State &state, Presentation &presentation) noexcept
{
	if(state.midboss_active && !extra_midboss_update(state, presentation)) return false;
	if(!state.boss_active) return true;
	return state.boss.statebytes[0U] == 0U
		? mugetsu_update(state, presentation) : gengetsu_update(state, presentation);
}

void extra_fights_render_tail(State &state) noexcept
{
	if(state.boss_active && state.boss.sprite != 0U &&
		state.boss.phase < extra_phase_explode_big &&
		state.boss.damage_this_frame != 0U &&
		(state.boss.statebytes[0U] == 0U || state.boss.gengetsu_wave_amplitude == 0U)) {
		if(state.boss.statebytes[0U] == 0U) ++state.boss.mugetsu_damage_frames;
		else ++state.boss.gengetsu_damage_frames;
		state.boss.damage_this_frame = 0U;
	}
	if(!state.boss_active) return;
	for(auto &explosion : state.boss_explosions.small) {
		if(explosion.alive) explosion_update(explosion);
	}
	auto &big = state.boss_explosions.big;
	if(big.alive) {
		explosion_update(big);
		++state.boss_explosions.big_frame;
		const auto frame = state.boss_explosions.big_frame;
		if(frame < 8 && (frame & 1) != 0) {
			state.palette_tone = 150U;
		} else if(frame >= 8 && frame < 16) {
			state.palette_tone = static_cast<std::uint16_t>(196 - frame * 6);
		} else {
			state.palette_tone = 100U;
		}
		state.palette_changed = true;
	} else {
		state.boss_explosions.big_frame = 0U;
	}
}

} // namespace th04::core
