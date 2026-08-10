#include "th03/present/main_bombs.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>

#include "th03/present/main_static.hpp"

namespace {

using th03::BombCommonEffectKind;
using th03::BombCommonEffectStep;
using th03::BombCompatibilityRead;
using th03::BombCompatibilityReadSite;
using th03::BombFrameStep;
using th03::CommonEffectKind;
using th03::CommonEffectSpawned;
using th03::PlayerId;
using th03::assets::MrsImage;
using th03::assets::MrsOrientation;
using th03::present::Index4;
using th03::present::MainBombAtlasLayout;
using th03::present::MainBombMrsAssets;
using th03::present::MainBombsPresenter;
using th03::present::MainBombsStatus;
using th03::present::MainPaletteState;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::RasterMode;
using th03::present::Sprite16Staging;
using th03::present::StaffFlakeTrigTables;
using th03::present::authentic_main_bomb_atlas_layout;
using th03::present::main_playfield_border;
using th03::present::main_playfield_bordered_width;
using th03::present::sprite16_source_top;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(
			stderr,
			"FAIL: %.*s\n",
			static_cast<int>(message.size()),
			message.data()
		);
	}
}

void expect_pixel(
	const Raster &raster,
	Point point,
	Index4 color,
	std::string_view message
)
{
	expect(raster.pixel(Page::zero, point) == color, message);
}

constexpr std::int8_t paletted(std::uint8_t character) noexcept
{
	return static_cast<std::int8_t>(character * 2U + 1U);
}

constexpr PlayerId player_id(std::uint8_t player) noexcept
{
	return static_cast<PlayerId>(player);
}

struct TestTrig {
	std::array<std::int16_t, 256U> sine{};
	std::array<std::int16_t, 256U> cosine{};

	TestTrig()
	{
		cosine[0x00U] = 256;
		cosine[0x0AU] = 403;
		cosine[0x0BU] = 805;
		cosine[0xC0U] = 0;
		sine[0xC0U] = -256;
		cosine[0x15U] = 220;
		sine[0x15U] = -128;
		cosine[0x6BU] = -224;
		sine[0x6BU] = 128;
		cosine[0x40U] = 0;
		sine[0x40U] = 256;
		cosine[0xEBU] = 224;
		sine[0xEBU] = -128;
		cosine[0x95U] = -224;
		sine[0x95U] = -128;
	}

	[[nodiscard]] StaffFlakeTrigTables view() const noexcept
	{
		return {sine, cosine};
	}
};

MrsImage make_mrs(Index4 color, MrsOrientation orientation)
{
	constexpr std::size_t plane_size = (288U / 8U) * 184U;
	MrsImage image{};
	auto &cel = image.cel;
	cel.bitplane_size = static_cast<std::uint16_t>(plane_size);
	cel.pixel_width = 288U;
	cel.pixel_height = 184U;
	cel.offset_at_bottom_left = 183 * 80;
	cel.vram_dword_width = 9U;
	cel.row_order = th03::assets::PlanarRowOrder::bottom_to_top;
	cel.bit_order = th03::assets::PlanarBitOrder::most_significant_bit_left;
	cel.alpha = std::vector<std::uint8_t>(plane_size, 0xFFU);
	for(std::size_t plane = 0U; plane < cel.color_planes.size(); ++plane) {
		cel.color_planes[plane] = std::vector<std::uint8_t>(
			plane_size,
			((color & (1U << plane)) != 0U) ? 0xFFU : 0U
		);
	}
	image.orientation = orientation;
	return image;
}

struct Fixture {
	Raster raster;
	Sprite16Staging staging;
	MainBombAtlasLayout layout = authentic_main_bomb_atlas_layout();
	MrsImage p1_mrs = make_mrs(2U, MrsOrientation::as_stored);
	MrsImage p2_mrs = make_mrs(2U, MrsOrientation::horizontally_flipped);
	TestTrig trig;
	MainPaletteState palette;
	std::optional<MainBombsPresenter> presenter;

