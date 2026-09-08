#pragma once

#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_motion.hpp"
#include "pc98/th0405_random.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace pc98::th0405 {

// th04/main/spark.inc: raw entity flags deliberately remain bytes. The update
// path turns every non-free/non-alive value into free on its next invocation.
inline constexpr std::uint8_t SPARK_FREE = 0U;
inline constexpr std::uint8_t SPARK_ALIVE = 1U;
inline constexpr std::uint8_t SPARK_REMOVE = 2U;

// th04/main/spark.inc, shared by both games. `angle` is initialized elsewhere:
// the TH05 96-iteration initializer has typed cross-record writes after its
// 64 active instances, so this active-pool seam intentionally does not own it.
struct Spark {
	std::uint8_t flag{};
	std::uint8_t age{};
	Motion center{};
	std::uint16_t angle{};
	bool operator==(const Spark &) const = default;
};

static_assert(offsetof(Spark, flag) == 0U);
static_assert(offsetof(Spark, age) == 1U);
static_assert(offsetof(Spark, center) == 2U);
static_assert(offsetof(Spark, angle) == 14U);
static_assert(sizeof(Spark) == 16U);

inline constexpr std::size_t SPARK_COUNT_TH04 = 96U;
inline constexpr std::size_t SPARK_COUNT_TH05 = 64U;

// Caller-owned storage view of the active game pool and its original byte
// offset ring. Only original active capacities are valid; this does not grant
// ownership of TH05's 32 initializer writes beyond the active 64 slots.
struct SparkPool {
	std::span<Spark> slots;
	std::uint16_t &ring_offset;
};

[[nodiscard]] constexpr bool spark_pool_valid(const SparkPool &pool) noexcept
{
	const auto count = pool.slots.size();
	if(count != SPARK_COUNT_TH04 && count != SPARK_COUNT_TH05) return false;
	const auto bytes = pool.slots.size_bytes();
	return pool.ring_offset < bytes &&
		(pool.ring_offset % sizeof(Spark)) == 0U;
}

struct SparksAddRandomResult {
	std::uint32_t attempts{};
	std::uint16_t added{};
	bool operator==(const SparksAddRandomResult &) const = default;
};

[[nodiscard]] constexpr std::int16_t spark_word_add(
	std::int16_t left,
	std::uint16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) + right
	));
}

[[nodiscard]] constexpr bool spark_center_clipped(SubpixelPoint center) noexcept
{
	// th04/main/spark_a.asm's unsigned comparisons against 384x368 Q12.4.
	return static_cast<std::uint16_t>(center.x) > 6144U ||
		static_cast<std::uint16_t>(center.y) > 5888U;
}

constexpr void spark_ring_offset_next(SparkPool pool) noexcept
{
	pool.ring_offset = static_cast<std::uint16_t>(
		pool.ring_offset + sizeof(Spark)
	);
	if(pool.ring_offset >= pool.slots.size_bytes()) {
		pool.ring_offset = 0U;
	}
}

// th04/main/spark_a.asm: first clip, then exactly `count` unsigned-word
// attempts (which means 65,536 for zero). An occupied slot advances only the
// spark ring. A free one takes exactly one randring word before vector setup.
// Unsupported host pool/RNG shapes refuse without changing either state.
[[nodiscard]] constexpr std::optional<SparksAddRandomResult> sparks_add_random(
	SparkPool pool,
	RandomState &random,
	SubpixelPoint center,
	std::int16_t radius_min,
	std::uint16_t count
) noexcept
{
	if(spark_center_clipped(center)) return SparksAddRandomResult{};
	if(!spark_pool_valid(pool) || random.cursor >= random.ring.size()) {
		return std::nullopt;
	}

	SparksAddRandomResult result{};
	std::uint16_t remaining = count;
	do {
		auto &spark = pool.slots[pool.ring_offset / sizeof(Spark)];
		if(spark.flag == SPARK_FREE) {
			// The source stores the alive/age word and current coordinates before
			// taking the guaranteed-valid word draw. It retains previous/angle.
			spark.flag = SPARK_ALIVE;
			spark.age = 0U;
			spark.center.cur = center;
			const auto distance_bits = *randring_next16_and(random, 31U);
			const auto distance = spark_word_add(radius_min, distance_bits);
			const auto velocity = pc98::masterlib::vector2(
				static_cast<std::uint8_t>(spark.angle),
				distance
			);
			spark.center.velocity = {velocity.x, velocity.y};
			++result.added;
		}
		spark_ring_offset_next(pool);
		++result.attempts;
		--remaining;
	} while(remaining != 0U);
	return result;
}

[[nodiscard]] constexpr bool spark_outside_update_bounds(
	SubpixelPoint center
) noexcept
{
	// sparks_update adds the half 8x8 sprite extent in a word register, then
	// uses unsigned bounds against (384 + 8) and (368 + 8) pixels respectively.
	const auto x = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(center.x) + 64U
	);
	const auto y = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(center.y) + 64U
	);
	return x >= 6272U || y >= 6016U;
}

// th04/main/spark.cpp's active-pool update. A removed spark is not free until
// the following update, and age overflow remains an unsigned byte wrap.
[[nodiscard]] constexpr bool sparks_update(SparkPool pool) noexcept
{
	if(!spark_pool_valid(pool)) return false;
	for(auto &spark : pool.slots) {
		if(spark.flag == SPARK_FREE) continue;
		if(spark.flag != SPARK_ALIVE) {
			spark.flag = SPARK_FREE;
			continue;
		}
		const auto center = update_motion(spark.center);
		if(spark_outside_update_bounds(center)) {
			spark.flag = SPARK_REMOVE;
			continue;
		}
		spark.center.velocity.y = spark_word_add(spark.center.velocity.y, 1U);
		spark.age = static_cast<std::uint8_t>(spark.age + 1U);
		if(spark.age > 40U) spark.flag = SPARK_REMOVE;
	}
	return true;
}

} // namespace pc98::th0405
