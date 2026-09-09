#pragma once

#include "th04/core/shot_reimu_a.hpp"

#include <cstdint>

namespace th04::core {

// A bounded native port cannot access the six source bytes after the five
// representable allocator guard records. Report that boundary instead of
// inventing a host-side continuation.
enum class ShotPatternStatus : std::uint8_t {
	complete,
	allocation_boundary,
	random_refused,
};

struct ShotPatternResult {
	ShotPatternStatus status{ShotPatternStatus::complete};
	ShotAddResult allocation{};
	std::uint8_t shots_allocated{};

	[[nodiscard]] constexpr bool complete() const noexcept
	{
		return status == ShotPatternStatus::complete;
	}
};

// th04/main/player/shot_reimu_b.cpp. The source's `shot_reimu_cycle` is also
// used by shottype A, so callers pass the existing ReimuAPatternState solely
// to preserve that shared byte; B does not inspect homing_target.
[[nodiscard]] ShotPatternResult shot_reimu_b_l2(
	ReimuAPatternState &, ShotSpawnState &, ShotState &, RandomState &
) noexcept;
[[nodiscard]] ShotPatternResult shot_reimu_b_l3(
	ReimuAPatternState &, ShotSpawnState &, ShotState &
) noexcept;
[[nodiscard]] ShotPatternResult shot_reimu_b_l4(
	ReimuAPatternState &, ShotSpawnState &, ShotState &
) noexcept;
[[nodiscard]] ShotPatternResult shot_reimu_b_l5(
	ReimuAPatternState &, ShotSpawnState &, ShotState &
) noexcept;
[[nodiscard]] ShotPatternResult shot_reimu_b_l6(
	ReimuAPatternState &, ShotSpawnState &, ShotState &
) noexcept;
[[nodiscard]] ShotPatternResult shot_reimu_b_l7(
	ReimuAPatternState &, ShotSpawnState &, ShotState &
) noexcept;
[[nodiscard]] ShotPatternResult shot_reimu_b_l8(
	ReimuAPatternState &, ShotSpawnState &, ShotState &
) noexcept;
[[nodiscard]] ShotPatternResult shot_reimu_b_l9(
	ReimuAPatternState &, ShotSpawnState &, ShotState &
) noexcept;

// th04/main/player/shot_marisa_a.cpp and p_marisa.cpp. `option_cur` is the
// previous-frame option position when called from player_update(), exactly as
// the donor reads player_option_pos_cur before that routine advances it.
[[nodiscard]] ShotPatternResult shot_marisa_a_l2(
	ShotSpawnState &, ShotState &, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_a_l3(
	ShotSpawnState &, ShotState &, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_a_l4(
	ShotSpawnState &, ShotState &, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_a_l5(
	ShotSpawnState &, ShotState &, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_a_l6(
	ShotSpawnState &, ShotState &, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_a_l7(
	ShotSpawnState &, ShotState &, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_a_l8(
	ShotSpawnState &, ShotState &, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_a_l9(
	ShotSpawnState &, ShotState &, RandomState &, pc98::th0405::SubpixelPoint option_cur
) noexcept;

// th04/main/player/shot_marisa_b.cpp.
[[nodiscard]] ShotPatternResult shot_marisa_b_l2(
	ShotSpawnState &, ShotState &, RandomState &
) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_b_l3(ShotSpawnState &, ShotState &) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_b_l4(
	ShotSpawnState &, ShotState &, RandomState &
) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_b_l5(
	ShotSpawnState &, ShotState &, RandomState &
) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_b_l6(ShotSpawnState &, ShotState &) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_b_l7(ShotSpawnState &, ShotState &) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_b_l8(ShotSpawnState &, ShotState &) noexcept;
[[nodiscard]] ShotPatternResult shot_marisa_b_l9(ShotSpawnState &, ShotState &) noexcept;

} // namespace th04::core