	void prepare(bool commit = true)
	{
		for(std::size_t player = 0U; player < 2U; ++player) {
			const auto seed = [this](
				th03::present::Sprite16AtlasRect rect,
				Index4 color
			) {
				expect(
					raster.clear(
						{
							static_cast<std::int32_t>(rect.left),
							static_cast<std::int32_t>(
								sprite16_source_top + rect.raw_top
							),
							rect.width,
							rect.raw_height,
						},
						color
					),
					"seed a Bomb Sprite16 atlas rectangle"
				);
			};
			seed(layout.reimu_falling[player], 2U);
			for(std::size_t column = 0U; column < 8U; ++column) {
				seed(
					layout.mima_columns[player][column],
					static_cast<Index4>(3U + ((column < 3U) ? 0U :
						((column < 6U) ? 1U : 2U)))
				);
			}
			seed(layout.ellen_vectors[player], 9U);
			seed(layout.marisa_flashes[player][0U], 6U);
			seed(layout.marisa_flashes[player][1U], 7U);
		}
		if(commit) {
			expect(staging.commit(raster), "commit the Bomb Sprite16 atlas");
		}
		expect(
			raster.set_raster_mode(RasterMode::zoomed_200_upper),
			"select MAIN's zoomed raster mode"
		);
		expect(palette.capture_baseline(raster), "capture MAIN palette baseline");
		presenter.emplace(
			raster,
			staging,
			layout,
			MainBombMrsAssets{{&p1_mrs, &p2_mrs}},
			trig.view(),
			palette
		);
	}
};

struct Driver {
	MainBombsPresenter &presenter;
	std::uint8_t character = 0U;
	std::uint8_t player = 0U;
	std::int16_t shift = 0;

	void common(CommonEffectSpawned spawn)
	{
		presenter.common_effect_spawned(spawn);
	}

	void frame(std::uint8_t frame)
	{
		const auto id = player_id(player);
		if(character == 1U && frame <= 64U && (frame % 8U) == 0U) {
			const auto column = static_cast<std::uint16_t>(frame / 8U - 1U);
			presenter.bomb_compatibility_read({
				.player = id,
				.site = BombCompatibilityReadSite::mima_column_x,
				.bomb_frame = frame,
				.raw_value = static_cast<std::uint16_t>(column * (32U << 4U)),
			});
		}
		presenter.bomb_frame_step({
			.player = id,
			.paletted_character = paletted(character),
			.bomb_frame = frame,
			.fg_shift_x_at_entry = shift,
		});
		if(character == 0U && frame < 32U) {
			presenter.bomb_compatibility_read({
				.player = id,
				.site = BombCompatibilityReadSite::reimu_falling_velocity,
				.bomb_frame = frame,
				.raw_value = 0x007FU,
			});
			presenter.bomb_compatibility_read({
				.player = id,
				.site = BombCompatibilityReadSite::reimu_falling_x,
				.bomb_frame = frame,
				.raw_value = 0U,
			});
		}
		if(character == 1U && frame > 80U && frame < 144U &&
			(frame % 8U) == 0U) {
			common({
				.kind = CommonEffectKind::expanding_circle,
				.player = id,
				.center_x = 144 << 4,
				.center_y = 184 << 4,
				.fg_shift_x_at_spawn = shift,
			});
		}
		if(character == 2U && frame >= 64U && frame < 128U &&
			(frame % 8U) == 0U) {
			common({
				.kind = CommonEffectKind::expanding_circle,
				.player = id,
				.center_x = 64 << 4,
				.center_y = 100 << 4,
				.fg_shift_x_at_spawn = static_cast<std::int16_t>(
					((frame & 3U) < 2U) ? 4 : -4
				),
			});
		}
		if(character == 4U || character == 5U) {
			const auto effect_shift = static_cast<std::int16_t>(
				(frame < 64U) ? shift :
					((frame < 128U) ? (((frame & 3U) < 2U) ? 4 : -4) : 0)
			);
			presenter.bomb_common_effect_step({
				.kind = (character == 5U) ? BombCommonEffectKind::kana :
					BombCommonEffectKind::kotohime,
				.player = id,
				.bomb_frame = frame,
				.fg_shift_x_at_effects = effect_shift,
			});
		}
		if(character == 7U && frame < 64U && (frame % 8U) == 0U) {
			common({
				.kind = CommonEffectKind::contracting_circle,
				.player = id,
				.center_x = 144 << 4,
				.center_y = 184 << 4,
				.fg_shift_x_at_spawn = shift,
			});
		}
		if(character == 7U && frame >= 64U && frame < 144U &&
			(frame % 16U) == 0U) {
			const auto radius = static_cast<std::int16_t>((frame - 64U) * 32U);
			const std::array<std::array<std::int16_t, 2U>, 4U> centers{{
				{{static_cast<std::int16_t>((144 << 4) - radius), 184 << 4}},
				{{static_cast<std::int16_t>((144 << 4) + radius), 184 << 4}},
				{{144 << 4, static_cast<std::int16_t>((184 << 4) - radius)}},
				{{144 << 4, static_cast<std::int16_t>((184 << 4) + radius)}},
			}};
			for(std::size_t effect = 0U; effect < centers.size(); ++effect) {
				common({
					.kind = CommonEffectKind::contracting_square,
					.player = id,
					.center_x = centers[effect][0U],
					.center_y = centers[effect][1U],
					.fg_shift_x_at_spawn = static_cast<std::int16_t>(
						((frame & 3U) < 2U) ? 4 : -4
					),
					.initial_angle = static_cast<std::uint8_t>(effect * 17U),
				});
			}
		}
		if(character == 8U && frame > 64U && frame < 144U &&
			(frame % 8U) == 0U) {
			common({
				.kind = CommonEffectKind::contracting_square,
				.player = id,
				.center_x = 144 << 4,
				.center_y = 184 << 4,
				.fg_shift_x_at_spawn = shift,
				.initial_angle = 0x5AU,
			});
		}
		if((character == 0U && frame >= 64U && frame < 112U) ||
			(character == 1U && frame > 80U && frame < 144U) ||
			(character >= 2U && character <= 7U && character != 5U &&
				frame >= 64U && frame < ((character == 7U) ? 144U : 128U)) ||
			((character == 4U || character == 5U) &&
				frame >= 64U && frame < 128U) ||
			(character == 8U && frame > 64U && frame < 144U)) {
			shift = ((frame & 3U) < 2U) ? 4 : -4;
		} else if((character == 0U && frame >= 112U) ||
			(character == 1U && frame >= 144U) ||
			(character >= 2U && character <= 6U && frame >= 128U) ||
			(character == 7U && frame >= 144U) ||
			(character == 8U && frame >= 144U)) {
			shift = 0;
		}
	}

