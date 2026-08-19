#include "th03/app/new_classic_pause.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include "th03/app/new_classic_assets.hpp"

namespace th03::app {
namespace {

constexpr std::size_t pause_cell_left = 24U;
constexpr std::size_t pause_cell_top = 8U;
constexpr std::size_t pause_cell_width = 32U;
constexpr std::size_t pause_cell_height = 7U;
constexpr std::size_t pause_choice_mark_left = pause_cell_left + 2U;
constexpr std::size_t pause_gaiji_text_left = pause_cell_left + 4U;
constexpr std::size_t pause_title_left = pause_cell_left + 11U;
constexpr std::size_t pause_title_row = pause_cell_top + 1U;
constexpr std::size_t pause_choice_first_row = pause_cell_top + 2U;

constexpr std::size_t pause_gaiji_title = 0U;
constexpr std::size_t pause_gaiji_resume = 5U;
constexpr std::size_t pause_gaiji_restart = 10U;
constexpr std::size_t pause_gaiji_save_exit = 15U;
constexpr std::size_t pause_gaiji_discard_exit = 28U;
constexpr std::size_t pause_gaiji_cursor = 39U;

constexpr present::Index4 pause_vram_blue = 9U;
constexpr present::Index4 pause_vram_cyan = 10U;
constexpr present::Index4 pause_vram_yellow = 12U;
constexpr present::Index4 pause_vram_white = 15U;

[[nodiscard]] constexpr bool pressed(
	std::uint16_t input,
	std::uint16_t bit
) noexcept
{
	return (input & bit) != 0U;
}

[[nodiscard]] constexpr std::size_t index_of(
	NewClassicPauseChoice choice
) noexcept
{
	return static_cast<std::size_t>(choice);
}

} // namespace

NewClassicPauseInput project_new_classic_pause_input(
	CoreInput input,
	bool restart_requested,
	bool application_quit_requested
) noexcept
{
	return {
		.up = pressed(input.system, 0x0001U),
		.down = pressed(input.system, 0x0002U),
		.confirm = pressed(input.system, 0x2020U),
		.cancel = pressed(input.system, 0x1000U),
		.quit = pressed(input.system, 0x4000U),
		.restart = restart_requested,
		.raw_any = input.system != 0U || input.p1 != 0U || input.p2 != 0U,
		.application_quit = application_quit_requested,
	};
}

NewClassicPauseInput project_new_classic_netplay_pause_input(
	CoreInput input,
	bool application_quit_requested
) noexcept
{
	auto result = project_new_classic_pause_input(
		input, false, application_quit_requested
	);
	result.up = result.up || pressed(input.p1, 0x0001U);
	result.down = result.down || pressed(input.p1, 0x0002U);
	result.confirm = result.confirm || pressed(input.p1, 0x0020U);
	return result;
}

NewClassicPauseMenu::NewClassicPauseMenu(
	bool save_enabled,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	NewClassicPausePresentation presentation,
	std::shared_ptr<const NewClassicPauseNetplayText> netplay_text
) noexcept :
	save_enabled_(save_enabled),
	patch_assets_(std::move(patch_assets)),
	presentation_(presentation),
	netplay_text_(std::move(netplay_text)),
	pixels_(present::graphics_page_pixels)
{}

void NewClassicPauseMenu::move(int delta) noexcept
{
	if(presentation_ == NewClassicPausePresentation::netplay_wait) {
		choice_ = choice_ == NewClassicPauseChoice::resume ?
			NewClassicPauseChoice::discard_exit :
			NewClassicPauseChoice::resume;
		return;
	}
	auto next = static_cast<int>(index_of(choice_)) + delta;
	if(next < 0) {
		next = 3;
	} else if(next > 3) {
		next = 0;
	}
	if(!save_enabled_ && next == 2) {
		next += delta;
		if(next < 0) {
			next = 1;
		} else if(next > 3) {
			next = 3;
		}
	}
	choice_ = static_cast<NewClassicPauseChoice>(next);
}

NewClassicPauseStatus NewClassicPauseMenu::consume(
	NewClassicPauseInput input
) noexcept
{
	if(status_ != NewClassicPauseStatus::running) {
		return status_;
	}
	if(input.application_quit) {
		status_ = NewClassicPauseStatus::application_quit;
		return status_;
	}
	if(phase_ == Phase::release_before_choice) {
		if(!input.raw_any) {
			phase_ = Phase::wait_for_choice;
			visible_ = true;
		}
		return status_;
	}
	if(phase_ == Phase::release_after_resume) {
		if(!input.raw_any) {
			status_ = NewClassicPauseStatus::resume;
			visible_ = false;
		}
		return status_;
	}
	if(presentation_ == NewClassicPausePresentation::netplay_wait) {
		if(input.quit) {
			status_ = NewClassicPauseStatus::discard_exit;
			return status_;
		}
		if(input.cancel) {
			choice_ = NewClassicPauseChoice::resume;
			phase_ = Phase::release_after_resume;
			return status_;
		}
		if(input.up || input.down) {
			move(input.up ? -1 : 1);
			phase_ = Phase::release_before_choice;
			return status_;
		}
		if(!input.confirm) {
			return status_;
		}
		if(choice_ == NewClassicPauseChoice::resume) {
			phase_ = Phase::release_after_resume;
		} else {
			status_ = NewClassicPauseStatus::discard_exit;
		}
		return status_;
	}
	if(input.quit) {
		status_ = NewClassicPauseStatus::discard_exit;
		return status_;
	}
	if(input.restart) {
		status_ = NewClassicPauseStatus::restart;
		return status_;
	}
	if(input.cancel) {
		phase_ = Phase::release_after_resume;
		return status_;
	}
	if(input.up || input.down) {
		move(input.up ? -1 : 1);
		phase_ = Phase::release_before_choice;
		return status_;
	}
	if(!input.confirm) {
		return status_;
	}
	switch(choice_) {
	case NewClassicPauseChoice::resume:
		phase_ = Phase::release_after_resume;
		break;
	case NewClassicPauseChoice::restart:
		status_ = NewClassicPauseStatus::restart;
		break;
	case NewClassicPauseChoice::save_exit:
		status_ = save_enabled_ ? NewClassicPauseStatus::save_exit :
			NewClassicPauseStatus::running;
		if(!save_enabled_) {
			choice_ = NewClassicPauseChoice::discard_exit;
			phase_ = Phase::release_before_choice;
			rejected_save_effect_ = true;
		}
		break;
	case NewClassicPauseChoice::discard_exit:
		status_ = NewClassicPauseStatus::discard_exit;
		break;
	}
	return status_;
}

void NewClassicPauseMenu::draw() noexcept
{
	if(!patch_assets_) {
		return;
	}

	// replay_pause_put_graph_backing() first copies the frozen front page and
	// fills this rectangle with V_WHITE. The following reverse-black TRAM
	// spaces cover every pixel, so the final composed result is a true-black
	// 32x7-cell modal whose gaiji holes expose the colored graphics backing.
	fill_text_black(
		pause_cell_left * present::ank_width,
		pause_cell_top * present::ank_height,
		pause_cell_width * present::ank_width,
		pause_cell_height * present::ank_height
	);

	put_gaiji(
		pause_title_left,
		pause_title_row,
		pause_gaiji_title,
		5U,
		pause_vram_cyan
	);
	if(presentation_ == NewClassicPausePresentation::netplay_wait) {
		const std::array choices{
			NewClassicPauseChoice::resume,
			NewClassicPauseChoice::discard_exit,
		};
		for(std::size_t index = 0U; index < choices.size(); ++index) {
			const auto selected = choices[index] == choice_;
			const auto color = selected ? pause_vram_yellow : pause_vram_white;
			if(selected) {
				put_gaiji(
					pause_choice_mark_left,
					pause_choice_first_row + index,
					pause_gaiji_cursor,
					1U,
					color
				);
			}
			if(index == 0U) {
				put_gaiji(
					pause_gaiji_text_left,
					pause_choice_first_row,
					pause_gaiji_resume,
					5U,
					color
				);
			} else if(netplay_text_) {
				put_menu_text_mask(
					static_cast<std::int32_t>(
						(pause_choice_first_row + index) * present::ank_height
					),
					netplay_text_->forfeit,
					color
				);
			}
		}
		if(resume_countdown_ && *resume_countdown_ >= 1U &&
			*resume_countdown_ <= 3U && netplay_text_) {
				put_menu_text_mask(
					static_cast<std::int32_t>(
						(pause_choice_first_row + 2U) * present::ank_height
					),
					netplay_text_->resume_countdown[
						3U - *resume_countdown_
					],
					pause_vram_cyan
				);
		}
		return;
	}

	for(std::size_t index = 0U; index < 4U; ++index) {
		const auto choice = static_cast<NewClassicPauseChoice>(index);
		const auto color = static_cast<present::Index4>(
			(index == 2U && !save_enabled_) ? pause_vram_blue :
			(choice == choice_) ? pause_vram_yellow : pause_vram_white
		);
		if(choice == choice_) {
			put_gaiji(
				pause_choice_mark_left,
				pause_choice_first_row + index,
				pause_gaiji_cursor,
				1U,
				color
			);
		}

		std::size_t tile = pause_gaiji_resume;
		std::size_t count = 5U;
		switch(choice) {
		case NewClassicPauseChoice::resume:
			break;
		case NewClassicPauseChoice::restart:
			tile = pause_gaiji_restart;
			break;
		case NewClassicPauseChoice::save_exit:
			tile = pause_gaiji_save_exit;
			count = 13U;
			break;
		case NewClassicPauseChoice::discard_exit:
			tile = pause_gaiji_discard_exit;
			count = 11U;
			break;
		}
		put_gaiji(
			pause_gaiji_text_left,
			pause_choice_first_row + index,
			tile,
			count,
			color
		);
	}
}

void NewClassicPauseMenu::put_menu_text_mask(
	std::int32_t top,
	const NewClassicPauseNetplayText::Mask &mask,
	present::Index4 graphics_color
) noexcept
{
	constexpr std::int32_t left = 192;
	if(top < 0 || top + static_cast<std::int32_t>(
		NewClassicPauseNetplayText::height
	) > present::graphics_height) {
		return;
	}
	for(std::size_t y = 0U; y < NewClassicPauseNetplayText::height; ++y) {
		for(std::size_t x = 0U; x < NewClassicPauseNetplayText::width; ++x) {
			if(mask[(y * NewClassicPauseNetplayText::width) + x] == 0U) {
				continue;
			}
			const auto destination =
				((static_cast<std::size_t>(top) + y) *
					present::graphics_width) +
				static_cast<std::size_t>(left) + x;
			pixels_[destination] = {
				present::ComposedPixelLayer::graphics,
				graphics_color,
			};
		}
	}
}

void NewClassicPauseMenu::fill_text_black(
	std::size_t left,
	std::size_t top,
	std::size_t width,
	std::size_t height
) noexcept
{
	for(std::size_t y = top; y < top + height; ++y) {
		const auto first = (y * present::graphics_width) + left;
		std::fill_n(
			pixels_.begin() + static_cast<std::ptrdiff_t>(first),
			width,
			present::ComposedPixel{
				present::ComposedPixelLayer::text,
				static_cast<std::uint8_t>(present::TextColor::black),
			}
		);
	}
}

void NewClassicPauseMenu::put_gaiji(
	std::size_t cell_left,
	std::size_t cell_top,
	std::size_t tile,
	std::size_t count,
	present::Index4 graphics_color
) noexcept
{
	if(!patch_assets_ || tile > patch_assets_->pause_gaiji.size() ||
		count > patch_assets_->pause_gaiji.size() - tile) {
		return;
	}
	for(std::size_t glyph = 0U; glyph < count; ++glyph) {
		const auto &bitmap = patch_assets_->pause_gaiji[tile + glyph];
		const auto glyph_left =
			((cell_left + (glyph * 2U)) * present::ank_width);
		const auto glyph_top = cell_top * present::ank_height;
		for(std::size_t row = 0U; row < present::gaiji_height; ++row) {
			for(std::size_t half = 0U; half < 2U; ++half) {
				const auto bits = bitmap[(row * 2U) + half];
				for(std::size_t bit = 0U; bit < 8U; ++bit) {
					if((bits & (0x80U >> bit)) == 0U) {
						continue;
					}
					const auto x = glyph_left + (half * 8U) + bit;
					const auto y = glyph_top + row;
					pixels_[(y * present::graphics_width) + x] = {
						present::ComposedPixelLayer::graphics,
						graphics_color,
					};
				}
			}
		}
	}
}

present::SourceSurface NewClassicPauseMenu::compose_over(
	present::SourceSurface base
) noexcept
{
	if(base.pixels.size() != pixels_.size() || !visible_ || !patch_assets_) {
		return base;
	}
	std::copy(base.pixels.begin(), base.pixels.end(), pixels_.begin());
	draw();
	return {
		.pixels = std::span<
			const present::ComposedPixel, present::graphics_page_pixels
		>(pixels_.data(), pixels_.size()),
		.graphics_palette = base.graphics_palette,
		.shown_page = base.shown_page,
		.raster_mode = base.raster_mode,
		.effective_scroll_y = base.effective_scroll_y,
		.palette_tone = base.palette_tone,
		.reduced_flash_palette = base.reduced_flash_palette,
		// The frozen surface fingerprints no longer describe the exact pixels
		// after applying the source-faithful graphics/TRAM overlay.
		.oracle_hashes = std::nullopt,
		.host_overlay = base.host_overlay,
		.reduced_flash_applied = base.reduced_flash_applied,
	};
}

NewClassicPauseChoice NewClassicPauseMenu::choice() const noexcept
{
	return choice_;
}

bool NewClassicPauseMenu::save_enabled() const noexcept
{
	return save_enabled_;
}

bool NewClassicPauseMenu::take_rejected_save_effect() noexcept
{
	const bool result = rejected_save_effect_;
	rejected_save_effect_ = false;
	return result;
}

void NewClassicPauseMenu::set_resume_countdown(
	std::optional<std::uint8_t> seconds
) noexcept
{
	resume_countdown_ = seconds;
	// Local Resume completes before the peer barrier can start its shared
	// countdown. Keep the frozen pause owner visible until the synchronized
	// resume actually releases it.
	if(presentation_ == NewClassicPausePresentation::netplay_wait && seconds) {
		visible_ = true;
	}
}

std::optional<std::uint8_t>
NewClassicPauseMenu::resume_countdown() const noexcept
{
	return resume_countdown_;
}

} // namespace th03::app
