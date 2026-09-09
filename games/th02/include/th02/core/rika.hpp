#pragma once

#include "th02/core/boss.hpp"
#include "th02/core/boss_explosion.hpp"
#include "th02/core/bullet.hpp"
#include "th02/core/laser.hpp"
#include "th02/core/random.hpp"
#include "th02/core/score.hpp"
#include "th02/core/spark.hpp"

#include <cstdint>

namespace th02::core {

// Globals local to th02/main/boss/b1.cpp's combat body. The pre-boss dialogue
// and post-boss stage transition are lifecycle owners and deliberately absent.
struct RikaState {
	ScreenPoint topleft{};
	std::int16_t move_state{};
	std::int16_t defeat_frame{};
	std::uint8_t pattern_angle{};
	std::uint16_t pattern_frame{};
	bool operator==(const RikaState &) const = default;
};

struct RikaInvalidation {
	ScreenPoint topleft{};
	std::int16_t width{};
	std::int16_t height{};
	bool operator==(const RikaInvalidation &) const = default;
};

enum class RikaSpriteKind : std::uint8_t { normal, hit_white, defeat_zoom };

struct RikaSprite {
	RikaSpriteKind kind{};
	ScreenPoint topleft{};
	std::int16_t patnum{};
	std::int16_t zoom{};
	bool operator==(const RikaSprite &) const = default;
};

// Tile/SFX/sprite/explosion requests remain synchronous presentation seams.
// They cannot mutate the borrowed combat owners or stage timing.
class RikaCallbacks : public LaserSpawnCallbacks, public BossExplosionCallbacks {
public:
	void sound_effect(std::uint8_t sound_id) noexcept override = 0;
	virtual void invalidate_rika(RikaInvalidation invalidation) noexcept = 0;
	virtual void render_rika(RikaSprite sprite) noexcept = 0;
};

// `stage_frame` is an explicit borrowed stage-global: the combat loop reads it
// but never advances it. Its dialogue/bonus/input lifecycle is not modeled.
struct RikaCombatContext {
	PlayerFrameState &player_frame;
	PlayerHitState &player_hit;
	PlayerShotPoolState &shots;
	ScoreState &score;
	SparkPoolState &sparks;
	EnemyBulletPoolState &bullets;
	LaserPoolState &lasers;
	RandomState &random;
	std::uint32_t &stage_frame;
	std::int16_t playperf{};
	std::int8_t rank{};
	bool bombing{};
	std::uint8_t shot_level{};
	std::uint8_t bomb_damage_frame_mask{};
	std::int16_t scroll_line{};
	std::uint8_t &slowdown_factor;
	std::uint16_t &palette_tone;
};

enum class RikaBackgroundResult : std::uint8_t { completed, invalid_page };

enum class RikaUpdateResult : std::uint8_t {
	boss,
	stage_clear,
	invalid_page,
	invalid_shot_hittest,
	shot_spark_allocation_refused,
	defeat_spark_allocation_refused,
	bullet_invalid_page,
	bullet_source_iatan2_fault,
};

// th02/main/boss/b1.cpp::rika_bg_render. It invalidates old back-page X at a
// fixed Y, copies only front X to back, then conditionally invalidates Rika's
// initial 32x32 entrance tile.
[[nodiscard]] RikaBackgroundResult rika_bg_render(BossState &boss,
	const RikaState &rika, const PlayerFrameState &frame,
	RikaCallbacks &callbacks) noexcept;

// Combat-only portion of th02/main/boss/b1.cpp::rika_update plus its private
// move/pattern/hittest/defeat helpers. On a native refusal it leaves the exact
// reached source prefix intact. Full rika_init/rika_end remain lifecycle work:
// they require BGM/dialogue/bonus/replay-input/stage-transition owners.
[[nodiscard]] RikaUpdateResult rika_update(BossState &boss, RikaState &rika,
	RikaCombatContext &context, RikaCallbacks &callbacks) noexcept;

} // namespace th02::core
