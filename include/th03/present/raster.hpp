#ifndef TH03_PRESENT_RASTER_HPP
#define TH03_PRESENT_RASTER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "th03/assets/types.hpp"
#include "th03/present/planar.hpp"

namespace th03::present {

struct Rgb8 {
	std::uint8_t r = 0U;
	std::uint8_t g = 0U;
	std::uint8_t b = 0U;

	friend bool operator==(const Rgb8 &, const Rgb8 &) = default;
};

struct Palette16Rgb8 {
	std::array<Rgb8, 16U> colors{};

	friend bool operator==(
		const Palette16Rgb8 &,
		const Palette16Rgb8 &
	) = default;
};

inline constexpr std::uint16_t graphics_width = 640U;
inline constexpr std::uint16_t graphics_height = 400U;
inline constexpr std::size_t graphics_page_pixels =
	static_cast<std::size_t>(graphics_width) * graphics_height;

using Index4 = std::uint8_t;

enum class Page : std::uint8_t {
	zero = 0U,
	one = 1U,
};

enum class RasterMode : std::uint8_t {
	lines_400,
	zoomed_200_upper,
	zoomed_200_lower,
};

struct Point {
	std::int32_t x = 0;
	std::int32_t y = 0;
};

struct Rect {
	std::int32_t x = 0;
	std::int32_t y = 0;
	std::uint16_t width = 0U;
	std::uint16_t height = 0U;
};

// Caller-owned row-major 4-bit pixels. Stride is measured in bytes.
struct IndexedSource {
	std::span<const Index4> pixels{};
	std::uint16_t width = 0U;
	std::uint16_t height = 0U;
	std::size_t stride = 0U;
};

class Raster {
public:
	Raster();

	[[nodiscard]] bool set_access_page(Page page) noexcept;
	[[nodiscard]] bool set_shown_page(Page page) noexcept;
	[[nodiscard]] Page access_page() const noexcept;
	[[nodiscard]] Page shown_page() const noexcept;
	// Mirrors graph_200line() and graph_400line(): selecting any hardware
	// raster mode also resets the access page to page 0.
	[[nodiscard]] bool set_raster_mode(RasterMode mode) noexcept;
	[[nodiscard]] RasterMode raster_mode() const noexcept;

	// These operations write the selected access page only.
	[[nodiscard]] bool clear(Rect rect, Index4 color) noexcept;
	// Clears one complete hardware plane on the selected access page while
	// preserving the other three planes.
	[[nodiscard]] bool clear_plane(PlanarPlane plane) noexcept;
	// Sets one hardware-plane bit throughout the clipped rectangle while
	// preserving the other three planes.
	[[nodiscard]] bool set_plane(Rect rect, PlanarPlane plane) noexcept;
	[[nodiscard]] bool plot(Point point, Index4 color) noexcept;
	[[nodiscard]] bool blit(IndexedSource source, Point destination) noexcept;
	[[nodiscard]] bool blit_transparent(
		IndexedSource source,
		Point destination,
		Index4 transparent_color
	) noexcept;
	// A nonzero mask byte copies the corresponding source pixel.
	[[nodiscard]] bool blit_masked(
		IndexedSource source,
		std::span<const std::uint8_t> mask,
		std::size_t mask_stride,
		Point destination
	) noexcept;

