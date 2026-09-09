#pragma once

#include "th04/core/item.hpp"
#include "th04/core/player.hpp"
#include "th04/core/score.hpp"
#include "th04/core/shot_reimu_a.hpp"
#include <array>
#include <bit>

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
	std::uint8_t bombs{}, credit_bombs{}, playperf_min{};
	std::uint8_t miss_count{}, bombs_used{};
	std::uint16_t stage_frame{};
	ScoreState score{};
	ExtendState resources{};
	RandomState random{}, random2{};
	ShotState shots{};
	ShotSpawnState shot_spawn{};
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

enum class HudField { power, lives, bombs, dream, point_items };
// Notifications never supply gameplay outcomes or mutate the simulation.
struct Presentation {
	virtual ~Presentation() = default;
	virtual void sound(std::uint8_t) noexcept {}
	virtual void hud(HudField) noexcept {}
	virtual void popup(std::uint8_t) noexcept {}
	virtual void pointnum(Point, std::uint16_t, bool) noexcept {}
	virtual void item_splash(Point) noexcept {}
	virtual void bomb_cel(std::uint8_t) noexcept {}
	virtual void bomb_background_put(std::uint8_t) noexcept {}
	virtual void bomb_star(Point, std::uint8_t) noexcept {}
	virtual void growing_circle(Point) noexcept {}
	virtual void graph_scroll(std::int16_t) noexcept {}
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
// False is an unsupported source state, not a completed gameplay step.
[[nodiscard]] bool player_update(State &state, std::uint16_t input, bool shift,
	Presentation &presentation) noexcept;
void player_bomb(State &state, Presentation &presentation) noexcept;
[[nodiscard]] bool player_miss_update(State &state, Presentation &presentation) noexcept;
[[nodiscard]] bool bomb_update(State &state, Presentation &presentation) noexcept;
} // namespace th04::core
