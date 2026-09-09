#include "th02/core/tile_scroll.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

class Recorder final : public TileCopyCallbacks {
public:
	std::array<TileLineCopy, 64U> copies{};
	std::size_t count{};

	void copy_line(const TileLineCopy &request) noexcept override
	{
		if(count < copies.size()) copies[count] = request;
		++count;
	}
};

[[nodiscard]] EnemyStageData stage_with_map(std::int16_t map_length) noexcept
{
	EnemyStageData stage{};
	stage.map_length = map_length;
	for(std::size_t index = 0U; index < stage.map.size(); ++index) {
		stage.map[index] = static_cast<std::uint8_t>(index % tile_scroll_section_count);
	}
	return stage;
}

[[nodiscard]] TileSectionTiles patterned_sections() noexcept
{
	TileSectionTiles sections{};
	for(std::size_t section = 0U; section < sections.size(); ++section) {
		for(std::size_t row = 0U; row < sections[section].size(); ++row) {
			for(std::size_t column = 0U;
				column < sections[section][row].size(); ++column) {
				sections[section][row][column] = static_cast<std::uint8_t>(
					(section * 80U) + (row * tile_scroll_columns) + column);
			}
		}
	}
	return sections;
}

[[nodiscard]] bool test_reset_is_complete_and_retains_source_arrays() noexcept
{
	TileScrollState state{};
	state.map_full_row_at_top_of_screen = 7U;
	state.tile_line_at_top = -3;
	state.copy_lines_top = 0xAAAAU;
	state.copy_lines_h = 0xBEEFU;
	state.tiles_for_new_row[0U] = 0xCCU;
	state.tile_ring[0U][0U] = 0xDDU;
	std::size_t page_back = 1U;
	std::uint8_t tile_mode = 7U;
	bool render_all = true;

	tiles_stuff_reset(state, page_back, tile_mode, render_all);
	return state.map_full_row_at_top_of_screen == 23U &&
		state.tile_line_at_top == 0 && page_back == 0U &&
		tile_mode == tile_mode_tiles && !render_all &&
		state.copy_lines_top == 0xAAAAU && state.copy_lines_h == 0xBEEFU &&
		state.tiles_for_new_row[0U] == 0xCCU &&
		state.tile_ring[0U][0U] == 0xDDU;
}

[[nodiscard]] bool test_regular_copy_order_and_predecessor_byte() noexcept
{
	auto state = TileScrollState{};
	state.tile_line_at_top = 5;
	state.copy_lines_h = 0xA5A5U;
	for(std::size_t x = 0U; x < state.tiles_for_new_row.size(); ++x) {
		state.tiles_for_new_row[x] = static_cast<std::uint8_t>(0x80U + x);
	}
	auto stage = stage_with_map(4);
	const auto sections = patterned_sections();
	Recorder copies;
	if(tiles_scroll_and_egc_render_both(state, stage, sections, 32, 2U,
		1U, 0U, copies) != TileScrollResult::completed || copies.count != 49U ||
		state.tile_line_at_top != 3 || state.copy_lines_top != 3U ||
		state.copy_lines_h != 2U) {
		return false;
	}
	for(std::size_t x = 0U; x < tile_scroll_columns; ++x) {
		const auto left = static_cast<std::int16_t>(32 + (x * 16U));
		if(copies.copies[x] != TileLineCopy{
			1U, left, 32, 3U, 2U, static_cast<std::uint8_t>(0x80U + x)} ||
			copies.copies[tile_scroll_columns + x] != TileLineCopy{
			0U, static_cast<std::int16_t>(400 - (x * 16U)), 32, 3U, 2U,
				static_cast<std::uint8_t>(0x97U - x)}) {
			return false;
		}
	}
	return copies.copies[48U] == TileLineCopy{0U, 16, 32, 3U, 2U, 0U};
}

