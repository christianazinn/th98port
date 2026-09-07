#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "th03/assets.hpp"

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const std::string &message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void append_u8(std::vector<std::byte> &bytes, std::uint8_t value)
{
	bytes.push_back(static_cast<std::byte>(value));
}

void append_be16(std::vector<std::byte> &bytes, std::uint16_t value)
{
	append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
	append_u8(bytes, static_cast<std::uint8_t>(value));
}

void append_le16(std::vector<std::byte> &bytes, std::uint16_t value)
{
	append_u8(bytes, static_cast<std::uint8_t>(value));
	append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
}

void append_le32(std::vector<std::byte> &bytes, std::uint32_t value)
{
	append_u8(bytes, static_cast<std::uint8_t>(value));
	append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
	append_u8(bytes, static_cast<std::uint8_t>(value >> 16U));
	append_u8(bytes, static_cast<std::uint8_t>(value >> 24U));
}

void set_le16(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::uint16_t value
)
{
	bytes[offset] = static_cast<std::byte>(value);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

class BitWriter {
public:
	void bit(std::uint8_t value)
	{
		if((bit_count_ % 8U) == 0U) {
			bytes_.push_back(std::byte{0U});
		}
		const auto shift = static_cast<unsigned>(7U - (bit_count_ % 8U));
		auto byte = std::to_integer<std::uint8_t>(bytes_.back());
		byte = static_cast<std::uint8_t>(
			byte | static_cast<std::uint8_t>((value & 1U) << shift)
		);
		bytes_.back() = static_cast<std::byte>(byte);
		++bit_count_;
	}

	void bits(std::uint32_t value, unsigned count)
	{
		for(unsigned remaining = count; remaining > 0U; --remaining) {
			bit(static_cast<std::uint8_t>((value >> (remaining - 1U)) & 1U));
		}
	}

	[[nodiscard]] const std::vector<std::byte> &bytes() const
	{
		return bytes_;
	}

private:
	std::vector<std::byte> bytes_;
	std::size_t bit_count_ = 0U;
};

class ColorEncoder {
public:
	ColorEncoder()
	{
		for(std::size_t previous = 0U; previous < tables_.size(); ++previous) {
			for(std::size_t index = 0U; index < tables_[previous].size(); ++index) {
				tables_[previous][index] = static_cast<std::uint8_t>(
					(previous + index + 1U) & 0x0FU
				);
			}
		}
	}

	void color(BitWriter &bits, std::uint8_t previous, std::uint8_t color)
	{
		auto &table = tables_[previous & 0x0FU];
		const auto found = std::find(table.begin(), table.end(), color);
		const auto table_index = static_cast<std::uint8_t>(
			std::distance(table.begin(), found)
		);
		const auto code = static_cast<std::uint8_t>(table_index ^ 0x0FU);
		write_code(bits, code);
		std::move(found + 1, table.end(), found);
		table.back() = color;
	}

	std::uint8_t code(
		BitWriter &bits,
		std::uint8_t previous,
		std::uint8_t code
	)
	{
		auto &table = tables_[previous & 0x0FU];
		const auto table_index = static_cast<std::uint8_t>(code ^ 0x0FU);
		const auto color = table[table_index];
		write_code(bits, code);
		std::move(
			table.begin() + static_cast<std::ptrdiff_t>(table_index + 1U),
			table.end(),
			table.begin() + static_cast<std::ptrdiff_t>(table_index)
		);
		table.back() = color;
		return color;
	}

private:
	static void write_code(BitWriter &bits, std::uint8_t code)
	{
		if(code <= 1U) {
			bits.bit(1U);
			bits.bits(code, 1U);
		} else if(code <= 3U) {
			bits.bits(0U, 2U);
			bits.bits(static_cast<std::uint32_t>(code - 2U), 1U);
		} else if(code <= 7U) {
			bits.bits(2U, 3U); // 010
			bits.bits(static_cast<std::uint32_t>(code - 4U), 2U);
		} else {
			bits.bits(3U, 3U); // 011
			bits.bits(static_cast<std::uint32_t>(code - 8U), 3U);
		}
	}

	std::array<std::array<std::uint8_t, 16U>, 16U> tables_{};
};

void write_position(BitWriter &bits, std::uint8_t position)
{
	if(position < 3U) {
		bits.bits(position, 2U);
	} else {
		bits.bits(3U, 2U);
		bits.bit(static_cast<std::uint8_t>(position - 3U));
	}
}

void write_length(BitWriter &bits, std::size_t length)
{
	unsigned suffix_width = 0U;
	for(auto remaining = length; remaining > 1U; remaining >>= 1U) {
		++suffix_width;
	}
	for(unsigned index = 0U; index < suffix_width; ++index) {
		bits.bit(1U);
	}
	bits.bit(0U);
	const auto base = std::size_t{1U} << suffix_width;
	bits.bits(
		static_cast<std::uint32_t>(length - base),
		suffix_width
	);
}

void write_run(BitWriter &bits, std::uint8_t position, std::size_t length)
{
	write_position(bits, position);
	write_length(bits, length);
}

struct PiHeaderOptions {
	std::uint8_t mode = 0U;
	std::uint8_t aspect_n = 0U;
	std::uint8_t aspect_m = 0U;
	std::uint8_t plane_count = 4U;
};

std::vector<std::byte> make_pi(
	std::uint16_t width,
	std::uint16_t height,
	const std::vector<std::byte> &compressed,
	PiHeaderOptions options = {}
)
{
	std::vector<std::byte> bytes;
	append_u8(bytes, static_cast<std::uint8_t>('P'));
	append_u8(bytes, static_cast<std::uint8_t>('i'));
	append_u8(bytes, static_cast<std::uint8_t>('T'));
	append_u8(bytes, static_cast<std::uint8_t>('3'));
	append_u8(bytes, 0x1AU);
	append_u8(bytes, 0U); // Dummy header terminator.
	append_u8(bytes, options.mode);
	append_u8(bytes, options.aspect_n);
	append_u8(bytes, options.aspect_m);
	append_u8(bytes, options.plane_count);
	for(const auto value : {0x01U, 0x02U, 0x03U, 0x04U}) {
		append_u8(bytes, static_cast<std::uint8_t>(value));
	}
	append_be16(bytes, 2U);
	append_u8(bytes, 0x12U);
	append_u8(bytes, 0x34U);
	append_be16(bytes, width);
	append_be16(bytes, height);
	if((options.mode & 0x80U) == 0U) {
		for(std::uint8_t color = 0U; color < 16U; ++color) {
			append_u8(bytes, static_cast<std::uint8_t>(color << 4U));
			append_u8(bytes, static_cast<std::uint8_t>((15U - color) << 4U));
			append_u8(bytes, static_cast<std::uint8_t>((color ^ 5U) << 4U));
		}
	}
	bytes.insert(bytes.end(), compressed.begin(), compressed.end());
	return bytes;
}

std::vector<std::byte> color_path_stream()
{
	BitWriter bits;
	ColorEncoder colors;
	const auto first = colors.code(bits, 0U, 0U);
	const auto second = colors.code(bits, first, 2U);
	write_run(bits, 1U, 1U);
	write_position(bits, 1U); // Repeated position selects literal colors.
	const auto high = colors.code(bits, second, 4U);
	(void)colors.code(bits, high, 8U);
	bits.bit(0U);
	return bits.bytes();
}

std::vector<std::byte> position_path_stream()
{
	BitWriter bits;
	ColorEncoder colors;
	colors.color(bits, 0U, 1U);
	colors.color(bits, 1U, 2U);

	write_run(bits, 1U, 2U);
	write_position(bits, 1U);
	colors.color(bits, 2U, 3U);
	colors.color(bits, 3U, 4U);
	bits.bit(1U);
	colors.color(bits, 4U, 5U);
	colors.color(bits, 5U, 6U);
	bits.bit(0U);
	write_run(bits, 0U, 2U);
	write_run(bits, 2U, 2U);
	write_run(bits, 3U, 2U);
	write_run(bits, 4U, 2U);
	write_run(bits, 0U, 2U);
	write_run(bits, 1U, 2U);
	return bits.bytes();
}

std::vector<std::byte> same_color_left_stream()
{
	BitWriter bits;
	ColorEncoder colors;
	colors.color(bits, 0U, 10U);
	colors.color(bits, 10U, 10U);
	write_run(bits, 1U, 1U);
	write_run(bits, 0U, 3U);
	return bits.bytes();
}

std::vector<std::byte> single_run_stream(std::size_t pixel_count)
{
	BitWriter bits;
	ColorEncoder colors;
	colors.color(bits, 0U, 10U);
	colors.color(bits, 10U, 10U);
	write_run(bits, 1U, (pixel_count / 2U) - 1U);
	return bits.bytes();
}

std::uint8_t cdg_fixture_byte(
	std::size_t cel,
	std::size_t plane,
	std::size_t index
)
{
	return static_cast<std::uint8_t>(
		(cel * 0x40U) + (plane * 0x10U) + index
	);
}

std::vector<std::byte> make_cdg(
	std::uint16_t pixel_width,
	std::uint16_t pixel_height,
	std::uint8_t stored_image_count
)
{
	const auto byte_width = static_cast<std::size_t>(pixel_width / 8U);
	const auto bitplane_size = byte_width * pixel_height;
	std::vector<std::byte> bytes;
	append_le16(bytes, static_cast<std::uint16_t>(bitplane_size));
	append_le16(bytes, pixel_width);
	append_le16(bytes, pixel_height);
	append_le16(bytes, static_cast<std::uint16_t>(
		80U * static_cast<std::uint16_t>(pixel_height - 1U)
	));
	append_le16(bytes, static_cast<std::uint16_t>(pixel_width / 32U));
	append_u8(bytes, stored_image_count);
	append_u8(bytes, 0U); // TH03 colors-and-alpha layout.
	append_le16(bytes, 0U); // File headers contain no alpha segment.
	append_le16(bytes, 0U); // File headers contain no color segment.

	const auto image_count = (stored_image_count == 0U) ?
		1U : static_cast<std::size_t>(stored_image_count);
	for(std::size_t cel = 0U; cel < image_count; ++cel) {
		for(std::size_t plane = 0U; plane < 5U; ++plane) {
			for(std::size_t index = 0U; index < bitplane_size; ++index) {
				append_u8(bytes, cdg_fixture_byte(cel, plane, index));
			}
		}
	}
	return bytes;
}

constexpr std::size_t mrs_test_width_bytes = 36U;
constexpr std::size_t mrs_test_height = 184U;
constexpr std::size_t mrs_test_plane_size =
	mrs_test_width_bytes * mrs_test_height;

std::uint8_t mrs_fixture_byte(
	std::size_t plane,
	std::size_t row,
	std::size_t column
)
{
	return static_cast<std::uint8_t>(
		(plane * 0x31U) + (row * 0x0DU) + (column * 0x07U)
	);
}

std::uint8_t reverse_fixture_bits(std::uint8_t value)
{
	value = static_cast<std::uint8_t>((value >> 4U) | (value << 4U));
	value = static_cast<std::uint8_t>(
		((value & 0xCCU) >> 2U) | ((value & 0x33U) << 2U)
	);
	return static_cast<std::uint8_t>(
		((value & 0xAAU) >> 1U) | ((value & 0x55U) << 1U)
	);
}

std::vector<std::byte> make_mrs()
{
	std::vector<std::byte> bytes;
	bytes.reserve(mrs_test_plane_size * 5U);
	for(std::size_t plane = 0U; plane < 5U; ++plane) {
		for(std::size_t row = 0U; row < mrs_test_height; ++row) {
			for(std::size_t column = 0U; column < mrs_test_width_bytes; ++column) {
				append_u8(bytes, mrs_fixture_byte(plane, row, column));
			}
		}
	}
	return bytes;
}

std::vector<std::uint8_t> expected_mrs_plane(
	std::size_t source_plane,
	bool horizontally_flipped
)
{
	std::vector<std::uint8_t> plane;
	plane.reserve(mrs_test_plane_size);
	for(std::size_t row = 0U; row < mrs_test_height; ++row) {
		for(std::size_t column = 0U; column < mrs_test_width_bytes; ++column) {
			const auto source_column = horizontally_flipped ?
				(mrs_test_width_bytes - 1U - column) : column;
			auto value = mrs_fixture_byte(source_plane, row, source_column);
			if(horizontally_flipped) {
				value = reverse_fixture_bits(value);
			}
			plane.push_back(value);
		}
	}
	return plane;
}

struct BfntFixtureOptions {
	std::uint8_t color_flags = 0x83U;
	std::uint16_t pixel_width = 8U;
	std::uint16_t pixel_height = 2U;
	std::uint16_t first_code = 5U;
	std::uint16_t last_code = 6U;
	std::vector<std::byte> extended_header;
	std::uint16_t first_extended_record_size = 0U;
};

std::vector<std::byte> make_bfnt(BfntFixtureOptions options = {})
{
	std::vector<std::byte> bytes;
	for(const auto value : {'B', 'F', 'N', 'T'}) {
		append_u8(bytes, static_cast<std::uint8_t>(value));
	}
	append_u8(bytes, 0x1AU);
	append_u8(bytes, options.color_flags);
	append_u8(bytes, 0x16U);
	append_u8(bytes, 0x5AU);
	append_le16(bytes, options.pixel_width);
	append_le16(bytes, options.pixel_height);
	append_le16(bytes, options.first_code);
	append_le16(bytes, options.last_code);
	for(const auto value : {'S', 'Y', 'N', 'T', 'H', '\0', '\1', '\2'}) {
		append_u8(bytes, static_cast<std::uint8_t>(value));
	}
	append_le32(bytes, 0x44332211U);
	append_le16(
		bytes,
		static_cast<std::uint16_t>(options.extended_header.size())
	);
	append_le16(bytes, options.first_extended_record_size);
	bytes.insert(
		bytes.end(),
		options.extended_header.begin(),
		options.extended_header.end()
	);

	if((options.color_flags & 0x80U) != 0U) {
		for(std::uint8_t color = 0U; color < 16U; ++color) {
			append_u8(bytes, static_cast<std::uint8_t>((color ^ 5U) << 4U));
			append_u8(bytes, static_cast<std::uint8_t>(color << 4U));
			append_u8(bytes, static_cast<std::uint8_t>((15U - color) << 4U));
		}
	}

	const auto glyph_count = static_cast<std::size_t>(
		options.last_code - options.first_code
	) + 1U;
	const auto pixel_count = static_cast<std::size_t>(options.pixel_width) *
		options.pixel_height;
	if((options.color_flags & 0x7FU) == 0U) {
		const auto bitplane_size = (pixel_count / 8U);
		for(std::size_t glyph = 0U; glyph < glyph_count; ++glyph) {
			for(std::size_t index = 0U; index < bitplane_size; ++index) {
				append_u8(bytes, static_cast<std::uint8_t>(glyph + index));
			}
		}
	} else {
		for(std::size_t glyph = 0U; glyph < glyph_count; ++glyph) {
			for(std::size_t pixel = 0U; pixel < pixel_count; pixel += 2U) {
				const auto left = static_cast<std::uint8_t>((glyph + pixel) & 0x0FU);
				const auto right = static_cast<std::uint8_t>(
					(glyph + pixel + 1U) & 0x0FU
				);
				append_u8(bytes, static_cast<std::uint8_t>((left << 4U) | right));
			}
		}
	}
	return bytes;
}

void test_rgb48()
{
	std::array<std::byte, 48U> bytes{};
	for(std::size_t color = 0U; color < 16U; ++color) {
		bytes[(color * 3U)] = static_cast<std::byte>(color);
		bytes[(color * 3U) + 1U] = static_cast<std::byte>(15U - color);
		bytes[(color * 3U) + 2U] = static_cast<std::byte>(color ^ 5U);
	}
	const auto palette = th03::assets::decode_rgb48(bytes);
	check(palette.has_value(), "RGB48 accepts a 16-entry RGB4 palette");
	if(palette) {
		check(
			palette->colors[3U] == th03::assets::Rgb4{3U, 12U, 6U},
			"RGB48 preserves source component order and four-bit values"
		);
	}

	check(
		!th03::assets::decode_rgb48(std::span(bytes).first(47U)),
		"RGB48 rejects truncation"
	);
	auto oversized = std::vector<std::byte>(bytes.begin(), bytes.end());
	oversized.push_back(std::byte{0U});
	check(
		!th03::assets::decode_rgb48(oversized),
		"RGB48 rejects trailing bytes"
	);
	auto invalid = bytes;
	invalid[17U] = std::byte{0x10U};
	const auto invalid_result = th03::assets::decode_rgb48(invalid);
	check(
		!invalid_result &&
		invalid_result.error().code == th03::assets::AssetErrorCode::invalid_value,
		"RGB48 rejects components outside the RGB4 range"
	);
}

void test_pi_color_and_header()
{
	const auto bytes = make_pi(4U, 1U, color_path_stream());
	const auto pi = th03::assets::decode_pi(bytes);
	check(pi.has_value(), "PI accepts the synthetic adaptive-color fixture");
	if(!pi) {
		return;
	}
	check(
		pi->comment == std::vector<std::byte>{std::byte{'T'}, std::byte{'3'}} &&
		pi->machine == std::array<std::uint8_t, 4U>{1U, 2U, 3U, 4U} &&
		pi->machine_extension ==
			std::vector<std::byte>{std::byte{0x12U}, std::byte{0x34U}},
		"PI retains owned non-pointer header metadata"
	);
	check(
		pi->palette.has_value() &&
		pi->palette->colors[3U] == th03::assets::Rgb4{3U, 12U, 6U},
		"PI exposes the hardware-visible RGB4 high nibbles"
	);
	check(
		pi->image.width == 4U && pi->image.height == 1U &&
		pi->image.indices == std::vector<std::uint8_t>{0U, 14U, 10U, 2U},
		"PI decodes all four adaptive color-code widths deterministically"
	);

	const auto no_palette = th03::assets::decode_pi(make_pi(
		4U,
		1U,
		color_path_stream(),
		PiHeaderOptions{.mode = 0x80U}
	));
	check(
		no_palette && !no_palette->palette.has_value() && no_palette->mode == 0x80U,
		"PI retains palette suppression without reading inline palette bytes"
	);
}

void test_pi_position_paths()
{
	const auto pi = th03::assets::decode_pi(make_pi(
		8U,
		4U,
		position_path_stream()
	));
	check(pi.has_value(), "PI accepts the synthetic position/run fixture");
	if(pi) {
		const std::vector<std::uint8_t> expected{
			1U, 2U, 1U, 2U, 3U, 4U, 5U, 6U,
			3U, 4U, 5U, 6U, 1U, 2U, 1U, 2U,
			4U, 5U, 6U, 1U, 6U, 1U, 2U, 1U,
			6U, 1U, 2U, 1U, 6U, 1U, 2U, 1U,
		};
		check(
			pi->image.indices == expected,
			"PI reproduces positions 0 through 4 and repeated-position literals"
		);
	}

	const auto same = th03::assets::decode_pi(make_pi(
		8U,
		1U,
		same_color_left_stream()
	));
	check(
		same && same->image.indices == std::vector<std::uint8_t>(8U, 10U),
		"PI position 0 uses a one-byte run when the prior pair has one color"
	);

	constexpr std::uint16_t large_width = 640U;
	constexpr std::uint16_t large_height = 400U;
	constexpr auto large_pixel_count =
		static_cast<std::size_t>(large_width) * large_height;
	auto large_bytes = make_pi(
		large_width,
		large_height,
		single_run_stream(large_pixel_count)
	);
	large_bytes.insert(large_bytes.end(), 3U, std::byte{0U});
	const auto large = th03::assets::decode_pi(large_bytes);
	check(
		large && large->image.width == large_width &&
		large->image.height == large_height &&
		large->image.indices == std::vector<std::uint8_t>(large_pixel_count, 10U),
		"PI decodes nonzero dimension high bytes and ignores source-compatible trailer padding"
	);
}

void test_pi_rejections()
{
	auto valid = make_pi(4U, 1U, color_path_stream());
	auto bad = valid;
	bad[0U] = std::byte{'X'};
	check(
		!th03::assets::decode_pi(bad),
		"PI rejects bad magic"
	);

	const auto retained_mode = th03::assets::decode_pi(make_pi(
			4U,
			1U,
			color_path_stream(),
			PiHeaderOptions{.mode = 0x01U}
		));
	check(
		retained_mode && retained_mode->mode == 0x01U &&
		retained_mode->palette.has_value(),
		"PI retains lower mode bits that the Anniversary decoder ignores"
	);
	check(
		!th03::assets::decode_pi(make_pi(
			4U,
			1U,
			color_path_stream(),
			PiHeaderOptions{.plane_count = 3U}
		)),
		"PI rejects non-four-plane images"
	);
	check(
		!th03::assets::decode_pi(make_pi(
			4U,
			1U,
			color_path_stream(),
			PiHeaderOptions{.aspect_n = 1U}
		)),
		"PI rejects the aspect mode rejected by the Anniversary decoder"
	);
	check(
		!th03::assets::decode_pi(make_pi(3U, 1U, color_path_stream())),
		"PI rejects odd widths that cannot form packed pixel pairs"
	);

	bad = valid;
	bad.pop_back();
	const auto truncated = th03::assets::decode_pi(bad);
	check(
		!truncated &&
		truncated.error().code == th03::assets::AssetErrorCode::truncated,
		"PI rejects a truncated compressed code"
	);

	BitWriter overflow_bits;
	ColorEncoder overflow_colors;
	overflow_colors.color(overflow_bits, 0U, 1U);
	overflow_colors.color(overflow_bits, 1U, 2U);
	write_run(overflow_bits, 1U, 3U);
	const auto overflow = th03::assets::decode_pi(make_pi(
		4U,
		1U,
		overflow_bits.bytes()
	));
	check(
		!overflow &&
		overflow.error().code == th03::assets::AssetErrorCode::output_overflow,
		"PI rejects a run that crosses the declared output boundary"
	);

	BitWriter unavailable_bits;
	ColorEncoder unavailable_colors;
	unavailable_colors.color(unavailable_bits, 0U, 1U);
	unavailable_colors.color(unavailable_bits, 1U, 2U);
	write_run(unavailable_bits, 3U, 1U);
	const auto unavailable = th03::assets::decode_pi(make_pi(
		2U,
		1U,
		unavailable_bits.bytes()
	));
	check(
		!unavailable &&
		unavailable.error().code == th03::assets::AssetErrorCode::malformed_stream,
		"PI rejects a reference to an unavailable predictor pixel"
	);
}

void test_cdg_single_and_planes()
{
	const auto cdg = th03::assets::decode_cdg(make_cdg(32U, 2U, 0U));
	check(cdg.has_value(), "CDG accepts a synthetic single-cel file");
	if(!cdg) {
		return;
	}
	check(
		cdg->header.stored_image_count == 0U && cdg->cels.size() == 1U,
		"CDG maps its stored zero image count to one decoded cel"
	);
	const auto &cel = cdg->cels.front();
	check(
		cel.bitplane_size == 8U && cel.pixel_width == 32U &&
		cel.pixel_height == 2U && cel.offset_at_bottom_left == 80 &&
		cel.vram_dword_width == 1U,
		"CDG retains source geometry and precomputed PC-98 VRAM metadata"
	);
	check(
		cel.row_order == th03::assets::PlanarRowOrder::bottom_to_top &&
		cel.bit_order ==
			th03::assets::PlanarBitOrder::most_significant_bit_left,
		"CDG declares source row and bit orientation"
	);
	check(
		cel.alpha.has_value() && cel.alpha->front() ==
			cdg_fixture_byte(0U, 0U, 0U) &&
		cel.alpha->back() == cdg_fixture_byte(0U, 0U, 7U),
		"CDG preserves the complete source alpha plane"
	);
	check(
		cel.color_planes[static_cast<std::size_t>(
			th03::assets::PlanarColorPlane::blue
		)].front() == cdg_fixture_byte(0U, 1U, 0U) &&
		cel.color_planes[static_cast<std::size_t>(
			th03::assets::PlanarColorPlane::intensity
		)].back() == cdg_fixture_byte(0U, 4U, 7U),
		"CDG maps the four source color planes in B/R/G/E order"
	);
}

void test_cdg_multiple_and_selection()
{
	const auto bytes = make_cdg(64U, 1U, 2U);
	const auto cdg = th03::assets::decode_cdg(bytes);
	check(
		cdg && cdg->header.stored_image_count == 2U &&
		cdg->cels.size() == 2U,
		"CDG uses the same decoder for a multi-cel CD2-style payload"
	);
	if(cdg) {
		check(
			cdg->cels[1U].alpha && cdg->cels[1U].alpha->front() ==
				cdg_fixture_byte(1U, 0U, 0U),
			"CDG advances through complete alpha+B/R/G/E cel payloads"
		);
	}

	const auto selected = th03::assets::decode_cdg_cel(bytes, 1U);
	check(
		selected && selected->alpha && selected->alpha->front() ==
			cdg_fixture_byte(1U, 0U, 0U),
		"CDG single-cel selection returns the requested cel"
	);
	const auto out_of_range = th03::assets::decode_cdg_cel(bytes, 2U);
	check(
		!out_of_range && out_of_range.error().code ==
			th03::assets::AssetErrorCode::index_out_of_range,
		"CDG single-cel selection rejects an out-of-range index"
	);

	const auto colors_only = th03::assets::decode_cdg(
		bytes,
		th03::assets::CdgAlphaPolicy::discard
	);
	check(
		cdg && colors_only && !colors_only->cels[0U].alpha.has_value() &&
		colors_only->cels[0U].color_planes[0U] == cdg->cels[0U].color_planes[0U],
		"CDG colors-only policy explicitly discards alpha without changing color"
	);
}

void test_cdg_rejections()
{
	const auto valid = make_cdg(32U, 2U, 0U);
	check(
		!th03::assets::decode_cdg(std::span(valid).first(15U)),
		"CDG rejects a truncated header"
	);

	auto bad = valid;
	set_le16(bad, 0U, 0U);
	check(!th03::assets::decode_cdg(bad), "CDG rejects a zero bitplane size");
	bad = valid;
	set_le16(bad, 2U, 16U);
	check(
		!th03::assets::decode_cdg(bad),
		"CDG rejects widths outside its 32-pixel VRAM dword grid"
	);
	bad = valid;
	set_le16(bad, 4U, 0U);
	check(!th03::assets::decode_cdg(bad), "CDG rejects a zero height");
	bad = valid;
	set_le16(bad, 0U, 7U);
	check(
		!th03::assets::decode_cdg(bad),
		"CDG rejects a bitplane size inconsistent with its dimensions"
	);
	bad = valid;
	set_le16(bad, 6U, 79U);
	check(
		!th03::assets::decode_cdg(bad),
		"CDG rejects an inconsistent bottom-left VRAM offset"
	);
	bad = valid;
	set_le16(bad, 8U, 2U);
	check(
		!th03::assets::decode_cdg(bad),
		"CDG rejects an inconsistent VRAM dword width"
	);
	bad = valid;
	bad[11U] = std::byte{1U};
	check(
		!th03::assets::decode_cdg(bad),
		"CDG rejects a plane layout not supported by TH03"
	);
	bad = valid;
	set_le16(bad, 12U, 1U);
	check(
		!th03::assets::decode_cdg(bad),
		"CDG rejects an in-file alpha segment pointer"
	);
	bad = valid;
	set_le16(bad, 14U, 1U);
	check(
		!th03::assets::decode_cdg(bad),
		"CDG rejects an in-file color segment pointer"
	);
	bad = valid;
	bad.pop_back();
	const auto truncated = th03::assets::decode_cdg(bad);
	check(
		!truncated && truncated.error().code ==
			th03::assets::AssetErrorCode::truncated,
		"CDG rejects a truncated cel payload"
	);
	bad = valid;
	bad.push_back(std::byte{0U});
	const auto trailing = th03::assets::decode_cdg(bad);
	check(
		!trailing && trailing.error().code ==
			th03::assets::AssetErrorCode::size_mismatch,
		"CDG rejects bytes after the declared payload"
	);
}

void test_mrs_geometry_and_planes()
{
	auto bytes = make_mrs();
	const auto mrs = th03::assets::decode_mrs(bytes);
	check(mrs.has_value(), "MRS accepts its fixed synthetic five-plane image");
	if(!mrs) {
		return;
	}
	const auto &cel = mrs->cel;
	check(
		cel.bitplane_size == 6624U && cel.pixel_width == 288U &&
		cel.pixel_height == 184U && cel.offset_at_bottom_left == 14640 &&
		cel.vram_dword_width == 9U,
		"MRS retains its fixed source and PC-98 VRAM geometry"
	);
	check(
		cel.row_order == th03::assets::PlanarRowOrder::bottom_to_top &&
		cel.bit_order ==
			th03::assets::PlanarBitOrder::most_significant_bit_left &&
		mrs->orientation == th03::assets::MrsOrientation::as_stored,
		"MRS declares its bottom-first, MSB-left stored orientation"
	);
	check(
		cel.alpha && *cel.alpha == expected_mrs_plane(0U, false),
		"MRS preserves the complete leading alpha plane"
	);
	bool colors_match = true;
	for(std::size_t plane = 0U; plane < cel.color_planes.size(); ++plane) {
		colors_match = colors_match &&
			(cel.color_planes[plane] == expected_mrs_plane(plane + 1U, false));
	}
	check(colors_match, "MRS maps the four source color planes in B/R/G/E order");

	bytes.assign(bytes.size(), std::byte{0U});
	check(
		cel.color_planes[0U].front() == mrs_fixture_byte(1U, 0U, 0U),
		"MRS decoded storage is independent of the caller-owned input span"
	);
}

void test_mrs_hflip()
{
	auto mrs = th03::assets::decode_mrs(make_mrs());
	check(mrs.has_value(), "MRS fixture is available for horizontal flipping");
	if(!mrs) {
		return;
	}
	th03::assets::horizontal_flip(*mrs);
	check(
		mrs->cel.alpha && mrs->orientation ==
			th03::assets::MrsOrientation::horizontally_flipped,
		"MRS persistent hflip retains and transforms alpha"
	);
	bool planes_match = mrs->cel.alpha &&
		(*mrs->cel.alpha == expected_mrs_plane(0U, true));
	for(std::size_t plane = 0U; plane < mrs->cel.color_planes.size(); ++plane) {
		planes_match = planes_match && (mrs->cel.color_planes[plane] ==
			expected_mrs_plane(plane + 1U, true));
	}
	check(
		planes_match,
		"MRS hflip reverses byte and bit order in every row of all five planes"
	);

	th03::assets::horizontal_flip(*mrs);
	planes_match = mrs->cel.alpha &&
		(*mrs->cel.alpha == expected_mrs_plane(0U, false));
	for(std::size_t plane = 0U; plane < mrs->cel.color_planes.size(); ++plane) {
		planes_match = planes_match && (mrs->cel.color_planes[plane] ==
			expected_mrs_plane(plane + 1U, false));
	}
	check(
		planes_match && mrs->orientation == th03::assets::MrsOrientation::as_stored,
		"MRS hflip is involutive and restores the stored orientation"
	);
}

void test_mrs_rejections()
{
	const auto valid = make_mrs();
	const auto truncated = th03::assets::decode_mrs(
		std::span(valid).first(valid.size() - 1U)
	);
	check(
		!truncated && truncated.error().code ==
			th03::assets::AssetErrorCode::truncated &&
		truncated.error().offset == (valid.size() - 1U),
		"MRS rejects a truncated fixed payload"
	);
	auto trailing = valid;
	trailing.push_back(std::byte{0U});
	const auto oversized = th03::assets::decode_mrs(trailing);
	check(
		!oversized && oversized.error().code ==
			th03::assets::AssetErrorCode::size_mismatch &&
		oversized.error().offset == valid.size(),
		"MRS rejects bytes after its fixed payload"
	);
}

void test_bfnt_superimpose()
{
	const std::vector<std::byte> extension{
		std::byte{0x21U}, std::byte{0xAAU}, std::byte{0xBBU},
		std::byte{0x04U}, std::byte{0x00U},
		std::byte{0x10U}, std::byte{0x12U}, std::byte{0x34U}, std::byte{0x07U},
	};
	const auto bfnt = th03::assets::decode_bfnt(
		make_bfnt(BfntFixtureOptions{
			.extended_header = extension,
			.first_extended_record_size = 5U,
		}),
		th03::assets::BfntConsumerPolicy::superimpose
	);
	check(bfnt.has_value(), "BFNT accepts a synthetic superimpose sheet");
	if(!bfnt) {
		return;
	}
	const std::array<std::byte, 8U> expected_name{
		std::byte{'S'}, std::byte{'Y'}, std::byte{'N'}, std::byte{'T'},
		std::byte{'H'}, std::byte{0U}, std::byte{1U}, std::byte{2U},
	};
	check(
		bfnt->header.color_flags == 0x83U && bfnt->header.version == 0x16U &&
		bfnt->header.reserved == 0x5AU && bfnt->header.pixel_width == 8U &&
		bfnt->header.pixel_height == 2U && bfnt->header.first_code == 5U &&
		bfnt->header.last_code == 6U && bfnt->header.font_name == expected_name &&
		bfnt->header.timestamp == 0x44332211U &&
		bfnt->header.extended_header_size == extension.size() &&
		bfnt->header.first_extended_record_size == 5U,
		"BFNT retains its complete portable 32-byte header"
	);
	check(
		bfnt->extended_header.bytes == extension &&
		bfnt->extended_header.transparent_color == 7U &&
		bfnt->transparent_color == 7U,
		"BFNT follows linked extension records and retains transparent color"
	);
	check(
		bfnt->palette &&
		bfnt->palette->colors[3U] == th03::assets::Rgb4{3U, 12U, 6U},
		"BFNT converts its B/R/G high-nibble palette to RGB4 source order"
	);
	check(
		bfnt->glyphs.size() == 2U && bfnt->glyphs[0U].code == 5U &&
		bfnt->glyphs[1U].code == 6U,
		"BFNT preserves its inclusive glyph code range"
	);
	const auto *pixels = std::get_if<th03::assets::PlanarBitmap4>(
		&bfnt->glyphs[0U].pixels
	);
	check(pixels != nullptr, "BFNT superimpose consumer returns planar glyphs");
	if(pixels) {
		check(
			pixels->row_order == th03::assets::PlanarRowOrder::top_to_bottom &&
			pixels->bit_order ==
				th03::assets::PlanarBitOrder::most_significant_bit_left,
			"BFNT planar glyphs declare source row and bit orientation"
		);
		check(
			pixels->color_planes[0U] == std::vector<std::uint8_t>{0x55U, 0x55U} &&
			pixels->color_planes[1U] == std::vector<std::uint8_t>{0x33U, 0x33U} &&
			pixels->color_planes[2U] == std::vector<std::uint8_t>{0x0FU, 0x0FU} &&
			pixels->color_planes[3U] == std::vector<std::uint8_t>{0x00U, 0xFFU},
			"BFNT transposes packed high-nibble-first pixels into B/R/G/E planes"
		);
	}
}

void test_bfnt_gaiji()
{
	const std::vector<std::byte> ignored_extension{
		std::byte{0x99U}, std::byte{0x55U},
	};
	const auto bfnt = th03::assets::decode_bfnt(
		make_bfnt(BfntFixtureOptions{
			.color_flags = 0U,
			.pixel_width = 16U,
			.pixel_height = 16U,
			.first_code = 0U,
			.last_code = 255U,
			.extended_header = ignored_extension,
			.first_extended_record_size = 0U,
		}),
		th03::assets::BfntConsumerPolicy::gaiji
	);
	check(bfnt.has_value(), "BFNT accepts the source-required gaiji layout");
	if(!bfnt) {
		return;
	}
	check(
		bfnt->consumer == th03::assets::BfntConsumerPolicy::gaiji &&
		!bfnt->palette && bfnt->extended_header.bytes == ignored_extension &&
		!bfnt->extended_header.transparent_color &&
		bfnt->glyphs.size() == 256U,
		"BFNT gaiji policy retains but does not analyze the skipped extension"
	);
	const auto *first = std::get_if<th03::assets::MonochromeBitmap1>(
		&bfnt->glyphs.front().pixels
	);
	const auto *last = std::get_if<th03::assets::MonochromeBitmap1>(
		&bfnt->glyphs.back().pixels
	);
	check(
		first && last && first->bits.size() == 32U && last->bits.size() == 32U &&
		first->bits.front() == 0U && first->bits.back() == 31U &&
		last->bits.front() == 255U && last->bits.back() == 30U &&
		bfnt->glyphs.front().code == 0U && bfnt->glyphs.back().code == 255U,
		"BFNT gaiji policy preserves 256 top-to-bottom monochrome glyphs"
	);
}

void test_bfnt_defaults_and_rejections()
{
	const auto no_extension = th03::assets::decode_bfnt(
		make_bfnt(),
		th03::assets::BfntConsumerPolicy::superimpose
	);
	check(
		no_extension && no_extension->extended_header.bytes.empty() &&
		!no_extension->extended_header.transparent_color &&
		no_extension->transparent_color == 0U,
		"BFNT defaults superimpose transparency to color zero"
	);

	const auto valid = make_bfnt();
	check(
		!th03::assets::decode_bfnt(
			std::span(valid).first(31U),
			th03::assets::BfntConsumerPolicy::superimpose
		),
		"BFNT rejects a truncated header"
	);
	auto bad = valid;
	bad[0U] = std::byte{'X'};
	check(
		!th03::assets::decode_bfnt(
			bad,
			th03::assets::BfntConsumerPolicy::superimpose
		),
		"BFNT rejects a bad identifier"
	);
	bad = valid;
	set_le16(bad, 8U, 10U);
	check(
		!th03::assets::decode_bfnt(
			bad,
			th03::assets::BfntConsumerPolicy::superimpose
		),
		"BFNT rejects a width outside its byte grid"
	);
	bad = valid;
	set_le16(bad, 10U, 0U);
	check(
		!th03::assets::decode_bfnt(
			bad,
			th03::assets::BfntConsumerPolicy::superimpose
		),
		"BFNT rejects a zero height"
	);
	bad = valid;
	set_le16(bad, 12U, 7U);
	set_le16(bad, 14U, 6U);
	check(
		!th03::assets::decode_bfnt(
			bad,
			th03::assets::BfntConsumerPolicy::superimpose
		),
		"BFNT rejects a reversed glyph range"
	);
	bad = valid;
	bad[5U] = std::byte{0U};
	check(
		!th03::assets::decode_bfnt(
			bad,
			th03::assets::BfntConsumerPolicy::superimpose
		),
		"BFNT superimpose policy rejects monochrome data"
	);
	bad = make_bfnt(BfntFixtureOptions{
		.color_flags = 0U,
		.pixel_width = 16U,
		.pixel_height = 16U,
		.first_code = 0U,
		.last_code = 255U,
		.extended_header = {},
		.first_extended_record_size = 0U,
	});
	bad[5U] = std::byte{3U};
	check(
		!th03::assets::decode_bfnt(
			bad,
			th03::assets::BfntConsumerPolicy::gaiji
		),
		"BFNT gaiji policy rejects color data"
	);
	bad = valid;
	set_le16(bad, 12U, 0U);
	set_le16(bad, 14U, 512U);
	check(
		!th03::assets::decode_bfnt(
			bad,
			th03::assets::BfntConsumerPolicy::superimpose
		),
		"BFNT superimpose policy enforces master.lib's pattern capacity"
	);

	auto truncated_extension = valid;
	truncated_extension.resize(35U);
	set_le16(truncated_extension, 28U, 4U);
	set_le16(truncated_extension, 30U, 4U);
	truncated_extension[32U] = std::byte{0x10U};
	check(
		!th03::assets::decode_bfnt(
			truncated_extension,
			th03::assets::BfntConsumerPolicy::superimpose
		),
		"BFNT rejects a truncated extended header"
	);
	const auto short_record = make_bfnt(BfntFixtureOptions{
		.extended_header = {std::byte{0x20U}, std::byte{0U}},
		.first_extended_record_size = 2U,
	});
	check(
		!th03::assets::decode_bfnt(
			short_record,
			th03::assets::BfntConsumerPolicy::superimpose
		),
		"BFNT rejects an undersized linked extension record"
	);
	const auto oversized_record = make_bfnt(BfntFixtureOptions{
		.extended_header = {
			std::byte{0x20U}, std::byte{0U}, std::byte{0U},
		},
		.first_extended_record_size = 4U,
	});
	check(
		!th03::assets::decode_bfnt(
			oversized_record,
			th03::assets::BfntConsumerPolicy::superimpose
		),
		"BFNT rejects an extension record past the declared region"
	);
	const auto truncated_clear = make_bfnt(BfntFixtureOptions{
		.extended_header = {
			std::byte{0x10U}, std::byte{0U}, std::byte{0U},
		},
		.first_extended_record_size = 3U,
	});
	check(
		!th03::assets::decode_bfnt(
			truncated_clear,
			th03::assets::BfntConsumerPolicy::superimpose
		),
		"BFNT rejects a truncated transparent-color record"
	);
	const auto overlapping_clear = make_bfnt(BfntFixtureOptions{
		.extended_header = {
			std::byte{0x10U}, std::byte{0U}, std::byte{0U},
			std::byte{0x0DU}, std::byte{0U},
		},
		.first_extended_record_size = 3U,
	});
	const auto overlapping_result = th03::assets::decode_bfnt(
		overlapping_clear,
		th03::assets::BfntConsumerPolicy::superimpose
	);
	check(
		!overlapping_result && overlapping_result.error().code ==
			th03::assets::AssetErrorCode::malformed_stream,
		"BFNT does not read transparent color across an undersized record boundary"
	);

	auto truncated_palette = valid;
	truncated_palette.resize(32U + 47U);
	check(
		!th03::assets::decode_bfnt(
			truncated_palette,
			th03::assets::BfntConsumerPolicy::superimpose
		),
		"BFNT rejects a truncated inline palette"
	);
	bad = valid;
	bad.pop_back();
	const auto truncated_payload = th03::assets::decode_bfnt(
		bad,
		th03::assets::BfntConsumerPolicy::superimpose
	);
	check(
		!truncated_payload && truncated_payload.error().code ==
			th03::assets::AssetErrorCode::truncated,
		"BFNT rejects a truncated glyph payload"
	);
	bad = valid;
	bad.push_back(std::byte{0U});
	const auto trailing = th03::assets::decode_bfnt(
		bad,
		th03::assets::BfntConsumerPolicy::superimpose
	);
	check(
		!trailing && trailing.error().code ==
			th03::assets::AssetErrorCode::size_mismatch,
		"BFNT rejects bytes after the declared glyph payload"
	);
}

} // namespace

int main()
{
	test_rgb48();
	test_pi_color_and_header();
	test_pi_position_paths();
	test_pi_rejections();
	test_cdg_single_and_planes();
	test_cdg_multiple_and_selection();
	test_cdg_rejections();
	test_mrs_geometry_and_planes();
	test_mrs_hflip();
	test_mrs_rejections();
	test_bfnt_superimpose();
	test_bfnt_gaiji();
	test_bfnt_defaults_and_rejections();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return (failures == 0) ? 0 : 1;
}
