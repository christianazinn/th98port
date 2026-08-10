#ifndef TH03_PRESENT_CELS_HPP
#define TH03_PRESENT_CELS_HPP

#include <cstdint>

#include "th03/assets/types.hpp"
#include "th03/present/raster.hpp"

namespace th03::present {

enum class CdgAlphaMode : std::uint8_t {
	no_alpha,
	alpha,
};

enum class MrsColorMode : std::uint8_t {
	regular,
	altered,
};

// Portable equivalents of TH03's byte-aligned CDG puts. The source floors
// [left] to the preceding 8-pixel boundary. CDG rows remain bottom-to-top in
// the decoded model and are projected into top-to-bottom raster coordinates.
[[nodiscard]] bool cdg_put_8(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const assets::PlanarCel4 &cel
);
[[nodiscard]] bool cdg_put_noalpha_8(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const assets::PlanarCel4 &cel
);
[[nodiscard]] bool cdg_put_hflip_8(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const assets::PlanarCel4 &cel
);

// Draws a centered CDG through the selected source alpha path, then applies
// the TH03 staff-roll dissolve pattern to the E plane only. Strength is modulo
// 8, matching the source.
[[nodiscard]] bool cdg_put_dissolve_e_8(
	Raster &raster,
	std::int32_t center_x,
	std::int32_t center_y,
	const assets::PlanarCel4 &cel,
	std::uint8_t strength,
	CdgAlphaMode alpha_mode
);

// Clears the E plane on the bottom two rows of the previously centered CDG
// and on the row immediately below it, matching the staff-roll upward motion
// helper. The remaining B/R/G bits are preserved.
[[nodiscard]] bool cdg_unput_for_upwards_motion_e_8(
	Raster &raster,
	std::int32_t center_x,
	std::int32_t center_y,
	const assets::PlanarCel4 &cel
);

// MRS takes its vertical destination in source 400-line screen coordinates
// and divides it by two before writing the zoomed 200-line VRAM backing.
[[nodiscard]] bool mrs_put_8(
	Raster &raster,
	std::int32_t left,
	std::uint16_t screen_top,
	const assets::MrsImage &image
);
[[nodiscard]] bool mrs_put_noalpha_8(
	Raster &raster,
	std::int32_t left,
	std::uint16_t screen_top,
	const assets::MrsImage &image,
	MrsColorMode color_mode
);

// The source mrs_hflip() persistently transforms the loaded asset. Its native
// equivalent is assets::horizontal_flip(); these puts consume either stored
// orientation without adding a second presentation-side flip state.

} // namespace th03::present

#endif
