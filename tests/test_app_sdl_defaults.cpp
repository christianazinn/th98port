#include <array>
#include <cstddef>
#include <iostream>

#include "default_bindings.hpp"

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	checks++;
	if(!condition) {
		failures++;
		std::cerr << "FAIL: " << message << '\n';
	}
}

std::size_t key_matches(
	const th03::platform::SdlInputBindings &bindings,
	SDL_Scancode scancode,
	th03::platform::SdlInputPlayer player
) noexcept
{
	std::size_t count = 0U;
	for(std::size_t index = 0U; index < bindings.keyboard_count; ++index) {
		if(bindings.keyboard[index].scancode == scancode &&
			bindings.keyboard[index].player == player) {
			count++;
		}
	}
	return count;
}

void test_classic_defaults()
{
	using th03::platform::SdlAxisDirection;
	using th03::platform::SdlGamepadAxisBinding;
	using th03::platform::SdlGamepadButtonBinding;
	using th03::platform::SdlInputPlayer;
	using th03::platform::SdlKeyboardBinding;
	using th03::platform::SdlLogicalAction;
	const auto bindings = th03::app::detail::classic_direct_vs_bindings();
	expect(bindings.keyboard_count == 28U,
		"key-vs-key preset has four cardinals, four diagonals, and two actions per player");
	expect(key_matches(bindings, SDL_SCANCODE_R,
		th03::platform::SdlInputPlayer::p1) == 2U &&
		key_matches(bindings, SDL_SCANCODE_KP_7,
			th03::platform::SdlInputPlayer::p2) == 2U,
		"diagonal keys are duplicate-safe pairs of source cardinal bits");
	expect(key_matches(bindings, SDL_SCANCODE_Z,
		th03::platform::SdlInputPlayer::p1) == 1U &&
		key_matches(bindings, SDL_SCANCODE_LEFT,
			th03::platform::SdlInputPlayer::p2) == 1U,
		"each player has one default Shot binding");
	expect(key_matches(bindings, SDL_SCANCODE_X,
		th03::platform::SdlInputPlayer::p1) == 1U &&
		key_matches(bindings, SDL_SCANCODE_RIGHT,
			th03::platform::SdlInputPlayer::p2) == 1U,
		"each player has one default Bomb binding");
	expect(key_matches(bindings, SDL_SCANCODE_ESCAPE,
		th03::platform::SdlInputPlayer::p1) == 0U &&
		key_matches(bindings, SDL_SCANCODE_ESCAPE,
			th03::platform::SdlInputPlayer::p2) == 0U,
		"Escape remains an app command rather than gameplay Cancel");
	for(std::size_t index = 0U; index < bindings.keyboard_count; ++index) {
		expect(bindings.keyboard[index].action !=
			th03::platform::SdlLogicalAction::charge,
			"live direct VS has no Replay-Autofire Charge binding");
	}
	expect(bindings.gamepad_button_count == 6U &&
		bindings.gamepad_axis_count == 4U,
		"controller preset has D-pad, Shot/Bomb, and left-stick axes");
	constexpr std::array<SdlKeyboardBinding, 28U> expected_keys{{
		{SDL_SCANCODE_T, SdlInputPlayer::p1, SdlLogicalAction::up},
		{SDL_SCANCODE_B, SdlInputPlayer::p1, SdlLogicalAction::down},
		{SDL_SCANCODE_F, SdlInputPlayer::p1, SdlLogicalAction::left},
		{SDL_SCANCODE_H, SdlInputPlayer::p1, SdlLogicalAction::right},
		{SDL_SCANCODE_R, SdlInputPlayer::p1, SdlLogicalAction::up},
		{SDL_SCANCODE_R, SdlInputPlayer::p1, SdlLogicalAction::left},
		{SDL_SCANCODE_Y, SdlInputPlayer::p1, SdlLogicalAction::up},
		{SDL_SCANCODE_Y, SdlInputPlayer::p1, SdlLogicalAction::right},
		{SDL_SCANCODE_V, SdlInputPlayer::p1, SdlLogicalAction::down},
		{SDL_SCANCODE_V, SdlInputPlayer::p1, SdlLogicalAction::left},
		{SDL_SCANCODE_N, SdlInputPlayer::p1, SdlLogicalAction::down},
		{SDL_SCANCODE_N, SdlInputPlayer::p1, SdlLogicalAction::right},
		{SDL_SCANCODE_Z, SdlInputPlayer::p1, SdlLogicalAction::shot},
		{SDL_SCANCODE_X, SdlInputPlayer::p1, SdlLogicalAction::bomb},
		{SDL_SCANCODE_KP_8, SdlInputPlayer::p2, SdlLogicalAction::up},
		{SDL_SCANCODE_KP_2, SdlInputPlayer::p2, SdlLogicalAction::down},
		{SDL_SCANCODE_KP_4, SdlInputPlayer::p2, SdlLogicalAction::left},
		{SDL_SCANCODE_KP_6, SdlInputPlayer::p2, SdlLogicalAction::right},
		{SDL_SCANCODE_KP_7, SdlInputPlayer::p2, SdlLogicalAction::up},
		{SDL_SCANCODE_KP_7, SdlInputPlayer::p2, SdlLogicalAction::left},
		{SDL_SCANCODE_KP_9, SdlInputPlayer::p2, SdlLogicalAction::up},
		{SDL_SCANCODE_KP_9, SdlInputPlayer::p2, SdlLogicalAction::right},
		{SDL_SCANCODE_KP_1, SdlInputPlayer::p2, SdlLogicalAction::down},
		{SDL_SCANCODE_KP_1, SdlInputPlayer::p2, SdlLogicalAction::left},
		{SDL_SCANCODE_KP_3, SdlInputPlayer::p2, SdlLogicalAction::down},
		{SDL_SCANCODE_KP_3, SdlInputPlayer::p2, SdlLogicalAction::right},
		{SDL_SCANCODE_LEFT, SdlInputPlayer::p2, SdlLogicalAction::shot},
		{SDL_SCANCODE_RIGHT, SdlInputPlayer::p2, SdlLogicalAction::bomb},
	}};
	for(std::size_t index = 0U; index < expected_keys.size(); ++index) {
		expect(
			bindings.keyboard[index].scancode == expected_keys[index].scancode &&
			bindings.keyboard[index].player == expected_keys[index].player &&
			bindings.keyboard[index].action == expected_keys[index].action,
			"every keyboard default retains its exact source key/action mapping"
		);
	}
	constexpr std::array<SdlGamepadButtonBinding, 6U> expected_buttons{{
		{SDL_GAMEPAD_BUTTON_DPAD_UP, SdlLogicalAction::up},
		{SDL_GAMEPAD_BUTTON_DPAD_DOWN, SdlLogicalAction::down},
		{SDL_GAMEPAD_BUTTON_DPAD_LEFT, SdlLogicalAction::left},
		{SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SdlLogicalAction::right},
		{SDL_GAMEPAD_BUTTON_SOUTH, SdlLogicalAction::shot},
		{SDL_GAMEPAD_BUTTON_EAST, SdlLogicalAction::bomb},
	}};
	for(std::size_t index = 0U; index < expected_buttons.size(); ++index) {
		expect(
			bindings.gamepad_buttons[index].button == expected_buttons[index].button &&
			bindings.gamepad_buttons[index].action == expected_buttons[index].action,
			"every gamepad button retains its exact logical action"
		);
	}
	constexpr std::array<SdlGamepadAxisBinding, 4U> expected_axes{{
		{SDL_GAMEPAD_AXIS_LEFTX, SdlAxisDirection::negative, SdlLogicalAction::left},
		{SDL_GAMEPAD_AXIS_LEFTX, SdlAxisDirection::positive, SdlLogicalAction::right},
		{SDL_GAMEPAD_AXIS_LEFTY, SdlAxisDirection::negative, SdlLogicalAction::up},
		{SDL_GAMEPAD_AXIS_LEFTY, SdlAxisDirection::positive, SdlLogicalAction::down},
	}};
	for(std::size_t index = 0U; index < expected_axes.size(); ++index) {
		expect(
			bindings.gamepad_axes[index].axis == expected_axes[index].axis &&
			bindings.gamepad_axes[index].direction == expected_axes[index].direction &&
			bindings.gamepad_axes[index].action == expected_axes[index].action,
			"every gamepad axis direction retains its exact logical action"
		);
	}
	for(std::size_t slot = 0U; slot < bindings.gamepad_players.size(); ++slot) {
		expect(bindings.gamepad_players[slot] == ((slot % 2U == 0U)
			? th03::platform::SdlInputPlayer::p1
			: th03::platform::SdlInputPlayer::p2),
			"first-free gamepad slots alternate P1/P2 deterministically");
	}
}

} // namespace

int main()
{
	test_classic_defaults();
	if(failures != 0U) {
		std::cerr << failures << " of " << checks
			<< " SDL default-binding checks failed\n";
		return 1;
	}
	std::cout << "all " << checks
		<< " SDL default-binding checks passed\n";
	return 0;
}
