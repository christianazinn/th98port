#include "th03/app/runtime/host_surface.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <vector>

namespace {

using th03::app::HostRgbaByteOrder;
using th03::app::HostRgbaTarget;
using th03::app::HostSurfaceErrorCode;
using th03::app::IntegerLetterboxRect;
using th03::present::ComposedPixel;
using th03::present::ComposedPixelLayer;

constexpr std::size_t source_width = th03::present::graphics_width;
constexpr std::size_t source_height = th03::present::graphics_height;
constexpr std::size_t source_pixels = th03::present::graphics_page_pixels;
constexpr std::size_t row_bytes = source_width * 4U;
constexpr std::byte untouched{0xA5U};

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

struct SurfaceFixture {
	std::vector<ComposedPixel> pixels = std::vector<ComposedPixel>(
		source_pixels,
		ComposedPixel{ComposedPixelLayer::graphics, 0U}
	);
	th03::assets::Palette16Rgb4 palette{};

	SurfaceFixture()
	{
		palette.colors[0U] = {1U, 2U, 15U};
	}

	[[nodiscard]] th03::present::SourceSurface source() const
	{
		return {
			std::span<const ComposedPixel, source_pixels>{
				pixels.data(), pixels.size()
			},
			palette,
		};
	}
};

[[nodiscard]] std::array<std::byte, 4U> bytes(
	std::uint8_t first,
	std::uint8_t second,
	std::uint8_t third,
	std::uint8_t fourth
)
{
	return {
		static_cast<std::byte>(first),
		static_cast<std::byte>(second),
		static_cast<std::byte>(third),
		static_cast<std::byte>(fourth),
	};
}

[[nodiscard]] bool equals_at(
	std::span<const std::byte> output,
	std::size_t offset,
	const std::array<std::byte, 4U> &expected
)
{
	return std::equal(
		expected.begin(), expected.end(), output.begin() +
			static_cast<std::ptrdiff_t>(offset)
	);
}

void expect_atomic_error(
	const th03::present::SourceSurface &source,
	HostRgbaTarget target,
	HostSurfaceErrorCode expected_code,
	std::size_t expected_pixel,
	const std::vector<std::byte> &before,
	const char *message
)
{
	const auto result = th03::app::source_surface_to_host_rgba(source, target);
	expect(
		!result && result.error().code == expected_code &&
			result.error().pixel_offset == expected_pixel &&
			target.bytes.size() == before.size() &&
			std::equal(target.bytes.begin(), target.bytes.end(), before.begin()),
		message
	);
}

void test_color_domains_and_pitch()
{
	SurfaceFixture fixture;
	for(std::uint8_t value = 0U; value <= 7U; ++value) {
		fixture.pixels[value] = {ComposedPixelLayer::text, value};
	}
	fixture.pixels[source_width] = {ComposedPixelLayer::text, 7U};

	constexpr std::size_t pitch = row_bytes + 7U;
	constexpr std::size_t required = ((source_height - 1U) * pitch) + row_bytes;
	std::vector<std::byte> output(required + 5U, untouched);
	const auto result = th03::app::source_surface_to_host_rgba(
		fixture.source(), {output, pitch, HostRgbaByteOrder::rgba}
	);
	expect(result.has_value(), "valid fixed text colors convert into RGBA");

	for(std::uint8_t value = 0U; value <= 7U; ++value) {
		const auto expected = bytes(
			((value & 0x02U) != 0U) ? 0xFFU : 0U,
			((value & 0x04U) != 0U) ? 0xFFU : 0U,
			((value & 0x01U) != 0U) ? 0xFFU : 0U,
			0xFFU
		);
		expect(
			equals_at(output, static_cast<std::size_t>(value) * 4U, expected),
			"text B=1/R=2/G=4 bits map to fixed full-intensity RGBA"
		);
	}
	expect(
		equals_at(output, pitch, bytes(0xFFU, 0xFFU, 0xFFU, 0xFFU)),
		"destination rows begin at the caller-provided byte pitch"
	);
	expect(
		std::all_of(
			output.begin() + static_cast<std::ptrdiff_t>(row_bytes),
			output.begin() + static_cast<std::ptrdiff_t>(pitch),
			[](std::byte value) { return value == untouched; }
		),
		"conversion leaves row padding untouched"
	);
	expect(
		std::all_of(
			output.end() - 5, output.end(),
			[](std::byte value) { return value == untouched; }
		),
		"conversion leaves bytes after the final written row untouched"
	);
}

void test_graphics_palette_and_byte_orders()
{
	SurfaceFixture fixture;
	fixture.palette.colors[0U] = {1U, 2U, 15U};
	struct OrderCase {
		HostRgbaByteOrder order;
		std::array<std::byte, 4U> expected;
	};
	const std::array cases{
		OrderCase{HostRgbaByteOrder::rgba, bytes(17U, 34U, 255U, 255U)},
		OrderCase{HostRgbaByteOrder::bgra, bytes(255U, 34U, 17U, 255U)},
		OrderCase{HostRgbaByteOrder::argb, bytes(255U, 17U, 34U, 255U)},
		OrderCase{HostRgbaByteOrder::abgr, bytes(255U, 255U, 34U, 17U)},
	};
	std::vector<std::byte> output(source_height * row_bytes, untouched);
	for(const auto &test : cases) {
		std::fill(output.begin(), output.end(), untouched);
		const auto result = th03::app::source_surface_to_host_rgba(
			fixture.source(), {output, row_bytes, test.order}
		);
		expect(
			result.has_value() && equals_at(output, 0U, test.expected),
			"graphics RGB4 expands by 17 in the selected RGBA byte order"
		);
	}
}

void test_all_graphics_palette_indices()
{
	SurfaceFixture fixture;
	for(std::size_t index = 0U; index < fixture.palette.colors.size(); ++index) {
		const auto value = static_cast<std::uint8_t>(index);
		fixture.pixels[index] = {ComposedPixelLayer::graphics, value};
		fixture.palette.colors[index] = {
			value,
			static_cast<std::uint8_t>((index + 5U) & 0x0FU),
			static_cast<std::uint8_t>((index + 10U) & 0x0FU),
		};
	}

	std::vector<std::byte> output(source_height * row_bytes, untouched);
	const auto result = th03::app::source_surface_to_host_rgba(
		fixture.source(), {output, row_bytes, HostRgbaByteOrder::rgba}
	);
	expect(result.has_value(), "all 16 legal graphics palette indices convert");
	for(std::size_t index = 0U; index < fixture.palette.colors.size(); ++index) {
		const auto color = fixture.palette.colors[index];
		expect(
			equals_at(
				output,
				index * 4U,
				bytes(
					static_cast<std::uint8_t>(color.r * 17U),
					static_cast<std::uint8_t>(color.g * 17U),
					static_cast<std::uint8_t>(color.b * 17U),
					0xFFU
				)
			),
			"each graphics-domain value selects and expands its own RGB4 entry"
		);
	}
}

void test_invalid_source_atomicity()
{
	constexpr auto no_pixel = std::numeric_limits<std::size_t>::max();
	constexpr std::size_t late_pixel = source_pixels - 1U;
	std::vector<std::byte> output(source_height * row_bytes, untouched);
	const auto before = output;

	SurfaceFixture bad_graphics;
	bad_graphics.pixels[late_pixel].value = 16U;
	expect_atomic_error(
		bad_graphics.source(), {output, row_bytes, HostRgbaByteOrder::rgba},
		HostSurfaceErrorCode::invalid_composed_pixel, late_pixel, before,
		"a malformed late graphics index leaves the entire target unchanged"
	);

	SurfaceFixture bad_text;
	bad_text.pixels[late_pixel] = {ComposedPixelLayer::text, 8U};
	expect_atomic_error(
		bad_text.source(), {output, row_bytes, HostRgbaByteOrder::rgba},
		HostSurfaceErrorCode::invalid_composed_pixel, late_pixel, before,
		"a malformed late text value leaves the entire target unchanged"
	);

	SurfaceFixture bad_layer;
	bad_layer.pixels[late_pixel] = {
		static_cast<ComposedPixelLayer>(0xFFU), 0U
	};
	expect_atomic_error(
		bad_layer.source(), {output, row_bytes, HostRgbaByteOrder::rgba},
		HostSurfaceErrorCode::invalid_composed_pixel, late_pixel, before,
		"an unknown late composed-pixel layer is rejected atomically"
	);

	SurfaceFixture bad_palette;
	bad_palette.palette.colors[15U].g = 16U;
	expect_atomic_error(
		bad_palette.source(), {output, row_bytes, HostRgbaByteOrder::rgba},
		HostSurfaceErrorCode::invalid_graphics_palette, no_pixel, before,
		"an invalid unused RGB4 palette entry is rejected before conversion"
	);
}

void test_invalid_target_atomicity()
{
	constexpr auto no_pixel = std::numeric_limits<std::size_t>::max();
	SurfaceFixture fixture;
	std::vector<std::byte> output(source_height * row_bytes, untouched);
	const auto before = output;

	expect_atomic_error(
		fixture.source(), {
			output,
			row_bytes,
			static_cast<HostRgbaByteOrder>(0xFFU),
		},
		HostSurfaceErrorCode::invalid_byte_order, no_pixel, before,
		"an invalid public byte-order enum is rejected before the first write"
	);
	expect_atomic_error(
		fixture.source(), {output, row_bytes - 1U, HostRgbaByteOrder::rgba},
		HostSurfaceErrorCode::pitch_too_small, no_pixel, before,
		"a pitch shorter than one RGBA row is rejected atomically"
	);

	std::vector<std::byte> short_output(output.size() - 1U, untouched);
	const auto short_before = short_output;
	expect_atomic_error(
		fixture.source(), {
			short_output, row_bytes, HostRgbaByteOrder::rgba
		},
		HostSurfaceErrorCode::output_too_small, no_pixel, short_before,
		"an output span one byte short is rejected atomically"
	);

	std::vector<std::byte> tiny_output(1U, untouched);
	const auto tiny_before = tiny_output;
	expect_atomic_error(
		fixture.source(), {
			tiny_output,
			std::numeric_limits<std::size_t>::max(),
			HostRgbaByteOrder::rgba,
		},
		HostSurfaceErrorCode::size_overflow, no_pixel, tiny_before,
		"overflow in the pitched destination extent is rejected atomically"
	);
}

void test_integer_letterbox()
{
	using th03::app::integer_letterbox_8x5;
	expect(
		integer_letterbox_8x5(640U, 400U) ==
			IntegerLetterboxRect{0U, 0U, 640U, 400U, 1U},
		"native drawable dimensions select a centered scale of one"
	);
	expect(
		integer_letterbox_8x5(1920U, 1080U) ==
			IntegerLetterboxRect{320U, 140U, 1280U, 800U, 2U},
		"wide 16:9 drawable receives centered integer 8:5 letterboxing"
	);
	expect(
		integer_letterbox_8x5(800U, 600U) ==
			IntegerLetterboxRect{80U, 100U, 640U, 400U, 1U},
		"tall drawable receives centered integer 8:5 letterboxing"
	);
	expect(
		integer_letterbox_8x5(642U, 401U) ==
			IntegerLetterboxRect{1U, 0U, 640U, 400U, 1U},
		"odd leftover dimensions center with deterministic integer truncation"
	);
	expect(
		!integer_letterbox_8x5(639U, 400U) &&
			!integer_letterbox_8x5(640U, 399U) &&
			!integer_letterbox_8x5(0U, 0U),
		"drawables smaller than one complete 640x400 surface have no rectangle"
	);

	constexpr auto maximum = std::numeric_limits<std::size_t>::max();
	constexpr auto maximum_scale = maximum / source_width;
	constexpr auto maximum_width = maximum_scale * source_width;
	constexpr auto maximum_height = maximum_scale * source_height;
	const auto huge = integer_letterbox_8x5(maximum, maximum);
	expect(
		huge == IntegerLetterboxRect{
			(maximum - maximum_width) / 2U,
			(maximum - maximum_height) / 2U,
			maximum_width,
			maximum_height,
			maximum_scale,
		},
		"SIZE_MAX drawable dimensions produce the largest fitting rectangle without overflow"
	);
}

} // namespace

int main()
{
	test_color_domains_and_pitch();
	test_graphics_palette_and_byte_orders();
	test_all_graphics_palette_indices();
	test_invalid_source_atomicity();
	test_invalid_target_atomicity();
	test_integer_letterbox();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return (failures == 0U) ? 0 : 1;
}
