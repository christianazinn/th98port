#pragma once

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_random.hpp"
#include "th04/core/shot_spawn.hpp"

#include <bit>
#include <cstdint>

namespace th04::core {

inline constexpr std::int16_t homing_target_none = -15984;
inline constexpr std::int16_t pat_shot_reimu_sub_a = 30;
inline constexpr std::int16_t player_option_distance = 384;

// th04/main/player/shot_reimu.hpp has one byte outside the allocator. Keep the
// raw x word even when y is the no-target sentinel: the source only tests y.
struct ReimuAPatternState {
	std::uint8_t cycle{};
	pc98::th0405::SubpixelPoint homing_target{};
	bool operator==(const ReimuAPatternState &) const = default;
};

enum class ReimuAPatternStatus : std::uint8_t {
	complete,
	allocation_boundary,
	random_refused,
	iatan2_fault,
};

struct ReimuAPatternResult {
	ReimuAPatternStatus status{ReimuAPatternStatus::complete};
	ShotAddResult allocation{};
	std::uint8_t shots_allocated{};
	[[nodiscard]] constexpr bool operator==(const ReimuAPatternResult &other) const noexcept
	{
		return status == other.status && allocation.status == other.allocation.status &&
			allocation.record_index == other.allocation.record_index &&
			shots_allocated == other.shots_allocated;
	}
};

namespace detail {

[[nodiscard]] constexpr std::int16_t reimu_a_word_add(
	std::int16_t left, std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(left) + right
	));
}

[[nodiscard]] constexpr std::int16_t reimu_a_word_sub(
	std::int16_t left, std::int16_t right
) noexcept
{
	const auto negated = static_cast<std::uint16_t>(
		0U - std::bit_cast<std::uint16_t>(right)
	);
	return reimu_a_word_add(left, std::bit_cast<std::int16_t>(negated));
}

constexpr void reimu_a_set_velocity(Shot &shot, std::uint8_t angle) noexcept
{
	const auto velocity = pc98::masterlib::vector2(angle, 192);
	shot.pos.velocity = {velocity.x, velocity.y};
}

constexpr void reimu_a_from_option(Shot &shot, bool left) noexcept
{
	shot.pos.cur.x = reimu_a_word_add(
		shot.pos.cur.x, left ? -player_option_distance : player_option_distance
	);
}

enum class ReimuAConfigureStatus : std::uint8_t {
	complete,
	random_refused,
	iatan2_fault,
};

[[nodiscard]] constexpr ReimuAConfigureStatus reimu_a_set_homing_velocity(
	Shot &shot,
	const ReimuAPatternState &pattern,
	pc98::th0405::SubpixelPoint player_cur,
	std::uint8_t angle_offset
) noexcept
{
	const auto angle = pc98::masterlib::iatan2_source(
		reimu_a_word_sub(pattern.homing_target.y, player_cur.y),
		reimu_a_word_sub(pattern.homing_target.x, shot.pos.cur.x)
	);
	if(!angle) return ReimuAConfigureStatus::iatan2_fault;
	reimu_a_set_velocity(shot, static_cast<std::uint8_t>(angle_offset + *angle));
	return ReimuAConfigureStatus::complete;
}

[[nodiscard]] constexpr ReimuAConfigureStatus reimu_a_option_homing(
	Shot &shot,
	const ReimuAPatternState &pattern,
	ShotSpawnState &spawn,
	pc98::th0405::RandomState &random,
	bool left
) noexcept
{
	reimu_a_from_option(shot, left);
	if(pattern.homing_target.y == homing_target_none) {
		shot.patnum_base = pat_shot_reimu_sub_a;
		return ReimuAConfigureStatus::complete;
	}
	const auto random_offset = pc98::th0405::randring_next16_and(random, 7U);
	if(!random_offset) return ReimuAConfigureStatus::random_refused;
	const auto status = reimu_a_set_homing_velocity(
		shot,
		pattern,
		spawn.player_cur,
		static_cast<std::uint8_t>(static_cast<std::uint16_t>(*random_offset) - 4U)
	);
	if(status == ReimuAConfigureStatus::complete) {
		shot.patnum_base = pat_shot_reimu_sub_a;
	}
	return status;
}

