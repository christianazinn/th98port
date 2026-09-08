#pragma once

#include "pc98/masterlib_vector.hpp"
#include "th04/core/random.hpp"
#include "th04/core/shot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace th04::core {

inline constexpr std::size_t shot_spawn_guard_count = 5;
inline constexpr std::size_t shot_spawn_tail_bytes = 6;
inline constexpr std::uint16_t shot_spawn_first_guard =
	static_cast<std::uint16_t>(shot_count);
inline constexpr std::uint16_t shot_spawn_tail_offset = static_cast<std::uint16_t>(
	(shot_count + shot_spawn_guard_count) * sizeof(Shot)
);
inline constexpr std::int16_t pat_shot_reimu = 28;
inline constexpr std::int16_t pat_shot_marisa = 34;

using ShotSpawnGuard = std::array<Shot, shot_spawn_guard_count>;
static_assert(sizeof(Shot) == 18U);
static_assert(sizeof(ShotSpawnGuard) == 90U);

// th04_main.asm places 96 raw bytes after shots[68]. Five complete source
// records fit in that storage; the final six bytes are retained but cannot
// safely be treated as a Shot.
struct ShotSpawnState {
	ShotSpawnGuard guard{};
	std::array<std::byte, shot_spawn_tail_bytes> tail{};
	pc98::th0405::SubpixelPoint player_cur{};
	std::uint16_t shot_cursor_bytes{};
	std::uint8_t shot_last_id{};
	std::uint8_t shot_time{};
	bool operator==(const ShotSpawnState &) const = default;
};

enum class ShotAddStatus : std::uint8_t {
	active_slot,
	guard_slot,
	no_slot,
	unsupported_tail_access,
	invalid_cursor,
};

struct ShotAddResult {
	ShotAddStatus status{ShotAddStatus::no_slot};
	std::uint16_t record_index{0xFFFFU};

	[[nodiscard]] constexpr bool allocated() const noexcept
	{
		return status == ShotAddStatus::active_slot || status == ShotAddStatus::guard_slot;
	}
};

struct ShotFireResult {
	ShotAddResult allocation{};
	bool random_refused{};
};

struct ShotCycleResult {
	bool fired{};
	bool sound_1{};
	bool operator==(const ShotCycleResult &) const = default;
};

namespace detail {

[[nodiscard]] constexpr Shot *shot_spawn_record(
	ShotSpawnState &state, ShotState &shots, std::uint16_t record_index
) noexcept
{
	if(record_index < shot_count) return &shots.shots[record_index];
	if(record_index < (shot_count + shot_spawn_guard_count)) {
		return &state.guard[record_index - shot_count];
	}
	return nullptr;
}

constexpr void initialize_shot(Shot &shot, pc98::th0405::SubpixelPoint player_cur) noexcept
{
	// th04/main/player/shots_add.cpp writes these fields only. pos.prev,
	// patnum_base, damage, and angle retain prior storage until a caller writes.
	shot.flag = shot_alive;
	shot.age = 0;
	shot.pos.cur = player_cur;
	shot.pos.velocity = {0, -192};
}

constexpr void shot_velocity_set(Shot &shot, std::uint8_t angle) noexcept
{
	const auto velocity = pc98::masterlib::vector2(angle, 192);
	shot.pos.velocity = {velocity.x, velocity.y};
}

} // namespace detail

constexpr void reset_shot_allocator(ShotSpawnState &state) noexcept
{
	state.shot_cursor_bytes = 0;
	state.shot_last_id = 0;
}

