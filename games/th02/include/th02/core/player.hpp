#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th02::core {

struct PlayerShotPoolState;
struct PlayerShotLifetimeState;

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

struct PlayerShotStreamState {
	std::uint8_t stream_a_phase{0xFF};
	std::uint8_t stream_b_phase{0xFF};
	std::uint8_t stream_a_cooldown{};
	std::uint8_t stream_b_cooldown{};
	std::uint8_t shot_level{};
	std::uint8_t shot_patnum{};
	std::uint8_t shot_option_patnum{};
	std::uint8_t shot_patnum_powered{};
	std::uint8_t shot_option_patnum_powered{};
	std::int8_t spread_angle_delta{};
	bool operator==(const PlayerShotStreamState &) const = default;
};

// A borrowed synchronous seam for the three calls in the shot-stream tail.
// shot_volley() receives the source-position state plus only the two mutable
// handoff patnums. It does not model a shot pool, RNG, bomb, HUD, or a frame
// driver.
class PlayerShotStreamCallbacks {
public:
	virtual ~PlayerShotStreamCallbacks() = default;
	virtual void shot_volley(const PlayerShotStreamState &state,
		std::uint8_t &shot_patnum, std::uint8_t &shot_option_patnum) noexcept = 0;
	virtual void shot_sound(const PlayerShotStreamState &state) noexcept = 0;
	virtual void bomb_request(const PlayerShotStreamState &state) noexcept = 0;
};

// The caller-visible outcomes of the state-only player_update_and_render()
// composition. The two shot outcomes preserve the typed lifetime seam's
// refusal distinction after the player prefix has already run.
enum class PlayerUpdateResult : std::uint8_t {
	completed,
	miss_handled,
	shot_lifetime_invalid_page,
	shot_lifetime_source_divide_fault,
};

// Borrowed synchronous owner of the still-unported
// player_miss_update_and_render() call. It may mutate the represented player
// hit state at this exact source position, but must not mutate the borrowed
// shot/lifetime state or the caller's quit flag.
class PlayerUpdateCallbacks {
public:
	virtual ~PlayerUpdateCallbacks() = default;
	virtual void player_miss_update(PlayerHitState &state) noexcept = 0;
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

// State-only composition of player_update_and_render's update prefix: prepare
// the player hit state, run the typed player-shot lifetime update, then call
// the synchronous miss owner if the post-shot raw hit byte is nonzero. A miss
// callback that leaves 0xFF requests quit and has the raw byte cleared. The
// original VRAM-only render tail, complete miss implementation, page driver,
// and stage scheduling remain deliberately outside this seam.
[[nodiscard]] PlayerUpdateResult player_update_state_step(
	PlayerHitState &player, bool &quit, PlayerShotPoolState &shots,
	PlayerShotLifetimeState &lifetime, std::size_t back_page,
	std::uint32_t stage_frame, PlayerUpdateCallbacks &callbacks
) noexcept;

// Only the shot-stream tail of player_move_and_shoot, after movement and
// option positioning. Calls its borrowed callbacks in source order; no action
// is deferred. The callback implementations remain outside this native slice.
void player_move_and_shoot_shot_stream_step(PlayerShotStreamState &state,
	std::uint16_t input, PlayerShotStreamCallbacks &callbacks) noexcept;

// th02/main/player/player.cpp::player_move. The caller supplies the page
// established by player_invalidate; this does not copy the front page first.
// An invalid native page is rejected without changing any state.
[[nodiscard]] bool player_move(PlayerPositionState &state, std::size_t back_page,
	std::int16_t delta_x, std::int16_t delta_y) noexcept;

} // namespace th02::core
