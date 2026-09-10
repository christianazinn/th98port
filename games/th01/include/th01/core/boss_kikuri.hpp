#pragma once

#include <array>
#include <cstdint>

namespace th01::core {

struct State;
enum class BossUpdateResult : std::uint8_t;

// b15j.cpp uses CBossEntity for both of these fixed pools.  This deliberately
// holds only the source fields that its locked movement and collision code
// mutates; their bounds are fixed by the Kikuri source constants.
struct KikuriSoulState {
	std::int16_t left{};
	std::int16_t top{};
	std::int16_t previous_left{};
	std::int16_t previous_top{};
	std::int16_t previous_delta_x{};
	std::int16_t previous_delta_y{};
	std::int16_t lock_frame{};
	std::uint8_t image{};
};

struct KikuriTearState {
	std::int16_t left{};
	std::int16_t top{};
	std::int16_t previous_left{};
	std::int16_t previous_top{};
	std::int16_t previous_delta_x{};
	std::int16_t previous_delta_y{};
	std::int16_t lock_frame{};
	std::uint8_t image{};
};

// Source-local state in th01/main/boss/b15j.cpp.  The phase union is held as
// a word because phase 4 and phase 6 never use it concurrently, exactly like
// b15j.cpp's [kikuri_phase_state.u1].
struct KikuriState {
	std::int16_t hit_invincibility_frame{};
	std::int16_t entrance_ring_radius_base{};
	std::int16_t phase_u1{};
	std::int16_t patterns_done{};
	std::int16_t pattern_state{};
	std::int16_t phase_2_distance{};
	std::int16_t phase_6_random_range_x_half{};
	std::array<KikuriSoulState, 2U> souls{};
	std::array<KikuriTearState, 10U> tears{};
	std::array<std::int8_t, 10U> tear_animation_frame{};
	std::uint8_t phase_2_angle{};
	std::uint8_t phase_2_drift{};
	std::uint8_t phase_6_spiral_angle{};
	bool hit_invincible{};
	bool initial_hp_rendered{};
};

void kikuri_setup(State &) noexcept;
[[nodiscard]] BossUpdateResult kikuri_update(State &) noexcept;
void kikuri_render_tail(State &) noexcept;

} // namespace th01::core
