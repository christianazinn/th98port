#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <span>
#include <string>
#include <utility>
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

} // namespace

int main()
{
	test_rgb48();
	test_pi_color_and_header();
	test_pi_position_paths();
	test_pi_rejections();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return (failures == 0) ? 0 : 1;
}
