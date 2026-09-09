#pragma once

#include "th04/core/item.hpp"
#include "th04/core/player.hpp"
#include "th04/core/score.hpp"
#include "th04/core/shot_reimu_a.hpp"
#include "th04/core/shot_collision.hpp"
#include "th04/core/std.hpp"
#include "th04/core/bullet.hpp"
#include "th04/core/enemy.hpp"
#include "th04/core/boss.hpp"
#include "th04/core/thicklaser.hpp"
#include <array>
#include <bit>
#include <vector>

namespace th04::core {
using Point = pc98::th0405::SubpixelPoint;
using RandomState = pc98::th0405::RandomState;

struct BombStar {
	Point center{};
	std::uint8_t angle{}, speed{};
};
struct Rgb { std::uint8_t r{}, g{}, b{}; };

// Owned source globals. This is not a stage bootstrap: callers must supply
// the run's starting resources, RNG rings and character before execution.
struct State {
	Motion player_pos{};
	Point option_cur{}, option_prev{};
	std::uint16_t previous_movement{};
	std::uint8_t player_invincibility_time{}, miss_time{}, miss_move_lock_time{};
	bool player_is_hit{}, gameover_pending{};
	std::int16_t miss_explosion_radius{}, power_overflow{};
	std::uint8_t miss_explosion_angle{}, power{}, shot_level{}, playchar{}, shottype{};
	std::uint8_t bombs{}, credit_bombs{}, credit_lives{}, playperf_min{}, stage_id{};
	std::uint8_t miss_count{}, bombs_used{};
	std::uint16_t stage_frame{};
	std::uint8_t rank{};
	bool midboss_active{};
	MidbossState midboss{};
	BossState boss{};
	CircleState circles{};
	BossExplosionState boss_explosions{};
	bool boss_active{}, stage_clear_requested{};
	bool dialog_pending{};
	bool pointnum_times_2{};
	std::int16_t playfield_shake_x{}, playfield_shake_y{};
	std::int16_t playfield_shake_anim_time{};
	std::int8_t playfield_shake_redraw_time{};
	std::uint16_t total_std_frames{};
	std::int16_t scroll_last_delta{};
	StdState std_data{};
	BulletState bullets{};
	ThickLaserState thicklasers{};
	EnemyState enemies{};
	std::array<std::uint16_t, 25 * 32> tile_ring{};
	std::vector<std::uint8_t> map_data{};
	std::int16_t tile_ring_row_filled{};
	std::uint8_t scroll_subpixel_line{}, scroll_lines_pending{}, scroll_lines_prev_frame{};
	std::uint8_t page_back{};
	std::array<std::int16_t, 2> scroll_line_on_page{};
	ScoreState score{};
	ExtendState resources{};
	// randring1/randring2 are code-segment entry points over this same ring.
	RandomState random{};
	ShotState shots{};
	ShotSpawnState shot_spawn{};
	ShotCollisionState shot_collision{};
	std::array<pc98::th0405::Spark, pc98::th0405::SPARK_COUNT_TH04> sparks{};
	std::uint16_t spark_ring_offset{};
	ReimuAPatternState reimu_a{};
	ItemState items{};
	bool bombing{}, bombing_disabled{}, scroll_active{}, bomb_background{};
	std::uint8_t bomb_frame{}, circles_color{};
	std::int16_t scroll_line{};
	std::uint16_t palette_tone{100};
	bool palette_changed{};
	std::array<Rgb, 16> palette{};
	Rgb bomb_col14_backup{};
	std::array<BombStar, 48> bomb_stars{};
};

enum class HudField { power, lives, bombs, dream, point_items, graze, score };
// Notifications never supply gameplay outcomes or mutate the simulation.
struct Presentation {
	virtual ~Presentation() = default;
	virtual void sound(std::uint8_t) noexcept {}
	virtual void palette_show(std::uint16_t) noexcept {}
	virtual void hud(HudField) noexcept {}
	virtual void popup(std::uint8_t) noexcept {}
	virtual void pointnum(Point, std::uint16_t, bool) noexcept {}
	virtual void item_splash(Point) noexcept {}
	virtual void bomb_cel(std::uint8_t) noexcept {}
	virtual void bomb_background_put(std::uint8_t) noexcept {}
	virtual void bomb_star(Point, std::uint8_t) noexcept {}
	virtual void growing_circle(Point) noexcept {}
	virtual void graph_scroll(std::int16_t) noexcept {}
	virtual void playfield_shake(Point) noexcept {}
	virtual void thicklaser_geometry(const ThickLaserGeometry &) noexcept {}
};

inline void playperf_raise(State &state, std::uint8_t delta) noexcept
{
	const auto raised = static_cast<std::uint8_t>(state.resources.playperf + delta);
	state.resources.playperf = raised <= state.resources.playperf_max
		? raised : state.resources.playperf_max;
}
inline void playperf_lower(State &state, std::uint8_t delta) noexcept
{
	const auto lowered = static_cast<std::uint8_t>(state.resources.playperf - delta);
	state.resources.playperf = std::bit_cast<std::int8_t>(lowered) >=
		std::bit_cast<std::int8_t>(state.playperf_min) ? lowered : state.playperf_min;
}

void player_shot_level_update(State &state, Presentation &presentation) noexcept;
void score_update(State &state, Presentation &presentation) noexcept;
// False is an unsupported source state, not a completed gameplay step.
[[nodiscard]] bool player_update(State &state, std::uint16_t input, bool shift,
	Presentation &presentation) noexcept;
void player_bomb(State &state, Presentation &presentation) noexcept;
[[nodiscard]] bool player_miss_update(State &state, Presentation &presentation) noexcept;
[[nodiscard]] bool bomb_update(State &state, Presentation &presentation) noexcept;
} // namespace th04::core
