#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace th04::core {

inline constexpr std::size_t shot_count = 68;
inline constexpr std::uint8_t shot_free = 0;
inline constexpr std::uint8_t shot_alive = 1;
inline constexpr std::uint8_t shot_hit = 2;
inline constexpr std::uint8_t shot_remove = 18;

struct Shot {
	std::uint8_t flag{};
	std::int8_t age{};
	pc98::th0405::Motion pos{};
	std::int16_t patnum_base{};
	std::int8_t damage{}, angle{};
	bool operator==(const Shot &) const = default;
};

struct ShotAlive {
	pc98::th0405::SubpixelPoint pos{};
	// Stable pool index replaces the original near pointer.
	std::uint16_t shot_index{};
	bool operator==(const ShotAlive &) const = default;
};

struct ShotState {
	std::array<Shot, shot_count> shots{};
	std::array<ShotAlive, shot_count> shots_alive{};
	std::uint16_t shots_alive_count{};
	std::uint16_t shot_laser_time{};
	// th04/main/player/shot.hpp's forced-uint8 laser-style enum and the
	// separate byte ring-shot cadence counter. The source only assigns styles
	// 0 through 4; retain their byte representation for deterministic state.
	std::uint8_t shot_laser_style{};
	std::uint8_t shot_laser_ring_cycle{};
	pc98::th0405::Motion shot_laser_bottomcenter{};
	bool operator==(const ShotState &) const = default;
};

// th04/main/player/shots_update.cpp. This is not TH05's homing/missile
// update. The caller supplies the already-maintained option position, not
// the player's current position (options can lag by a frame).
constexpr void shots_update(ShotState &state, pc98::th0405::SubpixelPoint option_pos) noexcept
{
	state.shots_alive_count = 0;
	for(std::size_t i = 0; i < state.shots.size(); ++i) {
		auto &shot = state.shots[i];
		if(shot.flag >= shot_remove) shot.flag = shot_free;
		if(shot.flag == shot_free) continue;

		const auto cur = pc98::th0405::update_motion(shot.pos);
		// Strict bounds, in raw Q12.4: (-8, 392) x (-8, 376) pixels.
		if(!(cur.x > -128 && cur.x < 6272 && cur.y > -128 && cur.y < 6016)) {
			shot.flag = shot_remove;
			continue;
		}
		if(shot.flag > shot_alive) {
			++shot.flag;
			if((shot.flag & 3U) == shot_hit) {
				shot.patnum_base = std::bit_cast<std::int16_t>(
					static_cast<std::uint16_t>(static_cast<std::int32_t>(shot.patnum_base) + 1));
			}
			continue;
		}
		state.shots_alive[state.shots_alive_count++] = {cur, static_cast<std::uint16_t>(i)};
		shot.age = std::bit_cast<std::int8_t>(
			static_cast<std::uint8_t>(static_cast<int>(shot.age) + 1));
	}
	// Preserve stale cache entries beyond the count, as the source does.
	// The laser tracks its position even during the inactive cooldown <=32.
	if(state.shot_laser_time != 0) {
		state.shot_laser_bottomcenter.prev = state.shot_laser_bottomcenter.cur;
		state.shot_laser_bottomcenter.cur = option_pos;
		--state.shot_laser_time;
	}
}

} // namespace th04::core
