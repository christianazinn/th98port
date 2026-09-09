#include "th02/core/scroll.hpp"

#include <bit>

namespace th02::core {

void scroll_reset(ScrollState &state) noexcept
{
	state.speed = 1;
	state.cycle = 0;
	state.line = 0;
	state.unused = 0;
	state.step = 0;
	state.interval = 4;
	state.done = false;
	state.unused_2 = 0;
	state.unused_3 = 0;
	state.sad = 0;
	state.step_advanced = 0;
	state.delta = 0;
}

bool scroll_advance_line(ScrollState &state, std::int16_t saved_line) noexcept
{
	state.delta = 0;
	state.line = saved_line;
	if (!state.done) {
		if (state.interval == 0) return false;
		if ((state.cycle % state.interval) == 0) {
			state.line = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
				static_cast<std::int32_t>(state.line) - state.speed));
			if (state.line < 0) {
				state.line = static_cast<std::int16_t>(state.line + 400);
			}
			state.delta = state.speed;
		}
	}
	return true;
}

TileScrollResult scroll_finish_frame(ScrollState &state, TileScrollState &tiles,
	const EnemyStageData &stage, const TileSectionTiles &sections,
	std::size_t page_front, std::size_t page_back,
	ScrollCallbacks &callbacks) noexcept
{
	if (state.delta != 0) {
		state.sad = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
			static_cast<std::int32_t>(state.line) * 40));
		if ((state.line & 7) == 0) {
			state.step = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
				static_cast<std::uint16_t>(state.step) + 1U));
			state.step_advanced = 1;
		}
		callbacks.egc_begin();
		const auto result = tiles_scroll_and_egc_render_both(tiles, stage,
			sections, state.line, state.speed, page_front, page_back, callbacks);
		if (result == TileScrollResult::invalid_native_reference) return result;
		state.done = (result == TileScrollResult::map_end);
		callbacks.egc_end();
		if (state.done) {
			state.line = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
				static_cast<std::uint16_t>(state.line) + 1U));
			return TileScrollResult::map_end;
		}
		callbacks.graph_scroll({
			std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
				400 - static_cast<std::int32_t>(state.line))), state.sad, 400, 0});
	}
	state.cycle = static_cast<std::uint8_t>(state.cycle + 1U);
	return TileScrollResult::completed;
}

} // namespace th02::core
