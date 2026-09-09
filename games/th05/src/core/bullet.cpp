#include "th05/core/state.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_sparks.hpp"
#include "th05/core/bullet.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace th05::core {
namespace {

constexpr std::int16_t playfield_width = 384;
constexpr std::int16_t playfield_height = 368;
constexpr std::int16_t bullet_half_extent = 128;
constexpr std::uint16_t bullet_killbox_extent = 128U;
constexpr std::uint16_t graze_left = 256U;
constexpr std::uint16_t graze_top = 352U;
constexpr std::uint16_t graze_width = 576U;
constexpr std::uint16_t graze_height = 704U;
constexpr std::uint16_t stage_graze_cap = 999U;
constexpr std::uint8_t decelerate_base_speed = 72U;
constexpr std::uint8_t decelerate_threshold = 64U;
constexpr std::uint8_t decelerate_frames = 32U;
constexpr std::uint8_t decay_frames_per_cel = 4U;
constexpr std::int16_t gather_radius_end = 32;
constexpr std::int16_t gather_radius_start = 1024;
constexpr std::int16_t gather_frames = 32;
constexpr std::uint16_t pat_bullet16_directional = 52U;
constexpr std::uint16_t pat_bullet16_vector = 84U;
constexpr std::uint16_t pat_bullet16_directional_blue_last = 67U;
constexpr std::uint16_t pat_bullet16_directional_green = 68U;
constexpr std::uint16_t pat_bullet16_directional_green_last = 83U;
constexpr std::uint16_t pat_bullet16_vector_red_last = 115U;
constexpr std::uint16_t pat_bullet16_vector_blue = 116U;
constexpr std::uint16_t pat_bullet_zap_initial = 152U;
constexpr std::uint16_t pat_bullet_zap_compare = 72U;
constexpr std::uint16_t pat_decay_pellet = 156U;
constexpr std::uint16_t pat_decay_bullet16 = 160U;

[[nodiscard]] constexpr std::uint8_t byte_add(
	std::uint8_t left, int right
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<unsigned>(left) + static_cast<unsigned>(right)
	);
}

[[nodiscard]] constexpr std::int16_t word_add(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) + static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::int16_t word_sub(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) - static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::int16_t word_shift_left(
	std::int16_t value, unsigned count
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(value) << count
	));
}

[[nodiscard]] constexpr bool playfield_encloses(
	pc98::th0405::SubpixelPoint point
) noexcept
{
	return point.x > -bullet_half_extent &&
		point.x < static_cast<std::int16_t>((playfield_width * 16) + bullet_half_extent) &&
		point.y > -bullet_half_extent &&
		point.y < static_cast<std::int16_t>((playfield_height * 16) + bullet_half_extent);
}

[[nodiscard]] constexpr bool overlap_wh(
	std::int16_t delta_x, std::int16_t delta_y
) noexcept
{
	const auto overlaps = [](std::int16_t delta, std::uint16_t extent) {
		return static_cast<std::uint16_t>(
			static_cast<std::uint16_t>(delta) + (extent / 2U)
		) <= extent;
	};
	return overlaps(delta_x, bullet_killbox_extent) &&
		overlaps(delta_y, bullet_killbox_extent);
}

[[nodiscard]] constexpr bool overlap_graze(
	std::int16_t delta_x, std::int16_t delta_y
) noexcept
{
	return static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(delta_x) + graze_left
	) <= graze_width && static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(delta_y) + graze_top
	) <= graze_height;
}

[[nodiscard]] constexpr bool is_aimed_group(std::uint8_t group) noexcept
{
	return (group & 1U) != 0U && group <=
		static_cast<std::uint8_t>(BulletGroup::forcesingle_aimed);
}

[[nodiscard]] constexpr bool is_stack_group(std::uint8_t group) noexcept
{
	return (group & static_cast<std::uint8_t>(~1U)) ==
		static_cast<std::uint8_t>(BulletGroup::stack);
}

[[nodiscard]] constexpr std::uint16_t bullet_patnum_for_angle(
	std::uint16_t patnum_base, std::uint8_t angle
) noexcept
{
	angle = byte_add(angle, 7);
	if(patnum_base < pat_bullet16_vector) angle = static_cast<std::uint8_t>(angle % 0x80U);
	return static_cast<std::uint16_t>(patnum_base + (angle >> 3U));
}

void bullet_update_patnum(Bullet &bullet) noexcept
{
	// th04/main/bullet/update.cpp with GAME == 5. Bounces retile every
	// directional/vector color range from the just-mutated raw angle.
	if(bullet.patnum < pat_bullet16_directional) return;
	std::uint16_t patnum_base{};
	if(bullet.patnum <= pat_bullet16_directional_blue_last) {
		patnum_base = pat_bullet16_directional;
	} else if(bullet.patnum <= pat_bullet16_directional_green_last) {
		patnum_base = pat_bullet16_directional_green;
	} else if(bullet.patnum <= pat_bullet16_vector_red_last) {
		patnum_base = pat_bullet16_vector;
	} else {
		patnum_base = pat_bullet16_vector_blue;
	}
	bullet.patnum = bullet_patnum_for_angle(patnum_base, bullet.angle);
}

