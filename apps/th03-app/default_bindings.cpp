#include "default_bindings.hpp"

#include <cstddef>

namespace th03::app::detail {
namespace {

using platform::SdlAxisDirection;
using platform::SdlGamepadAxisBinding;
using platform::SdlGamepadButtonBinding;
using platform::SdlInputBindings;
using platform::SdlInputPlayer;
using platform::SdlKeyboardBinding;
using platform::SdlLogicalAction;

void add_key(
	SdlInputBindings &bindings,
	SDL_Scancode scancode,
	SdlInputPlayer player,
	SdlLogicalAction action
) noexcept
{
	bindings.keyboard[bindings.keyboard_count++] = SdlKeyboardBinding{
		.scancode = scancode,
		.player = player,
		.action = action,
	};
}

void add_dual(
	SdlInputBindings &bindings,
	SDL_Scancode scancode,
	SdlInputPlayer player,
	SdlLogicalAction player_action,
	SdlLogicalAction single_action
) noexcept
{
	add_key(bindings, scancode, player, player_action);
	add_key(bindings, scancode, player, single_action);
}

[[nodiscard]] constexpr SDL_GamepadButton sdl_button(
	NewClassicJoyButton button
) noexcept
{
	switch(button) {
	case NewClassicJoyButton::south: return SDL_GAMEPAD_BUTTON_SOUTH;
	case NewClassicJoyButton::east: return SDL_GAMEPAD_BUTTON_EAST;
	case NewClassicJoyButton::west: return SDL_GAMEPAD_BUTTON_WEST;
	case NewClassicJoyButton::north: return SDL_GAMEPAD_BUTTON_NORTH;
	case NewClassicJoyButton::back: return SDL_GAMEPAD_BUTTON_BACK;
	case NewClassicJoyButton::start: return SDL_GAMEPAD_BUTTON_START;
	case NewClassicJoyButton::left_stick: return SDL_GAMEPAD_BUTTON_LEFT_STICK;
	case NewClassicJoyButton::right_stick: return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
	case NewClassicJoyButton::left_shoulder:
		return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
	case NewClassicJoyButton::right_shoulder:
		return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
	case NewClassicJoyButton::dpad_up: return SDL_GAMEPAD_BUTTON_DPAD_UP;
	case NewClassicJoyButton::dpad_down: return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
	case NewClassicJoyButton::dpad_left: return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
	case NewClassicJoyButton::dpad_right: return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
	case NewClassicJoyButton::misc: return SDL_GAMEPAD_BUTTON_MISC1;
	case NewClassicJoyButton::unbound: return SDL_GAMEPAD_BUTTON_INVALID;
	}
	return SDL_GAMEPAD_BUTTON_INVALID;
}

[[nodiscard]] constexpr SDL_GamepadAxis sdl_axis(
	NewClassicJoyAxis axis
) noexcept
{
	switch(axis) {
	case NewClassicJoyAxis::left_x: return SDL_GAMEPAD_AXIS_LEFTX;
	case NewClassicJoyAxis::left_y: return SDL_GAMEPAD_AXIS_LEFTY;
	case NewClassicJoyAxis::right_x: return SDL_GAMEPAD_AXIS_RIGHTX;
	case NewClassicJoyAxis::right_y: return SDL_GAMEPAD_AXIS_RIGHTY;
	}
	return SDL_GAMEPAD_AXIS_INVALID;
}

void add_gamepad_button(
	SdlInputBindings &bindings,
	NewClassicJoyButton button,
	SdlLogicalAction action
) noexcept
{
	if(button == NewClassicJoyButton::unbound) {
		return;
	}
	bindings.gamepad_buttons[bindings.gamepad_button_count++] = {
		.button = sdl_button(button),
		.action = action,
	};
}

void add_gamepad_dual(
	SdlInputBindings &bindings,
	NewClassicJoyButton button,
	SdlLogicalAction player_action,
	SdlLogicalAction single_action
) noexcept
{
	add_gamepad_button(bindings, button, player_action);
	add_gamepad_button(bindings, button, single_action);
}

void add_gamepad_axis(
	SdlInputBindings &bindings,
	NewClassicJoyAxis axis,
	SdlAxisDirection direction,
	SdlLogicalAction action
) noexcept
{
	bindings.gamepad_axes[bindings.gamepad_axis_count++] = {
		.axis = sdl_axis(axis),
		.direction = direction,
		.action = action,
	};
}

} // namespace

NewClassicKeyConfig new_classic_default_key_config() noexcept
{
	NewClassicKeyConfig config{};
	constexpr std::array<SDL_Scancode, new_classic_player_action_count> p1{
		SDL_SCANCODE_R, SDL_SCANCODE_T, SDL_SCANCODE_Y,
		SDL_SCANCODE_F, SDL_SCANCODE_H,
		SDL_SCANCODE_V, SDL_SCANCODE_B, SDL_SCANCODE_N,
		SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_LSHIFT,
	};
	constexpr std::array<SDL_Scancode, new_classic_player_action_count> p2{
		SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9,
		SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6,
		SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3,
		SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_KP_5,
	};
	constexpr std::array<SDL_Scancode, new_classic_story_action_count> story{
		SDL_SCANCODE_UP, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
		SDL_SCANCODE_DOWN,
	};
	for(std::size_t index = 0U; index < p1.size(); ++index) {
		config.bindings[index] = static_cast<std::uint16_t>(p1[index]);
		config.bindings[new_classic_player_action_count + index] =
			static_cast<std::uint16_t>(p2[index]);
	}
	for(std::size_t index = 0U; index < story.size(); ++index) {
		config.bindings[(2U * new_classic_player_action_count) + index] =
			static_cast<std::uint16_t>(story[index]);
	}
	return config;
}

SdlInputBindings new_classic_bindings(
	const NewClassicKeyConfig &config,
	const NewClassicJoyConfig &joy_config,
	const NewClassicDeviceAssignment &device_assignment
) noexcept
{
	SdlInputBindings bindings{};
	bindings.charge_enabled_players = config.autofire;
	constexpr std::array<SdlLogicalAction, new_classic_player_action_count>
		actions{
			SdlLogicalAction::up_left, SdlLogicalAction::up,
			SdlLogicalAction::up_right, SdlLogicalAction::left,
			SdlLogicalAction::right, SdlLogicalAction::down_left,
			SdlLogicalAction::down, SdlLogicalAction::down_right,
			SdlLogicalAction::shot, SdlLogicalAction::bomb,
			SdlLogicalAction::charge,
		};
	constexpr std::array<SdlLogicalAction, new_classic_story_action_count>
		story_actions{
			SdlLogicalAction::single_up, SdlLogicalAction::single_left,
			SdlLogicalAction::single_right, SdlLogicalAction::single_down,
		};
	for(std::size_t player = 0U; player < 2U; ++player) {
		for(std::size_t action = 0U; action < actions.size(); ++action) {
			if(action == static_cast<std::size_t>(
				NewClassicPlayerAction::charge
			) && (config.autofire & (1U << player)) == 0U) {
				continue;
			}
			const auto key = config.bindings[
				(player * new_classic_player_action_count) + action
			];
			if(key != new_classic_host_key_unbound) {
				add_key(bindings, static_cast<SDL_Scancode>(key),
					player == 0U ? SdlInputPlayer::p1 : SdlInputPlayer::p2,
					actions[action]);
			}
		}
	}
	for(std::size_t action = 0U; action < story_actions.size(); ++action) {
		const auto key = config.bindings[
			(2U * new_classic_player_action_count) + action
		];
		if(key != new_classic_host_key_unbound) {
			add_key(bindings, static_cast<SDL_Scancode>(key),
				SdlInputPlayer::p1, story_actions[action]);
		}
	}

	// Interface/menu actions stay fixed, matching Replay Patch's raw input_sp
	// menu controls. They may intentionally share a physical key with a player
	// action without violating the configured player-binding uniqueness rule.
	add_key(bindings, SDL_SCANCODE_UP, SdlInputPlayer::p1,
		SdlLogicalAction::single_up);
	add_key(bindings, SDL_SCANCODE_DOWN, SdlInputPlayer::p1,
		SdlLogicalAction::single_down);
	add_key(bindings, SDL_SCANCODE_LEFT, SdlInputPlayer::p1,
		SdlLogicalAction::single_left);
	add_key(bindings, SDL_SCANCODE_RIGHT, SdlInputPlayer::p1,
		SdlLogicalAction::single_right);
	add_key(bindings, SDL_SCANCODE_Z, SdlInputPlayer::p1,
		SdlLogicalAction::single_shot);
	add_key(bindings, SDL_SCANCODE_X, SdlInputPlayer::p1,
		SdlLogicalAction::single_bomb);
	add_key(bindings, SDL_SCANCODE_ESCAPE, SdlInputPlayer::p1,
		SdlLogicalAction::cancel);
	add_key(bindings, SDL_SCANCODE_RETURN, SdlInputPlayer::p1,
		SdlLogicalAction::ok);
	add_key(bindings, SDL_SCANCODE_Q, SdlInputPlayer::p1,
		SdlLogicalAction::q);

	add_gamepad_dual(bindings, NewClassicJoyButton::dpad_up,
		SdlLogicalAction::up, SdlLogicalAction::single_up);
	add_gamepad_dual(bindings, NewClassicJoyButton::dpad_down,
		SdlLogicalAction::down, SdlLogicalAction::single_down);
	add_gamepad_dual(bindings, NewClassicJoyButton::dpad_left,
		SdlLogicalAction::left, SdlLogicalAction::single_left);
	add_gamepad_dual(bindings, NewClassicJoyButton::dpad_right,
		SdlLogicalAction::right, SdlLogicalAction::single_right);
	add_gamepad_dual(bindings,
		joy_config.buttons[static_cast<std::size_t>(NewClassicJoyAction::shot)],
		SdlLogicalAction::shot, SdlLogicalAction::single_shot);
	add_gamepad_dual(bindings,
		joy_config.buttons[static_cast<std::size_t>(NewClassicJoyAction::bomb)],
		SdlLogicalAction::bomb, SdlLogicalAction::single_bomb);
	if(config.autofire != 0U) {
		add_gamepad_button(bindings,
			joy_config.buttons[static_cast<std::size_t>(
				NewClassicJoyAction::charge
			)],
			SdlLogicalAction::charge);
	}
	add_gamepad_button(bindings,
		joy_config.buttons[static_cast<std::size_t>(NewClassicJoyAction::cancel)],
		SdlLogicalAction::cancel);
	add_gamepad_button(bindings,
		joy_config.buttons[static_cast<std::size_t>(NewClassicJoyAction::ok)],
		SdlLogicalAction::ok);

	const auto horizontal_negative = joy_config.horizontal_reversed ?
		SdlLogicalAction::right : SdlLogicalAction::left;
	const auto horizontal_positive = joy_config.horizontal_reversed ?
		SdlLogicalAction::left : SdlLogicalAction::right;
	const auto horizontal_single_negative = joy_config.horizontal_reversed ?
		SdlLogicalAction::single_right : SdlLogicalAction::single_left;
	const auto horizontal_single_positive = joy_config.horizontal_reversed ?
		SdlLogicalAction::single_left : SdlLogicalAction::single_right;
	add_gamepad_axis(bindings, joy_config.horizontal_axis,
		SdlAxisDirection::negative, horizontal_negative);
	add_gamepad_axis(bindings, joy_config.horizontal_axis,
		SdlAxisDirection::negative, horizontal_single_negative);
	add_gamepad_axis(bindings, joy_config.horizontal_axis,
		SdlAxisDirection::positive, horizontal_positive);
	add_gamepad_axis(bindings, joy_config.horizontal_axis,
		SdlAxisDirection::positive, horizontal_single_positive);

	const auto vertical_negative = joy_config.vertical_reversed ?
		SdlLogicalAction::down : SdlLogicalAction::up;
	const auto vertical_positive = joy_config.vertical_reversed ?
		SdlLogicalAction::up : SdlLogicalAction::down;
	const auto vertical_single_negative = joy_config.vertical_reversed ?
		SdlLogicalAction::single_down : SdlLogicalAction::single_up;
	const auto vertical_single_positive = joy_config.vertical_reversed ?
		SdlLogicalAction::single_up : SdlLogicalAction::single_down;
	add_gamepad_axis(bindings, joy_config.vertical_axis,
		SdlAxisDirection::negative, vertical_negative);
	add_gamepad_axis(bindings, joy_config.vertical_axis,
		SdlAxisDirection::negative, vertical_single_negative);
	add_gamepad_axis(bindings, joy_config.vertical_axis,
		SdlAxisDirection::positive, vertical_positive);
	add_gamepad_axis(bindings, joy_config.vertical_axis,
		SdlAxisDirection::positive, vertical_single_positive);

	for(std::size_t slot = 0U; slot < bindings.gamepad_players.size(); ++slot) {
		bindings.gamepad_players[slot] = (slot % 2U == 0U) ?
			SdlInputPlayer::p1 : SdlInputPlayer::p2;
	}
	bindings.identity_assignment_enabled = true;
	bindings.device_assignment = device_assignment;
	const auto press = static_cast<std::int32_t>(
		(32'768 * joy_config.dead_zone_percent) / 100U
	);
	bindings.axis_press_threshold = static_cast<std::int16_t>(press);
	bindings.axis_release_threshold = static_cast<std::int16_t>(
		(press * 3) / 4
	);
	return bindings;
}

SdlInputBindings new_classic_bindings(
	const NewClassicKeyConfig &config,
	const NewClassicJoyConfig &joy_config
) noexcept
{
	return new_classic_bindings(config, joy_config, {});
}

NewClassicJoyConfig new_classic_default_joy_config() noexcept
{
	return {
		.buttons = {
			NewClassicJoyButton::south,
			NewClassicJoyButton::east,
			NewClassicJoyButton::west,
			NewClassicJoyButton::back,
			NewClassicJoyButton::start,
		},
	};
}

SdlInputBindings new_classic_bindings(
	const NewClassicKeyConfig &key_config
) noexcept
{
	return new_classic_bindings(key_config, new_classic_default_joy_config());
}

SdlInputBindings classic_direct_vs_bindings() noexcept
{
	SdlInputBindings bindings{};
	add_key(bindings, SDL_SCANCODE_T, SdlInputPlayer::p1,
		SdlLogicalAction::up);
	add_key(bindings, SDL_SCANCODE_B, SdlInputPlayer::p1,
		SdlLogicalAction::down);
	add_key(bindings, SDL_SCANCODE_F, SdlInputPlayer::p1,
		SdlLogicalAction::left);
	add_key(bindings, SDL_SCANCODE_H, SdlInputPlayer::p1,
		SdlLogicalAction::right);
	add_key(bindings, SDL_SCANCODE_R, SdlInputPlayer::p1,
		SdlLogicalAction::up_left);
	add_key(bindings, SDL_SCANCODE_Y, SdlInputPlayer::p1,
		SdlLogicalAction::up_right);
	add_key(bindings, SDL_SCANCODE_V, SdlInputPlayer::p1,
		SdlLogicalAction::down_left);
	add_key(bindings, SDL_SCANCODE_N, SdlInputPlayer::p1,
		SdlLogicalAction::down_right);
	add_dual(bindings, SDL_SCANCODE_Z, SdlInputPlayer::p1,
		SdlLogicalAction::shot, SdlLogicalAction::single_shot);
	add_dual(bindings, SDL_SCANCODE_X, SdlInputPlayer::p1,
		SdlLogicalAction::bomb, SdlLogicalAction::single_bomb);
	add_key(bindings, SDL_SCANCODE_ESCAPE, SdlInputPlayer::p1,
		SdlLogicalAction::cancel);
	add_key(bindings, SDL_SCANCODE_RETURN, SdlInputPlayer::p1,
		SdlLogicalAction::ok);
	add_key(bindings, SDL_SCANCODE_Q, SdlInputPlayer::p1,
		SdlLogicalAction::q);
	add_key(bindings, SDL_SCANCODE_SPACE, SdlInputPlayer::p1,
		SdlLogicalAction::single_shot);
	add_key(bindings, SDL_SCANCODE_UP, SdlInputPlayer::p1,
		SdlLogicalAction::single_up);
	add_key(bindings, SDL_SCANCODE_DOWN, SdlInputPlayer::p1,
		SdlLogicalAction::single_down);

	add_dual(bindings, SDL_SCANCODE_KP_8, SdlInputPlayer::p2,
		SdlLogicalAction::up, SdlLogicalAction::single_up);
	add_dual(bindings, SDL_SCANCODE_KP_2, SdlInputPlayer::p2,
		SdlLogicalAction::down, SdlLogicalAction::single_down);
	add_dual(bindings, SDL_SCANCODE_KP_4, SdlInputPlayer::p2,
		SdlLogicalAction::left, SdlLogicalAction::single_left);
	add_dual(bindings, SDL_SCANCODE_KP_6, SdlInputPlayer::p2,
		SdlLogicalAction::right, SdlLogicalAction::single_right);
	add_dual(bindings, SDL_SCANCODE_KP_7, SdlInputPlayer::p2,
		SdlLogicalAction::up_left, SdlLogicalAction::single_up_left);
	add_dual(bindings, SDL_SCANCODE_KP_9, SdlInputPlayer::p2,
		SdlLogicalAction::up_right, SdlLogicalAction::single_up_right);
	add_dual(bindings, SDL_SCANCODE_KP_1, SdlInputPlayer::p2,
		SdlLogicalAction::down_left, SdlLogicalAction::single_down_left);
	add_dual(bindings, SDL_SCANCODE_KP_3, SdlInputPlayer::p2,
		SdlLogicalAction::down_right, SdlLogicalAction::single_down_right);
	add_dual(bindings, SDL_SCANCODE_LEFT, SdlInputPlayer::p2,
		SdlLogicalAction::shot, SdlLogicalAction::single_left);
	add_dual(bindings, SDL_SCANCODE_RIGHT, SdlInputPlayer::p2,
		SdlLogicalAction::bomb, SdlLogicalAction::single_right);

	bindings.gamepad_buttons[0] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_DPAD_UP,
		.action = SdlLogicalAction::up,
	};
	bindings.gamepad_buttons[1] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_DPAD_UP,
		.action = SdlLogicalAction::single_up,
	};
	bindings.gamepad_buttons[2] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN,
		.action = SdlLogicalAction::down,
	};
	bindings.gamepad_buttons[3] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN,
		.action = SdlLogicalAction::single_down,
	};
	bindings.gamepad_buttons[4] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_DPAD_LEFT,
		.action = SdlLogicalAction::left,
	};
	bindings.gamepad_buttons[5] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_DPAD_LEFT,
		.action = SdlLogicalAction::single_left,
	};
	bindings.gamepad_buttons[6] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
		.action = SdlLogicalAction::right,
	};
	bindings.gamepad_buttons[7] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
		.action = SdlLogicalAction::single_right,
	};
	bindings.gamepad_buttons[8] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_SOUTH,
		.action = SdlLogicalAction::shot,
	};
	bindings.gamepad_buttons[9] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_SOUTH,
		.action = SdlLogicalAction::single_shot,
	};
	bindings.gamepad_buttons[10] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_EAST,
		.action = SdlLogicalAction::bomb,
	};
	bindings.gamepad_buttons[11] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_EAST,
		.action = SdlLogicalAction::single_bomb,
	};
	bindings.gamepad_buttons[12] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_BACK,
		.action = SdlLogicalAction::cancel,
	};
	bindings.gamepad_buttons[13] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_START,
		.action = SdlLogicalAction::ok,
	};
	bindings.gamepad_button_count = 14U;
	bindings.gamepad_axes[0] = SdlGamepadAxisBinding{
		.axis = SDL_GAMEPAD_AXIS_LEFTX,
		.direction = SdlAxisDirection::negative,
		.action = SdlLogicalAction::left,
	};
	bindings.gamepad_axes[1] = SdlGamepadAxisBinding{
		.axis = SDL_GAMEPAD_AXIS_LEFTX,
		.direction = SdlAxisDirection::negative,
		.action = SdlLogicalAction::single_left,
	};
	bindings.gamepad_axes[2] = SdlGamepadAxisBinding{
		.axis = SDL_GAMEPAD_AXIS_LEFTX,
		.direction = SdlAxisDirection::positive,
		.action = SdlLogicalAction::right,
	};
	bindings.gamepad_axes[3] = SdlGamepadAxisBinding{
		.axis = SDL_GAMEPAD_AXIS_LEFTX,
		.direction = SdlAxisDirection::positive,
		.action = SdlLogicalAction::single_right,
	};
	bindings.gamepad_axes[4] = SdlGamepadAxisBinding{
		.axis = SDL_GAMEPAD_AXIS_LEFTY,
		.direction = SdlAxisDirection::negative,
		.action = SdlLogicalAction::up,
	};
	bindings.gamepad_axes[5] = SdlGamepadAxisBinding{
		.axis = SDL_GAMEPAD_AXIS_LEFTY,
		.direction = SdlAxisDirection::negative,
		.action = SdlLogicalAction::single_up,
	};
	bindings.gamepad_axes[6] = SdlGamepadAxisBinding{
		.axis = SDL_GAMEPAD_AXIS_LEFTY,
		.direction = SdlAxisDirection::positive,
		.action = SdlLogicalAction::down,
	};
	bindings.gamepad_axes[7] = SdlGamepadAxisBinding{
		.axis = SDL_GAMEPAD_AXIS_LEFTY,
		.direction = SdlAxisDirection::positive,
		.action = SdlLogicalAction::single_down,
	};
	bindings.gamepad_axis_count = 8U;
	for(std::size_t slot = 0U; slot < bindings.gamepad_players.size(); ++slot) {
		bindings.gamepad_players[slot] = (slot % 2U == 0U)
			? SdlInputPlayer::p1
			: SdlInputPlayer::p2;
	}
	return bindings;
}

