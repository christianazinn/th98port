#pragma once

#include "th01/core/shot_orb.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace th01::core {

struct State;

// TH01 stores pellet coordinates, velocities, and per-motion speeds in Q12.4.
inline constexpr std::int16_t pellet_subpixel_factor = 16;

[[nodiscard]] constexpr std::int16_t pellet_pixels_to_subpixels(
	std::int16_t pixels
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(pixels) << 4U
	));
}

// `pellet_speed_t` from pellet_s.hpp: pixels multiplied by 40.
[[nodiscard]] constexpr std::int16_t pellet_speed_from_fortieths(
	std::int16_t fortieths
) noexcept
{
	return fortieths;
}

enum class PelletMotion : std::uint8_t {
	regular = 0U,
	gravity = 1U,
	sling_aimed = 2U,
	bounce_from_top_then_gravity = 3U,
	fall_straight_from_top_then_regular = 4U,
	spin = 5U,
	chase = 6U,
};

enum class PelletSlingDirection : std::int16_t {
	none = 0,
	clockwise = 1,
	counterclockwise = 2,
};

// Values are the source's deliberately non-contiguous pellet_group_t values.
enum class PelletGroup : std::int16_t {
	none = 0,
	one = 1,
	one_aimed = 12,
	two_spread_wide = 13,
	two_spread_narrow = 14,
	three_spread_wide = 15,
	three_spread_narrow = 16,
	four_spread_wide = 17,
	four_spread_narrow = 18,
	five_spread_wide = 19,
	five_spread_narrow = 20,
	two_spread_wide_aimed = 21,
	two_spread_narrow_aimed = 22,
	three_spread_wide_aimed = 23,
	three_spread_narrow_aimed = 24,
	four_spread_wide_aimed = 25,
	four_spread_narrow_aimed = 26,
	five_spread_wide_aimed = 27,
	five_spread_narrow_aimed = 28,
	one_random_narrow_aimed = 29,
	one_random_wide = 30,
};

struct Pellet {
	bool moving{};
	PelletMotion motion{PelletMotion::regular};
	std::int16_t cur_left{}, cur_top{};
	std::int16_t spin_center_left{}, spin_center_top{};
	std::int16_t prev_left{}, prev_top{};
	PelletGroup from_group{PelletGroup::none};
	std::int16_t velocity_x{}, velocity_y{};
	std::int16_t spin_velocity_x{}, spin_velocity_y{};
	bool not_rendered{};
	std::int16_t age{};
	std::int16_t speed{};
	std::int16_t decay_frame{}, cloud_frame{};
	std::int16_t cloud_left{}, cloud_top{};
	std::int16_t angle{};
	PelletSlingDirection sling_direction{PelletSlingDirection::none};
	bool operator==(const Pellet &) const = default;
};

enum class MissileFlag : std::uint8_t {
	free = 0U,
	moving = 1U,
	hit = 2U,
	hit_last = 3U,
};

struct Missile {
	std::int16_t cur_left{}, cur_top{};
	std::int16_t prev_left{}, prev_top{};
	std::int16_t velocity_x{}, velocity_y{};
	std::int16_t unused{};
	std::int8_t unknown{};
	MissileFlag flag{MissileFlag::free};
	bool operator==(const Missile &) const = default;
};

// The shootout laser's coordinate components directly model master.lib's
// Q24.8 Sin8/Cos8 results, separately from the pellets' Q12.4 space.
struct ShootoutLaser {
	std::int32_t origin_left{}, origin_top{};
	std::int32_t ray_start_left{}, ray_start_top{};
	std::int32_t ray_i_left{}, ray_i_top{};
	std::int16_t ray_length{}, ray_moveout_speed{};
	std::int16_t target_left{}, target_top{};
	std::int16_t unknown{};
	std::int32_t unused_1{};
	std::int32_t velocity_y{}, step_y{}, velocity_x{}, step_x{};
	std::int16_t ray_extend_speed{}, unused_2{};
	bool alive{};
	std::int16_t age{}, moveout_at_age{};
	std::uint8_t color{}, width_cel{};
	bool damaging{};
	std::int8_t id{};
	bool put_flag{};
	bool operator==(const ShootoutLaser &) const = default;
};

