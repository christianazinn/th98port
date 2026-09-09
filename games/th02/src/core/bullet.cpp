#include "th02/core/bullet.hpp"

#include "th02/core/random.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"

#include <bit>

namespace th02::core {
namespace {

constexpr unsigned int subpixel_bits = 4U;
constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_top = 16;
constexpr std::int16_t playfield_right = 416;
constexpr std::int16_t playfield_bottom = 384;
constexpr std::int16_t pellet_width = 8;
constexpr std::int16_t pellet_height = 8;
constexpr std::int16_t bullet16_height = 16;
constexpr std::int16_t speed_min = 16;
constexpr std::int8_t rank_easy = 0;
constexpr std::int8_t rank_lunatic = 3;

enum class GroupVelocityResult : std::uint8_t {
	continued,
	done,
	source_iatan2_fault,
};

[[nodiscard]] std::int16_t word_add(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t word_subtract(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) -
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t word_multiply(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) *
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t signed_word_divide(
	std::int16_t numerator, std::int16_t denominator
) noexcept
{
	return static_cast<std::int16_t>(
		static_cast<std::int32_t>(numerator) /
		static_cast<std::int32_t>(denominator)
	);
}

[[nodiscard]] std::int16_t arithmetic_shift_right(
	std::int16_t value, unsigned int count
) noexcept
{
	const auto divisor = static_cast<std::int32_t>(1) << count;
	if(value >= 0) return static_cast<std::int16_t>(value / divisor);
	return static_cast<std::int16_t>(
		-(((-static_cast<std::int32_t>(value)) + divisor - 1) / divisor)
	);
}

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) <<
		subpixel_bits
	);
	return std::bit_cast<std::int16_t>(bits);
}

void increment_byte(std::int8_t &value) noexcept
{
	const auto bits = static_cast<std::uint8_t>(
		std::bit_cast<std::uint8_t>(value) + 1U
	);
	value = std::bit_cast<std::int8_t>(bits);
}

[[nodiscard]] bool bullet_clip(ScreenPoint topleft) noexcept
{
	return (topleft.x < static_cast<std::int16_t>(playfield_left - pellet_width)) ||
		(topleft.x > playfield_right) ||
		(topleft.y < static_cast<std::int16_t>(playfield_top - bullet16_height)) ||
		(topleft.y > static_cast<std::int16_t>(playfield_bottom - 1));
}

[[nodiscard]] bool bullet_speed_tune(EnemyBulletPoolState &state,
	std::int16_t playperf, std::int8_t rank, std::int16_t &speed) noexcept
{
	const auto product = word_multiply(speed, playperf);
	speed = word_add(speed, signed_word_divide(product, 48));
	speed = word_add(speed, static_cast<std::int16_t>(state.rank_base_speed));
	if(speed >= speed_min) return false;

	increment_byte(state.easy_slow_skip_cycle);
	if(rank == rank_easy &&
		(std::bit_cast<std::uint8_t>(state.easy_slow_skip_cycle) & 1U) != 0U) {
		return true;
	}
	speed = speed_min;
	return false;
}

