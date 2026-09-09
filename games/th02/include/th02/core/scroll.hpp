#pragma once

#include <cstdint>
#include "th02/core/tile_scroll.hpp"

namespace th02::core {

struct ScrollState {
	std::int16_t line{};
	std::uint8_t speed{};
	std::uint8_t cycle{};
	std::uint8_t interval{};
	bool done{};
	std::uint8_t delta{};
	std::int16_t sad{};
	std::int16_t step{};
	std::int16_t step_advanced{};
	std::int16_t unused{};
	std::uint8_t unused_2{};
	std::uint8_t unused_3{};
	bool operator==(const ScrollState &) const = default;
};

// GDC scroll submission is presentation only. The final map frame deliberately
// does not issue it, even though entities have already used the advanced line.
struct ScrollPresentation {
	std::int16_t upper_lines{};
	std::int16_t upper_address{};
	std::int16_t lower_lines{400};
	std::int16_t lower_address{};
	bool operator==(const ScrollPresentation &) const = default;
};

class ScrollCallbacks : public TileCopyCallbacks {
public:
	virtual void egc_begin() noexcept = 0;
	virtual void egc_end() noexcept = 0;
	virtual void graph_scroll(ScrollPresentation request) noexcept = 0;
};

void scroll_reset(ScrollState &state) noexcept;

// The pre-entity portion of stage_loop. Restores the saved current line after
// old-page invalidation, then advances scrolling. Interval zero is a source
// division fault; native code reports it after retaining delta/line writes.
[[nodiscard]] bool scroll_advance_line(ScrollState &state,
	std::int16_t saved_line) noexcept;

// Called after entity updates. Uses the actual map/ring progression rather
// than accepting a caller-supplied map-end verdict. Does not flip pages.
[[nodiscard]] TileScrollResult scroll_finish_frame(ScrollState &state,
	TileScrollState &tiles, const EnemyStageData &stage,
	const TileSectionTiles &sections, std::size_t page_front,
	std::size_t page_back, ScrollCallbacks &callbacks) noexcept;

} // namespace th02::core