	void through(std::uint8_t last)
	{
		for(std::uint16_t frame_index = 1U; frame_index <= last; ++frame_index) {
			frame(static_cast<std::uint8_t>(frame_index));
		}
	}
};

void test_authentic_layout_and_asset_gate()
{
	const auto layout = authentic_main_bomb_atlas_layout();
	const auto reimu = layout.reimu_falling[0U];
	expect(
		reimu.left == 192U && reimu.raw_top == 0U && reimu.width == 16U &&
			reimu.raw_height == 8U,
		"authentic Reimu atlas rectangle"
	);
	for(std::size_t player = 0U; player < 2U; ++player) {
		for(std::size_t column = 0U; column < 8U; ++column) {
			const auto expected = static_cast<std::uint16_t>(
				player * 320U + ((column < 3U) ? 224U :
					((column < 6U) ? 240U : 256U))
			);
			expect(
				layout.mima_columns[player][column].left == expected,
				"Mima atlas uses the source 3/3/2 cel grouping"
			);
		}
	}

	Fixture invalid;
	invalid.p2_mrs.orientation = MrsOrientation::as_stored;
	const auto before = invalid.raster.display_state_hash();
	invalid.prepare();
	expect(
		invalid.presenter->status() == MainBombsStatus::invalid_assets,
		"unflipped P2 BM.MRS is rejected"
	);
	expect(
		invalid.raster.display_state_hash() != before,
		"fixture staging, not presenter construction, changes the raster"
	);
	const auto after_construction = invalid.raster.display_state_hash();
	invalid.presenter->bomb_frame_step({
		.player = PlayerId::p1,
		.paletted_character = paletted(0U),
		.bomb_frame = 1U,
	});
	expect(
		invalid.raster.display_state_hash() == after_construction,
		"invalid assets make callbacks atomic no-ops"
	);

	Fixture uncommitted;
	uncommitted.prepare(false);
	expect(
		uncommitted.presenter->status() == MainBombsStatus::staging_not_committed,
		"uncommitted Sprite16 staging is rejected"
	);
}