[[nodiscard]] GroupVelocityResult group_velocity_set(
	std::int16_t &group_i, std::uint8_t group, EnemyBullet &bullet,
	std::int16_t speed, RandomState &random,
	const PlayerFrameState &player_frame
) noexcept
{
	std::int16_t angle_offset{};
	bool done = false;
	bool aim = true;

	switch(group) {
	case enemy_bullet_group::one_aimed:
		done = true;
		break;

	case enemy_bullet_group::spread_2_ultrawide_aimed:
		angle_offset = word_add(angle_offset, 5);
		[[fallthrough]];
	case enemy_bullet_group::spread_2_wide:
	case enemy_bullet_group::spread_2_wide_aimed:
		angle_offset = word_add(angle_offset, 3);
		[[fallthrough]];
	case enemy_bullet_group::spread_2_medium:
	case enemy_bullet_group::spread_2_medium_aimed:
		angle_offset = word_add(angle_offset, 3);
		[[fallthrough]];
	case enemy_bullet_group::spread_2_narrow:
	case enemy_bullet_group::spread_2_narrow_aimed:
		if(group_i == 0) {
			angle_offset = word_add(3, angle_offset);
		} else if(group_i == 1) {
			angle_offset = word_subtract(-3, angle_offset);
			done = true;
		}
		aim = group >= enemy_bullet_group::spread_2_narrow_aimed;
		break;

	case enemy_bullet_group::spread_3_wide:
	case enemy_bullet_group::spread_3_wide_aimed:
		angle_offset = word_add(angle_offset, 8);
		[[fallthrough]];
	case enemy_bullet_group::spread_3_medium:
	case enemy_bullet_group::spread_3_medium_aimed:
		angle_offset = word_add(angle_offset, 8);
		[[fallthrough]];
	case enemy_bullet_group::spread_3_narrow:
	case enemy_bullet_group::spread_3_narrow_aimed:
		if(group_i == 0) {
			angle_offset = 0;
		} else if(group_i == 1) {
			angle_offset = word_add(8, angle_offset);
		} else if(group_i == 2) {
			angle_offset = word_subtract(-8, angle_offset);
			done = true;
		}
		aim = group >= enemy_bullet_group::spread_3_narrow_aimed;
		break;

	case enemy_bullet_group::spread_4_wide:
	case enemy_bullet_group::spread_4_wide_aimed:
		angle_offset = word_add(angle_offset, 6);
		[[fallthrough]];
	case enemy_bullet_group::spread_4_medium:
	case enemy_bullet_group::spread_4_medium_aimed:
		angle_offset = word_add(angle_offset, 6);
		[[fallthrough]];
	case enemy_bullet_group::spread_4_narrow:
	case enemy_bullet_group::spread_4_narrow_aimed:
		if(group_i == 0) {
			angle_offset = word_add(3, angle_offset);
		} else if(group_i == 1) {
			angle_offset = word_subtract(-3, angle_offset);
		} else if(group_i == 2) {
			angle_offset = word_add(9, word_multiply(angle_offset, 3));
		} else if(group_i == 3) {
			angle_offset = word_subtract(-9, word_multiply(angle_offset, 3));
			done = true;
		}
		aim = group >= enemy_bullet_group::spread_4_narrow_aimed;
		break;

	case enemy_bullet_group::spread_5_wide:
	case enemy_bullet_group::spread_5_wide_aimed:
		angle_offset = word_add(angle_offset, 6);
		[[fallthrough]];
	case enemy_bullet_group::spread_5_medium:
	case enemy_bullet_group::spread_5_medium_aimed:
		angle_offset = word_add(angle_offset, 6);
		[[fallthrough]];
	case enemy_bullet_group::spread_5_narrow:
	case enemy_bullet_group::spread_5_narrow_aimed:
		if(group_i == 0) {
			angle_offset = 0;
		} else if(group_i == 1) {
			angle_offset = word_add(6, angle_offset);
		} else if(group_i == 2) {
			angle_offset = word_subtract(-6, angle_offset);
		} else if(group_i == 3) {
			angle_offset = word_add(12, word_multiply(angle_offset, 2));
		} else if(group_i == 4) {
			angle_offset = word_subtract(-12, word_multiply(angle_offset, 2));
			done = true;
		}
		aim = group >= enemy_bullet_group::spread_5_narrow_aimed;
		break;

	case enemy_bullet_group::ring_2:
		angle_offset = (group_i == 0) ? 0 : 0x80;
		done = group_i != 0;
		aim = false;
		break;

	case enemy_bullet_group::ring_4:
		angle_offset = word_multiply(group_i, 64);
		done = group_i >= 3;
		aim = false;
		break;
	case enemy_bullet_group::ring_8:
		angle_offset = word_multiply(group_i, 32);
		done = group_i >= 7;
		aim = false;
		break;
	case enemy_bullet_group::ring_16:
		angle_offset = word_multiply(group_i, 16);
		done = group_i >= 15;
		aim = false;
		break;
	case enemy_bullet_group::ring_32:
		angle_offset = word_multiply(group_i, 8);
		done = group_i >= 31;
		aim = false;
		break;

	case enemy_bullet_group::spread_2_horizontally_symmetric:
		if(group_i == 0) {
			angle_offset = 0;
		} else {
			angle_offset = word_subtract(0x80,
				word_multiply(static_cast<std::int16_t>(bullet.angle), 2));
			done = true;
		}
		aim = false;
		break;

	case enemy_bullet_group::one:
		done = true;
		aim = false;
		break;

	case enemy_bullet_group::one_random_angle:
		angle_offset = static_cast<std::int16_t>(randring_next8(random));
		done = true;
		aim = false;
		break;

	case enemy_bullet_group::random_angle:
		angle_offset = static_cast<std::int16_t>(randring_next8(random));
		done = group_i >= static_cast<std::int16_t>(bullet.angle);
		aim = false;
		break;

	case enemy_bullet_group::random_angle_and_speed:
		angle_offset = static_cast<std::int16_t>(randring_next8(random));
		speed = word_add(speed, static_cast<std::int16_t>(
			randring_next8(random) & 0x1FU
		));
		done = group_i >= static_cast<std::int16_t>(bullet.angle);
		aim = false;
		break;

	default:
		break;
	}

	const auto vector_angle = static_cast<std::uint8_t>(
		static_cast<std::int16_t>(bullet.angle) + angle_offset
	);
	if(aim) {
		const auto aim_y = word_add(player_frame.player.current.y,
			static_cast<std::int16_t>(24 - (pellet_height / 2)));
		const auto aim_x = word_add(player_frame.player.current.x,
			static_cast<std::int16_t>(16 - (pellet_width / 2)));
		const auto aimed = pc98::masterlib::iatan2_source(
			word_subtract(aim_y, bullet.screen_topleft_on_page[player_frame.page_back].y),
			word_subtract(aim_x, bullet.screen_topleft_on_page[player_frame.page_back].x)
		);
		if(!aimed) return GroupVelocityResult::source_iatan2_fault;
		const auto velocity = pc98::masterlib::vector2(
			static_cast<std::uint8_t>(*aimed + vector_angle), speed
		);
		bullet.velocity = {velocity.x, velocity.y};
	} else {
		const auto velocity = pc98::masterlib::vector2(vector_angle, speed);
		bullet.velocity = {velocity.x, velocity.y};
	}

	group_i = word_add(group_i, 1);
	auto &position = bullet.screen_topleft_on_page[player_frame.page_back];
	position.x = to_subpixels(position.x);
	position.y = to_subpixels(position.y);
	return done ? GroupVelocityResult::done : GroupVelocityResult::continued;
}