SdlInputBindings new_classic_netplay_story_bindings() noexcept
{
	// A direct peer owns exactly one logical player. Reuse the ordinary New
	// Classic Story defaults on that peer's keyboard, then let the negotiated
	// netplay role place this local P1-shaped value into the P1 or P2 lane.
	const auto keys = new_classic_default_key_config();
	const auto joy = new_classic_default_joy_config();
	SdlInputBindings bindings{};
	bindings.charge_enabled_players = 0U;
	constexpr std::array<SdlLogicalAction, new_classic_story_action_count>
		story_actions{
			SdlLogicalAction::up,
			SdlLogicalAction::left,
			SdlLogicalAction::right,
			SdlLogicalAction::down,
		};
	constexpr auto story_offset = 2U * new_classic_player_action_count;
	for(std::size_t action = 0U; action < story_actions.size(); ++action) {
		add_key(bindings,
			static_cast<SDL_Scancode>(keys.bindings[story_offset + action]),
			SdlInputPlayer::p1, story_actions[action]);
	}
	constexpr std::array<std::size_t, 2U> shared_action_indexes{8U, 9U};
	constexpr std::array<SdlLogicalAction, 2U> shared_actions{
		SdlLogicalAction::shot,
		SdlLogicalAction::bomb,
	};
	for(std::size_t action = 0U; action < shared_actions.size(); ++action) {
		add_key(bindings,
			static_cast<SDL_Scancode>(keys.bindings[shared_action_indexes[action]]),
			SdlInputPlayer::p1, shared_actions[action]);
	}
	add_key(bindings, SDL_SCANCODE_ESCAPE, SdlInputPlayer::p1,
		SdlLogicalAction::cancel);
	add_key(bindings, SDL_SCANCODE_RETURN, SdlInputPlayer::p1,
		SdlLogicalAction::ok);
	add_key(bindings, SDL_SCANCODE_Q, SdlInputPlayer::p1,
		SdlLogicalAction::q);

	add_gamepad_button(bindings, NewClassicJoyButton::dpad_up,
		SdlLogicalAction::up);
	add_gamepad_button(bindings, NewClassicJoyButton::dpad_down,
		SdlLogicalAction::down);
	add_gamepad_button(bindings, NewClassicJoyButton::dpad_left,
		SdlLogicalAction::left);
	add_gamepad_button(bindings, NewClassicJoyButton::dpad_right,
		SdlLogicalAction::right);
	add_gamepad_button(bindings,
		joy.buttons[static_cast<std::size_t>(NewClassicJoyAction::shot)],
		SdlLogicalAction::shot);
	add_gamepad_button(bindings,
		joy.buttons[static_cast<std::size_t>(NewClassicJoyAction::bomb)],
		SdlLogicalAction::bomb);
	add_gamepad_button(bindings,
		joy.buttons[static_cast<std::size_t>(NewClassicJoyAction::cancel)],
		SdlLogicalAction::cancel);
	add_gamepad_button(bindings,
		joy.buttons[static_cast<std::size_t>(NewClassicJoyAction::ok)],
		SdlLogicalAction::ok);
	add_gamepad_axis(bindings, joy.horizontal_axis, SdlAxisDirection::negative,
		SdlLogicalAction::left);
	add_gamepad_axis(bindings, joy.horizontal_axis, SdlAxisDirection::positive,
		SdlLogicalAction::right);
	add_gamepad_axis(bindings, joy.vertical_axis, SdlAxisDirection::negative,
		SdlLogicalAction::up);
	add_gamepad_axis(bindings, joy.vertical_axis, SdlAxisDirection::positive,
		SdlLogicalAction::down);
	bindings.gamepad_players.fill(SdlInputPlayer::p1);
	const auto press = static_cast<std::int32_t>(
		(32'768 * joy.dead_zone_percent) / 100U
	);
	bindings.axis_press_threshold = static_cast<std::int16_t>(press);
	bindings.axis_release_threshold = static_cast<std::int16_t>(
		(press * 3) / 4
	);
	return bindings;
}

} // namespace th03::app::detail
