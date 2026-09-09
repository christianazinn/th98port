#include "th02/core/bomb_update.hpp"

#include <bit>

namespace th02::core {

BombUpdateResult bomb_update_and_render(
	BombState &bomb, PlayerHitState &player, BombSelection selection,
	BombAnimationState &animation, BombEffectState &effects,
	SparkPoolState &sparks, EnemyBulletPoolState &bullets, RandomState &random,
	std::size_t page_back, std::size_t page_front, std::int16_t scroll_line,
	bool reduce_effects, BombAnimationCallbacks &callbacks) noexcept
{
	if (!bomb.bombing) {
		return BombUpdateResult::inactive;
	}
	bomb.bomb_frame = std::bit_cast<std::int16_t>(
		static_cast<std::uint16_t>(static_cast<std::uint16_t>(bomb.bomb_frame) + 1U));
	if (bomb.bomb_circle_done == 0) {
		if (bomb.bomb_frame <= bomb_circle_frames) {
			if (bomb_circle_update_and_render(bomb, effects, page_back,
				scroll_line, callbacks) == BombCircleEffectResult::invalid_page) {
				return BombUpdateResult::invalid_page;
			}
		} else if (bomb.bomb_circle_frame == 0) {
			bomb.bomb_frame = 0;
			++bomb.bomb_circle_done;
		}
		return BombUpdateResult::running;
	}
	if (bomb.bomb_circle_done != 1) {
		return BombUpdateResult::running;
	}
	using Body = decltype(&bomb_reimu_a);
	Body body{};
	switch (selection) {
	case BombSelection::reimu_a: body = &bomb_reimu_a; break;
	case BombSelection::reimu_b: body = &bomb_reimu_b; break;
	case BombSelection::reimu_c: body = &bomb_reimu_c; break;
	default: return BombUpdateResult::invalid_selection;
	}
	const auto result = body(bomb, animation, effects, sparks, bullets, random,
		page_back, page_front, scroll_line, reduce_effects, callbacks);
	switch (result) {
	case BombAnimationResult::running: return BombUpdateResult::running;
	case BombAnimationResult::invalid_page: return BombUpdateResult::invalid_page;
	case BombAnimationResult::spark_allocation_failed:
		return BombUpdateResult::spark_allocation_failed;
	case BombAnimationResult::completed: break;
	}
	bomb.bombing = false;
	player.invincible_via_bomb = false;
	player.invincibility_time = 70;
	animation.palette_tone = 100;
	callbacks.palette_show();
	return BombUpdateResult::completed;
}

} // namespace th02::core