template <typename Configure>
[[nodiscard]] constexpr ReimuAPatternResult reimu_a_run(
	ShotSpawnState &spawn,
	ShotState &shots,
	int remaining,
	Configure configure
) noexcept
{
	reset_shot_allocator(spawn);
	ReimuAPatternResult result{};
	while(true) {
		result.allocation = shots_add(spawn, shots);
		if(!result.allocation.allocated()) {
			if(result.allocation.status != ShotAddStatus::no_slot) {
				result.status = ReimuAPatternStatus::allocation_boundary;
			}
			return result;
		}
		++result.shots_allocated;
		auto *const shot = shot_spawn_record(spawn, shots, result.allocation.record_index);
		if(shot == nullptr) {
			result.status = ReimuAPatternStatus::allocation_boundary;
			return result;
		}
		switch(configure(*shot, remaining)) {
		case ReimuAConfigureStatus::complete:
			break;
		case ReimuAConfigureStatus::random_refused:
			result.status = ReimuAPatternStatus::random_refused;
			return result;
		case ReimuAConfigureStatus::iatan2_fault:
			result.status = ReimuAPatternStatus::iatan2_fault;
			return result;
		}
		--remaining;
		if(remaining <= 0) return result;
	}
}

[[nodiscard]] constexpr int reimu_a_cycle_count(
	ReimuAPatternState &pattern,
	const ShotSpawnState &spawn,
	int own_count,
	std::uint8_t divisor
) noexcept
{
	if(spawn.shot_time == 18U) pattern.cycle = 0;
	if((pattern.cycle % divisor) == 0U) own_count += 2;
	pattern.cycle = static_cast<std::uint8_t>(pattern.cycle + 1U);
	return own_count;
}

[[nodiscard]] constexpr ReimuAConfigureStatus reimu_a_l2_option(
	Shot &shot,
	int remaining,
	const ReimuAPatternState &pattern,
	ShotSpawnState &spawn,
	pc98::th0405::RandomState &random
) noexcept
{
	const auto status = reimu_a_option_homing(
		shot, pattern, spawn, random, remaining == 3
	);
	if(status != ReimuAConfigureStatus::complete) return status;
	shot.damage = 10;
	return status;
}

[[nodiscard]] constexpr ReimuAConfigureStatus reimu_a_option(
	Shot &shot,
	int remaining,
	int left_at,
	std::int8_t damage,
	const ReimuAPatternState &pattern,
	ShotSpawnState &spawn,
	pc98::th0405::RandomState &random
) noexcept
{
	const auto status = reimu_a_option_homing(
		shot, pattern, spawn, random, remaining == left_at
	);
	if(status != ReimuAConfigureStatus::complete) return status;
	shot.damage = damage;
	return status;
}

} // namespace detail

[[nodiscard]] constexpr ReimuAPatternResult shot_reimu_a_l2(
	ReimuAPatternState &pattern,
	ShotSpawnState &spawn,
	ShotState &shots,
	pc98::th0405::RandomState &random
) noexcept
{
	const auto count = detail::reimu_a_cycle_count(pattern, spawn, 1, 3U);
	return detail::reimu_a_run(spawn, shots, count,
		[&](Shot &shot, int remaining) constexpr {
			if(remaining == 1) {
				const auto angle = pc98::th0405::randring_next16_and(random, 15U);
				if(!angle) return detail::ReimuAConfigureStatus::random_refused;
				detail::reimu_a_set_velocity(shot, static_cast<std::uint8_t>(0xB8U + *angle));
				shot.patnum_base = pat_shot_reimu;
				shot.damage = 10;
				return detail::ReimuAConfigureStatus::complete;
			}
			return detail::reimu_a_l2_option(shot, remaining, pattern, spawn, random);
		}
	);
}

