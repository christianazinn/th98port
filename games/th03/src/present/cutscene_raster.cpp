#include "th03/present/cutscene_raster.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

#include "th03/present/graphics_text.hpp"
#include "th03/present/pi.hpp"

namespace th03::present {
namespace {

[[nodiscard]] CutsceneRasterError raster_error(
	CutsceneRasterErrorCode code
) noexcept {
	return {code, std::nullopt};
}

[[nodiscard]] CutsceneRasterError raster_text_error(
	GraphicsTextError error
) noexcept {
	return {CutsceneRasterErrorCode::invalid_text, error};
}

[[nodiscard]] constexpr bool valid_weight(
	GraphicsTextWeight weight
) noexcept {
	switch(weight) {
	case GraphicsTextWeight::normal:
	case GraphicsTextWeight::heavy:
	case GraphicsTextWeight::bold:
	case GraphicsTextWeight::black:
		return true;
	}
	return false;
}

[[nodiscard]] constexpr bool valid_fade_color(
	CutsceneFadeColor color
) noexcept {
	return color == CutsceneFadeColor::white ||
		color == CutsceneFadeColor::black;
}

[[nodiscard]] constexpr bool valid_fade_direction(
	CutsceneFadeDirection direction
) noexcept {
	return direction == CutsceneFadeDirection::in ||
		direction == CutsceneFadeDirection::out;
}

[[nodiscard]] constexpr bool valid_rgb4(assets::Rgb4 color) noexcept {
	return color.r <= 0x0FU && color.g <= 0x0FU && color.b <= 0x0FU;
}

[[nodiscard]] bool valid_background_image(
	const assets::PiImage &image
) noexcept {
	if(image.image.width != graphics_width ||
		image.image.height != graphics_height ||
		image.image.indices.size() != graphics_page_pixels) {
		return false;
	}
	if(std::ranges::any_of(image.image.indices, [](std::uint8_t color) {
		return color > 0x0FU;
	})) {
		return false;
	}
	return !image.palette || std::ranges::all_of(
		image.palette->colors,
		[](assets::Rgb4 color) { return valid_rgb4(color); }
	);
}

[[nodiscard]] constexpr std::optional<Page> page_from_raw(
	std::uint16_t page
) noexcept {
	if(page == 0U) {
		return Page::zero;
	}
	if(page == 1U) {
		return Page::one;
	}
	return std::nullopt;
}

[[nodiscard]] constexpr std::optional<CutscenePiQuarter> quarter_from_raw(
	std::uint16_t quarter
) noexcept {
	switch(quarter) {
	case 0U:
		return CutscenePiQuarter::top_left;
	case 1U:
		return CutscenePiQuarter::top_right;
	case 2U:
		return CutscenePiQuarter::bottom_left;
	case 3U:
		return CutscenePiQuarter::bottom_right;
	default:
		return std::nullopt;
	}
}

[[nodiscard]] constexpr std::optional<CutscenePiMaskStep> mask_from_step(
	CutsceneCrossfadeStep step
) noexcept {
	switch(step) {
	case CutsceneCrossfadeStep::mask_first:
		return CutscenePiMaskStep::first;
	case CutsceneCrossfadeStep::mask_second:
		return CutscenePiMaskStep::second;
	case CutsceneCrossfadeStep::mask_third:
		return CutscenePiMaskStep::third;
	case CutsceneCrossfadeStep::mask_fourth:
		return CutscenePiMaskStep::fourth;
	case CutsceneCrossfadeStep::complete:
		return std::nullopt;
	}
	return std::nullopt;
}

} // namespace

CutsceneRasterPresenter::CutsceneRasterPresenter(
	Raster &raster,
	const AnkGlyphProvider &font,
	const Tram &gaiji
) noexcept :
	raster_(&raster),
	font_(&font),
	gaiji_(&gaiji)
{}

std::expected<CutsceneRasterPresenter, CutsceneRasterError>
CutsceneRasterPresenter::create(
	Raster &raster,
	const AnkGlyphProvider &font,
	const Tram &gaiji
) noexcept {
	if(raster.raster_mode() != RasterMode::lines_400) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_raster_mode
		));
	}
	return CutsceneRasterPresenter(raster, font, gaiji);
}