void tune_for_easy(BulletTemplate &bullet_template) noexcept
{
	const auto group = bullet_template.group;
	if(group < static_cast<std::uint8_t>(BulletGroup::spread) ||
		group > static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed)) {
		return;
	}
	switch(group & static_cast<std::uint8_t>(~1U)) {
	case static_cast<std::uint8_t>(BulletGroup::spread):
	case static_cast<std::uint8_t>(BulletGroup::spread_stack):
		if(bullet_template.spread >= 3U) bullet_template.spread = byte_add(bullet_template.spread, -2);
		return;
	case static_cast<std::uint8_t>(BulletGroup::stack):
		bullet_template.stack_speed_delta = byte_add(
			bullet_template.stack_speed_delta,
			-static_cast<int>(bullet_template.stack_speed_delta >> 2U)
		);
		// Source tests the stale spread-angle byte rather than stack count.
		if(bullet_template.spread_angle_delta >= 2U) --bullet_template.stack;
		return;
	case static_cast<std::uint8_t>(BulletGroup::ring):
	case static_cast<std::uint8_t>(BulletGroup::ring_stack):
	case static_cast<std::uint8_t>(BulletGroup::random_angle):
		bullet_template.spread = static_cast<std::uint8_t>(bullet_template.spread >> 1U);
		return;
	default:
		return;
	}
}

void tune_for_hard(BulletTemplate &bullet_template) noexcept
{
	const auto group = bullet_template.group;
	if(group <= static_cast<std::uint8_t>(BulletGroup::single_aimed)) {
		bullet_template.group = byte_add(
			bullet_template.group, static_cast<int>(BulletGroup::stack)
		);
		bullet_template.stack = 2U;
		bullet_template.stack_speed_delta = 6U;
		return;
	}
	if(group < static_cast<std::uint8_t>(BulletGroup::ring) ||
		group > static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed)) {
		return;
	}
	switch(group & static_cast<std::uint8_t>(~1U)) {
	case static_cast<std::uint8_t>(BulletGroup::stack):
	case static_cast<std::uint8_t>(BulletGroup::spread_stack):
		bullet_template.stack_speed_delta = byte_add(
			bullet_template.stack_speed_delta,
			static_cast<int>(bullet_template.stack_speed_delta >> 1U)
		);
		return;
	case static_cast<std::uint8_t>(BulletGroup::ring):
	case static_cast<std::uint8_t>(BulletGroup::ring_stack):
	case static_cast<std::uint8_t>(BulletGroup::random_angle): {
		auto addition = static_cast<std::uint8_t>(bullet_template.spread >> 2U);
		if(addition > 6U) addition = 6U;
		bullet_template.spread = byte_add(bullet_template.spread, addition);
		return;
	}
	default:
		return;
	}
}

void tune_for_lunatic(BulletTemplate &bullet_template) noexcept
{
	const auto group = bullet_template.group;
	if(group > static_cast<std::uint8_t>(BulletGroup::random_angle_and_speed)) return;
	switch(group & static_cast<std::uint8_t>(~1U)) {
	case static_cast<std::uint8_t>(BulletGroup::single):
		bullet_template.group = byte_add(
			bullet_template.group, static_cast<int>(BulletGroup::spread)
		);
		bullet_template.spread = 3U;
		bullet_template.spread_angle_delta = 6U;
		return;
	case static_cast<std::uint8_t>(BulletGroup::spread):
	case static_cast<std::uint8_t>(BulletGroup::spread_stack):
		bullet_template.spread = byte_add(bullet_template.spread, 2);
		return;
	case static_cast<std::uint8_t>(BulletGroup::stack):
		bullet_template.stack_speed_delta = byte_add(
			bullet_template.stack_speed_delta,
			static_cast<int>(bullet_template.stack_speed_delta >> 1U)
		);
		++bullet_template.stack;
		return;
	case static_cast<std::uint8_t>(BulletGroup::ring):
	case static_cast<std::uint8_t>(BulletGroup::ring_stack): {
		auto addition = static_cast<std::uint8_t>(bullet_template.spread / 3U);
		if(addition > 12U) addition = 12U;
		bullet_template.spread = byte_add(bullet_template.spread, addition);
		return;
	}
	case static_cast<std::uint8_t>(BulletGroup::random_angle):
		bullet_template.spread = byte_add(
			bullet_template.spread, static_cast<int>(bullet_template.spread)
		);
		return;
	default:
		return;
	}
}

void tune_for_playperf(BulletTemplate &bullet_template, std::uint8_t playperf) noexcept
{
	if(!is_stack_group(bullet_template.group)) return;
	if(playperf >= 38U) {
		++bullet_template.stack;
	} else if(playperf <= 24U && bullet_template.stack >= 2U) {
		--bullet_template.stack;
	}
}

