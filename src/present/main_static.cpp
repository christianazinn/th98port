#include "th03/present/main_static.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace th03::present {
namespace {

constexpr std::uint8_t glyph_half_width = 8U;
constexpr std::uint8_t glyph_height = 16U;
constexpr std::uint8_t gaiji_tram_width = 2U;
constexpr std::uint8_t main_playfield_tram_top = 1U;
constexpr std::uint8_t main_playfield_tram_width = 36U;
constexpr std::uint8_t main_bombs_coordinate_limit = 18U;
constexpr std::uint8_t main_rounds_coordinate_limit = 19U;
constexpr std::uint8_t main_story_lives_coordinate_limit = 19U;
constexpr std::uint16_t character_sheet_cel_width = 32U;
constexpr std::uint16_t character_sheet_cel_height = 32U;
constexpr std::size_t character_sheet_cel_count = 9U;
constexpr std::array<std::size_t, 3U> shot_pattern_indices{0U, 3U, 6U};
constexpr std::array<std::int32_t, 3U> shot_sheet_lefts{544, 576, 608};
constexpr std::array<std::int32_t, main_player_count> shot_sheet_tops{328, 360};

struct PreparedShotPattern {
	PlanarSource color{};
	std::vector<std::uint8_t> opaque{};
};

constexpr std::int32_t playfield_left(std::size_t player) noexcept
{
	return static_cast<std::int32_t>(
		main_playfield_border + (player * main_playfield_bordered_width)
	);
}

constexpr std::int32_t playfield_tram_x(
	std::size_t player,
	std::uint16_t local_x
) noexcept
{
	return (playfield_left(player) + local_x) / glyph_half_width;
}

bool valid_hud_state(const MainHudState &state) noexcept
{
	for(const auto &player : state.players) {
		if(player.halfhearts > main_halfhearts_max ||
			player.bombs > main_bombs_coordinate_limit ||
			player.rounds_won > main_rounds_coordinate_limit ||
			player.gauge_level < main_gauge_level_min ||
			player.gauge_level > main_gauge_level_max) {
			return false;
		}
	}
	if(state.boss_level > main_boss_level_max) {
		return false;
	}
	return (state.game_mode != MainGameMode::story) ||
		(state.story_lives <= main_story_lives_coordinate_limit);
}

bool put_halfhearts(
	Tram &tram,
	std::size_t player_id,
	std::uint8_t halfhearts
) noexcept
{
	constexpr auto halfhearts_width = static_cast<std::uint16_t>(
		(main_halfhearts_max / 2U) * gaiji_width
	);
	auto x = playfield_tram_x(
		player_id,
		static_cast<std::uint16_t>(
			(main_playfield_width / 2U) - (halfhearts_width / 2U)
		)
	);
	std::uint8_t halfheart_index = 0U;
	while(halfheart_index < halfhearts) {
		const auto glyph = static_cast<std::uint8_t>(
			((halfhearts - halfheart_index) >= 2) ?
				gaiji_heart_full : gaiji_heart_half
		);
		if(!tram.put_gaiji({x, main_playfield_tram_top}, glyph, tram_white)) {
			return false;
		}
		x += gaiji_tram_width;
		halfheart_index = static_cast<std::uint8_t>(halfheart_index + 2U);
	}
	while(halfheart_index < main_halfhearts_max) {
		if(!tram.put_gaiji(
			{x, main_playfield_tram_top},
			gaiji_heart_empty,
			tram_white
		)) {
			return false;
		}
		x += gaiji_tram_width;
		halfheart_index = static_cast<std::uint8_t>(halfheart_index + 2U);
	}
	return true;
}

bool put_bombs(
	Tram &tram,
	std::size_t player_id,
	std::uint8_t bombs
) noexcept
{
	auto x = playfield_tram_x(
		player_id,
		(player_id == 0U) ?
			static_cast<std::uint16_t>(main_playfield_width - gaiji_width) : 0U
	);
	const auto step = (player_id == 0U) ?
		-static_cast<std::int32_t>(gaiji_tram_width) :
		static_cast<std::int32_t>(gaiji_tram_width);
	constexpr auto y = static_cast<std::int32_t>(
		(main_playfield_bottom - glyph_height) / glyph_height
	);
	for(std::uint8_t index = 0U; index < bombs; ++index) {
		if(!tram.put_gaiji({x, y}, gaiji_bomb, tram_white)) {
			return false;
		}
		x += step;
	}
	return tram.put_gaiji({x, y}, gaiji_space, tram_white);
}

bool put_rounds_won(
	Tram &tram,
	std::size_t player_id,
	std::uint8_t rounds_won
) noexcept
{
	auto x = playfield_tram_x(player_id, 0U);
	for(std::uint8_t index = 0U; index < rounds_won; ++index) {
		if(!tram.put_gaiji(
			{x, main_playfield_tram_top},
			gaiji_thalia,
			tram_white
		)) {
			return false;
		}
		x += gaiji_tram_width;
	}
	return true;
}

bool put_story_lives(Tram &tram, std::uint8_t lives) noexcept
{
	auto x = playfield_tram_x(
		0U,
		static_cast<std::uint16_t>(main_playfield_width - gaiji_width)
	);
	for(std::uint8_t index = 0U; index < lives; ++index) {
		if(!tram.put_gaiji(
			{x, main_playfield_tram_top + 1},
			gaiji_yinyang,
			tram_white
		)) {
			return false;
		}
		x -= gaiji_tram_width;
	}
	return true;
}

bool put_gauge_levels(
	Tram &tram,
	std::size_t player_id,
	std::uint8_t gauge_level,
	std::uint8_t boss_level
) noexcept
{
	auto x = playfield_tram_x(player_id, 0U);
	const auto gauge_digit = static_cast<std::uint8_t>(
		gaiji_proportional_digit_1 - 1U + gauge_level
	);
	constexpr auto y = static_cast<std::int32_t>(
		main_playfield_bottom / glyph_height
	);
	if(!tram.put_gaiji({x, y}, gauge_digit, tram_green)) {
		return false;
	}
	x += static_cast<std::int32_t>(
		(main_playfield_width / glyph_half_width) - gaiji_tram_width
	);

	// The source displays the next Boss Attack level. At the cap it decrements
	// once, so gpd_1 + level still selects the proportional digit 16.
	const auto displayed_boss_level = static_cast<std::uint8_t>(
		(boss_level >= main_boss_level_max) ? (boss_level - 1U) : boss_level
	);
	return tram.put_gaiji(
		{x, y},
		static_cast<std::uint8_t>(
			gaiji_proportional_digit_1 + displayed_boss_level
		),
		tram_red
	);
}

bool valid_character_sheet(const assets::BfntSheet &sheet) noexcept
{
	if(sheet.consumer != assets::BfntConsumerPolicy::superimpose ||
		sheet.header.pixel_width != character_sheet_cel_width ||
		sheet.header.pixel_height != character_sheet_cel_height ||
		sheet.header.first_code != 0U ||
		sheet.header.last_code != (character_sheet_cel_count - 1U) ||
		sheet.transparent_color > 15U ||
		sheet.glyphs.size() != character_sheet_cel_count) {
		return false;
	}

	constexpr auto stride = static_cast<std::size_t>(
		character_sheet_cel_width / 8U
	);
	constexpr auto plane_size = stride * character_sheet_cel_height;
	for(std::size_t index = 0U; index < sheet.glyphs.size(); ++index) {
		const auto &glyph = sheet.glyphs[index];
		const auto *pixels = std::get_if<assets::PlanarBitmap4>(&glyph.pixels);
		if(glyph.code != index || pixels == nullptr ||
			pixels->row_order != assets::PlanarRowOrder::top_to_bottom ||
			pixels->bit_order !=
				assets::PlanarBitOrder::most_significant_bit_left) {
			return false;
		}
		for(const auto &plane : pixels->color_planes) {
			if(plane.size() != plane_size) {
				return false;
			}
		}
	}
	return true;
}

PreparedShotPattern prepare_pattern(
	const assets::BfntSheet &sheet,
	std::size_t glyph_index
)
{
	const auto &pixels = std::get<assets::PlanarBitmap4>(
		sheet.glyphs[glyph_index].pixels
	);
	constexpr auto stride = static_cast<std::size_t>(
		character_sheet_cel_width / 8U
	);
	constexpr auto plane_size = stride * character_sheet_cel_height;
	PreparedShotPattern prepared{};
	for(std::size_t plane = 0U; plane < planar_plane_count; ++plane) {
		prepared.color.planes[plane] = pixels.color_planes[plane];
	}
	prepared.color.width = character_sheet_cel_width;
	prepared.color.height = character_sheet_cel_height;
	prepared.color.stride = stride;
	prepared.color.row_order = PackedRowOrder::top_to_bottom;
	prepared.opaque.resize(plane_size);

	for(std::size_t offset = 0U; offset < plane_size; ++offset) {
		std::uint8_t opaque = 0U;
		for(std::size_t plane = 0U; plane < planar_plane_count; ++plane) {
			const auto transparent_plane =
				((sheet.transparent_color & (1U << plane)) != 0U) ?
					0xFFU : 0x00U;
			opaque = static_cast<std::uint8_t>(
				opaque |
				(pixels.color_planes[plane][offset] ^ transparent_plane)
			);
		}
		prepared.opaque[offset] = opaque;
	}
	return prepared;
}

} // namespace

