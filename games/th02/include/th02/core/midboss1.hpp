#pragma once

#include "th02/core/bullet.hpp"
#include "th02/core/random.hpp"
#include "th02/core/score.hpp"
#include "th02/core/spark.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

// A bounded projection of the boss globals reached by stage 1's M1 body. A
// later common boss owner must unify this rather than creating a second copy.
struct Midboss1BossState {
	std::array<ScreenPoint, 2U> topleft_on_page{};
	std::int16_t phase_frame{};
	std::int16_t damage{};
	std::uint8_t phase{};
	ScreenPoint target{-1, -1};
	bool operator==(const Midboss1BossState &) const = default;
};

// Stage-1-local globals reached by the M1 body. `active` is source bool16,
// deliberately represented as a word rather than a host bool.
struct Midboss1State {
	std::int16_t scenery_frame{};
	std::int16_t defeat_frame{};
	std::int16_t active{};
	std::int16_t patnum{};
	bool operator==(const Midboss1State &) const = default;
};

struct Midboss1Invalidation {
	ScreenPoint topleft{};
	std::int16_t width{};
	std::int16_t height{};
	bool operator==(const Midboss1Invalidation &) const = default;
};

enum class Midboss1SpriteKind : std::uint8_t { normal, hit_white, defeat_zoom };

struct Midboss1Sprite {
	Midboss1SpriteKind kind{};
	ScreenPoint topleft{};
	std::int16_t patnum{};
	std::int16_t zoom{};
	bool operator==(const Midboss1Sprite &) const = default;
};

// Tile, sprite, and SFX operations are synchronous presentation boundaries.
// They must not mutate the borrowed gameplay owners or page arguments.
class Midboss1Callbacks {
public:
	virtual ~Midboss1Callbacks() = default;
	virtual void invalidate_midboss(Midboss1Invalidation invalidation) noexcept = 0;
	virtual void tile_ring_set_and_put_both_8(
		std::int16_t left, std::int16_t top, std::int16_t tile
	) noexcept = 0;
	virtual void sound_effect(std::uint8_t sound_id) noexcept = 0;
	virtual void render_midboss(Midboss1Sprite sprite) noexcept = 0;
};

// Borrowed state consumed in source order by one M1 update. Page selection is
// taken from PlayerFrameState; stage scheduling and page flipping remain out
// of this slice.
struct Midboss1Context {
	PlayerFrameState &player_frame;
	PlayerShotPoolState &shots;
	ScoreState &score;
	SparkPoolState &sparks;
	EnemyBulletPoolState &bullets;
	RandomState &random;
	std::int16_t playperf{};
	std::int8_t rank{};
	bool bombing{};
	std::uint8_t shot_level{};
	std::uint8_t bomb_damage_frame_mask{};
	std::uint32_t stage_frame{};
	std::int16_t scroll_line{};
	std::int16_t scroll_step{};
};

enum class Midboss1InvalidateResult : std::uint8_t {
	inactive,
	active,
	invalid_page,
};

enum class Midboss1UpdateResult : std::uint8_t {
	completed,
	invalid_page,
	invalid_shot_hittest,
	shot_spark_allocation_refused,
	defeat_spark_allocation_refused,
	bullet_invalid_page,
	bullet_source_iatan2_fault,
};

// th02/main/midboss/m1.cpp::midboss1_invalidate. It invalidates the old back
// position first, then carries front coordinates to the selected back page.
[[nodiscard]] Midboss1InvalidateResult midboss1_invalidate(
	Midboss1BossState &boss, const PlayerFrameState &frame,
	const Midboss1State &state, Midboss1Callbacks &callbacks
) noexcept;

// th02/main/midboss/m1.cpp::midboss1_update_and_render, including its private
// firing-pattern and defeat helpers. A represented allocator/hittest refusal
// preserves its reached source prefix and is never reported as completion.
[[nodiscard]] Midboss1UpdateResult midboss1_update_and_render(
	Midboss1BossState &boss, Midboss1State &state, Midboss1Context &context,
	Midboss1Callbacks &callbacks
) noexcept;

} // namespace th02::core
