#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstdint>

namespace th05::core {

struct State;
struct Presentation;

using BossPoint = pc98::th0405::SubpixelPoint;
using BossMotion = pc98::th0405::Motion;

// th04/main/midboss/midboss.hpp. Stage 1 uses the shared boss angle as its
// bullet angle, leaving this record's angle under the normal source contract.
struct MidbossState {
	BossMotion pos{};
	// th05_main.asm's Stage 2 scratch point (sppoint_2CE26), retained between
	// Midboss 2 orbit updates rather than recomputed from the visible motion.
	BossPoint stage2_center{};
	std::uint16_t frames_until{};
	std::int16_t hp{};
	std::uint8_t sprite{}, phase{};
	std::int16_t phase_frame{};
	std::uint8_t damage_this_frame{}, angle{};
	bool operator==(const MidbossState &) const = default;
};

// th04/main/boss/boss.hpp. `statebytes` backs each source boss's local state
// overlay directly: Sara consumes bytes 9, 10, and 13..15; Louise consumes
// bytes 10, 14, and 15.
struct BossState {
	BossMotion pos{};
	std::int16_t hp{};
	std::uint8_t sprite{}, phase{};
	std::int16_t phase_frame{};
	std::uint8_t damage_this_frame{}, mode{}, angle{}, phase_state{};
	std::int16_t phase_end_hp{};
	std::array<std::uint8_t, 16U> statebytes{};
	BossPoint hitbox_radius{};
	std::uint8_t sprite_left{}, sprite_right{}, sprite_stay{};
	std::int16_t flystep_next_y_direction{};
	// th05_main.asm's separate `sara_phase_2_3_pattern` pointer, represented
	// by its 1..4 table index so copied native state stays self-contained.
	std::uint8_t selected_pattern{};
	bool phase_timed_out{true};
	bool operator==(const BossState &) const = default;
};

// th04/main/boss/explode.hpp. The update belongs to the foreground render
// tail, after the combat update that may have spawned one of these records.
struct BossExplosion {
	bool alive{};
	std::uint8_t age{};
	BossPoint center{}, radius_cur{}, radius_delta{};
	std::int8_t unused{};
	std::uint8_t angle_offset{};
	bool operator==(const BossExplosion &) const = default;
};

struct BossExplosionState {
	std::array<BossExplosion, 2U> small{};
	BossExplosion big{};
	std::int16_t big_frame{};
	bool operator==(const BossExplosionState &) const = default;
};

// th05/main/boss/b3puppet.hpp. Stage 3 overlays exactly two of the source
// custom-entity records with these fields. `radius` preserves the source
// union: spiral motion and gather-render radius never overlap in time.
struct PuppetState {
	BossMotion pos{};
	std::uint16_t phase_frame{};
	std::int16_t radius{}, hp{}, damage_this_frame{};
	std::uint8_t flag{}, angle{}, patnum{};
	bool operator==(const PuppetState &) const = default;
};

enum class PuppetPattern : std::uint8_t {
	random_pellets,
	aimed_spread_stack,
	aimed_spread,
	aimed_ring,
	global_spread,
	idle_wait,
	never,
};

enum class AlicePattern : std::uint8_t {
	random_balls_and_pellets,
	ring_stacks,
	spread_stacks,
	lasers_and_random_balls,
	mirrored_stacks,
	rotating_ring,
};

// Alice's two source callback pointers and barrier words are ordinary BSS,
// separate from the custom-entity puppet storage. Stage initialization clears
// only `puppets`; callers must retain the remaining source globals.
struct AliceState {
	std::array<PuppetState, 2U> puppets{};
	std::array<PuppetPattern, 2U> puppet_patterns{};
	std::uint16_t barrier_frame{}, barrier_fire_frames{};
	AlicePattern boss_pattern{};
	bool operator==(const AliceState &) const = default;
};

// th05/main/bullet/b4ball.hpp and cheeto.hpp. The source overlays the first
// seven B4 ball records as cheeto heads; CheetoHead is the adapter's local
// snapshot shape, while trails remain distinct source BSS in Stage4State.
struct B4Ball {
	std::uint8_t flag{}, angle{};
	BossMotion pos{};
	std::uint16_t age{};
	bool revenge{};
	std::int16_t patnum_tiny_base{}, hp{}, damage_this_frame{};
	std::uint8_t speed{};
	bool operator==(const B4Ball &) const = default;
};

struct B4BallTemplate {
	std::uint8_t angle{};
	BossPoint origin{};
	bool revenge{};
	std::int16_t patnum_tiny_base{}, hp{};
	std::uint8_t speed{};
	bool operator==(const B4BallTemplate &) const = default;
};

enum class CheetoFlag : std::uint8_t { free, decelerate, speedup };

struct CheetoHead {
	CheetoFlag flag{};
	std::uint8_t angle{};
	BossMotion pos{};
	std::uint16_t age{};
	std::int16_t sprite{};
	std::uint8_t speed{};
	bool operator==(const CheetoHead &) const = default;
};

struct CheetoTrail {
	CheetoFlag flag{};
	std::int8_t color{};
	std::array<BossPoint, 16U> nodes{};
	std::array<std::uint8_t, 16U> sprites{};
	bool operator==(const CheetoTrail &) const = default;
};

struct CheetoTemplate {
	std::uint8_t angle{};
	BossPoint origin{};
	std::int8_t color{};
	std::uint8_t speed{};
	bool operator==(const CheetoTemplate &) const = default;
};

enum class Stage4PairMaiPattern : std::uint8_t {
	blue_cross_stacks, turning_blue_spread, rotating_pellets, blue_ball_spread,
	manual_laser, blue_ball_ring, cheetos,
};
enum class Stage4PairYukiPattern : std::uint8_t {
	exact_linear_spread, red_ring, red_stack, red_ball_spread, red_big_stack,
	random_pellets, rotating_red_ring, cheetos,
};
enum class Stage4SoloPattern : std::uint8_t {
	mai_spread_pairs, mai_turning_stacks, mai_spread_balls, mai_spread_stacks,
	mai_revenge_balls, mai_fixed_lasers, yuki_ring, yuki_random_spread,
	yuki_pellet_ring, yuki_spread_balls, yuki_ball_cross, yuki_sweeping_stacks,
	yuki_speedup_cloud,
};
enum class Stage4MaiLaserPattern : std::uint8_t { spread, ring, ball_triplet };
enum class Stage4Fight : std::uint8_t { pair, mai_solo, yuki_solo };

// The custom pool is reset by stage_init. `trails`, selectors, and laser
// words are ordinary source BSS and deliberately retain their contents.
struct Stage4CustomEntities {
	B4BallTemplate b4ball_template{};
	std::array<B4Ball, 63U> b4balls{};
	bool operator==(const Stage4CustomEntities &) const = default;
};

struct Stage4State {
	BossState yuki{};
	Stage4CustomEntities custom{};
	std::array<CheetoTrail, 7U> cheeto_trails{};
	Stage4PairMaiPattern pair_mai_pattern{};
	Stage4PairYukiPattern pair_yuki_pattern{};
	Stage4SoloPattern solo_pattern{};
	Stage4MaiLaserPattern mai_laser_pattern{};
	std::int16_t mai_laser_count{}, mai_laser_angle_speed{}, mai_laser_angle_progress{};
	// m4_updt.cpp's separate BSS center for the two horizontal warp shakes.
	// It is not part of the Stage 4 custom pool and deliberately retains its
	// previous value until Midboss 4 starts a new warp on frame 16.
	std::int16_t midboss_warp_x{};
	Stage4Fight fight{};
	bool pair_defeated_yuki{};
	bool operator==(const Stage4State &) const = default;
};

// th05/main/midboss/m5_updt.cpp and boss/b5.cpp carry their selected source
// callbacks in ordinary BSS.  The native state keeps the choices explicit;
// intervals are written by Stage 5 setup, not by the custom-pool reset.
enum class Stage5MidbossPattern : std::uint8_t {
	blue_spreads,
	random_green_rings,
	aimed_pellet_stacks,
};
enum class Stage5YumekoPattern : std::uint8_t {
	phase2_sword_fan,
	phase2_red_ball_ring,
	phase4_arrowheads_and_sword,
	phase5_exact_linear_pairs,
	phase5_green_arrowheads,
	phase7_arrowheads_laser_swords,
	phase8_swords_and_spreads,
	phase10_turning_rings,
};
struct Stage5State {
	Stage5MidbossPattern midboss_pattern{};
	Stage5YumekoPattern yumeko_pattern{};
	std::uint8_t interval_phase4{}, interval_phase7{};
	bool operator==(const Stage5State &) const = default;
};

// th05/main/boss/b6.cpp's two callback pointers are ordinary BSS, while its
// laser-delay word and float-direction byte are initialized DATA. None of
// these are custom-entity storage or reset by stage initialization.
enum class Stage6Phase23Pattern : std::uint8_t {
	none,
	curved_rings,
	dualspeed_rings,
	random_directional_and_kunai,
	dense_blue_stacks,
};
enum class Stage6WingPattern : std::uint8_t {
	none,
	random_rain_and_spreads,
	cheetos_within_spread_walls,
	aimed_b6balls_and_symmetric_spreads,
	devil,
};
struct Stage6State {
	Stage6Phase23Pattern phase_2_3_pattern{};
	Stage6WingPattern wing_pattern{};
	std::uint16_t devil_laser_grow_delay{};
	bool float_direction{};
	bool operator==(const Stage6State &) const = default;
};

// th05/main/boss/bx_updt.cpp's callable pattern table plus the four direct
// callbacks installed between its pattern groups.  The selector models the
// source function pointer without inventing a callback framework.
enum class ExtraPattern : std::uint8_t {
	random_red_spreads,
	rotating_red_rings,
	spreads_and_firewaves,
	gravity_balls_and_stacks,
	rotating_blue_rings,
	bouncing_blue_rings,
	lasers_and_green_rings,
	speedup_spreads,
	pingpong_lasers,
	aimed_crosses,
	aimed_cheetos,
	mirrored_crosses,
};

// BX_TEXT/BX_UPDATE_TEXT mix BSS callbacks with two initialized DATA words:
// `pattern_origin_x` starts centered and `laser_slot` starts at zero.  They
// survive stage initialization; b6balls is the separate custom pool that
// stores Extra's Cheeto heads.
struct ExtraState {
	ExtraPattern pattern{};
	std::uint8_t invincibility_frames{};
	std::int16_t pattern_origin_x{3072};
	BossPoint random_origin{};
	std::uint8_t laser_slot{};
	std::uint16_t overlay_patnum{};
	bool custom_render_active{};
	bool operator==(const ExtraState &) const = default;
};

// th04/main/midboss/defeat.cpp's midboss path awards 1280 points per unit,
// unlike the 1000-point boss helper. Extra's separate midboss owner shares
// this concrete accounting and its source RNG point-number placement.
[[nodiscard]] bool midboss_score_bonus(State &, Presentation &, std::uint16_t units) noexcept;

// Stage setup invokes this after its non-combat initialization, preserving the
// source values for Stage 1's midboss and Sara combat objects.
void stage1_fights_setup(State &) noexcept;
void stage1_midboss_activate(State &) noexcept;
void stage1_boss_activate(State &) noexcept;

// Executes the active Stage 1 midboss/Sara callbacks. A false result denotes
// a reached unsupported pool/RNG state; dialog and clear hand-off are explicit
// State flags, rather than presentation callbacks.
[[nodiscard]] bool stage1_fights_update(State &, Presentation &) noexcept;

// Foreground boss/midboss work updates the shared explosion records after
// gameplay. The caller keeps this at the source render-tail position.
void stage1_fights_render_tail(State &, Presentation &) noexcept;

// Stage 2 setup invokes this after its non-combat initialization. The update
// covers Midboss 2 and Louise through State's explicit clear/dialog hand-off.
void stage2_fights_setup(State &) noexcept;
void stage2_midboss_activate(State &) noexcept;
void stage2_boss_activate(State &) noexcept;
[[nodiscard]] bool stage2_fights_update(State &, Presentation &) noexcept;
void stage2_fights_render_tail(State &, Presentation &) noexcept;

// Stage 3 carries Midboss 3 and Alice, including Alice's two custom-entity
// puppets. The State owner resets only AliceState::puppets with the source
// custom-entity pool during stage initialization.
void stage3_fights_setup(State &) noexcept;
void stage3_midboss_activate(State &) noexcept;
void stage3_boss_activate(State &) noexcept;
[[nodiscard]] bool stage3_fights_update(State &, Presentation &) noexcept;
void stage3_fights_render_tail(State &, Presentation &) noexcept;

// Stage 4 carries Midboss 4 and Mai/Yuki's paired and solo fights. The State
// owner clears only Stage4State::custom at stage initialization.
void stage4_fights_setup(State &) noexcept;
void stage4_midboss_activate(State &) noexcept;
void stage4_boss_activate(State &) noexcept;
[[nodiscard]] bool stage4_fights_update(State &, Presentation &) noexcept;
void stage4_fights_render_tail(State &, Presentation &) noexcept;

void stage5_fights_setup(State &) noexcept;
void stage5_midboss_activate(State &) noexcept;
void stage5_boss_activate(State &) noexcept;
[[nodiscard]] bool stage5_fights_update(State &, Presentation &) noexcept;
void stage5_fights_render_tail(State &, Presentation &) noexcept;

// Stage 6 has no midboss: Shinki owns the entire source fight.
void stage6_fights_setup(State &) noexcept;
void stage6_midboss_activate(State &) noexcept;
void stage6_boss_activate(State &) noexcept;
[[nodiscard]] bool stage6_fights_update(State &, Presentation &) noexcept;
void stage6_fights_render_tail(State &, Presentation &) noexcept;

// Extra combines its independently-owned midboss with EX-Alice.  The root
// stage dispatcher keeps their scripted activation/dialog hand-off separate.
void extra_fights_setup(State &) noexcept;
void extra_boss_activate(State &) noexcept;
[[nodiscard]] bool extra_fights_update(State &, Presentation &) noexcept;
void extra_fights_render_tail(State &, Presentation &) noexcept;

} // namespace th05::core
