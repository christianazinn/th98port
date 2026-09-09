#pragma once

#include "th02/core/bomb_update.hpp"
#include "th02/core/boss.hpp"
#include "th02/core/player.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace th02::core {

// The exact two-call prefix in stage_loop(): player_move_and_shoot() followed
// immediately by bomb_update_and_render(). Every referenced owner is borrowed;
// this context does not add a second player, volley, bomb, pool, or animation
// state. The caller supplies the bomb function selected by bomb_load().
struct PlayerBombUpdateContext {
	PlayerFrameState &frame;
	const BossState &boss;
	PlayerHitState &player;
	MovementSpeeds speeds{};
	std::uint16_t input{};
	PlayerShotStreamState &stream;
	PlayerShotPoolState &shots;
	PlayerShotLifetimeState &shot_lifetime;
	PlayerShotVolleyContext &volley;
	RandomState &random;
	BombState &bomb;
	ScoreState &score;
	EnemyBulletPoolState &bullets;
	SparkPoolState &sparks;
	BombAnimationState &animation;
	BombEffectState &effects;
	BombSelection bomb_selection{BombSelection::reimu_a};
	std::int16_t scroll_line{};
	bool reduce_effects{};
};

// The result keeps both source call results visible. A bomb request is absent
// when the player body was hit-blocked or did not reach its bomb label. When
// present, no_bombs and already_bombing are ordinary source no-ops and do not
// prevent the following bomb update. An invalid request is reported with
// bomb_update_called=false, after player_bomb's already-written prefix.
struct PlayerBombUpdateOutcome {
	PlayerFrameResult player_result{PlayerFrameResult::completed};
	BombUpdateResult bomb_result{BombUpdateResult::inactive};
	std::optional<PlayerBombResult> bomb_request_result{};
	bool bomb_update_called{};
	bool operator==(const PlayerBombUpdateOutcome &) const = default;
};

// The source-position bridge emits the player stream's SFX 1 and invokes the
// concrete player_bomb() body synchronously. The two callback references may
// be backed by one presentation object, but neither callback may mutate the
// borrowed deterministic owners. Player movement/shot refusals stop before
// bomb update; hit_blocked deliberately still advances an existing bomb.
[[nodiscard]] PlayerBombUpdateOutcome player_move_and_bomb_update(
	PlayerBombUpdateContext &context,
	BombAnimationCallbacks &animation_callbacks,
	BombPresentationCallbacks &bomb_callbacks) noexcept;

} // namespace th02::core
