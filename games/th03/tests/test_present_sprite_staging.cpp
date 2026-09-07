#include "th03/present/sprite_staging.hpp"

#include "th03/present/main_static.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using th03::assets::BfntConsumerPolicy;
using th03::assets::BfntSheet;
using th03::assets::PlanarBitmap4;
using th03::assets::PiImage;
using th03::present::BitSource;
using th03::present::MainPlayerShotSheets;
using th03::present::MainSprite16PiAssets;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::Sprite16Staging;
using th03::present::sprite16_source_height;
using th03::present::sprite16_source_plane_size;
using th03::present::sprite16_source_stride;
using th03::present::sprite16_source_top;
using th03::present::sprite16_source_width;

int failures = 0;

void expect(bool condition, std::string_view message)
{
	if(!condition) {
		std::fprintf(
			stderr,
			"FAIL: %.*s\n",
			static_cast<int>(message.size()),
			message.data()
		);
		++failures;
	}
}

void expect_pixel(
	const Raster &raster,
	Page page,
	Point point,
	std::uint8_t expected,
	std::string_view message
)
{
	const auto actual = raster.pixel(page, point);
	expect(actual.has_value() && *actual == expected, message);
}

[[nodiscard]] bool mask_bit(BitSource source, std::size_t x, std::size_t y)
{
	const auto byte = source.bits[(y * source.stride) + (x / 8U)];
	return ((byte >> (7U - (x & 7U))) & 1U) != 0U;
}

BfntSheet make_character_sheet(std::uint8_t color_base)
{
	BfntSheet sheet{};
	sheet.header.pixel_width = 32U;
	sheet.header.pixel_height = 32U;
	sheet.header.first_code = 0U;
	sheet.header.last_code = 8U;
	sheet.consumer = BfntConsumerPolicy::superimpose;
	sheet.transparent_color = 0U;
	constexpr std::size_t plane_size = (32U / 8U) * 32U;
	for(std::uint16_t code = 0U; code < 9U; ++code) {
		PlanarBitmap4 pixels{};
		for(auto &plane : pixels.color_planes) {
			plane.assign(plane_size, 0U);
		}
		if(code == 0U || code == 3U || code == 6U) {
			const auto color = static_cast<std::uint8_t>(
				color_base + static_cast<std::uint8_t>(code / 3U)
			);
			for(std::size_t plane = 0U; plane < pixels.color_planes.size(); ++plane) {
				if((color & static_cast<std::uint8_t>(1U << plane)) != 0U) {
					pixels.color_planes[plane][0U] = 0x80U;
				}
			}
		}
		sheet.glyphs.push_back({code, std::move(pixels)});
	}
	return sheet;
}

PiImage make_pi(
	std::uint16_t width,
	std::uint16_t height,
	std::uint8_t even_color,
	std::uint8_t odd_color
)
{
	PiImage image{};
	image.image.width = width;
	image.image.height = height;
	image.image.indices.resize(static_cast<std::size_t>(width) * height);
	for(std::size_t y = 0U; y < height; ++y) {
		const auto color = ((y & 1U) == 0U) ? even_color : odd_color;
		const auto begin = y * width;
		for(std::size_t x = 0U; x < width; ++x) {
			image.image.indices[begin + x] = color;
		}
	}
	return image;
}

MainSprite16PiAssets pi_assets(
	PiImage &p1_shot,
	PiImage &p2_shot,
	PiImage &p1_extra,
	PiImage &p2_extra,
	PiImage &enemy_center,
	PiImage &opponent_enemies
)
{
	return {
		.player_shots = {&p1_shot, &p2_shot},
		.player_extra_shots = {&p1_extra, &p2_extra},
		.enemy_center = &enemy_center,
		.opponent_enemies = &opponent_enemies,
	};
}

