#pragma once

#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_motion.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace th05::core {

inline constexpr std::size_t bullet_pellet_count = 180;
inline constexpr std::size_t bullet16_count = 220;
inline constexpr std::size_t bullet_count =
	bullet_pellet_count + bullet16_count;

inline constexpr std::uint8_t bullet_free = 0;
inline constexpr std::uint8_t bullet_alive = 1;
inline constexpr std::uint8_t bullet_spawn_grazeable = 0;
inline constexpr std::uint8_t bullet_spawn_cloud_backwards = 3;
inline constexpr std::uint8_t bullet_spawn_cloud_forwards = 4;
inline constexpr std::uint8_t bullet_move_decelerate = 0;
inline constexpr std::uint8_t bullet_move_regular = 2;
inline constexpr std::uint8_t bullet_no_decelerate = 0x10;
inline constexpr std::uint8_t bullet_base_decelerate_speed = 72;
inline constexpr std::uint8_t bullet_decelerate_threshold = 64;
inline constexpr std::uint8_t bullet_decelerate_frames = 32;
inline constexpr std::int16_t bullet16_width = 16;
inline constexpr std::int16_t bullet16_height = 16;
inline constexpr std::int16_t bullet_killbox_width = 128;
inline constexpr std::int16_t bullet_killbox_height = 128;

// th04/main/bullet/bullet.inc:81-105. The TH05 template is a persistent
// 14-byte object; the native representation keeps every source byte explicit
// rather than allowing host enum or padding choices to alter the contract.
struct BulletTemplate {
	std::uint8_t spawn_type{};
	std::uint8_t patnum{};
	pc98::th0405::SubpixelPoint origin{};
	std::uint8_t group{};
	std::uint8_t special_motion{};
	std::uint8_t spread{};
	std::uint8_t spread_angle_delta{};
	std::uint8_t stack{};
	std::uint8_t stack_speed_delta{};
	std::uint8_t angle{};
	std::uint8_t speed{};
	bool operator==(const BulletTemplate &) const = default;
};

static_assert(sizeof(BulletTemplate) == 14);
static_assert(offsetof(BulletTemplate, spawn_type) == 0);
static_assert(offsetof(BulletTemplate, patnum) == 1);
static_assert(offsetof(BulletTemplate, origin) == 2);
static_assert(offsetof(BulletTemplate, group) == 6);
static_assert(offsetof(BulletTemplate, special_motion) == 7);
static_assert(offsetof(BulletTemplate, spread) == 8);
static_assert(offsetof(BulletTemplate, spread_angle_delta) == 9);
static_assert(offsetof(BulletTemplate, stack) == 10);
static_assert(offsetof(BulletTemplate, stack_speed_delta) == 11);
static_assert(offsetof(BulletTemplate, angle) == 12);
static_assert(offsetof(BulletTemplate, speed) == 13);

struct BulletU1 {
	std::uint8_t value{};
	constexpr bool operator==(const BulletU1 &other) const noexcept
	{
		return value == other.value;
	}
};

struct BulletU2 {
	std::uint8_t value{};
	constexpr bool operator==(const BulletU2 &other) const noexcept
	{
		return value == other.value;
	}
};

// th04/main/bullet/bullet.hpp:136-165. The map establishes a 32-byte TH05
// record. The untouched previous position, special fields, origin/distance,
// and padding are retained when a free slot is reused, as in bullet.asm.
struct Bullet {
	std::uint8_t flag{};
	std::uint8_t age{};
	pc98::th0405::Motion pos{};
	std::uint8_t from_group{};
	std::uint8_t unused{};
	std::uint8_t speed_cur{};
	std::uint8_t angle{};
	std::uint8_t spawn_flag{};
	std::uint8_t move_flag{};
	std::uint8_t special_motion{};
	std::uint8_t speed_final{};
	BulletU1 u1{};
	BulletU2 u2{};
	std::uint16_t patnum{};
	pc98::th0405::SubpixelPoint origin{};
	std::int16_t distance{};
	bool operator==(const Bullet &) const = default;
};

static_assert(sizeof(Bullet) == 32);
static_assert(offsetof(Bullet, flag) == 0);
static_assert(offsetof(Bullet, age) == 1);
static_assert(offsetof(Bullet, pos) == 2);
static_assert(offsetof(Bullet, from_group) == 14);
static_assert(offsetof(Bullet, unused) == 15);
static_assert(offsetof(Bullet, speed_cur) == 16);
static_assert(offsetof(Bullet, angle) == 17);
static_assert(offsetof(Bullet, spawn_flag) == 18);
static_assert(offsetof(Bullet, move_flag) == 19);
static_assert(offsetof(Bullet, special_motion) == 20);
static_assert(offsetof(Bullet, speed_final) == 21);
static_assert(offsetof(Bullet, u1) == 22);
static_assert(offsetof(Bullet, u2) == 23);
static_assert(offsetof(Bullet, patnum) == 24);
static_assert(offsetof(Bullet, origin) == 26);
static_assert(offsetof(Bullet, distance) == 30);

