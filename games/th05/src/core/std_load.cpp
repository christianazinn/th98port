#include "th05/core/std.hpp"

namespace th05::core {
bool std_load(StdState &state, std::span<const std::uint8_t> file)
{
	if (file.size() < 2) return false;
	const auto declared = static_cast<unsigned>(file[0]) |
		(static_cast<unsigned>(file[1]) << 8);
	if (declared == 0 || declared > file.size() - 2) return false;
	const auto data = file.subspan(2, declared);
	std::array<std::uint16_t, 3> titles{};
	std::size_t offset = 0;
	for (auto &title : titles) {
		title = static_cast<std::uint16_t>(offset);
		while (offset < data.size() && data[offset] != 0) ++offset;
		if (offset == data.size()) return false;
		++offset;
	}
	if (offset >= data.size()) return false;
	const auto map_offset = offset + 5; // 1 + (368 / 16) / 5
	if (map_offset >= data.size()) return false;
	offset += static_cast<unsigned>(data[offset]) + 1;
	if (offset >= data.size()) return false;
	const auto scroll_speed = data[offset];
	const auto scroll_offset = offset + 5;
	if (scroll_offset >= data.size()) return false;
	offset += static_cast<unsigned>(scroll_speed) + 1;
	if (offset >= data.size()) return false;
	const auto count = data[offset++];
	// Zero executes 256 source iterations and overflows its 32-entry table.
	if (count == 0 || count > state.enemy_scripts.size()) return false;
	auto scripts = state.enemy_scripts;
	for (unsigned i = 0; i < count; ++i) {
		if (offset >= data.size()) return false;
		const auto length = data[offset++];
		scripts[i] = static_cast<std::uint16_t>(offset);
		if (length > data.size() - offset) return false;
		offset += length;
	}
	++offset; // The source skips one marker before the stage stream.
	if (offset > data.size() || data.size() - offset < 2) return false;
	state.data.assign(data.begin(), data.end());
	state.title_offsets = titles;
	state.enemy_scripts = scripts;
	state.ip = static_cast<std::uint16_t>(offset);
	state.map_section_offset = static_cast<std::uint16_t>(map_offset);
	state.scroll_speed_offset = static_cast<std::uint16_t>(scroll_offset);
	state.scroll_speed = scroll_speed; // Source initializes from chunk size.
	state.tile_row_in_section = 0;
	state.active = true;
	return true;
}
}