void test_reimu_nonzero_shift_and_wrap()
{
	Fixture fixture;
	fixture.prepare();
	expect(fixture.raster.plot({16, 0}, 15U), "seed early-fill top row");
	expect(fixture.raster.plot({16, 183}, 15U), "seed early-fill bottom row");
	expect(fixture.raster.plot({16, 184}, 15U), "seed row after early fill");
	Driver driver{*fixture.presenter, 0U, 0U, 5};
	driver.frame(1U);
	expect_pixel(
		fixture.raster, {16, 0}, 0U,
		"early Bomb fill starts at raw row 0"
	);
	expect_pixel(
		fixture.raster, {16, 183}, 0U,
		"early Bomb fill ends at raw row 183"
	);
	expect_pixel(
		fixture.raster, {16, 184}, 15U,
		"early Bomb fill does not reach raw row 184"
	);
	expect_pixel(
		fixture.raster, {13, 196}, 2U,
		"Reimu first falling sprite preserves nonzero entry shift"
	);
	for(std::uint8_t frame = 2U; frame <= 32U; ++frame) {
		driver.frame(frame);
	}
	expect(
		fixture.presenter->ready(),
		"Reimu velocity/X callback sequence remains ready"
	);
	expect_pixel(
		fixture.raster, {13, 182}, 2U,
		"Reimu record wraps to 384 pixels and accelerates before frame 32 draw"
	);
}

void test_mima_columns_and_effect_before_mrs()
{
	Fixture fixture;
	fixture.prepare();
	Driver driver{*fixture.presenter, 1U, 0U, 3};
	driver.through(64U);
	for(std::size_t column = 0U; column < 8U; ++column) {
		const auto color = static_cast<Index4>(
			3U + ((column < 3U) ? 0U : ((column < 6U) ? 1U : 2U))
		);
		expect_pixel(
			fixture.raster,
			{static_cast<std::int32_t>(19U + column * 32U), 8},
			color,
			"Mima columns preserve nonzero shift and 3/3/2 source cels"
		);
	}
	for(std::uint8_t frame = 65U; frame < 88U; ++frame) {
		driver.frame(frame);
	}
	driver.presenter.bomb_frame_step({
		.player = PlayerId::p1,
		.paletted_character = paletted(1U),
		.bomb_frame = 88U,
		.fg_shift_x_at_entry = driver.shift,
	});
	expect(fixture.raster.plot({20, 10}, 13U), "seed common-effect order marker");
	driver.common({
		.kind = CommonEffectKind::expanding_circle,
		.player = PlayerId::p1,
		.center_x = 144 << 4,
		.center_y = 184 << 4,
		.fg_shift_x_at_spawn = driver.shift,
	});
	expect_pixel(
		fixture.raster, {20, 10}, 2U,
		"Mima common allocation precedes current shift and MRS draw"
	);
	expect(fixture.presenter->ready(), "Mima source order remains ready");
}

void test_marisa_mrs_before_effect_and_private_sprite()
{
	Fixture fixture;
	fixture.prepare();
	Driver driver{*fixture.presenter, 2U, 0U, 0};
	for(std::uint8_t frame = 1U; frame < 64U; ++frame) {
		driver.frame(frame);
	}
	fixture.presenter->bomb_frame_step({
		.player = PlayerId::p1,
		.paletted_character = paletted(2U),
		.bomb_frame = 64U,
		.fg_shift_x_at_entry = driver.shift,
	});
	expect_pixel(fixture.raster, {20, 10}, 2U, "Marisa MRS draws before effect");
	expect(fixture.raster.plot({20, 10}, 13U), "seed post-MRS effect marker");
	fixture.presenter->common_effect_spawned({
		.kind = CommonEffectKind::expanding_circle,
		.player = PlayerId::p1,
		.center_x = 64 << 4,
		.center_y = 100 << 4,
		.fg_shift_x_at_spawn = 4,
	});
	expect_pixel(
		fixture.raster, {20, 10}, 13U,
		"Marisa common allocation and sprite do not replay the earlier MRS"
	);
	expect_pixel(
		fixture.raster, {60, 46}, 7U,
		"Marisa source-shared point drives the dependent frame-64 sprite"
	);

	Driver p2{*fixture.presenter, 2U, 1U, 0};
	for(std::uint8_t frame = 1U; frame < 64U; ++frame) {
		p2.frame(frame);
	}
	fixture.presenter->bomb_frame_step({
		.player = PlayerId::p2,
		.paletted_character = paletted(2U),
		.bomb_frame = 64U,
		.fg_shift_x_at_entry = 0,
	});
	fixture.presenter->common_effect_spawned({
		.kind = CommonEffectKind::expanding_circle,
		.player = PlayerId::p2,
		.center_x = 200 << 4,
		.center_y = 100 << 4,
		.fg_shift_x_at_spawn = 4,
	});
	driver.shift = 4;
	driver.frame(65U);
	expect_pixel(
		fixture.raster, {516, 46}, 7U,
		"Marisa P1 observes the point replaced by the preceding P2 callback"
	);
}

