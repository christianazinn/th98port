#pragma once

#include "th02/core/shot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

inline constexpr std::size_t spark_count = 64U;
inline constexpr std::uint8_t spark_free = 0U;
inline constexpr std::uint8_t spark_alive = 1U;

// The source stores these modes as single bytes in spark_t.
enum class SparkRenderAs : std::uint8_t {
	dot = 0U,
	sprite = 14U,
};

// Q12.4 coordinates and velocities, matching TH02's 16-bit SPPoint fields.
struct SparkSubpixelPoint {
	std::int16_t x{};
	std::int16_t y{};
	bool operator==(const SparkSubpixelPoint &) const = default;
};

// Packed source spark_t record. The two unknown bytes intentionally remain
// represented and are not initialized or changed by sparks_add().
struct Spark {
	std::uint8_t flag{};
	std::uint8_t age{};
	std::array<SparkSubpixelPoint, 2U> screen_topleft{};
	SparkSubpixelPoint velocity{};
	SparkRenderAs render_as{};
	std::int8_t unused_1{};
	std::uint8_t angle{};
	std::uint8_t speed_base{};
	std::int8_t unused_2{};
	SparkRenderAs default_render_as{};
	bool operator==(const Spark &) const = default;
};

static_assert(sizeof(Spark) == 20U);
static_assert(offsetof(Spark, flag) == 0U);
static_assert(offsetof(Spark, age) == 1U);
static_assert(offsetof(Spark, screen_topleft) == 2U);
static_assert(offsetof(Spark, velocity) == 10U);
static_assert(offsetof(Spark, render_as) == 14U);
static_assert(offsetof(Spark, angle) == 16U);
static_assert(offsetof(Spark, speed_base) == 17U);
static_assert(offsetof(Spark, default_render_as) == 19U);

// Only the source pool and ring global read by sparks_add(). Stage setup owns
// the per-slot angle, speed_base, and default_render_as seeding; update,
// invalidation, and rendering own the remaining spark lifecycle.
struct SparkPoolState {
	std::array<Spark, spark_count> sparks{};
	std::uint16_t ring_index{};
	bool operator==(const SparkPoolState &) const = default;
};

// th02/main/spark.cpp::sparks_add. Positive counts overwrite a ring of source
// slots on the supplied back page. Native-invalid positive calls (a page other
// than 0/1 or a ring word outside 0..63) refuse without mutation before the
// source pointer would leave its represented pool. As in the source, count <=
// 0 does not dereference either value but still updates only the ring low byte.
[[nodiscard]] bool sparks_add(SparkPoolState &state, std::size_t back_page,
	std::int16_t left, std::uint16_t top, std::int16_t speed_base,
	std::int16_t count, bool as_sprite) noexcept;

// Concrete bridge for th02/main/player/shot_hittest.cpp's synchronous
// sparks_add() calls. It preserves the source order for valid state. A native
// allocator refusal is sticky and caller-visible after shots_hittest() returns;
// it is not a successful complete simulation and does not invent rollback of
// the source prefix already written by the collision body.
class PlayerShotSparkAllocatorCallbacks final : public PlayerShotHitCallbacks {
public:
	PlayerShotSparkAllocatorCallbacks(SparkPoolState &state,
		std::size_t back_page) noexcept;

	void shot_hit_spark(PlayerShotHitSpark spark) noexcept override;

	[[nodiscard]] bool all_allocations_succeeded() const noexcept;

private:
	SparkPoolState &state_;
	std::size_t back_page_;
	bool all_allocations_succeeded_{true};
};

} // namespace th02::core