[[nodiscard]] bool template_clip(State &state) noexcept
{
	auto &bullet_template = state.bullets.bullet_template;
	if(state.resources.bullet_clear_time != 0U &&
		state.resources.bullet_clear_time < 17U) {
		return true;
	}
	if(!playfield_encloses(bullet_template.origin)) return true;
	if(overlap_wh(
		word_sub(bullet_template.origin.x, state.player_pos.cur.x),
		word_sub(bullet_template.origin.y, state.player_pos.cur.y)
	)) {
		// TH05 deliberately continues into allocation after this hit write.
		state.player_is_hit = true;
	}
	if(!state.bullets.group_fixedspeed) {
		bullet_template.speed = detail::bullet_speed_tune(
			bullet_template.speed, state.resources.playperf
		);
	}
	return false;
}

[[nodiscard]] bool group_velocity_and_angle_set(State &state, bool &done) noexcept
{
	auto &bullets = state.bullets;
	auto &bullet_template = bullets.bullet_template;
	auto group_i = bullets.group_i;
	auto spread = bullet_template.spread;
	if(spread == 0U) spread = 1U;
	auto speed = bullet_template.speed;
	std::uint8_t angle{};
	done = false;

	if(bullet_template.group > static_cast<std::uint8_t>(BulletGroup::forcesingle_aimed)) {
		done = true;
	} else {
		switch(static_cast<BulletGroup>(bullet_template.group)) {
		case BulletGroup::single:
			done = true;
			break;
		case BulletGroup::single_aimed:
			done = true;
			goto aimed;
		case BulletGroup::spread:
		case BulletGroup::spread_aimed:
		case BulletGroup::spread_stack:
		case BulletGroup::spread_stack_aimed:
			if(bullet_template.group == static_cast<std::uint8_t>(BulletGroup::spread_stack) ||
				bullet_template.group == static_cast<std::uint8_t>(BulletGroup::spread_stack_aimed)) {
				const auto level = static_cast<std::uint8_t>(group_i / spread);
				speed = byte_add(speed, static_cast<int>(level) * bullet_template.stack_speed_delta);
				const auto total = static_cast<std::uint8_t>(spread * bullet_template.stack);
				done = group_i >= static_cast<std::uint8_t>(total - 1U);
				group_i = static_cast<std::uint8_t>(group_i % spread);
			} else {
				done = group_i >= static_cast<std::uint8_t>(spread - 1U);
			}
			if((spread & 1U) != 0U) {
				if(group_i == 0U) {
					bullets.group_i_spread_angle = 0U;
					angle = 0U;
				} else if((group_i & 1U) != 0U) {
					bullets.group_i_spread_angle = byte_add(
						bullets.group_i_spread_angle, bullet_template.spread_angle_delta
					);
					angle = bullets.group_i_spread_angle;
				} else {
					angle = static_cast<std::uint8_t>(0U - bullets.group_i_spread_angle);
				}
			} else if(group_i == 0U) {
				bullets.group_i_spread_angle = static_cast<std::uint8_t>(
					bullet_template.spread_angle_delta >> 1U
				);
				angle = bullets.group_i_spread_angle;
			} else if((group_i & 1U) != 0U) {
				angle = static_cast<std::uint8_t>(0U - bullets.group_i_spread_angle);
			} else {
				bullets.group_i_spread_angle = byte_add(
					bullets.group_i_spread_angle, bullet_template.spread_angle_delta
				);
				angle = bullets.group_i_spread_angle;
			}
			if(is_aimed_group(bullet_template.group)) goto aimed;
			break;
		case BulletGroup::ring:
		case BulletGroup::ring_aimed:
		case BulletGroup::ring_stack:
		case BulletGroup::ring_stack_aimed:
			if(bullet_template.group == static_cast<std::uint8_t>(BulletGroup::ring_stack) ||
				bullet_template.group == static_cast<std::uint8_t>(BulletGroup::ring_stack_aimed)) {
				const auto level = static_cast<std::uint8_t>(group_i / spread);
				speed = byte_add(speed, static_cast<int>(level) * bullet_template.stack_speed_delta);
				const auto total = static_cast<std::uint8_t>(spread * bullet_template.stack);
				done = group_i >= static_cast<std::uint8_t>(total - 1U);
				group_i = static_cast<std::uint8_t>(group_i % spread);
			} else {
				done = group_i >= static_cast<std::uint8_t>(spread - 1U);
			}
			angle = static_cast<std::uint8_t>((static_cast<std::uint16_t>(group_i) << 8U) / spread);
			if(is_aimed_group(bullet_template.group)) goto aimed;
			break;
		case BulletGroup::stack:
		case BulletGroup::stack_aimed:
			speed = byte_add(speed, static_cast<int>(bullet_template.stack_speed_delta) * group_i);
			done = group_i >= static_cast<std::uint8_t>(bullet_template.stack - 1U) ||
				speed >= 160U;
			if(bullet_template.group == static_cast<std::uint8_t>(BulletGroup::stack_aimed)) goto aimed;
			break;
		case BulletGroup::random_angle_and_speed: {
			const auto speed_draw = pc98::th0405::randring_next16_and(state.random, 31U);
			if(!speed_draw) return false;
			speed = byte_add(speed, static_cast<int>(*speed_draw));
			[[fallthrough]];
		}
		case BulletGroup::random_angle: {
			const auto angle_draw = pc98::th0405::randring_next16(state.random);
			if(!angle_draw) return false;
			angle = static_cast<std::uint8_t>(*angle_draw);
			++group_i;
			done = group_i >= spread;
			break;
		}
		case BulletGroup::forcesingle:
			done = true;
			break;
		case BulletGroup::forcesingle_aimed:
			done = true;
			goto aimed;
		}
	}
	goto velocity;

aimed: {
	const auto aimed_angle = pc98::masterlib::iatan2_source(
		word_sub(state.player_pos.cur.y, bullet_template.origin.y),
		word_sub(state.player_pos.cur.x, bullet_template.origin.x)
	);
	if(!aimed_angle) return false;
	angle = byte_add(angle, static_cast<int>(*aimed_angle));
}

velocity:
	bullets.group_i_speed = speed;
	bullets.group_i_absolute_angle = byte_add(bullet_template.angle, angle);
	const auto velocity = pc98::masterlib::vector2(
		bullets.group_i_absolute_angle, static_cast<std::int16_t>(speed)
	);
	bullets.group_i_velocity = {velocity.x, velocity.y};
	return true;
}