void test_main_pi_source_sheet_placement()
{
	auto p1_shot = make_pi(320U, 16U, 1U, 11U);
	auto p2_shot = make_pi(320U, 16U, 2U, 12U);
	auto p1_extra = make_pi(320U, 128U, 3U, 13U);
	auto p2_extra = make_pi(320U, 144U, 4U, 14U);
	auto enemy_center = make_pi(640U, 48U, 5U, 15U);
	auto opponent_enemies = make_pi(640U, 192U, 6U, 10U);

	// Distinct P2 endpoints make an accidental horizontal flip observable.
	p2_shot.image.indices[0U] = 7U;
	p2_shot.image.indices[319U] = 8U;

	Raster raster;
	expect(raster.set_access_page(Page::zero), "select untouched PI page zero");
	expect(raster.plot({0, 200}, 9U), "seed untouched PI page zero");
	expect(raster.set_access_page(Page::one), "select stale PI source page");
	expect(
		raster.clear({0, 0, 640U, 400U}, 9U),
		"seed stale complete PI staging page"
	);
	expect(raster.set_access_page(Page::zero), "restore pre-handoff access page");

	expect(
		th03::present::main_prepare_sprite16_pi_source_sheet(
			raster,
			pi_assets(
				p1_shot,
				p2_shot,
				p1_extra,
				p2_extra,
				enemy_center,
				opponent_enemies
			)
		),
		"prepare exact MAINL Sprite16 PI handoff"
	);
	expect(raster.access_page() == Page::one, "PI handoff leaves page one accessed");
	expect_pixel(raster, Page::zero, {0, 200}, 9U, "PI handoff preserves page zero");
	expect_pixel(raster, Page::one, {0, 0}, 0U, "PI handoff clears page-one upper half");
	expect_pixel(raster, Page::one, {639, 199}, 0U, "PI handoff clears through upper row 199");
	expect_pixel(raster, Page::one, {0, 200}, 1U, "P1 NN16 begins at 0,200");
	expect_pixel(raster, Page::one, {319, 207}, 1U, "P1 NN16 reaches row 207");
	expect_pixel(raster, Page::one, {320, 200}, 7U, "P2 NN16 keeps its left edge");
	expect_pixel(raster, Page::one, {639, 200}, 8U, "P2 NN16 keeps its right edge");
	expect_pixel(raster, Page::one, {0, 208}, 3U, "P1 NNEX begins at row 208");
	expect_pixel(raster, Page::one, {319, 271}, 3U, "128-row NNEX ends at row 271");
	expect_pixel(raster, Page::one, {0, 272}, 0U, "short NNEX leaves cleared gap");
	expect_pixel(raster, Page::one, {0, 279}, 0U, "short NNEX gap reaches row 279");
	expect_pixel(raster, Page::one, {320, 279}, 4U, "144-row NNEX reaches row 279");
	expect_pixel(raster, Page::one, {0, 280}, 5U, "EN2 begins at row 280");
	expect_pixel(raster, Page::one, {639, 303}, 5U, "EN2 terminal row is source row 46");
	expect_pixel(raster, Page::one, {0, 304}, 6U, "opponent ENEMY begins at row 304");
	expect_pixel(raster, Page::one, {639, 399}, 6U, "opponent ENEMY fills final VRAM row");
}

