#include "th02/core/tile_scroll.hpp"

#include <bit>

namespace th02::core {
namespace {

inline constexpr std::uint16_t tile_scroll_initial_full_row = 23U;
inline constexpr std::uint8_t tile_scroll_height = 16U;
inline constexpr std::size_t native_page_count = 2U;

[[nodiscard]] std::int8_t byte_subtract(std::int8_t lhs,
	std::uint8_t rhs) noexcept
{
	const auto lhs_bits = std::bit_cast<std::uint8_t>(lhs);
	return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
		static_cast<std::uint16_t>(lhs_bits) - rhs));
}

[[nodiscard]] std::int8_t byte_add(std::int8_t lhs,
	std::uint8_t rhs) noexcept
{
	const auto lhs_bits = std::bit_cast<std::uint8_t>(lhs);
	return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
		static_cast<std::uint16_t>(lhs_bits) + rhs));
}

[[nodiscard]] bool scroll_line_is_native(std::int16_t scroll_line) noexcept
{
	return scroll_line >= 0 && scroll_line < tile_scroll_vram_height;
}

[[nodiscard]] std::size_t section_index_for(std::uint16_t full_row) noexcept
{
	return static_cast<std::size_t>(full_row / tile_scroll_rows_per_section);
}

[[nodiscard]] std::size_t row_index_for(std::uint16_t full_row) noexcept
{
	return static_cast<std::size_t>(full_row % tile_scroll_rows_per_section);
}

[[nodiscard]] std::size_t ring_row_for(std::int16_t scroll_line) noexcept
{
	return static_cast<std::size_t>(scroll_line / tile_scroll_height);
}

void copy_line(TileCopyCallbacks &callbacks, std::size_t page,
	std::int16_t left, std::int16_t scroll_line,
	const TileScrollState &state, std::uint8_t image
) noexcept
{
	callbacks.copy_line({
		page,
		left,
		scroll_line,
		state.copy_lines_top,
		state.copy_lines_h,
		image,
	});
}

} // namespace

void tiles_stuff_reset(TileScrollState &state, std::size_t &page_back,
	std::uint8_t &tile_mode, bool &tiles_egc_render_all) noexcept
{
	state.map_full_row_at_top_of_screen = tile_scroll_initial_full_row;
	state.tile_line_at_top = 0;
	page_back = 0U;
	tile_mode = tile_mode_tiles;
	tiles_egc_render_all = false;
}

TileScrollResult tiles_scroll_and_egc_render_both(
	TileScrollState &state, const EnemyStageData &stage,
	const TileSectionTiles &section_tiles, std::int16_t scroll_line,
	std::uint8_t speed, std::size_t page_front, std::size_t page_back,
	TileCopyCallbacks &callbacks
) noexcept
{
	state.tile_line_at_top = byte_subtract(state.tile_line_at_top, speed);

	if(state.tile_line_at_top < 0) {
		state.tile_line_at_top = byte_add(state.tile_line_at_top,
			tile_scroll_height);
		state.map_full_row_at_top_of_screen = static_cast<std::uint16_t>(
			static_cast<std::uint32_t>(state.map_full_row_at_top_of_screen) + 1U);

		const auto section_index = section_index_for(
			state.map_full_row_at_top_of_screen);
		const auto source_map_length = static_cast<std::uint16_t>(stage.map_length);
		if(section_index >= static_cast<std::size_t>(source_map_length)) {
			return TileScrollResult::map_end;
		}
		if(!scroll_line_is_native(scroll_line) ||
			section_index >= stage.map.size()) {
			return TileScrollResult::invalid_native_reference;
		}

		const auto tile_section = static_cast<std::size_t>(stage.map[section_index]);
		if(tile_section >= section_tiles.size()) {
			return TileScrollResult::invalid_native_reference;
		}

		auto &ring_row = state.tile_ring[ring_row_for(scroll_line)];
		const auto &source_row = section_tiles[tile_section][row_index_for(
			state.map_full_row_at_top_of_screen)];
		for(std::size_t x = 0U; x < tile_scroll_columns; ++x) {
			ring_row[x] = source_row[x];
			state.tiles_for_new_row[x] = source_row[x];
		}
	}

	state.copy_lines_top = static_cast<std::uint16_t>(state.tile_line_at_top);
	state.copy_lines_h = speed;
	if(!scroll_line_is_native(scroll_line) || page_front >= native_page_count) {
		return TileScrollResult::invalid_native_reference;
	}

	for(std::size_t x = 0U; x < tile_scroll_columns; ++x) {
		copy_line(callbacks, page_front, static_cast<std::int16_t>(
			tile_scroll_playfield_left +
			static_cast<std::int16_t>(x * tile_scroll_tile_width)),
			scroll_line, state, state.tiles_for_new_row[x]);
	}
	if(page_back >= native_page_count) {
		return TileScrollResult::invalid_native_reference;
	}

	for(std::size_t x = tile_scroll_columns; x > 0U; --x) {
		const auto tile_x = x - 1U;
		copy_line(callbacks, page_back, static_cast<std::int16_t>(
			tile_scroll_playfield_left +
			static_cast<std::int16_t>(tile_x * tile_scroll_tile_width)),
			scroll_line, state, state.tiles_for_new_row[tile_x]);
	}
	// The source's final reverse-loop iteration reads tiles_for_new_row[-1],
	// which is the high byte of the preceding copy-lines word in source BSS.
	copy_line(callbacks, page_back, static_cast<std::int16_t>(
		tile_scroll_playfield_left - tile_scroll_tile_width), scroll_line, state,
		static_cast<std::uint8_t>(state.copy_lines_h >> 8U));
	return TileScrollResult::completed;
}

} // namespace th02::core
