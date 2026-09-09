#include "th04/core/shot_patterns.hpp"

#include "pc98/masterlib_vector.hpp"

#include <array>
#include <bit>
#include <cstdint>

namespace th04::core {
namespace {

constexpr std::int16_t pat_shot_reimu_sub_b = 32;
constexpr std::int16_t pat_shot_marisa_sub_b = 36;
constexpr std::int16_t pat_shot_laser_ring = 70;
constexpr std::int16_t player_fan_distance = 128;
constexpr std::int16_t option_shot_speed = -256;

[[nodiscard]] std::int16_t word_add(std::int16_t left, std::int16_t right) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(left) + right
	));
}

[[nodiscard]] std::int16_t word_sub(std::int16_t left, std::int16_t right) noexcept
{
	return word_add(left, std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		0U - std::bit_cast<std::uint16_t>(right)
	)));
}

void set_velocity(Shot &shot, std::uint8_t angle, std::int16_t length = 192) noexcept
{
	const auto velocity = pc98::masterlib::vector2(angle, length);
	shot.pos.velocity = {velocity.x, velocity.y};
}

[[nodiscard]] Shot *next_shot(
	ShotSpawnState &spawn, ShotState &shots, ShotPatternResult &result
) noexcept
{
	result.allocation = shots_add(spawn, shots);
	if(!result.allocation.allocated()) {
		if(result.allocation.status != ShotAddStatus::no_slot) {
			result.status = ShotPatternStatus::allocation_boundary;
		}
		return nullptr;
	}
	auto *const shot = detail::shot_spawn_record(spawn, shots, result.allocation.record_index);
	if(shot == nullptr) {
		result.status = ShotPatternStatus::allocation_boundary;
		return nullptr;
	}
	result.shots_allocated = static_cast<std::uint8_t>(result.shots_allocated + 1U);
	return shot;
}

void reimu_b_cycle(
	ReimuAPatternState &pattern, const ShotSpawnState &spawn,
	int &count, std::uint8_t divisor, int secondary
) noexcept
{
	if(spawn.shot_time == 18U) pattern.cycle = 0;
	if((pattern.cycle % divisor) == 0U) count += secondary;
	pattern.cycle = static_cast<std::uint8_t>(pattern.cycle + 1U);
}

void reimu_b_option(Shot &shot, bool left, std::uint8_t angle, std::int8_t damage) noexcept
{
	shot.pos.cur.x = word_add(shot.pos.cur.x, left ? -player_option_distance : player_option_distance);
	set_velocity(shot, angle);
	shot.patnum_base = pat_shot_reimu_sub_b;
	shot.damage = damage;
}

[[nodiscard]] ShotPatternResult marisa_a_laser_update(
	ShotSpawnState &spawn,
	ShotState &shots,
	pc98::th0405::SubpixelPoint option_cur,
	std::uint16_t frames,
	std::uint8_t style
) noexcept
{
	ShotPatternResult result{};
	if(shots.shot_laser_time == 0U) {
		shots.shot_laser_time = frames;
		shots.shot_laser_style = style;
		shots.shot_laser_bottomcenter.cur = option_cur;
		shots.shot_laser_bottomcenter.prev = option_cur;
		shots.shot_laser_ring_cycle = 0;
	}
	if(shots.shot_laser_time < 48U) return result;

	shots.shot_laser_ring_cycle = static_cast<std::uint8_t>(
		shots.shot_laser_ring_cycle + 1U
	);
	if(shots.shot_laser_ring_cycle > 4U) {
		if(shots.shot_laser_ring_cycle >= 8U) shots.shot_laser_ring_cycle = 0;
		return result;
	}

	reset_shot_allocator(spawn);
	for(const auto offset : std::array<std::int16_t, 2>{
		-player_option_distance, player_option_distance
	}) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		shot->patnum_base = pat_shot_laser_ring;
		shot->damage = 9;
		shot->pos.velocity.y = option_shot_speed;
		shot->pos.cur.x = word_add(option_cur.x, offset);
	}
	return result;
}