void test_main_pi_source_sheet_validation_is_atomic()
{
	auto p1_shot = make_pi(320U, 16U, 1U, 2U);
	auto p2_shot = make_pi(320U, 16U, 2U, 3U);
	auto p1_extra = make_pi(320U, 128U, 3U, 4U);
	auto p2_extra = make_pi(320U, 144U, 4U, 5U);
	auto enemy_center = make_pi(640U, 48U, 5U, 6U);
	auto opponent_enemies = make_pi(640U, 192U, 6U, 7U);
	auto assets = pi_assets(
		p1_shot,
		p2_shot,
		p1_extra,
		p2_extra,
		enemy_center,
		opponent_enemies
	);

	Raster raster;
	expect(raster.set_access_page(Page::one), "select atomic PI destination");
	expect(raster.plot({0, 200}, 9U), "seed atomic PI destination");
	expect(raster.set_access_page(Page::zero), "select atomic PI caller page");

	// The bad value sits on source row 191, which line-skip would discard.
	opponent_enemies.image.indices.back() = 16U;
	expect(
		!th03::present::main_prepare_sprite16_pi_source_sheet(raster, assets),
		"PI handoff validates every pixel before clearing"
	);
	expect(raster.access_page() == Page::zero, "invalid PI handoff preserves access page");
	expect_pixel(raster, Page::one, {0, 200}, 9U, "invalid PI handoff preserves pixels");

	opponent_enemies.image.indices.back() = 7U;
	p1_shot.image.width = 319U;
	expect(
		!th03::present::main_prepare_sprite16_pi_source_sheet(raster, assets),
		"PI handoff rejects a non-320x16 NN16 image"
	);
	expect_pixel(raster, Page::one, {0, 200}, 9U, "bad NN16 geometry is atomic");

	p1_shot.image.width = 320U;
	p1_extra.image.height = 130U;
	expect(
		!th03::present::main_prepare_sprite16_pi_source_sheet(raster, assets),
		"PI handoff rejects an unsupported NNEX height"
	);
	expect_pixel(raster, Page::one, {0, 200}, 9U, "bad NNEX geometry is atomic");

	p1_extra.image.height = 128U;
	enemy_center.image.height = 46U;
	expect(
		!th03::present::main_prepare_sprite16_pi_source_sheet(raster, assets),
		"PI handoff rejects a non-640x48 EN2 image"
	);
	expect_pixel(raster, Page::one, {0, 200}, 9U, "bad EN2 geometry is atomic");

	enemy_center.image.height = 48U;
	opponent_enemies.image.width = 639U;
	expect(
		!th03::present::main_prepare_sprite16_pi_source_sheet(raster, assets),
		"PI handoff rejects a non-640x192 ENEMY image"
	);
	expect_pixel(raster, Page::one, {0, 200}, 9U, "bad ENEMY geometry is atomic");

	opponent_enemies.image.width = 640U;
	assets.enemy_center = nullptr;
	expect(
		!th03::present::main_prepare_sprite16_pi_source_sheet(raster, assets),
		"PI handoff rejects a missing EN2 image"
	);
	expect_pixel(raster, Page::one, {0, 200}, 9U, "missing PI image is atomic");
}

void test_main_pi_bfnt_commit_compatibility()
{
	auto p1_shot = make_pi(320U, 16U, 1U, 2U);
	auto p2_shot = make_pi(320U, 16U, 2U, 3U);
	auto p1_extra = make_pi(320U, 128U, 3U, 4U);
	auto p2_extra = make_pi(320U, 128U, 4U, 5U);
	auto enemy_center = make_pi(640U, 48U, 5U, 6U);
	auto opponent_enemies = make_pi(640U, 192U, 9U, 10U);
	auto player_one = make_character_sheet(1U);
	auto player_two = make_character_sheet(4U);

	Raster raster;
	expect(
		th03::present::main_prepare_sprite16_pi_source_sheet(
			raster,
			pi_assets(
				p1_shot,
				p2_shot,
				p1_extra,
				p2_extra,
				enemy_center,
				opponent_enemies
			)
		),
		"prepare PI sheet before MAIN startup"
	);
	expect(
		raster.copy_page(
			Page::one,
			{0, 0, 640U, 400U},
			Page::zero,
			{0, 0}
		),
		"carry MAINL page one into MAIN page zero"
	);

	Sprite16Staging staging;
	expect(
		staging.stage_main_shots(raster, {&player_one, &player_two}),
		"BFNT shots overlay both carried PI sheets"
	);
	for(const auto page : {Page::zero, Page::one}) {
		expect_pixel(raster, page, {544, 328}, 1U, "BFNT opaque pixel overwrites PI");
		expect_pixel(raster, page, {545, 328}, 9U, "BFNT transparent pixel preserves PI");
		expect_pixel(raster, page, {543, 328}, 9U, "BFNT overlay preserves neighbor");
	}
	expect(staging.commit(raster), "commit combined PI and BFNT source sheet");
	expect(staging.committed(), "combined source sheet produces alpha masks");
	expect_pixel(raster, Page::one, {544, 328}, 1U, "commit retains BFNT overwrite");
	expect_pixel(raster, Page::one, {545, 328}, 9U, "commit retains PI transparency");
	const auto positive = staging.positive_alpha();
	expect(
		positive.has_value() && mask_bit(*positive, 544U, 128U) &&
		mask_bit(*positive, 545U, 128U),
		"alpha commit covers both BFNT and retained PI pixels"
	);
}

