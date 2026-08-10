#ifndef TH03_PRESENT_PLANAR_HPP
#define TH03_PRESENT_PLANAR_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace th03::present {

// PC-98 color bit and VRAM plane order. The packed byte for each plane uses
// bit 7 for its leftmost pixel and bit 0 for its rightmost pixel.
enum class PlanarPlane : std::uint8_t {
	blue = 0U,
	red = 1U,
	green = 2U,
	intensity = 3U,
};

inline constexpr std::size_t planar_plane_count = 4U;

enum class PackedRowOrder : std::uint8_t {
	top_to_bottom,
	bottom_to_top,
};

// Caller-owned row-major 1bpp pixels. Stride is measured in bytes. Each row
// needs at least ceil(width / 8) bytes.
struct BitSource {
	std::span<const std::uint8_t> bits{};
	std::uint16_t width = 0U;
	std::uint16_t height = 0U;
	std::size_t stride = 0U;
	PackedRowOrder row_order = PackedRowOrder::top_to_bottom;
};

// Four caller-owned 1bpp planes in B, R, G, E order. A color index is formed
// as B | (R << 1) | (G << 2) | (E << 3).
struct PlanarSource {
	std::array<std::span<const std::uint8_t>, planar_plane_count> planes{};
	std::uint16_t width = 0U;
	std::uint16_t height = 0U;
	std::size_t stride = 0U;
	PackedRowOrder row_order = PackedRowOrder::top_to_bottom;
};

enum class PlanarWriteMode : std::uint8_t {
	// Replace all four destination plane bits.
	replace,
	// OR each source plane bit into its destination plane independently.
	or_planes,
};

// GRCG tile-register bytes in hardware output order: B, R, G, E. Tile bit 7
// corresponds to the leftmost pixel of a destination VRAM byte.
struct GrcgTile {
	std::array<std::uint8_t, planar_plane_count> planes{};

	[[nodiscard]] static std::optional<GrcgTile> solid(
		std::uint8_t color_index
	) noexcept;
};

enum class GrcgWriteMode : std::uint8_t {
	// Tile Direct Write: the CPU byte is ignored and the tile is written.
	tile_direct,
	// Read-Modify-Write: 1 CPU bits select pixels replaced by the tile.
	read_modify_write,
};

} // namespace th03::present

#endif
