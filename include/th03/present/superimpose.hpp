#ifndef TH03_PRESENT_SUPERIMPOSE_HPP
#define TH03_PRESENT_SUPERIMPOSE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "th03/assets/types.hpp"
#include "th03/present/raster.hpp"

namespace th03::present {

// Prepared equivalent of master.lib's super_entry_bfnt() pattern storage.
// Like master.lib's pattern registration, this owns packed copies of all four
// color planes plus the derived opacity plane. The decoded sheet can be
// released immediately after preparation.
class PreparedSuperimposeSheet {
public:
	PreparedSuperimposeSheet(PreparedSuperimposeSheet &&) noexcept = default;
	PreparedSuperimposeSheet &operator=(
		PreparedSuperimposeSheet &&
	) noexcept = default;

	PreparedSuperimposeSheet(const PreparedSuperimposeSheet &) = delete;
	PreparedSuperimposeSheet &operator=(
		const PreparedSuperimposeSheet &
	) = delete;

	[[nodiscard]] std::uint16_t first_code() const noexcept;
	[[nodiscard]] std::uint16_t last_code() const noexcept;
	[[nodiscard]] std::uint16_t pixel_width() const noexcept;
	[[nodiscard]] std::uint16_t pixel_height() const noexcept;

private:
	friend std::optional<PreparedSuperimposeSheet>
	prepare_superimpose_sheet(const assets::BfntSheet &);
	friend bool superimpose_put(
		Raster &,
		const PreparedSuperimposeSheet &,
		Point,
		std::uint16_t
	) noexcept;
	friend bool superimpose_put_blue_plane_row_white(
		Raster &,
		const PreparedSuperimposeSheet &,
		Point,
		std::uint16_t,
		std::uint8_t
	) noexcept;

	PreparedSuperimposeSheet(
		std::uint16_t width,
		std::uint16_t height,
		std::uint16_t first_code,
		std::uint16_t last_code,
		std::size_t plane_size,
		std::array<
			std::vector<std::uint8_t>,
			planar_plane_count
		> color_planes,
		std::vector<std::uint8_t> opacity
	) noexcept;

	std::uint16_t width_;
	std::uint16_t height_;
	std::uint16_t first_code_;
	std::uint16_t last_code_;
	std::size_t plane_size_;
	std::array<
		std::vector<std::uint8_t>,
		planar_plane_count
	> color_planes_;
	std::vector<std::uint8_t> opacity_;
};

// Validates one decoded superimpose BFNT and derives master.lib's positive
// opacity mask for every glyph. Allocation occurs only during preparation.
[[nodiscard]] std::optional<PreparedSuperimposeSheet>
prepare_superimpose_sheet(const assets::BfntSheet &sheet);

// Source-equivalent super_put() composition on Raster's current access page.
// Transparent pixels preserve the destination; every other pixel replaces all
// four color planes. Host page bounds safely clip stock-invalid destinations.
[[nodiscard]] bool superimpose_put(
	Raster &raster,
	const PreparedSuperimposeSheet &sheet,
	Point position,
	std::uint16_t glyph_code
) noexcept;

// Exact SUB_BDC2() subset used by MAIN's flying HUD banner. The sheet must be
// 32 pixels wide and contain source_row. Four bytes from one stored B-plane
// row are rotated into overlapping 16-pixel GRCG-white writes, with the
// source's byte-start X clipping.
[[nodiscard]] bool superimpose_put_blue_plane_row_white(
	Raster &raster,
	const PreparedSuperimposeSheet &sheet,
	Point position,
	std::uint16_t glyph_code,
	std::uint8_t source_row
) noexcept;

} // namespace th03::present

#endif