struct ProjectileState {
	std::array<Pellet, 100U> pellets{};
	std::int16_t pellet_alive_count{};
	std::int16_t pellet_unknown_seven{};
	bool pellet_interlace{};
	bool pellet_interlace_field{};
	bool pellet_spawn_with_cloud{};
	std::int16_t pellet_speed_x40{};
	std::uint16_t pellet_destroy_score_delta{};

	std::array<Missile, 50U> missiles{};
	std::uint8_t missile_ptn_id_base{};

	std::array<ShootoutLaser, 10U> shootout_lasers{};
	bool operator==(const ProjectileState &) const = default;
};

void pellets_add_group(
	State &state,
	std::int16_t left,
	std::int16_t top,
	PelletGroup group,
	std::int16_t speed_base_q12_4
) noexcept;
void pellets_add_single(
	State &state,
	std::int16_t left,
	std::int16_t top,
	std::uint8_t angle,
	std::int16_t speed_base_q12_4,
	PelletMotion motion = PelletMotion::regular,
	std::int16_t fixed_motion_speed_q12_4 = 0,
	std::int16_t spin_center_left = 0,
	std::int16_t spin_center_top = 0
) noexcept;

void pellet_speed_lower(
	State &state, std::int16_t max_x40, std::int16_t negative_delta_x40
) noexcept;
void pellet_speed_raise(State &state, std::int16_t delta_x40) noexcept;

// Source order: update the pellet pool, then CShots::unput_update_render().
// The status preserves the existing native scalar refusal boundary.
[[nodiscard]] ShotOrbUpdateStatus pellets_update_and_shots(State &state) noexcept;
void pellets_decay(State &state) noexcept;
void pellets_reset_nonclouds(State &state) noexcept;
void pellets_unput_and_reset_nonclouds(State &state) noexcept;
void pellet_destroy_score_delta_commit(State &state) noexcept;

void missiles_set_ptn_id_base(State &state, std::uint8_t ptn_id_base) noexcept;
void missiles_add(
	State &state,
	std::int16_t left,
	std::int16_t top,
	std::int16_t velocity_x_q12_4,
	std::int16_t velocity_y_q12_4,
	std::int8_t unknown = 0
) noexcept;
void missiles_reset(State &state) noexcept;
void missiles_update(State &state) noexcept;

void shootout_lasers_init(State &state) noexcept;
void shootout_laser_spawn(
	State &state,
	std::size_t slot,
	std::int16_t origin_left,
	std::int16_t origin_top,
	std::int16_t target_left,
	std::int16_t target_top,
	std::int16_t speed_multiplied_by_8,
	std::uint8_t color,
	std::int16_t moveout_at_age,
	std::int16_t width
) noexcept;
void shootout_laser_spawn_safe(
	State &state,
	std::size_t index,
	std::int16_t origin_left,
	std::int16_t origin_top,
	std::int16_t target_left,
	std::int16_t target_top,
	std::int16_t speed_multiplied_by_8,
	std::uint8_t color,
	std::int16_t moveout_at_age,
	std::int16_t width
) noexcept;
void shootout_laser_update(State &state, std::size_t slot) noexcept;
void shootout_lasers_update(State &state) noexcept;
void shootout_lasers_reset_first(State &state, std::size_t count) noexcept;

// `linebullet_*` immediately render and test their right/bottom endpoint in
// source. Presentation is intentionally owned by the renderer; these helpers
// preserve the only gameplay side effect.
void linebullet_unput(
	State &state,
	std::int16_t left,
	std::int16_t top,
	std::int16_t right,
	std::int16_t bottom
) noexcept;
void linebullet_put_and_hittest(
	State &state,
	std::int16_t left,
	std::int16_t top,
	std::int16_t right,
	std::int16_t bottom,
	std::uint8_t color
) noexcept;
void linebullet_put_patterned_and_hittest(
	State &state,
	std::int16_t left,
	std::int16_t top,
	std::int16_t right,
	std::int16_t bottom,
	std::uint8_t color,
	std::uint16_t pattern
) noexcept;

} // namespace th01::core
