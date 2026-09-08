#pragma once

#include "th05/core/shot.hpp"
#include "th05/core/score.hpp"
#include "pc98/th0405_random.hpp"
#include "pc98/th0405_sparks.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
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

// These values are the native spellings of th05/playchar.h and
// th04/main/rank.hpp. Keep the fields below byte-sized: the donor reads the
// globals as bytes and the wrappers refuse values outside their guest tables
// before they can be used as indices or selector inputs.
enum class ShotPlaychar : std::uint8_t {
	reimu = 0,
	marisa = 1,
	mima = 2,
	yuuka = 3,
};

enum class ShotRank : std::uint8_t {
	easy = 0,
	normal = 1,
	hard = 2,
	lunatic = 3,
};

struct ShotHitDamageContext {
	bool bombing{};
	bool against_boss{};
	ShotPlaychar playchar{ShotPlaychar::reimu};
	std::uint8_t stage_id{};
	std::uint8_t stage_frame_mod4{};
	std::uint8_t bomb_frame{};
	pc98::th0405::SubpixelPoint player_position{};
};

struct ShotRevengeContext {
	ShotRank rank{ShotRank::easy};
	std::uint8_t stage_frame_mod4{};
};

// The collision source writes these two fields in the global TH05
// BulletTemplate. The concrete pool is not native yet, so this persistent,
// caller-owned template is the allocation boundary: the wrapper owns source
// ordering, origin, and the RNG1 low-byte angle; the callback only owns
// allocation/template fields not represented here.
struct RevengeBulletTemplate {
	pc98::th0405::SubpixelPoint origin{};
	std::uint8_t angle{};
};

using RevengeBulletRequest = RevengeBulletTemplate;

