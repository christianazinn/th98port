#include "th03/platform/sdl_input.hpp"

#include "sdl_runtime_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <utility>

namespace th03::platform {
namespace {

constexpr std::uint16_t input_up = 0x0001U;
constexpr std::uint16_t input_down = 0x0002U;
constexpr std::uint16_t input_left = 0x0004U;
constexpr std::uint16_t input_right = 0x0008U;
constexpr std::uint16_t input_bomb = 0x0010U;
constexpr std::uint16_t input_shot = 0x0020U;
constexpr std::uint16_t input_up_left = 0x0100U;
constexpr std::uint16_t input_down_left = 0x0200U;
constexpr std::uint16_t input_up_right = 0x0400U;
constexpr std::uint16_t input_down_right = 0x0800U;
constexpr std::uint16_t input_cancel = 0x1000U;
constexpr std::uint16_t input_ok = 0x2000U;
constexpr std::uint16_t input_q = 0x4000U;

[[nodiscard]] constexpr bool valid_player(SdlInputPlayer player) noexcept
{
	return player == SdlInputPlayer::p1 || player == SdlInputPlayer::p2;
}

[[nodiscard]] constexpr bool valid_action(SdlLogicalAction action) noexcept
{
	return static_cast<std::uint8_t>(action) <=
		static_cast<std::uint8_t>(SdlLogicalAction::charge);
}

[[nodiscard]] constexpr bool valid_axis_direction(
	SdlAxisDirection direction
) noexcept
{
	return direction == SdlAxisDirection::negative ||
		direction == SdlAxisDirection::positive;
}

[[nodiscard]] constexpr bool valid_scancode(SDL_Scancode scancode) noexcept
{
	const auto value = static_cast<int>(scancode);
	return value > static_cast<int>(SDL_SCANCODE_UNKNOWN) &&
		value < static_cast<int>(SDL_SCANCODE_COUNT);
}

[[nodiscard]] constexpr bool valid_gamepad_button(
	SDL_GamepadButton button
) noexcept
{
	const auto value = static_cast<int>(button);
	return value >= 0 && value < static_cast<int>(SDL_GAMEPAD_BUTTON_COUNT);
}

[[nodiscard]] constexpr bool valid_gamepad_axis(
	SDL_GamepadAxis axis
) noexcept
{
	const auto value = static_cast<int>(axis);
	return value >= 0 && value < static_cast<int>(SDL_GAMEPAD_AXIS_COUNT);
}

[[nodiscard]] bool valid_bindings(
	const SdlInputBindings &bindings,
	SDL_WindowID main_window,
	const SdlGamepadBackend &backend
) noexcept
{
	if(main_window == 0U || backend.open == nullptr || backend.close == nullptr ||
		bindings.keyboard_count > bindings.keyboard.size() ||
		bindings.gamepad_button_count > bindings.gamepad_buttons.size() ||
		bindings.gamepad_axis_count > bindings.gamepad_axes.size() ||
		bindings.axis_release_threshold <= 0 ||
		bindings.axis_press_threshold <= bindings.axis_release_threshold) {
		return false;
	}
	for(std::size_t index = 0U; index < bindings.keyboard_count; ++index) {
		const auto &binding = bindings.keyboard[index];
		if(!valid_scancode(binding.scancode) || !valid_player(binding.player) ||
			!valid_action(binding.action)) {
			return false;
		}
	}
	for(std::size_t index = 0U;
		index < bindings.gamepad_button_count;
		++index) {
		const auto &binding = bindings.gamepad_buttons[index];
		if(!valid_gamepad_button(binding.button) ||
			!valid_action(binding.action)) {
			return false;
		}
	}
	for(std::size_t index = 0U;
		index < bindings.gamepad_axis_count;
		++index) {
		const auto &binding = bindings.gamepad_axes[index];
		if(!valid_gamepad_axis(binding.axis) ||
			!valid_axis_direction(binding.direction) ||
			!valid_action(binding.action)) {
			return false;
		}
	}
	for(const auto player : bindings.gamepad_players) {
		if(!valid_player(player)) {
			return false;
		}
	}
	return true;
}

void apply_action(
	app::PhysicalInputSourceState &state,
	SdlInputPlayer player,
	SdlLogicalAction action
) noexcept
{
	auto &player_bits = (player == SdlInputPlayer::p1) ? state.p1 : state.p2;
	switch(action) {
	case SdlLogicalAction::up:
		player_bits = static_cast<std::uint16_t>(player_bits | input_up);
		break;
	case SdlLogicalAction::down:
		player_bits = static_cast<std::uint16_t>(player_bits | input_down);
		break;
	case SdlLogicalAction::left:
		player_bits = static_cast<std::uint16_t>(player_bits | input_left);
		break;
	case SdlLogicalAction::right:
		player_bits = static_cast<std::uint16_t>(player_bits | input_right);
		break;
	case SdlLogicalAction::bomb:
		player_bits = static_cast<std::uint16_t>(player_bits | input_bomb);
		break;
	case SdlLogicalAction::shot:
		player_bits = static_cast<std::uint16_t>(player_bits | input_shot);
		break;
	case SdlLogicalAction::up_left:
		player_bits = static_cast<std::uint16_t>(player_bits | input_up_left);
		break;
	case SdlLogicalAction::down_left:
		player_bits = static_cast<std::uint16_t>(player_bits | input_down_left);
		break;
	case SdlLogicalAction::up_right:
		player_bits = static_cast<std::uint16_t>(player_bits | input_up_right);
		break;
	case SdlLogicalAction::down_right:
		player_bits = static_cast<std::uint16_t>(player_bits | input_down_right);
		break;
	case SdlLogicalAction::single_up:
		state.system = static_cast<std::uint16_t>(state.system | input_up);
		break;
	case SdlLogicalAction::single_down:
		state.system = static_cast<std::uint16_t>(state.system | input_down);
		break;
	case SdlLogicalAction::single_left:
		state.system = static_cast<std::uint16_t>(state.system | input_left);
		break;
	case SdlLogicalAction::single_right:
		state.system = static_cast<std::uint16_t>(state.system | input_right);
		break;
	case SdlLogicalAction::single_bomb:
		state.system = static_cast<std::uint16_t>(state.system | input_bomb);
		break;
	case SdlLogicalAction::single_shot:
		state.system = static_cast<std::uint16_t>(state.system | input_shot);
		break;
	case SdlLogicalAction::single_up_left:
		state.system = static_cast<std::uint16_t>(state.system | input_up_left);
		break;
	case SdlLogicalAction::single_down_left:
		state.system = static_cast<std::uint16_t>(state.system | input_down_left);
		break;
	case SdlLogicalAction::single_up_right:
		state.system = static_cast<std::uint16_t>(state.system | input_up_right);
		break;
	case SdlLogicalAction::single_down_right:
		state.system = static_cast<std::uint16_t>(state.system | input_down_right);
		break;
	case SdlLogicalAction::cancel:
		state.system = static_cast<std::uint16_t>(
			state.system | input_cancel
		);
		break;
	case SdlLogicalAction::ok:
		state.system = static_cast<std::uint16_t>(state.system | input_ok);
		break;
	case SdlLogicalAction::q:
		state.system = static_cast<std::uint16_t>(state.system | input_q);
		break;
	case SdlLogicalAction::charge:
		player_bits = static_cast<std::uint16_t>(player_bits | input_shot);
		state.charge_mask = static_cast<std::uint8_t>(
			state.charge_mask |
			((player == SdlInputPlayer::p1) ? 0x01U : 0x02U)
		);
		break;
	}
}

[[nodiscard]] constexpr bool same_source_state(
	const app::PhysicalInputSourceState &left,
	const app::PhysicalInputSourceState &right
) noexcept
{
	return left.p1 == right.p1 && left.p2 == right.p2 &&
		left.system == right.system &&
		left.charge_mask == right.charge_mask &&
		left.device_present == right.device_present;
}

[[nodiscard]] constexpr bool axis_active(
	bool was_active,
	std::int16_t value,
	SdlAxisDirection direction,
	std::int16_t press_threshold,
	std::int16_t release_threshold
) noexcept
{
	const auto wide_value = static_cast<std::int32_t>(value);
	const auto wide_press = static_cast<std::int32_t>(press_threshold);
	const auto wide_release = static_cast<std::int32_t>(release_threshold);
	if(direction == SdlAxisDirection::negative) {
		return was_active ? (wide_value < -wide_release) :
			(wide_value <= -wide_press);
	}
	return was_active ? (wide_value > wide_release) :
		(wide_value >= wide_press);
}

[[nodiscard]] SdlInputMapperError mapper_error(
	SdlInputMapperErrorCode code,
	SDL_JoystickID gamepad_id = 0U
) noexcept
{
	return {.code = code, .gamepad_id = gamepad_id};
}

[[nodiscard]] SdlInputMapperError tick_error(
	app::TickInputError error
) noexcept
{
	return {
		.code = SdlInputMapperErrorCode::tick_input,
		.tick_input = error,
	};
}

SDL_Gamepad *native_open_gamepad(
	void *,
	SDL_JoystickID instance_id
) noexcept
{
	return SDL_OpenGamepad(instance_id);
}

void native_close_gamepad(void *, SDL_Gamepad *gamepad) noexcept
{
	SDL_CloseGamepad(gamepad);
}

} // namespace

SdlGamepadBackend native_sdl_gamepad_backend() noexcept
{
	return {
		.open = native_open_gamepad,
		.close = native_close_gamepad,
	};
}

app::TickTimestampNs sdl_ticks_now_ns() noexcept
{
	return static_cast<app::TickTimestampNs>(SDL_GetTicksNS());
}

app::TickTimestampNs sdl_event_timestamp_ns(const SDL_Event &event) noexcept
{
	return static_cast<app::TickTimestampNs>(event.common.timestamp);
}

SdlInputMapper::SdlInputMapper(
	SdlInputBindings bindings,
	SDL_WindowID main_window,
	SdlGamepadBackend gamepad_backend,
	bool native_runtime_lease
) noexcept :
	bindings_(std::move(bindings)),
	main_window_(main_window),
	gamepad_backend_(gamepad_backend),
	native_runtime_lease_(native_runtime_lease)
{}

std::expected<SdlInputMapper, SdlInputMapperError> SdlInputMapper::create(
	SdlInputBindings bindings,
	SDL_WindowID main_window,
	SdlGamepadBackend gamepad_backend
) noexcept
{
	if(!valid_bindings(bindings, main_window, gamepad_backend)) {
		return std::unexpected(mapper_error(
			SdlInputMapperErrorCode::invalid_bindings
		));
	}
	const bool native_backend = gamepad_backend.context == nullptr &&
		gamepad_backend.open == native_open_gamepad &&
		gamepad_backend.close == native_close_gamepad;
	if(native_backend && !detail::acquire_platform_runtime_lease()) {
		return std::unexpected(mapper_error(
			SdlInputMapperErrorCode::runtime_unavailable
		));
	}
	return SdlInputMapper(
		std::move(bindings),
		main_window,
		gamepad_backend,
		native_backend
	);
}

SdlInputMapper::~SdlInputMapper()
{
	close_connected_gamepads();
	if(native_runtime_lease_) {
		detail::release_platform_runtime_lease();
	}
}

SdlInputMapper::SdlInputMapper(SdlInputMapper &&other) noexcept :
	bindings_(std::move(other.bindings_)),
	main_window_(other.main_window_),
	gamepad_backend_(other.gamepad_backend_),
	keyboard_(std::move(other.keyboard_)),
	raw_keyboard_held_(std::move(other.raw_keyboard_held_)),
	last_scancode_press_(other.last_scancode_press_),
	gamepads_(std::move(other.gamepads_)),
	connected_gamepad_count_(other.connected_gamepad_count_),
	native_runtime_lease_(other.native_runtime_lease_),
	pending_event_(other.pending_event_),
	has_pending_event_(other.has_pending_event_)
{
	for(auto &gamepad : other.gamepads_) {
		gamepad.handle = nullptr;
		gamepad.id = 0U;
	}
	other.connected_gamepad_count_ = 0U;
	other.gamepad_backend_ = {};
	other.native_runtime_lease_ = false;
	other.has_pending_event_ = false;
	other.raw_keyboard_held_.fill(false);
	other.last_scancode_press_.reset();
}

app::PhysicalInputSourceState SdlInputMapper::keyboard_contribution(
	const std::array<bool, sdl_keyboard_binding_capacity> &held
) const noexcept
{
	app::PhysicalInputSourceState state{};
	for(std::size_t index = 0U; index < bindings_.keyboard_count; ++index) {
		if(held[index]) {
			const auto &binding = bindings_.keyboard[index];
			apply_action(state, binding.player, binding.action);
		}
	}
	return state;
}

app::PhysicalInputSourceState SdlInputMapper::gamepad_contribution(
	std::size_t slot,
	const std::array<bool, sdl_gamepad_button_binding_capacity> &buttons,
	const std::array<bool, sdl_gamepad_axis_binding_capacity> &axes
) const noexcept
{
	app::PhysicalInputSourceState state{.device_present = true};
	const auto player = bindings_.gamepad_players[slot];
	for(std::size_t index = 0U;
		index < bindings_.gamepad_button_count;
		++index) {
		if(buttons[index]) {
			apply_action(state, player, bindings_.gamepad_buttons[index].action);
		}
	}
	for(std::size_t index = 0U;
		index < bindings_.gamepad_axis_count;
		++index) {
		if(axes[index]) {
			apply_action(state, player, bindings_.gamepad_axes[index].action);
		}
	}
	return state;
}

std::expected<void, SdlInputMapperError> SdlInputMapper::push_event(
	app::TickInputScheduler &scheduler,
	app::PhysicalInputEvent event
) const noexcept
{
	const auto pushed = scheduler.push_live_event(event);
	if(!pushed) {
		return std::unexpected(tick_error(pushed.error()));
	}
	return {};
}

void SdlInputMapper::clear_held_states() noexcept
{
	keyboard_.held.fill(false);
	keyboard_.contribution = {};
	raw_keyboard_held_.fill(false);
	last_scancode_press_.reset();
	for(auto &gamepad : gamepads_) {
		gamepad.button_held.fill(false);
		gamepad.axis_held.fill(false);
		gamepad.contribution = {
			.device_present = gamepad.handle != nullptr,
		};
	}
}

void SdlInputMapper::close_connected_gamepads() noexcept
{
	if(gamepad_backend_.close == nullptr) {
		return;
	}
	for(auto &gamepad : gamepads_) {
		if(gamepad.handle != nullptr) {
			gamepad_backend_.close(gamepad_backend_.context, gamepad.handle);
			gamepad = {};
		}
	}
	connected_gamepad_count_ = 0U;
}

std::size_t SdlInputMapper::connected_gamepad_count() const noexcept
{
	return connected_gamepad_count_;
}

std::optional<std::uint8_t> SdlInputMapper::source_for(
	SDL_JoystickID gamepad_id
) const noexcept
{
	for(std::size_t slot = 0U; slot < gamepads_.size(); ++slot) {
		if(gamepads_[slot].handle != nullptr && gamepads_[slot].id == gamepad_id) {
			return static_cast<std::uint8_t>(slot + 1U);
		}
	}
	return std::nullopt;
}

bool SdlInputMapper::keyboard_scancode_held(
	SDL_Scancode scancode
) const noexcept
{
	return valid_scancode(scancode) && raw_keyboard_held_[
		static_cast<std::size_t>(scancode)
	];
}

bool SdlInputMapper::any_keyboard_scancode_held() const noexcept
{
	for(const auto held : raw_keyboard_held_) {
		if(held) {
			return true;
		}
	}
	return false;
}

std::optional<SDL_Scancode>
SdlInputMapper::take_keyboard_scancode_press() noexcept
{
	return std::exchange(last_scancode_press_, std::nullopt);
}

std::expected<void, SdlInputMapperError> SdlInputMapper::replace_bindings(
	SdlInputBindings bindings
) noexcept
{
	if(!valid_bindings(bindings, main_window_, gamepad_backend_)) {
		return std::unexpected(mapper_error(
			SdlInputMapperErrorCode::invalid_bindings
		));
	}
	bindings_ = std::move(bindings);
	clear_held_states();
	return {};
}

std::expected<void, SdlInputMapperError> SdlInputMapper::reset_after_modal(
	app::TickInputScheduler &scheduler,
	app::TickTimestampNs timestamp_ns
) noexcept
{
	clear_held_states();
	has_pending_event_ = false;
	if(auto pushed = push_event(scheduler, {
		.timestamp_ns = timestamp_ns,
		.kind = app::PhysicalInputEventKind::source_state_changed,
		.source = 0U,
		.state = keyboard_.contribution,
	}); !pushed) {
		return pushed;
	}
	for(std::size_t slot = 0U; slot < gamepads_.size(); ++slot) {
		if(gamepads_[slot].handle == nullptr) {
			continue;
		}
		if(auto pushed = push_event(scheduler, {
			.timestamp_ns = timestamp_ns,
			.kind = app::PhysicalInputEventKind::source_state_changed,
			.source = static_cast<std::uint8_t>(slot + 1U),
			.state = gamepads_[slot].contribution,
		}); !pushed) {
			return pushed;
		}
	}
	return {};
}

std::expected<SdlInputEventResult, SdlInputMapperError>
SdlInputMapper::handle_event(
	const SDL_Event &event,
	app::TickInputScheduler &scheduler
) noexcept
{
	const auto timestamp = sdl_event_timestamp_ns(event);
	switch(event.type) {
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP: {
		const auto down = event.type == SDL_EVENT_KEY_DOWN;
		if(event.key.windowID != main_window_) {
			return SdlInputEventResult{};
		}
		if(event.key.down != down) {
			return std::unexpected(mapper_error(
				SdlInputMapperErrorCode::malformed_event
			));
		}
		// SDL's Windows raw-input backend reports some legitimate keys without
		// a portable scancode. In particular, Alt+PrintScreen produces the
		// SysRq scan (0x54) as SDL_SCANCODE_UNKNOWN. It is outside the game's
		// input vocabulary, but it is not a malformed event.
		if(event.key.scancode == SDL_SCANCODE_UNKNOWN) {
			return SdlInputEventResult{.handled = true};
		}
		if(!valid_scancode(event.key.scancode)) {
			return std::unexpected(mapper_error(
				SdlInputMapperErrorCode::malformed_event
			));
		}
		if(event.key.repeat) {
			return SdlInputEventResult{.handled = true};
		}
		const auto raw_index = static_cast<std::size_t>(event.key.scancode);
		const auto commit_raw = [&]() noexcept {
			raw_keyboard_held_[raw_index] = down;
			if(down) {
				last_scancode_press_ = event.key.scancode;
			}
		};
		auto candidate = keyboard_.held;
		bool matched = false;
		for(std::size_t index = 0U;
			index < bindings_.keyboard_count;
			++index) {
			if(bindings_.keyboard[index].scancode == event.key.scancode) {
				candidate[index] = down;
				matched = true;
			}
		}
		if(!matched) {
			commit_raw();
			return SdlInputEventResult{.handled = true};
		}
		const auto contribution = keyboard_contribution(candidate);
		if(same_source_state(contribution, keyboard_.contribution)) {
			keyboard_.held = candidate;
			commit_raw();
			return SdlInputEventResult{.handled = true};
		}
		const auto pushed = push_event(scheduler, {
			.timestamp_ns = timestamp,
			.kind = app::PhysicalInputEventKind::source_state_changed,
			.source = 0U,
			.state = contribution,
		});
		if(!pushed) {
			return std::unexpected(pushed.error());
		}
		keyboard_.held = candidate;
		keyboard_.contribution = contribution;
		commit_raw();
		return SdlInputEventResult{
			.handled = true,
			.physical_event_queued = true,
		};
	}
	case SDL_EVENT_KEYBOARD_REMOVED: {
		const auto pushed = push_event(scheduler, {
			.timestamp_ns = timestamp,
			.kind = app::PhysicalInputEventKind::source_released,
			.source = 0U,
		});
		if(!pushed) {
			return std::unexpected(pushed.error());
		}
		keyboard_ = {};
		raw_keyboard_held_.fill(false);
		last_scancode_press_.reset();
		return SdlInputEventResult{
			.handled = true,
			.physical_event_queued = true,
		};
	}
	case SDL_EVENT_GAMEPAD_ADDED: {
		const auto id = event.gdevice.which;
		if(id == 0U) {
			return std::unexpected(mapper_error(
				SdlInputMapperErrorCode::malformed_event
			));
		}
		if(source_for(id)) {
			return SdlInputEventResult{.handled = true};
		}
		std::size_t slot = gamepads_.size();
		for(std::size_t index = 0U; index < gamepads_.size(); ++index) {
			if(gamepads_[index].handle == nullptr) {
				slot = index;
				break;
			}
		}
		if(slot == gamepads_.size()) {
			return std::unexpected(mapper_error(
				SdlInputMapperErrorCode::gamepad_capacity,
				id
			));
		}
		auto *handle = gamepad_backend_.open(gamepad_backend_.context, id);
		if(handle == nullptr) {
			return std::unexpected(mapper_error(
				SdlInputMapperErrorCode::gamepad_open_failed,
				id
			));
		}
		const auto source = static_cast<std::uint8_t>(slot + 1U);
		const auto pushed = push_event(scheduler, {
			.timestamp_ns = timestamp,
			.kind = app::PhysicalInputEventKind::source_state_changed,
			.source = source,
			.state = {.device_present = true},
		});
		if(!pushed) {
			gamepad_backend_.close(gamepad_backend_.context, handle);
			return std::unexpected(pushed.error());
		}
		gamepads_[slot].handle = handle;
		gamepads_[slot].id = id;
		gamepads_[slot].contribution.device_present = true;
		++connected_gamepad_count_;
		return SdlInputEventResult{
			.handled = true,
			.physical_event_queued = true,
		};
	}
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP: {
		const auto id = event.gbutton.which;
		const auto button = static_cast<SDL_GamepadButton>(event.gbutton.button);
		const auto down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
		if(id == 0U || event.gbutton.down != down ||
			!valid_gamepad_button(button)) {
			return std::unexpected(mapper_error(
				SdlInputMapperErrorCode::malformed_event,
				id
			));
		}
		const auto source = source_for(id);
		if(!source) {
			return SdlInputEventResult{};
		}
		const auto slot = static_cast<std::size_t>(*source - 1U);
		auto candidate = gamepads_[slot].button_held;
		bool matched = false;
		for(std::size_t index = 0U;
			index < bindings_.gamepad_button_count;
			++index) {
			if(bindings_.gamepad_buttons[index].button == button) {
				candidate[index] = down;
				matched = true;
			}
		}
		if(!matched) {
			return SdlInputEventResult{};
		}
		const auto contribution = gamepad_contribution(
			slot,
			candidate,
			gamepads_[slot].axis_held
		);
		if(same_source_state(contribution, gamepads_[slot].contribution)) {
			gamepads_[slot].button_held = candidate;
			return SdlInputEventResult{.handled = true};
		}
		const auto pushed = push_event(scheduler, {
			.timestamp_ns = timestamp,
			.kind = app::PhysicalInputEventKind::source_state_changed,
			.source = *source,
			.state = contribution,
		});
		if(!pushed) {
			return std::unexpected(pushed.error());
		}
		gamepads_[slot].button_held = candidate;
		gamepads_[slot].contribution = contribution;
		return SdlInputEventResult{
			.handled = true,
			.physical_event_queued = true,
		};
	}
	case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
		const auto id = event.gaxis.which;
		const auto axis = static_cast<SDL_GamepadAxis>(event.gaxis.axis);
		if(id == 0U || !valid_gamepad_axis(axis)) {
			return std::unexpected(mapper_error(
				SdlInputMapperErrorCode::malformed_event,
				id
			));
		}
		const auto source = source_for(id);
		if(!source) {
			return SdlInputEventResult{};
		}
		const auto slot = static_cast<std::size_t>(*source - 1U);
		auto candidate = gamepads_[slot].axis_held;
		bool matched = false;
		for(std::size_t index = 0U;
			index < bindings_.gamepad_axis_count;
			++index) {
			const auto &binding = bindings_.gamepad_axes[index];
			if(binding.axis == axis) {
				candidate[index] = axis_active(
					candidate[index],
					event.gaxis.value,
					binding.direction,
					bindings_.axis_press_threshold,
					bindings_.axis_release_threshold
				);
				matched = true;
			}
		}
		if(!matched) {
			return SdlInputEventResult{};
		}
		const auto contribution = gamepad_contribution(
			slot,
			gamepads_[slot].button_held,
			candidate
		);
		if(same_source_state(contribution, gamepads_[slot].contribution)) {
			gamepads_[slot].axis_held = candidate;
			return SdlInputEventResult{.handled = true};
		}
		const auto pushed = push_event(scheduler, {
			.timestamp_ns = timestamp,
			.kind = app::PhysicalInputEventKind::source_state_changed,
			.source = *source,
			.state = contribution,
		});
		if(!pushed) {
			return std::unexpected(pushed.error());
		}
		gamepads_[slot].axis_held = candidate;
		gamepads_[slot].contribution = contribution;
		return SdlInputEventResult{
			.handled = true,
			.physical_event_queued = true,
		};
	}
	case SDL_EVENT_GAMEPAD_REMAPPED:
	case SDL_EVENT_GAMEPAD_REMOVED: {
		const auto id = event.gdevice.which;
		if(id == 0U) {
			return std::unexpected(mapper_error(
				SdlInputMapperErrorCode::malformed_event
			));
		}
		const auto source = source_for(id);
		if(!source) {
			return SdlInputEventResult{};
		}
		const bool removed = event.type == SDL_EVENT_GAMEPAD_REMOVED;
		const auto pushed = push_event(scheduler, {
			.timestamp_ns = timestamp,
			.kind = removed
				? app::PhysicalInputEventKind::source_released
				: app::PhysicalInputEventKind::source_state_changed,
			.source = *source,
			.state = {.device_present = !removed},
		});
		if(!pushed) {
			return std::unexpected(pushed.error());
		}
		const auto slot = static_cast<std::size_t>(*source - 1U);
		if(removed) {
			gamepad_backend_.close(
				gamepad_backend_.context,
				gamepads_[slot].handle
			);
			gamepads_[slot] = {};
			--connected_gamepad_count_;
		} else {
			gamepads_[slot].button_held.fill(false);
			gamepads_[slot].axis_held.fill(false);
			gamepads_[slot].contribution = {.device_present = true};
		}
		return SdlInputEventResult{
			.handled = true,
			.physical_event_queued = true,
		};
	}
	case SDL_EVENT_WINDOW_FOCUS_LOST: {
		if(event.window.windowID != main_window_) {
			return SdlInputEventResult{};
		}
		const auto pushed = push_event(scheduler, {
			.timestamp_ns = timestamp,
			.kind = app::PhysicalInputEventKind::focus_released,
		});
		if(!pushed) {
			return std::unexpected(pushed.error());
		}
		clear_held_states();
		return SdlInputEventResult{
			.handled = true,
			.physical_event_queued = true,
		};
	}
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
		if(event.window.windowID != main_window_) {
			return SdlInputEventResult{};
		}
		const auto pushed = push_event(scheduler, {
			.timestamp_ns = timestamp,
			.kind = app::PhysicalInputEventKind::app_quit,
		});
		if(!pushed) {
			return std::unexpected(pushed.error());
		}
		return SdlInputEventResult{
			.handled = true,
			.physical_event_queued = true,
		};
	}
	case SDL_EVENT_QUIT: {
		const auto pushed = push_event(scheduler, {
			.timestamp_ns = timestamp,
			.kind = app::PhysicalInputEventKind::app_quit,
		});
		if(!pushed) {
			return std::unexpected(pushed.error());
		}
		return SdlInputEventResult{
			.handled = true,
			.physical_event_queued = true,
		};
	}
	default:
		return SdlInputEventResult{};
	}
}