	// PC-98 planar input uses B, R, G, E planes and MSB-first horizontal bits.
	// All writes are clipped to the 640x400 page. Invalid sources are rejected
	// before any pixel is changed.
	[[nodiscard]] bool blit_planar(
		PlanarSource source,
		Point destination,
		PlanarWriteMode mode = PlanarWriteMode::replace
	) noexcept;
	// Replaces one destination plane from packed 1bpp input while preserving
	// the other three planes. This is the chunky native analogue of a PC-98
	// single-plane VRAM surface write.
	[[nodiscard]] bool write_plane(
		PlanarPlane plane,
		BitSource source,
		Point destination
	) noexcept;
	// A 1 alpha bit replaces all four destination plane bits; a 0 preserves
	// the destination. This is the source SPRITE16 OVERLAP_CLEAR behavior.
	[[nodiscard]] bool blit_planar_masked(
		PlanarSource source,
		BitSource alpha,
		Point destination
	) noexcept;
	// Clears all four destination plane bits selected by a positive 1bpp mask.
	// This is the logical result of the source GRCG RMW color-0 alpha pass.
	[[nodiscard]] bool clear_planar_masked(
		BitSource mask,
		Point destination
	) noexcept;
	// Emulates byte-aligned GRCG writes. Width and destination X must be
	// multiples of 8; the CPU source controls RMW masks and only defines the
	// write extent in TDW mode.
	[[nodiscard]] bool grcg_write(
		BitSource cpu_source,
		Point destination,
		GrcgWriteMode mode,
		GrcgTile tile
	) noexcept;

	// Source and destination are explicit. A same-page overlap behaves as though
	// all source pixels were read before the first destination write.
	[[nodiscard]] bool copy_page(
		Page source,
		Rect source_rect,
		Page destination,
		Point target
	);

	[[nodiscard]] bool set_pending_palette_entry(
		std::uint8_t index,
		assets::Rgb4 color
	) noexcept;
	// master.lib stores its logical Palettes array as RGB8. Asset RGB4 entries
	// use the overload above, which expands each nibble to v * 0x11 exactly as
	// palette_entry_rgb(); runtime palette effects use this full-width setter.
	[[nodiscard]] bool set_pending_palette_entry_rgb8(
		std::uint8_t index,
		Rgb8 color
	) noexcept;
	[[nodiscard]] const Palette16Rgb8 &pending_palette() const noexcept;
	[[nodiscard]] const assets::Palette16Rgb4 &applied_palette() const noexcept;
	// PaletteTone is a signed source word. Applying extracts each logical RGB8
	// component's high nibble, then clamps a local tone copy to 0..200 while
	// preserving the raw value exactly as palette_show() does.
	void set_palette_tone(std::int16_t tone) noexcept;
	[[nodiscard]] std::int16_t palette_tone() const noexcept;
	[[nodiscard]] assets::Palette16Rgb4 palette_for_tone(
		std::int16_t tone
	) const noexcept;
	void apply_pending_palette() noexcept;

	void set_scroll_y(std::uint16_t scroll_y) noexcept;
	[[nodiscard]] std::uint16_t scroll_y() const noexcept;

	[[nodiscard]] std::optional<Index4> pixel(Page page, Point point) const noexcept;
	[[nodiscard]] std::optional<std::uint8_t> planar_byte(
		Page page,
		PlanarPlane plane,
		std::uint16_t byte_x,
		std::uint16_t y
	) const noexcept;
	[[nodiscard]] std::span<const Index4> graphics(Page page) const noexcept;
	// The raw shown page. Scroll composition is intentionally deferred.
	[[nodiscard]] std::span<const Index4> shown_graphics() const noexcept;

	// Provisional FNV-1a diagnostic over the raw shown page, applied palette,
	// raw PaletteTone, raster mode, shown-page identity, and logical vertical
	// scroll. It is not a complete visual-oracle or presenter-snapshot hash;
	// pending palette edits are absent.
	[[nodiscard]] std::uint64_t display_state_hash() const noexcept;

private:
	std::array<std::vector<Index4>, 2U> pages_{};
	Page access_page_ = Page::zero;
	Page shown_page_ = Page::zero;
	RasterMode raster_mode_ = RasterMode::lines_400;
	Palette16Rgb8 pending_palette_{};
	assets::Palette16Rgb4 applied_palette_{};
	std::int16_t palette_tone_ = 100;
	std::uint16_t scroll_y_ = 0U;
};

} // namespace th03::present

#endif
