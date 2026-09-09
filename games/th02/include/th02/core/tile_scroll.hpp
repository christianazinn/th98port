#pragma once

#include "th02/core/stage_data.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

inline constexpr std::size_t tile_scroll_columns = 24U;
inline constexpr std::size_t tile_scroll_ring_rows = 25U;
inline constexpr std::size_t tile_scroll_section_count = 16U;
inline constexpr std::size_t tile_scroll_rows_per_section = 8U;
inline constexpr std::int16_t tile_scroll_playfield_left = 32;
inline constexpr std::int16_t tile_scroll_tile_width = 16;
inline constexpr std::int16_t tile_scroll_vram_height = 400;
inline constexpr std::uint8_t tile_mode_tiles = 1U;

using TileSectionRow = std::array<std::uint8_t, tile_scroll_columns>;
using TileSection = std::array<TileSectionRow, tile_scroll_rows_per_section>;
using TileSectionTiles = std::array<TileSection, tile_scroll_section_count>;

// Source state from th02/main/tile/tile.cpp. The source reset deliberately
// retains the copy fields, pending row, and ring; callers must not treat this
// as a value-initialized stage field.
struct TileScrollState {
	std::uint16_t map_full_row_at_top_of_screen{};
	std::int8_t tile_line_at_top{};
	std::uint16_t copy_lines_top{};
	std::uint16_t copy_lines_h{};
	std::array<std::uint8_t, tile_scroll_columns> tiles_for_new_row{};
	std::array<TileSectionRow, tile_scroll_ring_rows> tile_ring{};
	bool operator==(const TileScrollState &) const = default;
};

enum class TileScrollResult : std::uint8_t {
	completed,
	map_end,
	invalid_native_reference,
};

// A final source-to-playfield line copy. `left` includes the original
// back-page predecessor copy at 16, which is covered by the black TRAM area.
struct TileLineCopy {
	std::size_t page{};
	std::int16_t left{};
	std::int16_t scroll_line{};
	std::uint16_t source_line_top{};
	std::uint16_t line_count{};
	std::uint8_t image{};
	bool operator==(const TileLineCopy &) const = default;
};

// The EGC/VRAM operation is represented as a synchronous final presentation
// request. Implementations must not mutate the borrowed state during a call.
class TileCopyCallbacks {
public:
	virtual ~TileCopyCallbacks() = default;
	virtual void copy_line(const TileLineCopy &request) noexcept = 0;
};

// th02/main/tile/tile.cpp::tiles_stuff_reset. The last three references are
// root-owned source globals, kept borrowed so this port retains all five
// original writes without duplicating page or tile-render ownership.
void tiles_stuff_reset(TileScrollState &state, std::size_t &page_back,
	std::uint8_t &tile_mode, bool &tiles_egc_render_all) noexcept;

// th02/main/tile/tile.cpp::tiles_scroll_and_egc_render_both. `scroll_line`
// is the root-owned current line after pre-entity scrolling. The normal source
// caller supplies a speed no larger than one tile and pages 0/1. A source map,
// tile, ring, or page reference that native state cannot represent reports
// invalid_native_reference after retaining the prefix that preceded it.
[[nodiscard]] TileScrollResult tiles_scroll_and_egc_render_both(
	TileScrollState &state, const EnemyStageData &stage,
	const TileSectionTiles &section_tiles, std::int16_t scroll_line,
	std::uint8_t speed, std::size_t page_front, std::size_t page_back,
	TileCopyCallbacks &callbacks
) noexcept;

} // namespace th02::core
