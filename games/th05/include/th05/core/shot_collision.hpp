#pragma once

#include "th05/core/shot.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace th05::core {

// th05/main/player/shot_hit.cpp: the caller writes the shared center and
// radius globals immediately before each hittest. Values are raw Q12.4 words.
struct ShotHitbox {
	pc98::th0405::SubpixelPoint center{};
	pc98::th0405::SubpixelPoint radius{};
	bool operator==(const ShotHitbox &) const = default;
};

struct ShotHitResult {
	// The source keeps this accumulator in an unsigned 16-bit pseudo-register.
	// This scan's bounded 64-shot total fits without wrap; the caller-owned
	// bomb/scaling wrapper must continue to model any later word arithmetic.
	std::uint16_t damage{};
	std::uint8_t hit_count{};
	bool operator==(const ShotHitResult &) const = default;
};

namespace detail {

// The Turbo C++ source performs these expressions as 16-bit `int`s before
// converting the result to unsigned for the collision test. Keep the guest
// word narrowing explicit on hosts whose `int` is wider.
[[nodiscard]] constexpr std::uint16_t shot_word_bits(
	std::int16_t value
) noexcept
{
	return std::bit_cast<std::uint16_t>(value);
}

[[nodiscard]] constexpr bool shot_hitbox_contains(
	pc98::th0405::SubpixelPoint position,
	ShotHitbox hitbox
) noexcept
{
	const auto left = word_subtract(hitbox.center.x, hitbox.radius.x);
	const auto top = word_subtract(hitbox.center.y, hitbox.radius.y);
	const auto width = word_add(hitbox.radius.x, hitbox.radius.x);
	const auto height = word_add(hitbox.radius.y, hitbox.radius.y);
	// `sa->pos - left` is converted to unsigned before the source's `ja`.
	// Thus both edges are inclusive, and all four words retain 16-bit wrap.
	return shot_word_bits(word_subtract(position.x, left)) <=
		shot_word_bits(width) &&
		shot_word_bits(word_subtract(position.y, top)) <=
		shot_word_bits(height);
}

// Source: th05/main/player/shot_hit.cpp@c6c2ee5390012b6a334163716a1703bf2edd0229.
// The two callbacks are deliberately synchronous and must be nonthrowing. They
// borrow only a copy of the cached position; callbacks must not mutate the
// caller's ShotUpdateState, its Shot records, or its ShotAlive cache. The
// `on_spark` callback models the optional cached-position sparks_add_random()
// call. `on_revenge` is called at the source's bullet-template site: the source
// stores the cached position as bullet_template.origin FIRST, then draws RNG1's
// full word for the angle, and finally calls the regular allocator before this
// helper creates the hit animation.
template <typename SparkFn, typename RevengeFn>
[[nodiscard]] constexpr std::optional<ShotHitResult> shots_hittest_impl(
	ShotUpdateState &state,
	ShotHitbox hitbox,
	std::uint8_t &shot_hit_spark_parity,
	bool spawn_revenge,
	SparkFn &on_spark,
	RevengeFn &on_revenge
) noexcept
{
	static_assert(std::is_nothrow_invocable_v<
		SparkFn &, pc98::th0405::SubpixelPoint
	>);
	static_assert(std::is_nothrow_invocable_v<
		RevengeFn &, pc98::th0405::SubpixelPoint
	>);
	// The original has trusted fixed-size arrays and near pointers. A malformed
	// host snapshot must not turn those pointers/indices into an out-of-bounds
	// read. Refusal is atomic: validate every index and the hitshot ring cursor
	// before invoking either callback or mutating the shot/cache state. The
	// cursor is validated even for a no-hit call, so invalid snapshots cannot
	// be mistaken for successful native execution.
	if(state.shots_alive_count > state.shots_alive.size() ||
		state.hitshot_state.next_free_id >= hitshot_count) {
		return std::nullopt;
	}
	for(std::size_t i = 0; i < state.shots_alive_count; ++i) {
		if(state.shots_alive[i].shot_index >= state.shots.size()) {
			return std::nullopt;
		}
	}

	ShotHitResult result{};
	for(std::size_t i = 0; i < state.shots_alive_count; ++i) {
		// Copy before callbacks. The source passes this cached position to
		// sparks_add_random() and uses it as the revenge bullet origin, while
		// hitshot_from() below reads the live Shot record instead.
		const auto cached_position = state.shots_alive[i].pos;
		if(!shot_hitbox_contains(cached_position, hitbox)) continue;

		const auto shot_index = state.shots_alive[i].shot_index;
		const auto &shot = state.shots[shot_index];
		result.hit_count = static_cast<std::uint8_t>(result.hit_count + 1U);
		const auto per_shot = static_cast<std::uint8_t>(
			static_cast<std::uint8_t>(shot.damage) / result.hit_count
		);
		result.damage = static_cast<std::uint16_t>(result.damage +
			static_cast<std::uint16_t>(per_shot == 0U ? 1U : per_shot));

		shot_hit_spark_parity = static_cast<std::uint8_t>(
			shot_hit_spark_parity + 1U
		);
		if((shot_hit_spark_parity & 1U) != 0U) {
			on_spark(cached_position);
		}
		if(spawn_revenge) {
			on_revenge(cached_position);
		}
		// hitshot_from() removes the live Shot even when its selected ring slot
		// is occupied. The preflight above makes this source-trusted operation
		// safe for a well-formed native snapshot.
		if(!hitshot_from(state.hitshot_state, state.shots[shot_index])) {
			return std::nullopt;
		}
		state.shots_alive[i].pos.x = homing_target_none;
	}
	return result;
}

struct NoRevenge {
	constexpr void operator()(pc98::th0405::SubpixelPoint) const noexcept {}
};

} // namespace detail

// Regular TH05 shot-hittest scan. The caller owns spark-pool/RNG state and
// supplies the exact synchronous effect seam. This is intentionally named a
// scan rather than a complete `shots_hittest`: bomb damage, player/stage
// scaling, and score_delta remain outside this bounded consumer.
template <typename SparkFn>
[[nodiscard]] constexpr std::optional<ShotHitResult> shots_hittest_scan(
	ShotUpdateState &state,
	ShotHitbox hitbox,
	std::uint8_t &shot_hit_spark_parity,
	SparkFn &&on_spark
) noexcept
{
	detail::NoRevenge no_revenge;
	return detail::shots_hittest_impl(
		state,
		hitbox,
		shot_hit_spark_parity,
		false,
		on_spark,
		no_revenge
	);
}

// Alice's barrier shot-hittest scan. The caller computes the source rank/frame
// gate and passes it as [spawn_revenge]. The revenge callback must synchronously
// set the cached position as bullet_template.origin, draw RNG1's full word
// (retaining its low byte as the bullet angle), and call the regular allocator.
template <typename SparkFn, typename RevengeFn>
[[nodiscard]] constexpr std::optional<ShotHitResult> shots_hittest_revenge_scan(
	ShotUpdateState &state,
	ShotHitbox hitbox,
	std::uint8_t &shot_hit_spark_parity,
	bool spawn_revenge,
	SparkFn &&on_spark,
	RevengeFn &&on_revenge
) noexcept
{
	return detail::shots_hittest_impl(
		state,
		hitbox,
		shot_hit_spark_parity,
		spawn_revenge,
		on_spark,
		on_revenge
	);
}

} // namespace th05::core
