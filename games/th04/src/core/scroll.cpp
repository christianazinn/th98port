#include "th04/core/scroll.hpp"
#include "th04/core/state.hpp"

namespace th04::core {
namespace {
bool fill_row(State &s, std::size_t destination, std::uint16_t section_offset,
	std::uint8_t row) noexcept
{
	if (section_offset >= s.std_data.data.size()) return false;
	const auto section = s.std_data.data[section_offset];
	if (section >= 32 || row >= 5 || destination >= 25) return false;
	const auto offset = static_cast<std::size_t>(section) * 320 + row * 64U;
	if (offset > s.map_data.size() || s.map_data.size() - offset < 48) return false;
	// Only 24 of each row's 32 words are copied; the remaining words persist.
	for (std::size_t x = 0; x < 24; ++x) {
		s.tile_ring[destination * 32 + x] = static_cast<std::uint16_t>(
			s.map_data[offset + x * 2] | (s.map_data[offset + x * 2 + 1] << 8));
	}
	return true;
}
}
bool map_load(State &s, std::span<const std::uint8_t> file)
{
	if (file.size() < 8) return false;
	const auto size = static_cast<std::size_t>(file[0] | (file[1] << 8));
	if (file.size() - 8 < size) return false;
	s.map_data.assign(file.begin() + 8, file.begin() + 8 + static_cast<std::ptrdiff_t>(size));
	return true;
}
bool tiles_fill_initial(State &s) noexcept
{
	for (std::uint16_t section = 0; section < 5; ++section) {
		for (std::uint8_t row = 0; row < 5; ++row) {
			if (!fill_row(s, 24U - section * 5U - row, section,
				static_cast<std::uint8_t>(4 - row))) return false;
		}
	}
	return true;
}
bool scroll_update(State &s, Presentation &p) noexcept
{
	if (s.page_back >= s.scroll_line_on_page.size()) return false;
	s.scroll_line_on_page[s.page_back] = s.scroll_line;
	if (s.scroll_lines_prev_frame && s.scroll_active) p.graph_scroll(s.scroll_line);
	s.scroll_last_delta = 0;
	s.scroll_subpixel_line = static_cast<std::uint8_t>(
		s.scroll_subpixel_line + s.std_data.scroll_speed);
	if (s.scroll_subpixel_line >= 16) {
		const auto pixels = static_cast<std::uint8_t>(s.scroll_subpixel_line >> 4);
		s.scroll_line = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(s.scroll_line - pixels));
		if (s.scroll_line < 0) s.scroll_line = static_cast<std::int16_t>(s.scroll_line + 400);
		s.scroll_lines_pending = pixels;
		s.scroll_subpixel_line &= 15;
		s.scroll_last_delta = static_cast<std::int16_t>(pixels * 16);
	}
	if ((!s.scroll_lines_pending && !s.scroll_lines_prev_frame) || !s.std_data.scroll_speed) return true;
	const auto row = static_cast<std::int16_t>(s.scroll_line >> 4);
	if (row != s.tile_ring_row_filled) {
		s.tile_ring_row_filled = row;
		s.std_data.tile_row_in_section = std::bit_cast<std::int8_t>(
			static_cast<std::uint8_t>(s.std_data.tile_row_in_section - 1));
		if (s.std_data.tile_row_in_section < 0) {
			s.std_data.tile_row_in_section = 4;
			s.std_data.map_section_id = std::bit_cast<std::int16_t>(
				static_cast<std::uint16_t>(s.std_data.map_section_id + 1));
			++s.std_data.scroll_speed_offset;
			if (s.std_data.scroll_speed_offset >= s.std_data.data.size()) return false;
			s.std_data.scroll_speed = s.std_data.data[s.std_data.scroll_speed_offset];
			if (!s.std_data.scroll_speed) {
				s.scroll_line = 0;
				s.scroll_lines_prev_frame = s.scroll_lines_pending = 0;
				return true;
			}
		}
		if (!fill_row(s, static_cast<std::uint16_t>(row),
			static_cast<std::uint16_t>(s.std_data.map_section_id),
			static_cast<std::uint8_t>(s.std_data.tile_row_in_section))) return false;
	}
	s.scroll_lines_prev_frame = s.scroll_lines_pending;
	// The remaining source work copies pending lines to VRAM, then clears them.
	s.scroll_lines_pending = 0;
	return true;
}
}
