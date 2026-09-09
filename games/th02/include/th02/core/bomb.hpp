#pragma once

#include <cstddef>
#include <cstdint>

#include "th02/core/bullet_lifecycle.hpp"
#include "th02/core/player.hpp"
#include "th02/core/score.hpp"

namespace th02::core {

inline constexpr std::int16_t bomb_circle_frames = 32;

// The state written by bomb_reset(), player_bomb(), and bomb_invalidate().
// Bomb animation, palette, and page scheduling remain separate owners.
// The two performance counters are source uint8_t globals and therefore wrap
// at the byte boundary rather than widening to a host counter.
struct BombState {
	bool bombing{};
	std::int16_t bomb_frame{};
	ScreenPoint bomb_circle_center{};
	std::int16_t bomb_circle_frame{};
	// ReC98 bool16 is a raw 16-bit integer. The source compares it with true,
	// so only the raw value 1 enables the completed-circle redraw gate.
	std::int16_t bomb_circle_done{};
	std::uint8_t stage_bombs_used{};
	std::uint8_t total_bombs_used{};
	bool operator==(const BombState &) const = default;
};

struct BombInvalidation {
	ScreenPoint topleft{};
	std::int16_t width{};
	std::int16_t height{};
	bool operator==(const BombInvalidation &) const = default;
};

// This callback is also the bullet-lifetime invalidation sink so that
// player_bomb() can call the existing bullets_clear() body directly. All
// methods are synchronous source-position effects. Implementations must not
// mutate the borrowed bomb, score, hit, bullet-pool, or page state while a
// call is in progress.
class BombPresentationCallbacks : public EnemyBulletInvalidationCallbacks {
public:
	virtual ~BombPresentationCallbacks() = default;
	virtual void hud_bombs(std::int8_t bombs) noexcept = 0;
	virtual void sound_effect(std::uint8_t sound_id) noexcept = 0;
	virtual void invalidate_bomb(BombInvalidation invalidation) noexcept = 0;
	virtual void render_all() noexcept = 0;
};

enum class PlayerBombResult : std::uint8_t {
	started,
	already_bombing,
	no_bombs,
	invalid_page,
};

// th02/main/player/bombload.cpp::bomb_reset. This intentionally resets only
// the per-stage counter and active flag; the caller owns the other bomb
// animation fields and persistent total counter.
void bomb_reset(BombState &state) noexcept;

// th02/main/player/bombload.cpp::player_bomb. The source prefix (bomb guard,
// stock/HUD/counters/SFX/center initialization) remains visible even when
// the final bullets_clear() call refuses invalid page arguments.
[[nodiscard]] PlayerBombResult player_bomb(
	BombState &bomb, ScoreState &score, PlayerHitState &player,
	EnemyBulletPoolState &bullets, std::size_t page_back,
	std::size_t page_front, BombPresentationCallbacks &callbacks
) noexcept;

// th02/main/player/bombload.cpp::bomb_invalidate. The caller supplies the
// resident shottype byte. This preserves the source timer/circle lifecycle
// writes and emits tile invalidation/full-redraw requests, but does not draw
// or otherwise implement bomb animation.
void bomb_invalidate(BombState &state, std::uint8_t shottype,
	BombPresentationCallbacks &callbacks) noexcept;

} // namespace th02::core
