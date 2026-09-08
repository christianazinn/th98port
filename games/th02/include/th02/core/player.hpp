#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th02::core {

struct ScreenPoint {
	std::int16_t x{};
	std::int16_t y{};
	bool operator==(const ScreenPoint &) const = default;
};

struct PlayerPositionState {
	std::array<ScreenPoint, 2> on_page{};
	ScreenPoint current{};
	bool operator==(const PlayerPositionState &) const = default;
};

struct MovementSpeeds {
	std::int8_t aligned_x{}, aligned_y{}, diagonal_x{}, diagonal_y{};
};

enum class PlayerPose : std::int16_t { still = 0, left = 2, right = 3 };

struct MovementIntent {
	ScreenPoint delta{};
	PlayerPose pose{PlayerPose::still};
	bool operator==(const MovementIntent &) const = default;
};

// TH02 stores the hit enum in one raw byte: 0 means not hit, 1 is an ordinary
// hit, and 0xFF is the game-over result. Keep other nonzero values representable
// rather than reducing this to bool.
struct PlayerHitState {
	std::uint8_t raw_hit{};
	std::uint8_t invincibility_time{};
	bool invincible_via_bomb{};
	bool miss_active{};
	bool operator==(const PlayerHitState &) const = default;
};

// Only the input-to-delta/pose block of player_move_and_shoot. Its caller
// must already have applied the invincibility/hit guard. No shots or bombs.
[[nodiscard]] std::optional<MovementIntent> movement_intent(
	std::uint16_t input, MovementSpeeds speeds, std::size_t back_page) noexcept;

// The prefix of player_move_and_shoot: apply its hit-immunity guard and report
// whether the remaining movement, shot-stream, and bomb work may run. It does
// not run any of that remaining work and never decrements the timer.
[[nodiscard]] bool player_move_and_shoot_allowed(PlayerHitState &state) noexcept;

// The state-only prefix before player_update_and_render's shot callback. It
// applies the same hit-immunity guard and then decrements a nonzero timer.
// The caller owns shots, miss handling, quit, and all rendering.
void prepare_player_callbacks(PlayerHitState &state) noexcept;

// th02/main/player/player.cpp::player_move. The caller supplies the page
// established by player_invalidate; this does not copy the front page first.
// An invalid native page is rejected without changing any state.
[[nodiscard]] bool player_move(PlayerPositionState &state, std::size_t back_page,
	std::int16_t delta_x, std::int16_t delta_y) noexcept;

} // namespace th02::core