struct BulletPool {
	std::array<Bullet, bullet_pellet_count> pellets{};
	std::array<Bullet, bullet16_count> bullets16{};
};

static_assert(sizeof(BulletPool) == bullet_count * sizeof(Bullet));

// A span view permits snapshot loaders to be validated before any source-like
// reverse scan. The source always owns exactly these two fixed arrays.
struct BulletPoolView {
	std::span<Bullet> pellets{};
	std::span<Bullet> bullets16{};
};

[[nodiscard]] constexpr BulletPoolView bullet_pool_view(
	BulletPool &pool
) noexcept
{
	return {pool.pellets, pool.bullets16};
}

enum class BulletPoolKind : std::uint8_t {
	none = 0xFF,
	pellets = 0,
	bullets16 = 1,
};

enum class BulletAddStatus : std::uint8_t {
	allocated,
	clipped,
	no_slot,
	zapped,
	unsupported_spawn_type,
	unsupported_pattern,
	unsupported_group,
	invalid_host,
};

struct BulletAddResult {
	BulletAddStatus status{BulletAddStatus::invalid_host};
	BulletPoolKind pool{BulletPoolKind::none};
	std::uint16_t index{};
	bool operator==(const BulletAddResult &) const = default;
};

// The state read or written by th05/main/bullet/clip.cpp. The source globals
// are made explicit so a caller can observe player-hit side effects and pass
// the exact playperf/clear-time/fixed-speed inputs without a fake clip flag.
struct BulletClipState {
	std::uint8_t bullet_clear_time{};
	std::uint8_t playperf{16};
	pc98::th0405::SubpixelPoint player_position{};
	bool player_is_hit{};
	bool group_fixedspeed{};
	bool bullet_zap{};
};

namespace detail {

[[nodiscard]] constexpr std::uint16_t word_bits(
	std::int16_t value
) noexcept
{
	return std::bit_cast<std::uint16_t>(value);
}

[[nodiscard]] constexpr std::int16_t bullet_word_subtract(
	std::int16_t left,
	std::int16_t right
) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		word_bits(left) - word_bits(right)
	));
}

// th05/main/bullet/add_1.asm::playperf_speedtune. The source shifts the
// product first, adds the half-speed in AL (byte wrap), then clamps the byte
// to [8, 128].
[[nodiscard]] constexpr std::uint8_t bullet_speed_tune(
	std::uint8_t speed,
	std::uint8_t playperf
) noexcept
{
	const auto half_speed = static_cast<std::uint8_t>(speed >> 1U);
	const auto product = static_cast<std::uint16_t>(half_speed) * playperf;
	const auto scaled = static_cast<std::uint8_t>(product >> 5U);
	auto result = static_cast<std::uint8_t>(scaled + half_speed);
	if(result > 128U) result = 128U;
	if(result < 8U) result = 8U;
	return result;
}

[[nodiscard]] constexpr bool playfield_encloses(
	pc98::th0405::SubpixelPoint center
) noexcept
{
	// playfield_encloses(center, 16, 16), with relative Q12.4 coordinates:
	// strict (-8, 392) x (-8, 376) pixels.
	return center.x > -128 && center.x < 6272 &&
		center.y > -128 && center.y < 6016;
}

[[nodiscard]] constexpr bool overlap_1d_inplace(
	std::int16_t delta,
	std::int16_t extent
) noexcept
{
	// overlap_1d_inplace_fast: unsigned(delta + extent / 2) <= extent,
	// retaining the guest's 16-bit wrap at both edges.
	const auto shifted = static_cast<std::uint16_t>(
		word_bits(delta) + static_cast<std::uint16_t>(extent / 2)
	);
	return shifted <= static_cast<std::uint16_t>(extent);
}

[[nodiscard]] constexpr bool bullet_pool_valid(BulletPoolView pool) noexcept
{
	return pool.pellets.size() == bullet_pellet_count &&
		pool.bullets16.size() == bullet16_count;
}

[[nodiscard]] constexpr bool supported_spawn_type(
	std::uint8_t spawn_type
) noexcept
{
	// Support normal and cloud regular bullets, with the source's
	// BST_NO_DECELERATE bit. Gather and sentinel spawn types remain explicit
	// unsupported outcomes in this bounded regular allocator.
	if((spawn_type & 0xE0U) != 0U) return false;
	const auto base = static_cast<std::uint8_t>(spawn_type & 0x0FU);
	return base == 0U || base == 2U || base == 3U;
}

[[nodiscard]] constexpr std::uint8_t bullet_spawn_flag(
	std::uint8_t spawn_type
) noexcept
{
	switch(static_cast<std::uint8_t>(spawn_type & 0x0FU)) {
	case 3: return bullet_spawn_cloud_backwards;
	case 2: return bullet_spawn_cloud_forwards;
	default: return bullet_spawn_grazeable;
	}
}

} // namespace detail