bool main_hud_gauge_levels_put(
	Tram &tram,
	std::size_t player_id,
	std::uint8_t gauge_level,
	std::uint8_t boss_level
) noexcept
{
	if(player_id >= main_player_count ||
		gauge_level < main_gauge_level_min ||
		gauge_level > main_gauge_level_max ||
		boss_level > main_boss_level_max) {
		return false;
	}
	return put_gauge_levels(tram, player_id, gauge_level, boss_level);
}

void main_hud_wipe(Tram &tram) noexcept
{
	tram.fill({
		' ',
		static_cast<TramAttribute>(tram_black | tram_reverse),
	});
	constexpr auto playfield_rows = static_cast<std::uint8_t>(
		tram_height - main_playfield_tram_top
	);
	for(std::size_t player = 0U; player < main_player_count; ++player) {
		(void)tram.fill_attributes(
			{
				playfield_tram_x(player, 0U),
				main_playfield_tram_top,
				main_playfield_tram_width,
				playfield_rows,
			},
			tram_white
		);
	}
}

bool main_hud_static_put(Tram &tram, const MainHudState &state) noexcept
{
	if(!valid_hud_state(state)) {
		return false;
	}
	for(std::size_t player = 0U; player < main_player_count; ++player) {
		const auto &player_state = state.players[player];
		if(!put_halfhearts(tram, player, player_state.halfhearts) ||
			!put_bombs(tram, player, player_state.bombs) ||
			!put_rounds_won(tram, player, player_state.rounds_won) ||
			!main_hud_gauge_levels_put(
				tram,
				player,
				player_state.gauge_level,
				state.boss_level
			)) {
			return false;
		}
	}
	return (state.game_mode != MainGameMode::story) ||
		put_story_lives(tram, state.story_lives);
}

