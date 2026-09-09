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

} // namespace th02::core
