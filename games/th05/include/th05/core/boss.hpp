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

} // namespace th05::core