[[nodiscard]] std::int16_t to_pixels(std::int16_t subpixels) noexcept
{
	return arithmetic_shift_right(subpixels, subpixel_bits);
}

[[nodiscard]] std::uint8_t byte_add(
	std::uint8_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(lhs) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return static_cast<std::uint8_t>(bits);
}

void increment_byte(std::uint8_t &value) noexcept
{
	value = byte_add(value, 1);
}

[[nodiscard]] std::int16_t word_negate(std::int16_t value) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		0U - std::bit_cast<std::uint16_t>(value)
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] bool bullet_velocity_aimed(EnemyBullet &bullet,
	ScreenPoint bullet_topleft, const PlayerFrameState &player_frame
) noexcept
{
	const auto aim_y = word_add(player_frame.player.current.y, 8);
	const auto aim_x = word_add(player_frame.player.current.x, 8);
	const auto aimed = pc98::masterlib::iatan2_source(
		word_subtract(aim_y, bullet_topleft.y),
		word_subtract(aim_x, bullet_topleft.x)
	);
	if(!aimed) return false;
	const auto velocity = pc98::masterlib::vector2(
		*aimed, static_cast<std::int16_t>(bullet.speed)
	);
	bullet.velocity = {velocity.x, velocity.y};
	return true;
}

[[nodiscard]] bool bullet_update_special(EnemyBullet &bullet,
	ScreenPoint bullet_topleft, const PlayerFrameState &player_frame,
	const EnemyBulletSpecialState &special, std::uint32_t stage_frame
) noexcept
{
	const auto aim_y = word_add(player_frame.player.current.y, 8);
	const auto aim_x = word_add(player_frame.player.current.x, 8);
	auto chase_player = [&](std::int16_t speed) noexcept {
		if(bullet_topleft.x < aim_x) {
			bullet.velocity.x = word_add(bullet.velocity.x, speed);
		} else if(bullet_topleft.x > aim_x) {
			bullet.velocity.x = word_subtract(bullet.velocity.x, speed);
		}
		if(bullet_topleft.y < aim_y) {
			bullet.velocity.y = word_add(bullet.velocity.y, speed);
		} else if(bullet_topleft.y > aim_y) {
			bullet.velocity.y = word_subtract(bullet.velocity.y, speed);
		}
	};

	switch(bullet.group_or_special_motion) {
	case enemy_bullet_special_motion::chase:
		chase_player(special.chase_gravity_or_drift_angle);
		break;

	case enemy_bullet_special_motion::homing:
		increment_byte(bullet.u1);
		if(static_cast<std::int16_t>(bullet.u1) <
			special.homing_frames_or_drift_speed) {
			if(!bullet_velocity_aimed(bullet, bullet_topleft, player_frame)) {
				return false;
			}
		} else {
			bullet.group_or_special_motion = 0U;
		}
		break;

	case enemy_bullet_special_motion::decelerate_then_turn_aimed:
		if((stage_frame & 7U) != 0U) break;
		bullet.velocity.x = signed_word_divide(bullet.velocity.x, 2);
		bullet.velocity.y = signed_word_divide(bullet.velocity.y, 2);
		if((bullet.velocity.x == 0) && (bullet.velocity.y == 0)) {
			if(!bullet_velocity_aimed(bullet, bullet_topleft, player_frame)) {
				return false;
			}
			increment_byte(bullet.u1);
			if(static_cast<std::int16_t>(bullet.u1) >=
				special.turns_or_drift_frames) {
				bullet.group_or_special_motion = 0U;
			}
		}
		break;

	case enemy_bullet_special_motion::gravity:
		bullet.velocity.y = word_add(
			bullet.velocity.y, special.chase_gravity_or_drift_angle
		);
		break;

	case enemy_bullet_special_motion::drift_angle_and_speed: {
		bullet.angle = byte_add(
			bullet.angle, special.chase_gravity_or_drift_angle
		);
		bullet.speed = byte_add(
			bullet.speed, special.homing_frames_or_drift_speed
		);
		const auto velocity = pc98::masterlib::vector2(
			bullet.angle, static_cast<std::int16_t>(bullet.speed)
		);
		bullet.velocity = {velocity.x, velocity.y};
		increment_byte(bullet.u1);
		if(static_cast<std::int16_t>(bullet.u1) >
			special.turns_or_drift_frames) {
			bullet.group_or_special_motion = 0U;
		}
		break;
	}

	case enemy_bullet_special_motion::drift_angle_chase: {
		bullet.angle = byte_add(
			bullet.angle, special.chase_gravity_or_drift_angle
		);
		const auto velocity = pc98::masterlib::vector2(
			bullet.angle, static_cast<std::int16_t>(bullet.speed)
		);
		bullet.velocity = {velocity.x, velocity.y};
		chase_player(special.homing_frames_or_drift_speed);
		increment_byte(bullet.u1);
		if(static_cast<std::int16_t>(bullet.u1) >
			special.turns_or_drift_frames) {
			bullet.group_or_special_motion = 0U;
		}
		break;
	}

	case enemy_bullet_special_motion::bounce_left_right_top_bottom:
		if((bullet_topleft.x < 40) || (bullet_topleft.x > 392)) {
			bullet.velocity.x = word_negate(bullet.velocity.x);
			increment_byte(bullet.u1);
		}
		[[fallthrough]];

	case enemy_bullet_special_motion::bounce_top_bottom:
		if((bullet_topleft.y < 24) || (bullet_topleft.y > 360)) {
			bullet.velocity.y = word_negate(bullet.velocity.y);
			increment_byte(bullet.u1);
		}
		if(static_cast<std::int16_t>(bullet.u1) >
			special.turns_or_drift_frames) {
			bullet.group_or_special_motion = 0U;
		}
		break;

	default:
		break;
	}
	return true;
}

