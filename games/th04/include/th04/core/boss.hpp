#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th04::core {

struct State;
struct Presentation;

using Point = pc98::th0405::SubpixelPoint;
using Motion = pc98::th0405::Motion;

inline constexpr std::size_t circle_count = 16U;
inline constexpr std::size_t kurumi_spawnray_count = 6U;
inline constexpr std::uint8_t circle_free = 0U;
inline constexpr std::uint8_t circle_alive = 1U;
inline constexpr std::uint8_t circle_remove = 2U;

// th04/main/circle.hpp. Centers and radii are source pixel units, unlike the
// gameplay positions passed to the allocation helpers.
struct Circle {
	std::uint8_t flag{}, age{};
	std::int16_t center_x{}, center_y{};
	std::int16_t radius_cur{}, radius_delta{};
	bool operator==(const Circle &) const = default;
};

struct CircleState {
	std::array<Circle, circle_count> pool{};
	bool operator==(const CircleState &) const = default;
};

// th04/main/boss/b2.cpp. These use the Stage 2 custom-entity pool in the
// source; only Kurumi owns that pool during the fight.
enum class KurumiSpawnrayFlag : std::uint8_t {
	free = 0U,
	grow = 1U,
	shrink = 2U,
};

struct KurumiSpawnray {
	KurumiSpawnrayFlag flag{KurumiSpawnrayFlag::free};
	Point target{}, origin{}, velocity{};
	bool operator==(const KurumiSpawnray &) const = default;
};

// th04/main/boss/b3_upd.cpp and b3_fg.cpp. Elly's single custom entity is
// simulated by her update body and rendered before the explosion tail.
enum class EllyBoomerangFlag : std::uint8_t {
	free = 0U,
	thrown = 1U,
	caught = 2U,
};

struct EllyBoomerang {
	EllyBoomerangFlag flag{EllyBoomerangFlag::free};
	Motion pos{};
	std::uint8_t state{}, angle{}, angle_delta{}, speed{};
	std::uint16_t state_frame{};
	bool operator==(const EllyBoomerang &) const = default;
};

// th04/main/boss/b4r.hpp. Reimu owns all 32 source custom slots during the
// Stage 4 fight. The source changes the raw flag from spin to movement by an
// increment, so keep those byte values explicit.
inline constexpr std::size_t reimu_orb_count = 32U;
enum class ReimuOrbFlag : std::uint8_t {
	free = 0U,
	moveout_spin = 1U,
	move = 2U,
};

struct ReimuOrb {
	ReimuOrbFlag flag{ReimuOrbFlag::free};
	std::uint8_t angle{};
	Point center{}, origin{}, velocity{};
	std::uint16_t spin_time{};
	std::int16_t distance{}, unknown{};
	std::uint8_t move_speed{}, angle_speed{};
	bool operator==(const ReimuOrb &) const = default;
};

// th04/main/boss/b4m.hpp. Marisa reinterprets the same source custom block
// as four independently damageable orbiting bits.
inline constexpr std::size_t marisa_bit_count = 4U;
enum class MarisaBitFlag : std::uint8_t {
	free = 0U,
	moveout_spin = 1U,
	spin = 2U,
	kill_anim = 0x80U,
};

enum class MarisaBitFire : std::uint8_t {
	none = 0U,
	sideways_spread = 1U,
	single = 2U,
};

struct MarisaBit {
	MarisaBitFlag flag{MarisaBitFlag::free};
	std::uint8_t angle{};
	Point center{};
	std::uint8_t patnum{};
	std::int16_t distance{}, moveout_speed{}, hp{};
	std::uint8_t damage_this_frame{}, angle_speed{};
	bool operator==(const MarisaBit &) const = default;
};

