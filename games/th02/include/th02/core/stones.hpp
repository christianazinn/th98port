#pragma once

#include "th02/core/boss.hpp"
#include "th02/core/boss_explosion.hpp"
#include "th02/core/bullet.hpp"
#include "th02/core/item.hpp"
#include "th02/core/laser.hpp"
#include "th02/core/random.hpp"
#include "th02/core/score.hpp"
#include "th02/core/spark.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

inline constexpr std::size_t stone_count = 5U;
inline constexpr std::size_t stone_inner_west = 0U;
inline constexpr std::size_t stone_inner_east = 1U;
inline constexpr std::size_t stone_outer_west = 2U;
inline constexpr std::size_t stone_outer_east = 3U;
inline constexpr std::size_t stone_north = 4U;

// The Stage 3 midboss aliases these three arrays in the source. Keep their
// element widths and raw flag bytes public so M3 can bind the same storage;
// the rest belongs exclusively to the Stones combat body.
enum class StoneFlag : std::uint8_t {
	dormant = 0U,
	active = 1U,
	kill_anim = 2U,
	remove = 3U,
	removed = 4U,
};

struct StonesState {
	std::array<std::uint8_t, stone_count> flag{};
	std::array<std::int16_t, stone_count> damage{};
	std::array<std::int16_t, stone_count> patnum{};
	std::array<ScreenPoint, stone_count> topleft{};
	std::array<std::uint8_t, stone_count> hit_flash{};
	std::array<std::int16_t, stone_count> kill_frame{};
	std::uint8_t phase{};
	std::uint8_t pattern{};
	std::int32_t timeout_frame{};
	std::int16_t phase_frame_unused{};
	std::uint8_t phase6_pair_angle{};
	std::uint8_t phase6_pellet_angle{};
	std::uint8_t sweep_angle{};
	std::array<std::uint8_t, stone_count> aimed_angle{};
	std::int16_t laser_left{};
	std::uint8_t laser_return_delay{};
	ScreenPoint muzzle{};
	std::int16_t tile_row{};
	std::array<std::uint8_t, 24U> tile_pending{};
	std::uint8_t tile_pass{};
	std::uint8_t tile_pair{};
	std::uint8_t tile_cols_done{};
	std::array<std::int16_t, 3U> tile_images{};
	bool operator==(const StonesState &) const = default;
};

enum class StonesSpriteKind : std::uint8_t { normal, hit_white, kill };

struct StonesSprite {
	StonesSpriteKind kind{};
	std::size_t stone{};
	ScreenPoint topleft{};
	std::int16_t patnum{};
	bool operator==(const StonesSprite &) const = default;
};

// These presentation seams retain the source's immediate rendering and sound
// positions. They cannot mutate the borrowed combat pools or stage timing.
class StonesCallbacks : public LaserSpawnCallbacks, public BossExplosionCallbacks {
public:
	void sound_effect(std::uint8_t sound_id) noexcept override = 0;
	virtual void render_stone(StonesSprite sprite) noexcept = 0;
	virtual void put_takeover_tile(std::int16_t left, std::int16_t top,
		std::int16_t image) noexcept = 0;
};

// Source globals used by one stones_update() frame. Stage scheduling, the
// boss intro/end, background invalidation, and enemies_remove_all() remain
// separate owners.
struct StonesCombatContext {
	PlayerFrameState &player_frame;
	PlayerHitState &player_hit;
	PlayerShotPoolState &shots;
	ItemPoolState &items;
	ScoreState &score;
	SparkPoolState &sparks;
	EnemyBulletPoolState &bullets;
	LaserPoolState &lasers;
	RandomState &random;
	std::uint32_t stage_frame{};
	std::int16_t playperf{};
	std::int8_t rank{};
	bool bombing{};
	std::uint8_t shot_level{};
	std::uint8_t bomb_damage_frame_mask{};
	std::int16_t scroll_line{};
	std::uint8_t &slowdown_factor;
	std::uint16_t &palette_tone;
};

enum class StonesUpdateResult : std::uint8_t {
	boss,
	stage_clear,
	invalid_shot_hittest,
	shot_spark_allocation_refused,
	kill_spark_allocation_refused,
	bullet_invalid_page,
	bullet_source_iatan2_fault,
	source_iatan2_fault,
};

// Combat-state portion of th02/main/boss/b3.cpp::stones_12778. The source's
// enclosing boss init owns dialogue, BGM, laser callback setup, and its
// enemies_remove_all() call.
void stones_combat_reset(BossState &boss, StonesState &stones,
	LaserPoolState &lasers, std::int8_t rank, std::int16_t scroll_line) noexcept;

// Combat-only translation of th02/main/boss/b3.cpp::stones_update and its
// private housekeeping, hit, defeat, pattern, and take-over helpers. A native
// refusal leaves the exact reached source prefix observable; no stage driver
// or replay-parity claim is implied.
[[nodiscard]] StonesUpdateResult stones_update(BossState &boss,
	StonesState &stones, StonesCombatContext &context,
	StonesCallbacks &callbacks) noexcept;

} // namespace th02::core