// th04/main/player/shots_add.cpp. The source increments its global near
// pointer before it reads the candidate flag. At the sixth partial-tail probe,
// preserve that pointer increment but refuse before reading or writing bytes
// that this bounded representation cannot prove are a complete Shot.
[[nodiscard]] constexpr ShotAddResult shots_add(
	ShotSpawnState &state, ShotState &shots
) noexcept
{
	while(true) {
		if(state.shot_last_id >= shot_count) return {ShotAddStatus::no_slot, 0xFFFFU};
		if((state.shot_cursor_bytes % sizeof(Shot)) != 0U ||
			state.shot_cursor_bytes > shot_spawn_tail_offset) {
			return {ShotAddStatus::invalid_cursor, 0xFFFFU};
		}
		if(state.shot_cursor_bytes == shot_spawn_tail_offset) {
			state.shot_cursor_bytes = static_cast<std::uint16_t>(
				state.shot_cursor_bytes + sizeof(Shot)
			);
			return {ShotAddStatus::unsupported_tail_access, 0xFFFFU};
		}

		const auto record_index = static_cast<std::uint16_t>(
			state.shot_cursor_bytes / sizeof(Shot)
		);
		auto *const shot = detail::shot_spawn_record(state, shots, record_index);
		if(shot == nullptr) return {ShotAddStatus::invalid_cursor, 0xFFFFU};
		state.shot_cursor_bytes = static_cast<std::uint16_t>(
			state.shot_cursor_bytes + sizeof(Shot)
		);
		if(shot->flag != shot_free) {
			state.shot_last_id = static_cast<std::uint8_t>(state.shot_last_id + 1U);
			continue;
		}

		detail::initialize_shot(*shot, state.player_cur);
		return {
			record_index < shot_count ? ShotAddStatus::active_slot : ShotAddStatus::guard_slot,
			record_index,
		};
	}
}

// th04/main/player/shot_reimu.cpp and p_marisa.cpp, levels 0 and 1. These
// helpers reset the allocator before each pattern call; cadence and sound are
// the separate player-update seam below.
[[nodiscard]] constexpr ShotFireResult shot_l0(
	ShotSpawnState &state, ShotState &shots, std::int16_t patnum_base
) noexcept
{
	reset_shot_allocator(state);
	ShotFireResult result{shots_add(state, shots)};
	if(result.allocation.allocated()) {
		auto *const shot = detail::shot_spawn_record(state, shots, result.allocation.record_index);
		shot->patnum_base = patnum_base;
		shot->damage = 10;
	}
	return result;
}

[[nodiscard]] constexpr ShotFireResult shot_l1(
	ShotSpawnState &state,
	ShotState &shots,
	pc98::th0405::RandomState &random,
	std::int16_t patnum_base
) noexcept
{
	reset_shot_allocator(state);
	ShotFireResult result{shots_add(state, shots)};
	if(!result.allocation.allocated()) return result;

	const auto random_offset = pc98::th0405::randring_next16_and(random, 7U);
	if(!random_offset) {
		result.random_refused = true;
		return result;
	}
	auto *const shot = detail::shot_spawn_record(state, shots, result.allocation.record_index);
	const auto angle = static_cast<std::uint8_t>(0xBCU + *random_offset);
	detail::shot_velocity_set(*shot, angle);
	shot->patnum_base = patnum_base;
	shot->damage = 10;
	return result;
}

[[nodiscard]] constexpr ShotFireResult shot_reimu_l0(
	ShotSpawnState &state, ShotState &shots
) noexcept
{
	return shot_l0(state, shots, pat_shot_reimu);
}

[[nodiscard]] constexpr ShotFireResult shot_marisa_l0(
	ShotSpawnState &state, ShotState &shots
) noexcept
{
	return shot_l0(state, shots, pat_shot_marisa);
}

[[nodiscard]] constexpr ShotFireResult shot_reimu_l1(
	ShotSpawnState &state, ShotState &shots, pc98::th0405::RandomState &random
) noexcept
{
	return shot_l1(state, shots, random, pat_shot_reimu);
}

[[nodiscard]] constexpr ShotFireResult shot_marisa_l1(
	ShotSpawnState &state, ShotState &shots, pc98::th0405::RandomState &random
) noexcept
{
	return shot_l1(state, shots, random, pat_shot_marisa);
}

// The unlocked branch of th04/main/player/update.cpp. The callbacks preserve
// the source order: player pattern first, sound 1 second. The caller decides
// whether this branch is reachable; miss/bomb/movement logic is not hidden.
template <typename ShotFn, typename SoundFn>
[[nodiscard]] constexpr ShotCycleResult shot_cycle_step(
	ShotSpawnState &state, bool shot_held, ShotFn &fire_pattern, SoundFn &on_sound
) noexcept
{
	static_assert(std::is_nothrow_invocable_v<ShotFn &>);
	static_assert(std::is_nothrow_invocable_v<SoundFn &, std::uint8_t>);
	if(shot_held && state.shot_time <= 1U) {
		state.shot_time = 18;
		fire_pattern();
		on_sound(1);
		return {true, true};
	}
	if(state.shot_time == 0U) return {};
	--state.shot_time;
	if(state.shot_time != 6U && state.shot_time != 12U) return {};
	fire_pattern();
	on_sound(1);
	return {true, true};
}

} // namespace th04::core
