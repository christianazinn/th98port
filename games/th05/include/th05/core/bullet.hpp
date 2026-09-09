#pragma once

#include "th05/core/bullet_pool.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th05::core {

struct State;
struct Presentation;

// th05/main/bullet/types.inc. These compact source byte values are not the
// older TH04 group values; scripts write them directly into BulletTemplate.
enum class BulletGroup : std::uint8_t {
	single = 0U,
	single_aimed = 1U,
	spread = 2U,
	spread_aimed = 3U,
	ring = 4U,
	ring_aimed = 5U,
	stack = 6U,
	stack_aimed = 7U,
	spread_stack = 8U,
	spread_stack_aimed = 9U,
	ring_stack = 10U,
	ring_stack_aimed = 11U,
	random_angle = 12U,
	random_angle_and_speed = 13U,
	forcesingle = 14U,
	forcesingle_aimed = 15U,
};

// th04/main/bullet/bullet.inc with GAME == 5. This parcel executes the
// exact-linear case needed by Stage 1; other reached values are explicit
// source-boundary refusals rather than normalized motion.
enum class BulletSpecialMotion : std::uint8_t {
	exact_linear = 10U,
};

inline constexpr std::uint8_t bullet_spawn_type_normal = 0U;
inline constexpr std::uint8_t bullet_spawn_type_gather_pellet = 1U;
inline constexpr std::uint8_t bullet_spawn_type_cloud_forwards = 2U;
inline constexpr std::uint8_t bullet_spawn_type_cloud_backwards = 3U;
inline constexpr std::uint8_t bullet_spawn_type_no_decelerate = 0x10U;
inline constexpr std::uint8_t bullet_spawn_type_gather_normal_special = 0xFEU;

inline constexpr std::uint8_t bullet_move_special = 1U;
inline constexpr std::uint8_t bullet_move_decay = 4U;
inline constexpr std::uint8_t bullet_move_decay_end = 20U;
inline constexpr std::uint8_t bullet_remove = 2U;
inline constexpr std::uint8_t bullet_spawn_active = 2U;
inline constexpr std::uint8_t bullet_spawn_grazed = 1U;
inline constexpr std::uint8_t bullet_spawn_cloud_end = 20U;

// th04/main/bullet/clearzap.hpp. Source overlays active and frame in one raw
// byte, so an active zap always begins at frame 1.
struct BulletZap {
	std::uint8_t value{};
	[[nodiscard]] constexpr bool active() const noexcept { return value != 0U; }
	[[nodiscard]] constexpr std::uint8_t frame() const noexcept { return value; }
	constexpr void start() noexcept { value = 1U; }
	constexpr void advance() noexcept { ++value; }
	constexpr void stop() noexcept { value = 0U; }
	bool operator==(const BulletZap &) const = default;
};

inline constexpr std::size_t gather_count = 8U;

// th04/main/gather.inc under GAME == 5. The source BSS reserves sixteen
// records, but every TH05 gather loop uses GATHER_CAP == 8; this owner models
// exactly that reached active prefix rather than exposing unused raw storage.
struct Gather {
	std::uint8_t flag{};
	std::uint8_t color{};
	pc98::th0405::Motion center{};
	std::int16_t radius_cur{};
	std::int16_t ring_points{};
	std::uint8_t angle_cur{};
	std::uint8_t angle_delta{};
	std::int16_t radius_prev{};
	std::int16_t radius_delta{};
	BulletTemplate bullet_template{};
	bool operator==(const Gather &) const = default;
};
static_assert(sizeof(Gather) == 38U);

struct GatherTemplate {
	pc98::th0405::SubpixelPoint center{};
	pc98::th0405::SubpixelPoint velocity{};
	std::int16_t radius{};
	std::int16_t ring_points{};
	std::uint8_t color{};
	std::uint8_t angle_delta{};
	bool operator==(const GatherTemplate &) const = default;
};
static_assert(sizeof(GatherTemplate) == 14U);

// Source BSS local to TH05's bullet update/add path. bullet_clear_time,
// player hit state, score, random ring, playperf, rank, and slowdown remain
// canonical State owners shared with their existing native consumers.
struct BulletState {
	BulletPool pool{};
	BulletTemplate bullet_template{};
	std::uint8_t bullet_template_special_angle{};
	BulletZap zap{};
	bool zap_drop_point_items{};
	bool group_fixedspeed{};
	std::uint8_t group_i_spread_angle{};
	std::uint8_t group_i{};
	std::uint8_t group_i_absolute_angle{};
	std::uint8_t group_i_speed{};
	pc98::th0405::SubpixelPoint group_i_velocity{};
	std::uint16_t stage_graze{};
	std::uint16_t pellet_bottom_color{};
	std::array<std::uint16_t, bullet_pellet_count> pellet_cloud_indices{};
	std::uint16_t pellet_cloud_count{};
	std::array<Gather, gather_count> gathers{};
	GatherTemplate gather_template{};
	std::uint32_t popup_bonus{};
	bool operator==(const BulletState &) const = default;
};

// th05/main/bullet/tune.cpp + tune.asm. This mutates the persistent source
// template in place, exactly as boss patterns do before each add call.
[[nodiscard]] bool bullet_template_tune(State &) noexcept;

// th05/main/bullet/add_1.asm. These source-shaped wrappers save and restore
// the template's final angle/speed word around the raw spawn implementation.
// False reports an encountered source fault or an unsupported reached motion;
// a full appropriate pool and a zap-gated call remain source-successful.
[[nodiscard]] bool bullets_add_regular(State &) noexcept;
[[nodiscard]] bool bullets_add_special(State &) noexcept;
[[nodiscard]] bool bullets_add_regular_fixedspeed(State &) noexcept;
[[nodiscard]] bool bullets_add_special_fixedspeed(State &) noexcept;

// th04/main/gather.cpp with TH05's eight-record active capacity. The add-only
// APIs are used directly by the Stage 1 boss; gathers_update owns the source
// release into the real TH05 regular/special bullet add paths.
void gather_add_only(State &) noexcept;
void gather_add_only_3stack(
	State &, std::int16_t frame, std::uint8_t color_for_0,
	std::uint8_t color_for_2_and_4
) noexcept;
[[nodiscard]] bool gathers_update(State &) noexcept;

// th04/main/bullet/update.cpp compiled through th05/bullet_u.cpp. Hit,
// graze, spark, score, clear, zap, cloud-list, and slowdown effects are
// concrete; presentation only receives existing HUD/point/popup notices.
[[nodiscard]] bool bullets_update(State &, Presentation &) noexcept;

} // namespace th05::core