[[nodiscard]] std::int16_t scroll_screen_y_to_vram(
	std::int16_t screen_y, std::int16_t scroll_line
) noexcept
{
	auto vram_y = word_add(screen_y, scroll_line);
	if(vram_y >= 400) {
		vram_y = word_subtract(vram_y, 400);
	}
	return vram_y;
}

} // namespace

EnemyBulletAddResult bullets_add_pellet(
	EnemyBulletPoolState &state, RandomState &random,
	const PlayerFrameState &player_frame, std::int16_t playperf,
	std::int8_t rank, bool bombing, ScreenPoint topleft, std::uint8_t angle,
	std::uint8_t group, std::int16_t speed
) noexcept
{
	if(player_frame.page_back >= state.bullets.front().screen_topleft_on_page.size()) {
		return EnemyBulletAddResult::invalid_page;
	}
	if(bullet_clip(topleft) || bombing ||
		bullet_speed_tune(state, playperf, rank, speed)) {
		return EnemyBulletAddResult::completed;
	}

	std::int16_t group_i{};
	std::int16_t stack_i{};
	std::size_t slot_i{};
	do {
		while(slot_i < state.bullets.size()) {
			auto &bullet = state.bullets[slot_i];
			if(bullet.flag != enemy_bullet_free) {
				++slot_i;
				continue;
			}

			bullet.screen_topleft_on_page[player_frame.page_back] = {
				topleft.x, topleft.y,
			};
			bullet.group_or_special_motion = 0U;
			bullet.flag = enemy_bullet_alive;
			bullet.size_type = enemy_bullet_pellet;
			bullet.u1 = 0U;
			bullet.angle = angle;

			++slot_i;
			const auto velocity_result = group_velocity_set(
				group_i, group, bullet, speed, random, player_frame
			);
			if(velocity_result == GroupVelocityResult::source_iatan2_fault) {
				return EnemyBulletAddResult::source_iatan2_fault;
			}
			if(velocity_result == GroupVelocityResult::done) break;
		}

		if(stack_i >= static_cast<std::int16_t>(state.stack)) break;
		speed = arithmetic_shift_right(speed, 1U);
		if(rank == rank_lunatic) speed = word_add(speed, 8);
		if(speed < speed_min) break;
		stack_i = word_add(stack_i, 1);
		group_i = 0;
	} while(true);
	return EnemyBulletAddResult::completed;
}

