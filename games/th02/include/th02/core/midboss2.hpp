#pragma once

#include "th02/core/boss.hpp"
#include "th02/core/bullet.hpp"
#include "th02/core/random.hpp"
#include "th02/core/score.hpp"
#include "th02/core/spark.hpp"
#include "th02/core/stage_scenery.hpp"

#include <cstddef>
#include <cstdint>

namespace th02::core {

// Stage-2-local source globals. `active` is source bool16 and therefore
// retains its full raw word width rather than being normalized to host bool.
struct Midboss2State {
	std::int16_t active{};
	std::int16_t defeat_frame{};
	bool operator==(const Midboss2State &) const = default;
};

struct Midboss2Invalidation {
	ScreenPoint topleft{};
	std::int16_t width{};
	std::int16_t height{};
	bool operator==(const Midboss2Invalidation &) const = default;
};

enum class Midboss2SpriteKind : std::uint8_t { normal, hit_white, defeat_zoom };

struct Midboss2Sprite {
	Midboss2SpriteKind kind{};
	ScreenPoint topleft{};
	std::int16_t patnum{};
	std::int16_t zoom{};
	bool operator==(const Midboss2Sprite &) const = default;
};

// These synchronous seams preserve source call positions only. They must not
// mutate the borrowed gameplay owners, flash state, or page arguments.
class Midboss2Callbacks {
public:
	virtual ~Midboss2Callbacks() = default;
	virtual void invalidate_midboss(Midboss2Invalidation invalidation) noexcept = 0;
	virtual void sound_effect(std::uint8_t sound_id) noexcept = 0;
	virtual void render_midboss(Midboss2Sprite sprite) noexcept = 0;
};

// Source globals consumed by one M2 frame. Stage scheduling/page flipping and
// tile/VRAM work are deliberately caller responsibilities.
struct Midboss2Context {
	PlayerFrameState &player_frame;
	PlayerShotPoolState &shots;
	ScoreState &score;
	SparkPoolState &sparks;
	EnemyBulletPoolState &bullets;
	RandomState &random;
	StageBgFlashState &flash;
	std::int16_t playperf{};
	std::int8_t rank{};
	bool bombing{};
	std::uint8_t shot_level{};
	std::uint8_t bomb_damage_frame_mask{};
	std::uint32_t stage_frame{};
	std::int16_t scroll_line{};
};

enum class Midboss2InvalidateResult : std::uint8_t {
	inactive,
	active,
	invalid_page,
};

enum class Midboss2UpdateResult : std::uint8_t {
	completed,
	invalid_page,
	invalid_shot_hittest,
	shot_spark_allocation_refused,
	defeat_spark_allocation_refused,
	bullet_invalid_page,
	bullet_source_iatan2_fault,
};

// th02/main/midboss/m2.cpp::midboss2_invalidate. It invalidates the old back
// rectangle before carrying both source coordinate words from front to back.
[[nodiscard]] Midboss2InvalidateResult midboss2_invalidate(
	BossState &boss, const PlayerFrameState &frame,
	const Midboss2State &state, Midboss2Callbacks &callbacks
) noexcept;

// th02/main/midboss/m2.cpp::midboss2_update_and_render and its private defeat
// and bullet-pattern helpers. A modeled allocator/hittest refusal retains the
// source prefix that has already run; it is never reported as a complete frame.
[[nodiscard]] Midboss2UpdateResult midboss2_update_and_render(
	BossState &boss, Midboss2State &state, Midboss2Context &context,
	Midboss2Callbacks &callbacks
) noexcept;

} // namespace th02::core