bool CutsceneRasterPresenter::valid_raster_mode() const noexcept {
	return raster_ && raster_->raster_mode() == RasterMode::lines_400;
}

std::expected<CutsceneRasterDispatch, CutsceneRasterError>
CutsceneRasterPresenter::execute(const CutsceneOperation &operation) {
	return std::visit([this](const auto &typed) -> std::expected<
		CutsceneRasterDispatch,
		CutsceneRasterError
	> {
		using T = std::remove_cvref_t<decltype(typed)>;
		if constexpr(std::is_same_v<T, CutsceneDrawGlyph>) {
			auto result = draw_glyph(typed);
			if(!result) {
				return std::unexpected(result.error());
			}
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneDrawGaiji>) {
			auto result = draw_gaiji(typed);
			if(!result) {
				return std::unexpected(result.error());
			}
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneRestoreBox>) {
			auto result = restore_box();
			if(!result) {
				return std::unexpected(result.error());
			}
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneSnapshotBox>) {
			auto result = snapshot_box();
			if(!result) {
				return std::unexpected(result.error());
			}
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneReleaseBox>) {
			release_box();
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneSetTextColor>) {
			if(typed.color > 0x0FU) {
				return std::unexpected(raster_error(
					CutsceneRasterErrorCode::invalid_operation
				));
			}
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneSetTextWeight>) {
			if(!valid_weight(typed.weight)) {
				return std::unexpected(raster_error(
					CutsceneRasterErrorCode::invalid_operation
				));
			}
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneSetTextInterval>) {
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneShowPage>) {
			const auto page = page_from_raw(typed.page);
			if(!valid_raster_mode() || !page || !raster_->set_shown_page(*page)) {
				return std::unexpected(raster_error(
					CutsceneRasterErrorCode::invalid_operation
				));
			}
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneSetTone>) {
			if(!valid_raster_mode() || typed.tone > 999U) {
				return std::unexpected(raster_error(
					CutsceneRasterErrorCode::invalid_operation
				));
			}
			raster_->set_palette_tone(static_cast<std::int16_t>(typed.tone));
			raster_->apply_pending_palette();
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutscenePaletteFade>) {
			auto result = begin_palette_fade(typed);
			if(!result) {
				return std::unexpected(result.error());
			}
			return CutsceneRasterDispatch::palette_fade_required;
		} else if constexpr(std::is_same_v<T, CutsceneShake>) {
			return CutsceneRasterDispatch::shake_required;
		} else if constexpr(std::is_same_v<T, CutsceneClearPages>) {
			auto result = clear_pages(typed);
			if(!result) {
				return std::unexpected(result.error());
			}
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneLoadBackground>) {
			return CutsceneRasterDispatch::background_required;
		} else if constexpr(std::is_same_v<T, CutscenePresentBackground>) {
			auto result = present_background(typed);
			if(!result) {
				return std::unexpected(result.error());
			}
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneFreeBackground>) {
			background_.reset();
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneApplyBackgroundPalette>) {
			auto result = apply_background_palette();
			if(!result) {
				return std::unexpected(result.error());
			}
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneShowPictureQuarter>) {
			auto result = show_picture_quarter(typed);
			if(!result) {
				return std::unexpected(result.error());
			}
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneClearPicture>) {
			auto result = clear_picture();
			if(!result) {
				return std::unexpected(result.error());
			}
			return CutsceneRasterDispatch::applied;
		} else if constexpr(std::is_same_v<T, CutsceneCrossfadePicture>) {
			if(!background_) {
				return std::unexpected(raster_error(
					CutsceneRasterErrorCode::missing_background
				));
			}
			if(!quarter_from_raw(typed.quarter)) {
				return std::unexpected(raster_error(
					CutsceneRasterErrorCode::invalid_picture_quarter
				));
			}
			return CutsceneRasterDispatch::crossfade_required;
		} else if constexpr(
			std::is_same_v<T, CutsceneWait> ||
			std::is_same_v<T, CutsceneAudioFade> ||
			std::is_same_v<T, CutsceneBgm> ||
			std::is_same_v<T, CutscenePlaySoundEffect> ||
			std::is_same_v<T, CutsceneFinish>
		) {
			return CutsceneRasterDispatch::external;
		} else {
			static_assert(std::is_same_v<T, void>, "unhandled cutscene operation");
		}
	}, operation);
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::replace_background(assets::PiImage image) {
	if(!valid_background_image(image)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_background
		));
	}
	background_ = std::move(image);
	return {};
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::draw_glyph(const CutsceneDrawGlyph &operation) {
	if(!valid_raster_mode() || operation.color > 0x0FU ||
		!valid_weight(operation.weight)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_operation
		));
	}
	const auto prepared = prepare_graphics_text_8x16(
		std::span<const std::uint8_t>(operation.shift_jis),
		*font_
	);
	if(!prepared) {
		return std::unexpected(raster_text_error(prepared.error()));
	}
	const auto right = static_cast<std::int64_t>(operation.position.x) +
		prepared->pixel_width();
	if(operation.position.x < 0 || operation.position.y < 0 ||
		right > graphics_width ||
		operation.position.y >
			static_cast<std::int32_t>(graphics_height - jis_height)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_operation
		));
	}
	const GraphicsTextStyle style{
		static_cast<Index4>(operation.color),
		operation.weight,
		0U,
	};
	if(!raster_->set_access_page(Page::one) ||
		!graphics_text_put_8x16(
			*raster_,
			{operation.position.x, operation.position.y},
			*prepared,
			style
		) || !raster_->set_access_page(Page::zero) ||
		!graphics_text_put_8x16(
			*raster_,
			{operation.position.x, operation.position.y},
			*prepared,
			style
		)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::draw_failed
		));
	}
	return {};
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::draw_gaiji(
	const CutsceneDrawGaiji &operation
) noexcept {
	if(!valid_raster_mode() || operation.code > 0xFFU ||
		operation.color > 0x0FU || operation.position.x < 0 ||
		(operation.position.x & 7) != 0 || operation.position.y < 0 ||
		operation.position.x >
			static_cast<std::int32_t>(graphics_width - gaiji_width) ||
		operation.position.y >
			static_cast<std::int32_t>(graphics_height - gaiji_height)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_operation
		));
	}
	const auto tile = GrcgTile::solid(static_cast<Index4>(operation.color));
	if(!tile) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_operation
		));
	}
	const auto &glyph = gaiji_->gaiji(static_cast<std::uint8_t>(operation.code));
	const BitSource source{
		glyph.row_halves,
		gaiji_width,
		gaiji_height,
		gaiji_bytes_per_row,
		PackedRowOrder::top_to_bottom,
	};
	if(!raster_->set_access_page(Page::one) || !raster_->grcg_write(
		source,
		{operation.position.x, operation.position.y},
		GrcgWriteMode::read_modify_write,
		*tile
	) || !raster_->set_access_page(Page::zero) || !raster_->grcg_write(
		source,
		{operation.position.x, operation.position.y},
		GrcgWriteMode::read_modify_write,
		*tile
	)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::draw_failed
		));
	}
	return {};
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::snapshot_box() noexcept {
	if(!valid_raster_mode() || !raster_->set_access_page(Page::zero)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::draw_failed
		));
	}
	const auto page = raster_->graphics(Page::zero);
	for(std::size_t y = 0U; y < box_height; ++y) {
		const auto source =
			((static_cast<std::size_t>(box_top) + y) * graphics_width) +
			static_cast<std::size_t>(box_left);
		std::copy_n(
			page.begin() + static_cast<std::ptrdiff_t>(source),
			box_width,
			box_snapshot_.begin() +
				static_cast<std::ptrdiff_t>(y * box_width)
		);
	}
	box_snapshotted_ = true;
	return {};
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::restore_box() noexcept {
	if(!box_snapshotted_) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::box_not_snapshotted
		));
	}
	if(!valid_raster_mode()) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_raster_mode
		));
	}
	const IndexedSource source{
		box_snapshot_,
		box_width,
		box_height,
		box_width,
	};
	if(!raster_->set_access_page(Page::one) ||
		!raster_->blit(source, {box_left, box_top}) ||
		!raster_->set_access_page(Page::zero) ||
		!raster_->blit(source, {box_left, box_top})) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::draw_failed
		));
	}
	return {};
}

