#pragma once

#include <cstdint>

#include "th02/core/player.hpp"

namespace th02::core {

// The four source globals whose only represented owner is player_reset().
// They are intentionally separate from player, score, pool, stream, and miss
// state: this routine only writes the listed bytes and is not an aggregate
// attempt initializer.
struct PlayerResetState {
	std::uint8_t option_patnum{};
	std::uint8_t shot_unused{};
	std::uint8_t shot_unused_2{};
	std::uint8_t player_unused{};
	bool operator==(const PlayerResetState &) const = default;
};

// th02/main/player/reset.cpp::player_reset. This frees shot flags and applies
// the source's per-attempt partial write set. It deliberately does not touch
// player pages/position, score lives or bombs, stream cadence/current patnums,
// shot-pool payload, animation frames, or C's cycle. An unrecognized source
// shottype runs the common prefix and retains all selection-specific state.
void player_reset(PlayerResetState &reset, bool &quit, PlayerHitState &hit,
	PlayerMissState &miss, PlayerShotPoolState &shots,
	PlayerShotLifetimeState &lifetime, PlayerShotStreamState &stream,
	PlayerShotVolleyContext &volley, MovementSpeeds &speeds,
	std::uint8_t shottype) noexcept;

} // namespace th02::core
