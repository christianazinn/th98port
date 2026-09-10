#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace th01::core {

struct State;
enum class BossUpdateResult : std::uint8_t;

enum class KonngaraFaceDirection : std::int16_t {
	right = 0U,
	left = 1U,
	center = 2U,
	uninitialized = 9U,
};

enum class KonngaraFaceExpression : std::int16_t {
	neutral,
	closed,
	glare,
	aim,
};

template <std::size_t Count>
struct KonngaraSnakesState {
	std::array<std::array<std::int16_t, 5U>, Count> left{};
	std::array<std::array<std::int16_t, 5U>, Count> top{};
	std::array<std::int16_t, Count> velocity_x{};
	std::array<std::int16_t, Count> velocity_y{};
	std::array<std::int16_t, Count> target_left{};
	std::array<bool, Count> target_locked{};
};

// Source-local records in th01/main/boss/b20j.cpp. Shared HP and phase words
// remain in BossState, so its stage-20 sequence cannot be confused with a
// reusable boss-pattern machine.
struct KonngaraSlashSpawner {
	std::int16_t left{}, top{};
};

struct KonngaraState {
	std::int16_t hit_invincibility_frame{};
	std::int16_t pattern_previous{};
	std::int16_t pattern_current{};
	std::int16_t patterns_done{};
	std::int16_t pattern_state{};
	std::int16_t diamond_bottomleft_velocity_x{};
	std::int16_t diamond_bottomleft_velocity_y{};
	std::int16_t diamond_topleft_velocity_x{};
	std::int16_t diamond_topleft_velocity_y{};
	std::array<std::int16_t, 4U> diamond_left{};
	std::array<std::int16_t, 4U> diamond_top{};
	std::int16_t diamonds_at_edges_frame{};
	std::uint8_t cup_angle{};
	KonngaraSnakesState<2U> snakes_two{};
	KonngaraSnakesState<4U> snakes_four{};
	std::int16_t row_diamond_left{};
	std::int16_t row_diamond_top{};
	std::int16_t row_velocity_x{};
	std::int16_t row_velocity_y{};
	std::int16_t row_pellet_speed{};
	std::int16_t row_direction{};
	std::uint8_t spray_offset{};
	std::int16_t spray_delta{};
	std::int16_t pellets_fired_in_direction{};
	std::int16_t swordray_end_x{};
	std::int16_t swordray_end_y{};
	KonngaraSlashSpawner slash_rain{}, slash_triangular{}, slash_aimed{};
	std::int16_t laser_target_left{};
	std::int16_t laser_target_top{};
	KonngaraFaceDirection face_direction{KonngaraFaceDirection::center};
	KonngaraFaceExpression face_expression{KonngaraFaceExpression::neutral};
	bool hit_invincible{};
	bool face_direction_can_change{true};
	bool initial_hp_rendered{};
	bool lasers_right_to_left{};
};

void konngara_setup(State &) noexcept;
[[nodiscard]] BossUpdateResult konngara_update(State &) noexcept;
void konngara_render_tail(State &) noexcept;

} // namespace th01::core
