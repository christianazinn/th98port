#include "th04/core/boss.hpp"

#include "th04/core/bullet.hpp"
#include "th04/core/item.hpp"
#include "th04/core/shot_collision.hpp"
#include "th04/core/shot_reimu_a.hpp"
#include "th04/core/state.hpp"

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
	state.score.score_delta += static_cast<std::uint32_t>(units) * value;
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

[[nodiscard]] bool midboss_score_bonus(
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

void stage1_circles_update(State &state, Presentation &presentation) noexcept
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

} // namespace th04::core
