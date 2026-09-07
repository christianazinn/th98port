#ifndef TH03_PRESENT_MONO_HPP
#define TH03_PRESENT_MONO_HPP

#include <cstddef>
#include <cstdint>
#include <span>

#include "th03/present/raster.hpp"

namespace th03::present {

inline constexpr std::size_t score_digit_count = 10U;
inline constexpr std::size_t score_digit_storage_rows = 8U;
inline constexpr std::size_t score_digit_draw_rows = 7U;
inline constexpr std::size_t score_font_mask_bytes =
	score_digit_count * score_digit_storage_rows;

inline constexpr std::size_t pellet_cel_count = 2U;
inline constexpr std::size_t pellet_preshift_count = 16U;
inline constexpr std::size_t pellet_row_count = 4U;
inline constexpr std::size_t pellet_row_bytes = 4U;
inline constexpr std::size_t pellet_variant_bytes =
	pellet_row_count * pellet_row_bytes;
inline constexpr std::size_t pellet_mask_bytes =
	pellet_cel_count * pellet_preshift_count * pellet_variant_bytes;

inline constexpr std::size_t staff_flake_cel_count = 4U;
inline constexpr std::size_t staff_flake_row_count = 8U;
inline constexpr std::size_t staff_flake_row_bytes = 2U;
inline constexpr std::size_t staff_flake_cel_bytes =
	staff_flake_row_count * staff_flake_row_bytes;
inline constexpr std::size_t staff_flake_mask_bytes =
	staff_flake_cel_count * staff_flake_cel_bytes;

// Immutable views over the source-generated 1bpp layouts. Import and content
// ownership remain with the caller; these types retain no path or allocation.
struct ScoreFontMasks {
	std::span<const std::uint8_t> rows{};
	PackedRowOrder row_order = PackedRowOrder::bottom_to_top;
};

enum class PelletCel : std::uint8_t {
	normal = 0U,
	transfer = 1U,
};

struct PelletMasks {
	// Layout: [cel][left & 15][row][4 bytes].
	std::span<const std::uint8_t> rows{};
	PackedRowOrder row_order = PackedRowOrder::top_to_bottom;
};

struct StaffFlakeMasks {
	// Layout: [cel][row][little-endian 16-bit word].
	std::span<const std::uint8_t> rows{};
	PackedRowOrder row_order = PackedRowOrder::top_to_bottom;
};

// SUB_D50E's emitted loop consumes stored rows 0..6 in ascending address
// order while walking VRAM upward. The eighth stored row remains unread.
[[nodiscard]] bool score_digit_put(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const ScoreFontMasks &font,
	std::uint8_t digit,
	Index4 color
) noexcept;

// Selects the source's stored (left & 15) variant. The first eight wrapped
// VRAM bytes use the source's 16-bit narrow writes; all other offsets use the
// complete 32-bit rows.
[[nodiscard]] bool pellet_put(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const PelletMasks &masks,
	PelletCel cel,
	Index4 color
) noexcept;

// Rotates each little-endian source word right by (left & 7) and ORs it into
// the B plane only, preserving the other three planes and unselected B bits.
[[nodiscard]] bool staff_flake_put_blue_or(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const StaffFlakeMasks &masks,
	std::uint8_t cel
) noexcept;

} // namespace th03::present

#endif