[[nodiscard]] constexpr ReimuAPatternResult shot_reimu_a_l3(
	ReimuAPatternState &pattern,
	ShotSpawnState &spawn,
	ShotState &shots,
	pc98::th0405::RandomState &random
) noexcept
{
	const auto count = detail::reimu_a_cycle_count(pattern, spawn, 2, 3U);
	return detail::reimu_a_run(spawn, shots, count,
		[&](Shot &shot, int remaining) constexpr {
			if(remaining <= 2) {
				shot.pos.cur.x = detail::reimu_a_word_add(
					shot.pos.cur.x, remaining == 2 ? -128 : 128
				);
				shot.patnum_base = pat_shot_reimu;
				shot.damage = 9;
				return detail::ReimuAConfigureStatus::complete;
			}
			return detail::reimu_a_option(shot, remaining, 4, 10, pattern, spawn, random);
		}
	);
}

template <std::uint8_t AngleInit, std::uint8_t Divisor, std::uint8_t Step,
	std::int8_t OwnDamage, std::int8_t OptionDamage>
[[nodiscard]] constexpr ReimuAPatternResult shot_reimu_a_fan(
	ReimuAPatternState &pattern,
	ShotSpawnState &spawn,
	ShotState &shots,
	pc98::th0405::RandomState &random
) noexcept
{
	const auto count = detail::reimu_a_cycle_count(pattern, spawn, 3, Divisor);
	auto angle = AngleInit;
	return detail::reimu_a_run(spawn, shots, count,
		[&](Shot &shot, int remaining) constexpr {
			if(remaining <= 3) {
				detail::reimu_a_set_velocity(shot, angle);
				shot.patnum_base = pat_shot_reimu;
				shot.damage = OwnDamage;
				angle = static_cast<std::uint8_t>(angle + Step);
				return detail::ReimuAConfigureStatus::complete;
			}
			return detail::reimu_a_option(
				shot, remaining, 5, OptionDamage, pattern, spawn, random
			);
		}
	);
}

[[nodiscard]] constexpr ReimuAPatternResult shot_reimu_a_l4(
	ReimuAPatternState &pattern, ShotSpawnState &spawn, ShotState &shots,
	pc98::th0405::RandomState &random
) noexcept
{
	return shot_reimu_a_fan<0xBAU, 3U, 6U, 8, 9>(pattern, spawn, shots, random);
}

[[nodiscard]] constexpr ReimuAPatternResult shot_reimu_a_l5(
	ReimuAPatternState &pattern, ShotSpawnState &spawn, ShotState &shots,
	pc98::th0405::RandomState &random
) noexcept
{
	return shot_reimu_a_fan<0xB8U, 2U, 8U, 8, 9>(pattern, spawn, shots, random);
}

[[nodiscard]] constexpr ReimuAPatternResult shot_reimu_a_l6(
	ReimuAPatternState &pattern, ShotSpawnState &spawn, ShotState &shots,
	pc98::th0405::RandomState &random
) noexcept
{
	return shot_reimu_a_fan<0xB8U, 2U, 8U, 7, 8>(pattern, spawn, shots, random);
}

namespace detail {

[[nodiscard]] constexpr ReimuAConfigureStatus reimu_a_mid(
	Shot &shot, int remaining
) noexcept
{
	const auto left = remaining == 5;
	reimu_a_from_option(shot, left);
	reimu_a_set_velocity(shot, left ? 0xB8U : 0xC8U);
	shot.patnum_base = pat_shot_reimu;
	shot.damage = 7;
	return ReimuAConfigureStatus::complete;
}

} // namespace detail

[[nodiscard]] constexpr ReimuAPatternResult shot_reimu_a_l7(
	ReimuAPatternState &pattern,
	ShotSpawnState &spawn,
	ShotState &shots,
	pc98::th0405::RandomState &random
) noexcept
{
	const auto count = detail::reimu_a_cycle_count(pattern, spawn, 5, 2U);
	auto angle = std::uint8_t{0xBAU};
	return detail::reimu_a_run(spawn, shots, count,
		[&](Shot &shot, int remaining) constexpr {
			if(remaining <= 3) {
				detail::reimu_a_set_velocity(shot, angle);
				shot.patnum_base = pat_shot_reimu;
				// The original writes 4 and immediately overwrites it with 7.
				shot.damage = 4;
				shot.damage = 7;
				angle = static_cast<std::uint8_t>(angle + 6U);
				return detail::ReimuAConfigureStatus::complete;
			}
			if(remaining <= 5) return detail::reimu_a_mid(shot, remaining);
			return detail::reimu_a_option(shot, remaining, 7, 7, pattern, spawn, random);
		}
	);
}

