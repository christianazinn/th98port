#include "th03/app/new_classic_online_profile_scene.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string_view>
#include <utility>

#include "th03/app/new_classic_nametag.hpp"

namespace th03::app {
namespace {

constexpr std::int32_t center_x = 320;
constexpr std::int32_t title_top = 64;
constexpr present::Index4 title_color = 9U;
constexpr present::Index4 selected_color = 12U;
constexpr present::Index4 normal_color = 15U;
constexpr present::Index4 detail_color = 13U;
constexpr present::Index4 error_color = 11U;
constexpr std::uint8_t fade_boundary_count = 19U;

[[nodiscard]] bool raw_any(NewClassicUiInput input) noexcept
{
	return input.up || input.down || input.left || input.right ||
		input.confirm || input.bomb || input.cancel;
}

[[nodiscard]] bool link_code_character(char character) noexcept
{
	return (character >= '2' && character <= '9') ||
		(character >= 'A' && character <= 'H') ||
		(character >= 'J' && character <= 'N') ||
		(character >= 'P' && character <= 'Z');
}

} // namespace

NewClassicOnlineProfileScene::NewClassicOnlineProfileScene(
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	NewClassicLanguageAssetProfile language,
	std::optional<std::string> nametag
) :
	patch_assets_(std::move(patch_assets)),
	language_(language),
	nametag_(std::move(nametag))
{
	if((nametag_ && !layout_new_classic_nametag(*nametag_)) ||
		!patch_assets_ || !patch_surface_.reset(
			patch_assets_->menu_background(NewClassicMenuBackground::online_profile)
		)) {
		return;
	}
	if(!nametag_) {
		screen_ = Screen::name_entry;
	}
	valid_ = draw();
	if(valid_) {
		patch_surface_.raster().set_palette_tone(0);
	}
}

bool NewClassicOnlineProfileScene::draw() noexcept
{
	if(!patch_assets_ || !patch_surface_.begin_hidden_frame(
		patch_assets_->menu_background(NewClassicMenuBackground::online_profile)
	)) {
		return false;
	}
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages(language_);
	auto &raster = patch_surface_.raster();
	if(!font.put_centered(raster, center_x, title_top,
		messages.text(NewClassicMessageId::profile_title), title_color)) {
		return false;
	}

	if(screen_ == Screen::menu) {
		if(!font.put_centered(raster, center_x, 112,
			messages.text(NewClassicMessageId::profile_current), normal_color)) {
			return false;
		}
		const auto layout = nametag_ ? layout_new_classic_nametag(*nametag_) :
			std::expected<NewClassicNametagLayout, NewClassicNametagError>{
				std::unexpected(NewClassicNametagError::empty)
			};
		if(!layout || !font.put_centered(
			raster, center_x, 136, layout->lines[0U], detail_color
		) || (layout->line_count == 2U && !font.put_centered(
			raster, center_x, 160, layout->lines[1U], detail_color
		))) {
			return false;
		}
		constexpr std::array rows{
			NewClassicMessageId::profile_change_name,
			NewClassicMessageId::profile_link_another,
			NewClassicMessageId::profile_link_this,
			NewClassicMessageId::profile_return,
		};
		for(std::size_t row = 0U; row < rows.size(); ++row) {
			if(!font.put_centered(
				raster, center_x, 208 + static_cast<std::int32_t>(row) * 32,
				messages.text(rows[row]), row == selection_ ?
					selected_color : normal_color
			)) {
				return false;
			}
		}
	} else if(screen_ == Screen::name_entry) {
		if(!font.put_centered(raster, center_x, 112,
			messages.text(NewClassicMessageId::profile_enter_name), normal_color)) {
			return false;
		}
		const auto first = std::string_view(entry_).substr(
			0U, std::min<std::size_t>(entry_.size(), new_classic_nametag_line_cells)
		);
		const auto second = entry_.size() <= new_classic_nametag_line_cells ?
			std::string_view{} : std::string_view(entry_).substr(
				new_classic_nametag_line_cells
			);
		if(!font.put_centered(raster, center_x, 160,
			first.empty() ? std::string_view{"_"} : first,
			invalid_entry_ ? error_color : selected_color) ||
			(!second.empty() && !font.put_centered(
				raster, center_x, 184, second,
				invalid_entry_ ? error_color : selected_color
			)) || !font.put_centered(raster, center_x, 240,
			messages.text(invalid_entry_ ?
				NewClassicMessageId::profile_name_invalid :
				NewClassicMessageId::profile_name_hint),
			invalid_entry_ ? error_color : detail_color)) {
			return false;
		}
	} else if(screen_ == Screen::link_entry) {
		const auto shown = entry_.empty() ? std::string_view{"----------"} :
			std::string_view{entry_};
		if(!font.put_centered(raster, center_x, 128,
			messages.text(NewClassicMessageId::profile_link_this), normal_color) ||
			!font.put_centered(raster, center_x, 176, shown,
				invalid_entry_ ? error_color : selected_color) ||
			!font.put_centered(raster, center_x, 240,
				messages.text(NewClassicMessageId::netplay_room_footer_edit),
				detail_color)) {
			return false;
		}
	} else if(screen_ == Screen::link_code) {
		if(!font.put_centered(raster, center_x, 128,
			messages.text(NewClassicMessageId::profile_link_code), normal_color) ||
			!font.put_centered(raster, center_x, 176, link_code_, selected_color) ||
			!font.put_centered(raster, center_x, 224,
				messages.text(NewClassicMessageId::profile_link_code_hint),
				detail_color)) {
			return false;
		}
	} else {
		const auto text = screen_ == Screen::linked ?
			NewClassicMessageId::profile_linked :
			NewClassicMessageId::profile_failed;
		if(!font.put_centered(raster, center_x, 160, messages.text(text),
			screen_ == Screen::linked ? detail_color : error_color) ||
			!font.put_centered(raster, center_x, 240,
				messages.text(NewClassicMessageId::netplay_report_footer),
				detail_color)) {
			return false;
		}
	}
	return patch_surface_.publish(raster.access_page());
}

void NewClassicOnlineProfileScene::require_release() noexcept
{
	release_required_ = true;
}

void NewClassicOnlineProfileScene::begin_fade_out(
	NewClassicOnlineProfileAction action
) noexcept
{
	phase_ = Phase::fade_out;
	fade_boundary_ = 0U;
	fade_out_action_ = std::move(action);
}

bool NewClassicOnlineProfileScene::consume_fade() noexcept
{
	const bool black_in = phase_ == Phase::fade_in;
	auto &raster = patch_surface_.raster();
	bool changed = false;
	if(fade_boundary_ != 0U && fade_boundary_ <= 17U) {
		const auto pair = static_cast<std::int16_t>(fade_boundary_ - 1U);
		raster.set_palette_tone(static_cast<std::int16_t>(
			black_in ? (pair * 6) : (100 - (pair * 6))
		));
		raster.apply_pending_palette();
		changed = true;
	}
	++fade_boundary_;
	if(fade_boundary_ < fade_boundary_count) {
		return changed;
	}
	raster.set_palette_tone(black_in ? 100 : 0);
	raster.apply_pending_palette();
	fade_boundary_ = 0U;
	if(black_in) {
		phase_ = Phase::menu;
		require_release();
	} else {
		action_ = std::move(fade_out_action_);
	}
	return true;
}

bool NewClassicOnlineProfileScene::consume(
	NewClassicOnlineProfileInput input
) noexcept
{
	if(!valid_ || action_ || request_pending_) {
		return false;
	}
	if(input.ui.app_quit_requested) {
		action_ = NewClassicOnlineProfileAction{
			NewClassicOnlineProfileActionType::application_quit, {}
		};
		return false;
	}
	if(phase_ != Phase::menu) {
		return consume_fade();
	}
	const bool any = raw_any(input.ui) || input.text_character || input.erase;
	if(release_required_) {
		if(!any) {
			release_required_ = false;
		}
		return false;
	}
	if(screen_ == Screen::menu) {
		if(input.ui.up || input.ui.down) {
			selection_ = static_cast<std::uint8_t>((selection_ +
				(input.ui.up ? 3U : 1U)) % 4U);
			require_release();
			return draw();
		}
		if(input.ui.cancel || (input.ui.confirm && selection_ == 3U)) {
			begin_fade_out(NewClassicOnlineProfileAction{
				NewClassicOnlineProfileActionType::back, {}
			});
			return false;
		}
		if(input.ui.confirm) {
			if(selection_ == 0U) {
				screen_ = Screen::name_entry;
				entry_ = nametag_.value_or(std::string{});
			} else if(selection_ == 1U) {
				action_ = NewClassicOnlineProfileAction{
					NewClassicOnlineProfileActionType::create_link_code, {}
				};
				request_pending_ = true;
			} else {
				screen_ = Screen::link_entry;
				entry_.clear();
			}
			invalid_entry_ = false;
			require_release();
			return action_ ? false : draw();
		}
		return false;
	}

	if(screen_ == Screen::name_entry || screen_ == Screen::link_entry) {
		if(input.erase && !entry_.empty()) {
			entry_.pop_back();
			invalid_entry_ = false;
			return draw();
		}
		if(input.text_character) {
			char character = *input.text_character;
			if(screen_ == Screen::link_entry) {
				character = static_cast<char>(std::toupper(
					static_cast<unsigned char>(character)
				));
				if(link_code_character(character) && entry_.size() < 10U) {
					entry_.push_back(character);
				}
			} else if(character >= 0x20 && character <= 0x7E &&
				entry_.size() < new_classic_nametag_max_bytes) {
				entry_.push_back(character);
			}
			invalid_entry_ = false;
			return draw();
		}
		if(input.ui.cancel) {
			if(!nametag_) {
				begin_fade_out(NewClassicOnlineProfileAction{
					NewClassicOnlineProfileActionType::back, {}
				});
				return false;
			}
			screen_ = Screen::menu;
			entry_.clear();
			invalid_entry_ = false;
			require_release();
			return draw();
		}
		if(input.ui.confirm) {
			const bool valid = screen_ == Screen::name_entry ?
				layout_new_classic_nametag(entry_).has_value() :
				(entry_.size() == 10U && std::ranges::all_of(
					entry_, link_code_character
				));
			if(!valid) {
				invalid_entry_ = true;
				require_release();
				return draw();
			}
			action_ = NewClassicOnlineProfileAction{
				screen_ == Screen::name_entry ?
					NewClassicOnlineProfileActionType::update_nametag :
					NewClassicOnlineProfileActionType::redeem_link_code,
				entry_,
			};
			request_pending_ = true;
			return false;
		}
		return false;
	}

	if(any) {
		screen_ = nametag_ ? Screen::menu : Screen::name_entry;
		link_code_.clear();
		entry_.clear();
		invalid_entry_ = false;
		require_release();
		return draw();
	}
	return false;
}

std::optional<NewClassicOnlineProfileAction>
NewClassicOnlineProfileScene::take_action() noexcept
{
	return std::exchange(action_, std::nullopt);
}

bool NewClassicOnlineProfileScene::report_profile(
	NewClassicOnlineProfile profile
) noexcept
{
	if(!request_pending_ || !profile.nametag ||
		!layout_new_classic_nametag(*profile.nametag)) {
		return false;
	}
	nametag_ = std::move(profile.nametag);
	entry_.clear();
	screen_ = Screen::menu;
	request_pending_ = false;
	require_release();
	return draw();
}

bool NewClassicOnlineProfileScene::report_link_code(std::string code) noexcept
{
	if(!request_pending_ || code.size() != 10U ||
		!std::ranges::all_of(code, link_code_character)) {
		return false;
	}
	link_code_ = std::move(code);
	screen_ = Screen::link_code;
	request_pending_ = false;
	require_release();
	return draw();
}

bool NewClassicOnlineProfileScene::report_linked(
	NewClassicOnlineProfile profile
) noexcept
{
	if(!request_pending_ || (profile.nametag &&
		!layout_new_classic_nametag(*profile.nametag))) {
		return false;
	}
	nametag_ = std::move(profile.nametag);
	entry_.clear();
	screen_ = nametag_ ? Screen::linked : Screen::name_entry;
	request_pending_ = false;
	require_release();
	return draw();
}

bool NewClassicOnlineProfileScene::report_failure() noexcept
{
	if(!request_pending_) {
		return false;
	}
	request_pending_ = false;
	screen_ = Screen::failed;
	require_release();
	return draw();
}

bool NewClassicOnlineProfileScene::valid() const noexcept
{
	return valid_;
}

bool NewClassicOnlineProfileScene::text_entry_active() const noexcept
{
	return screen_ == Screen::name_entry || screen_ == Screen::link_entry;
}

present::SourceSurface NewClassicOnlineProfileScene::surface() const noexcept
{
	return patch_surface_.surface();
}

} // namespace th03::app
