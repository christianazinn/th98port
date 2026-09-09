#include "th02/core/scroll.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
using namespace th02::core;
void require(bool condition)
{
	if (!condition) { std::fputs("scroll assertion failed\n", stderr); std::abort(); }
}

struct Sink final : ScrollCallbacks {
	unsigned copies{};
	unsigned begins{};
	unsigned ends{};
	unsigned scrolls{};
	ScrollPresentation last{};
	void copy_line(const TileLineCopy &) noexcept override { ++copies; }
	void egc_begin() noexcept override { ++begins; }
	void egc_end() noexcept override { ++ends; }
	void graph_scroll(ScrollPresentation request) noexcept override {
		++scrolls;
		last = request;
	}
};

void complete_map(std::uint8_t speed, unsigned last_frame,
	std::int16_t final_line)
{
	ScrollState state{};
	scroll_reset(state);
	state.interval = 1;
	state.speed = speed;
	TileScrollState tiles{};
	std::size_t page_back = 1;
	std::uint8_t tile_mode = 2;
	bool render_all = true;
	tiles_stuff_reset(tiles, page_back, tile_mode, render_all);
	require(page_back == 0 && tile_mode == 1 && !render_all);
	EnemyStageData stage{};
	stage.map_length = 4;
	TileSectionTiles sections{};
	Sink sink{};
	// Reset starts at map row23. The first scroll enters row24; row32 ends
	// the four-section map after 129 one-pixel scroll operations.
	for (unsigned frame = 1; frame <= last_frame; ++frame) {
		require(scroll_advance_line(state, state.line));
		const auto result = scroll_finish_frame(state, tiles, stage, sections,
			1, page_back, sink);
		require(result == (frame == last_frame ? TileScrollResult::map_end : TileScrollResult::completed));
	}
	require(state.done && state.line == final_line && state.cycle == last_frame - 1);
	require(state.step == 16 && state.sad == (final_line - 1) * 40);
	require(sink.begins == last_frame && sink.ends == last_frame);
	require(sink.scrolls == last_frame - 1 && sink.copies == (last_frame - 1) * 49U);
	require(sink.last.upper_lines == 128 && sink.last.upper_address == 272 * 40);
	// The following frame has no scroll but still increments the cycle.
	require(scroll_advance_line(state, state.line));
	require(scroll_finish_frame(state, tiles, stage, sections, 1, page_back, sink) == TileScrollResult::completed);
	require(state.cycle == last_frame && sink.begins == last_frame && state.line == final_line);
}
}

int main()
{
	complete_map(1, 129, 272);
	// Source corrects by one even when scrolling by two pixels.
	complete_map(2, 65, 271);
	ScrollState state{};
	scroll_reset(state);
	require(state.speed == 1 && state.interval == 4 && !state.done);
	require(scroll_advance_line(state, 0));
	require(state.line == 399 && state.delta == 1);
	state.cycle = 1;
	require(scroll_advance_line(state, 399));
	require(state.line == 399 && state.delta == 0);
	state.cycle = 4;
	require(scroll_advance_line(state, 399));
	require(state.line == 398 && state.delta == 1);
	state.interval = 0;
	require(!scroll_advance_line(state, 123));
	require(state.line == 123 && state.delta == 0);
	state.done = true;
	require(scroll_advance_line(state, 321));
	require(state.line == 321 && state.delta == 0);
	scroll_reset(state);
	state.speed = 2;
	state.interval = 1;
	require(scroll_advance_line(state, 1));
	require(state.line == 399 && state.delta == 2);
}
