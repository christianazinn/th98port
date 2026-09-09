#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "th02/core/player.hpp"

namespace th02::core {

inline constexpr std::size_t laser_count = 12U;
inline constexpr std::uint8_t laser_free = 0U;
inline constexpr std::uint8_t laser_alive = 1U;
inline constexpr std::uint8_t laser_remove = 2U;
inline constexpr std::uint8_t laser_phase_wait = 1U;
inline constexpr std::uint8_t laser_charge_cels = 4U;
inline constexpr std::uint8_t laser_wait_frames_default = 16U;

// Packed source laser_t. Keep the flag raw: invalidate processes every
// nonfree byte, but update later selects only F_ALIVE records.
struct Laser {
	std::uint8_t flag{};
	std::uint8_t phase{};
	ScreenPoint origin{};
	std::int16_t wait_frames{};
	std::int16_t active_frames{};
	std::uint8_t charge_cel{};
	std::uint8_t patnum_base{};
	bool operator==(const Laser &) const = default;
};

static_assert(sizeof(Laser) == 12U);
static_assert(offsetof(Laser, flag) == 0U);
static_assert(offsetof(Laser, phase) == 1U);
static_assert(offsetof(Laser, origin) == 2U);
static_assert(offsetof(Laser, wait_frames) == 6U);
static_assert(offsetof(Laser, active_frames) == 8U);
static_assert(offsetof(Laser, charge_cel) == 10U);
static_assert(offsetof(Laser, patnum_base) == 11U);

// Source laser array plus its byte-sized spawn wait template. The future
// update/render body owns phase progression and player collision.
struct LaserPoolState {
	std::array<Laser, laser_count> lasers{};
	std::uint8_t wait_frames{};
	bool operator==(const LaserPoolState &) const = default;
};

struct LaserInvalidation {
	ScreenPoint topleft{};
	std::int16_t width{};
	std::int16_t height{};
	bool operator==(const LaserInvalidation &) const = default;
};

// The add call's SFX and invalidate's tile rectangle retain their respective
// source positions as synchronous boundaries; neither implies an audio or
// visual renderer implementation.
class LaserSpawnCallbacks {
public:
	virtual ~LaserSpawnCallbacks() = default;
	virtual void sound_effect(std::uint8_t sound_id) noexcept = 0;
};

class LaserInvalidationCallbacks {
public:
	virtual ~LaserInvalidationCallbacks() = default;
	virtual void invalidate_laser(LaserInvalidation invalidation) noexcept = 0;
};

enum class LaserAddResult : std::uint8_t {
	spawned,
	ignored,
};

// th02/main/laser.cpp::lasers_reset. Clears only raw flags and restores the
// byte wait template; all record payload is retained.
void lasers_reset(LaserPoolState &state) noexcept;

// th02/main/laser.cpp::lasers_add. Uses the first raw-free slot, clips only X
// as 32 <= x < 416, and narrows the source int pattern to one record byte.
[[nodiscard]] LaserAddResult lasers_add(LaserPoolState &state,
	ScreenPoint origin, std::int16_t active_frames, std::int16_t patnum_base,
	LaserSpawnCallbacks &callbacks) noexcept;

// th02/main/laser.cpp::lasers_invalidate. Every raw nonfree record emits one
// tile rectangle. Charge uses <= LASER_CHARGE_CELS here (not update's later
// < relation), and a raw F_REMOVE becomes free only after its callback and
// possible charge-byte advance.
void lasers_invalidate(LaserPoolState &state, std::uint32_t stage_frame,
	LaserInvalidationCallbacks &callbacks) noexcept;

} // namespace th02::core