void test_ellen_multiple_reads_and_shared_release_order()
{
	Fixture fixture;
	fixture.prepare();
	Driver p1{*fixture.presenter, 3U, 0U, 0};
	Driver p2{*fixture.presenter, 3U, 1U, 0};
	for(std::uint8_t frame = 1U; frame < 64U; ++frame) {
		p1.frame(frame);
		p2.frame(frame);
	}
	const auto circle = [](PlayerId player, std::int16_t shift) {
		return CommonEffectSpawned{
			.kind = CommonEffectKind::expanding_circle,
			.player = player,
			.center_x = 144 << 4,
			.center_y = 184 << 4,
			.fg_shift_x_at_spawn = shift,
		};
	};
	for(auto *driver : std::array<Driver *, 2U>{{&p1, &p2}}) {
		driver->presenter.bomb_frame_step({
			.player = player_id(driver->player),
			.paletted_character = paletted(3U),
			.bomb_frame = 64U,
			.fg_shift_x_at_entry = 0,
		});
		driver->presenter.common_effect_spawned(
			circle(player_id(driver->player), 4)
		);
		driver->shift = 4;
	}
	fixture.presenter->bomb_compatibility_read({
		.player = PlayerId::p1,
		.site = BombCompatibilityReadSite::ellen_vector_angle,
		.bomb_frame = 65U,
		.raw_value = 0x000AU,
	});
	fixture.presenter->bomb_compatibility_read({
		.player = PlayerId::p2,
		.site = BombCompatibilityReadSite::ellen_vector_angle,
		.bomb_frame = 65U,
		.raw_value = 0x000BU,
	});
	p1.frame(65U);
	p2.frame(65U);
	expect(
		fixture.presenter->ready(),
		"shared Ellen cursor releases P1 slot 0 then P2 slot 1"
	);

	// Continue P1 alone. Frame 68 releases slot 2 because P2 consumed the
	// shared slot-1 release at frame 64. Frames 69 and 73 therefore exercise
	// synchronous prefix reads without exposing a record ordinal.
	for(std::uint8_t frame = 66U; frame <= 68U; ++frame) {
		p1.presenter.bomb_frame_step({
			.player = PlayerId::p1,
			.paletted_character = paletted(3U),
			.bomb_frame = frame,
			.fg_shift_x_at_entry = p1.shift,
		});
		if((frame % 4U) == 0U) {
			p1.presenter.common_effect_spawned(circle(
				PlayerId::p1,
				static_cast<std::int16_t>(((frame & 3U) < 2U) ? 4 : -4)
			));
		}
		p1.shift = ((frame & 3U) < 2U) ? 4 : -4;
	}
	fixture.presenter->bomb_compatibility_read({
		.player = PlayerId::p1,
		.site = BombCompatibilityReadSite::ellen_vector_angle,
		.bomb_frame = 69U,
		.raw_value = 0x000BU,
	});
	p1.frame(69U);
	for(std::uint8_t frame = 70U; frame <= 72U; ++frame) {
		p1.presenter.bomb_frame_step({
			.player = PlayerId::p1,
			.paletted_character = paletted(3U),
			.bomb_frame = frame,
			.fg_shift_x_at_entry = p1.shift,
		});
		if(frame == 72U) {
			p1.presenter.common_effect_spawned(circle(PlayerId::p1, 4));
		}
		p1.shift = ((frame & 3U) < 2U) ? 4 : -4;
	}
	for(const auto raw : std::array<std::uint16_t, 3U>{{0x20U, 0x21U, 0x22U}}) {
		fixture.presenter->bomb_compatibility_read({
			.player = PlayerId::p1,
			.site = BombCompatibilityReadSite::ellen_vector_angle,
			.bomb_frame = 73U,
			.raw_value = raw,
		});
	}
	p1.frame(73U);
	expect(
		fixture.presenter->ready(),
		"Ellen accepts three same-frame reads in source slot-scan order"
	);
	expect_pixel(
		fixture.raster, {128, 84}, 9U,
		"Ellen draws respawned vectors after the complete prefix read sequence"
	);
}