void test_constants_and_initial_state()
{
	expect(sprite16_source_top == 200U, "Sprite16 source begins at VRAM row 200");
	expect(sprite16_source_width == 640U, "Sprite16 source spans all 640 pixels");
	expect(sprite16_source_height == 200U, "Sprite16 source has 200 rows");
	expect(sprite16_source_stride == 80U, "Sprite16 packed row is 80 bytes");
	expect(sprite16_source_plane_size == 16000U, "Sprite16 mask plane is 16000 bytes");

	Sprite16Staging staging;
	expect(!staging.committed(), "staging begins uncommitted");
	expect(
		!staging.positive_alpha().has_value(),
		"positive alpha unavailable before commit"
	);
	expect(
		!staging.negative_alpha().has_value(),
		"negative alpha unavailable before commit"
	);
}

void test_main_shot_placement_and_commit()
{
	auto player_one = make_character_sheet(1U);
	auto player_two = make_character_sheet(4U);
	Raster raster;
	expect(raster.set_access_page(Page::zero), "select source page zero");
	expect(raster.plot({17, 201}, 5U), "seed source atlas color");
	expect(raster.plot({639, 399}, 9U), "seed final source atlas pixel");
	expect(raster.plot({10, 199}, 3U), "seed page-zero visible boundary");
	expect(raster.set_access_page(Page::one), "select destination page one");
	expect(raster.plot({17, 201}, 12U), "seed stale destination atlas color");
	expect(raster.plot({639, 399}, 13U), "seed stale final destination pixel");
	expect(raster.plot({10, 199}, 7U), "seed page-one visible boundary");

	Sprite16Staging staging;
	expect(
		staging.stage_main_shots(raster, {&player_one, &player_two}),
		"stage the exact six sub_E266 patterns"
	);
	expect(!staging.committed(), "source placement requires a fresh commit");
	expect(
		raster.access_page() == Page::zero,
		"sub_E266 placement ends on page zero"
	);
	for(const auto page : {Page::zero, Page::one}) {
		expect_pixel(raster, page, {544, 328}, 1U, "P1 glyph 0 placement");
		expect_pixel(raster, page, {576, 328}, 2U, "P1 glyph 3 placement");
		expect_pixel(raster, page, {608, 328}, 3U, "P1 glyph 6 placement");
		expect_pixel(raster, page, {544, 360}, 4U, "P2 glyph 0 placement");
		expect_pixel(raster, page, {576, 360}, 5U, "P2 glyph 3 placement");
		expect_pixel(raster, page, {608, 360}, 6U, "P2 glyph 6 placement");
		expect_pixel(
			raster,
			page,
			{545, 328},
			0U,
			"transparent BF2 pixel preserves atlas zero"
		);
	}

	expect(staging.commit(raster), "commit staged Sprite16 source");
	expect(staging.committed(), "commit records generated alpha boundary");
	expect(raster.access_page() == Page::zero, "commit leaves page zero accessed");
	expect_pixel(
		raster, Page::one, {17, 201}, 5U, "commit copies ordinary source pixel"
	);
	expect_pixel(
		raster, Page::one, {639, 399}, 9U, "commit copies complete source extent"
	);
	expect_pixel(
		raster, Page::zero, {10, 199}, 3U, "commit preserves page-zero visible rows"
	);
	expect_pixel(
		raster, Page::one, {10, 199}, 7U, "commit preserves page-one visible rows"
	);

	const auto positive = staging.positive_alpha();
	const auto negative = staging.negative_alpha();
	expect(
		positive.has_value() && negative.has_value(),
		"commit exposes both alpha masks"
	);
	if(!positive || !negative) {
		return;
	}
	expect(
		positive->width == 640U && positive->height == 200U &&
		positive->stride == 80U && positive->bits.size() == 16000U,
		"positive mask retains packed source geometry"
	);
	const auto seeded_mask_offset = (1U * 80U) + (16U / 8U);
	expect(
		positive->bits[seeded_mask_offset] == 0x40U &&
		negative->bits[seeded_mask_offset] == 0xBFU,
		"alpha generation retains MSB-left packed bits"
	);
	const auto first_shot_mask_offset = (128U * 80U) + (544U / 8U);
	expect(
		positive->bits[first_shot_mask_offset] == 0x80U,
		"sub_E266 placement joins alpha at source-relative row 128"
	);
	expect(
		mask_bit(*positive, 17U, 1U),
		"nonzero source color generates positive alpha"
	);
	expect(
		!mask_bit(*negative, 17U, 1U),
		"nonzero source color clears negative alpha"
	);
	expect(
		!mask_bit(*positive, 18U, 1U),
		"zero source color clears positive alpha"
	);
	expect(
		mask_bit(*negative, 18U, 1U),
		"zero source color sets negative alpha"
	);
	expect(
		mask_bit(*positive, 639U, 199U),
		"final source pixel joins positive alpha"
	);
	for(std::size_t offset = 0U; offset < positive->bits.size(); ++offset) {
		expect(
			static_cast<std::uint8_t>(
				positive->bits[offset] ^ negative->bits[offset]
			) == 0xFFU,
			"negative alpha is exact bitwise complement"
		);
	}
}