namespace detail {

[[nodiscard]] constexpr bool shot_playchar_valid(
	ShotPlaychar playchar
) noexcept
{
	return static_cast<std::uint8_t>(playchar) <=
		static_cast<std::uint8_t>(ShotPlaychar::yuuka);
}

[[nodiscard]] constexpr bool shot_rank_valid(ShotRank rank) noexcept
{
	return static_cast<std::uint8_t>(rank) <=
		static_cast<std::uint8_t>(ShotRank::lunatic);
}

[[nodiscard]] constexpr bool shot_th05_spark_pool_valid(
	pc98::th0405::SparkPool pool
) noexcept
{
	// The shared validator also accepts TH04's 96-slot pool. TH05's donor
	// hittest addresses exactly its 64 active records, so accepting 96 here
	// would hide a cross-game snapshot mix-up.
	return pool.slots.size() == pc98::th0405::SPARK_COUNT_TH05 &&
		pc98::th0405::spark_pool_valid(pool);
}

[[nodiscard]] constexpr bool shot_damage_context_valid(
	ShotHitDamageContext context
) noexcept
{
	return shot_playchar_valid(context.playchar) &&
		context.stage_frame_mod4 <= 3U;
}

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

// This is the regular caller's concrete shared-effect wrapper. It consumes
// exactly one shared TH04/TH05 spark stream on odd-parity hits, including the
// source's free-slot RNG2 draw and ring-offset advance. Invalid pool shape or
// RNG cursor is rejected before the scan can mutate a shot, cache, parity, or
// hitshot ring. A clipped spark is a valid source no-op, not a refusal.
[[nodiscard]] constexpr std::optional<ShotHitResult> shots_hittest_effects_scan(
	ShotUpdateState &state,
	ShotHitbox hitbox,
	std::uint8_t &shot_hit_spark_parity,
	pc98::th0405::SparkPool spark_pool,
	pc98::th0405::RandomState &spark_random
) noexcept
{
	if(!detail::shot_th05_spark_pool_valid(spark_pool) ||
		spark_random.cursor >= spark_random.ring.size()) {
		return std::nullopt;
	}
	auto on_spark = [&spark_pool, &spark_random](
		pc98::th0405::SubpixelPoint position
	) noexcept {
		const auto added = pc98::th0405::sparks_add_random(
			spark_pool, spark_random, position, 128, 1
		);
		(void)added;
	};
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

// Alice's complete effect wrapper owns the rank/frame revenge gate and the
// source's synchronous order: cached-position spark, then cached-position
// revenge origin, then one full RNG1 word (low byte stored as angle), then the
// caller's regular bullet allocator, then hitshot_from(). `on_bullet` must be
// noexcept and must not mutate the borrowed ShotUpdateState, Shot records,
// alive-cache entries, spark-pool slots/ring cursor, or either RNG cursor.
// `bullet_template` is persistent caller-owned template storage, matching the
// source global; the callback receives a copy so it can fill/allocate its
// concrete bullet without aliasing collision or random state. Mutating any
// borrowed state violates this synchronous contract and can invalidate the
// preflight invariant; an impossible post-preflight RNG failure terminates
// rather than reporting a successful scan.
template <typename BulletFn>
[[nodiscard]] constexpr std::optional<ShotHitResult>
shots_hittest_revenge_effects_scan(
	ShotUpdateState &state,
	ShotHitbox hitbox,
	std::uint8_t &shot_hit_spark_parity,
	ShotRevengeContext revenge_context,
	pc98::th0405::SparkPool spark_pool,
	pc98::th0405::RandomState &spark_random,
	pc98::th0405::RandomState &revenge_random,
	RevengeBulletTemplate &bullet_template,
	BulletFn &&on_bullet
) noexcept
{
	static_assert(std::is_nothrow_invocable_v<
		BulletFn &, RevengeBulletTemplate
	>);
	if(!detail::shot_rank_valid(revenge_context.rank) ||
		revenge_context.stage_frame_mod4 > 3U ||
		!detail::shot_th05_spark_pool_valid(spark_pool) ||
		spark_random.cursor >= spark_random.ring.size() ||
		revenge_random.cursor >= revenge_random.ring.size()) {
		return std::nullopt;
	}
	const auto spawn_revenge =
		revenge_context.rank != ShotRank::easy ||
		revenge_context.stage_frame_mod4 == 0U;
	auto on_spark = [&spark_pool, &spark_random](
		pc98::th0405::SubpixelPoint position
	) noexcept {
		const auto added = pc98::th0405::sparks_add_random(
			spark_pool, spark_random, position, 128, 1
		);
		(void)added;
	};
	auto on_revenge = [
		&revenge_random, &bullet_template, &on_bullet
	](
		pc98::th0405::SubpixelPoint position
	) noexcept {
		// This assignment deliberately precedes randring_next16(), matching the
		// source's global bullet_template write before its RNG1 call.
		bullet_template.origin = position;
		const auto angle_word = pc98::th0405::randring_next16(revenge_random);
		// The cursor was preflighted above. A failed optional therefore signals a
		// violated wrapper invariant, not a source-level no-op; fail closed
		// rather than returning a successful scan after a partial prefix.
		if(!angle_word) std::terminate();
		bullet_template.angle = static_cast<std::uint8_t>(*angle_word & 0x00FFU);
		on_bullet(bullet_template);
	};
	return detail::shots_hittest_impl(
		state,
		hitbox,
		shot_hit_spark_parity,
		spawn_revenge,
		on_spark,
		on_revenge
	);
}

// select_for_playchar() is reached through the TH05 assembly tail
// select_for_rank(). For the fixed arguments in shot_hit.cpp's packed call,
// its character table is Reimu/Marisa/Mima/Yuuka = 3/4/15/3. Keep this helper
// separate from the generic rank selector: the source's dword push byte order
// is not the character order.
[[nodiscard]] constexpr std::optional<std::uint16_t>
shot_bomb_damage_for_playchar(ShotPlaychar playchar) noexcept
{
	if(!detail::shot_playchar_valid(playchar)) return std::nullopt;
	switch(playchar) {
	case ShotPlaychar::reimu: return static_cast<std::uint16_t>(3U);
	case ShotPlaychar::marisa: return static_cast<std::uint16_t>(4U);
	case ShotPlaychar::mima: return static_cast<std::uint16_t>(15U);
	case ShotPlaychar::yuuka: return static_cast<std::uint16_t>(3U);
	}
	return std::nullopt;
}

[[nodiscard]] constexpr std::uint16_t shot_word_add_u16(
	std::uint16_t left,
	std::uint16_t right
) noexcept
{
	return static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(left) + right
	);
}

// `_SI` is a guest word pseudo-register. The source's multiply therefore
// wraps to 16 bits before its following divide, even though the host product
// is wider. All divisors are source constants and nonzero.
[[nodiscard]] constexpr std::uint16_t shot_word_mul_div(
	std::uint16_t value,
	std::uint16_t factor,
	std::uint16_t divisor
) noexcept
{
	const auto wrapped_product = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(value) * factor
	);
	return static_cast<std::uint16_t>(wrapped_product / divisor);
}

