#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "th02/core/player.hpp"

namespace th02::core {

struct RandomState;

inline constexpr std::size_t enemy_bullet_count = 150U;
inline constexpr std::int8_t enemy_bullet_free = 0;
inline constexpr std::int8_t enemy_bullet_alive = 1;
inline constexpr std::int8_t enemy_bullet_pellet = 1;
inline constexpr std::int8_t enemy_bullet_16x16 = 2;
inline constexpr std::int8_t enemy_bullet_remove = 2;

// Raw group bytes from th02/main/bullet/bullet.hpp. The public allocator takes
// a byte because the donor's default branch gives unsupported values concrete
// (pool-filling aimed) behavior.
namespace enemy_bullet_group {
inline constexpr std::uint8_t spread_2_narrow = 0x01U;
inline constexpr std::uint8_t spread_2_medium = 0x02U;
inline constexpr std::uint8_t spread_2_wide = 0x03U;
inline constexpr std::uint8_t spread_2_narrow_aimed = 0x04U;
inline constexpr std::uint8_t spread_2_medium_aimed = 0x05U;
inline constexpr std::uint8_t spread_2_wide_aimed = 0x06U;
inline constexpr std::uint8_t spread_3_narrow = 0x07U;
inline constexpr std::uint8_t spread_3_medium = 0x08U;
inline constexpr std::uint8_t spread_3_wide = 0x09U;
inline constexpr std::uint8_t spread_3_narrow_aimed = 0x0AU;
inline constexpr std::uint8_t spread_3_medium_aimed = 0x0BU;
inline constexpr std::uint8_t spread_3_wide_aimed = 0x0CU;
inline constexpr std::uint8_t spread_4_narrow = 0x0DU;
inline constexpr std::uint8_t spread_4_medium = 0x0EU;
inline constexpr std::uint8_t spread_4_wide = 0x0FU;
inline constexpr std::uint8_t spread_4_narrow_aimed = 0x10U;
inline constexpr std::uint8_t spread_4_medium_aimed = 0x11U;
inline constexpr std::uint8_t spread_4_wide_aimed = 0x12U;
inline constexpr std::uint8_t spread_5_narrow = 0x13U;
inline constexpr std::uint8_t spread_5_medium = 0x14U;
inline constexpr std::uint8_t spread_5_wide = 0x15U;
inline constexpr std::uint8_t spread_5_narrow_aimed = 0x16U;
inline constexpr std::uint8_t spread_5_medium_aimed = 0x17U;
inline constexpr std::uint8_t spread_5_wide_aimed = 0x18U;
inline constexpr std::uint8_t one_aimed = 0x19U;
inline constexpr std::uint8_t one_random_angle = 0x1AU;
inline constexpr std::uint8_t random_angle = 0x1BU;
inline constexpr std::uint8_t random_angle_and_speed = 0x1CU;
inline constexpr std::uint8_t one = 0x20U;
inline constexpr std::uint8_t ring_4 = 0x21U;
inline constexpr std::uint8_t ring_2 = 0x22U;
inline constexpr std::uint8_t spread_2_horizontally_symmetric = 0x23U;
inline constexpr std::uint8_t ring_8 = 0x24U;
inline constexpr std::uint8_t ring_16 = 0x25U;
inline constexpr std::uint8_t spread_2_ultrawide_aimed = 0x26U;
inline constexpr std::uint8_t ring_32 = 0x27U;
} // namespace enemy_bullet_group

// Raw values at and above this threshold take bullets_add_16x16()'s direct
// special-motion allocation path. Allocation needs gravity's tuning exception;
// the update body owns the per-frame interpretations.
namespace enemy_bullet_special_motion {
inline constexpr std::uint8_t first = 0x80U;
inline constexpr std::uint8_t chase = 0x80U;
inline constexpr std::uint8_t homing = 0x81U;
inline constexpr std::uint8_t decelerate_then_turn_aimed = 0x82U;
inline constexpr std::uint8_t gravity = 0x83U;
inline constexpr std::uint8_t drift_angle_and_speed = 0x84U;
inline constexpr std::uint8_t drift_angle_chase = 0x85U;
inline constexpr std::uint8_t bounce_left_right_top_bottom = 0x87U;
inline constexpr std::uint8_t bounce_top_bottom = 0x88U;
} // namespace enemy_bullet_special_motion

// The source's packed bullet_t record. Pellet allocation intentionally keeps
// the untouched patnum, speed, padding and opposite-page bytes observable.
struct EnemyBullet {
	std::int8_t flag{};
	std::int8_t size_type{};
	std::array<ShotSubpixelPoint, 2> screen_topleft_on_page{};
	ShotSubpixelPoint velocity{};
	std::uint8_t patnum{};
	std::uint8_t group_or_special_motion{};
	std::uint8_t angle{};
	std::uint8_t speed{};
	std::uint8_t u1{};
	std::int8_t padding{};
	bool operator==(const EnemyBullet &) const = default;
};

static_assert(sizeof(EnemyBullet) == 20U);
static_assert(offsetof(EnemyBullet, flag) == 0U);
static_assert(offsetof(EnemyBullet, size_type) == 1U);
static_assert(offsetof(EnemyBullet, screen_topleft_on_page) == 2U);
static_assert(offsetof(EnemyBullet, velocity) == 10U);
static_assert(offsetof(EnemyBullet, patnum) == 14U);
static_assert(offsetof(EnemyBullet, group_or_special_motion) == 15U);
static_assert(offsetof(EnemyBullet, angle) == 16U);
static_assert(offsetof(EnemyBullet, speed) == 17U);
static_assert(offsetof(EnemyBullet, u1) == 18U);
static_assert(offsetof(EnemyBullet, padding) == 19U);

// Source globals read by the concrete TH02 bullet allocators. Rank-base stack
// setup belongs to the omitted stage initializer; these bodies read only these
// three fields.
struct EnemyBulletPoolState {
	std::array<EnemyBullet, enemy_bullet_count> bullets{};
	std::int8_t rank_base_speed{};
	std::uint8_t stack{};
	std::int8_t easy_slow_skip_cycle{};
	bool operator==(const EnemyBulletPoolState &) const = default;
};

enum class EnemyBulletAddResult : std::uint8_t {
	completed,
	invalid_page,
	source_iatan2_fault,
};

// Three source-global signed words, each standing in for one of bullet_special
// union's same-width interpretations. The update reads these exact raw values;
// stage setup remains their separate owner.
struct EnemyBulletSpecialState {
	std::int16_t chase_gravity_or_drift_angle{};
	std::int16_t homing_frames_or_drift_speed{};
	std::int16_t turns_or_drift_frames{};
	bool operator==(const EnemyBulletSpecialState &) const = default;
};

enum class EnemyBulletPresentationKind : std::uint8_t {
	set_pellet_color_white,
	render_pellet,
	grcg_off,
	render_bullet_16x16,
};

// A source-position presentation request. Its topleft has already passed the
// synchronous scroll conversion boundary; no VRAM or grcg implementation is
// implied by this state-only update slice.
struct EnemyBulletPresentationEvent {
	EnemyBulletPresentationKind kind{};
	ScreenPoint topleft{};
	std::uint8_t patnum{};
	bool operator==(const EnemyBulletPresentationEvent &) const = default;
};

class EnemyBulletPresentationCallbacks {
public:
	virtual ~EnemyBulletPresentationCallbacks() = default;
	virtual void present(EnemyBulletPresentationEvent event) noexcept = 0;
};

enum class EnemyBulletUpdateResult : std::uint8_t {
	completed,
	invalid_page,
	source_iatan2_fault,
};

// th02/main/bullet/bullet.cpp::bullets_add_pellet. This concrete regular enemy
// allocation body scans source-free records and expands every raw group byte.
// It borrows the already-owned frame position/page, current playperf, rank,
// bombing state and TH02 random ring; no update/render/collision/reset or bomb
// owner is implied. A host-invalid page refuses before mutation, while a
// modeled master.lib iatan2 fault preserves the source prefix already stored.
[[nodiscard]] EnemyBulletAddResult bullets_add_pellet(
	EnemyBulletPoolState &state, RandomState &random,
	const PlayerFrameState &player_frame, std::int16_t playperf,
	std::int8_t rank, bool bombing, ScreenPoint topleft, std::uint8_t angle,
	std::uint8_t group, std::int16_t speed
) noexcept;

// th02/main/bullet/bullet.cpp::bullets_add_16x16. This concrete 16x16 enemy
// allocation body shares every ordinary raw group with pellets. Raw values at
// least enemy_bullet_special_motion::first allocate one direct polar-motion
// record instead; gravity alone bypasses playperf/rank speed tuning. patnum is
// source-word-sized at the call boundary and narrows only at the source's
// byte-sized record store. No special-motion update, rendering, collision,
// reset, or bomb owner is implied.
[[nodiscard]] EnemyBulletAddResult bullets_add_16x16(
	EnemyBulletPoolState &state, RandomState &random,
	const PlayerFrameState &player_frame, std::int16_t playperf,
	std::int8_t rank, bool bombing, ScreenPoint topleft, std::uint8_t angle,
	std::uint8_t group_or_special_motion, std::int16_t patnum,
	std::int16_t speed
) noexcept;

// th02/main/bullet/bullet.cpp::bullets_update_and_render and its private
// bullet_update_special helper. It moves selected-back-page alive records,
// applies special motion only to size type 2, then performs source clip/hit
// removal and ordered synchronous presentation requests. A host-invalid page
// refuses before mutation. A modeled iatan2 fault preserves all earlier
// position/special writes and stops before subsequent records and the final
// grcg_off request. This owns neither VRAM nor scrolling state, invalidation,
// clear, collision follow-up, or frame scheduling.
[[nodiscard]] EnemyBulletUpdateResult bullets_update_and_render(
	EnemyBulletPoolState &state, const PlayerFrameState &player_frame,
	PlayerHitState &hit, const EnemyBulletSpecialState &special,
	std::uint32_t stage_frame, std::int16_t scroll_line,
	EnemyBulletPresentationCallbacks &presentation
) noexcept;

} // namespace th02::core