void test_recommit_and_invalid_sheet()
{
	auto player_one = make_character_sheet(1U);
	auto player_two = make_character_sheet(4U);
	Raster raster;
	Sprite16Staging staging;
	expect(staging.commit(raster), "empty atlas commit succeeds");

	auto invalid = player_one;
	invalid.header.pixel_width = 16U;
	expect(
		!staging.stage_main_shots(raster, {&invalid, &player_two}),
		"invalid BF2 geometry is rejected before staging"
	);
	expect(staging.committed(), "rejected staging preserves prior commit");

	expect(
		raster.set_access_page(Page::zero),
		"select page zero for atlas mutation"
	);
	expect(raster.plot({0, 200}, 8U), "mutate first source pixel");
	expect(
		raster.set_access_page(Page::one),
		"select page one for stale destination"
	);
	expect(raster.plot({0, 200}, 2U), "seed stale first destination pixel");
	expect(staging.commit(raster), "repeat commit refreshes source and alpha");
	expect_pixel(raster, Page::one, {0, 200}, 8U, "repeat commit refreshes page one");
	const auto positive = staging.positive_alpha();
	expect(
		positive.has_value() && mask_bit(*positive, 0U, 0U),
		"repeat commit refreshes positive alpha"
	);

	expect(
		staging.stage_main_shots(raster, {&player_one, &player_two}),
		"valid BF2 placement after commit succeeds"
	);
	expect(!staging.committed(), "successful source mutation invalidates masks");
	expect(!staging.positive_alpha().has_value(), "stale positive alpha is hidden");
}

} // namespace

int main()
{
	test_constants_and_initial_state();
	test_main_pi_source_sheet_placement();
	test_main_pi_source_sheet_validation_is_atomic();
	test_main_pi_bfnt_commit_compatibility();
	test_main_shot_placement_and_commit();
	test_recommit_and_invalid_sheet();
	if(failures != 0) {
		std::fprintf(stderr, "%d Sprite16 staging checks failed\n", failures);
		return EXIT_FAILURE;
	}
	std::fputs("th03-present Sprite16 staging tests passed\n", stdout);
	return EXIT_SUCCESS;
}
