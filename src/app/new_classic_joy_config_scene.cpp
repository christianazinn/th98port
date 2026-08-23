#include "th03/app/new_classic_joy_config_scene.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

#include "th03/present/pi.hpp"

namespace th03::app {
namespace {

constexpr present::Index4 color_header = 14U;
constexpr present::Index4 color_footer = 14U;
constexpr present::Index4 color_selected = 11U;
constexpr present::Index4 color_label = 12U;
constexpr present::Index4 color_value = 15U;
constexpr std::int32_t title_top = 4 * 16;
constexpr std::int32_t header_top = 6 * 16;
constexpr std::int32_t rows_top = 8 * 16;
constexpr std::int32_t label_left = 176;
constexpr std::int32_t value_left = 352;
constexpr std::int32_t command_center = 320;
constexpr std::int32_t cursor_gap = 16;
constexpr std::int32_t overlay_top = 20 * 16;
constexpr std::uint8_t row_count = 9U;
constexpr std::uint8_t switch_page_row = 5U;
constexpr std::uint8_t defaults_row = 6U;
constexpr std::uint8_t apply_row = 7U;
constexpr std::uint8_t cancel_row = 8U;
constexpr std::uint8_t fade_boundary_count = 19U;

constexpr std::array<NewClassicMessageId, new_classic_joy_action_count>
	action_labels{
		NewClassicMessageId::input_action_shot,
		NewClassicMessageId::input_action_bomb,
		NewClassicMessageId::input_action_charge,
		NewClassicMessageId::input_action_menu_back,
		NewClassicMessageId::input_action_menu_ok,
	};
constexpr std::array<NewClassicMessageId, 16U> button_labels{
	NewClassicMessageId::joy_button_south,
	NewClassicMessageId::joy_button_east,
	NewClassicMessageId::joy_button_west,
	NewClassicMessageId::joy_button_north,
	NewClassicMessageId::joy_button_back,
	NewClassicMessageId::joy_button_start,
	NewClassicMessageId::joy_button_left_stick,
	NewClassicMessageId::joy_button_right_stick,
	NewClassicMessageId::joy_button_left_shoulder,
	NewClassicMessageId::joy_button_right_shoulder,
	NewClassicMessageId::joy_button_dpad_up,
	NewClassicMessageId::joy_button_dpad_down,
	NewClassicMessageId::joy_button_dpad_left,
	NewClassicMessageId::joy_button_dpad_right,
	NewClassicMessageId::joy_button_misc,
	NewClassicMessageId::input_value_unbound,
};

[[nodiscard]] constexpr present::Page opposite(present::Page page) noexcept
{
	return page == present::Page::zero ? present::Page::one : present::Page::zero;
}

[[nodiscard]] bool put_header_centered(
	const NewClassicMenuFont &font,
	present::Raster &raster,
	std::int32_t center,
	std::int32_t top,
	std::string_view text
) noexcept
{
	return font.put_centered(raster, center + 1, top + 1, text, 0U) &&
		font.put_centered(raster, center, top, text, color_header);
}

[[nodiscard]] constexpr std::uint8_t button_ordinal(
	NewClassicJoyButton button
) noexcept
{
	return button == NewClassicJoyButton::unbound ? 15U :
		static_cast<std::uint8_t>(button);
}

[[nodiscard]] constexpr NewClassicJoyButton button_from_ordinal(
	std::uint8_t ordinal
) noexcept
{
	return ordinal == 15U ? NewClassicJoyButton::unbound :
		static_cast<NewClassicJoyButton>(ordinal);
}

[[nodiscard]] constexpr std::uint8_t ring(
	std::uint8_t value,
	int direction,
	std::uint8_t maximum
) noexcept
{
	if(direction < 0) {
		return value == 0U ? maximum : static_cast<std::uint8_t>(value - 1U);
	}
	return value == maximum ? 0U : static_cast<std::uint8_t>(value + 1U);
}

} // namespace

NewClassicJoyConfigScene::NewClassicJoyConfigScene(
	NewClassicJoyConfig config,
	NewClassicJoyConfig defaults,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	NewClassicLanguageAssetProfile language_profile
) :
	original_(config),
	config_(config),
	defaults_(defaults),
	language_profile_(language_profile),
	patch_assets_(std::move(patch_assets))
{
	if(!validate_new_classic_joy_config(config_) ||
		!validate_new_classic_joy_config(defaults_) || !patch_assets_ ||
		!patch_surface_.reset(patch_assets_->menu_background(
			NewClassicMenuBackground::neutral
		))) {
		fail_surface();
		return;
	}
	auto &raster = patch_surface_.raster();
	raster.set_palette_tone(0);
	raster.apply_pending_palette();
	valid_ = true;
	if(!compose_screen()) {
		fail_surface();
	}
}

bool NewClassicJoyConfigScene::draw_value(
	std::string_view label,
	std::string_view value,
	std::int32_t top,
	bool selected
) noexcept
{
	auto &raster = patch_surface_.raster();
	const auto &font = patch_assets_->menu_font;
	return (!selected || font.put(
		raster, label_left - cursor_gap, top, ">", color_selected
	)) && font.put(
		raster, label_left, top, label,
		selected ? color_selected : color_label
	) && font.put(
		raster, value_left, top, value,
		selected ? color_selected : color_value
	);
}

bool NewClassicJoyConfigScene::draw_command(
	std::string_view label,
	std::int32_t top,
	bool selected
) noexcept
{
	auto &raster = patch_surface_.raster();
	const auto &font = patch_assets_->menu_font;
	const auto left = command_center - (font.width(label) / 2);
	return (!selected || font.put(
		raster, left - cursor_gap, top, ">", color_selected
	)) && font.put(
		raster, left, top, label,
		selected ? color_selected : color_label
	);
}

bool NewClassicJoyConfigScene::draw_row(
	std::uint8_t row,
	bool selected
) noexcept
{
	const auto &messages = patch_assets_->messages(language_profile_);
	const auto top = rows_top + (static_cast<std::int32_t>(
		row + (row >= defaults_row ? 1U : 0U)
	) * 16);
	if(row == switch_page_row) {
		return draw_command(
			messages.text(page_ == Page::buttons ?
				NewClassicMessageId::joy_command_axes :
				NewClassicMessageId::joy_command_buttons),
			top, selected
		);
	}
	if(row == defaults_row) {
		return draw_command(messages.text(
			NewClassicMessageId::input_command_restore_defaults
		), top, selected);
	}
	if(row == apply_row) {
		return draw_command(messages.text(
			NewClassicMessageId::input_command_apply_return
		), top, selected);
	}
	if(row == cancel_row) {
		return draw_command(messages.text(
			NewClassicMessageId::input_command_cancel
		), top, selected);
	}
	if(page_ == Page::buttons) {
		const auto button = config_.buttons[row];
		return draw_value(
			messages.text(action_labels[row]),
			messages.text(button_labels[button_ordinal(button)]), top, selected
		);
	}
	if(row == 0U) {
		return draw_value(messages.text(
			NewClassicMessageId::joy_label_horizontal_axis
		),
			config_.horizontal_axis == NewClassicJoyAxis::left_x ?
				messages.text(NewClassicMessageId::joy_value_left_x) :
				messages.text(NewClassicMessageId::joy_value_right_x), top, selected);
	}
	if(row == 1U) {
		return draw_value(messages.text(
			NewClassicMessageId::joy_label_vertical_axis
		),
			config_.vertical_axis == NewClassicJoyAxis::left_y ?
				messages.text(NewClassicMessageId::joy_value_left_y) :
				messages.text(NewClassicMessageId::joy_value_right_y), top, selected);
	}
	if(row == 2U) {
		return draw_value(messages.text(
			NewClassicMessageId::joy_label_horizontal_dir
		), messages.text(config_.horizontal_reversed ?
			NewClassicMessageId::input_value_reverse :
			NewClassicMessageId::value_normal), top, selected);
	}
	if(row == 3U) {
		return draw_value(messages.text(
			NewClassicMessageId::joy_label_vertical_dir
		), messages.text(config_.vertical_reversed ?
			NewClassicMessageId::input_value_reverse :
			NewClassicMessageId::value_normal), top, selected);
	}
	std::array<char, 2U> percent{
		static_cast<char>('0' + (config_.dead_zone_percent / 10U)),
		static_cast<char>('0' + (config_.dead_zone_percent % 10U)),
	};
	const std::string_view percent_value{percent.data(), percent.size()};
	const std::array arguments{
		NewClassicMessageArgument{"percent", percent_value},
	};
	const auto formatted = messages.format(
		NewClassicMessageId::value_percent, arguments
	);
	return draw_value(messages.text(
		NewClassicMessageId::joy_label_dead_zone
	), formatted, top, selected);
}

bool NewClassicJoyConfigScene::compose_screen() noexcept
{
	if(!valid_ || !patch_assets_) {
		return false;
	}
	const auto target = opposite(front_page_);
	auto &raster = patch_surface_.raster();
	const auto &background = patch_assets_->menu_background(
		NewClassicMenuBackground::neutral
	);
	if(!raster.set_access_page(target) || !present::pi_put_8(
		raster, 0, 0, background
	)) {
		return false;
	}
	const auto &messages = patch_assets_->messages(language_profile_);
	const auto header = messages.text(page_ == Page::buttons ?
		NewClassicMessageId::joy_header_buttons :
		NewClassicMessageId::joy_header_axes);
	if(!put_header_centered(
			patch_assets_->menu_font, raster, 320, title_top,
			messages.text(NewClassicMessageId::option_joy_config)
		) || !put_header_centered(
		patch_assets_->menu_font, raster, 320, header_top, header
	)) {
		return false;
	}
	for(std::uint8_t row = 0U; row < row_count; ++row) {
		if(!draw_row(row, row == selected_)) {
			return false;
		}
	}
	if(!patch_surface_.publish(target) || !raster.set_access_page(target)) {
		return false;
	}
	front_page_ = target;
	return true;
}

bool NewClassicJoyConfigScene::show_overlay(std::string_view text) noexcept
{
	if(!valid_ || !patch_assets_) {
		return false;
	}
	auto &raster = patch_surface_.raster();
	return raster.set_access_page(front_page_) &&
		patch_assets_->menu_font.put_centered(
			raster, 320, overlay_top, text, color_footer
		) && patch_surface_.publish(front_page_) &&
		raster.set_access_page(front_page_);
}

bool NewClassicJoyConfigScene::adjust(int direction) noexcept
{
	if(selected_ >= switch_page_row) {
		return true;
	}
	if(page_ == Page::buttons) {
		const auto index = static_cast<std::size_t>(selected_);
		const auto old = config_.buttons[index];
		const auto next = button_from_ordinal(ring(
			button_ordinal(old), direction, 15U
		));
		for(std::size_t other = 0U; other < config_.buttons.size(); ++other) {
			if(other != index && next != NewClassicJoyButton::unbound &&
				config_.buttons[other] == next) {
				config_.buttons[other] = old;
				break;
			}
		}
		config_.buttons[index] = next;
		return true;
	}
	switch(selected_) {
	case 0U:
		config_.horizontal_axis = config_.horizontal_axis ==
			NewClassicJoyAxis::left_x ? NewClassicJoyAxis::right_x :
			NewClassicJoyAxis::left_x;
		break;
	case 1U:
		config_.vertical_axis = config_.vertical_axis ==
			NewClassicJoyAxis::left_y ? NewClassicJoyAxis::right_y :
			NewClassicJoyAxis::left_y;
		break;
	case 2U: config_.horizontal_reversed = !config_.horizontal_reversed; break;
	case 3U: config_.vertical_reversed = !config_.vertical_reversed; break;
	case 4U:
		config_.dead_zone_percent = static_cast<std::uint8_t>(
			ring(static_cast<std::uint8_t>(
				(config_.dead_zone_percent / 5U) - 1U
			), direction, 18U) * 5U + 5U
		);
		break;
	default: return false;
	}
	return true;
}

void NewClassicJoyConfigScene::restore_defaults() noexcept
{
	config_ = defaults_;
}

bool NewClassicJoyConfigScene::any_interface_input(
	const NewClassicUiInput &input
) noexcept
{
	return input.up || input.down || input.left || input.right ||
		input.confirm || input.bomb || input.cancel;
}

NewClassicJoyConfigSceneResult NewClassicJoyConfigScene::result(
	bool surface_changed
) const noexcept
{
	return {.surface_changed = surface_changed, .status = status_};
}

void NewClassicJoyConfigScene::fail_surface() noexcept
{
	valid_ = false;
	status_ = NewClassicJoyConfigSceneStatus::surface_failed;
}

void NewClassicJoyConfigScene::begin_fade_out(
	NewClassicJoyConfigSceneStatus terminal
) noexcept
{
	phase_ = Phase::fade_out;
	fade_boundary_ = 0U;
	fade_out_terminal_ = terminal;
	status_ = NewClassicJoyConfigSceneStatus::running;
}

NewClassicJoyConfigSceneResult NewClassicJoyConfigScene::advance_fade(
	const NewClassicUiInput &input
) noexcept
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
		return result(changed);
	}
	raster.set_palette_tone(black_in ? 100 : 0);
	raster.apply_pending_palette();
	fade_boundary_ = 0U;
	if(black_in) {
		phase_ = Phase::menu;
		input_previous_none_ = !any_interface_input(input);
	} else {
		status_ = fade_out_terminal_;
	}
	return result(true);
}