[[nodiscard]] bool bullets_add_raw(State &state, bool special) noexcept
{
	auto &bullets = state.bullets;
	auto &bullet_template = bullets.bullet_template;
	if(bullet_template.spawn_type == bullet_spawn_type_gather_pellet ||
		bullet_template.spawn_type ==
			static_cast<std::uint8_t>(bullet_spawn_type_gather_pellet |
				bullet_spawn_type_no_decelerate)) {
		bullets.gather_template.center = bullet_template.origin;
		bullets.gather_template.velocity = {};
		bullets.gather_template.radius = gather_radius_start;
		bullets.gather_template.angle_delta = 2U;
		bullets.gather_template.color = 9U;
		bullets.gather_template.ring_points = 8;
		const auto saved_spawn_type = bullet_template.spawn_type;
		bullet_template.spawn_type = static_cast<std::uint8_t>(
			bullet_template.spawn_type - 1U
		);
		if(special) bullet_template.spawn_type = bullet_spawn_type_gather_normal_special;
		for(auto &gather : bullets.gathers) {
			if(gather.flag != bullet_free) continue;
			gather.flag = bullet_alive;
			gather.bullet_template = bullet_template;
			gather.center.cur = bullets.gather_template.center;
			gather.radius_cur = bullets.gather_template.radius;
			gather.radius_prev = bullets.gather_template.radius;
			gather.center.velocity = bullets.gather_template.velocity;
			gather.angle_cur = 0U;
			gather.angle_delta = bullets.gather_template.angle_delta;
			gather.color = bullets.gather_template.color;
			gather.ring_points = bullets.gather_template.ring_points;
			gather.radius_delta = static_cast<std::int16_t>(
				bullets.gather_template.radius / gather_frames
			);
			break;
		}
		bullet_template.spawn_type = saved_spawn_type;
		return true;
	}
	if(template_clip(state)) return true;

	const auto use_pellets = bullet_template.patnum == 0U;
	auto spawn_flag = bullet_spawn_grazeable;
	const auto spawn_base = static_cast<std::uint8_t>(
		bullet_template.spawn_type & static_cast<std::uint8_t>(
			bullet_spawn_type_no_decelerate - 1U
		)
	);
	if(spawn_base == bullet_spawn_type_cloud_backwards) {
		spawn_flag = bullet_spawn_cloud_backwards;
	} else if(spawn_base == bullet_spawn_type_cloud_forwards) {
		spawn_flag = bullet_spawn_cloud_forwards;
	}

	auto move_flag = bullet_move_regular;
	if(!special && (bullet_template.speed < decelerate_threshold ||
		state.resources.bullet_clear_time != 0U) &&
		(bullet_template.spawn_type & bullet_spawn_type_no_decelerate) == 0U) {
		move_flag = bullet_move_decelerate;
	}
	bullets.group_i = 0U;

	auto spawn_into = [&](auto &pool) -> bool {
		for(std::size_t slot = pool.size(); slot-- > 0U;) {
			auto &bullet = pool[slot];
			if(bullet.flag != bullet_free) continue;
			bullet.flag = bullet_alive;
			bullet.spawn_flag = spawn_flag;
			bullet.pos.cur = bullet_template.origin;
			if(special) {
				bullet.origin = bullet_template.origin;
				bullet.move_flag = bullet_move_special;
				bullet.distance = 0;
				bullet.u1.value = 0U;
				bullet.u2.value = bullets.bullet_template_special_angle;
				bullet.special_motion = bullet_template.special_motion;
			} else {
				bullet.move_flag = move_flag;
				bullet.u1.value = decelerate_frames;
				bullet.u2.value = byte_add(decelerate_base_speed,
					-static_cast<int>(bullet_template.speed));
			}
			bullet.age = 0U;
			bullet.from_group = bullet_template.group;
			bool group_done{};
			if(!group_velocity_and_angle_set(state, group_done)) return false;
			bullet.patnum = bullet_template.patnum;
			if(bullet.patnum >= pat_bullet16_directional) {
				bullet.patnum = bullet_patnum_for_angle(
					bullet.patnum, bullets.group_i_absolute_angle
				);
			}
			bullet.pos.velocity = bullets.group_i_velocity;
			bullet.angle = bullets.group_i_absolute_angle;
			bullet.speed_final = bullets.group_i_speed;
			bullet.speed_cur = bullets.group_i_speed;
			if(group_done) return true;
			++bullets.group_i;
		}
		return true;
	};

	return use_pellets ? spawn_into(bullets.pool.pellets) : spawn_into(bullets.pool.bullets16);
}

