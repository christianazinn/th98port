#include <array>
#include <cstddef>
#include <cstdint>
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

std::size_t key_action_matches(
	const th03::platform::SdlInputBindings &bindings,
	SDL_Scancode scancode,
	th03::platform::SdlInputPlayer player,
	th03::platform::SdlLogicalAction action
) noexcept
{
	std::size_t count = 0U;
	for(std::size_t index = 0U; index < bindings.keyboard_count; ++index) {
		if(bindings.keyboard[index].scancode == scancode &&
			bindings.keyboard[index].player == player &&
			bindings.keyboard[index].action == action) {
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
	expect(bindings.keyboard_count == 38U,
		"Classic preset has exact player and source-global keyboard contributions");
	expect(key_matches(bindings, SDL_SCANCODE_R,
		th03::platform::SdlInputPlayer::p1) == 1U &&
		key_matches(bindings, SDL_SCANCODE_KP_7,
			th03::platform::SdlInputPlayer::p2) == 2U,
		"P1 and P2 diagonals retain the donor's distinct input domains");
	expect(key_matches(bindings, SDL_SCANCODE_Z,
		th03::platform::SdlInputPlayer::p1) == 2U &&
		key_matches(bindings, SDL_SCANCODE_LEFT,
			th03::platform::SdlInputPlayer::p2) == 2U,
		"Shot keys also retain their distinct source-global contribution");
	expect(key_matches(bindings, SDL_SCANCODE_X,
		th03::platform::SdlInputPlayer::p1) == 2U &&
		key_matches(bindings, SDL_SCANCODE_RIGHT,
			th03::platform::SdlInputPlayer::p2) == 2U,
		"Bomb and Right Arrow retain player and source-global meanings");
	expect(key_matches(bindings, SDL_SCANCODE_ESCAPE,
		th03::platform::SdlInputPlayer::p1) == 1U &&
		key_matches(bindings, SDL_SCANCODE_RETURN,
			th03::platform::SdlInputPlayer::p1) == 1U,
		"Escape and Return retain source-global Cancel and OK bindings");
	expect(key_matches(bindings, SDL_SCANCODE_UP,
		th03::platform::SdlInputPlayer::p1) == 1U &&
		key_matches(bindings, SDL_SCANCODE_DOWN,
			th03::platform::SdlInputPlayer::p1) == 1U,
		"arrow Up and Down navigate source-global Classic UI menus");
	for(std::size_t index = 0U; index < bindings.keyboard_count; ++index) {
		expect(bindings.keyboard[index].action !=
			th03::platform::SdlLogicalAction::charge,
			"live direct VS has no Replay-Autofire Charge binding");
	}
	expect(bindings.gamepad_button_count == 14U &&
		bindings.gamepad_axis_count == 8U,
		"controller preset contributes every gameplay action to the source UI domain");
	constexpr std::array<SdlKeyboardBinding, 38U> expected_keys{{
		{SDL_SCANCODE_T, SdlInputPlayer::p1, SdlLogicalAction::up},
		{SDL_SCANCODE_B, SdlInputPlayer::p1, SdlLogicalAction::down},
		{SDL_SCANCODE_F, SdlInputPlayer::p1, SdlLogicalAction::left},
		{SDL_SCANCODE_H, SdlInputPlayer::p1, SdlLogicalAction::right},
		{SDL_SCANCODE_R, SdlInputPlayer::p1, SdlLogicalAction::up_left},
		{SDL_SCANCODE_Y, SdlInputPlayer::p1, SdlLogicalAction::up_right},
		{SDL_SCANCODE_V, SdlInputPlayer::p1, SdlLogicalAction::down_left},
		{SDL_SCANCODE_N, SdlInputPlayer::p1, SdlLogicalAction::down_right},
		{SDL_SCANCODE_Z, SdlInputPlayer::p1, SdlLogicalAction::shot},
		{SDL_SCANCODE_Z, SdlInputPlayer::p1, SdlLogicalAction::single_shot},
		{SDL_SCANCODE_X, SdlInputPlayer::p1, SdlLogicalAction::bomb},
		{SDL_SCANCODE_X, SdlInputPlayer::p1, SdlLogicalAction::single_bomb},
		{SDL_SCANCODE_ESCAPE, SdlInputPlayer::p1, SdlLogicalAction::cancel},
		{SDL_SCANCODE_RETURN, SdlInputPlayer::p1, SdlLogicalAction::ok},
		{SDL_SCANCODE_Q, SdlInputPlayer::p1, SdlLogicalAction::q},
		{SDL_SCANCODE_SPACE, SdlInputPlayer::p1, SdlLogicalAction::single_shot},
		{SDL_SCANCODE_UP, SdlInputPlayer::p1, SdlLogicalAction::single_up},
		{SDL_SCANCODE_DOWN, SdlInputPlayer::p1, SdlLogicalAction::single_down},
		{SDL_SCANCODE_KP_8, SdlInputPlayer::p2, SdlLogicalAction::up},
		{SDL_SCANCODE_KP_8, SdlInputPlayer::p2, SdlLogicalAction::single_up},
		{SDL_SCANCODE_KP_2, SdlInputPlayer::p2, SdlLogicalAction::down},
		{SDL_SCANCODE_KP_2, SdlInputPlayer::p2, SdlLogicalAction::single_down},
		{SDL_SCANCODE_KP_4, SdlInputPlayer::p2, SdlLogicalAction::left},
		{SDL_SCANCODE_KP_4, SdlInputPlayer::p2, SdlLogicalAction::single_left},
		{SDL_SCANCODE_KP_6, SdlInputPlayer::p2, SdlLogicalAction::right},
		{SDL_SCANCODE_KP_6, SdlInputPlayer::p2, SdlLogicalAction::single_right},
		{SDL_SCANCODE_KP_7, SdlInputPlayer::p2, SdlLogicalAction::up_left},
		{SDL_SCANCODE_KP_7, SdlInputPlayer::p2, SdlLogicalAction::single_up_left},
		{SDL_SCANCODE_KP_9, SdlInputPlayer::p2, SdlLogicalAction::up_right},
		{SDL_SCANCODE_KP_9, SdlInputPlayer::p2, SdlLogicalAction::single_up_right},
		{SDL_SCANCODE_KP_1, SdlInputPlayer::p2, SdlLogicalAction::down_left},
		{SDL_SCANCODE_KP_1, SdlInputPlayer::p2, SdlLogicalAction::single_down_left},
		{SDL_SCANCODE_KP_3, SdlInputPlayer::p2, SdlLogicalAction::down_right},
		{SDL_SCANCODE_KP_3, SdlInputPlayer::p2, SdlLogicalAction::single_down_right},
		{SDL_SCANCODE_LEFT, SdlInputPlayer::p2, SdlLogicalAction::shot},
		{SDL_SCANCODE_LEFT, SdlInputPlayer::p2, SdlLogicalAction::single_left},
		{SDL_SCANCODE_RIGHT, SdlInputPlayer::p2, SdlLogicalAction::bomb},
		{SDL_SCANCODE_RIGHT, SdlInputPlayer::p2, SdlLogicalAction::single_right},
	}};
	for(std::size_t index = 0U; index < expected_keys.size(); ++index) {
		expect(
			bindings.keyboard[index].scancode == expected_keys[index].scancode &&
			bindings.keyboard[index].player == expected_keys[index].player &&
			bindings.keyboard[index].action == expected_keys[index].action,
			"every keyboard default retains its exact source key/action mapping"
		);
	}
	constexpr std::array<SdlGamepadButtonBinding, 14U> expected_buttons{{
		{SDL_GAMEPAD_BUTTON_DPAD_UP, SdlLogicalAction::up},
		{SDL_GAMEPAD_BUTTON_DPAD_UP, SdlLogicalAction::single_up},
		{SDL_GAMEPAD_BUTTON_DPAD_DOWN, SdlLogicalAction::down},
		{SDL_GAMEPAD_BUTTON_DPAD_DOWN, SdlLogicalAction::single_down},
		{SDL_GAMEPAD_BUTTON_DPAD_LEFT, SdlLogicalAction::left},
		{SDL_GAMEPAD_BUTTON_DPAD_LEFT, SdlLogicalAction::single_left},
		{SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SdlLogicalAction::right},
		{SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SdlLogicalAction::single_right},
		{SDL_GAMEPAD_BUTTON_SOUTH, SdlLogicalAction::shot},
		{SDL_GAMEPAD_BUTTON_SOUTH, SdlLogicalAction::single_shot},
		{SDL_GAMEPAD_BUTTON_EAST, SdlLogicalAction::bomb},
		{SDL_GAMEPAD_BUTTON_EAST, SdlLogicalAction::single_bomb},
		{SDL_GAMEPAD_BUTTON_BACK, SdlLogicalAction::cancel},
		{SDL_GAMEPAD_BUTTON_START, SdlLogicalAction::ok},
	}};
	for(std::size_t index = 0U; index < expected_buttons.size(); ++index) {
		expect(
			bindings.gamepad_buttons[index].button == expected_buttons[index].button &&
			bindings.gamepad_buttons[index].action == expected_buttons[index].action,
			"every gamepad button retains its exact logical action"
		);
	}
	constexpr std::array<SdlGamepadAxisBinding, 8U> expected_axes{{
		{SDL_GAMEPAD_AXIS_LEFTX, SdlAxisDirection::negative, SdlLogicalAction::left},
		{SDL_GAMEPAD_AXIS_LEFTX, SdlAxisDirection::negative, SdlLogicalAction::single_left},
		{SDL_GAMEPAD_AXIS_LEFTX, SdlAxisDirection::positive, SdlLogicalAction::right},
		{SDL_GAMEPAD_AXIS_LEFTX, SdlAxisDirection::positive, SdlLogicalAction::single_right},
		{SDL_GAMEPAD_AXIS_LEFTY, SdlAxisDirection::negative, SdlLogicalAction::up},
		{SDL_GAMEPAD_AXIS_LEFTY, SdlAxisDirection::negative, SdlLogicalAction::single_up},
		{SDL_GAMEPAD_AXIS_LEFTY, SdlAxisDirection::positive, SdlLogicalAction::down},
		{SDL_GAMEPAD_AXIS_LEFTY, SdlAxisDirection::positive, SdlLogicalAction::single_down},
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

void test_netplay_story_defaults()
{
	using th03::platform::SdlInputPlayer;
	using th03::platform::SdlLogicalAction;
	const auto bindings =
		th03::app::detail::new_classic_netplay_story_bindings(false);
	const auto autofire_bindings =
		th03::app::detail::new_classic_netplay_story_bindings(true);
	expect(bindings.charge_enabled_players == 0U,
		"netplay disables Charge when the local Story profile disables Autofire");
	expect(bindings.keyboard_count == 9U,
		"netplay exposes one Story keyboard rather than both VS columns");
	expect(
		key_action_matches(bindings, SDL_SCANCODE_UP, SdlInputPlayer::p1,
			SdlLogicalAction::up) == 1U &&
		key_action_matches(bindings, SDL_SCANCODE_DOWN, SdlInputPlayer::p1,
			SdlLogicalAction::down) == 1U &&
		key_action_matches(bindings, SDL_SCANCODE_LEFT, SdlInputPlayer::p1,
			SdlLogicalAction::left) == 1U &&
		key_action_matches(bindings, SDL_SCANCODE_RIGHT, SdlInputPlayer::p1,
			SdlLogicalAction::right) == 1U,
		"netplay maps the ordinary Story arrow directions into the local lane"
	);
	expect(
		key_action_matches(bindings, SDL_SCANCODE_Z, SdlInputPlayer::p1,
			SdlLogicalAction::shot) == 1U &&
		key_action_matches(bindings, SDL_SCANCODE_X, SdlInputPlayer::p1,
			SdlLogicalAction::bomb) == 1U &&
		key_matches(bindings, SDL_SCANCODE_LSHIFT, SdlInputPlayer::p1) == 0U,
		"netplay shares Story Shot and Bomb without an unnegotiated Autofire key"
	);
	expect(autofire_bindings.charge_enabled_players == 0x01U &&
		autofire_bindings.keyboard_count == 10U &&
		key_action_matches(autofire_bindings, SDL_SCANCODE_LSHIFT,
			SdlInputPlayer::p1, SdlLogicalAction::charge) == 1U &&
		autofire_bindings.gamepad_button_count == 9U &&
		autofire_bindings.gamepad_buttons[6U].action ==
			SdlLogicalAction::charge,
		"netplay enables ordinary local Charge/Shot only with agreed Autofire");
	expect(
		key_action_matches(bindings, SDL_SCANCODE_ESCAPE, SdlInputPlayer::p1,
			SdlLogicalAction::cancel) == 1U,
		"netplay keeps Escape in the source-global synchronized Pause domain"
	);
	expect(
		key_matches(bindings, SDL_SCANCODE_T, SdlInputPlayer::p1) == 0U &&
		key_matches(bindings, SDL_SCANCODE_KP_8, SdlInputPlayer::p2) == 0U,
		"netplay does not expose the local VS P1 or P2 keyboard columns"
	);
	expect(bindings.gamepad_button_count == 8U &&
		bindings.gamepad_axis_count == 4U,
		"netplay exposes one local Story controller action set");
	for(const auto player : bindings.gamepad_players) {
		expect(player == SdlInputPlayer::p1,
			"every local netplay controller slot feeds the negotiated local lane");
	}
}

void test_new_classic_joy_config()
{
	using namespace th03::app;
	using namespace th03::platform;
	const auto keys = detail::new_classic_default_key_config();
	const auto joy = detail::new_classic_default_joy_config();
	const auto defaults = detail::new_classic_bindings(keys, joy);
	expect(defaults.charge_enabled_players == 0U,
		"Autofire Off suppresses Charge before controller input latching");
	expect(defaults.gamepad_button_count == 14U &&
		defaults.gamepad_axis_count == 8U,
		"New Classic omits Charge while Autofire is disabled");
	expect(key_matches(defaults, SDL_SCANCODE_LSHIFT, SdlInputPlayer::p1) == 0U,
		"Shift is completely unbound while P1 Autofire is disabled");
	auto autofire_keys = keys;
	autofire_keys.autofire = 3U;
	const auto autofire_defaults = detail::new_classic_bindings(
		autofire_keys, joy
	);
	expect(autofire_defaults.charge_enabled_players == 3U,
		"per-player Autofire mask crosses the SDL controller boundary");
	expect(autofire_defaults.gamepad_button_count == 15U &&
		key_action_matches(
			autofire_defaults, SDL_SCANCODE_LSHIFT, SdlInputPlayer::p1,
			SdlLogicalAction::charge
		) == 1U,
		"enabled Autofire restores the configured Charge inputs");
	expect(autofire_defaults.gamepad_buttons[8U].button == SDL_GAMEPAD_BUTTON_SOUTH &&
		autofire_defaults.gamepad_buttons[8U].action == SdlLogicalAction::shot &&
		autofire_defaults.gamepad_buttons[10U].button == SDL_GAMEPAD_BUTTON_EAST &&
		autofire_defaults.gamepad_buttons[10U].action == SdlLogicalAction::bomb &&
		autofire_defaults.gamepad_buttons[12U].button == SDL_GAMEPAD_BUTTON_WEST &&
		autofire_defaults.gamepad_buttons[12U].action == SdlLogicalAction::charge,
		"default Shot, Bomb, and Charge controller buttons are exact");
	expect(defaults.axis_press_threshold == 16384 &&
		defaults.axis_release_threshold == 12288,
		"default controller dead zone preserves the existing hysteresis");
	expect(defaults.identity_assignment_enabled &&
		defaults.device_assignment == NewClassicDeviceAssignment{},
		"New Classic defaults use the stable automatic assignment policy");

	auto custom = joy;
	custom.buttons = {
		NewClassicJoyButton::north,
		NewClassicJoyButton::left_shoulder,
		NewClassicJoyButton::right_shoulder,
		NewClassicJoyButton::misc,
		NewClassicJoyButton::right_stick,
	};
	custom.horizontal_axis = NewClassicJoyAxis::right_x;
	custom.vertical_axis = NewClassicJoyAxis::right_y;
	custom.horizontal_reversed = true;
	custom.vertical_reversed = true;
	custom.dead_zone_percent = 25U;
	const auto bindings = detail::new_classic_bindings(autofire_keys, custom);
	expect(bindings.gamepad_buttons[8U].button == SDL_GAMEPAD_BUTTON_NORTH &&
		bindings.gamepad_buttons[10U].button ==
			SDL_GAMEPAD_BUTTON_LEFT_SHOULDER &&
		bindings.gamepad_buttons[12U].button ==
			SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER &&
		bindings.gamepad_buttons[13U].button == SDL_GAMEPAD_BUTTON_MISC1 &&
		bindings.gamepad_buttons[14U].button == SDL_GAMEPAD_BUTTON_RIGHT_STICK,
		"custom controller action buttons convert at the SDL boundary");
	expect(bindings.gamepad_axes[0U].axis == SDL_GAMEPAD_AXIS_RIGHTX &&
		bindings.gamepad_axes[0U].action == SdlLogicalAction::right &&
		bindings.gamepad_axes[2U].action == SdlLogicalAction::left &&
		bindings.gamepad_axes[4U].axis == SDL_GAMEPAD_AXIS_RIGHTY &&
		bindings.gamepad_axes[4U].action == SdlLogicalAction::down &&
		bindings.gamepad_axes[6U].action == SdlLogicalAction::up,
		"custom axes and reversals convert to exact logical directions");
	expect(bindings.axis_press_threshold == 8192 &&
		bindings.axis_release_threshold == 6144,
		"custom dead zone derives deterministic three-quarter hysteresis");

	custom.buttons[static_cast<std::size_t>(NewClassicJoyAction::charge)] =
		NewClassicJoyButton::unbound;
	const auto unbound = detail::new_classic_bindings(autofire_keys, custom);
	expect(unbound.gamepad_button_count == 14U,
		"an explicitly unbound controller action emits no SDL binding");
	NewClassicDeviceAssignment assignment{};
	assignment.mode[0] = NewClassicControllerAssignmentMode::disabled;
	const auto assigned = detail::new_classic_bindings(
		autofire_keys, custom, assignment
	);
	expect(assigned.identity_assignment_enabled &&
		assigned.device_assignment == assignment,
		"device assignment crosses only the SDL binding boundary");
}

SDL_Gamepad *fake_open_gamepad(void *context, SDL_JoystickID) noexcept
{
	return static_cast<SDL_Gamepad *>(context);
}

void fake_close_gamepad(void *, SDL_Gamepad *) noexcept
{}

SDL_Event key_event(
	std::uint64_t timestamp,
	SDL_WindowID window,
	SDL_Scancode scancode,
	bool down
) noexcept
{
	SDL_Event event{};
	event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
	event.key.timestamp = timestamp;
	event.key.windowID = window;
	event.key.scancode = scancode;
	event.key.down = down;
	return event;
}

void test_arrow_ui_channel_through_mapper()
{
	constexpr SDL_WindowID window = 17U;
	std::uint8_t backend_token = 0U;
	auto mapper = th03::platform::SdlInputMapper::create(
		th03::app::detail::classic_direct_vs_bindings(), window,
		{
			.context = &backend_token,
			.open = &fake_open_gamepad,
			.close = &fake_close_gamepad,
		}
	);
	auto scheduler = th03::app::TickInputScheduler::create(
		{100'000'000U, 1U}, 10U
	);
	expect(mapper.has_value() && scheduler.has_value(),
		"Classic defaults create a real mapper and logical scheduler");
	if(!mapper || !scheduler) { return; }

	const auto up_down = mapper->handle_event(
		key_event(10U, window, SDL_SCANCODE_UP, true), *scheduler
	);
	const auto first = scheduler->pop_due(10U);
	expect(
		up_down && first && *first && (**first).input.system == 0x0001U &&
			(**first).input.p1 == 0U && (**first).input.p2 == 0U,
		"Arrow Up reaches only the source-global Classic UI channel"
	);

	const auto up_release = mapper->handle_event(
		key_event(11U, window, SDL_SCANCODE_UP, false), *scheduler
	);
	const auto down_press = mapper->handle_event(
		key_event(12U, window, SDL_SCANCODE_DOWN, true), *scheduler
	);
	const auto second = scheduler->pop_due(20U);
	expect(
		up_release && down_press && second && *second &&
			(**second).input.system == 0x0002U &&
			(**second).input.p1 == 0U && (**second).input.p2 == 0U,
		"Arrow Down survives the complete mapper/scheduler path without gameplay input"
	);

	const auto down_release = mapper->handle_event(
		key_event(21U, window, SDL_SCANCODE_DOWN, false), *scheduler
	);
	const auto released = scheduler->pop_due(30U);
	expect(
		down_release && released && *released &&
			(**released).input.system == 0U,
		"Arrow release clears the Classic UI contribution"
	);
}

} // namespace

int main()
{
	test_classic_defaults();
	test_netplay_story_defaults();
	test_new_classic_joy_config();
	test_arrow_ui_channel_through_mapper();
	if(failures != 0U) {
		std::cerr << failures << " of " << checks
			<< " SDL default-binding checks failed\n";
		return 1;
	}
	std::cout << "all " << checks
		<< " SDL default-binding checks passed\n";
	return 0;
}
