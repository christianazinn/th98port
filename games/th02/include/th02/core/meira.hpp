#pragma once

#include "th02/core/boss.hpp"
#include "th02/core/bullet.hpp"
#include "th02/core/random.hpp"
#include "th02/core/boss_explosion.hpp"
#include "th02/core/score.hpp"

namespace th02::core {
struct MeiraSlash {
	std::uint8_t flag{};
	std::uint8_t group{};
	std::int16_t left{}, top{};
	std::uint8_t angle{}, age{}, speed{}, trail_patnum{}, trail_frames{};
	bool operator==(const MeiraSlash &) const = default;
};
struct MeiraState {
	std::uint8_t phase{}, pattern{};
	std::int16_t defeat_frame{}, down_stream_speed{}, return_stream_speed{};
	ScreenPoint dash_origin{}, dash_target{};
	std::int16_t dash_step{};
	std::array<MeiraSlash, 40> slashes{};
	std::array<std::array<std::int16_t, 3>, 2> afterimage_left{}, afterimage_top{};
	std::int16_t afterimages_active{};
	std::uint8_t player_is_right{}, slash_trail_patnum{}, slash_burst_i{};
	std::int16_t slash_trail_frames{};
	std::uint8_t burst_group{}, burst_speed{};
};
struct MeiraInvalidation {
	ScreenPoint topleft{};
	std::int16_t width{}, height{};
};
enum class MeiraSpriteKind : std::uint8_t { slash_trail, slash_bloom, normal, hit_white, defeat_zoom, afterimage };
struct MeiraSprite {
	MeiraSpriteKind kind{};
	ScreenPoint topleft{};
	std::int16_t patnum{};
	std::int16_t zoom{};
	std::uint8_t color{};
};
class MeiraCallbacks : public BossExplosionCallbacks {
public:
	virtual ~MeiraCallbacks() = default;
	virtual void invalidate_meira(MeiraInvalidation) noexcept = 0;
	virtual void render_meira(MeiraSprite) noexcept = 0;
	virtual void sound_effect(std::uint8_t) noexcept = 0;
};

// Real slash pool operations from th02/main/boss/b2m.cpp. Full pool is a
// normal no-op for allocation; movement still occurs before that no-op.
void meira_slashes_add(MeiraState &meira, ScreenPoint origin,
	std::uint8_t angle, std::uint8_t group, std::uint8_t speed,
	std::uint8_t trail_frames) noexcept;
[[nodiscard]] bool meira_slash_step(BossState &boss, MeiraState &meira,
	std::size_t page_back, std::uint8_t mirrored,
	std::int16_t delta_x, std::int16_t delta_y) noexcept;
void meira_slashes_invalidate(MeiraState &meira, MeiraCallbacks &callbacks) noexcept;
[[nodiscard]] bool meira_bg_render(BossState &boss, MeiraState &meira,
	const PlayerFrameState &frame, MeiraCallbacks &callbacks) noexcept;
[[nodiscard]] EnemyBulletAddResult meira_slashes_update_and_render(MeiraState &meira,
	EnemyBulletPoolState &bullets, RandomState &random, const PlayerFrameState &frame,
	PlayerHitState &player, std::int16_t playperf, std::int8_t rank,
	bool bombing, std::int16_t scroll_line, MeiraCallbacks &callbacks) noexcept;
enum class MeiraDashPattern : std::uint8_t { fall, rising_slashes, double_slash, four_slashes };
// The enclosing update increments phase_frame before calling. These complete
// pattern bodies perform only their own resets/rewinds. The caller binds a
// valid back page before dispatch; no full boss dispatcher is implied here.
[[nodiscard]] bool meira_dash_pattern_update(BossState &boss, MeiraState &meira,
	const PlayerFrameState &frame, MeiraDashPattern pattern, MeiraCallbacks &callbacks) noexcept;

struct MeiraCombatContext {
	PlayerFrameState &frame;
	PlayerHitState &player;
	PlayerShotPoolState &shots;
	ScoreState &score;
	SparkPoolState &sparks;
	EnemyBulletPoolState &bullets;
	EnemyBulletSpecialState &special;
	RandomState &random;
	std::uint32_t stage_frame{};
	std::int16_t playperf{};
	std::int8_t rank{};
	bool bombing{};
	std::uint8_t shot_level{}, bomb_damage_frame_mask{};
	std::int16_t scroll_line{};
	std::uint8_t &slowdown_factor;
	std::uint16_t &palette_tone;
};
enum class MeiraUpdateResult : std::uint8_t {
	boss, stage_clear, invalid_page, invalid_shot_hittest,
	shot_spark_refused, defeat_spark_refused, bullet_invalid_page, bullet_iatan2_fault,
};
// Combat suffix only, after Meira's actual intro/dialogue. Retains the private
// clocks, slash pool, history and trailing rank parameters the source retains.
void meira_combat_setup_after_dialogue(BossState &boss, MeiraState &meira,
	EnemyBulletSpecialState &special, std::int8_t rank) noexcept;
[[nodiscard]] MeiraUpdateResult meira_update(BossState &boss, MeiraState &meira,
	MeiraCombatContext &context, MeiraCallbacks &callbacks) noexcept;
} // namespace th02::core
