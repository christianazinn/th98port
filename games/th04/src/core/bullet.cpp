#include "th04/core/state.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_sparks.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace th04::core {
namespace {

constexpr std::int16_t playfield_width = 384;
constexpr std::int16_t playfield_height = 368;
constexpr std::int16_t bullet_half_width = 128;
constexpr std::int16_t bullet_half_height = 128;
constexpr std::uint8_t bullet_decelerate_base_speed = 72U;
constexpr std::uint8_t bullet_decelerate_threshold = 64U;
constexpr std::uint8_t bullet_decelerate_frames = 32U;
constexpr std::uint8_t bullet_decay_frames_per_cel = 4U;
constexpr std::int16_t gather_radius_end = 32;
constexpr std::int16_t gather_radius_start = 1024;
constexpr std::int16_t gather_frames = 32;
constexpr std::uint16_t stage_graze_cap = 999U;
constexpr std::uint16_t bullet_killbox_width = 128U;
constexpr std::uint16_t bullet_killbox_height = 128U;
constexpr std::uint16_t graze_left = 256U;
constexpr std::uint16_t graze_top = 352U;
constexpr std::uint16_t graze_width = 576U;
constexpr std::uint16_t graze_height = 704U;
constexpr std::int16_t spark_graze_radius = 32;
constexpr std::uint16_t spark_graze_count = 2U;
constexpr std::int16_t pat_bullet_zap = 72;
constexpr std::int16_t pat_bullet16_directional = 76;
constexpr std::int16_t pat_decay_pellet = 108;
constexpr std::int16_t pat_decay_bullet16 = 112;
constexpr std::uint8_t bullet_zap_frames_per_cel = 4U;
constexpr std::uint8_t bullet_zap_cels = 4U;

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

[[nodiscard]] constexpr std::uint16_t word_add_unsigned(
	std::int16_t left, std::uint16_t right
) noexcept
{
	return static_cast<std::uint16_t>(static_cast<std::uint16_t>(left) + right);
}

[[nodiscard]] constexpr std::uint8_t byte_add(
	std::uint8_t left, int right
) noexcept
{
	return static_cast<std::uint8_t>(static_cast<unsigned>(left) +
		static_cast<unsigned>(right));
}

[[nodiscard]] constexpr std::int8_t as_i8(std::uint8_t value) noexcept
{
	return std::bit_cast<std::int8_t>(value);
}

[[nodiscard]] constexpr std::uint8_t raw(BulletSpawnFlag value) noexcept
{
	return static_cast<std::uint8_t>(value);
}

[[nodiscard]] constexpr std::uint8_t raw(BulletMoveFlag value) noexcept
{
	return static_cast<std::uint8_t>(value);
}

[[nodiscard]] constexpr bool playfield_encloses(Point point) noexcept
{
	return point.x > -bullet_half_width &&
		point.x < static_cast<std::int16_t>(playfield_width * 16 + bullet_half_width) &&
		point.y > -bullet_half_height &&
		point.y < static_cast<std::int16_t>(playfield_height * 16 + bullet_half_height);
}

[[nodiscard]] constexpr bool overlap_wh_fast(
	std::int16_t delta_x, std::int16_t delta_y
) noexcept
{
	return word_add_unsigned(delta_x, bullet_killbox_width / 2U) <= bullet_killbox_width &&
		word_add_unsigned(delta_y, bullet_killbox_height / 2U) <= bullet_killbox_height;
}

[[nodiscard]] constexpr bool overlap_graze(
	std::int16_t delta_x, std::int16_t delta_y
) noexcept
{
	return word_add_unsigned(delta_x, graze_left) <= graze_width &&
		word_add_unsigned(delta_y, graze_top) <= graze_height;
}

[[nodiscard]] bool template_speedtune_for_playperf(State &state) noexcept
{
	auto &bullet_template = state.bullets.template_;
	bullet_template.speed = static_cast<std::uint8_t>(bullet_template.speed / 2U);
	auto speed_from_playperf = static_cast<std::int16_t>(bullet_template.speed);
	speed_from_playperf = static_cast<std::int16_t>(
		speed_from_playperf * static_cast<std::int16_t>(state.resources.playperf)
	);
	speed_from_playperf = static_cast<std::int16_t>(speed_from_playperf / 16);
	bullet_template.speed = byte_add(bullet_template.speed, speed_from_playperf);
	if(bullet_template.speed > 128U) {
		bullet_template.speed = 128U;
	} else if(bullet_template.speed < 8U) {
		bullet_template.speed = 8U;
	}
	return true;
}

void tune_for_playperf(BulletTemplate &bullet_template, std::uint8_t playperf) noexcept
{
	switch(bullet_template.group) {
	case BulletGroup::stack:
	case BulletGroup::stack_aimed:
		if(playperf >= 24U) {
			++bullet_template.count;
		} else if(playperf <= 6U && bullet_template.count >= 2U) {
			--bullet_template.count;
		}
		break;

	case BulletGroup::spread:
	case BulletGroup::spread_aimed:
		if(playperf >= 24U) {
			bullet_template.count = byte_add(bullet_template.count, 2);
		} else if(playperf <= 6U && bullet_template.count >= 3U) {
			bullet_template.count = byte_add(bullet_template.count, -2);
		}
		break;

	case BulletGroup::ring:
	case BulletGroup::ring_aimed:
		if(playperf >= 24U) {
			bullet_template.count = byte_add(bullet_template.count, 4);
		} else if(playperf >= 20U) {
			bullet_template.count = byte_add(bullet_template.count, 2);
		} else if(bullet_template.count >= 5U) {
			if(playperf <= 10U) {
				bullet_template.count = byte_add(bullet_template.count, -2);
			}
			if(playperf <= 4U) {
				bullet_template.count = byte_add(bullet_template.count, -4);
			}
		}
		break;

	default:
		break;
	}
}

void tune_for_easy(BulletTemplate &bullet_template) noexcept
{
	switch(bullet_template.group) {
	case BulletGroup::stack:
	case BulletGroup::stack_aimed:
		bullet_template.delta = byte_add(
			bullet_template.delta, -static_cast<int>(bullet_template.delta / 4U)
		);
		if(bullet_template.count >= 2U) --bullet_template.count;
		break;

	case BulletGroup::spread:
	case BulletGroup::spread_aimed:
		if(bullet_template.count >= 3U) {
			bullet_template.count = byte_add(bullet_template.count, -2);
		}
		break;

	case BulletGroup::random_angle:
	case BulletGroup::ring:
	case BulletGroup::ring_aimed:
		bullet_template.count = static_cast<std::uint8_t>(bullet_template.count / 2U);
		break;

	default:
		break;
	}
}

void tune_for_hard(BulletTemplate &bullet_template) noexcept
{
	switch(bullet_template.group) {
	case BulletGroup::single_aimed:
		bullet_template.group = BulletGroup::stack_aimed;
		bullet_template.count = 2U;
		bullet_template.delta = 6U;
		break;
	case BulletGroup::single:
		bullet_template.group = BulletGroup::stack;
		bullet_template.count = 2U;
		bullet_template.delta = 6U;
		break;

	case BulletGroup::stack:
	case BulletGroup::stack_aimed:
		bullet_template.delta = byte_add(
			bullet_template.delta, static_cast<int>(bullet_template.delta / 2U)
		);
		break;

	case BulletGroup::spread:
	case BulletGroup::spread_aimed:
		bullet_template.count = byte_add(bullet_template.count, 2);
		break;

	case BulletGroup::ring:
	case BulletGroup::ring_aimed:
		bullet_template.count = static_cast<std::uint8_t>(
			(static_cast<unsigned>(bullet_template.count) * 3U) / 2U
		);
		if(bullet_template.count > 48U) bullet_template.count = 48U;
		break;

	case BulletGroup::random_angle:
	case BulletGroup::random_angle_and_speed:
		bullet_template.count = byte_add(bullet_template.count,
			static_cast<int>(bullet_template.count / 2U));
		break;

	default:
		break;
	}
}

void tune_for_lunatic(BulletTemplate &bullet_template) noexcept
{
	switch(bullet_template.group) {
	case BulletGroup::single_aimed:
		bullet_template.group = BulletGroup::spread_aimed;
		bullet_template.count = 3U;
		bullet_template.delta = 6U;
		break;
	case BulletGroup::single:
		bullet_template.group = BulletGroup::spread;
		bullet_template.count = 3U;
		bullet_template.delta = 6U;
		break;

	case BulletGroup::stack:
	case BulletGroup::stack_aimed:
		bullet_template.delta = byte_add(
			bullet_template.delta, static_cast<int>(bullet_template.delta / 2U)
		);
		++bullet_template.count;
		break;

	case BulletGroup::spread:
	case BulletGroup::spread_aimed:
		bullet_template.count = byte_add(bullet_template.count, 4);
		break;

	case BulletGroup::ring:
	case BulletGroup::ring_aimed:
		bullet_template.count = byte_add(bullet_template.count,
			static_cast<int>(bullet_template.count));
		if(bullet_template.count > 48U) bullet_template.count = 48U;
		break;

	case BulletGroup::random_angle:
	case BulletGroup::random_angle_and_speed:
		bullet_template.count = byte_add(bullet_template.count,
			static_cast<int>(bullet_template.count));
		break;

	default:
		break;
	}
}

[[nodiscard]] bool group_velocity_and_angle_set(
	State &state, std::int16_t group_i, bool &done
) noexcept
{
	auto &bullets = state.bullets;
	auto &bullet_template = bullets.template_;
	std::int16_t angle = 0;
	auto speed = bullet_template.speed;
	done = false;
	bool aimed = true;

	switch(bullet_template.group) {
	case BulletGroup::spread:
	case BulletGroup::spread_aimed:
		if((bullet_template.count & 1U) != 0U) {
			if(group_i == 0) {
				bullets.group_i_spread_angle = 0U;
				angle = 0;
			} else if((group_i & 1) != 0) {
				bullets.group_i_spread_angle = byte_add(
					bullets.group_i_spread_angle, bullet_template.delta
				);
				angle = static_cast<std::int16_t>(0x100 - bullets.group_i_spread_angle);
			} else {
				angle = bullets.group_i_spread_angle;
			}
		} else {
			if(group_i == 0) {
				bullets.group_i_spread_angle = static_cast<std::uint8_t>(
					bullet_template.delta / 2U
				);
				angle = bullets.group_i_spread_angle;
			} else if((group_i & 1) != 0) {
				angle = static_cast<std::int16_t>(0x100 - bullets.group_i_spread_angle);
			} else {
				bullets.group_i_spread_angle = byte_add(
					bullets.group_i_spread_angle, bullet_template.delta
				);
				angle = bullets.group_i_spread_angle;
			}
		}
		done = group_i >= static_cast<std::int16_t>(
			static_cast<int>(bullet_template.count) - 1
		);
		aimed = bullet_template.group == BulletGroup::spread_aimed;
		break;

	case BulletGroup::ring:
	case BulletGroup::ring_aimed:
		if(bullet_template.count == 0U) return false;
		angle = static_cast<std::int16_t>((static_cast<std::int32_t>(group_i) * 0x100) /
			static_cast<std::int16_t>(bullet_template.count));
		done = group_i >= static_cast<std::int16_t>(
			static_cast<int>(bullet_template.count) - 1
		);
		aimed = bullet_template.group == BulletGroup::ring_aimed;
		break;

	case BulletGroup::forcesingle_random_angle: {
		const auto random_angle = pc98::th0405::randring_next16(state.random);
		if(!random_angle) return false;
		angle = std::bit_cast<std::int16_t>(*random_angle);
		done = true;
		aimed = false;
		break;
	}

	case BulletGroup::forcesingle:
	case BulletGroup::single:
		done = true;
		aimed = false;
		break;

	case BulletGroup::random_angle: {
		const auto random_angle = pc98::th0405::randring_next16(state.random);
		if(!random_angle) return false;
		angle = std::bit_cast<std::int16_t>(*random_angle);
		done = group_i >= static_cast<std::int16_t>(
			static_cast<int>(bullet_template.count) - 1
		);
		aimed = false;
		break;
	}

	case BulletGroup::random_angle_and_speed: {
		const auto random_angle = pc98::th0405::randring_next16(state.random);
		if(!random_angle) return false;
		angle = std::bit_cast<std::int16_t>(*random_angle);
		const auto random_speed = pc98::th0405::randring_next16_and(state.random, 31U);
		if(!random_speed) return false;
		speed = byte_add(speed, static_cast<int>(*random_speed));
		done = group_i >= static_cast<std::int16_t>(
			static_cast<int>(bullet_template.count) - 1
		);
		aimed = false;
		break;
	}

	case BulletGroup::random_constrained_angle_aimed: {
		const auto random_angle = pc98::th0405::randring_next16_and(state.random, 31U);
		if(!random_angle) return false;
		angle = static_cast<std::int16_t>(*random_angle) - 16;
		done = group_i >= static_cast<std::int16_t>(
			static_cast<int>(bullet_template.count) - 1
		);
		break;
	}

	case BulletGroup::forcesingle_aimed:
	case BulletGroup::single_aimed:
		done = true;
		break;

	case BulletGroup::stack:
	case BulletGroup::stack_aimed:
		speed = byte_add(speed, static_cast<int>(bullet_template.delta) * group_i);
		done = group_i >= static_cast<std::int16_t>(
			static_cast<int>(bullet_template.count) - 1
		) ||
			bullet_template.speed >= 160U;
		aimed = bullet_template.group == BulletGroup::stack_aimed;
		break;

	default:
		// The source falls through to the aimed tail for raw unknown group bytes.
		break;
	}

	if(aimed) {
		const auto aim_angle = pc98::masterlib::iatan2_source(
			word_sub(state.player_pos.cur.y, bullet_template.origin.y),
			word_sub(state.player_pos.cur.x, bullet_template.origin.x)
		);
		if(!aim_angle) return false;
		angle = word_add(angle, static_cast<std::int16_t>(*aim_angle));
	}
	const auto absolute_angle = byte_add(bullet_template.angle, angle);
	const auto velocity = pc98::masterlib::vector2(absolute_angle,
		static_cast<std::int16_t>(speed));
	bullet_template.velocity = {velocity.x, velocity.y};
	bullets.group_i_absolute_angle = absolute_angle;
	return true;
}

[[nodiscard]] bool template_clip(State &state) noexcept
{
	auto &bullet_template = state.bullets.template_;
	if(state.resources.bullet_clear_time > 0U &&
		state.resources.bullet_clear_time <= 17U) {
		return true;
	}
	if(!playfield_encloses(bullet_template.origin)) return true;
	if(overlap_wh_fast(
		word_sub(bullet_template.origin.x, state.player_pos.cur.x),
		word_sub(bullet_template.origin.y, state.player_pos.cur.y)
	)) {
		state.player_is_hit = true;
		return true;
	}
	if(!state.bullets.group_fixedspeed) {
		(void)template_speedtune_for_playperf(state);
	}
	return false;
}

[[nodiscard]] bool gather_add_bullets(State &state) noexcept
{
	for(auto &gather : state.bullets.gathers) {
		if(gather.flag != bullet_free) continue;
		gather.flag = bullet_alive;
		gather.bullet_template = state.bullets.template_;
		gather.center.cur = state.bullets.gather_template.center;
		gather.radius_cur = state.bullets.gather_template.radius;
		gather.radius_prev = state.bullets.gather_template.radius;
		gather.center.velocity = state.bullets.gather_template.velocity;
		gather.angle_cur = 0U;
		gather.angle_delta = state.bullets.gather_template.angle_delta;
		gather.col = state.bullets.gather_template.col;
		gather.ring_points = state.bullets.gather_template.ring_points;
		gather.radius_delta = static_cast<std::int16_t>(
			state.bullets.gather_template.radius / gather_frames
		);
		return true;
	}
	return true;
}

[[nodiscard]] bool bullets_add_raw(State &state, bool special) noexcept
{
	auto &bullets = state.bullets;
	auto &bullet_template = bullets.template_;
	if(!special && bullet_template.spawn_type ==
		static_cast<std::uint8_t>(BulletSpawnType::gather_pellet)) {
		bullets.gather_template.center = bullet_template.origin;
		bullets.gather_template.velocity = {};
		bullets.gather_template.radius = gather_radius_start;
		bullets.gather_template.angle_delta = 2U;
		bullets.gather_template.col = 9U;
		bullets.gather_template.ring_points = 8;
		bullet_template.spawn_type = static_cast<std::uint8_t>(BulletSpawnType::pellet);
		return gather_add_bullets(state);
	}
	if(template_clip(state)) return true;

	const auto pellet = bullet_template.spawn_type ==
		static_cast<std::uint8_t>(BulletSpawnType::pellet);
	auto spawn_flag = BulletSpawnFlag::grazeable;
	if(!pellet) {
		if(bullet_template.spawn_type ==
			static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_backwards)) {
			spawn_flag = BulletSpawnFlag::cloud_backwards;
		} else if(bullet_template.spawn_type ==
			static_cast<std::uint8_t>(BulletSpawnType::bullet16_cloud_forwards)) {
			spawn_flag = BulletSpawnFlag::cloud_forwards;
		}
	}