[[nodiscard]] bool test_row_refill_map_end_and_reference_prefixes() noexcept
{
	auto state = TileScrollState{};
	state.map_full_row_at_top_of_screen = 23U;
	state.copy_lines_top = 0xAAAAU;
	state.copy_lines_h = 0xBBBBU;
	state.tiles_for_new_row.fill(0xEEU);
	state.tile_ring[24U].fill(0xDDU);
	auto stage = stage_with_map(5);
	stage.map[3U] = 2U;
	const auto sections = patterned_sections();
	Recorder copies;
	if(tiles_scroll_and_egc_render_both(state, stage, sections, 384, 1U,
		0U, 1U, copies) != TileScrollResult::completed || copies.count != 49U ||
		state.tile_line_at_top != 15 ||
		state.map_full_row_at_top_of_screen != 24U ||
		state.tile_ring[24U] != sections[2U][0U] ||
		state.tiles_for_new_row != sections[2U][0U] ||
		copies.copies[0U] != TileLineCopy{0U, 32, 384, 15U, 1U,
			sections[2U][0U][0U]} ||
		copies.copies[48U] != TileLineCopy{1U, 16, 384, 15U, 1U, 0U}) {
		return false;
	}

	state = {};
	state.map_full_row_at_top_of_screen = 23U;
	state.copy_lines_top = 0xAAAAU;
	state.copy_lines_h = 0xBBBBU;
	const auto state_before_end = state;
	stage = stage_with_map(3);
	copies = {};
	if(tiles_scroll_and_egc_render_both(state, stage, sections, 384, 1U,
		0U, 1U, copies) != TileScrollResult::map_end || copies.count != 0U ||
		state.tile_line_at_top != 15 ||
		state.map_full_row_at_top_of_screen != 24U ||
		state.copy_lines_top != state_before_end.copy_lines_top ||
		state.copy_lines_h != state_before_end.copy_lines_h) {
		return false;
	}

	state = {};
	state.map_full_row_at_top_of_screen = 23U;
	state.copy_lines_top = 0xAAAAU;
	state.copy_lines_h = 0xBBBBU;
	stage = stage_with_map(4);
	stage.map[3U] = 16U;
	copies = {};
	if(tiles_scroll_and_egc_render_both(state, stage, sections, 384, 1U,
		0U, 1U, copies) != TileScrollResult::invalid_native_reference ||
		copies.count != 0U || state.tile_line_at_top != 15 ||
		state.map_full_row_at_top_of_screen != 24U ||
		state.copy_lines_top != 0xAAAAU || state.copy_lines_h != 0xBBBBU) {
		return false;
	}
	return true;
}

[[nodiscard]] bool test_invalid_line_and_pages_retain_their_prefixes() noexcept
{
	auto state = TileScrollState{};
	state.tile_line_at_top = 1;
	state.tiles_for_new_row.fill(0x22U);
	const auto stage = stage_with_map(4);
	const auto sections = patterned_sections();
	Recorder copies;
	if(tiles_scroll_and_egc_render_both(state, stage, sections, 400, 1U,
		0U, 1U, copies) != TileScrollResult::invalid_native_reference ||
		state.tile_line_at_top != 0 || state.copy_lines_top != 0U ||
		state.copy_lines_h != 1U || copies.count != 0U) {
		return false;
	}

	state = {};
	state.tile_line_at_top = 1;
	state.tiles_for_new_row.fill(0x33U);
	copies = {};
	if(tiles_scroll_and_egc_render_both(state, stage, sections, 80, 1U,
		0U, 2U, copies) != TileScrollResult::invalid_native_reference ||
		copies.count != 24U || state.copy_lines_top != 0U ||
		state.copy_lines_h != 1U || copies.copies[0U] !=
		TileLineCopy{0U, 32, 80, 0U, 1U, 0x33U} ||
		copies.copies[23U] != TileLineCopy{0U, 400, 80, 0U, 1U, 0x33U}) {
		return false;
	}

	state = {};
	state.tile_line_at_top = 1;
	state.tiles_for_new_row.fill(0x44U);
	copies = {};
	return tiles_scroll_and_egc_render_both(state, stage, sections, 80, 1U,
		2U, 0U, copies) == TileScrollResult::invalid_native_reference &&
		copies.count == 0U && state.copy_lines_top == 0U &&
		state.copy_lines_h == 1U;
}

} // namespace

int main()
{
	if(!test_reset_is_complete_and_retains_source_arrays()) {
		std::fputs("FAIL reset\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_regular_copy_order_and_predecessor_byte()) {
		std::fputs("FAIL regular\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_row_refill_map_end_and_reference_prefixes()) {
		std::fputs("FAIL row\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_invalid_line_and_pages_retain_their_prefixes()) {
		std::fputs("FAIL invalid\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
