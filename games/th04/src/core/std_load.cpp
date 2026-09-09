#include "th04/core/std.hpp"

namespace th04::core {
bool std_load(StdState &state, std::span<const std::uint8_t> file)
{
	if (file.size() < 3) return false;
	const auto declared = static_cast<unsigned>(file[0]) |
		(static_cast<unsigned>(file[1]) << 8);
	if (declared == 0 || file.size() - 3 < declared - 1) return false;
	const auto data = file.subspan(3, declared - 1);
	std::size_t offset = file[2];
	if (offset >= data.size()) return false;
	const auto scroll_speed = data[offset];
	const auto scroll_offset = offset + 5; // TILES_Y / TILE_ROWS_PER_SECTION
	if (scroll_offset >= data.size()) return false;
	offset += static_cast<unsigned>(scroll_speed) + 1;
	if (offset >= data.size()) return false;
	const auto count = data[offset++];
	// Source's byte do-while would read 256 entries for zero, overflowing its
	// 32-entry table. Do not manufacture adjacent host globals for that input.
	if (count == 0 || count > state.enemy_scripts.size()) return false;
	auto scripts = state.enemy_scripts;
	for (unsigned i = 0; i < count; ++i) {
		if (offset >= data.size()) return false;
		const auto length = data[offset++];
		scripts[i] = static_cast<std::uint16_t>(offset);
		if (length > data.size() - offset) return false;
		offset += length;
	}
	++offset; // Source skips the stage-stream marker.
	if (offset > data.size() || data.size() - offset < 2) return false;
	state.data.assign(data.begin(), data.end());
	state.enemy_scripts = scripts;
	state.ip = static_cast<std::uint16_t>(offset);
	state.scroll_speed_offset = static_cast<std::uint16_t>(scroll_offset);
	state.scroll_speed = scroll_speed; // Source initially stores the chunk size.
	state.map_section_id = 4; // (368 / 16) / 5
	state.tile_row_in_section = 0;
	state.active = true;
	return true;
}
} // namespace th04::core
