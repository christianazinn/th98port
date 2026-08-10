#include "th03/present/main_hud_result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "th03/present/main_static.hpp"
#include "th03/present/vectors.hpp"

namespace th03::present {
namespace {

constexpr std::size_t hud_particle_count = 128U;
constexpr std::size_t hud_reads_per_particle = 4U;
constexpr std::size_t hud_compatibility_read_count =
	hud_particle_count * hud_reads_per_particle;
constexpr std::uint8_t hud_flying_story_base = 22U;
constexpr std::uint8_t hud_flying_versus_base = 26U;
constexpr std::uint8_t hud_settled_base = 30U;
constexpr GrcgClipRect full_zoomed_clip{0, 0, 639, 199};

[[nodiscard]] constexpr std::int16_t wrap_word(std::int32_t value) noexcept
{
	return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr std::int32_t arithmetic_shift_right_8(
	std::int32_t value
) noexcept
{
	return (value >= 0) ? (value / 256) : ((value - 255) / 256);
}

[[nodiscard]] constexpr std::int16_t polar_delta(
	std::int16_t ratio
) noexcept
{
	return wrap_word(arithmetic_shift_right_8(
		static_cast<std::int32_t>(80) * ratio
	));
}

[[nodiscard]] constexpr bool valid_sheet_range(
	const PreparedSuperimposeSheet *sheet,
	std::uint16_t last_code
) noexcept
{
	return sheet != nullptr && sheet->first_code() == 0U &&
		sheet->last_code() == last_code && sheet->pixel_width() == 32U &&
		sheet->pixel_height() == 32U;
}

[[nodiscard]] bool valid_cells(
	std::span<const TramCell> cells,
	std::size_t expected_size,
	bool spaces_only = false
) noexcept
{
	if(cells.size() != expected_size) {
		return false;
	}
	for(const auto &cell : cells) {
		if(cell.attribute != tram_white || (spaces_only && cell.code != ' ')) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool valid_tram_assets(
	const MainHudResultTramAssets &assets
) noexcept
{
	for(std::size_t index = 0U; index < assets.result_labels.size(); ++index) {
		const auto label = static_cast<MainResultLabel>(index);
		const auto expected =
			(label == MainResultLabel::winner_bonus ||
			 label == MainResultLabel::all_clear_bonus) ? 32U : 26U;
		if(!valid_cells(assets.result_labels[index], expected)) {
			return false;
		}
	}
	return valid_cells(assets.combo_hit, 4U) &&
		valid_cells(assets.combo_bonus, 8U) &&
		valid_cells(assets.combo_erase, 8U, true);
}

[[nodiscard]] constexpr bool valid_player(PlayerId player) noexcept
{
	return player == PlayerId::p1 || player == PlayerId::p2;
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

[[nodiscard]] bool pattern_exists(
	const MainHudResultSheets &sheets,
	std::uint8_t global_pattern
) noexcept
{
	const auto resolved = resolve_pattern(sheets, global_pattern);
	return resolved.sheet != nullptr &&
		resolved.code >= resolved.sheet->first_code() &&
		resolved.code <= resolved.sheet->last_code();
}

[[nodiscard]] bool put_pattern(
	Raster &raster,
	const MainHudResultSheets &sheets,
	Point point,
	std::uint8_t global_pattern
) noexcept
{
	const auto resolved = resolve_pattern(sheets, global_pattern);
	return resolved.sheet != nullptr &&
		superimpose_put(raster, *resolved.sheet, point, resolved.code);
}

[[nodiscard]] bool put_pattern_row(
	Raster &raster,
	const MainHudResultSheets &sheets,
	Point point,
	std::uint8_t global_pattern,
	std::uint8_t row
) noexcept
{
	const auto resolved = resolve_pattern(sheets, global_pattern);
	return resolved.sheet != nullptr && superimpose_put_blue_plane_row_white(
		raster, *resolved.sheet, point, resolved.code, row
	);
}

} // namespace

MainHudResultPresenter::MainHudResultPresenter(
	Raster &raster,
	Tram &tram,
	ScoreFontMasks score_font,
	StaffFlakeTrigTables trig,
	MainHudResultSheets sheets,
	MainHudResultTramAssets tram_assets,
	std::uint8_t continues_used_digit
) noexcept :
	raster_(&raster),
	tram_(&tram),
	score_font_(score_font),
	trig_(trig),
	sheets_(sheets),
	tram_assets_(tram_assets),
	continues_used_digit_(continues_used_digit)
{
	if(score_font_.rows.size() != score_font_mask_bytes ||
		score_font_.row_order != PackedRowOrder::bottom_to_top ||
		trig_.sine_q8.size() != 256U || trig_.cosine_q8.size() != 256U ||
		!valid_sheet_range(sheets_.lose, 7U) ||
		!valid_sheet_range(sheets_.round, 10U) ||
		!valid_sheet_range(sheets_.zikicw, 7U) ||
		!valid_tram_assets(tram_assets_) || continues_used_digit_ > 3U) {
		status_ = MainHudResultStatus::invalid_assets;
	}
}

MainHudResultStatus MainHudResultPresenter::status() const noexcept
{
	return status_;
}

bool MainHudResultPresenter::ready() const noexcept
{
	return status_ == MainHudResultStatus::ready;
}

void MainHudResultPresenter::hud_start_frame_step(
	const HudStartFrameStep &step
) noexcept
{
	if(!ready()) {
		return;
	}
	if(step.state_before > 4U ||
		(step.initialize && (step.state_before != 0U || step.frame_before != 0U)) ||
		(step.state_before == 0U && step.frame_before >= 69U) ||
		(step.state_before == 1U && step.frame_before > 0x56U) ||
		(step.state_before == 2U && step.frame_before != 0U) ||
		(step.state_before == 3U && step.frame_before >= 64U) ||
		hud_particle_reads_pending_ ||
		((hud_completed_batch_waiting_ || hud_late_state_pending_) &&
		 !step.initialize)) {
		status_ = MainHudResultStatus::invalid_event_sequence;
		return;
	}
	if(step.initialize) {
		hud_completed_batch_waiting_ = false;
		hud_late_state_pending_ = false;
		for(std::size_t index = 0U; index < hud_banner_.size(); ++index) {
			auto &record = hud_banner_[index];
			record.x = static_cast<std::int16_t>(-288 +
				(static_cast<std::int32_t>(index) * 8));
			record.y = static_cast<std::int16_t>(84 + index);
			record.velocity = 8;
			record.active = 1U;
		}
		hud_pattern_base_ = step.story_mode ?
			hud_flying_story_base : hud_flying_versus_base;
		hud_banner_x_ = 0;
		hud_compatibility_read_count_ = 0U;
	}

	switch(step.state_before) {
	case 0U:
		for(auto &record : hud_banner_) {
			if(record.active == 0U) {
				continue;
			}
			record.x = wrap_word(record.x + record.velocity);
			if(record.active == 1U) {
				if(record.x >= 96) {
					record.active = 2U;
				}
			} else if(record.x > 64) {
				record.velocity = wrap_word(record.velocity - 1);
			} else {
				record.x = 64;
				record.active = 0U;
			}
		}
		break;

	case 1U:
		break;

	case 2U:
		for(std::size_t index = 0U; index < hud_particles_.size(); ++index) {
			auto &particle = hud_particles_[index];
			const auto half_x = (index < 64U) ? 96 : 416;
			const auto ordinal = static_cast<std::int32_t>(index % 64U);
			particle.x_q4 = static_cast<std::int16_t>(
				(half_x << 4U) + (ordinal * 0x20)
			);
			particle.active = 1U;
		}
		hud_compatibility_read_count_ = 0U;
		hud_particle_reads_pending_ = true;
		break;

	case 3U: {
		const auto frame_after = static_cast<std::uint16_t>(
			step.frame_before + 1U
		);
		for(auto &particle : hud_particles_) {
			if(particle.active == 0U) {
				continue;
			}
			particle.x_q4 = wrap_word(
				particle.x_q4 + polar_delta(trig_.cosine_q8[particle.angle])
			);
			particle.y_q4 = wrap_word(
				particle.y_q4 + polar_delta(trig_.sine_q8[particle.angle])
			);
			if(particle.x_q4 < 0 || particle.x_q4 >= (624 << 4U) ||
				particle.y_q4 < 0 || particle.y_q4 >= (384 << 4U)) {
				particle.active = 0U;
				continue;
			}
			const auto delta = (frame_after < 32U) ?
				particle.angle_delta_first : particle.angle_delta_second;
			particle.angle = static_cast<std::uint8_t>(
				particle.angle + ((static_cast<std::int16_t>(delta) - 1) * 4)
			);
		}
		break;
	}

	default:
		break;
	}

	hud_expected_story_mode_ = step.story_mode;
	hud_expected_story_stage_ = step.story_stage;
	hud_expected_round_id_ = step.round_id;
	hud_expected_state_after_ = step.state_before;
	hud_expected_frame_after_ = step.frame_before;
	switch(step.state_before) {
	case 0U:
		hud_expected_frame_after_ = static_cast<std::uint16_t>(
			step.frame_before + 1U
		);
		if((hud_banner_[0U].active == 0U) !=
			(hud_expected_frame_after_ >= 69U)) {
			status_ = MainHudResultStatus::invalid_event_sequence;
			return;
		}
		if(hud_banner_[0U].active == 0U) {
			hud_expected_state_after_ = 1U;
			hud_expected_frame_after_ = 0U;
		}
		break;
	case 1U:
		hud_expected_frame_after_ = static_cast<std::uint16_t>(
			step.frame_before + 1U
		);
		if(hud_expected_frame_after_ > 0x56U) {
			hud_expected_state_after_ = 2U;
			hud_expected_frame_after_ = 0U;
		}
		break;
	case 2U:
		hud_expected_state_after_ = 3U;
		hud_expected_frame_after_ = 0U;
		break;
	case 3U:
		hud_expected_frame_after_ = static_cast<std::uint16_t>(
			step.frame_before + 1U
		);
		if(hud_expected_frame_after_ >= 64U) {
			hud_expected_state_after_ = 4U;
		}
		break;
	case 4U:
		break;
	default:
		break;
	}
	hud_late_state_pending_ = true;
}

void MainHudResultPresenter::hud_start_compatibility_read(
	const HudStartCompatibilityRead &read
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!hud_particle_reads_pending_ ||
		hud_compatibility_read_count_ >= hud_compatibility_read_count) {
		status_ = MainHudResultStatus::invalid_event_sequence;
		return;
	}
	const auto expected_particle = static_cast<std::uint8_t>(
		hud_compatibility_read_count_ / hud_reads_per_particle
	);
	const auto expected_site = static_cast<HudStartCompatibilityReadSite>(
		hud_compatibility_read_count_ % hud_reads_per_particle
	);
	if(read.particle_ordinal != expected_particle || read.site != expected_site) {
		status_ = MainHudResultStatus::invalid_event_sequence;
		return;
	}
	auto &particle = hud_particles_[expected_particle];
	switch(read.site) {
	case HudStartCompatibilityReadSite::particle_y:
		particle.y_q4 = static_cast<std::int16_t>(
			(read.raw_value & 0x01FFU) + (168U << 4U)
		);
		break;
	case HudStartCompatibilityReadSite::particle_angle:
		particle.angle = static_cast<std::uint8_t>(read.raw_value);
		break;
	case HudStartCompatibilityReadSite::angle_delta_first:
		particle.angle_delta_first = static_cast<std::int8_t>(
			static_cast<std::int16_t>(read.raw_value) % 3
		);
		break;
	case HudStartCompatibilityReadSite::angle_delta_second:
		particle.angle_delta_second = static_cast<std::int8_t>(
			static_cast<std::int16_t>(read.raw_value) % 3
		);
		break;
	}
	++hud_compatibility_read_count_;
	if(hud_compatibility_read_count_ == hud_compatibility_read_count) {
		hud_particle_reads_pending_ = false;
		hud_completed_batch_waiting_ = true;
	}
}

void MainHudResultPresenter::main_hud_start_state(
	const MainHudStartState &state
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!hud_late_state_pending_ || state.state_after_prefix > 4U ||
		hud_particle_reads_pending_ ||
		state.story_mode != hud_expected_story_mode_ ||
		state.story_stage != hud_expected_story_stage_ ||
		state.round_id != hud_expected_round_id_ ||
		state.state_after_prefix != hud_expected_state_after_ ||
		state.frame_after_prefix != hud_expected_frame_after_ ||
		(hud_completed_batch_waiting_ && state.state_after_prefix != 3U)) {
		status_ = MainHudResultStatus::invalid_event_sequence;
		return;
	}
	if(hud_completed_batch_waiting_) {
		hud_completed_batch_waiting_ = false;
	}
	hud_late_state_pending_ = false;

	if(state.state_after_prefix == 0U) {
		for(std::uint8_t pattern = hud_pattern_base_;
			pattern < static_cast<std::uint8_t>(hud_pattern_base_ + 4U);
			++pattern) {
			if(!pattern_exists(sheets_, pattern)) {
				status_ = MainHudResultStatus::invalid_event_sequence;
				return;
			}
		}
		for(std::uint8_t row = 0U; row < hud_banner_.size(); ++row) {
			const auto &record = hud_banner_[row];
			const std::array<std::int32_t, 8U> x{{
				record.x,
				record.x + 32,
				record.x + 64,
				record.x + 96,
				544 - record.x,
				512 - record.x,
				480 - record.x,
				448 - record.x,
			}};
			const std::array<std::uint8_t, 8U> pattern{{
				hud_pattern_base_,
				static_cast<std::uint8_t>(hud_pattern_base_ + 1U),
				static_cast<std::uint8_t>(hud_pattern_base_ + 2U),
				static_cast<std::uint8_t>(hud_pattern_base_ + 3U),
				static_cast<std::uint8_t>(hud_pattern_base_ + 3U),
				static_cast<std::uint8_t>(hud_pattern_base_ + 2U),
				static_cast<std::uint8_t>(hud_pattern_base_ + 1U),
				hud_pattern_base_,
			}};
			for(std::size_t index = 0U; index < x.size(); ++index) {
				if(!put_pattern_row(
					*raster_, sheets_, {x[index], record.y}, pattern[index], row
				)) {
					status_ = MainHudResultStatus::draw_failed;
					return;
				}
			}
		}
		// word_2087A aliases the X word in the sixteenth 10-byte record.
		hud_banner_x_ = hud_banner_[15U].x;
		return;
	}

	if(state.state_after_prefix == 1U) {
		const auto frame = state.frame_after_prefix;
		const auto show_digit = state.story_mode ?
			(frame >= 24U && frame < 56U) : (frame >= 24U && frame < 64U);
		const auto settle = state.story_mode ? (frame == 56U) : (frame == 64U);
		const auto digit_pattern = static_cast<std::uint16_t>(
			34U + static_cast<std::uint16_t>(
				state.story_mode ? state.story_stage : state.round_id
			)
		);
		if((show_digit && (digit_pattern > 44U ||
			!pattern_exists(sheets_, static_cast<std::uint8_t>(digit_pattern)))) ||
			(!pattern_exists(sheets_, hud_pattern_base_) ||
			 !pattern_exists(sheets_, static_cast<std::uint8_t>(hud_pattern_base_ + 1U)) ||
			 !pattern_exists(sheets_, static_cast<std::uint8_t>(hud_pattern_base_ + 2U)) ||
			 !pattern_exists(sheets_, static_cast<std::uint8_t>(hud_pattern_base_ + 3U)))) {
			status_ = MainHudResultStatus::invalid_event_sequence;
			return;
		}
		if(show_digit && (!put_pattern(
			*raster_, sheets_, {208, 84}, static_cast<std::uint8_t>(digit_pattern)
		) || !put_pattern(
			*raster_, sheets_, {544, 84}, static_cast<std::uint8_t>(digit_pattern)
		))) {
			status_ = MainHudResultStatus::draw_failed;
			return;
		}
		if(settle) {
			hud_banner_x_ = 96;
			hud_pattern_base_ = hud_settled_base;
		}
		for(std::size_t index = 0U; index < 8U; ++index) {
			const auto half = index / 4U;
			const auto column = index % 4U;
			if(!put_pattern(
				*raster_,
				sheets_,
				{
					hud_banner_x_ + static_cast<std::int32_t>(half * 320U + column * 32U),
					84,
				},
				static_cast<std::uint8_t>(hud_pattern_base_ + column)
			)) {
				status_ = MainHudResultStatus::draw_failed;
				return;
			}
		}
		return;
	}

	if(state.state_after_prefix == 3U) {
		for(const auto &particle : hud_particles_) {
			if(particle.active != 0U && !grcg_hline(
				*raster_,
				static_cast<std::int16_t>(particle.x_q4 >> 4U),
				static_cast<std::int16_t>((particle.x_q4 >> 4U) + 1),
				static_cast<std::int16_t>(particle.y_q4 >> 5U),
				full_zoomed_clip,
				15U
			)) {
				status_ = MainHudResultStatus::draw_failed;
				return;
			}
		}
	}
}

void MainHudResultPresenter::main_hud_player_value_changed(
	const MainHudPlayerValueChanged &changed
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!valid_player(changed.player)) {
		status_ = MainHudResultStatus::invalid_event_sequence;
		return;
	}
	const auto player = static_cast<std::size_t>(changed.player);
	bool put = false;
	switch(changed.kind) {
	case MainHudPlayerValueKind::halfhearts:
		put = main_hud_halfhearts_put(*tram_, player, changed.value_after);
		break;
	case MainHudPlayerValueKind::bombs:
		put = main_hud_bombs_put(*tram_, player, changed.value_after);
		break;
	case MainHudPlayerValueKind::rounds_won:
		put = main_hud_rounds_won_put(*tram_, player, changed.value_after);
		break;
	default:
		status_ = MainHudResultStatus::invalid_event_sequence;
		return;
	}
	if(!put) {
		status_ = MainHudResultStatus::invalid_event_sequence;
	}
}

void MainHudResultPresenter::main_hud_story_lives_changed(
	const MainHudStoryLivesChanged &changed
) noexcept
{
	if(ready() && !main_hud_story_lives_put(*tram_, changed.value_after)) {
		status_ = MainHudResultStatus::invalid_event_sequence;
	}
}

} // namespace th03::present