void velocity_set_from_angle_and_speed(Bullet &bullet) noexcept
{
	const auto velocity = pc98::masterlib::vector2(
		bullet.angle, static_cast<std::int16_t>(bullet.speed_cur)
	);
	bullet.pos.velocity = {velocity.x, velocity.y};
}

void bullet_turn_x(State &state, Bullet &bullet) noexcept
{
	++bullet.u1.value;
	bullet.angle = static_cast<std::uint8_t>(0x80U - bullet.angle);
	if(bullet.u1.value >= state.bullets.special_turns_max) {
		bullet.move_flag = bullet_move_regular;
	}
	velocity_set_from_angle_and_speed(bullet);
	bullet_update_patnum(bullet);
}

void bullet_turn_y(State &state, Bullet &bullet) noexcept
{
	++bullet.u1.value;
	bullet.angle = static_cast<std::uint8_t>(0U - bullet.angle);
	if(bullet.u1.value >= state.bullets.special_turns_max) {
		bullet.move_flag = bullet_move_regular;
	}
	velocity_set_from_angle_and_speed(bullet);
	bullet_update_patnum(bullet);
}

void bullet_turn_complete(State &state, Bullet &bullet) noexcept
{
	bullet_update_patnum(bullet);
	bullet.speed_cur = bullet.speed_final;
	if(bullet.u1.value >= state.bullets.special_turns_max) {
		bullet.move_flag = bullet_move_regular;
	}
	velocity_set_from_angle_and_speed(bullet);
}