NewClassicJoyConfigSceneResult NewClassicJoyConfigScene::consume(
	NewClassicUiInput input
) noexcept
{
	if(status_ != NewClassicJoyConfigSceneStatus::running) {
		return result();
	}
	if(input.app_quit_requested) {
		status_ = NewClassicJoyConfigSceneStatus::application_quit;
		return result();
	}
	if(phase_ == Phase::fade_in || phase_ == Phase::fade_out) {
		return advance_fade(input);
	}
	const bool any = any_interface_input(input);
	if(phase_ == Phase::discard) {
		if(input_previous_none_) {
			if(input.confirm) {
				config_ = original_;
				begin_fade_out(NewClassicJoyConfigSceneStatus::back);
				input_previous_none_ = false;
				return result();
			}
			if(input.cancel) {
				phase_ = Phase::menu;
				input_previous_none_ = false;
				if(!compose_screen()) {
					fail_surface();
					return result();
				}
				return result(true);
			}
		}
		input_previous_none_ = !any;
		return result();
	}
	if(!input_previous_none_) {
		input_previous_none_ = !any;
		return result();
	}
	if(!any) {
		return result();
	}
	input_previous_none_ = false;
	if(input.up) {
		selected_ = selected_ == 0U ? cancel_row :
			static_cast<std::uint8_t>(selected_ - 1U);
	} else if(input.down) {
		selected_ = selected_ == cancel_row ? 0U :
			static_cast<std::uint8_t>(selected_ + 1U);
	} else if(input.left || input.right) {
		if(!adjust(input.left ? -1 : 1)) {
			fail_surface();
			return result();
		}
	} else if(input.confirm) {
		if(selected_ < switch_page_row) {
			if(!adjust(1)) {
				fail_surface();
				return result();
			}
		} else if(selected_ == switch_page_row) {
			page_ = page_ == Page::buttons ? Page::axes : Page::buttons;
			selected_ = 0U;
		} else if(selected_ == defaults_row) {
			restore_defaults();
		} else if(selected_ == apply_row) {
			status_ = NewClassicJoyConfigSceneStatus::apply_requested;
			return result();
		} else if(config_ == original_) {
			begin_fade_out(NewClassicJoyConfigSceneStatus::back);
			return result();
		} else {
			phase_ = Phase::discard;
			if(!show_overlay(patch_assets_->messages().text(
				NewClassicMessageId::input_prompt_discard
			))) {
				fail_surface();
				return result();
			}
			return result(true);
		}
	} else if(input.cancel) {
		if(config_ == original_) {
			begin_fade_out(NewClassicJoyConfigSceneStatus::back);
			return result();
		}
		phase_ = Phase::discard;
		if(!show_overlay(patch_assets_->messages().text(
			NewClassicMessageId::input_prompt_discard
		))) {
			fail_surface();
			return result();
		}
		return result(true);
	}
	if(!compose_screen()) {
		fail_surface();
		return result();
	}
	return result(true);
}

