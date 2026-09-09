#pragma once

#include "pc98/th0405_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace th05::core {

struct State;
struct Presentation;

struct BombPixelPoint {
	std::int16_t x{}, y{};
	bool operator==(const BombPixelPoint &) const = default;
};

// th05/main/player/bombanim[bss].asm: all four union views use exactly this
// ten-byte record extent. The native storage stays raw because a later
// playchar reinterprets the previous animation's bytes rather than resetting
// a distinct object.
struct BombReimuStar {
	pc98::th0405::SubpixelPoint topleft{};
	pc98::th0405::SubpixelPoint velocity{};
	std::uint8_t angle{};
	std::int8_t padding{};
	bool operator==(const BombReimuStar &) const = default;
};

struct BombMarisaLaser {
	pc98::th0405::SubpixelPoint center{};
	std::int16_t unused{};
	std::int16_t radius{};
	std::int16_t padding{};
	bool operator==(const BombMarisaLaser &) const = default;
};

struct BombMimaCircle {
	BombPixelPoint center{};
	std::int16_t unused{};
	std::int16_t distance{};
	std::uint8_t angle{};
	std::int8_t padding{};
	bool operator==(const BombMimaCircle &) const = default;
};

struct BombYuukaHeart {
	BombPixelPoint topleft{};
	std::int16_t unused{};
	std::int16_t distance{};
	std::uint8_t angle{};
	std::int8_t padding{};
	bool operator==(const BombYuukaHeart &) const = default;
};

static_assert(sizeof(BombPixelPoint) == 4U);
static_assert(sizeof(BombReimuStar) == 10U);
static_assert(sizeof(BombMarisaLaser) == 10U);
static_assert(sizeof(BombMimaCircle) == 10U);
static_assert(sizeof(BombYuukaHeart) == 10U);
static_assert(std::is_trivially_copyable_v<BombReimuStar>);
static_assert(std::is_trivially_copyable_v<BombMarisaLaser>);
static_assert(std::is_trivially_copyable_v<BombMimaCircle>);
static_assert(std::is_trivially_copyable_v<BombYuukaHeart>);

inline constexpr std::size_t bomb_animation_record_bytes = 10U;
inline constexpr std::size_t bomb_reimu_star_trails = 6U;
inline constexpr std::size_t bomb_reimu_star_nodes = 8U;
inline constexpr std::size_t bomb_marisa_lasers = 8U;
inline constexpr std::size_t bomb_mima_circles = 8U;
inline constexpr std::size_t bomb_animation_raw_bytes =
	bomb_reimu_star_trails * bomb_reimu_star_nodes * bomb_animation_record_bytes;

struct BombAnimationStorage {
	std::array<std::uint8_t, bomb_animation_raw_bytes> raw{};
	// The BSS declares one record immediately after the union. It is a real
	// adjacent record, not an alias for Reimu's later, farther render-only read.
	BombReimuStar reimu_guard{};
	bool operator==(const BombAnimationStorage &) const = default;
};

static_assert(sizeof(BombAnimationStorage) ==
	(bomb_animation_raw_bytes + sizeof(BombReimuStar)));
static_assert(offsetof(BombAnimationStorage, reimu_guard) == bomb_animation_raw_bytes);

struct BombState {
	BombAnimationStorage animation{};
	bool operator==(const BombState &) const = default;
};

template <typename Record>
[[nodiscard]] inline Record bomb_animation_record_load(
	const BombAnimationStorage &storage, std::size_t index
) noexcept
{
	static_assert(std::is_trivially_copyable_v<Record>);
	Record record{};
	std::memcpy(&record, storage.raw.data() + (index * sizeof(Record)), sizeof(Record));
	return record;
}

template <typename Record>
inline void bomb_animation_record_store(
	BombAnimationStorage &storage, std::size_t index, const Record &record
) noexcept
{
	static_assert(std::is_trivially_copyable_v<Record>);
	std::memcpy(storage.raw.data() + (index * sizeof(Record)), &record, sizeof(Record));
}

[[nodiscard]] inline BombReimuStar bomb_reimu_star_load(
	const BombAnimationStorage &storage, std::size_t index
) noexcept
{
	if(index == (bomb_reimu_star_trails * bomb_reimu_star_nodes)) {
		return storage.reimu_guard;
	}
	// Callers must keep active-union accesses below 48. In particular, the
	// native renderer's final index 54 is intentionally not redirected here.
	return bomb_animation_record_load<BombReimuStar>(storage, index);
}

inline void bomb_reimu_star_store(
	BombAnimationStorage &storage, std::size_t index, const BombReimuStar &star
) noexcept
{
	bomb_animation_record_store(storage, index, star);
}

enum class BombGeometryKind : std::uint8_t {
	reimu_star,
	growing_circle,
	marisa_laser,
	mima_beam,
	mima_circle,
	mima_ring,
	yuuka_heart,
};

// Presentation only: source gameplay state is already written before an event
// reaches this payload, and an adapter cannot feed a result back into State.
struct BombGeometry {
	BombGeometryKind kind{};
	pc98::th0405::SubpixelPoint subpixel{};
	BombPixelPoint pixel{};
	std::int16_t extent{};
	std::uint8_t color{};
	bool operator==(const BombGeometry &) const = default;
};

void player_bomb(State &, Presentation &) noexcept;
[[nodiscard]] bool bomb_update(State &, Presentation &) noexcept;

} // namespace th05::core
