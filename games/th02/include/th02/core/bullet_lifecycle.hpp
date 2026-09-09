#pragma once

#include <cstddef>
#include <cstdint>

#include "th02/core/bullet.hpp"

namespace th02::core {

// The source-sized background rectangle emitted before a bullet's old
// back-page state is removed or carried. The callback owns tiles and any
// actual presentation; no renderer or page flip is implied here.
struct EnemyBulletInvalidation {
	ScreenPoint topleft{};
	std::int16_t width{};
	std::int16_t height{};
	bool operator==(const EnemyBulletInvalidation &) const = default;
};

class EnemyBulletInvalidationCallbacks {
public:
	virtual ~EnemyBulletInvalidationCallbacks() = default;
	// Runs before the source flag transition or page carry. Implementations must
	// not mutate the borrowed pool or page arguments during this callback.
	virtual void invalidate_bullet(
		EnemyBulletInvalidation invalidation) noexcept = 0;
};

enum class EnemyBulletLifecycleResult : std::uint8_t {
	completed,
	invalid_page,
};

// th02/main/bullet/bullet.cpp::bullets_invalidate. Every raw nonfree record
// emits size_type * 8 at its old back-page position. F_REMOVE becomes free and
// skips the page carry; every other nonfree flag copies front_page to
// back_page. Both pages are explicitly validated before the first callback.
[[nodiscard]] EnemyBulletLifecycleResult bullets_invalidate(
	EnemyBulletPoolState &state, std::size_t page_back, std::size_t page_front,
	EnemyBulletInvalidationCallbacks &callbacks) noexcept;

// th02/main/bullet/bullet.cpp::bullets_clear. This intentionally invokes the
// actual invalidation pass first, then marks only F_ALIVE records F_REMOVE.
// Free, remove, and arbitrary raw flags retain their source-visible result.
[[nodiscard]] EnemyBulletLifecycleResult bullets_clear(
	EnemyBulletPoolState &state, std::size_t page_back, std::size_t page_front,
	EnemyBulletInvalidationCallbacks &callbacks) noexcept;

} // namespace th02::core