[[nodiscard]] constexpr ReimuAPatternResult shot_reimu_a_l8(
	ReimuAPatternState &pattern,
	ShotSpawnState &spawn,
	ShotState &shots,
	pc98::th0405::RandomState &random
) noexcept
{
	pattern.cycle = static_cast<std::uint8_t>(pattern.cycle + 1U);
	auto angle = std::uint8_t{0xBAU};
	return detail::reimu_a_run(spawn, shots, 7,
		[&](Shot &shot, int remaining) constexpr {
			if(remaining <= 3) {
				detail::reimu_a_set_velocity(shot, angle);
				shot.patnum_base = pat_shot_reimu;
				shot.damage = 7;
				angle = static_cast<std::uint8_t>(angle + 6U);
				return detail::ReimuAConfigureStatus::complete;
			}
			if(remaining <= 5) return detail::reimu_a_mid(shot, remaining);
			return detail::reimu_a_option(shot, remaining, 7, 7, pattern, spawn, random);
		}
	);
}

[[nodiscard]] constexpr ReimuAPatternResult shot_reimu_a_l9(
	ReimuAPatternState &pattern,
	ShotSpawnState &spawn,
	ShotState &shots,
	pc98::th0405::RandomState &random
) noexcept
{
	int count = 7;
	if(spawn.shot_time == 18U) pattern.cycle = 0;
	if((pattern.cycle % 2U) == 0U) count += 2;
	pattern.cycle = static_cast<std::uint8_t>(pattern.cycle + 1U);
	auto angle = std::uint8_t{0xBAU};
	return detail::reimu_a_run(spawn, shots, count,
		[&](Shot &shot, int remaining) constexpr {
			if(remaining <= 3) {
				detail::reimu_a_set_velocity(shot, angle);
				shot.patnum_base = pat_shot_reimu;
				shot.damage = 6;
				angle = static_cast<std::uint8_t>(angle + 6U);
				return detail::ReimuAConfigureStatus::complete;
			}
			if(remaining <= 5) return detail::reimu_a_mid(shot, remaining);
			if(remaining <= 7) {
				const auto left = remaining == 7;
				detail::reimu_a_from_option(shot, left);
				if(pattern.homing_target.y == homing_target_none) {
					detail::reimu_a_set_velocity(shot, left ? 0xB4U : 0xCCU);
				} else {
					const auto random_offset = pc98::th0405::randring_next16_and(random, 7U);
					if(!random_offset) return detail::ReimuAConfigureStatus::random_refused;
					const auto status = detail::reimu_a_set_homing_velocity(
						shot,
						pattern,
						spawn.player_cur,
						static_cast<std::uint8_t>(static_cast<std::uint16_t>(*random_offset) - 4U)
					);
					if(status != detail::ReimuAConfigureStatus::complete) return status;
				}
				shot.patnum_base = pat_shot_reimu_sub_a;
				shot.damage = 5;
				return detail::ReimuAConfigureStatus::complete;
			}
			const auto left = remaining == 9;
			detail::reimu_a_from_option(shot, left);
			if(pattern.homing_target.y != homing_target_none) {
				const auto status = detail::reimu_a_set_homing_velocity(
					shot, pattern, spawn.player_cur, 0U
				);
				if(status != detail::ReimuAConfigureStatus::complete) return status;
			}
			shot.patnum_base = pat_shot_reimu_sub_a;
			shot.damage = 7;
			return detail::ReimuAConfigureStatus::complete;
		}
	);
}

} // namespace th04::core
