#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "th03/app/new_classic_assets.hpp"

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

	bytes[24U] ^= std::byte{1U};
	auto corrupt = th03::app::detail::decode_new_classic_menu_font(bytes);
	expect(!corrupt && corrupt.error().code ==
		th03::app::NewClassicAssetsErrorCode::font_checksum,
		"font checksum mismatch rejects before rendering");
}

void test_authentic_archive_if_supplied()
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
	const auto *path = std::getenv("TH03_AZINN_DAT");
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
	if(!path || *path == '\0') {
		return;
	}
	auto loaded = th03::app::load_new_classic_replay_patch_assets(
		std::filesystem::path(path)
	);
	expect(loaded.has_value(), "authenticated Replay Patch AZINN profile loads");
	if(!loaded) {
		std::cerr << "asset error code=" << static_cast<int>(loaded.error().code)
			<< " offset=" << loaded.error().offset << '\n';
		return;
	}
	expect(loaded->replay_list_background.image.width == 640U &&
		loaded->replay_name_background.image.height == 400U &&
		loaded->new_menu_background.image.indices.size() == 256'000U &&
		loaded->key_config_background.image.indices.size() == 256'000U,
		"all four full-screen PI members decode");
	expect(loaded->menu_font.width("The PoDD Arrange Project") > 0,
		"authentic proportional title credit has measurable ink");
}

} // namespace

int main()
{
	test_font_decode_and_raster();
	test_authentic_archive_if_supplied();
	if(failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return 1;
	}
	std::cout << "New Classic Replay Patch asset tests passed\n";
	return 0;
}