bool main_prepare_shot_source_sheet(
	Raster &raster,
	MainPlayerShotSheets sheets
)
{
	if(sheets.player_one == nullptr || sheets.player_two == nullptr ||
		!valid_character_sheet(*sheets.player_one) ||
		!valid_character_sheet(*sheets.player_two)) {
		return false;
	}
	const std::array<const assets::BfntSheet *, main_player_count> players{
		sheets.player_one,
		sheets.player_two,
	};
	std::array<PreparedShotPattern, main_player_count * shot_pattern_indices.size()>
		patterns{};
	for(std::size_t player = 0U; player < players.size(); ++player) {
		for(std::size_t column = 0U; column < shot_pattern_indices.size(); ++column) {
			patterns[(player * shot_pattern_indices.size()) + column] =
				prepare_pattern(*players[player], shot_pattern_indices[column]);
		}
	}

	// sub_E266() selects page 1 first, followed by page 0.
	for(const auto page : {Page::one, Page::zero}) {
		if(!raster.set_access_page(page)) {
			return false;
		}
		for(std::size_t player = 0U; player < players.size(); ++player) {
			for(
				std::size_t column = 0U;
				column < shot_pattern_indices.size();
				++column
			) {
				const auto &pattern =
					patterns[(player * shot_pattern_indices.size()) + column];
				const BitSource opaque{
					pattern.opaque,
					character_sheet_cel_width,
					character_sheet_cel_height,
					character_sheet_cel_width / 8U,
					PackedRowOrder::top_to_bottom,
				};
				if(!raster.blit_planar_masked(
					pattern.color,
					opaque,
					{shot_sheet_lefts[column], shot_sheet_tops[player]}
				)) {
					return false;
				}
			}
		}
	}
	return true;
}

