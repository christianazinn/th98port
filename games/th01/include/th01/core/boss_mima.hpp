#pragma once

#include <array>
#include <cstdint>

namespace th01::core {

struct State;
enum class BossUpdateResult : std::uint8_t;

// th01/main/boss/b10j.cpp's individual rotating-square owners.  They remain
// separate below because their radius/angle lifetimes overlap between phases.
struct MimaSquareState {
	std::uint8_t angle{};
	std::int16_t radius{};
};

// One of b10j.cpp's eight flame-pillar records.
struct MimaPillarState {
	std::int16_t time{};
	std::int16_t center_x{};
	std::int16_t bottom{};
};

// Source-local state in b10j.cpp.  Shared boss phase, HP, and active words
// deliberately stay in BossState; no generic fight-pattern carrier is used.
struct MimaState {
	std::int16_t left{};
	std::int16_t top{};
	std::int16_t target_left{};
	std::int16_t hit_invincibility_frame{};
	std::int16_t pattern_cur{};
	std::int16_t pattern_state{};
	std::array<MimaPillarState, 8U> pillars{};
	std::array<std::int16_t, 4U> laser_corner_x{};
	std::array<std::int16_t, 4U> laser_corner_y{};
	MimaSquareState square_aimed_pellets{};
	MimaSquareState square_aimed_missiles{};
	MimaSquareState square_two_pellets{};
	MimaSquareState square_halfcircle_missiles{};
	MimaSquareState square_slow_spray{};
	MimaSquareState square_lasers{};
	std::uint8_t missile_angle{};
	std::uint8_t pellet_angle{};
	std::uint8_t hop{};
	std::int8_t hop_direction{};
	std::uint8_t spreadin_interval{4U};
	std::int8_t spreadin_speed{8};
	bool hit_invincible{};
	bool initial_hp_rendered{};
	bool meteor_active{true};
	bool orb_hitbox_inactive{};
};

void mima_setup(State &) noexcept;
[[nodiscard]] BossUpdateResult mima_update(State &) noexcept;
void mima_render_tail(State &) noexcept;

} // namespace th01::core
