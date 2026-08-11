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

} // namespace

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

} // namespace th03::app::detail
