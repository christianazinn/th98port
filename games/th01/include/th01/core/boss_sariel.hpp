#pragma once

#include <array>
#include <cstdint>

namespace th01::core {

struct State;
enum class BossUpdateResult : std::uint8_t;

// b20m.cpp's bird pool. Hatch and flight positions survive across all form-1
// patterns, because birds spawned by one pattern can fire in the next one.
struct SarielBirdState {
	double left{};
	double top{};
	double velocity_x{};
	double velocity_y{};
	std::int8_t unknown{};
	std::int8_t hatch_time{};
	std::int8_t hatch_frames{};
	bool alive{};
};

// Each of b20m.cpp's three 2x2 particle systems has its own allocation pool.
// Their simulation is retained because allocation determines IRAND consumption.
struct SarielParticleState {
	double left{};
	double top{};
	double velocity{};
	std::int16_t age{};
	bool alive{};
};

struct SarielEggState {
	std::int16_t left{};
	std::int16_t top{};
};

struct SarielCurvedSprayState {
	std::int16_t target_left{};
	std::int16_t target_top{400};
	std::int16_t speed{};
	std::uint8_t angle{};
	std::int16_t subpattern_id{};
};

// DecimalSubpixel is int/10 in the donor. Keep it as integer tenths rather
// than host floating point so the leaf collision and random-recycle sequence
// retain their source thresholds.
struct SarielLeafState {
	std::int16_t left_tenths{};
	std::int16_t top_tenths{};
	std::int16_t velocity_x_tenths{};
	std::int16_t velocity_y_tenths{};
	std::int16_t flag{};
};

// Persistent source locals of th01/main/boss/b20m.cpp. Shared HP and phase
// words remain in BossState. The fields intentionally remain grouped by their
// direct owner pattern rather than becoming a generic boss-pattern carrier.
struct SarielState {
	std::int16_t hit_invincibility_frame{};
	std::int16_t entrance_ring_radius_base{};
	std::int16_t pattern_current{};
	std::int16_t pattern_u1{};
	std::int16_t patterns_until_next{};
	std::int16_t pattern_state{};
	std::int16_t bg_gust_id{};

	std::int16_t vortex_pattern_end{};
	std::array<std::int16_t, 2U> vortex_left{};
	std::array<std::int16_t, 2U> vortex_top{};
	std::int16_t vortex_first_direction{};
	std::int16_t vortex_second_direction{};

	std::array<std::int16_t, 10U> purple_laser_left{};
	std::array<std::int16_t, 10U> purple_laser_top{};
	std::int16_t purple_laser_speed{};

	std::array<SarielBirdState, 30U> birds{};
	std::array<SarielEggState, 30U> eggs{};
	std::int16_t eggs_alive{};
	std::int16_t bird_spawner_left_q12_4{};
	std::int16_t bird_spawner_top_q12_4{};
	std::int16_t bird_spawner_velocity_x_q12_4{};
	std::int16_t bird_spawner_velocity_y_q12_4{};
	std::int16_t bird_interval{};
	std::int16_t bird_pellet_group{};

	std::array<SarielParticleState, 30U> particles_vertical{};
	std::array<SarielParticleState, 30U> particles_wavy{};
	std::array<SarielParticleState, 30U> particles_horizontal{};

	std::int16_t snowflake_phase{};
	std::int16_t snowflake_left_q12_4{};
	std::int16_t snowflake_top_q12_4{};
	std::int16_t snowflake_velocity_x_q12_4{};
	std::int16_t snowflake_velocity_y_q12_4{};
	std::int16_t snowflake_outer_radius_1{};
	std::int16_t snowflake_outer_radius_2{};
	std::int16_t snowflake_inner_radius{};
	std::int16_t snowflake_star_left{};
	std::int16_t snowflake_star_top{};
	std::int16_t snowflake_start_frame{};
	std::int16_t snowflake_wand_frame{};
	std::uint8_t rings_angle{};
	std::int16_t rings_interval{};
	std::int16_t sling_interval{};

	std::int16_t four_laser_origin_left{};
	std::int16_t four_laser_origin_top{};
	std::int16_t four_laser_velocity_left{};
	std::int16_t four_laser_velocity_top{};
	std::int16_t four_laser_target_first{};
	std::int16_t four_laser_speed{};
	std::uint8_t radial_angle{};
	std::uint8_t radial_angle_velocity{};
	std::int16_t radial_laser_speed{};

	std::int16_t symmetric_ray_left{};
	std::int16_t symmetric_ray_top{};
	std::int16_t symmetric_interval{};
	std::int16_t vertical_ray_left{};
	std::int16_t vertical_ray_top{};
	std::int16_t vertical_interval{};

	SarielCurvedSprayState curved_once{};
	SarielCurvedSprayState curved_twice{};
	std::int16_t seal_rain_pellet_count{};
	std::int16_t seal_rain_target_left{};
	std::int16_t seal_rain_target_top{};
	std::int16_t seal_rain_debris_cel{};
	std::int16_t form2_leaf_interval{};
	std::array<SarielLeafState, 30U> leaves{};

	bool hit_invincible{};
	bool initial_hp_rendered{};
	bool vortex_wand_raised{};
	bool bird_wand_raised{};
	bool form2_ready{};
};

void sariel_setup(State &) noexcept;
[[nodiscard]] BossUpdateResult sariel_update(State &) noexcept;
void sariel_render_tail(State &) noexcept;

} // namespace th01::core