// Complete local damage/scoring wrapper for shots_hittest(). The collision
// scan has already performed hitshot/cache effects; this function applies the
// source's bombing, character/stage scaling, and uint32 score_delta update.
// Invalid character values refuse before any score mutation. Rank is not read
// by this non-revenge source function and therefore is intentionally absent.
// The return is the raw guest word. Although the donor declares an `int`
// return, downstream code may interpret that same word as signed; callers
// crossing the 0x7FFF boundary must preserve this bit pattern until applying
// the source's signed/unsigned consumer contract.
[[nodiscard]] constexpr std::optional<std::uint16_t> shots_hittest_apply_damage(
	ShotHitResult hit_result,
	ShotHitbox hitbox,
	ShotHitDamageContext context,
	ScoreState &score
) noexcept
{
	if(!detail::shot_damage_context_valid(context)) {
		return std::nullopt;
	}
	const auto playchar = context.playchar;
	std::uint16_t damage = hit_result.damage;
	if(context.bombing) {
		if(context.against_boss) {
			damage = static_cast<std::uint16_t>(damage / 4U);
		}
		if(context.stage_frame_mod4 == 0U) {
			const auto bomb_damage = shot_bomb_damage_for_playchar(playchar);
			if(!bomb_damage) return std::nullopt;
			damage = shot_word_add_u16(damage, *bomb_damage);
		}
		if(
			(playchar == ShotPlaychar::marisa) &&
			(context.bomb_frame >= 16U) &&
			(context.bomb_frame < 80U) &&
			((context.bomb_frame % 4U) == 0U)
		) {
			const auto left = detail::word_subtract(
				hitbox.center.x, hitbox.radius.x
			);
			const auto top = detail::word_subtract(
				hitbox.center.y, hitbox.radius.y
			);
			const auto width = detail::word_add(
				hitbox.radius.x, hitbox.radius.x
			);
			const auto x_delta = detail::shot_word_bits(
				detail::word_subtract(context.player_position.x, left)
			);
			const auto y_value = detail::shot_word_bits(
				context.player_position.y
			);
			if(x_delta <= detail::shot_word_bits(width) &&
				y_value >= detail::shot_word_bits(top)) {
				damage = shot_word_add_u16(damage, 22U);
			}
		}
	}

	if(context.against_boss) {
		if(playchar == ShotPlaychar::reimu ||
			playchar == ShotPlaychar::yuuka) {
			if(context.stage_id == 6U) {
				damage = shot_word_mul_div(damage, 5U, 4U);
			} else if(playchar == ShotPlaychar::reimu) {
				damage = shot_word_mul_div(damage, 8U, 7U);
			}
		} else {
			damage = shot_word_mul_div(damage, 4U, 5U);
		}
	} else {
		if(context.stage_id == 6U && playchar == ShotPlaychar::reimu) {
			damage = shot_word_mul_div(damage, 4U, 5U);
		}
		if(playchar == ShotPlaychar::yuuka) {
			damage = shot_word_mul_div(damage, 4U, 5U);
		}
	}
	score.score_delta += static_cast<std::uint32_t>(damage);
	return damage;
}

// Composed regular-shot consumer. All context and pool validation happens
// before shots_hittest_effects_scan(), so a malformed character/frame/pool
// snapshot cannot leave a partially consumed collision prefix. For a valid
// snapshot, shots_hittest_apply_damage() cannot refuse after the scan; its
// optional result remains part of the public refusal contract rather than
// hiding an unchecked failure.
[[nodiscard]] constexpr std::optional<std::uint16_t> shots_hittest(
	ShotUpdateState &state,
	ShotHitbox hitbox,
	std::uint8_t &shot_hit_spark_parity,
	ShotHitDamageContext damage_context,
	pc98::th0405::SparkPool spark_pool,
	pc98::th0405::RandomState &spark_random,
	ScoreState &score
) noexcept
{
	if(!detail::shot_damage_context_valid(damage_context) ||
		!detail::shot_th05_spark_pool_valid(spark_pool) ||
		spark_random.cursor >= spark_random.ring.size()) {
		return std::nullopt;
	}
	const auto hit_result = shots_hittest_effects_scan(
		state, hitbox, shot_hit_spark_parity, spark_pool, spark_random
	);
	if(!hit_result) return std::nullopt;
	return shots_hittest_apply_damage(
		*hit_result, hitbox, damage_context, score
	);
}

// Composed Alice-barrier consumer. Unlike regular shots_hittest(), the donor
// revenge function adds its unscaled word damage directly to score_delta and
// has no bomb/stage/character damage modifiers. Validation is completed before
// the effect scan; the callback remains the only concrete bullet-pool seam.
// Its return is likewise the donor's raw 16-bit word even though the original
// ABI spells the function return as signed `int`.
template <typename BulletFn>
[[nodiscard]] constexpr std::optional<std::uint16_t>
shots_hittest_revenge(
	ShotUpdateState &state,
	ShotHitbox hitbox,
	std::uint8_t &shot_hit_spark_parity,
	ShotRevengeContext revenge_context,
	pc98::th0405::SparkPool spark_pool,
	pc98::th0405::RandomState &spark_random,
	pc98::th0405::RandomState &revenge_random,
	RevengeBulletTemplate &bullet_template,
	ScoreState &score,
	BulletFn &&on_bullet
) noexcept
{
	static_assert(std::is_nothrow_invocable_v<
		BulletFn &, RevengeBulletTemplate
	>);
	if(!detail::shot_rank_valid(revenge_context.rank) ||
		revenge_context.stage_frame_mod4 > 3U ||
		!detail::shot_th05_spark_pool_valid(spark_pool) ||
		spark_random.cursor >= spark_random.ring.size() ||
		revenge_random.cursor >= revenge_random.ring.size()) {
		return std::nullopt;
	}
	const auto hit_result = shots_hittest_revenge_effects_scan(
		state,
		hitbox,
		shot_hit_spark_parity,
		revenge_context,
		spark_pool,
		spark_random,
		revenge_random,
		bullet_template,
		static_cast<BulletFn &&>(on_bullet)
	);
	if(!hit_result) return std::nullopt;
	score.score_delta += static_cast<std::uint32_t>(hit_result->damage);
	return hit_result->damage;
}

} // namespace th05::core