[[nodiscard]] bool bullet_update_special(State &state, Bullet &bullet) noexcept
{
	switch(static_cast<BulletSpecialMotion>(bullet.special_motion)) {
	case BulletSpecialMotion::decelerate_then_turn_aimed:
		if(bullet.speed_cur != 0U) {
			velocity_set_from_angle_and_speed(bullet);
			--bullet.speed_cur;
		} else {
			++bullet.u1.value;
			const auto angle = pc98::masterlib::iatan2_source(
				word_sub(state.player_pos.cur.y, bullet.pos.cur.y),
				word_sub(state.player_pos.cur.x, bullet.pos.cur.x)
			);
			if(!angle) return false;
			bullet.angle = *angle;
			bullet_turn_complete(state, bullet);
		}
		break;

	case BulletSpecialMotion::decelerate_then_turn:
		if(bullet.speed_cur != 0U) {
			velocity_set_from_angle_and_speed(bullet);
			--bullet.speed_cur;
		} else {
			++bullet.u1.value;
			bullet.angle = byte_add(bullet.angle, bullet.u2.value);
			bullet_turn_complete(state, bullet);
		}
		break;

	case BulletSpecialMotion::speedup:
		velocity_set_from_angle_and_speed(bullet);
		bullet.speed_cur = byte_add(bullet.speed_cur, state.bullets.special_turns_max);
		break;

	case BulletSpecialMotion::decelerate_to_angle:
		if(bullet.speed_cur != 0U) {
			velocity_set_from_angle_and_speed(bullet);
			if(bullet.speed_cur > 1U) {
				bullet.speed_cur = byte_add(bullet.speed_cur, -2);
			} else {
				bullet.speed_cur = 0U;
			}
			if(bullet.speed_cur < 32U) {
				const auto turn = std::bit_cast<std::int8_t>(
					static_cast<std::uint8_t>(bullet.u2.value - bullet.angle)
				);
				bullet.angle = byte_add(
					bullet.angle, static_cast<int>(turn / 4)
				);
			}
		} else {
			bullet.angle = bullet.u2.value;
			bullet.speed_cur = bullet.speed_final;
			bullet.move_flag = bullet_move_regular;
			velocity_set_from_angle_and_speed(bullet);
		}
		break;

	case BulletSpecialMotion::bounce_left_right:
		if(bullet.pos.cur.x <= 0 || bullet.pos.cur.x >= playfield_width * 16) {
			bullet_turn_x(state, bullet);
		}
		break;

	case BulletSpecialMotion::bounce_top_bottom:
		if(bullet.pos.cur.y <= 0 || bullet.pos.cur.y >= playfield_height * 16) {
			bullet_turn_y(state, bullet);
		}
		break;

	case BulletSpecialMotion::bounce_left_right_top_bottom:
		if(bullet.pos.cur.x <= 0 || bullet.pos.cur.x >= playfield_width * 16) {
			bullet_turn_x(state, bullet);
		}
		if(bullet.pos.cur.y <= 0 || bullet.pos.cur.y >= playfield_height * 16) {
			bullet_turn_y(state, bullet);
		}
		break;

	case BulletSpecialMotion::bounce_left_right_top:
		if(bullet.pos.cur.x <= 0 || bullet.pos.cur.x >= playfield_width * 16) {
			bullet_turn_x(state, bullet);
		}
		if(bullet.pos.cur.y <= 0) bullet_turn_y(state, bullet);
		break;

	case BulletSpecialMotion::gravity:
		if((state.stage_frame & 1U) != 0U) {
			bullet.pos.velocity.y = word_add(
				bullet.pos.velocity.y,
				static_cast<std::int16_t>(state.bullets.special_turns_max)
			);
		}
		break;

	case BulletSpecialMotion::exact_linear: {
		bullet.distance = word_add(
			bullet.distance, static_cast<std::int16_t>(bullet.speed_cur)
		);
		const auto target = pc98::masterlib::vector2(bullet.angle, bullet.distance);
		const auto target_x = word_add(bullet.origin.x, target.x);
		const auto target_y = word_add(bullet.origin.y, target.y);
		bullet.pos.velocity.x = word_sub(target_x, bullet.pos.cur.x);
		bullet.pos.velocity.y = word_sub(target_y, bullet.pos.cur.y);
		break;
	}

	case BulletSpecialMotion::none:
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool spark_graze(State &state, pc98::th0405::SubpixelPoint center) noexcept
{
	pc98::th0405::SparkPool pool{state.sparks, state.spark_ring_offset};
	return pc98::th0405::sparks_add_random(pool, state.random, center, 32, 2U).has_value();
}

template <typename Pool>
[[nodiscard]] bool bullets_update_pool(
	State &state, Presentation &presentation, Pool &pool, bool pellets,
	int &bullets_seen
) noexcept
{
	for(std::size_t slot = pool.size(); slot-- > 0U;) {
		auto &bullet = pool[slot];
		if(bullet.flag == bullet_free) continue;
		if(bullet.flag != bullet_alive) {
			bullet.flag = bullet_free;
			continue;
		}
		++bullets_seen;
		if(state.resources.bullet_clear_time != 0U) {
			if(bullet.move_flag < bullet_move_decay) {
				bullet.move_flag = bullet_move_decay;
				bullet.patnum = pellets ? pat_decay_pellet : pat_decay_bullet16;
				state.score.score_delta += bullet.age != 0U ? 100U : 10U;
			} else {
				++bullet.move_flag;
				if(bullet.move_flag >= bullet_move_decay_end) {
					(void)pc98::th0405::update_motion(bullet.pos);
					bullet.flag = bullet_remove;
					continue;
				}
				if((bullet.move_flag % decay_frames_per_cel) == 0U) ++bullet.patnum;
			}
		}
		++bullet.age;
		if(bullet.spawn_flag >= bullet_spawn_active) {
			if(bullet.spawn_flag == bullet_spawn_active) {
				bullet.spawn_flag = bullet_spawn_grazeable;
			} else {
				if(bullet.spawn_flag == bullet_spawn_cloud_backwards) {
					bullet.pos.prev = bullet.pos.cur;
					bullet.pos.cur.x = word_sub(bullet.pos.cur.x,
						word_shift_left(bullet.pos.velocity.x, 3U));
					bullet.pos.cur.y = word_sub(bullet.pos.cur.y,
						word_shift_left(bullet.pos.velocity.y, 3U));
					bullet.spawn_flag = bullet_spawn_cloud_forwards;
				} else if(bullet.spawn_flag == bullet_spawn_cloud_forwards) {
					(void)pc98::th0405::update_motion(bullet.pos);
				} else {
					bullet.pos.prev = bullet.pos.cur;
					bullet.pos.cur.x = word_add(bullet.pos.cur.x,
						static_cast<std::int16_t>(bullet.pos.velocity.x / 3));
					bullet.pos.cur.y = word_add(bullet.pos.cur.y,
						static_cast<std::int16_t>(bullet.pos.velocity.y / 3));
				}
				++bullet.spawn_flag;
				if(bullet.spawn_flag >= bullet_spawn_cloud_end) {
					if(!playfield_encloses(bullet.pos.cur)) {
						bullet.flag = bullet_remove;
						continue;
					}
					bullet.spawn_flag = bullet_spawn_active;
				} else if(pellets) {
					if(state.bullets.pellet_cloud_count >= state.bullets.pellet_cloud_indices.size()) {
						return false;
					}
					state.bullets.pellet_cloud_indices[state.bullets.pellet_cloud_count++] =
						static_cast<std::uint16_t>(slot);
				}
				continue;
			}
		}
		if(bullet.move_flag == bullet_move_special) {
			if(!bullet_update_special(state, bullet)) return false;
		} else if(bullet.move_flag == bullet_move_decelerate) {
			--bullet.u1.value;
			bullet.speed_cur = byte_add(bullet.speed_final,
				(static_cast<int>(bullet.u1.value) * static_cast<int>(
					std::bit_cast<std::int8_t>(bullet.u2.value)
				)) /
					static_cast<int>(decelerate_frames));
			if(bullet.u1.value == 0U) {
				bullet.speed_cur = bullet.speed_final;
				bullet.move_flag = bullet_move_regular;
			}
			const auto velocity = pc98::masterlib::vector2(
				bullet.angle, static_cast<std::int16_t>(bullet.speed_cur)
			);
			bullet.pos.velocity = {velocity.x, velocity.y};
		}
		const auto position = pc98::th0405::update_motion(bullet.pos);
		if(!playfield_encloses(position)) {
			bullet.flag = bullet_remove;
			continue;
		}
		if(state.resources.bullet_clear_time != 0U) continue;
		if(state.player_invincibility_time != 0U) continue;
		const auto delta_x = word_sub(position.x, state.player_pos.cur.x);
		const auto delta_y = word_sub(position.y, state.player_pos.cur.y);
		if(bullet.spawn_flag != bullet_spawn_grazeable) {
			if(overlap_wh(delta_x, delta_y)) {
				bullet.flag = bullet_remove;
				state.player_is_hit = true;
				continue;
			}
		} else if(overlap_graze(delta_x, delta_y)) {
			if(!spark_graze(state, position)) return false;
			bullet.spawn_flag = bullet_spawn_grazed;
			if(state.bullets.stage_graze < stage_graze_cap) {
				++state.bullets.stage_graze;
				presentation.hud(HudField::graze);
				state.score.score_delta += state.graze_score;
			}
		}
	}
	return true;
}

[[nodiscard]] bool bullets_zap_update(State &state, Presentation &presentation) noexcept
{
	const auto patnum = static_cast<std::uint16_t>(pat_bullet_zap_initial +
		(static_cast<std::uint16_t>(state.bullets.zap.frame()) / 4U));
	std::uint16_t score_per_bullet = 1U;
	std::uint16_t score_step = 1U;
	std::uint16_t score_cap{};
	switch(state.rank) {
	case 0U: score_cap = 960U; break;
	case 1U:
	case 2U:
	case 3U: score_cap = 1280U; break;
	case 4U: score_cap = 1600U; break;
	default: return false;
	}
	state.bullets.popup_bonus = 0U;
	std::uint16_t bullets_seen{};
	auto zap_pool = [&](auto &pool) {
		for(std::size_t slot = pool.size(); slot-- > 0U;) {
			auto &bullet = pool[slot];
			if(bullet.flag != bullet_alive) continue;
			bullet.pos.velocity = {};
			(void)pc98::th0405::update_motion(bullet.pos);
			if(patnum < (pat_bullet_zap_compare + 4U)) {
				bullet.patnum = patnum;
				++bullets_seen;
				continue;
			}
			state.bullets.popup_bonus += score_per_bullet;
			state.score.score_delta += score_per_bullet;
			presentation.pointnum(bullet.pos.cur, score_per_bullet, false);
			score_per_bullet = static_cast<std::uint16_t>(score_per_bullet + score_step);
			score_step = static_cast<std::uint16_t>(score_step + 3U);
			if(score_per_bullet > score_cap) score_per_bullet = score_cap;
			bullet.flag = bullet_remove;
			if(state.bullets.zap_drop_point_items && ((bullets_seen % 4U) == 0U)) {
				items_add(state, presentation, bullet.pos.cur, ItemType::point);
			}
			++bullets_seen;
		}
	};
	zap_pool(state.bullets.pool.bullets16);
	zap_pool(state.bullets.pool.pellets);
	if(state.bullets.popup_bonus != 0U) presentation.popup(2U);
	state.bullets.zap.advance();
	// The macro below this function's first PAT_BULLET_ZAP use makes this
	// condition true on its first TH05 zap frame.
	if(patnum >= (pat_bullet_zap_compare + 4U)) state.bullets.zap.stop();
	return true;
}

} // namespace

bool bullet_template_tune(State &state) noexcept
{
	if(state.rank > 4U) return false;
	auto &bullet_template = state.bullets.bullet_template;
	tune_for_playperf(bullet_template, state.resources.playperf);
	switch(state.rank) {
	case 0U: tune_for_easy(bullet_template); break;
	case 1U: break;
	case 2U: tune_for_hard(bullet_template); break;
	case 3U: tune_for_lunatic(bullet_template); break;
	case 4U: break;
	default: return false;
	}
	return true;
}

bool bullets_add_regular(State &state) noexcept
{
	if(state.bullets.zap.active()) return true;
	const auto saved_angle = state.bullets.bullet_template.angle;
	const auto saved_speed = state.bullets.bullet_template.speed;
	const auto result = bullets_add_raw(state, false);
	state.bullets.bullet_template.angle = saved_angle;
	state.bullets.bullet_template.speed = saved_speed;
	return result;
}

bool bullets_add_special(State &state) noexcept
{
	if(state.bullets.zap.active()) return true;
	const auto saved_angle = state.bullets.bullet_template.angle;
	const auto saved_speed = state.bullets.bullet_template.speed;
	const auto result = bullets_add_raw(state, true);
	state.bullets.bullet_template.angle = saved_angle;
	state.bullets.bullet_template.speed = saved_speed;
	return result;
}

bool bullets_add_regular_fixedspeed(State &state) noexcept
{
	state.bullets.group_fixedspeed = true;
	const auto result = bullets_add_regular(state);
	state.bullets.group_fixedspeed = false;
	return result;
}

bool bullets_add_special_fixedspeed(State &state) noexcept
{
	state.bullets.group_fixedspeed = true;
	const auto result = bullets_add_special(state);
	state.bullets.group_fixedspeed = false;
	return result;
}

void gather_add_only(State &state) noexcept
{
	for(auto &gather : state.bullets.gathers) {
		if(gather.flag != bullet_free) continue;
		gather.flag = bullet_alive;
		gather.bullet_template.spawn_type = 0xFFU;
		gather.center.cur = state.bullets.gather_template.center;
		gather.radius_cur = state.bullets.gather_template.radius;
		gather.radius_prev = state.bullets.gather_template.radius;
		gather.center.velocity = state.bullets.gather_template.velocity;
		gather.angle_cur = 0U;
		gather.angle_delta = state.bullets.gather_template.angle_delta;
		gather.color = state.bullets.gather_template.color;
		gather.ring_points = state.bullets.gather_template.ring_points;
		gather.radius_delta = static_cast<std::int16_t>(
			state.bullets.gather_template.radius / gather_frames
		);
		return;
	}
}

void gather_add_only_3stack(
	State &state, std::int16_t frame, std::uint8_t color_for_0,
	std::uint8_t color_for_2_and_4
) noexcept
{
	switch(frame) {
	case 0:
		state.bullets.gather_template.color = color_for_0;
		gather_add_only(state);
		return;
	case 2:
		state.bullets.gather_template.color = color_for_2_and_4;
		gather_add_only(state);
		return;
	case 4:
		gather_add_only(state);
		return;
	default:
		return;
	}
}

bool gathers_update(State &state) noexcept
{
	for(auto &gather : state.bullets.gathers) {
		if(gather.flag == bullet_free) continue;
		if(gather.flag >= bullet_remove) {
			gather.flag = bullet_free;
			continue;
		}
		(void)pc98::th0405::update_motion(gather.center);
		gather.radius_prev = gather.radius_cur;
		gather.radius_cur = word_sub(gather.radius_cur, gather.radius_delta);
		gather.angle_cur = byte_add(gather.angle_cur, gather.angle_delta);
		if(gather.radius_cur >= gather_radius_end) continue;
		gather.flag = bullet_remove;
		if(gather.bullet_template.spawn_type == 0xFFU) continue;
		state.bullets.bullet_template = gather.bullet_template;
		state.bullets.bullet_template.origin = gather.center.cur;
		if(state.bullets.bullet_template.spawn_type < bullet_spawn_type_gather_normal_special) {
			if(!bullets_add_regular(state)) return false;
		} else {
			state.bullets.bullet_template.spawn_type = bullet_spawn_type_normal;
			if(!bullets_add_special(state)) return false;
		}
	}
	return true;
}

bool bullets_update(State &state, Presentation &presentation) noexcept
{
	state.bullets.pellet_cloud_count = 0U;
	if(state.bullets.zap.active()) {
		if(!bullets_zap_update(state, presentation)) return false;
	} else {
		int bullets_seen{};
		if(!bullets_update_pool(state, presentation, state.bullets.pool.bullets16,
			false, bullets_seen)) return false;
		if(!bullets_update_pool(state, presentation, state.bullets.pool.pellets,
			true, bullets_seen)) return false;
		if(!state.turbo_mode) {
			state.slowdown_caused_by_bullets = false;
			const auto threshold = 42 + (static_cast<int>(state.rank) * 8);
			if(bullets_seen >= threshold) {
				if((state.stage_frame & 1U) == 0U) {
					state.slowdown_factor = 2U;
					state.slowdown_caused_by_bullets = true;
				}
			} else if(bullets_seen >= (threshold + 32)) {
				state.slowdown_factor = 2U;
				state.slowdown_caused_by_bullets = true;
			}
		}
	}
	if(state.resources.bullet_clear_time != 0U) --state.resources.bullet_clear_time;
	return true;
}

} // namespace th05::core
