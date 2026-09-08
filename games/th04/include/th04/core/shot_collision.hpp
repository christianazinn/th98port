#pragma once

#include "th04/core/shot.hpp"

#include <optional>
#include <type_traits>

namespace th04::core {

struct ShotCollisionState {
	pc98::th0405::SubpixelPoint center{}, radius{};
	bool bombing{}, against_boss{};
	std::uint8_t stage_frame_mod2{}, stage_frame_mod4{};
	// th04/main/player/shots_hittest.cpp's byte_25980, shared by shot/laser hits.
	std::uint8_t spark_parity{};
	std::uint32_t score_delta{};
	bool operator==(const ShotCollisionState &) const = default;
};

struct ShotHitResult {
	std::uint16_t damage{};
	// Each is sparks_add_random(center.x, center.y, TO_SP(8), 1), in order.
	// The caller must apply these before the next gameplay operation. Do not
	// discard them as cosmetic: spark allocation/initialization has state.
	std::array<pc98::th0405::SubpixelPoint, shot_count + 2> sparks{};
	std::uint16_t spark_count{};
	bool operator==(const ShotHitResult &) const = default;
};

// Source: th04/main/player/shots_hittest.cpp, not TH05's damage policy.
// Invalid represented cache addresses are refused before mutation. Repeated
// valid indices remain repeated hits: this helper does not rebuild the cache
// or filter it by the Shot's flag (even if another hit test already hit it).
// The sink executes at each source call site, before later shot hits and the
// final score addition. It may update its own spark/RNG state, not these
// borrowed shot/collision inputs. Validate that state before calling here.
template<class SparkSink>
	requires std::is_nothrow_invocable_v<SparkSink &, pc98::th0405::SubpixelPoint>
[[nodiscard]] constexpr std::optional<ShotHitResult> shots_hittest(
	ShotState &state, ShotCollisionState &collision, SparkSink &&spark_sink
) noexcept
{
	const auto word = [](int value) { return static_cast<std::uint16_t>(value); };
	const auto left = word(static_cast<int>(collision.center.x) - collision.radius.x);
	const auto top = word(static_cast<int>(collision.center.y) - collision.radius.y);
	const auto width = word(static_cast<int>(collision.radius.x) * 2);
	const auto height = word(static_cast<int>(collision.radius.y) * 2);
	const auto inside = [&](pc98::th0405::SubpixelPoint pos) {
		return word(static_cast<int>(pos.x) - left) <= width &&
			word(static_cast<int>(pos.y) - top) <= height;
	};
	if(state.shots_alive_count > shot_count) return std::nullopt;
	for(std::size_t i = 0; i < state.shots_alive_count; ++i) {
		const auto &entry = state.shots_alive[i];
		if(inside(entry.pos) && entry.shot_index >= shot_count) return std::nullopt;
	}
	ShotHitResult result{};
	const auto add_spark = [&](pc98::th0405::SubpixelPoint center) {
		result.sparks[result.spark_count++] = center;
		spark_sink(center);
	};
	unsigned hits = 0;
	for(std::size_t i = 0; i < state.shots_alive_count; ++i) {
		const auto &entry = state.shots_alive[i];
		if(!inside(entry.pos)) continue;
		auto &shot = state.shots[entry.shot_index];
		shot.flag = shot_hit;
		shot.pos.velocity.x = static_cast<std::int16_t>(shot.pos.velocity.x / 6);
		shot.pos.velocity.y = static_cast<std::int16_t>(shot.pos.velocity.y / 6);
		shot.patnum_base = 40; // PAT_HITSHOT, from th04/sprites/main_pat.h
		++hits;
		result.damage = static_cast<std::uint16_t>(result.damage +
			static_cast<std::uint8_t>(shot.damage) / hits);
		++collision.spark_parity;
		if(collision.spark_parity & 1U) add_spark(shot.pos.cur);
	}
	if(collision.bombing) {
		if(collision.stage_frame_mod4 == 0) result.damage += 5;
		if(collision.against_boss) result.damage /= 4;
	}
	if(collision.stage_frame_mod2 != 0 && state.shot_laser_time > 32 &&
		top <= word(state.shot_laser_bottomcenter.cur.y)) {
		auto laser_x = word(static_cast<int>(state.shot_laser_bottomcenter.cur.x) - 384);
		for(int side = 0; side < 2; ++side) {
			if(word(static_cast<int>(laser_x) - left) <= width) {
				result.damage += 3;
				++collision.spark_parity;
				if((collision.spark_parity & 3U) == 0) {
					add_spark({std::bit_cast<std::int16_t>(laser_x), collision.center.y});
				}
			}
			laser_x = word(static_cast<int>(laser_x) + 768);
		}
	}
	collision.score_delta += result.damage;
	return result;
}

// Inspection-only form: reports requests without executing the spark pool.
// A complete simulation must supply the synchronous sink above.
[[nodiscard]] constexpr std::optional<ShotHitResult> shots_hittest(
	ShotState &state, ShotCollisionState &collision
) noexcept
{
	return shots_hittest(state, collision, [](pc98::th0405::SubpixelPoint) noexcept {});
}

} // namespace th04::core