void test_ellen_release_cursor_is_process_shared()
{
	Fixture fixture;
	fixture.prepare();
	Driver p1{*fixture.presenter, 3U, 0U, 0};
	Driver p2{*fixture.presenter, 3U, 1U, 0};
	for(std::uint8_t frame = 1U; frame < 64U; ++frame) {
		p1.frame(frame);
		p2.frame(frame);
	}

	const auto circle = [](PlayerId player) {
		return CommonEffectSpawned{
			.kind = CommonEffectKind::expanding_circle,
			.player = player,
			.center_x = 144 << 4,
			.center_y = 184 << 4,
			.fg_shift_x_at_spawn = 4,
		};
	};
	for(std::uint8_t frame = 64U; frame <= 81U; ++frame) {
		const bool respawn = frame == 65U || frame == 69U || frame == 73U ||
			frame == 77U;
		if(respawn) {
			for(const auto player : {PlayerId::p1, PlayerId::p2}) {
				fixture.presenter->bomb_compatibility_read({
					.player = player,
					.site = BombCompatibilityReadSite::ellen_vector_angle,
					.bomb_frame = frame,
					// Unspecified synthetic trig entries are zero, keeping every
					// released vector in bounds after its one required read.
					.raw_value = 0x0030U,
				});
			}
		}
		for(auto *driver : std::array<Driver *, 2U>{{&p1, &p2}}) {
			driver->presenter.bomb_frame_step({
				.player = player_id(driver->player),
				.paletted_character = paletted(3U),
				.bomb_frame = frame,
				.fg_shift_x_at_entry = driver->shift,
			});
			if((frame % 4U) == 0U) {
				driver->presenter.common_effect_spawned(
					circle(player_id(driver->player))
				);
			}
			driver->shift = ((frame & 3U) < 2U) ? 4 : -4;
		}
	}
	expect(
		fixture.presenter->ready(),
		"Ellen exhausts one process-shared eight-slot release cursor before frame 80"
	);
}

void test_kana_and_kotohime_steps()
{
	for(const auto character : std::array<std::uint8_t, 2U>{{4U, 5U}}) {
		Fixture fixture;
		fixture.prepare();
		Driver driver{*fixture.presenter, character, 0U, 6};
		driver.through(64U);
		expect(
			fixture.presenter->ready(),
			(character == 4U) ? "Kotohime callback steps remain ordered" :
				"Kana callback steps remain ordered"
		);
		expect_pixel(
			fixture.raster, {20, 10}, 2U,
			(character == 4U) ? "Kotohime MRS follows its exceptional step" :
				"Kana MRS follows its exceptional step"
		);
	}
}

void test_rikako_shared_radius_and_vline_after_mrs()
{
	Fixture fixture;
	fixture.prepare();
	Driver p1{*fixture.presenter, 6U, 0U, 0};
	Driver p2{*fixture.presenter, 6U, 1U, 0};
	p1.through(63U);
	p2.through(63U);
	p1.frame(64U);
	p2.frame(64U);
	expect_pixel(
		fixture.raster, {164, 100}, 15U,
		"Rikako P1 vline is drawn after BM.MRS"
	);
	expect_pixel(
		fixture.raster, {479, 100}, 15U,
		"Rikako P2 observes the radius advanced by the preceding P1 callback"
	);
	expect_pixel(
		fixture.raster, {480, 100}, 2U,
		"Rikako shared radius is not incorrectly reset per player"
	);
}