	auto move_flag = BulletMoveFlag::regular;
	if(bullet_template.speed < bullet_decelerate_threshold ||
		state.resources.bullet_clear_time != 0U) {
		if(bullet_template.group != BulletGroup::stack &&
			bullet_template.group != BulletGroup::stack_aimed) {
			move_flag = BulletMoveFlag::decelerate;
		}
	}

	std::int16_t group_i = 0;
	const std::size_t first = pellet ? 0U : pellet_count;
	const std::size_t last = pellet ? pellet_count : bullet_count;
	for(std::size_t slot = last; slot-- > first;) {
		auto &bullet = bullets.pool[slot];
		if(bullet.flag != bullet_free) continue;
		bullet.flag = bullet_alive;
		if(special) {
			bullet.move_flag = BulletMoveFlag::special;
			bullet.special_motion = bullet_template.special_motion;
			bullet.state_count = 0U;
			bullet.state_value = bullets.template_special_angle;
		} else {
			bullet.move_flag = move_flag;
			bullet.state_count = bullet_decelerate_frames;
			bullet.state_value = byte_add(bullet_decelerate_base_speed,
				-static_cast<int>(bullet_template.speed));
		}
		bullet.age = 0U;
		bullet.pos.cur = bullet_template.origin;
		bullet.from_group = bullet_template.group;
		bullet.patnum = bullet_template.patnum;
		bullet.spawn_flag = spawn_flag;

		bool group_done = false;
		if(!group_velocity_and_angle_set(state, group_i, group_done)) return false;
		if(bullet_template.patnum >= pat_bullet16_directional) {
			bullet.patnum = static_cast<std::int16_t>(bullet.patnum +
				static_cast<std::int16_t>((bullets.group_i_absolute_angle + 3U) % 128U / 8U));
		}
		bullet.pos.velocity = bullet_template.velocity;
		bullet.angle = bullets.group_i_absolute_angle;
		bullet.speed_final = bullet_template.speed;
		bullet.speed_cur = bullet_template.speed;
		if(group_done) break;
		group_i = word_add(group_i, 1);
	}
	return true;
}

