#include "th02/core/player_bomb_update.hpp"

namespace th02::core {
namespace {

class StreamEffects final : public PlayerShotStreamEffects {
public:
	StreamEffects(PlayerBombUpdateContext &context_arg,
		BombAnimationCallbacks &animation_callbacks_arg,
		BombPresentationCallbacks &bomb_callbacks_arg,
		std::optional<PlayerBombResult> &request_result_arg) noexcept
		: context(context_arg), animation_callbacks(animation_callbacks_arg),
		  bomb_callbacks(bomb_callbacks_arg), request_result(request_result_arg)
	{
	}

	void shot_sound(const PlayerShotStreamState &) noexcept override
	{
		// Forward either source SFX-1 site: immediately after stream A's
		// successful volley, or on stream B's cooldown branch.
		animation_callbacks.sound_effect(1U);
	}

	void bomb_request(const PlayerShotStreamState &) noexcept override
	{
		// player_bomb's own order and refusal prefix remain in its existing
		// concrete body; this adapter only records the result at the call site.
		request_result = player_bomb(context.bomb, context.score, context.player,
			context.bullets, context.frame.page_back, context.frame.page_front,
			bomb_callbacks);
	}

private:
	PlayerBombUpdateContext &context;
	BombAnimationCallbacks &animation_callbacks;
	BombPresentationCallbacks &bomb_callbacks;
	std::optional<PlayerBombResult> &request_result;
};

} // namespace

PlayerBombUpdateOutcome player_move_and_bomb_update(
	PlayerBombUpdateContext &context,
	BombAnimationCallbacks &animation_callbacks,
	BombPresentationCallbacks &bomb_callbacks) noexcept
{
	PlayerBombUpdateOutcome outcome{};
	// B's homing body reads this at its volley call. It is intentionally copied
	// from the caller's prior-frame BossState, not refreshed by this prefix. The
	// concrete player body still owns hit-guard/page precedence below.
	context.volley.boss_position = {
		context.boss.target.x, context.boss.target.y,
	};
	StreamEffects stream_effects{context, animation_callbacks, bomb_callbacks,
		outcome.bomb_request_result};
	PlayerShotVolleyDispatcher dispatcher{
		context.shots, context.shot_lifetime, context.random, context.volley,
		stream_effects,
	};
	outcome.player_result = player_frame_move_and_shoot(context.frame,
		context.player, context.speeds, context.input, context.stream, dispatcher);
	if(outcome.player_result != PlayerFrameResult::completed &&
		outcome.player_result != PlayerFrameResult::hit_blocked) {
		return outcome;
	}
	if(outcome.bomb_request_result.has_value() &&
		*outcome.bomb_request_result == PlayerBombResult::invalid_page) {
		// player_bomb has already exposed its source prefix. Do not call the
		// following stage-loop function after that typed request refusal.
		return outcome;
	}

	outcome.bomb_update_called = true;
	outcome.bomb_result = bomb_update_and_render(context.bomb, context.player,
		context.bomb_selection, context.animation, context.effects, context.sparks,
		context.bullets, context.random, context.frame.page_back,
		context.frame.page_front, context.scroll_line, context.reduce_effects,
		animation_callbacks);
	return outcome;
}

} // namespace th02::core