void test_chiyuri_four_effects_before_mrs()
{
	Fixture fixture;
	fixture.prepare();
	Driver driver{*fixture.presenter, 7U, 0U, 0};
	driver.through(79U);
	fixture.presenter->bomb_frame_step({
		.player = PlayerId::p1,
		.paletted_character = paletted(7U),
		.bomb_frame = 80U,
		.fg_shift_x_at_entry = driver.shift,
	});
	constexpr std::int16_t radius = 16 * 32;
	const std::array<std::array<std::int16_t, 2U>, 4U> centers{{
		{{static_cast<std::int16_t>((144 << 4) - radius), 184 << 4}},
		{{static_cast<std::int16_t>((144 << 4) + radius), 184 << 4}},
		{{144 << 4, static_cast<std::int16_t>((184 << 4) - radius)}},
		{{144 << 4, static_cast<std::int16_t>((184 << 4) + radius)}},
	}};
	for(std::uint8_t effect = 0U; effect < 3U; ++effect) {
		fixture.presenter->common_effect_spawned({
			.kind = CommonEffectKind::contracting_square,
			.player = PlayerId::p1,
			.center_x = centers[effect][0U],
			.center_y = centers[effect][1U],
			.fg_shift_x_at_spawn = 4,
			.initial_angle = effect,
		});
	}
	expect(fixture.raster.plot({20, 10}, 13U), "seed fourth-effect marker");
	fixture.presenter->common_effect_spawned({
		.kind = CommonEffectKind::contracting_square,
		.player = PlayerId::p1,
		.center_x = centers[3U][0U],
		.center_y = centers[3U][1U],
		.fg_shift_x_at_spawn = 4,
		.initial_angle = 3U,
	});
	expect_pixel(
		fixture.raster, {20, 10}, 2U,
		"Chiyuri waits for all four ordered square allocations before MRS"
	);
}

void test_yumemi_triangles_and_effect_before_mrs()
{
	Fixture fixture;
	fixture.prepare();
	Driver driver{*fixture.presenter, 8U, 0U, 0};
	driver.through(31U);
	expect(
		fixture.raster.clear({16, 8, 288, 184}, 0U),
		"clear before Yumemi frame-32 boundary probe"
	);
	driver.frame(32U);
	std::size_t frame32_count = 0U;
	std::int32_t frame32_min_x = 640;
	std::int32_t frame32_max_x = -1;
	std::int32_t frame32_min_y = 400;
	std::int32_t frame32_max_y = -1;
	for(std::int32_t y = 8; y <= 191; ++y) {
		for(std::int32_t x = 16; x <= 303; ++x) {
			if(fixture.raster.pixel(Page::zero, {x, y}) == 7U) {
				++frame32_count;
				frame32_min_x = (x < frame32_min_x) ? x : frame32_min_x;
				frame32_max_x = (x > frame32_max_x) ? x : frame32_max_x;
				frame32_min_y = (y < frame32_min_y) ? y : frame32_min_y;
				frame32_max_y = (y > frame32_max_y) ? y : frame32_max_y;
			}
		}
	}
	expect(
		frame32_count == 7163U && frame32_min_x == 48 &&
		frame32_max_x == 270 && frame32_min_y == 36 &&
		frame32_max_y == 132,
		"Yumemi frame 32 preserves the first source-angle triangle extent"
	);
	expect_pixel(
		fixture.raster, {160, 80}, 7U,
		"Yumemi frame 32 draws only the fixed-radius first triangle"
	);
	expect(
		fixture.raster.clear({16, 8, 288, 184}, 0U),
		"clear before Yumemi frame-33 boundary probe"
	);
	driver.frame(33U);
	expect_pixel(
		fixture.raster, {160, 100}, 8U,
		"Yumemi frame 33 overlays the second source-angle triangle"
	);
	for(std::uint8_t frame = 34U; frame < 72U; ++frame) {
		driver.frame(frame);
	}
	fixture.presenter->bomb_frame_step({
		.player = PlayerId::p1,
		.paletted_character = paletted(8U),
		.bomb_frame = 72U,
		.fg_shift_x_at_entry = driver.shift,
	});
	expect(fixture.raster.plot({20, 10}, 13U), "seed Yumemi square marker");
	fixture.presenter->common_effect_spawned({
		.kind = CommonEffectKind::contracting_square,
		.player = PlayerId::p1,
		.center_x = 144 << 4,
		.center_y = 184 << 4,
		.fg_shift_x_at_spawn = driver.shift,
		.initial_angle = 0x99U,
	});
	expect_pixel(
		fixture.raster, {20, 10}, 2U,
		"Yumemi square allocation precedes current shift and MRS"
	);
}