[[nodiscard]] ShotPatternResult merge_laser_result(
	ShotPatternResult laser, ShotPatternResult volley
) noexcept
{
	volley.shots_allocated = static_cast<std::uint8_t>(
		laser.shots_allocated + volley.shots_allocated
	);
	return volley;
}

[[nodiscard]] ShotPatternResult marisa_a_pair(
	ShotSpawnState &spawn, ShotState &shots, int count, std::int8_t damage
) noexcept
{
	ShotPatternResult result{};
	reset_shot_allocator(spawn);
	for(auto remaining = count; remaining > 0; --remaining) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		shot->pos.cur.x = word_add(
			shot->pos.cur.x, remaining == 2 ? -player_fan_distance : player_fan_distance
		);
		shot->patnum_base = pat_shot_marisa;
		shot->damage = damage;
	}
	return result;
}

[[nodiscard]] ShotPatternResult marisa_a_fan(
	ShotSpawnState &spawn, ShotState &shots, int count, std::uint8_t angle, std::uint8_t step,
	std::int8_t damage
) noexcept
{
	ShotPatternResult result{};
	reset_shot_allocator(spawn);
	for(auto remaining = count; remaining > 0; --remaining) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		set_velocity(*shot, angle);
		shot->patnum_base = pat_shot_marisa;
		shot->damage = damage;
		angle = static_cast<std::uint8_t>(angle + step);
	}
	return result;
}

void marisa_b_option_straight(Shot &shot, std::int16_t offset, std::int8_t damage) noexcept
{
	shot.pos.cur.x = word_sub(shot.pos.cur.x, offset);
	shot.patnum_base = pat_shot_marisa_sub_b;
	shot.pos.velocity.y = option_shot_speed;
	shot.damage = damage;
}

[[nodiscard]] bool marisa_b_random_option(
	Shot &shot, RandomState &random, ShotPatternResult &result, std::int16_t offset,
	std::int8_t damage, bool sprite_before_random
) noexcept
{
	shot.pos.cur.x = word_add(shot.pos.cur.x, offset);
	if(sprite_before_random) shot.patnum_base = pat_shot_marisa_sub_b;
	const auto random_angle = randring_next16_and(random, 7U);
	if(!random_angle) {
		result.status = ShotPatternStatus::random_refused;
		return false;
	}
	set_velocity(shot, static_cast<std::uint8_t>(0xBCU + *random_angle), 256);
	if(!sprite_before_random) shot.patnum_base = pat_shot_marisa_sub_b;
	shot.damage = damage;
	return true;
}

[[nodiscard]] ShotPatternResult marisa_b_l67(
	ShotSpawnState &spawn, ShotState &shots, std::uint8_t angle, std::uint8_t step,
	std::int8_t own_damage
) noexcept
{
	ShotPatternResult result{};
	reset_shot_allocator(spawn);
	for(auto remaining = 7; remaining > 0; --remaining) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining <= 3) {
			shot->patnum_base = pat_shot_marisa;
			set_velocity(*shot, angle);
			shot->damage = own_damage;
			angle = static_cast<std::uint8_t>(angle + step);
			continue;
		}
		const auto offset = static_cast<std::int16_t>(
			remaining == 7 ? -512 : remaining == 6 ? -256 : remaining == 5 ? 512 : 256
		);
		marisa_b_option_straight(*shot, offset, 5);
	}
	return result;
}

