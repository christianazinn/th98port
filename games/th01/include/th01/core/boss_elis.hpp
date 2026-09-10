#pragma once

#include <array>
#include <cstdint>

namespace th01::core {

struct State;
enum class BossUpdateResult : std::uint8_t;

struct ElisTopleftState {
	std::int16_t left{};
	std::int16_t top{};
};

// b15m.cpp's separate fixed graphics records also retain coordinates used as
// future projectile origins.  The native core stores those source values even
// though their pixel presentation remains renderer-owned.
struct ElisState {
	ElisTopleftState girl{};
	ElisTopleftState attack{};
	ElisTopleftState bat{};
	std::array<ElisTopleftState, 5U> teleport_stars{};
	std::array<ElisTopleftState, 5U> phase_1_rifts{};
	std::array<ElisTopleftState, 5U> transform_to_bat_rifts{};
	std::array<ElisTopleftState, 5U> transform_to_girl_rifts{};
	std::array<ElisTopleftState, 5U> phase_3_rifts{};
	std::array<ElisTopleftState, 10U> spheres{};
	std::int16_t invincibility_frame{};
	std::int16_t phase_pattern{};
	std::int16_t phase_3_pattern{};
	std::int16_t phase_5_girl_pattern{};
	std::int16_t phase_5_bat_pattern{};
	std::int16_t phase_5_subphase{};
	std::int16_t bat_target_left{};
	std::int16_t bat_target_top{};
	std::int16_t bat_frames_until_target{};
	std::int16_t bat_velocity_x{};
	std::int16_t bat_velocity_y{};
	std::int16_t circle_frame{};
	std::int16_t circle_target_left{};
	std::int16_t pattern_value{};
	std::uint8_t form{};
	std::uint8_t circle_angle{};
	std::uint8_t circle_radius{};
	std::uint8_t pattern_subphase{};
	bool invincible{};
	bool teleport_done{};
	bool girl_orb_hitbox_inactive{};
	bool initial_hp_rendered{};
};

void elis_setup(State &) noexcept;
[[nodiscard]] BossUpdateResult elis_update(State &) noexcept;
void elis_render_tail(State &) noexcept;

} // namespace th01::core