[[nodiscard]] constexpr std::uint8_t bullet_speed_tune_for_playperf(
	std::uint8_t speed,
	std::uint8_t playperf
) noexcept
{
	return detail::bullet_speed_tune(speed, playperf);
}

// Source: th05/main/bullet/clip.cpp followed by th05/bullet.asm and
// th05/main/bullet/add_1.asm. This is deliberately the bounded regular
// pellet/BG_SINGLE entry, not a generic bullet queue: gather, directional
// patterns, and all other groups return explicit unsupported statuses.
[[nodiscard]] constexpr BulletAddResult bullets_add_regular_single(
	BulletPoolView pool,
	const BulletTemplate &bullet_template,
	BulletClipState &clip_state
) noexcept
{
	// Validate every borrowed span before any player-hit side effect or slot
	// access. This makes malformed host snapshots an atomic invalid_host result.
	if(!detail::bullet_pool_valid(pool)) {
		return {BulletAddStatus::invalid_host, BulletPoolKind::none, 0};
	}
	// bullets_add_regular tests bullet_zap before raw/gather/clip.
	if(clip_state.bullet_zap) {
		return {BulletAddStatus::zapped, BulletPoolKind::none, 0};
	}
	if(bullet_template.patnum != 0U) {
		return {BulletAddStatus::unsupported_pattern, BulletPoolKind::none, 0};
	}
	if(bullet_template.group != 0U) {
		return {BulletAddStatus::unsupported_group, BulletPoolKind::none, 0};
	}
	if(!detail::supported_spawn_type(bullet_template.spawn_type)) {
		return {
			BulletAddStatus::unsupported_spawn_type, BulletPoolKind::none, 0
		};
	}

	// TH05 differs from TH04: clear times 1..16 clip before any player-hit or
	// speed-tune side effect. A value of 17 or more enters the raw path.
	if(clip_state.bullet_clear_time != 0U &&
		clip_state.bullet_clear_time < 17U) {
		return {BulletAddStatus::clipped, BulletPoolKind::none, 0};
	}
	if(!detail::playfield_encloses(bullet_template.origin)) {
		return {BulletAddStatus::clipped, BulletPoolKind::none, 0};
	}
	const auto delta_x = detail::bullet_word_subtract(
		bullet_template.origin.x, clip_state.player_position.x
	);
	if(detail::overlap_1d_inplace(delta_x, bullet_killbox_width)) {
		const auto delta_y = detail::bullet_word_subtract(
			bullet_template.origin.y, clip_state.player_position.y
		);
		if(detail::overlap_1d_inplace(delta_y, bullet_killbox_height)) {
			// Source sets player_is_hit but deliberately continues allocation.
			clip_state.player_is_hit = true;
		}
	}

	// The regular wrapper saves/restores the entire two-byte BT_angle word.
	// Consequently this effective speed is local: a clip-path playperf tune is
	// visible in the new record but never leaks into the caller's template.
	const auto effective_speed = clip_state.group_fixedspeed
		? bullet_template.speed
		: detail::bullet_speed_tune(
			bullet_template.speed, clip_state.playperf
		);
	auto move_flag = bullet_move_regular;
	if((effective_speed < bullet_decelerate_threshold ||
		clip_state.bullet_clear_time != 0U) &&
		(bullet_template.spawn_type & bullet_no_decelerate) == 0U) {
		move_flag = bullet_move_decelerate;
	}

	// The source starts at the last pellet and scans toward the first. A full
	// pool is a valid source no-allocation, not an unsupported or malformed
	// request; clip side effects above remain observable.
	for(std::size_t reverse = pool.pellets.size(); reverse-- > 0U;) {
		auto &bullet = pool.pellets[reverse];
		if(bullet.flag != bullet_free) continue;

		// Keep the source write order. Untouched pos.prev, unused, special
		// fields, origin, and distance deliberately retain reused-slot bytes.
		bullet.flag = bullet_alive;
		bullet.spawn_flag = detail::bullet_spawn_flag(
			bullet_template.spawn_type
		);
		bullet.pos.cur = bullet_template.origin;
		bullet.move_flag = move_flag;
		bullet.u1.value = bullet_decelerate_frames;
		bullet.u2.value = static_cast<std::uint8_t>(
			bullet_base_decelerate_speed - effective_speed
		);
		bullet.age = 0;
		bullet.from_group = bullet_template.group;
		const auto velocity = pc98::masterlib::vector2(
			bullet_template.angle,
			static_cast<std::int16_t>(effective_speed)
		);
		bullet.patnum = bullet_template.patnum;
		bullet.pos.velocity = {velocity.x, velocity.y};
		bullet.angle = bullet_template.angle;
		bullet.speed_final = effective_speed;
		bullet.speed_cur = effective_speed;
		return {
			BulletAddStatus::allocated,
			BulletPoolKind::pellets,
			static_cast<std::uint16_t>(reverse)
		};
	}
	return {BulletAddStatus::no_slot, BulletPoolKind::pellets, 0};
}

} // namespace th05::core