void NewClassicJoyConfigScene::report_commit(bool success) noexcept
{
	if(status_ != NewClassicJoyConfigSceneStatus::apply_requested) {
		return;
	}
	if(success) {
		original_ = config_;
		begin_fade_out(NewClassicJoyConfigSceneStatus::applied);
		return;
	}
	status_ = NewClassicJoyConfigSceneStatus::running;
	if(!show_overlay(patch_assets_->messages().text(
		NewClassicMessageId::joy_error_save
	))) {
		fail_surface();
	}
}

present::SourceSurface NewClassicJoyConfigScene::surface() const noexcept
{
	return patch_surface_.surface();
}

const NewClassicJoyConfig &NewClassicJoyConfigScene::config() const noexcept
{
	return config_;
}

NewClassicJoyConfigSceneStatus NewClassicJoyConfigScene::status() const noexcept
{
	return status_;
}

bool NewClassicJoyConfigScene::valid() const noexcept
{
	return valid_;
}

std::uint8_t NewClassicJoyConfigSceneTestAccess::page(
	const NewClassicJoyConfigScene &scene
) noexcept
{
	return static_cast<std::uint8_t>(scene.page_);
}

std::uint8_t NewClassicJoyConfigSceneTestAccess::selected_row(
	const NewClassicJoyConfigScene &scene
) noexcept
{
	return scene.selected_;
}

std::uint8_t NewClassicJoyConfigSceneTestAccess::phase(
	const NewClassicJoyConfigScene &scene
) noexcept
{
	return static_cast<std::uint8_t>(scene.phase_);
}

} // namespace th03::app