std::expected<SdlInputPumpResult, SdlInputMapperError>
SdlInputMapper::pump_events(
	app::TickInputScheduler &scheduler,
	bool escape_requests_quit
) noexcept
{
	SdlInputPumpResult result{};
	for(;;) {
		SDL_Event event{};
		if(has_pending_event_) {
			event = pending_event_;
		} else if(!SDL_PollEvent(&event)) {
			return result;
		}

		std::expected<SdlInputEventResult, SdlInputMapperError> handled{};
		if(escape_requests_quit && event.type == SDL_EVENT_KEY_DOWN &&
			event.key.down && event.key.windowID == main_window_ &&
			!event.key.repeat &&
			event.key.scancode == SDL_SCANCODE_ESCAPE) {
			const auto pushed = push_event(scheduler, {
				.timestamp_ns = sdl_event_timestamp_ns(event),
				.kind = app::PhysicalInputEventKind::app_quit,
			});
			if(pushed) {
				handled = SdlInputEventResult{
					.handled = true,
					.physical_event_queued = true,
				};
			} else {
				handled = std::unexpected(pushed.error());
			}
		} else {
			handled = handle_event(event, scheduler);
		}
		if(!handled) {
			if(handled.error().code == SdlInputMapperErrorCode::tick_input &&
				handled.error().tick_input ==
					app::TickInputError::event_queue_full) {
				pending_event_ = event;
				has_pending_event_ = true;
				result.queue_blocked = true;
				return result;
			}
			return std::unexpected(handled.error());
		}
		has_pending_event_ = false;
		result.events_consumed++;
		if(handled->physical_event_queued) {
			result.physical_events_queued++;
		}
	}
}

} // namespace th03::platform
