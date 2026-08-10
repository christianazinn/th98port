#ifndef TH03_PRESENT_PI_HPP
#define TH03_PRESENT_PI_HPP

#include <cstdint>

#include "th03/assets/types.hpp"
#include "th03/present/raster.hpp"

namespace th03::present {

enum class CutscenePiQuarter : std::uint8_t {
	top_left = 0U,
	top_right,
	bottom_left,
	bottom_right,
};

enum class CutscenePiMaskStep : std::uint8_t {
	first = 0U,
	second,
	third,
	fourth,
};

inline constexpr std::int32_t cutscene_pi_left = 160;
inline constexpr std::int32_t cutscene_pi_top = 64;
inline constexpr std::uint16_t cutscene_pi_width = 320U;
inline constexpr std::uint16_t cutscene_pi_height = 200U;

// Portable equivalent of TH03's pi_put_8(). The source floors [left] and the
// image width to 8-pixel boundaries, clips horizontally, and wraps successive
// rows at the 400-line VRAM boundary. As required by graph_pack_put_8(), [top]
// must name an initial VRAM row in [0, 399].
[[nodiscard]] bool pi_put_8(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const assets::PiImage &image
);

// Portable equivalent of loading a PI with PIF_LINESKIP and then putting it.
// The output height is floor(source height / 2): decoded source row 2*j is
// written to destination row j, every odd row and any final unpaired even row
// are discarded. X alignment, width truncation, clipping, and 400-line
// wrapping otherwise match pi_put_8(). The complete decoded image is validated
// before the first destination pixel is changed.
[[nodiscard]] bool pi_put_lineskip_8(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const assets::PiImage &image
);

// TH03 cutscene pictures are the four 320x200 quarters of a 640x400 PI. These
// operations write the selected quarter at the source's fixed (160, 64)
// destination. A masked step copies only pixels selected by the corresponding
// cumulative four-row PI_MASKS pattern; the final unmasked put completes the
// reveal.
[[nodiscard]] bool pi_cutscene_quarter_put_8(
	Raster &raster,
	const assets::PiImage &image,
	CutscenePiQuarter quarter
);
[[nodiscard]] bool pi_cutscene_quarter_put_masked(
	Raster &raster,
	const assets::PiImage &image,
	CutscenePiQuarter quarter,
	CutscenePiMaskStep step
);

} // namespace th03::present

#endif
