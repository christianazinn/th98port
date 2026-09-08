#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

struct RandomState;

inline constexpr std::size_t player_shot_count = 38;
inline constexpr std::uint8_t player_shot_free = 0;
inline constexpr std::uint8_t player_shot_alive = 1;
inline constexpr std::uint8_t player_shot_remove = 2;

struct ShotPixelPoint {
	std::int16_t x{};
	std::int16_t y{};
	bool operator==(const ShotPixelPoint &) const = default;
};

// Q12.4 coordinates and velocities, matching TH02's 16-bit subpixel_t.
struct ShotSubpixelPoint {
	std::int16_t x{};
	std::int16_t y{};
	bool operator==(const ShotSubpixelPoint &) const = default;
};

// The source's packed shot_t record. The host representation is asserted so
// the source-visible pool field boundaries cannot silently drift.
struct PlayerShot {
	std::uint8_t flag{};
	std::uint8_t decay_cel{};
	std::array<ShotSubpixelPoint, 2> pos_on_page{};
	ShotSubpixelPoint velocity{};
	std::uint8_t patnum{};
	bool from_option{};
	bool operator==(const PlayerShot &) const = default;
};

static_assert(sizeof(PlayerShot) == 16U);
static_assert(offsetof(PlayerShot, flag) == 0U);
static_assert(offsetof(PlayerShot, decay_cel) == 1U);
static_assert(offsetof(PlayerShot, pos_on_page) == 2U);
static_assert(offsetof(PlayerShot, velocity) == 10U);
static_assert(offsetof(PlayerShot, patnum) == 14U);
static_assert(offsetof(PlayerShot, from_option) == 15U);

// The fields surrounding the source pool that its writer helpers read. The
// future shottype spawners own current_slot and spawn_top setup; they are not
// implemented by these writers.
struct PlayerShotPoolState {
	std::array<PlayerShot, player_shot_count> shots{};
	std::int16_t current_slot{};
	std::int16_t spawn_top{};
	std::uint8_t patnum{};
	std::uint8_t option_patnum{};
	bool operator==(const PlayerShotPoolState &) const = default;
};

struct PlayerShotWriterPositions {
	ShotPixelPoint player_topleft{};
	ShotPixelPoint option_left_topleft_on_back_page{};
	bool operator==(const PlayerShotWriterPositions &) const = default;
};

// A host-invalid back-page is distinguishable from master.lib's explicit
// source DIV fault. The latter can occur only after the source body has
// already written its preceding regular-shot prefix.
enum class ShotBVolleyResult : std::uint8_t {
	completed,
	invalid_page,
	source_iatan2_fault,
};

// th02/main/player/shot.cpp::shot_add. The caller has already selected a free
// slot and supplied the current back page. Invalid native slot/page values
// refuse without mutation; valid calls preserve the source store ordering.
[[nodiscard]] bool shot_add(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	std::int16_t left_offset, std::uint8_t angle) noexcept;

// th02/main/player/shot.cpp::shot_option_add. It writes the same selected slot
// but starts its distinct option-shot decay counter at one.
[[nodiscard]] bool shot_option_add(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	std::int16_t left_offset, ShotSubpixelPoint velocity) noexcept;

// th02/main/player/shot.cpp::shots_free_all. Only flags are cleared; every
// other byte in each slot remains as the future allocation scan would see it.
void shots_free_all(PlayerShotPoolState &state) noexcept;

// th02/main/player/shot.cpp::shot_a. This is the synchronous selected-shot
// body reached from the player stream tail. It writes the supplied regular
// patnum back after a full-power volley changes it, and always clears the
// separate option-shot latch on a valid native page.
[[nodiscard]] bool shot_a_volley(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	RandomState &random, std::uint8_t shot_level,
	std::int8_t spread_angle_delta, std::uint8_t &shot_patnum,
	std::uint8_t &option_shots_alive) noexcept;

// th02/main/player/shot.cpp::shot_b. This selected-shot body shares one pool
// scan between regular and option writes. At its two source homing call sites,
// it uses the exact shared master.lib iatan2 model; a modeled source DIV fault
// returns after preserving the state prefix already reached.
[[nodiscard]] ShotBVolleyResult shot_b_volley(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	RandomState &random, std::uint8_t shot_level, ShotPixelPoint boss_position,
	std::int16_t &boss_pos_x_unused, std::uint8_t &shot_patnum,
	std::uint8_t &shot_option_patnum, std::uint8_t &option_shots_alive) noexcept;

} // namespace th02::core