void test_invalid_facts_are_atomic_and_sticky()
{
	Fixture fixture;
	fixture.prepare();
	const auto before = fixture.raster.display_state_hash();
	fixture.presenter->bomb_frame_step({
		.player = static_cast<PlayerId>(9U),
		.paletted_character = paletted(0U),
		.bomb_frame = 1U,
	});
	expect(
		fixture.presenter->status() == MainBombsStatus::invalid_observation,
		"invalid player is rejected"
	);
	expect(
		fixture.raster.display_state_hash() == before,
		"invalid frame fact is atomic"
	);
	fixture.presenter->bomb_frame_step({
		.player = PlayerId::p1,
		.paletted_character = paletted(0U),
		.bomb_frame = 1U,
	});
	expect(
		fixture.presenter->status() == MainBombsStatus::invalid_observation &&
		fixture.raster.display_state_hash() == before,
		"status is sticky and later callbacks are no-ops"
	);

	Fixture pending;
	pending.prepare();
	pending.presenter->bomb_frame_step({
		.player = PlayerId::p1,
		.paletted_character = paletted(5U),
		.bomb_frame = 1U,
		.fg_shift_x_at_entry = 0,
	});
	const auto after_first = pending.raster.display_state_hash();
	pending.presenter->bomb_frame_step({
		.player = PlayerId::p1,
		.paletted_character = paletted(5U),
		.bomb_frame = 2U,
		.fg_shift_x_at_entry = 0,
	});
	expect(
		pending.presenter->status() == MainBombsStatus::invalid_observation,
		"missing Kana callback step is detected at the next fact"
	);
	expect(
		pending.raster.display_state_hash() == after_first,
		"fact arriving while a callback is pending makes no new mutation"
	);
}

void test_completion_restores_process_baseline()
{
	Fixture fixture;
	fixture.prepare();
	Driver driver{*fixture.presenter, 2U, 0U, 0};
	driver.through(179U);
	expect(
		fixture.raster.pending_palette().colors[0U] !=
			th03::present::Rgb8{},
		"Marisa Bomb has changed the player palette before completion"
	);
	fixture.presenter->bomb_finished({
		.player = PlayerId::p1,
		.paletted_character = paletted(2U),
	});
	expect(
		fixture.presenter->ready(),
		"BombFinished is accepted at the frame-179-to-180 transition"
	);
	expect(
		fixture.raster.pending_palette().colors[0U] ==
			th03::present::Rgb8{} && fixture.palette.dirty(),
		"BombFinished restores only the saved entry through the deferred latch"
	);
}

void test_round_reset_rearms_an_interrupted_bomb()
{
	Fixture fixture;
	fixture.prepare();
	Driver interrupted{*fixture.presenter, 1U, 0U, 0};
	interrupted.through(64U);
	expect(
		fixture.presenter->ready(),
		"an interrupted Bomb remains valid before the round boundary"
	);

	Driver next_round{*fixture.presenter, 1U, 0U, 0};
	next_round.frame(1U);
	expect(
		fixture.presenter->ready(),
		"frame 1 re-arms presentation after a round aborts an active Bomb"
	);
	expect_pixel(
		fixture.raster, {16, 8}, 0U,
		"same-character re-arm clears stale per-activation Mima columns"
	);
}

void test_p2_mrs_uses_source_altered_blue_plane()
{
	Fixture fixture;
	(*fixture.p2_mrs.cel.alpha)[0U] = 0U;
	fixture.prepare();
	Driver driver{*fixture.presenter, 2U, 1U, 0};
	driver.through(65U);
	expect_pixel(
		fixture.raster, {336, 191}, 3U,
		"P2 BM.MRS ORs blue with inverse alpha before the no-alpha put"
	);
}

} // namespace

int main()
{
	test_authentic_layout_and_asset_gate();
	test_reimu_nonzero_shift_and_wrap();
	test_mima_columns_and_effect_before_mrs();
	test_marisa_mrs_before_effect_and_private_sprite();
	test_ellen_multiple_reads_and_shared_release_order();
	test_ellen_release_cursor_is_process_shared();
	test_kana_and_kotohime_steps();
	test_rikako_shared_radius_and_vline_after_mrs();
	test_chiyuri_four_effects_before_mrs();
	test_yumemi_triangles_and_effect_before_mrs();
	test_invalid_facts_are_atomic_and_sticky();
	test_round_reset_rearms_an_interrupted_bomb();
	test_p2_mrs_uses_source_altered_blue_plane();
	test_completion_restores_process_baseline();
	std::printf("%zu checks, %zu failures\n", checks, failures);
	return (failures == 0U) ? 0 : 1;
}