void CutsceneRasterPresenter::release_box() noexcept {
	box_snapshotted_ = false;
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::clear_pages(
	const CutsceneClearPages &operation
) noexcept {
	if(!valid_raster_mode() || operation.snapshot_box_after_clear) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_operation
		));
	}
	const Rect page{0, 0, graphics_width, graphics_height};
	if(!raster_->set_access_page(Page::one) || !raster_->clear(page, 0U) ||
		!raster_->set_access_page(Page::zero) || !raster_->clear(page, 0U)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::draw_failed
		));
	}
	return {};
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::apply_background_palette() noexcept {
	if(!background_) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::missing_background
		));
	}
	if(!background_->palette) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::missing_background_palette
		));
	}
	for(std::size_t index = 0U; index < background_->palette->colors.size(); ++index) {
		if(!raster_->set_pending_palette_entry(
			static_cast<std::uint8_t>(index),
			background_->palette->colors[index]
		)) {
			return std::unexpected(raster_error(
				CutsceneRasterErrorCode::draw_failed
			));
		}
	}
	raster_->apply_pending_palette();
	return {};
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::present_background(
	const CutscenePresentBackground &operation
) {
	if(!valid_raster_mode()) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_raster_mode
		));
	}
	if(!background_) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::missing_background
		));
	}
	if(operation.apply_palette) {
		auto applied = apply_background_palette();
		if(!applied) {
			return applied;
		}
	}
	if(!raster_->set_access_page(Page::one) ||
		!pi_put_8(*raster_, 0, 0, *background_) ||
		!raster_->copy_page(
			Page::one,
			{0, 0, graphics_width, graphics_height},
			Page::zero,
			{0, 0}
		) || !raster_->set_access_page(Page::zero)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::draw_failed
		));
	}
	return snapshot_box();
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::show_picture_quarter(
	const CutsceneShowPictureQuarter &operation
) {
	if(!background_) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::missing_background
		));
	}
	const auto quarter = quarter_from_raw(operation.quarter);
	if(!quarter) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_picture_quarter
		));
	}
	if(!valid_raster_mode() || !raster_->set_shown_page(Page::one) ||
		!raster_->set_access_page(Page::zero) ||
		!pi_cutscene_quarter_put_8(*raster_, *background_, *quarter) ||
		!raster_->set_shown_page(Page::zero)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::draw_failed
		));
	}
	return copy_picture_to_page_one();
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::clear_picture() {
	if(!valid_raster_mode() || !raster_->set_shown_page(Page::one) ||
		!raster_->set_access_page(Page::zero) || !raster_->clear(
			{cutscene_pi_left, cutscene_pi_top,
				cutscene_pi_width, cutscene_pi_height},
			0U
		) || !raster_->set_shown_page(Page::zero)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::draw_failed
		));
	}
	return copy_picture_to_page_one();
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::copy_picture_to_page_one() {
	if(!raster_->copy_page(
		Page::zero,
		{cutscene_pi_left, cutscene_pi_top,
			cutscene_pi_width, cutscene_pi_height},
		Page::one,
		{cutscene_pi_left, cutscene_pi_top}
	) || !raster_->set_access_page(Page::zero)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::draw_failed
		));
	}
	return {};
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::crossfade_step(
	const CutsceneCrossfadePicture &operation,
	CutsceneCrossfadeStep step
) {
	if(!background_) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::missing_background
		));
	}
	const auto quarter = quarter_from_raw(operation.quarter);
	if(!quarter) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_picture_quarter
		));
	}
	const auto mask = mask_from_step(step);
	if(step != CutsceneCrossfadeStep::complete && !mask) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_crossfade_step
		));
	}
	if(!valid_raster_mode() || !raster_->set_shown_page(Page::one) ||
		!raster_->set_access_page(Page::zero)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::draw_failed
		));
	}
	const bool drawn = (step == CutsceneCrossfadeStep::complete) ?
		pi_cutscene_quarter_put_8(*raster_, *background_, *quarter) :
		pi_cutscene_quarter_put_masked(
			*raster_, *background_, *quarter, *mask
		);
	if(!drawn || !raster_->set_shown_page(Page::zero)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::draw_failed
		));
	}
	return copy_picture_to_page_one();
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::shake_step(
	const CutsceneShake &operation,
	std::uint16_t position_index
) noexcept {
	if(!valid_raster_mode() || position_index > operation.parameter) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_shake_step
		));
	}
	raster_->set_scroll_y(
		((position_index & 1U) != 0U) ? 4U : 396U
	);
	return {};
}

