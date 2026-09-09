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

inline constexpr std::size_t pellet_count = 240U;
inline constexpr std::size_t bullet16_count = 200U;
inline constexpr std::size_t bullet_count = pellet_count + bullet16_count;
inline constexpr std::size_t gather_count = 16U;

// th04/main/bullet/types.h. Keep the raw source byte values: standard scripts
// write these directly into the global template.
enum class BulletGroup : std::uint8_t {
	single = 0x00U,
	single_aimed = 0x01U,
	random_angle = 0x1BU,
	random_angle_and_speed = 0x1CU,
	random_constrained_angle_aimed = 0x1DU,
	ring = 0x26U,
	ring_aimed = 0x2CU,
	spread = 0x2DU,
	spread_aimed = 0x2EU,
	stack = 0x2FU,
	stack_aimed = 0x30U,
	forcesingle = 0x40U,
	forcesingle_random_angle = 0x1AU,
	forcesingle_aimed = 0x41U,
};

enum class BulletSpawnType : std::uint8_t {
	gather_only = 0U,
	pellet = 1U,
	bullet16 = 2U,
	gather_pellet = 3U,
	bullet16_cloud_forwards = 4U,
	bullet16_cloud_backwards = 5U,
};

enum class BulletSpawnFlag : std::uint8_t {
	grazeable = 0U,
	grazed = 1U,
	active = 2U,
	cloud_backwards = 3U,
	cloud_forwards = 4U,
	cloud_end = 20U,
};

enum class BulletMoveFlag : std::uint8_t {
	decelerate = 0U,
	special = 1U,
	regular = 2U,
	decay = 4U,
	decay_end = 20U,
};

// ((GAME - 5) * 0x81) in th04/main/bullet/bullet.inc wraps to 0x7F.
enum class BulletSpecialMotion : std::uint8_t {
	decelerate_then_turn_aimed = 0x80U,
	decelerate_then_turn = 0x81U,
	speedup = 0x82U,
	decelerate_to_angle = 0x83U,
	bounce_left_right = 0x84U,
	bounce_top_bottom = 0x85U,
	bounce_left_right_top_bottom = 0x86U,
	bounce_left_right_top = 0x87U,
	gravity = 0x88U,
	none = 0xFFU,
};

inline constexpr std::uint8_t bullet_free = 0U;
inline constexpr std::uint8_t bullet_alive = 1U;
inline constexpr std::uint8_t bullet_remove = 2U;

// Q4.4 speeds are source bytes, rather than signed host lengths.
struct Bullet {
	std::uint8_t flag{};
	std::uint8_t age{};
	Motion pos{};
	BulletGroup from_group{};
	std::int8_t unused{};
	std::uint8_t speed_cur{};
	std::uint8_t angle{};
	BulletSpawnFlag spawn_flag{};
	BulletMoveFlag move_flag{};
	BulletSpecialMotion special_motion{};
	std::uint8_t speed_final{};
	// decelerate_time / turns_done, then decelerate_speed_delta / special angle
	std::uint8_t state_count{};
	std::uint8_t state_value{};
	std::int16_t patnum{};
	bool operator==(const Bullet &) const = default;
};
static_assert(sizeof(Bullet) == 26U);

// th04/main/bullet/bullet.inc. `velocity` is an output of the group helper;
// source scripts normally set only the fields after it.
struct BulletTemplate {
	std::uint8_t spawn_type{};
	std::uint8_t patnum{};
	Point origin{};
	Point velocity{};
	BulletGroup group{};
	std::uint8_t angle{};
	std::uint8_t speed{};
	std::uint8_t count{};
	// spread angle for spreads, Q4.4 speed delta for stacks.
	std::uint8_t delta{};
	std::uint8_t unused_1{};
	BulletSpecialMotion special_motion{};
	std::uint8_t unused_2{};
	bool operator==(const BulletTemplate &) const = default;
};
static_assert(sizeof(BulletTemplate) == 18U);

// A one-byte source union: active writes 0/1 and frame overwrites that same
// byte during the zap animation.
struct BulletZap {
	std::uint8_t value{};
	[[nodiscard]] constexpr bool active() const noexcept { return value != 0U; }
	[[nodiscard]] constexpr std::uint8_t frame() const noexcept { return value; }
	constexpr void set_active(bool active) noexcept { value = active ? 1U : 0U; }
	constexpr void advance_frame() noexcept { ++value; }
};
static_assert(sizeof(BulletZap) == 1U);

// `value` is turns_max for turn/bounce motions and a Q4.4 speed delta for the
// speedup/gravity motions; `template_special_angle` is the distinct source byte.
struct BulletSpecial {
	std::uint8_t value{};
	bool operator==(const BulletSpecial &) const = default;
};

struct Gather {
	std::uint8_t flag{};
	std::uint8_t col{};
	Motion center{};
	std::int16_t radius_cur{};
	std::int16_t ring_points{};
	std::uint8_t angle_cur{};
	std::uint8_t angle_delta{};
	BulletTemplate bullet_template{};
	std::int16_t radius_prev{};
	std::int16_t radius_delta{};
	bool operator==(const Gather &) const = default;
};
static_assert(sizeof(Gather) == 42U);

struct GatherTemplate {
	Point center{};
	Point velocity{};
	std::int16_t radius{};
	std::int16_t ring_points{};
	std::uint8_t col{};
	std::uint8_t angle_delta{};
	bool operator==(const GatherTemplate &) const = default;
};
static_assert(sizeof(GatherTemplate) == 14U);

// Owns all source bullet/gather BSS except bullet_clear_time, which is the
// shared State.resources member used by player, bomb, item, and bullets.
struct BulletState {
	std::array<Bullet, bullet_count> pool{};
	BulletTemplate template_{};
	std::uint8_t template_special_angle{};
	BulletSpecial special{};
	BulletZap zap{};
	// th04/main/bullet/add.cpp's globally stored but per-spawn temporaries.
	bool group_fixedspeed{};
	std::uint8_t group_i_spread_angle{}, group_i_absolute_angle{};
	std::array<Gather, gather_count> gathers{};
	GatherTemplate gather_template{};
	std::uint16_t stage_graze{};
	std::uint16_t graze_score{};
	std::uint16_t slowdown_factor{1U};
	bool turbo_mode{};
	std::uint32_t overlay_popup_bonus{};
};

// th04/main/bullet/add.cpp. False preserves an encountered source fault or a
// raw value not represented by this native pool; it never means spawn success.
[[nodiscard]] bool bullet_template_tune(State &state) noexcept;
[[nodiscard]] bool bullets_add(
	State &state, bool special = false, bool fixed_speed = false
) noexcept;

// th04/main/gather.cpp. The source only writes the gather-only spawn byte in
// a reused slot before initializing the visible circle state. A full pool is
// a source-valid no-op.
void gather_add_only(State &state) noexcept;

// th04/main/gather.cpp. Source frame values other than 0, 2, and 4 are
// no-ops. At 0 and 2 it first persists the supplied template color.
void gather_add_only_3stack(
	State &state,
	std::int16_t frame,
	std::uint8_t col_for_0,
	std::uint8_t col_for_2_and_4
) noexcept;

// th04/main/gather.cpp's simulation half. It fires a stored template through
// the ordinary regular spawn path when a circle reaches its final radius.
[[nodiscard]] bool gathers_update(State &state) noexcept;

// th04/main/bullet/update.cpp. Presentation only receives source rendering
// notifications; all hit, graze, score, clear, and zap state is concrete.
[[nodiscard]] bool bullets_update(State &state, Presentation &presentation) noexcept;

} // namespace th04::core
