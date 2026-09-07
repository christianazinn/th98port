#ifndef TH01_PRESENT_VRAM_HPP
#define TH01_PRESENT_VRAM_HPP

// TH01's renderer model: a plane-native PC-98 graphics VRAM.
//
// WHAT THIS IS AND IS NOT A CLAIM ABOUT. `th03-port`'s `present::Raster`
// already models two 640x400 pages, an access/shown split, GRCG TDW and RMW
// writes, planar blits, the palette and readback, and 91% of it is game-neutral
// by measurement rather than by intent. This header does not replace it and
// nothing here contradicts it. It exists because of a delta this campaign
// measured to be exactly one primitive wide -- see `egc_copy_rect_1_to_0_16()`
// below -- and because `th01-port` is built by copying, never by linking into
// the donor.
//
// STORAGE, and the reason that is a weaker decision than it looks. Plane-native
// and chunky both reproduce TH01's line-wrap correctly: a row-major chunky page
// and a linear plane-byte offset are the same linear order scaled by eight, so
// an overflowing X runs into the following row either way. The choice here is
// about transcription cost, and it is counted: `git grep` over `th01/` at
// `dc6cf486` finds 101 direct single-plane accesses in 24 files -- 28
// `VRAM_CHUNK`, 9 `VRAM_SNAP`, 42 `VRAM_PLANE_[BRGE]`, 22 `Planar<>` -- against
// a donor (`th03-port`) whose drawing goes through GRCG writes and sprite blits
// and which therefore stores chunky and synthesises plane bytes on demand
// (`planar_byte()`, `th03-port/src/present/planar.cpp:409`). The costs invert
// with the donor, so the storage does. `th01-port/docs/RENDERER_MODEL.md`
// carries the full comparison and says plainly which parts of it are forced.
//
// WHAT THE SDL LAYER REQUIRES OF THIS, which is the other half of the question.
// Almost nothing. The copied platform layer's texture path is
// `th03-port/src/platform/sdl_platform.cpp:353` ->
// `app::source_surface_to_host_rgba`, whose whole input is
// `present::SourceSurface::pixels` plus `::graphics_palette`
// (`th03-port/src/app/host_surface.cpp:119-181`). It takes no page selector and
// mentions neither planes nor the GRCG. So the seam between a TH01 renderer and
// the host is one 640x400 span of resolved indices plus 16 palette entries --
// `to_indexed()` below -- and the entire two-page, plane-native, EGC-shaped
// model lives on this side of it, invisible to the host.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace th01::present {

/// Geometry
/// --------
/// The donor's `RES_X`, `RES_Y`, `BYTE_DOTS`, `ROW_SIZE` and `PLANE_SIZE`
/// (`pc98.h:100-106`), and `EGC_REGISTER_DOTS`/`EGC_REGISTER_SIZE`
/// (`pc98.h:228-231`). Re-declared rather than included: the donor's are
/// preprocessor macros in a 16-bit real-mode header.

inline constexpr int res_x = 640;
inline constexpr int res_y = 400;
inline constexpr int byte_dots = 8;
inline constexpr int byte_bits = 3;
inline constexpr int row_size = res_x / byte_dots;
inline constexpr int plane_size = row_size * res_y;
inline constexpr int egc_register_dots = 16;
inline constexpr int egc_register_size = egc_register_dots / byte_dots;

inline constexpr std::size_t page_pixels =
	static_cast<std::size_t>(res_x) * static_cast<std::size_t>(res_y);

// PC-98 plane order, and therefore colour bit order: a pixel's index is
// B | (R << 1) | (G << 2) | (E << 3).
enum class Plane : std::uint8_t {
	b = 0U,
	r = 1U,
	g = 2U,
	e = 3U,
};
inline constexpr std::size_t plane_count = 4U;

// PC-98 16-colour VRAM carries two 640x400 pages for every game. TH01 shows
// page 0 permanently -- `graph_showpage_func()` has exactly one call site in
// the whole game, `th01/hardware/graph.cpp:94` -- and uses page 1 as the
// restore source that its unblitters read from. That is a *policy*, not a
// different hardware model, so it is not encoded here.
enum class Page : std::uint8_t {
	zero = 0U,
	one = 1U,
};
inline constexpr std::size_t page_count = 2U;

// One resolved 4-bit colour index. Spelled to match
// `th03-port/include/th03/present/raster.hpp:38` because `to_indexed()` feeds
// the same host converter.
using Index4 = std::uint8_t;

/// Address arithmetic
/// ------------------

// The donor's `x & 0xFFF0` (`th01/hardware/egcrect.cpp:13`), written so that it
// still means "round down to a multiple of 16" on a host whose `int` is not 16
// bits. Under Turbo C++ `0xFFF0` is an `int` equal to -16 and the mask is an
// arithmetic floor that works for negative X; on a 32-bit host the same literal
// is 65520 and the expression silently becomes something else. Negative X is
// not a hypothetical here: `egc_wave_unput()` (`th01/hardware/egcwave.cpp`)
// exists to feed this function X coordinates that leave the screen.
[[nodiscard]] constexpr int floor_to_egc_register(int x) noexcept
{
	return x & ~(egc_register_dots - 1);
}

// `vram_offset_shift()`, `planar.h:105` -- the SHIFT spelling.
[[nodiscard]] constexpr int vram_offset_shift(int x, int y) noexcept
{
	return (x >> byte_bits) + (y * row_size);
}

