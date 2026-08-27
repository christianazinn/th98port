#include "th03/app/new_classic/new_classic_frontend.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace th03::app {
namespace {

constexpr std::uint8_t palette_fade_boundary_count = 19U;

[[nodiscard]] constexpr bool any(NewClassicUiInput input) noexcept
{
	return input.up || input.down || input.left || input.right ||
		input.confirm || input.bomb || input.cancel;
}

} // namespace

NewClassicReplayHub::NewClassicReplayHub(
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	bool libraries_available,
	bool upload_available,
	bool my_records_available
) :
	patch_assets_(std::move(patch_assets))
{
	valid_ = static_cast<bool>(patch_assets_);
	static_cast<void>(refresh_capabilities(
		libraries_available, upload_available, my_records_available
	));
	if(valid_) {
		patch_surface_.raster().set_palette_tone(0);
		patch_surface_.raster().apply_pending_palette();
	}
}

bool NewClassicReplayHub::valid() const noexcept
{
	return valid_;
}

void NewClassicReplayHub::draw() noexcept
{
	if(!valid_ || !patch_assets_ || !patch_surface_.begin_hidden_frame(
		patch_assets_->menu_background(NewClassicMenuBackground::replay)
	)) {
		valid_ = false;
		return;
	}
	auto &raster = patch_surface_.raster();
	static_cast<void>(raster.set_pending_palette_entry(12U, {15U, 15U, 0U}));
	raster.apply_pending_palette();
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages();
	const auto message = [](NewClassicReplayHubStatus row) noexcept {
		switch(row) {
		case NewClassicReplayHubStatus::local_selected:
			return NewClassicMessageId::replay_command_local;
		case NewClassicReplayHubStatus::upload_selected:
			return NewClassicMessageId::replay_command_upload;
		case NewClassicReplayHubStatus::my_records_selected:
			return NewClassicMessageId::replay_command_my_records;
		case NewClassicReplayHubStatus::online_selected:
			return NewClassicMessageId::replay_command_online;
		case NewClassicReplayHubStatus::downloaded_selected:
			return NewClassicMessageId::replay_command_downloaded;
		case NewClassicReplayHubStatus::back:
			return NewClassicMessageId::replay_command_return;
		case NewClassicReplayHubStatus::running:
		case NewClassicReplayHubStatus::application_quit:
			break;
		}
		return NewClassicMessageId::replay_command_return;
	};
	valid_ = font.put_centered(
		raster, 320, 80, messages.text(NewClassicMessageId::title_replay), 9U
	);
	for(std::size_t row = 0U; valid_ && row < rows_.size(); ++row) {
		const auto top = static_cast<std::int32_t>(144U + (row * 32U));
		const auto color = static_cast<present::Index4>(
			row == selection_ ? 12U : 15U
		);
		valid_ = font.put_centered(
			raster, 320, top, messages.text(message(rows_[row])), color
		);
		if(valid_ && row == selection_) {
			valid_ = font.put(raster, 160, top, ">", 12U);
		}
	}
	if(valid_ && !patch_surface_.publish(raster.access_page())) {
		valid_ = false;
	}
}

NewClassicReplayHubResult NewClassicReplayHub::consume(
	NewClassicUiInput input
) noexcept
{
	if(!valid_ || status_ != NewClassicReplayHubStatus::running) {
		return {.status = status_};
	}
	if(input.app_quit_requested) {
		status_ = NewClassicReplayHubStatus::application_quit;
		return {.status = status_};
	}
	if(fade_in_active_) {
		auto &raster = patch_surface_.raster();
		bool changed = false;
		if(fade_boundary_ != 0U && fade_boundary_ <= 17U) {
			const auto pair = static_cast<std::int16_t>(fade_boundary_ - 1U);
			raster.set_palette_tone(static_cast<std::int16_t>(pair * 6));
			raster.apply_pending_palette();
			changed = true;
		}
		++fade_boundary_;
		if(fade_boundary_ >= palette_fade_boundary_count) {
			raster.set_palette_tone(100);
			raster.apply_pending_palette();
			fade_in_active_ = false;
			input_allowed_ = !any(input);
			changed = true;
		}
		return {.surface_changed = changed, .status = status_};
	}
	if(!input_allowed_) {
		if(!any(input)) {
			input_allowed_ = true;
		}
		return {.status = status_};
	}
	if(!any(input)) {
		return {.status = status_};
	}
	input_allowed_ = false;
	if(input.cancel || input.bomb) {
		status_ = NewClassicReplayHubStatus::back;
		return {.status = status_};
	}
	if(input.up) {
		selection_ = (selection_ + rows_.size() - 1U) % rows_.size();
	} else if(input.down) {
		selection_ = (selection_ + 1U) % rows_.size();
	} else if(input.confirm) {
		status_ = rows_[selection_];
		return {.status = status_};
	} else {
		return {.status = status_};
	}
	draw();
	return {.surface_changed = valid_, .status = status_};
}

bool NewClassicReplayHub::refresh_capabilities(
	bool libraries_available,
	bool upload_available,
	bool my_records_available
) noexcept
{
	if(!valid_ || status_ != NewClassicReplayHubStatus::running) {
		return false;
	}
	std::vector<NewClassicReplayHubStatus> rows{
		NewClassicReplayHubStatus::local_selected,
	};
	static_cast<void>(upload_available);
	if(my_records_available) {
		rows.push_back(NewClassicReplayHubStatus::my_records_selected);
	}
	if(libraries_available) {
		rows.push_back(NewClassicReplayHubStatus::online_selected);
		rows.push_back(NewClassicReplayHubStatus::downloaded_selected);
	}
	rows.push_back(NewClassicReplayHubStatus::back);
	if(rows == rows_) {
		return false;
	}
	const auto selected = selection_ < rows_.size() ?
		rows_[selection_] : NewClassicReplayHubStatus::local_selected;
	rows_ = std::move(rows);
	const auto retained = std::ranges::find(rows_, selected);
	selection_ = retained == rows_.end() ?
		std::min(selection_, rows_.size() - 1U) :
		static_cast<std::size_t>(retained - rows_.begin());
	draw();
	return valid_;
}

present::SourceSurface NewClassicReplayHub::surface() const noexcept
{
	return patch_surface_.surface();
}

NewClassicReplayHubStatus NewClassicReplayHub::status() const noexcept
{
	return status_;
}

std::size_t NewClassicReplayHub::selection() const noexcept
{
	return selection_;
}

} // namespace th03::app

