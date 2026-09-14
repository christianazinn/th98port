#include "th03/app/new_classic/input/new_classic_key_config_scene.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "th03/present/pi.hpp"

namespace th03::app {
namespace {

constexpr present::Index4 color_header = 14U;
constexpr present::Index4 color_page_caret = 14U;
constexpr present::Index4 color_footer = 14U;
constexpr present::Index4 color_selected = 11U;
constexpr present::Index4 color_label = 12U;
constexpr present::Index4 color_value = 15U;

constexpr std::int32_t title_top = 4 * 16;
constexpr std::int32_t header_top = 6 * 16;
constexpr std::int32_t rows_top = 8 * 16;
constexpr std::int32_t player_action_top = 10 * 16;
constexpr std::int32_t command_top = 17 * 16;
constexpr std::int32_t story_label_left = 216;
constexpr std::int32_t story_value_left = 312;
constexpr std::int32_t player_action_label_left = 100;
constexpr std::int32_t player_action_value_left = 207;
constexpr std::int32_t player_movement_label_left = 337;
constexpr std::int32_t player_movement_value_left = 444;
constexpr std::int32_t command_center = 320;
constexpr std::int32_t cursor_gap = 16;
constexpr std::int32_t overlay_top = 20 * 16;
// palette_black_in/out(1): one alignment boundary, 17 PaletteTone values at
// one VSync apiece, then the final 100/0 publication performed after the loop.
constexpr std::uint8_t fade_boundary_count = 19U;

constexpr std::uint8_t player_autofire_row = 0U;
constexpr std::uint8_t player_up_left_row = 1U;
constexpr std::uint8_t player_down_right_row = 8U;
constexpr std::uint8_t player_shot_row = 9U;
constexpr std::uint8_t player_charge_row = 11U;
constexpr std::uint8_t player_defaults_row = 12U;
constexpr std::uint8_t player_apply_row = 13U;
constexpr std::uint8_t player_cancel_row = 14U;

constexpr std::uint8_t story_up_row = 0U;
constexpr std::uint8_t story_down_row = 3U;
constexpr std::uint8_t story_shot_row = 4U;
constexpr std::uint8_t story_charge_row = 6U;
constexpr std::uint8_t story_autofire_row = 7U;
constexpr std::uint8_t story_defaults_row = 8U;
constexpr std::uint8_t story_apply_row = 9U;
constexpr std::uint8_t story_cancel_row = 10U;

constexpr std::array<NewClassicMessageId, new_classic_player_action_count>
	player_action_labels{
		NewClassicMessageId::input_action_up_left,
		NewClassicMessageId::input_action_up,
		NewClassicMessageId::input_action_up_right,
		NewClassicMessageId::input_action_left,
		NewClassicMessageId::input_action_right,
		NewClassicMessageId::input_action_down_left,
		NewClassicMessageId::input_action_down,
		NewClassicMessageId::input_action_down_right,
		NewClassicMessageId::input_action_shot,
		NewClassicMessageId::input_action_bomb,
		NewClassicMessageId::input_action_charge,
	};
constexpr std::array<NewClassicMessageId, new_classic_story_action_count>
	story_action_labels{
		NewClassicMessageId::input_action_up,
		NewClassicMessageId::input_action_left,
		NewClassicMessageId::input_action_right,
		NewClassicMessageId::input_action_down,
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

[[nodiscard]] bool put_header(
	const NewClassicMenuFont &font,
	present::Raster &raster,
	std::int32_t left,
	std::int32_t top,
	std::string_view text
) noexcept
{
	return font.put(raster, left + 1, top + 1, text, 0U) &&
		font.put(raster, left, top, text, color_page_caret);
}

[[nodiscard]] constexpr std::size_t player_binding(
	std::uint8_t player,
	std::uint8_t action
) noexcept
{
	return (static_cast<std::size_t>(player) *
		new_classic_player_action_count) + action;
}

} // namespace

NewClassicKeyConfigScene::NewClassicKeyConfigScene(
	NewClassicKeyConfig config,
	NewClassicKeyConfig defaults,
	std::uint16_t maximum_key,
	std::uint16_t capture_cancel_key,
	NewClassicHostKeyName key_name,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	void *key_name_context,
	NewClassicLanguageAssetProfile language_profile
) :
	original_(config),
	config_(config),
	defaults_(defaults),
	maximum_key_(maximum_key),
	capture_cancel_key_(capture_cancel_key),
	key_name_(key_name),
	key_name_context_(key_name_context),
	language_profile_(language_profile),
	patch_assets_(std::move(patch_assets))
{
	if(!patch_assets_ || !patch_surface_.reset(
		patch_assets_->key_config_background
	)) {
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

bool NewClassicKeyConfigScene::draw_row(
	std::string_view label,
	std::string_view value,
	std::int32_t label_left,
	std::int32_t value_left,
	std::int32_t top,
	bool selected
) noexcept
{
	auto &raster = patch_surface_.raster();
	const auto &font = patch_assets_->menu_font;
	if(selected && !font.put(
		raster, label_left - cursor_gap, top, ">", color_selected
	)) {
		return false;
	}
	if(!font.put(
		raster, label_left, top, label,
		selected ? color_selected : color_label
	)) {
		return false;
	}
	return value.empty() || font.put(
		raster, value_left, top, value,
		selected ? color_selected : color_value
	);
}

bool NewClassicKeyConfigScene::draw_command(
	std::string_view label,
	std::int32_t top,
	bool selected
) noexcept
{
	auto &raster = patch_surface_.raster();
	const auto &font = patch_assets_->menu_font;
	const auto left = command_center - (font.width(label) / 2);
	if(selected && !font.put(
		raster, left - cursor_gap, top, ">", color_selected
	)) {
		return false;
	}
	return font.put(
		raster, left, top, label,
		selected ? color_selected : color_label
	);
}

std::string_view NewClassicKeyConfigScene::key_label(
	std::uint16_t key,
	std::string &fallback
) const noexcept
{
	const auto &messages = patch_assets_->messages(language_profile_);
	if(key == new_classic_host_key_unbound) {
		return messages.text(NewClassicMessageId::input_value_unbound);
	}
	if(key_name_) {
		const auto resolved = key_name_(key_name_context_, key);
		if(!resolved.empty()) {
			return resolved;
		}
	}
	const auto value = std::to_string(key);
	const std::array arguments{NewClassicMessageArgument{"key", value}};
	fallback = messages.format(NewClassicMessageId::key_value_fallback, arguments);
	return fallback;
}

bool NewClassicKeyConfigScene::draw_player_row(
	std::uint8_t player,
	std::uint8_t row,
	bool selected
) noexcept
{
	const auto &messages = patch_assets_->messages(language_profile_);
	if(row == player_autofire_row) {
		return draw_row(
			messages.text(NewClassicMessageId::input_action_autofire),
			messages.text((config_.autofire & (1U << player)) ?
				NewClassicMessageId::value_on : NewClassicMessageId::value_off),
			player_action_label_left, player_action_value_left,
			player_action_top + (3 * 16), selected
		);
	}
	if(row >= player_up_left_row && row <= player_charge_row) {
		const auto action = static_cast<std::uint8_t>(row - player_up_left_row);
		std::string fallback{};
		const auto value = key_label(
			config_.bindings[player_binding(player, action)], fallback
		);
		if(row <= player_down_right_row) {
			return draw_row(
				messages.text(player_action_labels[action]), value,
				player_movement_label_left, player_movement_value_left,
				rows_top + (static_cast<std::int32_t>(action) * 16), selected
			);
		}
		return draw_row(
			messages.text(player_action_labels[action]), value,
			player_action_label_left, player_action_value_left,
			player_action_top +
				(static_cast<std::int32_t>(row - player_shot_row) * 16),
			selected
		);
	}
	const auto top = command_top +
		(static_cast<std::int32_t>(row - player_defaults_row) * 16);
	if(row == player_defaults_row) {
		return draw_command(messages.text(
			NewClassicMessageId::key_command_defaults_player
		), top, selected);
	}
	if(row == player_apply_row) {
		return draw_command(messages.text(
			NewClassicMessageId::input_command_apply_return
		), top, selected);
	}
	return draw_command(messages.text(
		NewClassicMessageId::input_command_cancel
	), top, selected);
}

bool NewClassicKeyConfigScene::draw_story_row(
	std::uint8_t row,
	bool selected
) noexcept
{
	const auto &messages = patch_assets_->messages(language_profile_);
	const auto top = rows_top + (static_cast<std::int32_t>(
		row + (row >= story_defaults_row ? 1U : 0U)
	) * 16);
	if(row <= story_down_row) {
		std::string fallback{};
		const auto index = (2U * new_classic_player_action_count) + row;
		return draw_row(
			messages.text(story_action_labels[row]),
			key_label(config_.bindings[index], fallback),
			story_label_left, story_value_left, top, selected
		);
	}
	if(row >= story_shot_row && row <= story_charge_row) {
		const auto action = static_cast<std::size_t>(
			NewClassicPlayerAction::shot
		) + (row - story_shot_row);
		std::string fallback{};
		return draw_row(
			messages.text(player_action_labels[action]),
			key_label(config_.bindings[action], fallback),
			story_label_left, story_value_left, top, selected
		);
	}
	if(row == story_autofire_row) {
		return draw_row(
			messages.text(NewClassicMessageId::input_action_autofire),
			messages.text((config_.autofire & 1U) ?
				NewClassicMessageId::value_on : NewClassicMessageId::value_off),
			story_label_left, story_value_left, top, selected
		);
	}
	if(row == story_defaults_row) {
		return draw_command(messages.text(
			NewClassicMessageId::key_command_defaults_story
		), top, selected);
	}
	if(row == story_apply_row) {
		return draw_command(messages.text(
			NewClassicMessageId::input_command_apply_return
		), top, selected);
	}
	return draw_command(messages.text(
		NewClassicMessageId::input_command_cancel
	), top, selected);
}

bool NewClassicKeyConfigScene::compose_screen() noexcept
{
	if(!valid_ || !patch_assets_) {
		return false;
	}
	const auto target = opposite(front_page_);
	auto &raster = patch_surface_.raster();
	const auto &background = patch_assets_->key_config_background;
	if(!raster.set_access_page(target) || !present::pi_put_8(
		raster, 0, 0, background
	)) {
		return false;
	}
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages(language_profile_);
	const auto header = messages.text(page_ == Page::p1 ?
		NewClassicMessageId::key_header_p1 : page_ == Page::p2 ?
		NewClassicMessageId::key_header_p2 :
		NewClassicMessageId::key_header_story);
	if(!put_header_centered(
			font, raster, 320, title_top,
			messages.text(NewClassicMessageId::option_key_config)
		) || !put_header(font, raster, 248, header_top, "<") ||
		!put_header_centered(font, raster, 320, header_top, header) ||
		!put_header(font, raster, 380, header_top, ">")) {
		return false;
	}
	if(page_ == Page::story) {
		for(std::uint8_t row = 0U; row <= story_cancel_row; ++row) {
			if(!draw_story_row(row, row == selected_)) {
				return false;
			}
		}
	} else {
		const auto player = static_cast<std::uint8_t>(page_);
		for(std::uint8_t row = 0U; row <= player_cancel_row; ++row) {
			if(!draw_player_row(player, row, row == selected_)) {
				return false;
			}
		}
	}
	if(!patch_surface_.publish(target) || !raster.set_access_page(target)) {
		return false;
	}
	front_page_ = target;
	return true;
}

bool NewClassicKeyConfigScene::show_overlay(std::string_view text) noexcept
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

bool NewClassicKeyConfigScene::any_interface_input(
	const NewClassicUiInput &input
) noexcept
{
	return input.up || input.down || input.left || input.right ||
		input.confirm || input.bomb || input.cancel;
}

NewClassicKeyConfigSceneResult NewClassicKeyConfigScene::result(
	bool surface_changed
) const noexcept
{
	return {.surface_changed = surface_changed, .status = status_};
}

void NewClassicKeyConfigScene::fail_surface() noexcept
{
	valid_ = false;
	status_ = NewClassicKeyConfigSceneStatus::surface_failed;
}

NewClassicKeyConfigSceneResult NewClassicKeyConfigScene::advance_fade(
	const NewClassicKeyConfigSceneInput &input
) noexcept
{
	const bool black_in = phase_ == Phase::fade_in;
	auto &raster = patch_surface_.raster();
	bool changed = false;
	if(fade_boundary_ != 0U && fade_boundary_ <= 17U) {
		const auto pair = static_cast<std::int16_t>(
			fade_boundary_ - 1U
		);
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
		input_previous_none_ = !any_interface_input(input.ui);
	} else {
		status_ = fade_out_terminal_;
	}
	return result(true);
}

void NewClassicKeyConfigScene::begin_fade_out(
	NewClassicKeyConfigSceneStatus terminal
) noexcept
{
	phase_ = Phase::fade_out;
	fade_boundary_ = 0U;
	fade_out_terminal_ = terminal;
	status_ = NewClassicKeyConfigSceneStatus::running;
}

std::uint8_t NewClassicKeyConfigScene::player_selected_row(
	bool movement_column,
	std::uint8_t at
) noexcept
{
	if(movement_column) {
		return at < 8U ? static_cast<std::uint8_t>(player_up_left_row + at) :
			static_cast<std::uint8_t>(player_defaults_row + (at - 8U));
	}
	if(at < 3U) {
		return static_cast<std::uint8_t>(player_shot_row + at);
	}
	return at == 3U ? player_autofire_row :
		static_cast<std::uint8_t>(player_defaults_row + (at - 4U));
}

void NewClassicKeyConfigScene::set_page(Page page) noexcept
{
	page_ = page;
	if(page_ == Page::story) {
		selected_ = story_up_row;
		return;
	}
	movement_column_ = false;
	player_at_ = 0U;
	selected_ = player_shot_row;
}

std::optional<std::size_t> NewClassicKeyConfigScene::binding_index() const noexcept
{
	if(page_ == Page::story) {
		if(selected_ <= story_down_row) {
			return (2U * new_classic_player_action_count) + selected_;
		}
		if(selected_ >= story_shot_row && selected_ <= story_charge_row) {
			return static_cast<std::size_t>(NewClassicPlayerAction::shot) +
				(selected_ - story_shot_row);
		}
		return std::nullopt;
	}
	if(selected_ >= player_up_left_row && selected_ <= player_charge_row) {
		return player_binding(
			static_cast<std::uint8_t>(page_),
			static_cast<std::uint8_t>(selected_ - player_up_left_row)
		);
	}
	return std::nullopt;
}

void NewClassicKeyConfigScene::assign(std::uint16_t key) noexcept
{
	if(!capture_binding_ || key == 0U || key >= maximum_key_) {
		return;
	}
	const auto index = *capture_binding_;
	const auto old = config_.bindings[index];
	const auto first = index < 2U * new_classic_player_action_count ? 0U :
		2U * new_classic_player_action_count;
	const auto end = index < 2U * new_classic_player_action_count ?
		2U * new_classic_player_action_count : new_classic_key_binding_count;
	for(std::size_t other = first; other < end; ++other) {
		if(other != index && config_.bindings[other] == key) {
			config_.bindings[other] = old;
			break;
		}
	}
	config_.bindings[index] = key;
}

void NewClassicKeyConfigScene::restore_defaults() noexcept
{
	if(page_ == Page::story) {
		for(std::size_t action = 0U; action < new_classic_story_action_count;
			++action) {
			const auto index = (2U * new_classic_player_action_count) + action;
			config_.bindings[index] = defaults_.bindings[index];
		}
		for(std::size_t action = static_cast<std::size_t>(
			NewClassicPlayerAction::shot
		); action <= static_cast<std::size_t>(NewClassicPlayerAction::charge);
			++action) {
			config_.bindings[action] = defaults_.bindings[action];
		}
		config_.autofire = static_cast<std::uint8_t>(config_.autofire & ~1U);
		return;
	}
	const auto player = static_cast<std::uint8_t>(page_);
	for(std::uint8_t action = 0U; action < new_classic_player_action_count;
		++action) {
		const auto index = player_binding(player, action);
		config_.bindings[index] = defaults_.bindings[index];
	}
	config_.autofire = static_cast<std::uint8_t>(
		config_.autofire & ~(1U << player)
	);
}

NewClassicKeyConfigSceneResult NewClassicKeyConfigScene::consume(
	NewClassicKeyConfigSceneInput input
) noexcept
{
	if(status_ != NewClassicKeyConfigSceneStatus::running) {
		return result();
	}
	if(input.ui.app_quit_requested) {
		status_ = NewClassicKeyConfigSceneStatus::application_quit;
		return result();
	}
	if(phase_ == Phase::fade_in || phase_ == Phase::fade_out) {
		return advance_fade(input);
	}
	if(phase_ == Phase::capture_wait_release) {
		if(!input.any_scancode_held) {
			phase_ = Phase::capture_wait_press;
		}
		return result();
	}
	if(phase_ == Phase::capture_wait_press) {
		if(!input.scancode_pressed) {
			return result();
		}
		if(*input.scancode_pressed != capture_cancel_key_) {
			if(*input.scancode_pressed == 0U ||
				*input.scancode_pressed >= maximum_key_) {
				return result();
			}
			assign(*input.scancode_pressed);
		}
		phase_ = Phase::capture_wait_final_release;
		return result();
	}
	if(phase_ == Phase::capture_wait_final_release) {
		if(!input.any_scancode_held) {
			phase_ = Phase::menu;
			capture_binding_.reset();
			input_previous_none_ = false;
			if(!compose_screen()) {
				fail_surface();
				return result();
			}
			return result(true);
		}
		return result();
	}

	const bool any = any_interface_input(input.ui);
	if(phase_ == Phase::discard) {
		if(input_previous_none_) {
			if(input.ui.confirm) {
				config_ = original_;
				begin_fade_out(NewClassicKeyConfigSceneStatus::back);
				input_previous_none_ = false;
				return result();
			}
			if(input.ui.cancel) {
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
	if(input.ui.up) {
		if(page_ == Page::story) {
			selected_ = selected_ == 0U ? story_cancel_row :
				static_cast<std::uint8_t>(selected_ - 1U);
		} else {
			const auto count = static_cast<std::uint8_t>(
				movement_column_ ? 11U : 7U
			);
			player_at_ = player_at_ == 0U ?
				static_cast<std::uint8_t>(count - 1U) :
				static_cast<std::uint8_t>(player_at_ - 1U);
			selected_ = player_selected_row(movement_column_, player_at_);
		}
	} else if(input.ui.down) {
		if(page_ == Page::story) {
			selected_ = selected_ == story_cancel_row ? 0U :
				static_cast<std::uint8_t>(selected_ + 1U);
		} else {
			const auto count = static_cast<std::uint8_t>(
				movement_column_ ? 11U : 7U
			);
			player_at_ = player_at_ == (count - 1U) ? 0U :
				static_cast<std::uint8_t>(player_at_ + 1U);
			selected_ = player_selected_row(movement_column_, player_at_);
		}
	} else if(input.ui.left || input.ui.right) {
		const bool left = input.ui.left;
		const bool command = page_ != Page::story &&
			selected_ >= player_defaults_row;
		if(!command && page_ != Page::story &&
			((left && movement_column_) || (!left && !movement_column_))) {
			if(movement_column_) {
				movement_column_ = false;
				player_at_ = player_at_ < 2U ? 0U :
					player_at_ > 5U ? 3U :
					static_cast<std::uint8_t>(player_at_ - 2U);
			} else {
				movement_column_ = true;
				player_at_ = static_cast<std::uint8_t>(player_at_ + 2U);
			}
			selected_ = player_selected_row(movement_column_, player_at_);
		} else {
			const auto page = static_cast<std::uint8_t>(page_);
			set_page(static_cast<Page>(left ?
				(page == 0U ? 2U : page - 1U) :
				(page == 2U ? 0U : page + 1U)));
		}
	} else if(input.ui.confirm) {
		if((page_ != Page::story && selected_ == player_autofire_row) ||
			(page_ == Page::story && selected_ == story_autofire_row)) {
			const auto player = page_ == Page::p2 ? 1U : 0U;
			config_.autofire = static_cast<std::uint8_t>(
				config_.autofire ^ (1U << player)
			);
		} else if(const auto binding = binding_index()) {
			capture_binding_ = binding;
			phase_ = Phase::capture_wait_release;
			if(!show_overlay(patch_assets_->messages().text(
				NewClassicMessageId::key_prompt_capture
			))) {
				fail_surface();
				return result();
			}
			return result(true);
		} else if((page_ != Page::story &&
			selected_ == player_defaults_row) ||
			(page_ == Page::story && selected_ == story_defaults_row)) {
			restore_defaults();
		} else if((page_ != Page::story && selected_ == player_apply_row) ||
			(page_ == Page::story && selected_ == story_apply_row)) {
			status_ = NewClassicKeyConfigSceneStatus::apply_requested;
			return result();
		} else {
			if(config_ == original_) {
				begin_fade_out(NewClassicKeyConfigSceneStatus::back);
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
	} else if(input.ui.cancel) {
		if(config_ == original_) {
			begin_fade_out(NewClassicKeyConfigSceneStatus::back);
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

void NewClassicKeyConfigScene::report_commit(bool success) noexcept
{
	if(status_ != NewClassicKeyConfigSceneStatus::apply_requested) {
		return;
	}
	if(success) {
		original_ = config_;
		begin_fade_out(NewClassicKeyConfigSceneStatus::applied);
		return;
	}
	status_ = NewClassicKeyConfigSceneStatus::running;
	if(!show_overlay(patch_assets_->messages().text(
		NewClassicMessageId::key_error_save
	))) {
		fail_surface();
	}
}

present::SourceSurface NewClassicKeyConfigScene::surface() const noexcept
{
	return patch_surface_.surface();
}

const NewClassicKeyConfig &NewClassicKeyConfigScene::config() const noexcept
{
	return config_;
}

NewClassicKeyConfigSceneStatus NewClassicKeyConfigScene::status() const noexcept
{
	return status_;
}

bool NewClassicKeyConfigScene::valid() const noexcept
{
	return valid_;
}

std::uint8_t NewClassicKeyConfigSceneTestAccess::page(
	const NewClassicKeyConfigScene &scene
) noexcept
{
	return static_cast<std::uint8_t>(scene.page_);
}

std::uint8_t NewClassicKeyConfigSceneTestAccess::selected_row(
	const NewClassicKeyConfigScene &scene
) noexcept
{
	return scene.selected_;
}

bool NewClassicKeyConfigSceneTestAccess::movement_column(
	const NewClassicKeyConfigScene &scene
) noexcept
{
	return scene.movement_column_;
}

std::uint8_t NewClassicKeyConfigSceneTestAccess::phase(
	const NewClassicKeyConfigScene &scene
) noexcept
{
	return static_cast<std::uint8_t>(scene.phase_);
}

} // namespace th03::app