bool MainStaticPresenter::round_startup(
	Raster &raster,
	Tram &tram,
	const MainHudState &hud,
	MainPlayerShotSheets sheets,
	MainPaletteState &palette
)
{
	if(!valid_hud_state(hud) || sheets.player_one == nullptr ||
		sheets.player_two == nullptr ||
		!valid_character_sheet(*sheets.player_one) ||
		!valid_character_sheet(*sheets.player_two)) {
		return false;
	}
	if(!palette.capture_baseline(raster)) {
		return false;
	}

	// The first text_fillca() precedes graph_copy_page(0). Later source startup
	// repeats the fill as part of hud_wipe(). Keeping both calls also preserves
	// the exact raw-TRAM transaction for future operation tracing.
	tram.fill({
		' ',
		static_cast<TramAttribute>(tram_black | tram_reverse),
	});
	if(!raster.copy_page(
		Page::one,
		{0, 0, graphics_width, graphics_height},
		Page::zero,
		{0, 0}
	) || !raster.set_access_page(Page::zero)) {
		return false;
	}
	if(!sprite_staging_.stage_main_shots(raster, sheets)) {
		return false;
	}

	// BF2 loads temporarily replace the logical palette. SUB_A38E() restores
	// the full RGB8 palette captured from the resident stage handoff and applies
	// it. Keeping the logical low nibbles matters to later runtime palette math.
	if(!palette.restore_baseline_and_show(raster)) {
		return false;
	}

	// graph_200line(0) selects the upper zoomed half and access page 0.
	pages_ = {Page::zero, Page::one};
	if(!raster.set_raster_mode(RasterMode::zoomed_200_upper) ||
		!sprite_staging_.commit(raster) ||
		!raster.set_shown_page(Page::one)) {
		return false;
	}
	main_hud_wipe(tram);
	if(!main_hud_static_put(tram, hud)) {
		return false;
	}
	started_ = true;
	return true;
}

bool MainStaticPresenter::repeat_round_startup(
	Raster &raster,
	Tram &tram,
	const MainHudState &hud,
	MainPaletteState &palette
) noexcept
{
	if(!started_ || !valid_hud_state(hud) || !palette.baseline_ready()) {
		return false;
	}
	constexpr Rect upper_page{
		0,
		0,
		graphics_width,
		sprite16_source_top,
	};
	// sub_A21F(): GRCG fill page 1, then page 0, and show page 1 while
	// leaving page 0 accessed. The source reaches repeat on the matching role
	// parity; spelling the roles out here makes the native invariant explicit.
	if(!raster.set_access_page(Page::one) || !raster.clear(upper_page, 2U) ||
		!raster.set_access_page(Page::zero) || !raster.clear(upper_page, 2U) ||
		!raster.set_shown_page(Page::one)) {
		return false;
	}
	main_hud_wipe(tram);
	if(!palette.restore_baseline_and_show(raster) ||
		!main_hud_static_put(tram, hud)) {
		return false;
	}
	pages_ = {Page::zero, Page::one};
	return true;
}

bool MainStaticPresenter::end_frame_swap_and_clear(Raster &raster) noexcept
{
	if(!started_) {
		return false;
	}
	const auto old_back = pages_.back;
	const auto old_front = pages_.front;
	if(!raster.set_access_page(old_front) ||
		!raster.set_shown_page(old_back)) {
		return false;
	}
	pages_.front = old_back;
	pages_.back = (old_back == Page::zero) ? Page::one : Page::zero;
	constexpr auto clear_height = static_cast<std::uint16_t>(
		main_next_page_clear_bottom + 1U
	);
	return raster.clear(
		{playfield_left(0U), 0, main_playfield_width, clear_height},
		0U
	) && raster.clear(
		{playfield_left(1U), 0, main_playfield_width, clear_height},
		1U
	);
}

MainPageRoles MainStaticPresenter::page_roles() const noexcept
{
	return pages_;
}

const Sprite16Staging &MainStaticPresenter::sprite_staging() const noexcept
{
	return sprite_staging_;
}

bool MainStaticPresenter::started() const noexcept
{
	return started_;
}

} // namespace th03::present