void velocity_set_from_angle_and_speed(Bullet &bullet) noexcept
{
	const auto velocity = pc98::masterlib::vector2(bullet.angle,
		static_cast<std::int16_t>(bullet.speed_cur));
	bullet.pos.velocity = {velocity.x, velocity.y};
}

void bullet_turn_x(State &state, Bullet &bullet) noexcept
{
	++bullet.state_count;
	bullet.angle = static_cast<std::uint8_t>(0x80U - bullet.angle);
	if(bullet.state_count >= state.bullets.special.value) {
		bullet.move_flag = BulletMoveFlag::regular;
	}
	velocity_set_from_angle_and_speed(bullet);
}

void bullet_turn_y(State &state, Bullet &bullet) noexcept
{
	++bullet.state_count;
	bullet.angle = static_cast<std::uint8_t>(0U - bullet.angle);
	if(bullet.state_count >= state.bullets.special.value) {
		bullet.move_flag = BulletMoveFlag::regular;
	}
	velocity_set_from_angle_and_speed(bullet);
}

void bullet_turn_complete(State &state, Bullet &bullet) noexcept
{
	bullet.speed_cur = bullet.speed_final;
	if(bullet.state_count >= state.bullets.special.value) {
		bullet.move_flag = BulletMoveFlag::regular;
	}
	velocity_set_from_angle_and_speed(bullet);
}

