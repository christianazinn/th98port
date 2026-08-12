#include "th03/app/new_classic_pause.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace th03::app {
namespace {

constexpr std::array<std::string_view, 4U> labels{
	"Resume",
	"Restart",
	"Save Replay and Exit",
	"Exit Without Saving",
};

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
		.raw_any = input.system != 0U || restart_requested,
		.application_quit = application_quit_requested,
	};
}

NewClassicPauseMenu::NewClassicPauseMenu(bool save_enabled) noexcept :
	save_enabled_(save_enabled)
{}

void NewClassicPauseMenu::move(int delta) noexcept
{
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
	if(input.quit) {
		status_ = NewClassicPauseStatus::discard_exit;
		return status_;
	}
	if(input.restart) {
		status_ = NewClassicPauseStatus::restart;
		return status_;
	}
	if(phase_ == Phase::release_before_choice) {
		if(!input.raw_any) {
			phase_ = Phase::wait_for_choice;
		}
		return status_;
	}
	if(phase_ == Phase::release_after_resume) {
		if(!input.raw_any) {
			status_ = NewClassicPauseStatus::resume;
		}
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
	constexpr auto text_layer = present::ComposedPixelLayer::text;
	constexpr std::size_t left = 192U;
	constexpr std::size_t top = 128U;
	canvas_.fill_rect(left, top, 256U, 144U, 0U, text_layer);
	canvas_.draw_text_centered("Pause", 320U, top + 16U, 2U, 5U, text_layer);
	for(std::size_t index = 0U; index < labels.size(); ++index) {
		const auto selected = index == index_of(choice_);
		const auto disabled = index == 2U && !save_enabled_;
		const auto color = static_cast<std::uint8_t>(
			disabled ? 1U : selected ? 6U : 7U
		);
		canvas_.draw_text(selected ? ">" : " ", left + 16U,
			top + 48U + (index * 20U), 2U, color, text_layer);
		canvas_.draw_text(labels[index], left + 40U,
			top + 48U + (index * 20U), 2U, color, text_layer);
	}
}

present::SourceSurface NewClassicPauseMenu::compose_over(
	present::SourceSurface base
) noexcept
{
	canvas_.copy_from(base);
	draw();
	return canvas_.surface();
}

NewClassicPauseChoice NewClassicPauseMenu::choice() const noexcept
{
	return choice_;
}

bool NewClassicPauseMenu::save_enabled() const noexcept
{
	return save_enabled_;
}

} // namespace th03::app
