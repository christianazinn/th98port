#pragma once

#include "th05/core/random.hpp"
#include "th05/core/shot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace th05::core {

inline constexpr std::size_t shot_spawn_guard_count = 4;
inline constexpr std::uint16_t shot_spawn_first_guard =
	static_cast<std::uint16_t>(shot_count);
inline constexpr std::uint8_t shot_cycle_1x = 0x8;
inline constexpr std::uint8_t shot_cycle_2x = 0x2;
inline constexpr std::uint8_t shot_cycle_3x = 0x1;
inline constexpr std::uint8_t shot_cycle_6x = 0x4;

// th05_main.asm places 72 bytes between shots[64] and hitshots. The source
// allocator can reach only the first record after the active pool, but all
// four records remain typed persistent storage and are never an alias for
// hitshots or the update cache.
using ShotSpawnGuard = std::array<Shot, shot_spawn_guard_count>;
static_assert(sizeof(ShotSpawnGuard) == 72);

// Spawn-owned state. The existing update pool and shared TH04/05 random state
// stay borrowed function arguments, so copying this value neither aliases
// another snapshot's active shots nor forks its random stream. `shot_cursor`
// expresses the original near pointer as a source-record offset: 0..64 are
// normal states, 65 is a successful guard allocation, and larger host values
// are rejected before mutation. The legacy byte is reset but not read here.
struct ShotSpawnState {
	ShotSpawnGuard guard{};
	pc98::th0405::SubpixelPoint player_cur{};
	std::uint16_t shot_cursor{};
	std::uint8_t shot_last_id{};
	std::uint8_t shot_time{};
	bool operator==(const ShotSpawnState &) const = default;
};

enum class ShotAddStatus : std::uint8_t {
	active_slot,
	guard_slot,
	no_slot,
	invalid_cursor,
};

// `shot` is non-null exactly for the two source-reachable allocation states.
// `record_index` is 0..63 for an active shot or 64 for the first guard record.
struct ShotAddResult {
	ShotAddStatus status{ShotAddStatus::no_slot};
	Shot *shot{};
	std::uint16_t record_index{0xFFFFU};

	[[nodiscard]] constexpr bool allocated() const noexcept
	{
		return shot != nullptr;
	}
};

struct ShotCycleResult {
	std::uint8_t flags{};
	bool sound_1{};
};

// Results expose bounded-model diagnostics; the original L0/L1 functions
// return void. A random refusal happens after a successful allocation, just
// as the source executes allocation stores before its random-ring call.
struct ShotFireResult {
	ShotCycleResult cycle{};
	ShotAddResult allocation{};
	bool random_refused{};
};

namespace detail {

constexpr void shot_spawn_initialize(
	Shot &shot, pc98::th0405::SubpixelPoint player_cur
) noexcept
{
	// th05/main/player/shots_add.cpp writes these fields only. In particular,
	// `pos.prev`, damage, and angle retain their previous storage contents.
	shot.flag = shot_alive;
	shot.age = 0;
	shot.pos.cur = player_cur;
	shot.pos.velocity = {0, -192};
	shot.patnum_base = 20;
	shot.type = shot_normal;
}

[[nodiscard]] constexpr Shot *shot_spawn_record(
	ShotSpawnState &state, ShotUpdateState &update, std::uint16_t record_index
) noexcept
{
	if(record_index < shot_count) return &update.shots[record_index];
	if(record_index == shot_spawn_first_guard) return &state.guard[0];
	return nullptr;
}

} // namespace detail