EnemyBulletAddResult bullets_add_16x16(
	EnemyBulletPoolState &state, RandomState &random,
	const PlayerFrameState &player_frame, std::int16_t playperf,
	std::int8_t rank, bool bombing, ScreenPoint topleft, std::uint8_t angle,
	std::uint8_t group_or_special_motion, std::int16_t patnum,
	std::int16_t speed
) noexcept
{
	if(player_frame.page_back >= state.bullets.front().screen_topleft_on_page.size()) {
		return EnemyBulletAddResult::invalid_page;
	}
	if(bullet_clip(topleft) || bombing ||
		((group_or_special_motion != enemy_bullet_special_motion::gravity) &&
			bullet_speed_tune(state, playperf, rank, speed))) {
		return EnemyBulletAddResult::completed;
	}

	std::int16_t group_i{};
	std::int16_t stack_i{};
	std::size_t slot_i{};
	do {
		while(slot_i < state.bullets.size()) {
			auto &bullet = state.bullets[slot_i];
			if(bullet.flag != enemy_bullet_free) {
				++slot_i;
				continue;
			}

			bullet.group_or_special_motion = group_or_special_motion;
			bullet.size_type = enemy_bullet_16x16;
			bullet.flag = enemy_bullet_alive;
			bullet.patnum = static_cast<std::uint8_t>(patnum);
			bullet.u1 = 0U;
			bullet.speed = static_cast<std::uint8_t>(speed);
			bullet.angle = angle;

			++slot_i;
			if(group_or_special_motion < enemy_bullet_special_motion::first) {
				bullet.screen_topleft_on_page[player_frame.page_back] = {
					topleft.x, topleft.y,
				};
				const auto velocity_result = group_velocity_set(
					group_i, group_or_special_motion, bullet, speed, random,
					player_frame
				);
				if(velocity_result == GroupVelocityResult::source_iatan2_fault) {
					return EnemyBulletAddResult::source_iatan2_fault;
				}
				if(velocity_result == GroupVelocityResult::continued) continue;
			} else {
				bullet.screen_topleft_on_page[player_frame.page_back] = {
					to_subpixels(topleft.x), to_subpixels(topleft.y),
				};
				const auto velocity = pc98::masterlib::vector2(angle, speed);
				bullet.velocity = {velocity.x, velocity.y};
			}
			break;
		}

		if(stack_i >= static_cast<std::int16_t>(state.stack)) break;
		speed = arithmetic_shift_right(speed, 1U);
		if(rank == rank_lunatic) speed = word_add(speed, 8);
		if(speed < speed_min) break;
		stack_i = word_add(stack_i, 1);
		group_i = 0;
	} while(true);
	return EnemyBulletAddResult::completed;
}

