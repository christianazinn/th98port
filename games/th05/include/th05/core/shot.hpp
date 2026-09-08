#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th05::core {

inline constexpr std::size_t shot_count = 64;
inline constexpr std::size_t hitshot_count = 24;
inline constexpr std::uint8_t shot_free = 0;
inline constexpr std::uint8_t shot_alive = 1;
inline constexpr std::uint8_t shot_remove = 2;

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

} // namespace th05::core