[[nodiscard]] ShotPatternResult marisa_b_l89(
	ShotSpawnState &spawn, ShotState &shots, int count, std::int8_t option_damage
) noexcept
{
	ShotPatternResult result{};
	auto angle = std::uint8_t{0xB6U};
	reset_shot_allocator(spawn);
	for(auto remaining = count; remaining > 0; --remaining) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining <= 4) {
			if(remaining == 3) shot->pos.cur.x = word_add(shot->pos.cur.x, -player_fan_distance);
			if(remaining == 2) shot->pos.cur.x = word_add(shot->pos.cur.x, player_fan_distance);
			shot->patnum_base = pat_shot_marisa;
			set_velocity(*shot, angle);
			shot->damage = 8;
			if(remaining != 3) angle = static_cast<std::uint8_t>(angle + 10U);
			continue;
		}
		std::int16_t offset{};
		if(count == 8) {
			offset = static_cast<std::int16_t>(
				remaining == 8 ? -512 : remaining == 7 ? -256 : remaining == 6 ? 512 : 256
			);
		} else {
			offset = static_cast<std::int16_t>(
				remaining == 10 ? -768 : remaining == 9 ? -512 : remaining == 8 ? -256
				: remaining == 7 ? 768 : remaining == 6 ? 512 : 256
			);
		}
		marisa_b_option_straight(*shot, offset, option_damage);
	}
	return result;
}

} // namespace

ShotPatternResult shot_reimu_b_l2(
	ReimuAPatternState &pattern, ShotSpawnState &spawn, ShotState &shots, RandomState &random
) noexcept
{
	int remaining = 1;
	reimu_b_cycle(pattern, spawn, remaining, 3U, 2);
	reset_shot_allocator(spawn);
	ShotPatternResult result{};
	while(remaining > 0) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining == 1) {
			const auto random_angle = randring_next16_and(random, 15U);
			if(!random_angle) {
				result.status = ShotPatternStatus::random_refused;
				return result;
			}
			set_velocity(*shot, static_cast<std::uint8_t>(0xB8U + *random_angle));
			shot->patnum_base = pat_shot_reimu;
		} else {
			reimu_b_option(*shot, remaining == 3, remaining == 3 ? 0xB8U : 0xC8U, 10);
		}
		shot->damage = 10;
		--remaining;
	}
	return result;
}

ShotPatternResult shot_reimu_b_l3(
	ReimuAPatternState &pattern, ShotSpawnState &spawn, ShotState &shots
) noexcept
{
	int remaining = 2;
	reimu_b_cycle(pattern, spawn, remaining, 3U, 2);
	reset_shot_allocator(spawn);
	ShotPatternResult result{};
	while(remaining > 0) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining <= 2) {
			shot->pos.cur.x = word_add(
				shot->pos.cur.x, remaining == 2 ? -player_fan_distance : player_fan_distance
			);
			shot->patnum_base = pat_shot_reimu;
		} else {
			reimu_b_option(*shot, remaining == 4, remaining == 4 ? 0xB8U : 0xC8U, 9);
		}
		shot->damage = 9;
		--remaining;
	}
	return result;
}

ShotPatternResult shot_reimu_b_l4(
	ReimuAPatternState &pattern, ShotSpawnState &spawn, ShotState &shots
) noexcept
{
	int remaining = 3;
	reimu_b_cycle(pattern, spawn, remaining, 2U, 2);
	reset_shot_allocator(spawn);
	ShotPatternResult result{};
	auto angle = std::uint8_t{0xBAU};
	while(remaining > 0) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining <= 3) {
			set_velocity(*shot, angle);
			shot->patnum_base = pat_shot_reimu;
			shot->damage = 9;
			angle = static_cast<std::uint8_t>(angle + 6U);
		} else {
			reimu_b_option(*shot, remaining == 5, remaining == 5 ? 0xB8U : 0xC8U, 9);
		}
		--remaining;
	}
	return result;
}