// `vram_offset_muldiv()`, `planar.h:128` -- the DIVIDE spelling.
//
// These two are NOT the same function, and the difference is not cosmetic. For
// negative X an arithmetic right shift floors while integer division truncates
// toward zero, so they disagree by one byte -- eight pixels -- for every X in
// (-8, 0), and generally wherever `x % 8 != 0` and `x < 0`. The donor picked
// between them per call site for code-generation reasons and TH01 uses both. A
// port that "simplifies" them into one changes where those pixels land.
[[nodiscard]] constexpr int vram_offset_muldiv(int x, int y) noexcept
{
	return (y * row_size) + (x / byte_dots);
}

/// The surface
/// -----------

class Vram {
public:
	Vram();

	// The access page: what CPU reads and writes address. `page_accessed` in
	// the donor (`th01/hardware/graph.cpp`, `graph_accesspage_func()`).
	void set_access_page(Page page) noexcept;
	[[nodiscard]] Page access_page() const noexcept;

	// A single plane byte, eight dots, MSB first. `offset` is a linear
	// plane-byte offset in [0, plane_size); anything else is refused rather
	// than guessed at, because what the hardware does past the end of a plane
	// is not something this campaign has measured. Wrap from one row into the
	// next is NOT that case -- it is ordinary linear addressing and is
	// reproduced exactly.
	[[nodiscard]] bool plane_byte(
		Page page, Plane plane, int offset, std::uint8_t &out
	) const noexcept;
	[[nodiscard]] bool set_plane_byte(
		Page page, Plane plane, int offset, std::uint8_t value
	) noexcept;

	// The EGC in the only configuration TH01 ever puts it in.
	//
	// `egc_setup_copy()` (`th01/hardware/egc_impl.hpp:4`) enables all four
	// planes, selects a memory-read latch and a 16-bit transfer length, and
	// every TH01 EGC copy runs under it. `egc_setup_copy_masked()` sits beside
	// it in the same donor header and has ZERO call sites under `th01/`. So
	// TH01 needs no EGC register model, no ROP and no mask: one 16-bit access
	// at a plane-B offset (`egc_chunk()`, `planar.h:248`) moves 16 dots across
	// all four planes, which on a host is a two-byte copy per plane. Modelled
	// as bytes rather than as a `uint16_t` so the model has no endianness.
	[[nodiscard]] bool egc_copy_chunk_1_to_0(int offset) noexcept;

	// `egc_copy_rect_1_to_0_16()`, `th01/hardware/egcrect.cpp:3`.
	//
	// Blits page 1 to page 0 at the same position, from
	// (floor(left/16)*16, top) to (ceil((left + w)/16)*16, top + h).
	// Deliberately unclipped: the donor's own header documents that an X range
	// leaving the screen "will wrap around into the next or previous line"
	// (`th01/hardware/egc.h`), and `egc_wave_unput()` depends on it visually.
	//
	// THIS FUNCTION ALREADY EXISTS IN THE DONOR TREE, and the diff against it
	// is the whole finding. `NewClassicMenuFont::restore_rect()`
	// (`th03-port/src/app/new_classic_assets.cpp:554-592`) is documented as the
	// "exact page-1-to-page-0 EGC restore granularity used by mnufont.cpp": it
	// floors `left` with `& ~0x0F`, ceils `right` with `(right + 15) & ~0x0F`,
	// calls `Raster::copy_page(Page::one -> Page::zero)`, and leaves the access
	// page at 0. Every one of those is what TH01 does. It differs in exactly
	// one respect -- it clamps `left`, `right` and `bottom` into the page
	// first, and `copy_page()` clips again underneath it
	// (`clipped_mapping()`, `th03-port/src/present/raster.cpp:122-175`).
	// TH01 must not, because `egc_wave_unput()` (`th01/hardware/egcwave.cpp`)
	// exists to drive X off both edges and the wrap is what the player sees.
	// So: same primitive, one clamp removed, and that is the measured size of
	// the renderer gap between the two games.
	//
	// Leaves the access page at 0, as the donor does -- but only if at least
	// one register was copied, because the donor sets it inside the inner loop
	// (`egcrect.cpp:23-24`) and an empty rectangle therefore leaves it alone.
	// `restore_rect()` sets it unconditionally on success; that difference is
	// unobservable there and observable here, so the donor's spelling wins.
	//
	// Returns false, having written nothing, if the rectangle would address
	// outside a plane; see `set_plane_byte()`.
	[[nodiscard]] bool egc_copy_rect_1_to_0_16(
		int left, int top, int w, int h
	) noexcept;

	// The host seam. Resolves one page's four planes into `page_pixels`
	// row-major 4-bit indices -- the exact shape
	// `app::source_surface_to_host_rgba()` consumes once TRAM has been
	// composited over it. Returns false if `out` is not `page_pixels` long.
	[[nodiscard]] bool to_indexed(
		Page page, std::span<Index4> out
	) const noexcept;

private:
	[[nodiscard]] static bool offset_in_plane(int offset) noexcept;

	// [page][plane][offset]. 2 * 4 * 32,000 = 256,000 bytes, which is what a
	// PC-98 has.
	std::array<
		std::array<std::array<std::uint8_t, plane_size>, plane_count>,
		page_count
	> planes_{};
	Page access_page_ = Page::zero;
};

} // namespace th01::present

#endif /* TH01_PRESENT_VRAM_HPP */
