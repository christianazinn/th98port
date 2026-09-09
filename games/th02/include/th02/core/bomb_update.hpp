#pragma once

#include "th02/core/bomb_animation.hpp"

namespace th02::core {

enum class BombSelection : std::uint8_t { reimu_a, reimu_b, reimu_c };
enum class BombUpdateResult : std::uint8_t {
	inactive, running, completed, invalid_selection, invalid_page,
	spark_allocation_failed,
};

// Selection represents the function installed by bomb_load(), not a fresh
// interpretation of the resident shottype each frame. Presentation callbacks
// must not mutate the borrowed deterministic state.
[[nodiscard]] BombUpdateResult bomb_update_and_render(
	BombState &bomb, PlayerHitState &player, BombSelection selection,
	BombAnimationState &animation, BombEffectState &effects,
	SparkPoolState &sparks, EnemyBulletPoolState &bullets, RandomState &random,
	std::size_t page_back, std::size_t page_front, std::int16_t scroll_line,
	bool reduce_effects, BombAnimationCallbacks &callbacks) noexcept;

} // namespace th02::core
