#pragma once

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_motion.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace th05::core {

inline constexpr std::size_t shot_count = 64;
inline constexpr std::size_t hitshot_count = 24;
inline constexpr std::uint8_t shot_free = 0;
inline constexpr std::uint8_t shot_alive = 1;
inline constexpr std::uint8_t shot_remove = 2;
inline constexpr std::uint8_t shot_normal = 0;
inline constexpr std::uint8_t shot_homing = 1;
inline constexpr std::uint8_t shot_missile_left = 2;
inline constexpr std::uint8_t shot_missile_right = 3;
inline constexpr std::uint8_t shot_missile_straight = 4;
inline constexpr std::uint8_t hitshot_cels = 4;
inline constexpr std::uint8_t hitshot_frames_per_cel = 3;
inline constexpr std::uint8_t hitshot_frames =
	hitshot_cels * hitshot_frames_per_cel;
inline constexpr std::int16_t homing_target_none = -15984;

// TH05's byte fields differ from TH04's word sprite index. Raw byte flags
// retain nonstandard states rather than introducing an enum validity filter.
struct Shot {
	std::uint8_t flag{}, age{};
	pc98::th0405::Motion pos{};
	std::uint8_t patnum_base{}, type{};
	std::int8_t damage{};
	std::uint8_t angle{};
	bool operator==(const Shot &) const = default;
};

struct HitShot {
	std::uint8_t age{}, patnum{};
	pc98::th0405::Motion pos{};
	bool operator==(const HitShot &) const = default;
};

static_assert(sizeof(Shot) == 18);
static_assert(offsetof(Shot, pos) == 2);
static_assert(offsetof(Shot, patnum_base) == 14);
static_assert(offsetof(Shot, type) == 15);
static_assert(offsetof(Shot, damage) == 16);
static_assert(offsetof(Shot, angle) == 17);
static_assert(sizeof(HitShot) == 14);
static_assert(offsetof(HitShot, pos) == 2);

struct HitShotState {
	std::array<HitShot, hitshot_count> hitshots{};
	std::uint16_t next_free_id{};
	bool operator==(const HitShotState &) const = default;
};

struct ShotAlive {
	pc98::th0405::SubpixelPoint pos{};
	// Stable pool index replaces the original near pointer, keeping copied host
	// state self-contained while denoting the same slot in `shots`.
	std::uint16_t shot_index{};
	bool operator==(const ShotAlive &) const = default;
};

struct ShotUpdateState {
	std::array<Shot, shot_count> shots{};
	HitShotState hitshot_state{};
	std::array<ShotAlive, shot_count> shots_alive{};
	std::uint16_t shots_alive_count{};
	pc98::th0405::SubpixelPoint homing_target{};
	bool operator==(const ShotUpdateState &) const = default;
};

enum class ShotUpdateResult : std::uint8_t {
	complete,
	iatan2_fault,
};

// th05/p_common.cpp::hitshot_from: advance exactly one ring slot and remove
// the original shot even if that animation slot is occupied. Do not search
// for another free slot. Invalid native cursor values refuse before mutation.
[[nodiscard]] constexpr bool hitshot_from(HitShotState &state, Shot &shot) noexcept
{
	if(state.next_free_id >= hitshot_count) return false;
	auto &hitshot = state.hitshots[state.next_free_id];
	if(state.next_free_id < hitshot_count - 1) {
		++state.next_free_id;
	} else {
		state.next_free_id = 0;
	}
	shot.flag = shot_remove;
	if(hitshot.age != 0) return true;
	hitshot.age = 1;
	hitshot.patnum = shot.patnum_base == 20 ? 28 : 32;
	hitshot.pos.cur = shot.pos.cur;
	hitshot.pos.prev = shot.pos.prev;
	hitshot.pos.velocity.x = static_cast<std::int16_t>(shot.pos.velocity.x / 6);
	hitshot.pos.velocity.y = static_cast<std::int16_t>(shot.pos.velocity.y / 6);
	return true;
}

// th04/main/player/shot_velocity.asm. TH05's source function writes the
// owning Shot::angle via its velocity pointer before storing the Q12.4 vector;
// the native model represents that storage relation directly.
constexpr void shot_velocity_set(Shot &shot, std::uint8_t angle) noexcept
{
	shot.angle = angle;
	const auto velocity = pc98::masterlib::vector2(angle, 192);
	shot.pos.velocity = {velocity.x, velocity.y};
}

namespace detail {
constexpr std::int16_t word_add(std::int16_t left, std::int16_t right) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(left)) +
		std::bit_cast<std::uint16_t>(right)
	));
}

