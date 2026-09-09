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

// Source globals read by bullets_add_pellet(). Rank-base stack setup belongs
// to the omitted stage initializer; this body reads only these three fields.
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

// th02/main/bullet/bullet.cpp::bullets_add_pellet. This concrete regular
// enemy allocation body scans source-free records and expands every raw group
// byte. It borrows the already-owned frame position/page, current playperf,
// rank, bombing state and TH02 random ring; no update/render/collision/reset
// or bomb owner is implied. A host-invalid page refuses before mutation, while
// a modeled master.lib iatan2 fault preserves the source prefix already stored.
[[nodiscard]] EnemyBulletAddResult bullets_add_pellet(
	EnemyBulletPoolState &state, RandomState &random,
	const PlayerFrameState &player_frame, std::int16_t playperf,
	std::int8_t rank, bool bombing, ScreenPoint topleft, std::uint8_t angle,
	std::uint8_t group, std::int16_t speed
) noexcept;

} // namespace th02::core