// th04/main/boss/explode.hpp. The source keeps two small explosion slots and
// one big slot; their aging is deliberately a render-tail operation.
struct BossExplosion {
	bool alive{};
	std::uint8_t age{};
	Point center{}, radius_cur{}, radius_delta{};
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

// th04/main/midboss/midboss.hpp, with both fights' otherwise-private bytes.
struct MidbossState {
	Motion pos{};
	std::uint16_t frames_until{};
	std::int16_t hp{};
	std::uint8_t sprite{}, phase{};
	std::int16_t phase_frame{};
	std::uint8_t damage_this_frame{}, angle{};
	std::uint8_t stage1_fan_angle{};
	std::uint8_t stage2_pattern{}, stage2_direction{}, stage2_cycles{};
	std::uint8_t stage3_pattern{}, stage3_mirrored{}, stage3_patterns_done{};
	std::uint8_t stage4_pattern{}, stage4_passes{}, stage4_angle_flip{1U};
	bool operator==(const MidbossState &) const = default;
};

// th04/main/boss/boss.hpp. The source globals boss_hitbox_radius and
// boss_phase_timed_out are grouped here for the native State owner.
struct BossState {
	Motion pos{};
	std::int16_t hp{};
	std::uint8_t sprite{}, phase{};
	std::int16_t phase_frame{};
	std::uint8_t damage_this_frame{}, mode{}, angle{};
	std::array<std::uint8_t, 16U> statebytes{};
	// patterns_seen during regular phases; defeat_bonus in Orange phase 4/5.
	std::uint8_t phase_state{};
	std::int16_t phase_end_hp{};
	Point hitbox_radius{};
	bool phase_timed_out{true};
	std::array<KurumiSpawnray, kurumi_spawnray_count> kurumi_spawnrays{};
	std::uint8_t stage2_turn_direction{}, stage2_unused{};
	std::uint8_t elly_pattern_set{};
	std::int16_t elly_scythe_tick{};
	EllyBoomerang elly_boomerang{};
	std::array<ReimuOrb, reimu_orb_count> reimu_orbs{};
	ReimuOrb reimu_orb_template{};
	std::uint8_t reimu_orb_patnum_base{}, reimu_pattern8_angle{};
	std::uint8_t reimu_sweep_angle_delta{};
	bool reimu_bg_pulse_dimming{}, reimu_afterimage{};
	std::array<MarisaBit, marisa_bit_count> marisa_bits{};
	std::uint8_t marisa_bits_alive{}, marisa_bit_angle_speed{};
	MarisaBitFire marisa_bit_fire{MarisaBitFire::none};
	std::uint8_t marisa_pattern_prev{}, marisa_bits_at_pattern_start{};
	bool marisa_pulse_dimming{};
	std::uint8_t marisa_patterns_without_bits{}, marisa_explode_milestone{};
	std::uint8_t marisa_flystep_pointreflected_tick{};
	// th04/main/boss/b4m.cpp. Yuuka's Stage 5 controls are separate source
	// BSS, retained across stage setup just as the original does.
	std::uint8_t yuuka5_warp_phase{};
	std::int16_t yuuka5_spread_origin_x{};
	std::uint8_t yuuka5_ring_rate{}, yuuka5_ring_accumulator{};
	std::uint8_t yuuka5_palette_tone{};
	bool operator==(const BossState &) const = default;
};

// Source setup seeds state and callbacks separately. The driver calls the
// activation checks at their actual source positions: midboss before STD,
// boss only after the dialog/scroll condition succeeds.
void stage1_fights_setup(State &) noexcept;
void stage1_midboss_activate(State &) noexcept;
void stage1_boss_activate(State &) noexcept;

// Call in the source midboss/boss gameplay slot, after enemies_update(). A
// false result is a reached unsupported source boundary; no callback stands in
// for an allocation, RNG draw, or collision path.
[[nodiscard]] bool stage1_fights_update(State &, Presentation &) noexcept;

// th04/main/stage/loop.cpp updates circles before sparks/player gameplay.
void circles_update(State &, Presentation &) noexcept;

// The explosion age/animation updates belong to boss_fg_render()/midboss_render
// after bomb processing, rather than to the earlier gameplay update.
void stage1_fights_render_tail(State &, Presentation &) noexcept;

void stage2_fights_setup(State &) noexcept;
void stage2_midboss_activate(State &) noexcept;
void stage2_boss_activate(State &) noexcept;
[[nodiscard]] bool stage2_fights_update(State &, Presentation &) noexcept;
void stage2_fights_render_tail(State &, Presentation &) noexcept;

void stage3_fights_setup(State &) noexcept;
void stage3_midboss_activate(State &) noexcept;
void stage3_boss_activate(State &) noexcept;
[[nodiscard]] bool stage3_fights_update(State &, Presentation &) noexcept;
void stage3_fights_render_tail(State &, Presentation &) noexcept;

void stage4_fights_setup(State &) noexcept;
void stage4_boss_activate(State &) noexcept;
[[nodiscard]] bool stage4_fights_update(State &, Presentation &) noexcept;
void stage4_fights_render_tail(State &, Presentation &) noexcept;

void stage5_fights_setup(State &) noexcept;
void stage5_boss_activate(State &) noexcept;
[[nodiscard]] bool stage5_fights_update(State &, Presentation &) noexcept;
void stage5_fights_render_tail(State &, Presentation &) noexcept;

// The later midboss fights share the source shot collision and score-bonus
// behavior, while keeping their own phase machines and source-local bytes.
[[nodiscard]] bool midboss_hittest_shots_damage(
	State &, Presentation &, Point radius, std::uint8_t hit_sound,
	std::uint16_t &damage
) noexcept;
[[nodiscard]] bool midboss_score_bonus(
	State &, Presentation &, std::uint16_t units
) noexcept;

} // namespace th04::core
