#include "th04/core/b6ent.hpp"

#include "th04/core/bullet.hpp"
#include "th04/core/item.hpp"
#include "th04/core/shot_collision.hpp"
#include "th04/core/state.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_sparks.hpp"

#include <bit>
#include <cstdint>

namespace th04::core {
namespace {

constexpr std::int16_t subpixel_factor = 16;
constexpr std::int16_t chasecross_half_size = 256;
constexpr std::int16_t chasecross_hitbox_radius = 192;
constexpr std::int16_t chasecross_right_edge = 6400;
constexpr std::int16_t chasecross_bottom_edge = 6144;
constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_top = 16;
constexpr std::uint8_t pat_bullet16_small_ball_red = 60U;
constexpr std::uint8_t chasecross_kill_anim_end = 48U;

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

[[nodiscard]] constexpr std::int16_t word_mul(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(left) * right
	));
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

[[nodiscard]] constexpr bool chasecross_clipped(
	Yuuka6CustomPoint center
) noexcept
{
	return center.x <= -chasecross_half_size ||
		center.x >= chasecross_right_edge ||
		center.y >= chasecross_bottom_edge ||
		center.y <= -chasecross_half_size;
}

[[nodiscard]] constexpr bool player_in_chasecross_box(
	const State &state, Yuuka6CustomPoint center
) noexcept
{
	const auto left = static_cast<std::uint16_t>(word_sub(center.x,
		chasecross_hitbox_radius));
	const auto top = static_cast<std::uint16_t>(word_sub(center.y,
		chasecross_hitbox_radius));
	return static_cast<std::uint16_t>(static_cast<std::uint16_t>(state.player_pos.cur.x) -
		left) < static_cast<std::uint16_t>(chasecross_hitbox_radius * 2) &&
		static_cast<std::uint16_t>(static_cast<std::uint16_t>(state.player_pos.cur.y) -
		top) < static_cast<std::uint16_t>(chasecross_hitbox_radius * 2);
}

[[nodiscard]] bool chasecross_shots_hit(
	State &state, Yuuka6CustomPoint center, std::uint16_t &damage
) noexcept
{
	state.shot_collision.center = center;
	state.shot_collision.bombing = state.bombing;
	state.shot_collision.against_boss = false;
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

constexpr void safetycircle_alias_store_word(
	std::array<std::int8_t, 4U> &bytes, std::size_t word_index,
	std::uint16_t value
) noexcept
{
	const auto offset = word_index * 2U;
	bytes[offset] = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(value));
	bytes[offset + 1U] = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
		value >> 8U
	));
}

// The source's scan writes a chasecross_t into the physical final custom slot.
// This preserves every non-written byte of that safetycircle_t alias.
void safetycircle_alias_add_cross(
	Yuuka6SafetyCircle &circle, const BossState &boss, std::uint8_t angle,
	std::uint8_t speed
) noexcept
{
	circle.flag = Yuuka6SafetyCircleFlag::grow;
	circle.unused_1 = std::bit_cast<std::int8_t>(angle);
	circle.center = {boss.pos.cur.x, boss.pos.cur.y};
	circle.shrink_frame = 0U;
	safetycircle_alias_store_word(circle.unused_3, 0U, 100U);
	safetycircle_alias_store_word(circle.unused_3, 1U, 0U);
	circle.color_ring = speed;
}

[[nodiscard]] constexpr Yuuka6CustomPoint safetycircle_pellet_origin(
	const Yuuka6SafetyCircle &circle, std::int16_t radius, std::uint8_t angle
) noexcept
{
	const auto offset = pc98::masterlib::vector2(angle, radius);
	const auto x = word_sub(word_add(circle.center.x, offset.x), 32);
	const auto y = word_sub(word_add(circle.center.y, offset.y), 16);
	return {word_mul(x, subpixel_factor), word_mul(y, subpixel_factor)};
}

} // namespace

void yuuka6_chasecrosses_add(
	State &state, std::uint8_t angle, std::uint8_t speed
) noexcept
{
	for(auto &cross : state.yuuka6_customs.chasecrosses) {
		if(cross.flag != Yuuka6ChasecrossFlag::free) continue;
		cross.flag = Yuuka6ChasecrossFlag::alive;
		cross.damage_this_frame = 0;
		cross.age = 0U;
		cross.angle = angle;
		cross.speed = speed;
		cross.hp = 100;
		cross.center = state.boss.pos.cur;
		return;
	}
	if(state.yuuka6_customs.safetycircle.flag == Yuuka6SafetyCircleFlag::free) {
		safetycircle_alias_add_cross(state.yuuka6_customs.safetycircle, state.boss,
			angle, speed);
	}
}

void yuuka6_safetycircle_open(State &state, Presentation &presentation) noexcept
{
	auto &circle = state.yuuka6_customs.safetycircle;
	circle.flag = Yuuka6SafetyCircleFlag::grow;
	circle.shrink_frame = 0U;
	circle.color_ring = 8U;
	circle.center = {
		word_add(sar4(state.player_pos.cur.x), playfield_left),
		word_add(sar4(state.player_pos.cur.y), playfield_top),
	};
	circle.radius_filled = 8;
	circle.radius_ring_distance = 80;
	presentation.sound(8U);
}

