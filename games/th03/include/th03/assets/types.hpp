#ifndef TH03_ASSETS_TYPES_HPP
#define TH03_ASSETS_TYPES_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace th03::assets {

struct Rgb4 {
	std::uint8_t r = 0U;
	std::uint8_t g = 0U;
	std::uint8_t b = 0U;

	friend bool operator==(const Rgb4 &, const Rgb4 &) = default;
};

struct Palette16Rgb4 {
	std::array<Rgb4, 16U> colors{};

	friend bool operator==(
		const Palette16Rgb4 &,
		const Palette16Rgb4 &
	) = default;
};

struct IndexedImage4 {
	std::uint16_t width = 0U;
	std::uint16_t height = 0U;

	// Row-major, one byte per pixel. Every value is in [0, 15].
	std::vector<std::uint8_t> indices;
};

struct PiImage {
	// Header data is retained as format data, without native pointers.
	std::vector<std::byte> comment;
	std::uint8_t mode = 0U;
	std::uint8_t aspect_n = 0U;
	std::uint8_t aspect_m = 0U;
	std::uint8_t plane_count = 0U;
	std::array<std::uint8_t, 4U> machine{};
	std::vector<std::byte> machine_extension;

	// Mode bit 7 suppresses the inline palette.
	std::optional<Palette16Rgb4> palette;
	IndexedImage4 image;
};

enum class PlanarColorPlane : std::size_t {
	blue = 0U,
	red,
	green,
	intensity,
	count,
};

enum class PlanarRowOrder {
	bottom_to_top,
	top_to_bottom,
};

enum class PlanarBitOrder {
	most_significant_bit_left,
};

struct PlanarCel4 {
	std::uint16_t bitplane_size = 0U;
	std::uint16_t pixel_width = 0U;
	std::uint16_t pixel_height = 0U;
	std::int16_t offset_at_bottom_left = 0;
	std::uint16_t vram_dword_width = 0U;

	// CDG rows are stored bottom-to-top, with the high bit at the left.
	PlanarRowOrder row_order = PlanarRowOrder::bottom_to_top;
	PlanarBitOrder bit_order = PlanarBitOrder::most_significant_bit_left;

	// Source plane order is alpha, then B/R/G/E. A consumer can explicitly
	// discard alpha at decode time to model the original no-alpha load calls.
	std::optional<std::vector<std::uint8_t>> alpha;
	std::array<
		std::vector<std::uint8_t>,
		static_cast<std::size_t>(PlanarColorPlane::count)
	> color_planes;
};

struct CdgHeader {
	std::uint16_t bitplane_size = 0U;
	std::uint16_t pixel_width = 0U;
	std::uint16_t pixel_height = 0U;
	std::int16_t offset_at_bottom_left = 0;
	std::uint16_t vram_dword_width = 0U;

	// The original format stores 0 for a single-image file.
	std::uint8_t stored_image_count = 0U;
	std::int8_t plane_layout = 0;

	// These are real-mode segment fields in the in-memory slot structure and
	// must both be zero in a file header.
	std::uint16_t reserved_alpha_segment = 0U;
	std::uint16_t reserved_color_segment = 0U;
};

struct CdgFile {
	CdgHeader header;
	std::vector<PlanarCel4> cels;
};

enum class CdgAlphaPolicy {
	preserve,
	discard,
};

enum class MrsOrientation {
	as_stored,
	horizontally_flipped,
};

struct MrsImage {
	PlanarCel4 cel;
	MrsOrientation orientation = MrsOrientation::as_stored;
};

enum class BfntConsumerPolicy {
	gaiji,
	superimpose,
};

struct BfntHeader {
	std::array<std::byte, 5U> identifier{};
	std::uint8_t color_flags = 0U;
	std::uint8_t version = 0U;
	std::uint8_t reserved = 0U;
	std::uint16_t pixel_width = 0U;
	std::uint16_t pixel_height = 0U;
	std::uint16_t first_code = 0U;
	std::uint16_t last_code = 0U;
	std::array<std::byte, 8U> font_name{};
	std::uint32_t timestamp = 0U;
	std::uint16_t extended_header_size = 0U;
	std::uint16_t first_extended_record_size = 0U;
};

struct BfntExtendedHeader {
	std::vector<std::byte> bytes;

	// Extension record 0x10 supplies the low four bits of its data byte.
	std::optional<std::uint8_t> transparent_color;
};

struct MonochromeBitmap1 {
	PlanarRowOrder row_order = PlanarRowOrder::top_to_bottom;
	PlanarBitOrder bit_order = PlanarBitOrder::most_significant_bit_left;
	std::vector<std::uint8_t> bits;
};

struct PlanarBitmap4 {
	PlanarRowOrder row_order = PlanarRowOrder::top_to_bottom;
	PlanarBitOrder bit_order = PlanarBitOrder::most_significant_bit_left;
	std::array<
		std::vector<std::uint8_t>,
		static_cast<std::size_t>(PlanarColorPlane::count)
	> color_planes;
};

using BfntGlyphPixels = std::variant<MonochromeBitmap1, PlanarBitmap4>;

struct BfntGlyph {
	std::uint16_t code = 0U;
	BfntGlyphPixels pixels;
};

struct BfntSheet {
	BfntHeader header;
	BfntConsumerPolicy consumer = BfntConsumerPolicy::superimpose;
	BfntExtendedHeader extended_header;
	std::optional<Palette16Rgb4> palette;

	// master.lib defaults this to color 0 when extension record 0x10 is absent.
	std::uint8_t transparent_color = 0U;
	std::vector<BfntGlyph> glyphs;
};

enum class AssetErrorCode {
	truncated,
	bad_magic,
	unsupported_layout,
	invalid_value,
	size_mismatch,
	malformed_stream,
	output_overflow,
	index_out_of_range,
};

struct AssetError {
	AssetErrorCode code;
	std::string message;
	std::size_t offset = 0U;
};

} // namespace th03::assets

#endif
