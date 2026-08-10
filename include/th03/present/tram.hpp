#ifndef TH03_PRESENT_TRAM_HPP
#define TH03_PRESENT_TRAM_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace th03::present {

inline constexpr std::uint8_t tram_width = 80U;
inline constexpr std::uint8_t tram_height = 25U;
inline constexpr std::size_t tram_cell_count =
	static_cast<std::size_t>(tram_width) * tram_height;

inline constexpr std::uint8_t gaiji_width = 16U;
inline constexpr std::uint8_t gaiji_height = 16U;
inline constexpr std::size_t gaiji_bytes_per_row = 2U;
inline constexpr std::size_t gaiji_glyph_bytes =
	gaiji_height * gaiji_bytes_per_row;
inline constexpr std::size_t gaiji_count = 256U;

using TramCode = std::uint16_t;
using TramAttribute = std::uint16_t;

// Raw PC-98 text attributes used by TH03. Keep the complete word even though
// the source constants fit in its low byte.
inline constexpr TramAttribute tram_black = 0x0001U;
inline constexpr TramAttribute tram_blue = 0x0021U;
inline constexpr TramAttribute tram_red = 0x0041U;
inline constexpr TramAttribute tram_magenta = 0x0061U;
inline constexpr TramAttribute tram_green = 0x0081U;
inline constexpr TramAttribute tram_cyan = 0x00A1U;
inline constexpr TramAttribute tram_yellow = 0x00C1U;
inline constexpr TramAttribute tram_white = 0x00E1U;
inline constexpr TramAttribute tram_blink = 0x0002U;
inline constexpr TramAttribute tram_reverse = 0x0004U;
inline constexpr TramAttribute tram_underline = 0x0008U;

struct TramPoint {
	std::int32_t x = 0;
	std::int32_t y = 0;
};

struct TramRect {
	std::int32_t x = 0;
	std::int32_t y = 0;
	std::uint8_t width = 0U;
	std::uint8_t height = 0U;
};

struct TramCell {
	TramCode code = 0U;
	TramAttribute attribute = 0U;

	[[nodiscard]] friend constexpr bool operator==(
		const TramCell &,
		const TramCell &
	) = default;
};

// Source byte order from gaiji_write(): left half, right half, for each of 16
// rows. Within each half, bit 7 is the leftmost pixel.
struct GaijiGlyph {
	std::array<std::uint8_t, gaiji_glyph_bytes> row_halves{};

	[[nodiscard]] friend constexpr bool operator==(
		const GaijiGlyph &,
		const GaijiGlyph &
	) = default;
};

enum class GaijiHalf : std::uint8_t {
	left,
	right,
};

struct GaijiCellReference {
	std::uint8_t glyph_id = 0U;
	GaijiHalf half = GaijiHalf::left;

	[[nodiscard]] friend constexpr bool operator==(
		const GaijiCellReference &,
		const GaijiCellReference &
	) = default;
};

[[nodiscard]] constexpr TramCode gaiji_cell_code(
	std::uint8_t glyph_id,
	GaijiHalf half
) noexcept {
	const auto row = static_cast<std::uint16_t>(
		0x56U + (static_cast<std::uint16_t>(glyph_id) >> 7U)
	);
	const auto column = static_cast<std::uint16_t>(glyph_id & 0x7FU) |
		((half == GaijiHalf::right) ? 0x80U : 0U);
	return static_cast<TramCode>((column << 8U) | row);
}

[[nodiscard]] std::optional<GaijiCellReference> decode_gaiji_cell(
	TramCode code
) noexcept;

// Gaiji IDs 0 and 128 have left-cell codes identical to ANK V and W. A raw
// code is a gaiji only when its adjacent cell forms the matching ordered pair.
[[nodiscard]] std::optional<GaijiCellReference> decode_paired_gaiji_cell(
	std::span<const TramCell> cells,
	std::size_t offset
) noexcept;

class Tram {
public:
	Tram() = default;

	// Source text routines require valid coordinates; these portable methods
	// reject an invalid write atomically instead of inventing clipping.
	void fill(TramCell cell) noexcept;
	[[nodiscard]] bool fill_attributes(
		TramRect rect,
		TramAttribute attribute
	) noexcept;
	[[nodiscard]] bool put_cell(TramPoint point, TramCell cell) noexcept;
	[[nodiscard]] bool put_gaiji(
		TramPoint left,
		std::uint8_t glyph_id,
		TramAttribute attribute
	) noexcept;

	[[nodiscard]] bool load_gaiji(
		std::uint8_t glyph_id,
		std::span<const std::uint8_t> row_halves
	) noexcept;

	void set_visible(bool visible) noexcept;
	[[nodiscard]] bool visible() const noexcept;
	[[nodiscard]] std::optional<TramCell> cell(TramPoint point) const noexcept;
	[[nodiscard]] std::span<const TramCell> cells() const noexcept;
	[[nodiscard]] const GaijiGlyph &gaiji(std::uint8_t glyph_id) const noexcept;
	[[nodiscard]] std::optional<std::uint8_t> gaiji_row_half(
		std::uint8_t glyph_id,
		std::uint8_t row,
		GaijiHalf half
	) const noexcept;

	// Provisional diagnostic over raw cells/attributes, visibility, and only
	// the gaiji glyphs referenced by current TRAM cells. Graphics stay separate.
	[[nodiscard]] std::uint64_t visible_state_hash() const noexcept;

private:
	std::array<TramCell, tram_cell_count> cells_{};
	std::array<GaijiGlyph, gaiji_count> gaiji_{};
	bool visible_ = true;
};

} // namespace th03::present

#endif
