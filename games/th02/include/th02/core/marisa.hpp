#pragma once

#include "th02/core/boss.hpp"
#include "th02/core/boss_explosion.hpp"
#include "th02/core/bg_particle.hpp"
#include "th02/core/bullet.hpp"
#include "th02/core/item.hpp"
#include "th02/core/random.hpp"
#include "th02/core/score.hpp"
#include "th02/core/spark.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

inline constexpr std::size_t marisa_orb_count = 4U;

enum class MarisaOrbFlag : std::int16_t {
	alive = 0,
	kill_anim = 1,
	removed = 2,
};

// The first four fields alias Stage 4's M4 storage. The following fields are
// private to b4.cpp's Marisa combat body.
struct MarisaState {
	ScreenPoint topleft{};
	std::int16_t intro_step{};
	std::int16_t intro_direction{};
	bool hit_flash{};
	std::int16_t velocity_x{};
	std::int16_t velocity_y{};
	std::array<std::int16_t, marisa_orb_count> orb_flag{};
	std::array<std::int16_t, marisa_orb_count> orb_damage{};
	std::array<std::int16_t, marisa_orb_count> orb_hit_flash{};
	std::array<std::int16_t, marisa_orb_count> orb_kill_frame{};
	std::array<std::array<ScreenPoint, marisa_orb_count>, 2U> orb_on_page{};
	std::array<std::int16_t, marisa_orb_count> orb_radius{};
	std::array<std::uint8_t, marisa_orb_count> orb_angle{};
	std::array<std::int16_t, marisa_orb_count> orb_angle_delta{};
	std::int16_t orb_flag_sum{};
	std::uint8_t volleys_fired{};
	std::array<std::uint8_t, marisa_orb_count> orb_volley_angle{};
	std::uint8_t spray_angle{};
	std::uint8_t drift_star_angle{};
	std::uint8_t stream_angle{};
	std::uint8_t orb_pair_angle{};
	std::int8_t swoop_direction{};
	ScreenPoint swoop_center{};
	std::uint8_t swoop_angle{};
	std::int8_t pattern{};
	std::int8_t pattern_side{1};
	std::uint8_t star_drift_angle{2U};
	bool spray_is_first_run{};
	std::uint8_t patterns_seen{};
	std::int8_t orbless_patterns_seen{};
	std::uint8_t rounds_done{};
	std::int16_t defeat_frame{};
	std::uint8_t damage_multiplier{};
	std::int8_t bg_particle_col_i{};
	bool operator==(const MarisaState &) const = default;
};

enum class MarisaSpriteKind : std::uint8_t { normal, hit_white, defeat_zoom };

struct MarisaSprite {
	MarisaSpriteKind kind{};
	ScreenPoint topleft{};
	std::int16_t patnum{};
	std::int16_t zoom{};
	bool operator==(const MarisaSprite &) const = default;
};

enum class MarisaOrbSpriteKind : std::uint8_t { normal, hit_white, kill };

struct MarisaOrbSprite {
	MarisaOrbSpriteKind kind{};
	std::size_t orb{};
	ScreenPoint topleft{};
	std::int16_t patnum{};
	bool operator==(const MarisaOrbSprite &) const = default;
};

// Synchronous source-position presentation seams. Background clearing and
// Stage 4 particle orchestration remain their caller's separate owners.
class MarisaCallbacks : public BossExplosionCallbacks {
public:
	void sound_effect(std::uint8_t sound_id) noexcept override = 0;
	virtual void render_marisa(MarisaSprite sprite) noexcept = 0;
	virtual void render_orb_line(ScreenPoint from, ScreenPoint to) noexcept = 0;
	virtual void render_orb(MarisaOrbSprite sprite) noexcept = 0;
	virtual void clear_orb_on_front_page(std::size_t orb,
		ScreenPoint topleft) noexcept = 0;
};

// Source owners borrowed by one marisa_update() combat call. Page carry,
// background clearing, particle effects, intro, ending, and frame scheduling
// remain outside this body.
struct MarisaCombatContext {
	PlayerFrameState &player_frame;
	PlayerHitState &player_hit;
	PlayerShotPoolState &shots;
	ItemPoolState &items;
	ScoreState &score;
	SparkPoolState &sparks;
	EnemyBulletPoolState &bullets;
	EnemyBulletSpecialState &bullet_special;
	RandomState &random;
	BgParticleState &background_particles;
	BgParticleCallbacks &background_particle_callbacks;
	std::uint32_t stage_frame{};
	bool reduce_effects{};
	std::int16_t playperf{};
	std::int8_t rank{};
	bool bombing{};
	std::uint8_t shot_level{};
	std::uint8_t &bomb_damage_frame_mask;
	std::uint8_t player_power{};
	std::int16_t scroll_line{};
	std::uint8_t &slowdown_factor;
	std::uint16_t &palette_tone;
};

enum class MarisaUpdateResult : std::uint8_t {
	boss,
	stage_clear,
	invalid_page,
	invalid_shot_hittest,
	shot_spark_allocation_refused,
	defeat_spark_allocation_refused,
	orb_spark_allocation_refused,
	bullet_invalid_page,
	bullet_source_iatan2_fault,
};

// Combat-state tail of th02/main/boss/b4.cpp::marisa_init. Dialogue, BGM,
// page/VRAM setup, particle setup, and the stage lifecycle remain external.
void marisa_combat_reset(BossState &boss, MarisaState &marisa,
	std::int8_t rank) noexcept;

// Combat portion of th02/main/boss/b4.cpp::marisa_update and its private
// hit, orb, defeat, movement, and pattern helpers. Native refusals retain the
// reached source prefix. The caller prepares current back-page boss/orb data.
[[nodiscard]] MarisaUpdateResult marisa_update(BossState &boss,
	MarisaState &marisa, MarisaCombatContext &context,
	MarisaCallbacks &callbacks) noexcept;

} // namespace th02::core
