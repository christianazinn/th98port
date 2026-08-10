#include "th03/present/main_hud_result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "th03/present/main_static.hpp"
#include "th03/present/triangles.hpp"
#include "th03/present/vectors.hpp"

namespace th03::present {
namespace {

constexpr std::uint8_t gaiji_bold_digit_0 = 0xA0U;
constexpr std::uint8_t combo_frames = 80U;
constexpr std::uint8_t combo_hit_reset_frames = 32U;
constexpr GrcgClipRect transition_opening_clip{8, 0, 623, 191};
constexpr GrcgClipRect transition_closing_clip{16, 0, 623, 191};
constexpr GrcgClipRect full_zoomed_clip{0, 0, 639, 199};

[[nodiscard]] constexpr std::int16_t wrap_word(std::int32_t value) noexcept
{
	return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr bool valid_player(PlayerId player) noexcept
{
	return player == PlayerId::p1 || player == PlayerId::p2;
}

[[nodiscard]] constexpr std::size_t player_index(PlayerId player) noexcept
{
	return static_cast<std::size_t>(player);
}

struct ResolvedPattern {
	const PreparedSuperimposeSheet *sheet = nullptr;
	std::uint16_t code = 0U;
};

[[nodiscard]] ResolvedPattern resolve_pattern(
	const MainHudResultSheets &sheets,
	std::uint8_t global_pattern
) noexcept
{
	if(global_pattern >= 18U && global_pattern <= 25U) {
		return {sheets.lose, static_cast<std::uint16_t>(global_pattern - 18U)};
	}
	if(global_pattern >= 26U && global_pattern <= 36U) {
		return {sheets.round, static_cast<std::uint16_t>(global_pattern - 26U)};
	}
	if(global_pattern >= 37U && global_pattern <= 44U) {
		return {sheets.zikicw, static_cast<std::uint16_t>(global_pattern - 37U)};
	}
	return {};
}

[[nodiscard]] bool put_pattern(
	Raster &raster,
	const MainHudResultSheets &sheets,
	Point point,
	std::uint8_t global_pattern
) noexcept
{
	const auto pattern = resolve_pattern(sheets, global_pattern);
	return pattern.sheet != nullptr &&
		superimpose_put(raster, *pattern.sheet, point, pattern.code);
}

[[nodiscard]] bool put_cells(
	Tram &tram,
	TramPoint point,
	std::span<const TramCell> cells
) noexcept
{
	if(point.x < 0 || point.y < 0 || point.y >= tram_height ||
		point.x + static_cast<std::int32_t>(cells.size()) > tram_width) {
		return false;
	}
	for(std::size_t index = 0U; index < cells.size(); ++index) {
		if(!tram.put_cell(
			{point.x + static_cast<std::int32_t>(index), point.y},
			cells[index]
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool put_gaiji_value(
	Tram &tram,
	std::int32_t left,
	std::int32_t source_top,
	std::uint8_t value
) noexcept
{
	const auto row = (source_top + 96) / 16;
	auto remainder = value;
	if(remainder >= 10U) {
		if(remainder >= 100U) {
			if(!tram.put_gaiji(
				{left + 18, row},
				static_cast<std::uint8_t>(gaiji_bold_digit_0 + (remainder / 100U)),
				tram_white
			)) {
				return false;
			}
			remainder = static_cast<std::uint8_t>(remainder % 100U);
		}
		if(!tram.put_gaiji(
			{left + 20, row},
			static_cast<std::uint8_t>(gaiji_bold_digit_0 + (remainder / 10U)),
			tram_white
		)) {
			return false;
		}
		remainder = static_cast<std::uint8_t>(remainder % 10U);
	}
	return tram.put_gaiji(
		{left + 22, row},
		static_cast<std::uint8_t>(gaiji_bold_digit_0 + remainder),
		tram_white
	);
}

[[nodiscard]] bool points_put(
	Raster &raster,
	const ScoreFontMasks &font,
	std::int32_t left,
	std::int32_t source_top,
	std::uint16_t points,
	Index4 color,
	bool append_zero
) noexcept
{
	constexpr std::array<std::uint16_t, 4U> divisors{{
		10000U, 1000U, 100U, 10U,
	}};
	auto remainder = points;
	bool visible = false;
	auto x = left;
	for(const auto divisor : divisors) {
		const auto digit = static_cast<std::uint8_t>(remainder / divisor);
		remainder = static_cast<std::uint16_t>(remainder % divisor);
		visible = visible || (digit != 0U);
		if(visible && !score_digit_put(
			raster, x, source_top + 1, font, digit, color
		)) {
			return false;
		}
		x += 8;
	}
	const auto ones = static_cast<std::uint8_t>(remainder);
	if((append_zero || visible || ones != 0U) && !score_digit_put(
		raster, x, source_top + 1, font, ones, color
	)) {
		return false;
	}
	if(append_zero && !score_digit_put(
		raster, x + 8, source_top + 1, font, 0U, color
	)) {
		return false;
	}
	return true;
}

[[nodiscard]] bool draw_score_digits(
	Raster &raster,
	const ScoreFontMasks &font,
	const std::array<std::uint8_t, 8U> &digits,
	std::int32_t left
) noexcept
{
	bool visible = false;
	for(std::size_t column = 0U; column < digits.size(); ++column) {
		const auto digit = digits[(digits.size() - 1U) - column];
		visible = visible || (digit != 0U);
		if(visible && !score_digit_put(
			raster,
			left + static_cast<std::int32_t>(column * 8U),
			9,
			font,
			digit,
			15U
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool draw_transition_triangle(
	Raster &raster,
	GrcgPoint first,
	GrcgPoint second,
	GrcgPoint third,
	GrcgClipRect clip,
	std::uint16_t mask
) noexcept
{
	return grcg_triangle(raster, first, second, third, clip, {2U, mask});
}

} // namespace

void MainHudResultPresenter::main_result_state(
	const MainResultState &state
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto has_defeated = state.defeated_player.has_value();
	const auto has_winner = state.winner.has_value();
	if(has_defeated != has_winner ||
		(has_defeated && (!valid_player(*state.defeated_player) ||
		 !valid_player(*state.winner) ||
		 *state.defeated_player == *state.winner)) ||
		(state.summary_active && !has_winner)) {
		status_ = MainHudResultStatus::invalid_event_sequence;
		return;
	}

	if(has_defeated) {
		const auto defeated = player_index(*state.defeated_player);
		const auto winner = player_index(*state.winner);
		const auto phase = static_cast<std::uint8_t>(
			state.context.round_or_result_frame & 31U
		);
		const auto winner_y = (phase < 16U) ?
			static_cast<std::int32_t>(phase + 32U) :
			static_cast<std::int32_t>(64U - phase);
		if(!put_pattern(
			*raster_,
			sheets_,
			{static_cast<std::int32_t>((defeated * 320U) + 96U), 40},
			18U
		) || !put_pattern(
			*raster_,
			sheets_,
			{static_cast<std::int32_t>((defeated * 320U) + 160U), 40},
			19U
		) || !put_pattern(
			*raster_,
			sheets_,
			{static_cast<std::int32_t>((winner * 320U) + 96U), winner_y},
			20U
		) || !put_pattern(
			*raster_,
			sheets_,
			{static_cast<std::int32_t>((winner * 320U) + 160U), winner_y},
			21U
		)) {
			status_ = MainHudResultStatus::draw_failed;
			return;
		}
	}
	if(!state.summary_active) {
		return;
	}

	const auto frame = state.context.round_or_result_frame;
	const auto winner = player_index(*state.winner);
	const auto tram_left = static_cast<std::int32_t>(4U + (winner * 40U));
	const auto labels_refresh = (frame & 7U) == 1U;
	if(labels_refresh) {
		if(frame == 1U) {
			result_snapshot_ = state.winner_stats;
			std::uint32_t total =
				(static_cast<std::uint32_t>(result_snapshot_.combo_hits_max) * 1000U) +
				(static_cast<std::uint32_t>(result_snapshot_.gauge_attacks_fired) * 10000U) +
				(static_cast<std::uint32_t>(result_snapshot_.boss_attacks_fired) * 15000U) +
				(static_cast<std::uint32_t>(result_snapshot_.boss_attacks_reversed) * 20000U) +
				(static_cast<std::uint32_t>(result_snapshot_.boss_panics_fired) * 30000U);
			if(state.story_stage == 8U) {
				result_final_lives_ = state.story_lives;
				total += static_cast<std::uint32_t>(result_final_lives_) * 100000U;
			}
			result_initial_remainder_ = static_cast<std::uint16_t>(total % 10000U);
			result_initial_quotient_ = static_cast<std::uint16_t>(total / 10000U);
		}

		const auto &labels = tram_assets_.result_labels;
		const auto label = [&labels](MainResultLabel id) noexcept {
			return labels[static_cast<std::size_t>(id)];
		};
		if(!put_cells(*tram_, {tram_left, 8}, label(MainResultLabel::max_combo)) ||
			!put_cells(*tram_, {tram_left, 10}, label(MainResultLabel::gauge_attack)) ||
			!put_cells(*tram_, {tram_left, 12}, label(MainResultLabel::boss_attack)) ||
			!put_cells(*tram_, {tram_left, 14}, label(MainResultLabel::boss_attack_reversed)) ||
			!put_cells(*tram_, {tram_left, 16}, label(MainResultLabel::boss_panic)) ||
			!put_cells(*tram_, {tram_left, 20}, label(MainResultLabel::total)) ||
			!put_cells(
				*tram_,
				{tram_left, 6},
				label((state.story_stage < 8U) ? MainResultLabel::winner_bonus :
					MainResultLabel::all_clear_bonus)
			) || (state.story_stage >= 8U && !put_cells(
				*tram_, {tram_left, 18}, label(MainResultLabel::player_remaining)
			)) ||
			!put_gaiji_value(*tram_, tram_left, 32, result_snapshot_.combo_hits_max) ||
			!put_gaiji_value(*tram_, tram_left, 64, result_snapshot_.gauge_attacks_fired) ||
			!put_gaiji_value(*tram_, tram_left, 96, result_snapshot_.boss_attacks_fired) ||
			!put_gaiji_value(*tram_, tram_left, 128, result_snapshot_.boss_attacks_reversed) ||
			!put_gaiji_value(*tram_, tram_left, 160, result_snapshot_.boss_panics_fired) ||
			(state.story_stage == 8U && !put_gaiji_value(
				*tram_, tram_left, 192, result_final_lives_
			))) {
			status_ = MainHudResultStatus::draw_failed;
			return;
		}
	}

	const auto pixel_left = tram_left * 8;
	if(!points_put(*raster_, score_font_, pixel_left + 216, 64, 1000U, 15U, true) ||
		!points_put(*raster_, score_font_, pixel_left + 216, 80, 10000U, 15U, true) ||
		!points_put(*raster_, score_font_, pixel_left + 216, 96, 15000U, 15U, true) ||
		!points_put(*raster_, score_font_, pixel_left + 216, 112, 20000U, 15U, true) ||
		!points_put(*raster_, score_font_, pixel_left + 216, 128, 30000U, 15U, true) ||
		(state.story_stage == 8U &&
		 (!points_put(*raster_, score_font_, pixel_left + 208, 144, 10000U, 15U, true) ||
		  !points_put(*raster_, score_font_, pixel_left + 216, 144, 0U, 15U, true)))) {
		status_ = MainHudResultStatus::draw_failed;
		return;
	}
	if(result_initial_remainder_ != 0U) {
		if(!points_put(
			*raster_, score_font_, pixel_left + 216, 160,
			result_initial_remainder_, 12U, true
		)) {
			status_ = MainHudResultStatus::draw_failed;
			return;
		}
	} else if(!points_put(*raster_, score_font_, pixel_left + 216, 160, 0U, 12U, true) ||
		!points_put(*raster_, score_font_, pixel_left + 200, 160, 0U, 12U, true) ||
		!points_put(*raster_, score_font_, pixel_left + 192, 160, 0U, 12U, true)) {
		status_ = MainHudResultStatus::draw_failed;
		return;
	}
	if(!points_put(
		*raster_, score_font_, pixel_left + 184, 160,
		result_initial_quotient_, 12U, false
	)) {
		status_ = MainHudResultStatus::draw_failed;
	}
}

void MainHudResultPresenter::main_score_state(const MainScoreState &state) noexcept
{
	if(!ready()) {
		return;
	}
	for(const auto &player : state.digits) {
		for(const auto digit : player) {
			if(digit > 9U) {
				status_ = MainHudResultStatus::invalid_event_sequence;
				return;
			}
		}
	}
	if(!draw_score_digits(*raster_, score_font_, state.digits[1U], 544) ||
		!score_digit_put(*raster_, 608, 9, score_font_, 0U, 15U) ||
		!draw_score_digits(*raster_, score_font_, state.digits[0U], 224) ||
		!score_digit_put(
			*raster_, 288, 9, score_font_, continues_used_digit_, 15U
		)) {
		status_ = MainHudResultStatus::draw_failed;
	}
}

void MainHudResultPresenter::main_combo_state(const MainComboState &state) noexcept
{
	if(!ready()) {
		return;
	}
	for(const auto &combo : state.players) {
		if(combo.time > combo_frames ||
			(combo.time == combo_frames && combo.hits_highest > 99U)) {
			status_ = MainHudResultStatus::invalid_event_sequence;
			return;
		}
	}
	for(std::size_t player = 0U; player < state.players.size(); ++player) {
		const auto &combo = state.players[player];
		if(combo.time == 0U) {
			continue;
		}
		const auto tram_left = static_cast<std::int32_t>(4U + (player * 40U));
		if(combo.time == combo_frames) {
			const auto tens = static_cast<std::uint8_t>(combo.hits_highest / 10U);
			const auto ones = static_cast<std::uint8_t>(combo.hits_highest % 10U);
			if(!put_cells(
				*tram_, {tram_left + 4, 2}, tram_assets_.combo_hit
			) || !put_cells(
				*tram_, {tram_left, 3}, tram_assets_.combo_bonus
			) || !tram_->put_gaiji(
				{tram_left, 2},
				(combo.hits_highest >= 10U) ?
					static_cast<std::uint8_t>(gaiji_bold_digit_0 + tens) : gaiji_space,
				tram_white
			) || !tram_->put_gaiji(
				{tram_left + 2, 2},
				static_cast<std::uint8_t>(gaiji_bold_digit_0 + ones),
				tram_white
			)) {
				status_ = MainHudResultStatus::draw_failed;
				return;
			}
		}
		const auto post_time = static_cast<std::uint8_t>(combo.time - 1U);
		const auto color = static_cast<Index4>(
			(post_time < combo_hit_reset_frames &&
			 (state.context.round_or_result_frame & 3U) < 2U) ? 8U : 12U
		);
		if(!points_put(
			*raster_,
			score_font_,
			static_cast<std::int32_t>(40U + (player * 320U)),
			24,
			combo.bonus_total,
			color,
			true
		) || (post_time == 0U &&
			(!put_cells(*tram_, {tram_left, 2}, tram_assets_.combo_erase) ||
			 !put_cells(*tram_, {tram_left, 3}, tram_assets_.combo_erase)))) {
			status_ = MainHudResultStatus::draw_failed;
			return;
		}
	}
}

void MainHudResultPresenter::main_result_transition_state(
	const MainResultTransitionState &state
) noexcept
{
	if(!ready() || state.phase == MainResultTransitionPhase::idle) {
		return;
	}
	if((state.phase != MainResultTransitionPhase::opening &&
		state.phase != MainResultTransitionPhase::closing) ||
		state.frame_before >= 16U) {
		status_ = MainHudResultStatus::invalid_event_sequence;
		return;
	}
	if(state.phase == MainResultTransitionPhase::opening) {
		if(state.frame_before == 0U) {
			transition_coordinates_ = {{303, 382, 16, 382, 16, 16, 303, 16}};
		}
		transition_coordinates_[0U] = wrap_word(transition_coordinates_[0U] - 18);
		transition_coordinates_[3U] = wrap_word(transition_coordinates_[3U] - 23);
		transition_coordinates_[4U] = wrap_word(transition_coordinates_[4U] + 18);
		transition_coordinates_[7U] = wrap_word(transition_coordinates_[7U] + 23);
		for(const auto offset : {0, 320}) {
			if(!draw_transition_triangle(
				*raster_,
				{static_cast<std::int16_t>(offset + 303), 8},
				{static_cast<std::int16_t>(offset + 303), 191},
				{wrap_word(offset + transition_coordinates_[0U]),
				 static_cast<std::int16_t>(transition_coordinates_[1U] / 2)},
				transition_opening_clip,
				0x5555U
			) || !draw_transition_triangle(
				*raster_,
				{static_cast<std::int16_t>(offset + 16), 8},
				{static_cast<std::int16_t>(offset + 16), 191},
				{wrap_word(offset + transition_coordinates_[4U]),
				 static_cast<std::int16_t>(transition_coordinates_[5U] / 2)},
				transition_opening_clip,
				0x5555U
			)) {
				status_ = MainHudResultStatus::draw_failed;
				return;
			}
		}
		for(const auto offset : {0, 320}) {
			if(!draw_transition_triangle(
				*raster_,
				{static_cast<std::int16_t>(offset + 16), 191},
				{static_cast<std::int16_t>(offset + 303), 191},
				{wrap_word(offset + transition_coordinates_[2U]),
				 static_cast<std::int16_t>(transition_coordinates_[3U] / 2)},
				transition_opening_clip,
				0xAAAAU
			) || !draw_transition_triangle(
				*raster_,
				{static_cast<std::int16_t>(offset + 16), 8},
				{static_cast<std::int16_t>(offset + 303), 8},
				{wrap_word(offset + transition_coordinates_[6U]),
				 static_cast<std::int16_t>(transition_coordinates_[7U] / 2)},
				transition_opening_clip,
				0xAAAAU
			)) {
				status_ = MainHudResultStatus::draw_failed;
				return;
			}
		}
		if(state.frame_before == 15U) {
			const auto tile = GrcgTile::solid(2U);
			if(!tile || !grcg_boxfill(
				*raster_, {16, 0}, {303, 183}, full_zoomed_clip, *tile
			) || !grcg_boxfill(
				*raster_, {336, 0}, {623, 183}, full_zoomed_clip, *tile
			)) {
				status_ = MainHudResultStatus::draw_failed;
			}
		}
		return;
	}

	if(state.frame_before == 0U) {
		transition_coordinates_ = {{303, 16, 303, 382, 16, 382, 16, 16}};
	}
	transition_coordinates_[1U] = wrap_word(transition_coordinates_[1U] + 23);
	transition_coordinates_[2U] = wrap_word(transition_coordinates_[2U] - 18);
	transition_coordinates_[5U] = wrap_word(transition_coordinates_[5U] - 23);
	transition_coordinates_[6U] = wrap_word(transition_coordinates_[6U] + 18);
	for(const auto offset : {0, 320}) {
		if(!draw_transition_triangle(
			*raster_,
			{static_cast<std::int16_t>(offset + 16), 191},
			{static_cast<std::int16_t>(offset + 303), 191},
			{wrap_word(offset + transition_coordinates_[0U]),
			 static_cast<std::int16_t>(transition_coordinates_[1U] / 2)},
			transition_closing_clip,
			0xFFFFU
		) || !draw_transition_triangle(
			*raster_,
			{static_cast<std::int16_t>(offset + 16), 8},
			{static_cast<std::int16_t>(offset + 16), 191},
			{wrap_word(offset + transition_coordinates_[2U]),
			 static_cast<std::int16_t>(transition_coordinates_[3U] / 2)},
			transition_closing_clip,
			0xFFFFU
		) || !draw_transition_triangle(
			*raster_,
			{static_cast<std::int16_t>(offset + 16), 8},
			{static_cast<std::int16_t>(offset + 303), 8},
			{wrap_word(offset + transition_coordinates_[4U]),
			 static_cast<std::int16_t>(transition_coordinates_[5U] / 2)},
			transition_closing_clip,
			0xFFFFU
		) || !draw_transition_triangle(
			*raster_,
			{static_cast<std::int16_t>(offset + 303), 8},
			{static_cast<std::int16_t>(offset + 303), 191},
			{wrap_word(offset + transition_coordinates_[6U]),
			 static_cast<std::int16_t>(transition_coordinates_[7U] / 2)},
			transition_closing_clip,
			0xFFFFU
		)) {
			status_ = MainHudResultStatus::draw_failed;
			return;
		}
	}
}

} // namespace th03::present
