#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/beta_features.hpp"

namespace {

int failures = 0;

void expect(bool condition, std::string_view message)
{
	if(!condition) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}

void put_u16(std::vector<std::byte> &bytes, std::size_t at, std::uint16_t value)
{
	bytes[at] = static_cast<std::byte>(value & 0xFFU);
	bytes[at + 1U] = static_cast<std::byte>(value >> 8U);
}

void put_u32(std::vector<std::byte> &bytes, std::size_t at, std::uint32_t value)
{
	bytes[at] = static_cast<std::byte>(value & 0xFFU);
	bytes[at + 1U] = static_cast<std::byte>((value >> 8U) & 0xFFU);
	bytes[at + 2U] = static_cast<std::byte>((value >> 16U) & 0xFFU);
	bytes[at + 3U] = static_cast<std::byte>(value >> 24U);
}

std::vector<std::byte> menu_background_half(
	std::uint8_t first_row,
	std::uint8_t second_row,
	std::uint8_t remaining
)
{
	constexpr std::size_t pixel_offset = 118U;
	constexpr std::size_t row_bytes = 320U;
	constexpr std::size_t height = 200U;
	std::vector<std::byte> bytes(pixel_offset + (row_bytes * height));
	bytes[0U] = std::byte{'B'};
	bytes[1U] = std::byte{'M'};
	put_u32(bytes, 2U, static_cast<std::uint32_t>(bytes.size()));
	put_u32(bytes, 10U, pixel_offset);
	put_u32(bytes, 14U, 40U);
	put_u32(bytes, 18U, 640U);
	put_u32(bytes, 22U, height);
	put_u16(bytes, 26U, 1U);
	put_u16(bytes, 28U, 4U);
	put_u32(bytes, 34U, row_bytes * height);
	put_u32(bytes, 38U, 4'000U);
	put_u32(bytes, 42U, 4'000U);
	put_u32(bytes, 46U, 16U);
	put_u32(bytes, 50U, 16U);
	for(std::size_t index = 0U; index < 16U; ++index) {
		const auto at = 54U + (index * 4U);
		bytes[at] = static_cast<std::byte>(index << 4U);
		bytes[at + 1U] = static_cast<std::byte>((15U - index) << 4U);
		bytes[at + 2U] = static_cast<std::byte>((index ^ 5U) << 4U);
	}
	const auto fill_row = [&](std::size_t logical_y, std::uint8_t color) {
		const auto packed = static_cast<std::byte>((color << 4U) | color);
		const auto stored_y = height - logical_y - 1U;
		std::fill_n(
			bytes.begin() + static_cast<std::ptrdiff_t>(
				pixel_offset + (stored_y * row_bytes)
			),
			row_bytes,
			packed
		);
	};
	for(std::size_t y = 0U; y < height; ++y) {
		fill_row(y, y == 0U ? first_row : (y == 1U ? second_row : remaining));
	}
	return bytes;
}

std::vector<std::byte> font_fixture()
{
	std::vector<std::byte> bytes(3'168U, std::byte{0U});
	constexpr std::array magic{'T', '3', 'P', 'F', 'N', 'T', '1'};
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		bytes[index] = static_cast<std::byte>(magic[index]);
	}
	put_u16(bytes, 8U, 3'168U);
	put_u16(bytes, 10U, 32U);
	put_u16(bytes, 12U, 32U);
	put_u16(bytes, 14U, 128U);
	put_u16(bytes, 16U, 95U);
	bytes[18U] = std::byte{0x20U};
	bytes[19U] = std::byte{0x7EU};
	bytes[20U] = std::byte{16U};
	bytes[21U] = std::byte{16U};
	bytes[22U] = std::byte{0x3FU};
	for(std::size_t glyph = 0U; glyph < 95U; ++glyph) {
		bytes[32U + glyph] = std::byte{8U};
	}
	const auto a = 128U + (static_cast<std::size_t>('A' - 0x20) * 32U);
	bytes[a] = std::byte{0x80U};
	bytes[a + 2U] = std::byte{0x40U};
	const auto v = 128U + (static_cast<std::size_t>('v' - 0x20) * 32U);
	bytes[v + 4U] = std::byte{0x40U};
	std::uint16_t checksum = 0U;
	for(std::size_t index = 32U; index < bytes.size(); ++index) {
		checksum = static_cast<std::uint16_t>(
			checksum + std::to_integer<std::uint8_t>(bytes[index])
		);
	}
	put_u16(bytes, 24U, checksum);
	return bytes;
}

void test_font_decode_and_raster()
{
	auto bytes = font_fixture();
	auto decoded = th03::app::detail::decode_new_classic_menu_font(bytes);
	expect(decoded.has_value(), "source-shaped proportional font decodes");
	if(!decoded) {
		return;
	}
	expect(decoded->width("A A") == 24, "font advances are authoritative");
	expect(!decoded->uses_machine_glyphs("Language") &&
		decoded->uses_machine_glyphs("\x83\x89\x83\x93\x83\x4F\x83\x47\x81\x5B\x83\x57"),
		"valid Shift-JIS text alone selects the exact machine-glyph path");

	th03::present::Raster raster{};
	expect(raster.set_access_page(th03::present::Page::one),
		"restore source page selects");
	expect(raster.clear({0, 0, 640U, 400U}, 3U),
		"restore source page initializes");
	expect(raster.set_access_page(th03::present::Page::zero),
		"font target page selects");
	expect(raster.clear({0, 0, 640U, 400U}, 2U),
		"font target page initializes");
	expect(decoded->put(raster, 10, 20, "A", 12U),
		"proportional glyph draws");
	expect(raster.pixel(th03::present::Page::zero, {10, 20}) == 12U &&
		raster.pixel(th03::present::Page::zero, {11, 20}) == 2U &&
		raster.pixel(th03::present::Page::zero, {11, 21}) == 12U,
		"glyph set bits replace and zero bits preserve the background");
	expect(decoded->restore_rect(raster, 10, 20, 1U, 2U),
		"font background restore succeeds");
	expect(raster.pixel(th03::present::Page::zero, {10, 20}) == 3U &&
		raster.pixel(th03::present::Page::zero, {0, 20}) == 3U,
		"restore follows source sixteen-pixel EGC granularity");
	expect(decoded->put_vertical_flipped(raster, 20, 30, 'v', 12U),
		"vertical mirror draws a source-font navigation glyph");
	expect(raster.pixel(th03::present::Page::zero, {21, 43}) == 12U &&
		raster.pixel(th03::present::Page::zero, {21, 32}) == 2U,
		"up caret is the exact lowercase v bitmap mirrored by rows");

	bytes[24U] ^= std::byte{1U};
	auto corrupt = th03::app::detail::decode_new_classic_menu_font(bytes);
	expect(!corrupt && corrupt.error().code ==
		th03::app::NewClassicAssetsErrorCode::font_checksum,
		"font checksum mismatch rejects before rendering");
}

void test_menu_background_decode()
{
	auto top = menu_background_half(12U, 15U, 5U);
	auto bottom = menu_background_half(7U, 8U, 9U);
	auto decoded = th03::app::detail::decode_new_classic_menu_background(
		top, bottom
	);
	expect(decoded.has_value(), "split 4-bpp menu BMP decodes");
	if(!decoded) {
		return;
	}
	expect(decoded->image.width == 640U && decoded->image.height == 400U &&
		decoded->image.indices.size() == 256'000U,
		"split menu BMP joins to one exact full-screen image");
	expect(decoded->image.indices[0U] == 10U &&
		decoded->image.indices[640U] == 0U &&
		decoded->image.indices[1'280U] == 5U &&
		decoded->image.indices[128'000U] == 7U &&
		decoded->image.indices[128'640U] == 8U,
		"BMP rows are top-down after joining and overlay indices are remapped");
	expect(std::ranges::find(decoded->image.indices, 12U) ==
		decoded->image.indices.end() &&
		std::ranges::find(decoded->image.indices, 15U) ==
		decoded->image.indices.end() && decoded->palette &&
		decoded->palette->colors[8U] == th03::assets::Rgb4{12U, 3U, 14U} &&
		decoded->palette->colors[9U] == th03::assets::Rgb4{12U, 10U, 15U} &&
		decoded->palette->colors[11U] == th03::assets::Rgb4{15U, 10U, 15U} &&
		decoded->palette->colors[12U] == th03::assets::Rgb4{15U, 15U, 0U} &&
		decoded->palette->colors[13U] == th03::assets::Rgb4{15U, 12U, 15U} &&
		decoded->palette->colors[15U] == th03::assets::Rgb4{15U, 15U, 15U},
		"background preserves every fixed retained-menu text palette slot");

	top[0U] = std::byte{'X'};
	const auto malformed =
		th03::app::detail::decode_new_classic_menu_background(top, bottom);
	expect(!malformed && malformed.error().code ==
		th03::app::NewClassicAssetsErrorCode::menu_background_shape,
		"malformed split BMP is rejected before pixel access");
}

void test_authentic_archive_if_supplied()
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
	const auto *path = std::getenv("TH03_AZINN_DAT");
	const auto *menu_root = std::getenv("TH03_PODDUI_DIR");
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
	if(!path || *path == '\0') {
		return;
	}
	if(menu_root && *menu_root != '\0') {
		const auto unicode_path = std::filesystem::path(menu_root) /
			"T3UFN001.BIN";
		std::ifstream input(unicode_path, std::ios::binary);
		std::vector<std::byte> bytes(static_cast<std::size_t>(
			std::filesystem::file_size(unicode_path)
		));
		input.read(reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size()));
		auto unicode = th03::app::decode_new_classic_unicode_font(bytes);
		expect(unicode.has_value(),
			"repo-owned Unicode font package passes its runtime parser");
		if(!unicode) {
			std::cerr << "unicode error code=" <<
				static_cast<int>(unicode.error().code) << " offset=" <<
				unicode.error().offset << '\n';
		}
	}
	auto loaded = th03::app::load_new_classic_replay_patch_assets(
		std::filesystem::path(path),
		menu_root && *menu_root != '\0' ? std::filesystem::path(menu_root) :
			std::filesystem::path{}
	);
	expect(loaded.has_value(), "authenticated Replay Patch AZINN profile loads");
	if(!loaded) {
		std::cerr << "asset error code=" << static_cast<int>(loaded.error().code)
			<< " offset=" << loaded.error().offset << '\n';
		return;
	}
	const auto &neutral = loaded->menu_background(
		th03::app::NewClassicMenuBackground::neutral
	);
	expect(loaded->replay_list_background.image.width == 640U &&
		loaded->replay_name_background.image.height == 400U &&
		loaded->key_config_background.image.width == 640U &&
		loaded->key_config_background.image.height == 400U &&
		neutral.image.indices.size() == 256'000U,
		"used source, BG1 KeyConfig, and neutral full-screen backgrounds decode");
	expect(std::ranges::find(
		neutral.image.indices, 12U
	) == neutral.image.indices.end() &&
		std::ranges::find(
			neutral.image.indices, 15U
		) == neutral.image.indices.end() && neutral.palette &&
		neutral.palette->colors[9U] ==
			th03::assets::Rgb4{12U, 10U, 15U} &&
		neutral.palette->colors[12U] ==
			th03::assets::Rgb4{15U, 15U, 0U} &&
		neutral.palette->colors[15U] ==
			th03::assets::Rgb4{15U, 15U, 15U},
		"neutral background retains lavender, yellow, and white menu colors");
	constexpr std::array<std::uint8_t, 6U> reserved{
		8U, 9U, 11U, 12U, 13U, 15U
	};
	for(std::uint8_t value = static_cast<std::uint8_t>(
		th03::app::NewClassicMenuBackground::replay
	); value < static_cast<std::uint8_t>(
		th03::app::NewClassicMenuBackground::count
	); ++value) {
		const auto &background = loaded->menu_background(
			static_cast<th03::app::NewClassicMenuBackground>(value)
		);
		expect(background.image.indices.size() == 256'000U &&
			background.palette && std::ranges::all_of(reserved,
				[&background](std::uint8_t index) {
					return std::ranges::find(background.image.indices, index) ==
						background.image.indices.end();
				}) && background.palette->colors[9U] ==
				th03::assets::Rgb4{12U, 10U, 15U},
			"sample background excludes all reserved pixels and keeps lavender");
	}
	expect(loaded->menu_font.width("The PoDD Arrange Project") > 0,
		"authentic proportional title credit has measurable ink");
	if(menu_root && *menu_root != '\0') {
		expect(loaded->menu_font.supports_utf8(
			"Jos\xC3\xA9 \xE5\xB9\xBB\xE6\x83\xB3"
		), "installed fallback font covers Latin and Japanese UTF-8");
		expect(!loaded->menu_font.supports_utf8("\xE2\x80\xAEspoof"),
			"installed fallback font rejects unsupported bidi controls");
		if constexpr(th03::app::new_classic_beta_features_enabled) {
			expect(loaded->custom_character_select_assets &&
				loaded->custom_character_select_assets->locale() == "es-TEST",
				"beta Character Select package loads with PODDUI");
		} else {
			expect(!loaded->custom_character_select_assets,
				"default assets do not load the experimental Character Select package");
		}
	}
}

} // namespace

int main()
{
	test_font_decode_and_raster();
	test_menu_background_decode();
	test_authentic_archive_if_supplied();
	if(failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return 1;
	}
	std::cout << "New Classic Replay Patch asset tests passed\n";
	return 0;
}
