#include "th03/app/new_classic_device_scene.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "th03/present/pi.hpp"

namespace th03::app {
namespace {

constexpr present::Index4 header_color = 14U;
constexpr present::Index4 selected_color = 11U;
constexpr present::Index4 label_color = 12U;
constexpr present::Index4 value_color = 15U;
constexpr std::int32_t header_top = 96;
constexpr std::int32_t rows_top = 128;
constexpr std::int32_t footer_top = 336;
constexpr std::int32_t label_left = 152;
constexpr std::int32_t value_left = 352;
constexpr std::int32_t center = 320;
constexpr std::uint8_t assignment_rows = 5U;
constexpr std::uint8_t defaults_row = 2U;
constexpr std::uint8_t apply_row = 3U;
constexpr std::uint8_t cancel_row = 4U;
constexpr std::uint8_t fade_boundaries = 19U;

[[nodiscard]] constexpr present::Page opposite(present::Page page) noexcept
{
	return page == present::Page::zero ? present::Page::one : present::Page::zero;
}

void append(std::array<char, 64U> &storage, std::size_t &used,
	std::string_view text) noexcept
{
	const auto count = std::min(text.size(), storage.size() - 1U - used);
	std::copy_n(text.begin(), count, storage.begin() +
		static_cast<std::ptrdiff_t>(used));
	used += count;
}

[[nodiscard]] std::string_view store(
	std::array<char, 64U> &storage,
	std::string_view text
) noexcept
{
	const auto count = std::min(text.size(), storage.size() - 1U);
	std::copy_n(text.begin(), count, storage.begin());
	return {storage.data(), count};
}

[[nodiscard]] std::string_view hex_word(
	std::uint16_t value,
	std::array<char, 64U> &storage,
	std::string_view prefix
) noexcept
{
	constexpr std::string_view digits = "0123456789ABCDEF";
	std::size_t used = 0U;
	append(storage, used, prefix);
	for(int shift = 12; shift >= 0; shift -= 4) {
		storage[used++] = digits[static_cast<std::size_t>(
			(value >> static_cast<unsigned int>(shift)) & 0x0FU
		)];
	}
	return {storage.data(), used};
}

[[nodiscard]] std::string_view controller_name(
	const NewClassicControllerSnapshot &controller
) noexcept
{
	std::size_t length = 0U;
	while(length < controller.name.size() && controller.name[length] != '\0') {
		++length;
	}
	return {controller.name.data(), length};
}

[[nodiscard]] std::string_view fit_controller_name(
	const NewClassicMenuFont &font,
	std::string_view prefix,
	std::string_view name,
	std::int32_t maximum_width
) noexcept
{
	while(!name.empty() &&
		font.width(prefix) + font.width(name) > maximum_width) {
		name.remove_suffix(1U);
	}
	return name;
}

[[nodiscard]] constexpr bool same_input(
	const CoreInput &left,
	const CoreInput &right
) noexcept
{
	return left.p1 == right.p1 && left.p2 == right.p2 &&
		left.system == right.system && left.charge_mask == right.charge_mask;
}

} // namespace

NewClassicDeviceScene::NewClassicDeviceScene(
	NewClassicDeviceSceneMode mode,
	NewClassicDeviceAssignment assignment,
	NewClassicDeviceAssignment defaults,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
) :
	mode_(mode),
	original_(assignment),
	assignment_(assignment),
	defaults_(defaults),
	patch_assets_(std::move(patch_assets))
{
	if(!valid_new_classic_device_assignment(assignment_) ||
		!valid_new_classic_device_assignment(defaults_) || !patch_assets_ ||
		!patch_surface_.reset(patch_assets_->key_config_background)) {
		fail();
		return;
	}
	auto &raster = patch_surface_.raster();
	raster.set_palette_tone(0);
	raster.apply_pending_palette();
	valid_ = true;
	if(!compose()) {
		fail();
	}
}

bool NewClassicDeviceScene::draw_value(
	std::string_view label,
	std::string_view value,
	std::int32_t top,
	bool selected
) noexcept
{
	auto &raster = patch_surface_.raster();
	const auto &font = patch_assets_->menu_font;
	return (!selected || font.put(
		raster, label_left - 16, top, ">", selected_color
	)) && font.put(raster, label_left, top, label,
		selected ? selected_color : label_color) &&
		font.put(raster, value_left, top, value,
			selected ? selected_color : value_color);
}

bool NewClassicDeviceScene::draw_command(
	std::string_view label,
	std::int32_t top,
	bool selected
) noexcept
{
	auto &raster = patch_surface_.raster();
	const auto &font = patch_assets_->menu_font;
	const auto left = center - (font.width(label) / 2);
	return (!selected || font.put(
		raster, left - 16, top, ">", selected_color
	)) && font.put(raster, left, top, label,
		selected ? selected_color : label_color);
}

std::string_view NewClassicDeviceScene::assignment_value(
	std::size_t player,
	std::array<char, 64U> &storage
) const noexcept
{
	const auto &messages = patch_assets_->messages();
	const auto mode = assignment_.mode[player];
	if(mode == NewClassicControllerAssignmentMode::automatic) {
		return messages.text(NewClassicMessageId::device_value_automatic);
	}
	if(mode == NewClassicControllerAssignmentMode::disabled) {
		return messages.text(NewClassicMessageId::device_value_disabled);
	}
	for(std::size_t index = 0U; index < catalog_.count; ++index) {
		if(catalog_.controllers[index].identity == assignment_.identity[player]) {
			const auto name = fit_controller_name(
				patch_assets_->menu_font, {},
				controller_name(catalog_.controllers[index]), 280
			);
			return store(storage, name);
		}
	}
	constexpr std::string_view digits = "0123456789ABCDEF";
	std::array<char, 8U> identifier{};
	for(std::size_t index = 0U; index < 4U; ++index) {
		const auto value = assignment_.identity[player].bytes[index];
		identifier[index * 2U] = digits[value >> 4U];
		identifier[(index * 2U) + 1U] = digits[value & 0x0FU];
	}
	const std::string_view id{identifier.data(), identifier.size()};
	const std::array arguments{NewClassicMessageArgument{"id", id}};
	return store(storage, messages.format(
		NewClassicMessageId::device_value_disconnected, arguments
	));
}

bool NewClassicDeviceScene::compose_assignment() noexcept
{
	const auto &messages = patch_assets_->messages();
	for(std::size_t player = 0U; player < 2U; ++player) {
		std::array<char, 64U> value{};
		if(!draw_value(messages.text(player == 0U ?
				NewClassicMessageId::device_label_p1_controller :
				NewClassicMessageId::device_label_p2_controller),
			assignment_value(player, value),
			rows_top + static_cast<std::int32_t>(player * 24U),
			selected_ == player)) {
			return false;
		}
	}
	return draw_command(messages.text(
		NewClassicMessageId::input_command_restore_defaults
	), rows_top + 64,
		selected_ == defaults_row) &&
		draw_command(messages.text(
			NewClassicMessageId::input_command_apply_return
		), rows_top + 88,
			selected_ == apply_row) &&
		draw_command(messages.text(
			NewClassicMessageId::input_command_cancel
		), rows_top + 112,
			selected_ == cancel_row);
}

bool NewClassicDeviceScene::compose_input_test() noexcept
{
	const auto &messages = patch_assets_->messages();
	std::array<char, 64U> storage{};
	if(!draw_value(messages.text(NewClassicMessageId::device_label_p1_logical),
		hex_word(logical_input_.p1, storage, ""),
		rows_top, false)) {
		return false;
	}
	storage.fill('\0');
	if(!draw_value(messages.text(NewClassicMessageId::device_label_p2_logical),
		hex_word(logical_input_.p2, storage, ""),
		rows_top + 20, false)) {
		return false;
	}
	storage.fill('\0');
	if(!draw_value(messages.text(NewClassicMessageId::device_label_system),
		hex_word(logical_input_.system, storage, ""),
		rows_top + 40, false)) {
		return false;
	}
	const auto shown = std::min<std::size_t>(catalog_.count, 5U);
	for(std::size_t index = 0U; index < shown; ++index) {
		const auto &controller = catalog_.controllers[index];
		const auto owner = messages.text(
			controller.player == NewClassicControllerPlayer::p1 ?
				NewClassicMessageId::device_owner_p1 :
			controller.player == NewClassicControllerPlayer::p2 ?
				NewClassicMessageId::device_owner_p2 :
				NewClassicMessageId::device_owner_none
		);
		const std::array empty_arguments{
			NewClassicMessageArgument{"owner", owner},
			NewClassicMessageArgument{"name", ""},
		};
		const auto prefix = messages.format(
			NewClassicMessageId::device_controller_row, empty_arguments
		);
		const auto name = fit_controller_name(
			patch_assets_->menu_font, prefix, controller_name(controller), 184
		);
		const std::array arguments{
			NewClassicMessageArgument{"owner", owner},
			NewClassicMessageArgument{"name", name},
		};
		const auto label = messages.format(
			NewClassicMessageId::device_controller_row, arguments
		);
		std::array<char, 64U> value{};
		const auto bits = static_cast<std::uint16_t>(
			controller.p1 | controller.p2 | controller.system
		);
		if(!draw_value(label, hex_word(bits, value, ""),
			rows_top + 72 + static_cast<std::int32_t>(index * 20U), false)) {
			return false;
		}
	}
	if(catalog_.count == 0U) {
		return draw_command(messages.text(
			NewClassicMessageId::device_no_controllers
		), rows_top + 88, false);
	}
	return true;
}

bool NewClassicDeviceScene::compose() noexcept
{
	if(!valid_ || !patch_assets_) {
		return false;
	}
	const auto target = opposite(front_page_);
	auto &raster = patch_surface_.raster();
	const auto &messages = patch_assets_->messages();
	if(!raster.set_access_page(target) || !present::pi_put_8(
		raster, 0, 0, patch_assets_->key_config_background
	) || !patch_assets_->menu_font.put_centered(
		raster, center, header_top,
		messages.text(mode_ == NewClassicDeviceSceneMode::assignment ?
			NewClassicMessageId::device_header_assignment :
			NewClassicMessageId::device_header_input_test),
		header_color
	) || !(mode_ == NewClassicDeviceSceneMode::assignment ?
		compose_assignment() : compose_input_test()) ||
		(mode_ == NewClassicDeviceSceneMode::input_test &&
		!patch_assets_->menu_font.put_centered(
		raster, center, footer_top,
			messages.text(NewClassicMessageId::device_footer_input_test),
			header_color
		)) || !patch_surface_.publish(target) ||
		!raster.set_access_page(target)) {
		return false;
	}
	front_page_ = target;
	return true;
}

bool NewClassicDeviceScene::show_overlay(std::string_view text) noexcept
{
	auto &raster = patch_surface_.raster();
	return raster.set_access_page(front_page_) &&
		patch_assets_->menu_font.put_centered(
			raster, center, 320, text, header_color
		) && patch_surface_.publish(front_page_) &&
		raster.set_access_page(front_page_);
}

bool NewClassicDeviceScene::adjust(int direction) noexcept
{
	if(selected_ >= 2U) {
		return true;
	}
	const auto player = static_cast<std::size_t>(selected_);
	std::size_t current = 0U;
	if(assignment_.mode[player] == NewClassicControllerAssignmentMode::disabled) {
		current = 1U;
	} else if(assignment_.mode[player] ==
		NewClassicControllerAssignmentMode::specific) {
		current = 1U;
		for(std::size_t index = 0U; index < catalog_.count; ++index) {
			if(catalog_.controllers[index].identity == assignment_.identity[player]) {
				current = index + 2U;
				break;
			}
		}
	}
	const auto count = catalog_.count + 2U;
	current = direction < 0 ? (current == 0U ? count - 1U : current - 1U) :
		(current + 1U) % count;
	assignment_.identity[player] = {};
	if(current == 0U) {
		assignment_.mode[player] =
			NewClassicControllerAssignmentMode::automatic;
	} else if(current == 1U) {
		assignment_.mode[player] =
			NewClassicControllerAssignmentMode::disabled;
	} else {
		assignment_.mode[player] =
			NewClassicControllerAssignmentMode::specific;
		assignment_.identity[player] = catalog_.controllers[current - 2U].identity;
		const auto other = 1U - player;
		if(assignment_.mode[other] ==
			NewClassicControllerAssignmentMode::specific &&
			assignment_.identity[other] == assignment_.identity[player]) {
			assignment_.mode[other] =
				NewClassicControllerAssignmentMode::automatic;
			assignment_.identity[other] = {};
		}
	}
	return valid_new_classic_device_assignment(assignment_);
}

bool NewClassicDeviceScene::any(const NewClassicUiInput &input) noexcept
{
	return input.up || input.down || input.left || input.right || input.confirm ||
		input.bomb || input.cancel;
}

NewClassicDeviceSceneResult NewClassicDeviceScene::result(bool changed)
	const noexcept
{
	return {.surface_changed = changed, .status = status_};
}

void NewClassicDeviceScene::fail() noexcept
{
	valid_ = false;
	status_ = NewClassicDeviceSceneStatus::surface_failed;
}

void NewClassicDeviceScene::begin_fade(
	NewClassicDeviceSceneStatus terminal
) noexcept
{
	phase_ = Phase::fade_out;
	fade_boundary_ = 0U;
	fade_terminal_ = terminal;
}

NewClassicDeviceSceneResult NewClassicDeviceScene::advance_fade(
	const NewClassicUiInput &input
) noexcept
{
	const bool black_in = phase_ == Phase::fade_in;
	auto &raster = patch_surface_.raster();
	bool changed = false;
	if(fade_boundary_ != 0U && fade_boundary_ <= 17U) {
		const auto step = static_cast<std::int16_t>(fade_boundary_ - 1U) * 6;
		raster.set_palette_tone(static_cast<std::int16_t>(
			black_in ? step : 100 - step
		));
		raster.apply_pending_palette();
		changed = true;
	}
	++fade_boundary_;
	if(fade_boundary_ < fade_boundaries) {
		return result(changed);
	}
	raster.set_palette_tone(black_in ? 100 : 0);
	raster.apply_pending_palette();
	if(black_in) {
		phase_ = Phase::menu;
		input_previous_none_ = !any(input);
	} else {
		status_ = fade_terminal_;
	}
	return result(true);
}

NewClassicDeviceSceneResult NewClassicDeviceScene::consume(
	NewClassicUiInput input,
	const NewClassicControllerCatalog &catalog,
	CoreInput logical_input
) noexcept
{
	if(status_ != NewClassicDeviceSceneStatus::running) {
		return result();
	}
	if(input.app_quit_requested) {
		status_ = NewClassicDeviceSceneStatus::application_quit;
		return result();
	}
	const bool facts_changed = catalog_ != catalog ||
		!same_input(logical_input_, logical_input);
	catalog_ = catalog;
	logical_input_ = logical_input;
	if(phase_ == Phase::fade_in || phase_ == Phase::fade_out) {
		if(phase_ == Phase::fade_in && facts_changed && !compose()) {
			fail();
			return result();
		}
		auto advanced = advance_fade(input);
		advanced.surface_changed = advanced.surface_changed || facts_changed;
		return advanced;
	}
	if(mode_ == NewClassicDeviceSceneMode::input_test) {
		if(input_previous_none_ && input.cancel) {
			begin_fade(NewClassicDeviceSceneStatus::back);
			input_previous_none_ = false;
			return result();
		}
		input_previous_none_ = !any(input);
		if(facts_changed && !compose()) {
			fail();
			return result();
		}
		return result(facts_changed);
	}
	const bool interface_any = any(input);
	if(phase_ == Phase::discard) {
		if(input_previous_none_ && input.confirm) {
			assignment_ = original_;
			begin_fade(NewClassicDeviceSceneStatus::back);
			input_previous_none_ = false;
			return result();
		}
		if(input_previous_none_ && input.cancel) {
			phase_ = Phase::menu;
			input_previous_none_ = false;
			if(!compose()) {
				fail();
			}
			return result(true);
		}
		input_previous_none_ = !interface_any;
		return result();
	}
	if(!input_previous_none_) {
		input_previous_none_ = !interface_any;
		return result();
	}
	if(!interface_any) {
		if(facts_changed && !compose()) {
			fail();
			return result();
		}
		return result(facts_changed);
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
			fail();
			return result();
		}
	} else if(input.confirm) {
		if(selected_ < 2U) {
			if(!adjust(1)) {
				fail();
				return result();
			}
		} else if(selected_ == defaults_row) {
			assignment_ = defaults_;
		} else if(selected_ == apply_row) {
			status_ = NewClassicDeviceSceneStatus::apply_requested;
			return result();
		} else if(assignment_ == original_) {
			begin_fade(NewClassicDeviceSceneStatus::back);
			return result();
		} else {
			phase_ = Phase::discard;
			if(!show_overlay(patch_assets_->messages().text(
				NewClassicMessageId::input_prompt_discard
			))) {
				fail();
			}
			return result(true);
		}
	} else if(input.cancel) {
		if(assignment_ == original_) {
			begin_fade(NewClassicDeviceSceneStatus::back);
			return result();
		}
		phase_ = Phase::discard;
		if(!show_overlay(patch_assets_->messages().text(
			NewClassicMessageId::input_prompt_discard
		))) {
			fail();
		}
		return result(true);
	}
	if(!compose()) {
		fail();
		return result();
	}
	return result(true);
}

void NewClassicDeviceScene::report_commit(bool success) noexcept
{
	if(status_ != NewClassicDeviceSceneStatus::apply_requested) {
		return;
	}
	if(success) {
		original_ = assignment_;
		status_ = NewClassicDeviceSceneStatus::running;
		begin_fade(NewClassicDeviceSceneStatus::applied);
	} else {
		status_ = NewClassicDeviceSceneStatus::running;
		if(!show_overlay(patch_assets_->messages().text(
			NewClassicMessageId::device_error_save
		))) {
			fail();
		}
	}
}

present::SourceSurface NewClassicDeviceScene::surface() const noexcept
{
	return patch_surface_.surface();
}

const NewClassicDeviceAssignment &NewClassicDeviceScene::assignment()
	const noexcept
{
	return assignment_;
}

NewClassicDeviceSceneStatus NewClassicDeviceScene::status() const noexcept
{
	return status_;
}

bool NewClassicDeviceScene::valid() const noexcept
{
	return valid_;
}

NewClassicDeviceSceneMode NewClassicDeviceScene::mode() const noexcept
{
	return mode_;
}

} // namespace th03::app