[[nodiscard]] bool bullet_update_special(State &state, Bullet &bullet) noexcept
{
	switch(bullet.special_motion) {
	case BulletSpecialMotion::decelerate_then_turn_aimed:
		if(bullet.speed_cur != 0U) {
			velocity_set_from_angle_and_speed(bullet);
			--bullet.speed_cur;
		} else {
			++bullet.state_count;
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
			++bullet.state_count;
			bullet.angle = byte_add(bullet.angle, bullet.state_value);
			bullet_turn_complete(state, bullet);
		}
		break;

	case BulletSpecialMotion::speedup:
		velocity_set_from_angle_and_speed(bullet);
		bullet.speed_cur = byte_add(bullet.speed_cur, state.bullets.special.value);
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
				const auto turn = static_cast<std::int8_t>(
					as_i8(bullet.state_value) - static_cast<int>(bullet.angle)
				);
				bullet.angle = byte_add(bullet.angle, static_cast<int>(turn / 4));
			}
		} else {
			bullet.angle = bullet.state_value;
			bullet.speed_cur = bullet.speed_final;
			bullet.move_flag = BulletMoveFlag::regular;
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
			bullet.pos.velocity.y = word_add(bullet.pos.velocity.y,
				static_cast<std::int16_t>(state.bullets.special.value));
		}
		break;

	case BulletSpecialMotion::none:
	default:
		break;
	}
	return true;
}