// th05/main/player/shots_add.cpp. The bound check intentionally precedes the
// increment, and a skipped occupied record stays only in the source's local
// cursor; `shot_cursor` is written exclusively on a successful allocation.
[[nodiscard]] constexpr ShotAddResult shots_add(
	ShotSpawnState &state, ShotUpdateState &update
) noexcept
{
	if(state.shot_cursor > (shot_spawn_first_guard + 1U)) {
		return {ShotAddStatus::invalid_cursor, nullptr, 0xFFFFU};
	}
	if(state.shot_cursor >= shot_count) {
		return {ShotAddStatus::no_slot, nullptr, 0xFFFFU};
	}

	auto probe = state.shot_cursor;
	while(probe < shot_count) {
		++probe;
		auto *const shot = detail::shot_spawn_record(state, update, probe);
		if(shot == nullptr) return {ShotAddStatus::invalid_cursor, nullptr, 0xFFFFU};
		if(shot->flag != shot_free) continue;

		detail::shot_spawn_initialize(*shot, state.player_cur);
		state.shot_cursor = static_cast<std::uint16_t>(probe + 1U);
		return {
			probe < shot_count ? ShotAddStatus::active_slot : ShotAddStatus::guard_slot,
			shot,
			probe,
		};
	}
	return {ShotAddStatus::no_slot, nullptr, 0xFFFFU};
}

// th05/p_common.cpp. The callback is synchronous before the source resets its
// cursor. A no-op is available only by explicitly passing `NoShotSound` for
// inspection; production callers supply their own audio seam.
struct NoShotSound {
	constexpr void operator()(std::uint8_t) const noexcept {
	}
};

template <typename SoundFn>
[[nodiscard]] constexpr ShotCycleResult shot_cycle_init(
	ShotSpawnState &state, SoundFn &on_sound
) noexcept
{
	static_assert(std::is_nothrow_invocable_v<SoundFn &, std::uint8_t>);
	ShotCycleResult result{};
	switch(state.shot_time) {
	case 18:
		on_sound(1);
		result = {static_cast<std::uint8_t>(
			shot_cycle_6x | shot_cycle_3x | shot_cycle_2x | shot_cycle_1x
		), true};
		break;
	case 15:
		result.flags = shot_cycle_6x;
		break;
	case 12:
		on_sound(1);
		result = {static_cast<std::uint8_t>(shot_cycle_6x | shot_cycle_3x), true};
		break;
	case 9:
		result.flags = static_cast<std::uint8_t>(shot_cycle_6x | shot_cycle_2x);
		break;
	case 6:
		on_sound(1);
		result = {static_cast<std::uint8_t>(shot_cycle_6x | shot_cycle_3x), true};
		break;
	case 3:
		result.flags = shot_cycle_6x;
		break;
	default:
		return result;
	}
	state.shot_cursor = 0;
	state.shot_last_id = 0;
	return result;
}

// th05/p_common.cpp::shot_l0. It neither decrements shot_time nor requests
// RNG: both actions are outside this bounded common helper.
template <typename SoundFn>
[[nodiscard]] constexpr ShotFireResult shot_l0(
	ShotSpawnState &state, ShotUpdateState &update, SoundFn &on_sound
) noexcept
{
	ShotFireResult result{};
	result.cycle = shot_cycle_init(state, on_sound);
	if((result.cycle.flags & shot_cycle_3x) == 0U) return result;
	result.allocation = shots_add(state, update);
	if(result.allocation.allocated()) result.allocation.shot->damage = 10;
	return result;
}

// th05/p_common.cpp::shot_l1. A random-ring refusal preserves the successful
// allocation prefix and leaves damage/angle unchanged, rather than pretending
// that source execution never entered the function.
template <typename SoundFn>
[[nodiscard]] constexpr ShotFireResult shot_l1(
	ShotSpawnState &state,
	ShotUpdateState &update,
	pc98::th0405::RandomState &random_state,
	SoundFn &on_sound
) noexcept
{
	ShotFireResult result{};
	result.cycle = shot_cycle_init(state, on_sound);
	if((result.cycle.flags & shot_cycle_3x) == 0U) return result;
	result.allocation = shots_add(state, update);
	if(!result.allocation.allocated()) return result;

	const auto random = pc98::th0405::randring_next16_and(random_state, 7U);
	if(!random) {
		result.random_refused = true;
		return result;
	}
	const auto angle = static_cast<std::uint8_t>(0xBCU + *random);
	shot_velocity_set(*result.allocation.shot, angle);
	result.allocation.shot->damage = 10;
	return result;
}

} // namespace th05::core