void CutsceneRasterPresenter::finish_shake() noexcept {
	if(raster_) {
		raster_->set_scroll_y(0U);
	}
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::begin_palette_fade(
	const CutscenePaletteFade &operation
) noexcept {
	if(!valid_raster_mode() || !valid_fade_color(operation.color) ||
		!valid_fade_direction(operation.direction)) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_operation
		));
	}
	std::int16_t tone = 100;
	if(operation.direction == CutsceneFadeDirection::in) {
		tone = (operation.color == CutsceneFadeColor::black) ? 0 : 200;
	}
	raster_->set_palette_tone(tone);
	return {};
}

std::expected<void, CutsceneRasterError>
CutsceneRasterPresenter::palette_fade_sample(
	const CutscenePaletteFade &operation,
	std::uint8_t sample_index
) noexcept {
	if(!valid_raster_mode() || !valid_fade_color(operation.color) ||
		!valid_fade_direction(operation.direction) ||
		sample_index >= cutscene_palette_fade_sample_count) {
		return std::unexpected(raster_error(
			CutsceneRasterErrorCode::invalid_palette_fade_sample
		));
	}

	std::uint16_t tone = 0U;
	if(sample_index == (cutscene_palette_fade_sample_count - 1U)) {
		if(operation.direction == CutsceneFadeDirection::in) {
			tone = 100U;
		} else {
			tone = (operation.color == CutsceneFadeColor::white) ? 200U : 0U;
		}
	} else {
		const auto delta = static_cast<std::uint16_t>(sample_index) * 6U;
		if(operation.color == CutsceneFadeColor::black) {
			tone = (operation.direction == CutsceneFadeDirection::in) ?
				static_cast<std::uint16_t>(delta) :
				static_cast<std::uint16_t>(100U - delta);
		} else {
			tone = (operation.direction == CutsceneFadeDirection::in) ?
				static_cast<std::uint16_t>(200U - delta) :
				static_cast<std::uint16_t>(100U + delta);
		}
	}
	raster_->set_palette_tone(static_cast<std::int16_t>(tone));
	raster_->apply_pending_palette();
	return {};
}

bool CutsceneRasterPresenter::has_background() const noexcept {
	return background_.has_value();
}

bool CutsceneRasterPresenter::has_background_palette() const noexcept {
	return background_ && background_->palette.has_value();
}

bool CutsceneRasterPresenter::has_box_snapshot() const noexcept {
	return box_snapshotted_;
}

} // namespace th03::present