[[nodiscard]] bool spark_graze(State &state, Point center) noexcept
{
	pc98::th0405::SparkPool pool{state.sparks, state.spark_ring_offset};
	return pc98::th0405::sparks_add_random(
		pool, state.random, center, spark_graze_radius, spark_graze_count
	).has_value();
}

} // namespace

bool bullet_template_tune(State &state) noexcept
{
	auto &bullet_template = state.bullets.template_;
	if(state.rank > 4U) return false;
	tune_for_playperf(bullet_template, state.resources.playperf);
	switch(state.rank) {
	case 0U:
		tune_for_easy(bullet_template);
		break;
	case 1U:
		break;
	case 2U:
		tune_for_hard(bullet_template);
		break;
	case 3U:
		tune_for_lunatic(bullet_template);
		break;
	case 4U:
		break;
	default:
		return false;
	}
	return true;
}

bool bullets_add(State &state, bool special, bool fixed_speed) noexcept
{
	if(state.rank > 4U) return false;
	if(state.bullets.zap.active()) return true;
	const auto count = state.bullets.template_.count;
	const auto speed = state.bullets.template_.speed;
	state.bullets.group_fixedspeed = fixed_speed;
	const auto result = bullets_add_raw(state, special);
	if(!result) return false;
	state.bullets.group_fixedspeed = false;
	if(!special && state.rank == 0U) state.bullets.template_.count = count;
	state.bullets.template_.speed = speed;
	return true;
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
		++gather.angle_cur;
		if(gather.radius_cur < gather_radius_end) {
			gather.flag = bullet_remove;
			if(gather.bullet_template.spawn_type !=
				static_cast<std::uint8_t>(BulletSpawnType::gather_only)) {
				state.bullets.template_ = gather.bullet_template;
				state.bullets.template_.origin = gather.center.cur;
				if(!bullets_add(state)) return false;
			}
		}
	}
	return true;
}

