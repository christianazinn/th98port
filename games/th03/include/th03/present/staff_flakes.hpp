#ifndef TH03_PRESENT_STAFF_FLAKES_HPP
#define TH03_PRESENT_STAFF_FLAKES_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "th03/present/mono.hpp"
#include "th03/present/raster.hpp"

namespace th03::present {

inline constexpr std::size_t staff_flake_count = 80U;
inline constexpr std::uint8_t staff_flake_count_after_overrun = 50U;
inline constexpr std::int16_t staff_flake_left_max_q4 =
	static_cast<std::int16_t>((graphics_width - 8U) * 16U);
inline constexpr std::int16_t staff_flake_top_max_q4 =
	static_cast<std::int16_t>((graphics_height - 8U) * 16U);

struct StaffFlake {
	bool alive = false;
	std::int16_t left_q4 = 0;
	std::int16_t top_q4 = 0;
	std::int16_t velocity_x_q4 = 0;
	std::int16_t velocity_y_q4 = 0;
	std::uint8_t cel = 0U;
};

// Caller-owned views of master.lib's signed 256-entry Q8 trigonometry tables.
// The original table bytes remain outside the presentation library.
struct StaffFlakeTrigTables {
	std::span<const std::int16_t> sine_q8{};
	std::span<const std::int16_t> cosine_q8{};
};

// MAINL owns this state in the source. Construct it with the resident seed
// captured at staff-roll entry; no gameplay-core state or host clock is read.
struct StaffFlakeState {
	explicit StaffFlakeState(std::uint32_t resident_seed) noexcept;

	std::array<StaffFlake, staff_flake_count> flakes{};
	std::uint32_t random_seed = 0U;
	std::int16_t frame = 0;
	std::uint8_t active_count =
		static_cast<std::uint8_t>(staff_flake_count);
	Page back_page = Page::zero;
	bool reset_pending = true;
	bool cdg_put_alpha = true;
};

// Mirrors flakes_reset(): only the 80 alive flags are cleared. The source
// leaves every other byte in the records untouched.
void staff_flakes_reset_records(StaffFlakeState &state) noexcept;

// Source phase drivers assign 0 or increment the signed 16-bit frame outside
// staffroll_flakes_tick(). These helpers preserve that separation and wrap.
void staff_flakes_restart_phase(StaffFlakeState &state) noexcept;
void staff_flakes_advance_frame(StaffFlakeState &state) noexcept;

// Clears the complete B plane on the current access page, preserving R/G/E.
// It remains separate because source CDG operations are interleaved between
// this clear and staff_flakes_tick().
void staffroll_blue_plane_clear(Raster &raster) noexcept;

// Exact source order: spawn, update, render, apply the one-time overrun reset,
// show [back_page], toggle it, then select the new access page. The boolean is
// the already-sampled result of the source's (vsync_Count1 > 1) test; this
// function never reads a clock, waits, or resets a platform counter. The
// current raster access page must equal [back_page], as in every source caller.
[[nodiscard]] bool staff_flakes_tick(
	Raster &raster,
	StaffFlakeState &state,
	const StaffFlakeMasks &masks,
	const StaffFlakeTrigTables &trig,
	bool vsync_count_above_one
) noexcept;

} // namespace th03::present

#endif