ShotPatternResult shot_reimu_b_l5(
	ReimuAPatternState &pattern, ShotSpawnState &spawn, ShotState &shots
) noexcept
{
	int remaining = 3;
	reimu_b_cycle(pattern, spawn, remaining, 2U, 4);
	reset_shot_allocator(spawn);
	ShotPatternResult result{};
	auto angle = std::uint8_t{0xBAU};
	while(remaining > 0) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining <= 3) {
			set_velocity(*shot, angle);
			shot->patnum_base = pat_shot_reimu;
			shot->damage = 8;
			angle = static_cast<std::uint8_t>(angle + 7U);
		} else {
			const auto option_angle = static_cast<std::uint8_t>(
				remaining == 7 ? 0xB2U : remaining == 6 ? 0xB9U
				: remaining == 5 ? 0xCEU : 0xC7U
			);
			reimu_b_option(*shot, remaining >= 6, option_angle, 9);
		}
		--remaining;
	}
	return result;
}

ShotPatternResult shot_reimu_b_l6(
	ReimuAPatternState &pattern, ShotSpawnState &spawn, ShotState &shots
) noexcept
{
	int remaining = 3;
	reimu_b_cycle(pattern, spawn, remaining, 2U, 4);
	reset_shot_allocator(spawn);
	ShotPatternResult result{};
	auto angle = std::uint8_t{0xBAU};
	while(remaining > 0) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining <= 3) {
			set_velocity(*shot, angle);
			shot->patnum_base = pat_shot_reimu;
			shot->damage = 8;
			angle = static_cast<std::uint8_t>(angle + 6U);
		} else {
			const auto option_angle = static_cast<std::uint8_t>(
				remaining == 7 ? 0xB2U : remaining == 6 ? 0xB9U
				: remaining == 5 ? 0xCEU : 0xC7U
			);
			reimu_b_option(*shot, remaining >= 6, option_angle, 9);
		}
		--remaining;
	}
	return result;
}

ShotPatternResult shot_reimu_b_l7(
	ReimuAPatternState &pattern, ShotSpawnState &spawn, ShotState &shots
) noexcept
{
	return shot_reimu_b_l6(pattern, spawn, shots);
}

ShotPatternResult shot_reimu_b_l8(
	ReimuAPatternState &pattern, ShotSpawnState &spawn, ShotState &shots
) noexcept
{
	int remaining = 5;
	reimu_b_cycle(pattern, spawn, remaining, 2U, 4);
	reset_shot_allocator(spawn);
	ShotPatternResult result{};
	auto angle = std::uint8_t{0xB8U};
	while(remaining > 0) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining <= 5) {
			set_velocity(*shot, angle);
			shot->patnum_base = pat_shot_reimu;
			shot->damage = 8;
			angle = static_cast<std::uint8_t>(angle + 4U);
		} else {
			const auto option_angle = static_cast<std::uint8_t>(
				remaining == 9 ? 0xB2U : remaining == 8 ? 0xB9U
				: remaining == 7 ? 0xCEU : 0xC7U
			);
			reimu_b_option(*shot, remaining >= 8, option_angle, 9);
		}
		--remaining;
	}
	return result;
}

ShotPatternResult shot_reimu_b_l9(
	ReimuAPatternState &pattern, ShotSpawnState &spawn, ShotState &shots
) noexcept
{
	int remaining = 7;
	reimu_b_cycle(pattern, spawn, remaining, 2U, 4);
	reset_shot_allocator(spawn);
	ShotPatternResult result{};
	auto angle = std::uint8_t{0xB8U};
	while(remaining > 0) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining <= 5) {
			set_velocity(*shot, angle);
			shot->patnum_base = pat_shot_reimu;
			shot->damage = 8;
			angle = static_cast<std::uint8_t>(angle + 4U);
		} else {
			const auto option_angle = static_cast<std::uint8_t>(
				remaining >= 10 ? 0xC0U : remaining == 9 ? 0xACU
				: remaining == 8 ? 0xD4U : remaining == 7 ? 0xB6U : 0xCAU
			);
			reimu_b_option(*shot, (remaining & 1) != 0, option_angle, 9);
		}
		--remaining;
	}
	return result;
}