EnemyBulletUpdateResult bullets_update_and_render(
	EnemyBulletPoolState &state, const PlayerFrameState &player_frame,
	PlayerHitState &hit, const EnemyBulletSpecialState &special,
	std::uint32_t stage_frame, std::int16_t scroll_line,
	EnemyBulletPresentationCallbacks &presentation
) noexcept
{
	if(player_frame.page_back >= state.bullets.front().screen_topleft_on_page.size()) {
		return EnemyBulletUpdateResult::invalid_page;
	}

	bool not_rendering_pellets = true;
	for(auto &bullet : state.bullets) {
		if(bullet.flag != enemy_bullet_alive) continue;

		auto &position = bullet.screen_topleft_on_page[player_frame.page_back];
		position.x = word_add(position.x, bullet.velocity.x);
		position.y = word_add(position.y, bullet.velocity.y);
		ScreenPoint topleft{to_pixels(position.x), to_pixels(position.y)};
		if(bullet.size_type == enemy_bullet_16x16 &&
			!bullet_update_special(
				bullet, topleft, player_frame, special, stage_frame
			)) {
			return EnemyBulletUpdateResult::source_iatan2_fault;
		}

		if((topleft.x <= 20) || (topleft.x >= playfield_right) ||
			(topleft.y >= playfield_bottom) || (topleft.y <= 0)) {
			bullet.flag = enemy_bullet_remove;
			continue;
		}

		const auto player_center_x = word_add(player_frame.player.current.x, 16);
		const auto player_center_y = word_add(player_frame.player.current.y, 24);
		const bool player_hit = (bullet.size_type == enemy_bullet_pellet)
			? ((topleft.x >= word_add(player_center_x, -9)) &&
				(topleft.x < word_add(player_center_x, 1)) &&
				(topleft.y >= word_add(player_center_y, -12)) &&
				(topleft.y <= word_add(player_center_y, -2)))
			: ((topleft.x >= word_add(player_center_x, -19)) &&
				(topleft.x < word_add(player_center_x, 3)) &&
				(topleft.y >= word_add(player_center_y, -20)) &&
				(topleft.y <= player_center_y));
		if(player_hit) {
			hit.raw_hit = 1U;
			bullet.flag = enemy_bullet_remove;
			continue;
		}

		const auto vram_top = scroll_screen_y_to_vram(topleft.y, scroll_line);
		if(bullet.size_type == enemy_bullet_pellet) {
			if(not_rendering_pellets) {
				presentation.present({
					EnemyBulletPresentationKind::set_pellet_color_white,
					{},
					0U,
				});
				not_rendering_pellets = false;
			}
			presentation.present({
				EnemyBulletPresentationKind::render_pellet,
				{topleft.x, vram_top},
				0U,
			});
		} else {
			presentation.present({
				EnemyBulletPresentationKind::grcg_off,
				{},
				0U,
			});
			presentation.present({
				EnemyBulletPresentationKind::render_bullet_16x16,
				{topleft.x, vram_top},
				bullet.patnum,
			});
			not_rendering_pellets = true;
		}
	}
	presentation.present({EnemyBulletPresentationKind::grcg_off, {}, 0U});
	return EnemyBulletUpdateResult::completed;
}

} // namespace th02::core