bool yuuka6_customs_update(State &state, Presentation &presentation) noexcept
{
	// Source writes the shared radius before checking whether any cross is live.
	state.shot_collision.radius = {
		chasecross_hitbox_radius, chasecross_hitbox_radius,
	};
	for(auto &cross : state.yuuka6_customs.chasecrosses) {
		if(cross.flag != Yuuka6ChasecrossFlag::alive) continue;
		const auto velocity = pc98::masterlib::vector2(cross.angle,
			static_cast<std::int16_t>(cross.speed));
		cross.velocity = {velocity.x, velocity.y};
		cross.center.x = word_add(cross.center.x, cross.velocity.x);
		cross.center.y = word_add(cross.center.y, cross.velocity.y);
		if(chasecross_clipped(cross.center)) cross.flag = Yuuka6ChasecrossFlag::free;
		if(player_in_chasecross_box(state, cross.center)) state.player_is_hit = true;
		if(cross.age < 56U) {
			const auto aimed = pc98::masterlib::iatan2_source(
				word_sub(state.player_pos.cur.y, cross.center.y),
				word_sub(state.player_pos.cur.x, cross.center.x)
			);
			if(!aimed) return false;
			const auto delta = byte_add(*aimed, -static_cast<std::int16_t>(cross.angle));
			if(delta < 0x80U) {
				cross.angle = byte_add(cross.angle, 1);
			} else if(delta >= 0x80U) {
				cross.angle = byte_add(cross.angle, -1);
			}
		}
		std::uint16_t damage{};
		if(!chasecross_shots_hit(state, cross.center, damage)) return false;
		cross.damage_this_frame = std::bit_cast<std::int16_t>(damage);
		if(cross.damage_this_frame != 0) presentation.sound(4U);
		cross.hp = word_sub(cross.hp, cross.damage_this_frame);
		if(cross.hp <= 0) {
			presentation.sound(3U);
			state.score.score_delta += 3000U;
			cross.flag = Yuuka6ChasecrossFlag::kill_anim;
			cross.age = 0U;
			if(!pc98::th0405::sparks_add_random(
				pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
				state.random, cross.center, 64, 8U
			)) return false;
			items_add(state, presentation, cross.center, ItemType::big_power);
		}
		++cross.age;
	}

	auto &circle = state.yuuka6_customs.safetycircle;
	switch(circle.flag) {
	case Yuuka6SafetyCircleFlag::grow:
		if(circle.radius_filled <= 128) {
			circle.radius_filled = word_add(circle.radius_filled, 8);
		} else {
			circle.flag = Yuuka6SafetyCircleFlag::shrink;
		}
		break;

	case Yuuka6SafetyCircleFlag::shrink:
		if(circle.shrink_frame < 8U) {
			circle.radius_ring_distance = word_sub(circle.radius_ring_distance, 8);
		} else if(circle.shrink_frame == 8U) {
			circle.color_ring = 9U;
		} else if(circle.shrink_frame < 16U) {
			circle.radius_ring_distance = word_sub(circle.radius_ring_distance, 2);
		} else if(circle.shrink_frame < 160U) {
			if((circle.shrink_frame & 0x1FU) < 16U) {
				circle.radius_ring_distance = word_add(circle.radius_ring_distance, 1);
			} else {
				circle.radius_ring_distance = word_sub(circle.radius_ring_distance, 1);
			}
			circle.color_ring = (state.stage_frame & 1U) != 0U ? 15U : 9U;
			if(circle.shrink_frame <= 104U) {
				circle.radius_filled = word_sub(circle.radius_filled, 1);
			}
			if((circle.shrink_frame & 0x0FU) == 0U) {
				auto &bullet = state.bullets.template_;
				auto angle_offset = static_cast<std::uint8_t>(-64);
				if((circle.shrink_frame & 0x1FU) == 0U) {
					bullet.origin = {
						state.boss.pos.cur.x,
						word_add(state.boss.pos.cur.y, 512),
					};
					bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::bullet16);
					bullet.patnum = pat_bullet16_small_ball_red;
					bullet.group = BulletGroup::ring_aimed;
					bullet.count = 32U;
					bullet.speed = 16U;
					bullet.angle = 0U;
					if(!bullet_template_tune(state) || !bullets_add(state)) return false;
					angle_offset = 64U;
				}
				bullet.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
				bullet.delta = 12U;
				auto angle = static_cast<std::uint8_t>(state.stage_frame / 5U);
				const auto radius = word_add(circle.radius_filled, 4);
				for(std::uint8_t i = 0U; i < 8U; ++i) {
					bullet.angle = byte_add(angle, static_cast<std::int16_t>(
						std::bit_cast<std::int8_t>(angle_offset)
					));
					bullet.origin = safetycircle_pellet_origin(circle, radius, angle);
					bullet.group = BulletGroup::stack;
					bullet.count = 8U;
					bullet.speed = 16U;
					if(!bullets_add(state)) return false;
					bullet.group = BulletGroup::spread;
					bullet.count = 4U;
					bullet.speed = 48U;
					if(!bullets_add(state)) return false;
					angle = byte_add(angle, 32);
				}
				presentation.sound(3U);
			}
		} else if(circle.shrink_frame < 176U) {
			circle.radius_ring_distance = word_add(circle.radius_ring_distance, 16);
			circle.radius_filled = word_sub(circle.radius_filled, 2);
		} else {
			circle.flag = Yuuka6SafetyCircleFlag::free;
		}
		++circle.shrink_frame;
		break;

	case Yuuka6SafetyCircleFlag::free:
		break;
	}
	return true;
}

void yuuka6_customs_render_tail(State &state) noexcept
{
	for(auto &cross : state.yuuka6_customs.chasecrosses) {
		if(cross.flag == Yuuka6ChasecrossFlag::free ||
			cross.flag == Yuuka6ChasecrossFlag::alive) continue;
		cross.flag = static_cast<Yuuka6ChasecrossFlag>(
			static_cast<std::uint8_t>(cross.flag) + 1U
		);
		if(static_cast<std::uint8_t>(cross.flag) >= chasecross_kill_anim_end) {
			cross.flag = Yuuka6ChasecrossFlag::free;
		}
	}
}

} // namespace th04::core
