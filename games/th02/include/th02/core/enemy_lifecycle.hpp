#pragma once

#include "th02/core/enemy.hpp"

#include <cstddef>
#include <cstdint>

namespace th02::core {

// The source invalidates a fixed enemy rectangle, independent of the
// per-template sprite dimensions. The callback owns tiles/VRAM; it is a
// synchronous presentation boundary, not an additional enemy-state owner.
struct EnemyInvalidation {
	ScreenPoint topleft{};
	std::int16_t width{};
	std::int16_t height{};
	bool operator==(const EnemyInvalidation &) const = default;
};

class EnemyInvalidationCallbacks {
public:
	virtual ~EnemyInvalidationCallbacks() = default;
	// Runs before the source flag transition or page carry. Implementations must
	// not mutate the borrowed pool or page arguments during this callback.
	virtual void invalidate_enemy(EnemyInvalidation invalidation) noexcept = 0;
};

enum class EnemyLifecycleResult : std::uint8_t {
	completed,
	invalid_page,
};

// th02/main/enemy/enemies.cpp::enemies_invalidate. Every raw nonfree record
// emits the fixed 32x32 old-back-page rectangle. F_REMOVE becomes free and
// skips the page carry; every other raw nonfree flag carries front_page to
// back_page and updates the upper-exclusive loop bound. Both page indices are
// validated before the first source mutation or callback.
[[nodiscard]] EnemyLifecycleResult enemies_invalidate(
	EnemyPoolState &state, std::size_t page_back, std::size_t page_front,
	EnemyInvalidationCallbacks &callbacks) noexcept;

// th02/main/enemy/enemies.cpp::enemies_remove_all. Every raw nonfree flag,
// including arbitrary values, becomes F_REMOVE; free records retain their
// payload. The source then clears the update-loop bound.
void enemies_remove_all(EnemyPoolState &state) noexcept;

} // namespace th02::core
