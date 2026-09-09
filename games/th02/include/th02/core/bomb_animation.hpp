#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "th02/core/bomb_effects.hpp"
#include "th02/core/bullet_lifecycle.hpp"
#include "th02/core/spark.hpp"

namespace th02::core {

struct RandomState;

struct BombRgb {
	std::uint8_t r{};
	std::uint8_t g{};
	std::uint8_t b{};
	bool operator==(const BombRgb &) const = default;
};

inline constexpr std::size_t bomb_smear_columns = 48U;

// The small set of globals owned by th02/main/player/bomb.cpp's A/B/C
// bodies. Tile modes and the B cel are raw source bytes; PaletteTone is a
// source unsigned word. The shared particle/circle fields live in
// BombEffectState instead of being duplicated here.
struct BombAnimationState {
	BombRgb col0_before_bomb_a{};
	BombRgb col3_before_bomb_a{};
	std::uint8_t tile_mode_before_bomb_a{};
	std::uint8_t tile_mode_before_bomb_b{};
	std::uint8_t tile_mode_before_bomb_c{};
	std::uint8_t bomb_b_cel{};
	std::array<std::int16_t, bomb_smear_columns> smear_bottoms{};
	std::uint8_t slowdown_factor{};
	std::uint16_t palette_tone{};
	bool operator==(const BombAnimationState &) const = default;
};

// These callbacks are synchronous source-position presentation boundaries.
// They must not mutate borrowed BombState, BombEffectState, SparkPoolState,
// EnemyBulletPoolState, or RandomState. Callbacks here must not invalidate any
// borrowed cursor/pool invariant or mutate the deterministic records.
class BombAnimationCallbacks : public BombEffectCallbacks,
	public EnemyBulletInvalidationCallbacks {
public:
	virtual ~BombAnimationCallbacks() = default;
	virtual BombRgb palette_get(std::uint8_t palette_index) noexcept = 0;
	virtual void palette_set(std::uint8_t palette_index,
		BombRgb color) noexcept = 0;
	virtual void palette_show() noexcept = 0;
	virtual std::uint8_t tile_mode_get() noexcept = 0;
	virtual void tile_mode_set(std::uint8_t mode) noexcept = 0;
	virtual void boxfill(std::int16_t left, std::int16_t top,
		std::int16_t right, std::int16_t bottom) noexcept = 0;
	// One request represents the source helper's complete tile/asset pass;
	// this parcel does not claim pixel or renderer fidelity.
	virtual void bomb_bft(std::int16_t cel) noexcept = 0;
	virtual void bomb_b_tram(std::uint8_t cel) noexcept = 0;
	virtual void bomb_smear(std::uint8_t column,
		std::int16_t bottom) noexcept = 0;
	virtual void pi_put(std::int16_t left, std::int16_t top,
		std::uint8_t image) noexcept = 0;
	virtual void sound_effect(std::uint8_t sound_id) noexcept = 0;
	virtual void overlay_wipe() noexcept = 0;
};

enum class BombAnimationResult : std::uint8_t {
	running,
	completed,
	invalid_page,
	spark_allocation_failed,
};

// Source bodies: th02/main/player/bomb.cpp::bomb_reimu_a/b/c. The native
// caller supplies page indices and shared pools explicitly; all callbacks
// remain synchronous, and a malformed page or spark pool is reported after
// the source-position prefix has run.
[[nodiscard]] BombAnimationResult bomb_reimu_a(
	BombState &bomb, BombAnimationState &animation, BombEffectState &effects,
	SparkPoolState &sparks, EnemyBulletPoolState &bullets, RandomState &random,
	std::size_t page_back, std::size_t page_front, std::int16_t scroll_line,
	bool reduce_effects, BombAnimationCallbacks &callbacks) noexcept;

[[nodiscard]] BombAnimationResult bomb_reimu_b(
	BombState &bomb, BombAnimationState &animation, BombEffectState &effects,
	SparkPoolState &sparks, EnemyBulletPoolState &bullets, RandomState &random,
	std::size_t page_back, std::size_t page_front, std::int16_t scroll_line,
	bool reduce_effects, BombAnimationCallbacks &callbacks) noexcept;

[[nodiscard]] BombAnimationResult bomb_reimu_c(
	BombState &bomb, BombAnimationState &animation, BombEffectState &effects,
	SparkPoolState &sparks, EnemyBulletPoolState &bullets, RandomState &random,
	std::size_t page_back, std::size_t page_front, std::int16_t scroll_line,
	bool reduce_effects, BombAnimationCallbacks &callbacks) noexcept;

} // namespace th02::core