ShotPatternResult shot_marisa_a_l2(
	ShotSpawnState &spawn, ShotState &shots, RandomState &random, pc98::th0405::SubpixelPoint option_cur
) noexcept
{
	auto laser = marisa_a_laser_update(spawn, shots, option_cur, 64U, 0U);
	if(!laser.complete()) return laser;
	ShotPatternResult volley{};
	reset_shot_allocator(spawn);
	auto *const shot = next_shot(spawn, shots, volley);
	if(shot == nullptr) return merge_laser_result(laser, volley);
	const auto random_angle = randring_next16_and(random, 7U);
	if(!random_angle) {
		volley.status = ShotPatternStatus::random_refused;
		return merge_laser_result(laser, volley);
	}
	set_velocity(*shot, static_cast<std::uint8_t>(0xBCU + *random_angle));
	shot->patnum_base = pat_shot_marisa;
	shot->damage = 9;
	return merge_laser_result(laser, volley);
}

ShotPatternResult shot_marisa_a_l3(
	ShotSpawnState &spawn, ShotState &shots, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept
{
	auto laser = marisa_a_laser_update(spawn, shots, option_cur, 72U, 0U);
	if(!laser.complete()) return laser;
	return merge_laser_result(laser, marisa_a_pair(spawn, shots, 2, 9));
}

ShotPatternResult shot_marisa_a_l4(
	ShotSpawnState &spawn, ShotState &shots, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept
{
	auto laser = marisa_a_laser_update(spawn, shots, option_cur, 88U, 1U);
	if(!laser.complete()) return laser;
	return merge_laser_result(laser, marisa_a_pair(spawn, shots, 2, 9));
}

ShotPatternResult shot_marisa_a_l5(
	ShotSpawnState &spawn, ShotState &shots, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept
{
	auto laser = marisa_a_laser_update(spawn, shots, option_cur, 104U, 1U);
	if(!laser.complete()) return laser;
	return merge_laser_result(laser, marisa_a_fan(spawn, shots, 3, 0xB8U, 8U, 8));
}

ShotPatternResult shot_marisa_a_l6(
	ShotSpawnState &spawn, ShotState &shots, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept
{
	auto laser = marisa_a_laser_update(spawn, shots, option_cur, 128U, 2U);
	if(!laser.complete()) return laser;
	return merge_laser_result(laser, marisa_a_fan(spawn, shots, 3, 0xB8U, 8U, 8));
}

ShotPatternResult shot_marisa_a_l7(
	ShotSpawnState &spawn, ShotState &shots, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept
{
	auto laser = marisa_a_laser_update(spawn, shots, option_cur, 144U, 3U);
	if(!laser.complete()) return laser;
	return merge_laser_result(laser, marisa_a_fan(spawn, shots, 3, 0xB8U, 8U, 8));
}

ShotPatternResult shot_marisa_a_l8(
	ShotSpawnState &spawn, ShotState &shots, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept
{
	auto laser = marisa_a_laser_update(spawn, shots, option_cur, 168U, 3U);
	if(!laser.complete()) return laser;
	return merge_laser_result(laser, marisa_a_fan(spawn, shots, 5, 0xB4U, 6U, 7));
}

ShotPatternResult shot_marisa_a_l9(
	ShotSpawnState &spawn, ShotState &shots, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept
{
	auto laser = marisa_a_laser_update(spawn, shots, option_cur, 192U, 4U);
	if(!laser.complete()) return laser;
	return merge_laser_result(laser, marisa_a_fan(spawn, shots, 5, 0xB4U, 6U, 7));
}

ShotPatternResult shot_marisa_b_l2(
	ShotSpawnState &spawn, ShotState &shots, RandomState &random
) noexcept
{
	ShotPatternResult result{};
	reset_shot_allocator(spawn);
	for(auto remaining = 3; remaining > 0; --remaining) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining <= 1) {
			const auto random_angle = randring_next16_and(random, 7U);
			if(!random_angle) {
				result.status = ShotPatternStatus::random_refused;
				return result;
			}
			shot->patnum_base = pat_shot_marisa;
			set_velocity(*shot, static_cast<std::uint8_t>(0xBCU + *random_angle));
			shot->damage = 10;
		} else {
			shot->pos.cur.x = word_add(
				shot->pos.cur.x, remaining == 3 ? -player_option_distance : player_option_distance
			);
			shot->patnum_base = pat_shot_marisa_sub_b;
			shot->pos.velocity.y = option_shot_speed;
			shot->damage = 6;
		}
	}
	return result;
}

ShotPatternResult shot_marisa_b_l3(ShotSpawnState &spawn, ShotState &shots) noexcept
{
	ShotPatternResult result{};
	reset_shot_allocator(spawn);
	for(auto remaining = 4; remaining > 0; --remaining) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining <= 2) {
			shot->pos.cur.x = word_add(
				shot->pos.cur.x, remaining == 2 ? -player_fan_distance : player_fan_distance
			);
			shot->patnum_base = pat_shot_marisa;
			shot->damage = 9;
		} else {
			shot->pos.cur.x = word_add(
				shot->pos.cur.x, remaining == 4 ? -player_option_distance : player_option_distance
			);
			shot->patnum_base = pat_shot_marisa_sub_b;
			shot->pos.velocity.y = option_shot_speed;
			shot->damage = 6;
		}
	}
	return result;
}

ShotPatternResult shot_marisa_b_l4(
	ShotSpawnState &spawn, ShotState &shots, RandomState &random
) noexcept
{
	ShotPatternResult result{};
	reset_shot_allocator(spawn);
	for(auto remaining = 4; remaining > 0; --remaining) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining <= 2) {
			shot->pos.cur.x = word_add(
				shot->pos.cur.x, remaining == 2 ? -player_fan_distance : player_fan_distance
			);
			shot->patnum_base = pat_shot_marisa;
			shot->damage = 9;
		} else if(!marisa_b_random_option(
			*shot, random, result,
			remaining == 4 ? -player_option_distance : player_option_distance, 6, true
		)) {
			return result;
		}
	}
	return result;
}

ShotPatternResult shot_marisa_b_l5(
	ShotSpawnState &spawn, ShotState &shots, RandomState &random
) noexcept
{
	ShotPatternResult result{};
	reset_shot_allocator(spawn);
	auto angle = std::uint8_t{0xB8U};
	for(auto remaining = 5; remaining > 0; --remaining) {
		auto *const shot = next_shot(spawn, shots, result);
		if(shot == nullptr) return result;
		if(remaining <= 3) {
			shot->patnum_base = pat_shot_marisa;
			set_velocity(*shot, angle);
			shot->damage = 9;
			angle = static_cast<std::uint8_t>(angle + 8U);
		} else if(!marisa_b_random_option(
			*shot, random, result,
			remaining == 5 ? -player_option_distance : player_option_distance, 5, false
		)) {
			return result;
		}
	}
	return result;
}

ShotPatternResult shot_marisa_b_l6(ShotSpawnState &spawn, ShotState &shots) noexcept
{
	return marisa_b_l67(spawn, shots, 0xB8U, 8U, 9);
}

ShotPatternResult shot_marisa_b_l7(ShotSpawnState &spawn, ShotState &shots) noexcept
{
	return marisa_b_l67(spawn, shots, 0xB6U, 10U, 8);
}

ShotPatternResult shot_marisa_b_l8(ShotSpawnState &spawn, ShotState &shots) noexcept
{
	return marisa_b_l89(spawn, shots, 8, 5);
}

ShotPatternResult shot_marisa_b_l9(ShotSpawnState &spawn, ShotState &shots) noexcept
{
	return marisa_b_l89(spawn, shots, 10, 4);
}

} // namespace th04::core
