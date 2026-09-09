#pragma once
#include "th02/core/midboss2.hpp"
#include "th02/core/laser.hpp"
#include "th02/core/stage_state_fwd.hpp"

namespace th02::core {
struct Midboss4State {
	std::int16_t active{};
	std::int16_t defeat_frame{};
	std::int16_t pellet_top{};
	std::uint8_t pattern{};
	std::uint8_t patterns_seen{};
	// The source caches its selected page pointer; frame 1 redirects it to page 0.
	std::uint8_t position_page{};
	bool operator==(const Midboss4State &) const = default;
};
[[nodiscard]] Midboss2InvalidateResult midboss4_invalidate(
	StageState &state, Midboss2Callbacks &presentation) noexcept;
[[nodiscard]] Midboss2UpdateResult midboss4_update_and_render(
	StageState &state, Midboss2Callbacks &presentation,
	LaserSpawnCallbacks &laser_presentation) noexcept;
} // namespace th02::core
