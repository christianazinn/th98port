#pragma once

#include "th05/core/player.hpp"
#include "th05/core/item.hpp"
#include "th05/core/score.hpp"
#include "th05/core/extend.hpp"
#include "th05/core/shot_spawn.hpp"
#include "th05/core/bomb.hpp"
#include "th05/core/std.hpp"
#include "th05/core/boss.hpp"
#include "th05/core/bullet.hpp"
#include "th05/core/laser.hpp"
#include "th05/core/enemy.hpp"
#include "th05/core/stage_scenery.hpp"
#include "th05/core/sword.hpp"
#include "th05/core/b6ball.hpp"
#include "th05/core/firewave.hpp"
#include "th05/core/shinki_background.hpp"
#include "pc98/th0405_sparks.hpp"
#include <bit>

namespace th05::core {
using Point = pc98::th0405::SubpixelPoint;
using RandomState = pc98::th0405::RandomState;
struct Rgb { std::uint8_t r{}, g{}, b{}; };

// Owned gameplay globals, not an implicit run or stage initializer.
struct State {
	Motion player_pos{};
	MovementSpeeds movement_speeds{};
	Point option_cur{}, option_prev{};
	std::uint16_t previous_movement{}, stage_frame{};
	std::uint32_t total_std_frames{};
	std::uint8_t player_invincibility_time{}, miss_time{}, miss_move_lock_time{};
	bool player_is_hit{}, gameover_pending{};
	std::int16_t miss_explosion_radius{}, power_overflow{};
	std::uint8_t miss_explosion_angle{}, power{}, shot_level{}, playchar{};
	std::uint8_t bombs{}, credit_bombs{}, credit_lives{}, playperf_min{}, rank{}, stage_id{};
	std::uint8_t miss_count{}, bombs_used{};
	std::uint8_t bonus_previous_miss_count{}, bonus_previous_bombs_used{};
	std::uint16_t graze_score{}, total_graze{};
	bool turbo_mode{};
	ExtendState resources{};
	ScoreState score{};
	RandomState random{};
	ShotUpdateState shots{};
	ShotSpawnState shot_spawn{};
	ItemState items{};
	BombState bomb{};
	StdState std_data{};
	MidbossState midboss{};
	BossState boss{};
	AliceState alice{};
	Stage4State stage4{};
	Stage5State stage5{};
	Stage6State stage6{};
	ExtraState extra{};
	FirewaveState firewaves{};
	ShinkiBackgroundState shinki_background{};
	SwordState swords{};
	B6BallState b6balls{};
	BossExplosionState boss_explosions{};
	BulletState bullets{};
	LaserState lasers{};
	EnemyState enemies{};
	Stage2Scenery stage2_scenery{};
	std::array<Rgb, 16> palette{};
	bool midboss_active{}, boss_active{}, stage_clear_requested{}, dialog_pending{};
	// The installed update survives midboss_active becoming false during defeat.
	bool midboss_callback_active{};
	std::int16_t playfield_shake_anim_time{}, playfield_shake_x{}, playfield_shake_y{};
	std::int8_t playfield_shake_redraw_time{};
	std::uint8_t shot_hit_spark_parity{};
	std::uint8_t slowdown_factor{1};
	bool slowdown_caused_by_bullets{};
	std::vector<std::uint8_t> map_data;
	std::array<std::uint16_t, 25 * 32> tile_ring{};
	std::array<std::int16_t, 2> scroll_line_on_page{};
	std::int16_t tile_ring_row_filled{}, scroll_last_delta{};
	std::uint8_t page_back{}, scroll_subpixel_line{}, scroll_lines_pending{}, scroll_lines_prev_frame{};
	std::array<pc98::th0405::Spark, pc98::th0405::SPARK_COUNT_TH05> sparks{};
	std::uint16_t spark_ring_offset{};
	bool pointnum_times_2{}, bombing{}, bombing_disabled{}, bomb_background{}, scroll_active{};
	std::uint8_t bomb_frame{}, circles_color{}, tiles_bb_color{};
	std::int16_t scroll_line{};
	std::uint16_t palette_tone{100};
	bool palette_changed{};
};

enum class HudField { power, lives, bombs, dream, point_items, score, graze };
// Notifications cannot supply a gameplay result or mutate the state.
struct Presentation {
	virtual ~Presentation() = default;
	virtual void sound(std::uint8_t) noexcept {}
	virtual void hud(HudField) noexcept {}
	virtual void popup(std::uint8_t) noexcept {}
	virtual void pointnum(Point, std::uint16_t, bool) noexcept {}
	virtual void item_splash(Point) noexcept {}
	virtual void bomb_cel(std::uint8_t) noexcept {}
	virtual void bomb_background_put(std::uint8_t) noexcept {}
	virtual void graph_scroll(std::int16_t) noexcept {}
	virtual void palette_show(std::uint16_t) noexcept {}
	virtual void bomb_geometry(const BombGeometry &) noexcept {}
	virtual void boss_hp(std::int16_t, std::int16_t) noexcept {}
	virtual void laser_geometry(const LaserGeometry &) noexcept {}
	virtual void playfield_shake(Point) noexcept {}
};

inline void playperf_raise(State &s, std::uint8_t delta) noexcept
{
	const auto value = static_cast<std::uint8_t>(s.resources.playperf + delta);
	s.resources.playperf = value <= s.resources.playperf_max ? value : s.resources.playperf_max;
}
inline void playperf_lower(State &s, std::uint8_t delta) noexcept
{
	const auto value = static_cast<std::uint8_t>(s.resources.playperf - delta);
	s.resources.playperf = std::bit_cast<std::int8_t>(value) >=
		std::bit_cast<std::int8_t>(s.playperf_min) ? value : s.playperf_min;
}
void player_shot_level_update(State &, Presentation &) noexcept;
void score_update(State &, Presentation &) noexcept;
// Run globals only; scorefile I/O and stage bootstrap are separate host steps.
[[nodiscard]] bool gameplay_init(State &, std::uint8_t rank, std::uint8_t playchar,
	std::uint8_t lives, std::uint8_t bombs, std::uint8_t stage_id,
	std::uint8_t demo_num = 0, bool turbo = false) noexcept;
[[nodiscard]] bool player_miss_update(State &, Presentation &) noexcept;
[[nodiscard]] bool player_update(State &, std::uint16_t input, bool shift, Presentation &) noexcept;
}