constexpr std::int16_t word_subtract(std::int16_t left, std::int16_t right) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(left)) -
		std::bit_cast<std::uint16_t>(right)
	));
}

constexpr bool shot_in_update_bounds(pc98::th0405::SubpixelPoint cur) noexcept
{
	// th05/p_common.cpp: strict (-8, 392) × (-8, 376) pixel bounds.
	return cur.x > -128 && cur.x < 6272 && cur.y > -128 && cur.y < 6016;
}
} // namespace detail

// th05/p_common.cpp::sub_1240B. This is the complete local update body, not
// allocation, collision scheduling, rendering, or whole-frame orchestration.
// An iatan2 source DIV fault stops at the exact homing call, preserving all
// earlier writes in this invocation and leaving later shots/hitshots untouched.
[[nodiscard]] constexpr ShotUpdateResult shots_update(ShotUpdateState &state) noexcept
{
	state.shots_alive_count = 0;
	for(std::size_t i = 0; i < state.shots.size(); ++i) {
		auto &shot = state.shots[i];
		if(shot.flag == shot_remove) {
			shot.flag = shot_free;
			continue;
		}
		if(shot.flag == shot_free) continue;
		if(shot.flag == shot_alive) {
			switch(shot.type) {
			case shot_homing: {
				if(shot.age >= 16U) shot.type = shot_normal;
				if(state.homing_target.x == homing_target_none) break;
				const auto aim_y = detail::word_subtract(
					state.homing_target.y, shot.pos.cur.y
				);
				const auto aim_x = detail::word_subtract(
					state.homing_target.x, shot.pos.cur.x
				);
				const auto target_angle = pc98::masterlib::iatan2_source(
					aim_y, aim_x
				);
				if(!target_angle) return ShotUpdateResult::iatan2_fault;
				const auto delta = static_cast<std::uint8_t>(shot.angle - *target_angle);
				if(delta >= 128U) {
					const auto turn = static_cast<std::uint8_t>((256U - delta) / 4U);
					if(turn < 3U) {
						const auto snap_angle = pc98::masterlib::iatan2_source(aim_y, aim_x);
						if(!snap_angle) return ShotUpdateResult::iatan2_fault;
						shot.angle = *snap_angle;
					} else {
						shot.angle = static_cast<std::uint8_t>(shot.angle + turn);
					}
				} else {
					const auto turn = static_cast<std::uint8_t>(delta / 4U);
					if(turn < 3U) {
						const auto snap_angle = pc98::masterlib::iatan2_source(aim_y, aim_x);
						if(!snap_angle) return ShotUpdateResult::iatan2_fault;
						shot.angle = *snap_angle;
					} else {
						shot.angle = static_cast<std::uint8_t>(shot.angle - turn);
					}
				}
				shot_velocity_set(shot, shot.angle);
				break;
			}
			case shot_missile_left:
				shot.pos.velocity.x = detail::word_add(shot.pos.velocity.x, 1);
				shot.pos.velocity.y = detail::word_subtract(shot.pos.velocity.y, 4);
				break;
			case shot_missile_right:
				shot.pos.velocity.x = detail::word_subtract(shot.pos.velocity.x, 1);
				shot.pos.velocity.y = detail::word_subtract(shot.pos.velocity.y, 4);
				break;
			case shot_missile_straight:
				shot.pos.velocity.y = detail::word_subtract(shot.pos.velocity.y, 4);
				break;
			default:
				break;
			}
		}

		const auto cur = pc98::th0405::update_motion(shot.pos);
		if(!detail::shot_in_update_bounds(cur)) {
			shot.flag = shot_remove;
			continue;
		}
		state.shots_alive[state.shots_alive_count++] = {
			cur, static_cast<std::uint16_t>(i)
		};
		shot.age = static_cast<std::uint8_t>(shot.age + 1U);
	}

	for(auto &hitshot : state.hitshot_state.hitshots) {
		if(hitshot.age >= static_cast<std::uint8_t>(hitshot_frames + 1U)) {
			hitshot.age = 0;
		}
		if(hitshot.age == 0) continue;
		const auto cur = pc98::th0405::update_motion(hitshot.pos);
		if(!detail::shot_in_update_bounds(cur)) {
			hitshot.age = static_cast<std::uint8_t>(
				hitshot_frames + hitshot_cels + 2U
			);
			continue;
		}
		hitshot.age = static_cast<std::uint8_t>(hitshot.age + 1U);
		if((hitshot.age % hitshot_frames_per_cel) == 1U) {
			hitshot.patnum = static_cast<std::uint8_t>(hitshot.patnum + 1U);
		}
	}
	return ShotUpdateResult::complete;
}

} // namespace th05::core