bool bullets_update(State &state, Presentation &presentation) noexcept
{
	int bullets_seen = 0;
	if(!state.bullets.zap.active()) {
		for(std::size_t index = bullet_count; index-- > 0U;) {
			auto &bullet = state.bullets.pool[index];
			if(bullet.flag == bullet_free) continue;
			if(bullet.flag == bullet_remove) {
				bullet.flag = bullet_free;
				continue;
			}
			++bullets_seen;
			if(state.resources.bullet_clear_time != 0U) {
				if(raw(bullet.move_flag) < raw(BulletMoveFlag::decay)) {
					bullet.move_flag = BulletMoveFlag::decay;
					bullet.patnum = index >= pellet_count
						? pat_decay_bullet16 : pat_decay_pellet;
					state.score.score_delta += bullet.age != 0U ? 100U : 10U;
				} else {
					bullet.move_flag = static_cast<BulletMoveFlag>(
						raw(bullet.move_flag) + 1U
					);
					if(raw(bullet.move_flag) >= raw(BulletMoveFlag::decay_end)) {
						(void)pc98::th0405::update_motion(bullet.pos);
						bullet.flag = bullet_remove;
						continue;
					}
					if((raw(bullet.move_flag) % bullet_decay_frames_per_cel) == 0U) {
						++bullet.patnum;
					}
				}
			}
			++bullet.age;
			if(raw(bullet.spawn_flag) >= raw(BulletSpawnFlag::active)) {
				if(bullet.spawn_flag == BulletSpawnFlag::active) {
					bullet.spawn_flag = BulletSpawnFlag::grazeable;
				} else {
					if(bullet.spawn_flag == BulletSpawnFlag::cloud_backwards) {
						bullet.pos.prev = bullet.pos.cur;
						bullet.pos.cur.x = word_sub(bullet.pos.cur.x,
							word_shift_left(bullet.pos.velocity.x, 3U));
						bullet.pos.cur.y = word_sub(bullet.pos.cur.y,
							word_shift_left(bullet.pos.velocity.y, 3U));
						bullet.spawn_flag = BulletSpawnFlag::cloud_forwards;
					} else if(bullet.spawn_flag == BulletSpawnFlag::cloud_forwards) {
						(void)pc98::th0405::update_motion(bullet.pos);
					} else {
						bullet.pos.prev = bullet.pos.cur;
						bullet.pos.cur.x = word_add(bullet.pos.cur.x,
							static_cast<std::int16_t>(bullet.pos.velocity.x / 3));
						bullet.pos.cur.y = word_add(bullet.pos.cur.y,
							static_cast<std::int16_t>(bullet.pos.velocity.y / 3));
					}
					bullet.spawn_flag = static_cast<BulletSpawnFlag>(
						raw(bullet.spawn_flag) + 1U
					);
					if(raw(bullet.spawn_flag) >= raw(BulletSpawnFlag::cloud_end)) {
						bullet.spawn_flag = BulletSpawnFlag::active;
					}
					continue;
				}
			}
			if(bullet.move_flag == BulletMoveFlag::special) {
				if(!bullet_update_special(state, bullet)) return false;
			} else if(bullet.move_flag == BulletMoveFlag::decelerate) {
				--bullet.state_count;
				bullet.speed_cur = byte_add(bullet.speed_final,
					(static_cast<int>(bullet.state_count) * static_cast<int>(bullet.state_value)) /
					static_cast<int>(bullet_decelerate_frames));
				if(bullet.state_count == 0U) {
					bullet.speed_cur = bullet.speed_final;
					bullet.move_flag = BulletMoveFlag::regular;
				}
				velocity_set_from_angle_and_speed(bullet);
			}
			const auto position = pc98::th0405::update_motion(bullet.pos);
			if(!playfield_encloses(position)) {
				bullet.flag = bullet_remove;
				continue;
			}
			if(state.resources.bullet_clear_time != 0U) continue;
			const auto delta_x = word_sub(position.x, state.player_pos.cur.x);
			const auto delta_y = word_sub(position.y, state.player_pos.cur.y);
			if(state.player_invincibility_time == 0U) {
				if(bullet.spawn_flag != BulletSpawnFlag::grazeable) {
					if(overlap_wh_fast(delta_x, delta_y)) {
						bullet.flag = bullet_remove;
						state.player_is_hit = true;
						continue;
					}
				} else if(overlap_graze(delta_x, delta_y)) {
					if(!spark_graze(state, position)) return false;
					bullet.spawn_flag = BulletSpawnFlag::grazed;
					if(state.bullets.stage_graze < stage_graze_cap) {
						++state.bullets.stage_graze;
						presentation.hud(HudField::graze);
						state.score.score_delta += state.bullets.graze_score;
					}
				}
			}
		}
		if(!state.bullets.turbo_mode) {
			int threshold = 24 + static_cast<int>(state.resources.playperf);
			threshold += static_cast<int>(state.rank) * 8;
			if(bullets_seen >= threshold) {
				if((state.stage_frame & 1U) == 0U) state.bullets.slowdown_factor = 2U;
			} else if(bullets_seen >= threshold + 24) {
				state.bullets.slowdown_factor = 2U;
			}
		}
	} else {
		if(state.rank > 4U) return false;
		const auto patnum = static_cast<std::int16_t>(pat_bullet_zap +
			static_cast<std::int16_t>(state.bullets.zap.frame() / bullet_zap_frames_per_cel));
		std::uint16_t score_per_bullet = 1U;
		std::uint16_t score_step = 1U;
		std::uint16_t score_per_bullet_cap = 1600U;
		if(state.rank == 0U) {
			score_per_bullet_cap = 1000U;
		} else if(state.rank == 4U) {
			score_per_bullet_cap = 2000U;
		}
		state.bullets.overlay_popup_bonus = 0U;
		for(std::size_t index = bullet_count; index-- > 0U;) {
			auto &bullet = state.bullets.pool[index];
			if(bullet.flag != bullet_alive) continue;
			bullet.pos.velocity = {};
			(void)pc98::th0405::update_motion(bullet.pos);
			if(patnum < pat_bullet_zap + static_cast<std::int16_t>(bullet_zap_cels)) {
				bullet.patnum = patnum;
				continue;
			}
			state.bullets.overlay_popup_bonus += score_per_bullet;
			state.score.score_delta += score_per_bullet;
			presentation.pointnum(bullet.pos.cur, score_per_bullet, false);
			score_per_bullet = static_cast<std::uint16_t>(score_per_bullet + score_step);
			score_step = static_cast<std::uint16_t>(score_step + 3U);
			if(score_per_bullet > score_per_bullet_cap) {
				score_per_bullet = score_per_bullet_cap;
			}
			bullet.flag = bullet_remove;
		}
		if(state.bullets.overlay_popup_bonus != 0U) presentation.popup(2U);
		state.bullets.zap.advance_frame();
		if(patnum >= pat_bullet_zap + static_cast<std::int16_t>(bullet_zap_cels)) {
			state.bullets.zap.value = 0U;
		}
	}
	if(state.resources.bullet_clear_time != 0U) --state.resources.bullet_clear_time;
	return true;
}

} // namespace th04::core
