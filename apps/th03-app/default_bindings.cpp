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

void add_diagonal(
	SdlInputBindings &bindings,
	SDL_Scancode scancode,
	SdlInputPlayer player,
	SdlLogicalAction vertical,
	SdlLogicalAction horizontal
) noexcept
{
	add_key(bindings, scancode, player, vertical);
	add_key(bindings, scancode, player, horizontal);
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
	add_diagonal(bindings, SDL_SCANCODE_R, SdlInputPlayer::p1,
		SdlLogicalAction::up, SdlLogicalAction::left);
	add_diagonal(bindings, SDL_SCANCODE_Y, SdlInputPlayer::p1,
		SdlLogicalAction::up, SdlLogicalAction::right);
	add_diagonal(bindings, SDL_SCANCODE_V, SdlInputPlayer::p1,
		SdlLogicalAction::down, SdlLogicalAction::left);
	add_diagonal(bindings, SDL_SCANCODE_N, SdlInputPlayer::p1,
		SdlLogicalAction::down, SdlLogicalAction::right);
	add_key(bindings, SDL_SCANCODE_Z, SdlInputPlayer::p1,
		SdlLogicalAction::shot);
	add_key(bindings, SDL_SCANCODE_X, SdlInputPlayer::p1,
		SdlLogicalAction::bomb);

	add_key(bindings, SDL_SCANCODE_KP_8, SdlInputPlayer::p2,
		SdlLogicalAction::up);
	add_key(bindings, SDL_SCANCODE_KP_2, SdlInputPlayer::p2,
		SdlLogicalAction::down);
	add_key(bindings, SDL_SCANCODE_KP_4, SdlInputPlayer::p2,
		SdlLogicalAction::left);
	add_key(bindings, SDL_SCANCODE_KP_6, SdlInputPlayer::p2,
		SdlLogicalAction::right);
	add_diagonal(bindings, SDL_SCANCODE_KP_7, SdlInputPlayer::p2,
		SdlLogicalAction::up, SdlLogicalAction::left);
	add_diagonal(bindings, SDL_SCANCODE_KP_9, SdlInputPlayer::p2,
		SdlLogicalAction::up, SdlLogicalAction::right);
	add_diagonal(bindings, SDL_SCANCODE_KP_1, SdlInputPlayer::p2,
		SdlLogicalAction::down, SdlLogicalAction::left);
	add_diagonal(bindings, SDL_SCANCODE_KP_3, SdlInputPlayer::p2,
		SdlLogicalAction::down, SdlLogicalAction::right);
	add_key(bindings, SDL_SCANCODE_LEFT, SdlInputPlayer::p2,
		SdlLogicalAction::shot);
	add_key(bindings, SDL_SCANCODE_RIGHT, SdlInputPlayer::p2,
		SdlLogicalAction::bomb);

	bindings.gamepad_buttons[0] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_DPAD_UP,
		.action = SdlLogicalAction::up,
	};
	bindings.gamepad_buttons[1] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN,
		.action = SdlLogicalAction::down,
	};
	bindings.gamepad_buttons[2] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_DPAD_LEFT,
		.action = SdlLogicalAction::left,
	};
	bindings.gamepad_buttons[3] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
		.action = SdlLogicalAction::right,
	};
	bindings.gamepad_buttons[4] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_SOUTH,
		.action = SdlLogicalAction::shot,
	};
	bindings.gamepad_buttons[5] = SdlGamepadButtonBinding{
		.button = SDL_GAMEPAD_BUTTON_EAST,
		.action = SdlLogicalAction::bomb,
	};
	bindings.gamepad_button_count = 6U;
	bindings.gamepad_axes[0] = SdlGamepadAxisBinding{
		.axis = SDL_GAMEPAD_AXIS_LEFTX,
		.direction = SdlAxisDirection::negative,
		.action = SdlLogicalAction::left,
	};
	bindings.gamepad_axes[1] = SdlGamepadAxisBinding{
		.axis = SDL_GAMEPAD_AXIS_LEFTX,
		.direction = SdlAxisDirection::positive,
		.action = SdlLogicalAction::right,
	};
	bindings.gamepad_axes[2] = SdlGamepadAxisBinding{
		.axis = SDL_GAMEPAD_AXIS_LEFTY,
		.direction = SdlAxisDirection::negative,
		.action = SdlLogicalAction::up,
	};
	bindings.gamepad_axes[3] = SdlGamepadAxisBinding{
		.axis = SDL_GAMEPAD_AXIS_LEFTY,
		.direction = SdlAxisDirection::positive,
		.action = SdlLogicalAction::down,
	};
	bindings.gamepad_axis_count = 4U;
	for(std::size_t slot = 0U; slot < bindings.gamepad_players.size(); ++slot) {
		bindings.gamepad_players[slot] = (slot % 2U == 0U)
			? SdlInputPlayer::p1
			: SdlInputPlayer::p2;
	}
	return bindings;
}

} // namespace th03::app::detail
