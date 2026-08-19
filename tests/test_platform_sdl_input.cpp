#include "th03/platform/sdl_input.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>

namespace {

using th03::app::LogicalTickInput;
using th03::app::PhysicalInputEvent;
using th03::app::PhysicalInputEventKind;
using th03::app::TickInputError;
using th03::app::TickInputScheduler;
using th03::platform::SdlAxisDirection;
using th03::platform::SdlGamepadBackend;
using th03::platform::SdlInputBindings;
using th03::platform::SdlInputEventResult;
using th03::platform::SdlInputMapper;
using th03::platform::SdlInputMapperErrorCode;
using th03::platform::SdlInputPlayer;
using th03::platform::SdlLogicalAction;

constexpr SDL_WindowID main_window = 17U;
constexpr std::uint16_t input_up = 0x0001U;
constexpr std::uint16_t input_down = 0x0002U;
constexpr std::uint16_t input_left = 0x0004U;
constexpr std::uint16_t input_bomb = 0x0010U;
constexpr std::uint16_t input_shot = 0x0020U;
constexpr std::uint16_t input_up_left = 0x0100U;
constexpr std::uint16_t input_down_right = 0x0800U;
constexpr std::uint16_t input_cancel = 0x1000U;
constexpr std::uint16_t input_ok = 0x2000U;
constexpr std::uint16_t input_q = 0x4000U;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

struct FakeGamepads {
	static constexpr std::size_t log_capacity = 32U;
	std::array<SDL_JoystickID, log_capacity> opened{};
	std::array<SDL_Gamepad *, log_capacity> closed{};
	std::size_t open_count = 0U;
	std::size_t close_count = 0U;
	SDL_JoystickID fail_id = 0U;

	[[nodiscard]] static SDL_Gamepad *open(
		void *context,
		SDL_JoystickID id
	) noexcept
	{
		auto &self = *static_cast<FakeGamepads *>(context);
		if(id == self.fail_id) {
			return nullptr;
		}
		if(self.open_count < self.opened.size()) {
			self.opened[self.open_count] = id;
		}
		++self.open_count;
		return reinterpret_cast<SDL_Gamepad *>(
			static_cast<std::uintptr_t>(id) + 1U
		);
	}

	static void close(void *context, SDL_Gamepad *gamepad) noexcept
	{
		auto &self = *static_cast<FakeGamepads *>(context);
		if(self.close_count < self.closed.size()) {
			self.closed[self.close_count] = gamepad;
		}
		++self.close_count;
	}

	[[nodiscard]] static bool identify(
		void *,
		SDL_Gamepad *,
		SDL_JoystickID id,
		th03::app::NewClassicControllerIdentity &identity,
		std::array<char, th03::app::new_classic_controller_name_bytes> &name
	) noexcept
	{
		// A reconnect receives a new transient SDL instance ID while retaining
		// the same device identity.
		const auto stable_id = id == 111U ? 11U : id;
		identity.bytes[0] = static_cast<std::uint8_t>(stable_id & 0xFFU);
		identity.bytes[1] = static_cast<std::uint8_t>((id >> 8U) & 0xFFU);
		name[0] = 'P';
		name[1] = 'a';
		name[2] = 'd';
		return true;
	}

	[[nodiscard]] SdlGamepadBackend backend() noexcept
	{
		return {
			.context = this,
			.open = open,
			.close = close,
			.identify = identify,
		};
	}
};

[[nodiscard]] SdlInputBindings bindings()
{
	SdlInputBindings result{};
	result.keyboard[0] = {
		.scancode = SDL_SCANCODE_A,
		.player = SdlInputPlayer::p1,
		.action = SdlLogicalAction::shot,
	};
	result.keyboard[1] = {
		.scancode = SDL_SCANCODE_B,
		.player = SdlInputPlayer::p1,
		.action = SdlLogicalAction::shot,
	};
	result.keyboard[2] = {
		.scancode = SDL_SCANCODE_W,
		.player = SdlInputPlayer::p1,
		.action = SdlLogicalAction::up,
	};
	result.keyboard[3] = {
		.scancode = SDL_SCANCODE_C,
		.player = SdlInputPlayer::p1,
		.action = SdlLogicalAction::charge,
	};
	result.keyboard[4] = {
		.scancode = SDL_SCANCODE_I,
		.player = SdlInputPlayer::p2,
		.action = SdlLogicalAction::up,
	};
	result.keyboard[5] = {
		.scancode = SDL_SCANCODE_O,
		.player = SdlInputPlayer::p2,
		.action = SdlLogicalAction::bomb,
	};
	result.keyboard[6] = {
		.scancode = SDL_SCANCODE_D,
		.player = SdlInputPlayer::p1,
		.action = SdlLogicalAction::cancel,
	};
	result.keyboard[7] = {
		.scancode = SDL_SCANCODE_E,
		.player = SdlInputPlayer::p1,
		.action = SdlLogicalAction::ok,
	};
	result.keyboard_count = 8U;

	result.gamepad_buttons[0] = {
		.button = SDL_GAMEPAD_BUTTON_SOUTH,
		.action = SdlLogicalAction::shot,
	};
	result.gamepad_buttons[1] = {
		.button = SDL_GAMEPAD_BUTTON_WEST,
		.action = SdlLogicalAction::shot,
	};
	result.gamepad_buttons[2] = {
		.button = SDL_GAMEPAD_BUTTON_EAST,
		.action = SdlLogicalAction::bomb,
	};
	result.gamepad_buttons[3] = {
		.button = SDL_GAMEPAD_BUTTON_DPAD_UP,
		.action = SdlLogicalAction::up,
	};
	result.gamepad_buttons[4] = {
		.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN,
		.action = SdlLogicalAction::down,
	};
	result.gamepad_buttons[5] = {
		.button = SDL_GAMEPAD_BUTTON_DPAD_LEFT,
		.action = SdlLogicalAction::left,
	};
	result.gamepad_buttons[6] = {
		.button = SDL_GAMEPAD_BUTTON_NORTH,
		.action = SdlLogicalAction::charge,
	};
	result.gamepad_button_count = 7U;

	result.gamepad_axes[0] = {
		.axis = SDL_GAMEPAD_AXIS_LEFTX,
		.direction = SdlAxisDirection::negative,
		.action = SdlLogicalAction::left,
	};
	result.gamepad_axes[1] = {
		.axis = SDL_GAMEPAD_AXIS_LEFTX,
		.direction = SdlAxisDirection::positive,
		.action = SdlLogicalAction::right,
	};
	result.gamepad_axes[2] = {
		.axis = SDL_GAMEPAD_AXIS_LEFTY,
		.direction = SdlAxisDirection::negative,
		.action = SdlLogicalAction::up,
	};
	result.gamepad_axes[3] = {
		.axis = SDL_GAMEPAD_AXIS_LEFTY,
		.direction = SdlAxisDirection::positive,
		.action = SdlLogicalAction::down,
	};
	result.gamepad_axis_count = 4U;

	for(std::size_t index = 0U; index < result.gamepad_players.size(); ++index) {
		result.gamepad_players[index] = ((index % 2U) == 0U) ?
			SdlInputPlayer::p1 : SdlInputPlayer::p2;
	}
	return result;
}

[[nodiscard]] TickInputScheduler scheduler(
	std::uint64_t first_deadline = 10U
)
{
	auto created = TickInputScheduler::create(
		{100'000'000U, 1U},
		first_deadline
	);
	expect(static_cast<bool>(created), "tick/input scheduler is created");
	if(!created) {
		std::terminate();
	}
	return std::move(*created);
}

[[nodiscard]] SDL_Event key_event(
	std::uint64_t timestamp,
	SDL_Scancode scancode,
	bool down,
	bool repeat = false
)
{
	SDL_Event event{};
	event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
	event.key.timestamp = timestamp;
	event.key.windowID = main_window;
	event.key.scancode = scancode;
	event.key.down = down;
	event.key.repeat = repeat;
	return event;
}

[[nodiscard]] SDL_Event text_event(
	std::uint64_t timestamp,
	const char *text,
	SDL_WindowID window = main_window
)
{
	SDL_Event event{};
	event.type = SDL_EVENT_TEXT_INPUT;
	event.text.timestamp = timestamp;
	event.text.windowID = window;
	event.text.text = text;
	return event;
}

[[nodiscard]] SDL_Event keyboard_removed(std::uint64_t timestamp)
{
	SDL_Event event{};
	event.type = SDL_EVENT_KEYBOARD_REMOVED;
	event.kdevice.timestamp = timestamp;
	event.kdevice.which = 1U;
	return event;
}

[[nodiscard]] SDL_Event gamepad_device_event(
	std::uint32_t type,
	std::uint64_t timestamp,
	SDL_JoystickID id
)
{
	SDL_Event event{};
	event.type = type;
	event.gdevice.timestamp = timestamp;
	event.gdevice.which = id;
	return event;
}

[[nodiscard]] SDL_Event gamepad_button_event(
	std::uint64_t timestamp,
	SDL_JoystickID id,
	SDL_GamepadButton button,
	bool down
)
{
	SDL_Event event{};
	event.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN :
		SDL_EVENT_GAMEPAD_BUTTON_UP;
	event.gbutton.timestamp = timestamp;
	event.gbutton.which = id;
	event.gbutton.button = static_cast<Uint8>(button);
	event.gbutton.down = down;
	return event;
}

[[nodiscard]] SDL_Event gamepad_axis_event(
	std::uint64_t timestamp,
	SDL_JoystickID id,
	SDL_GamepadAxis axis,
	std::int16_t value
)
{
	SDL_Event event{};
	event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
	event.gaxis.timestamp = timestamp;
	event.gaxis.which = id;
	event.gaxis.axis = static_cast<Uint8>(axis);
	event.gaxis.value = value;
	return event;
}

[[nodiscard]] SDL_Event window_event(
	std::uint32_t type,
	std::uint64_t timestamp,
	SDL_WindowID window
)
{
	SDL_Event event{};
	event.type = type;
	event.window.timestamp = timestamp;
	event.window.windowID = window;
	return event;
}

[[nodiscard]] SDL_Event quit_event(std::uint64_t timestamp)
{
	SDL_Event event{};
	event.type = SDL_EVENT_QUIT;
	event.quit.timestamp = timestamp;
	return event;
}

[[nodiscard]] SdlInputEventResult expect_event(
	SdlInputMapper &mapper,
	TickInputScheduler &input,
	const SDL_Event &event,
	const char *message
)
{
	const auto result = mapper.handle_event(event, input);
	expect(static_cast<bool>(result), message);
	return result ? *result : SdlInputEventResult{};
}

[[nodiscard]] LogicalTickInput expect_tick(
	TickInputScheduler &input,
	std::uint64_t now,
	const char *message
)
{
	const auto due = input.pop_due(now);
	expect(due && due->has_value(), message);
	return (due && due->has_value()) ? **due : LogicalTickInput{};
}

void test_binding_validation()
{
	FakeGamepads fake{};
	const auto valid = SdlInputMapper::create(bindings(), main_window, fake.backend());
	expect(static_cast<bool>(valid), "explicit duplicate bindings are accepted");
	const auto unavailable = SdlInputMapper::create(
		bindings(), main_window, th03::platform::native_sdl_gamepad_backend()
	);
	expect(
		!unavailable && unavailable.error().code ==
			SdlInputMapperErrorCode::runtime_unavailable,
		"native gamepad mapper requires a live owning SDL platform runtime"
	);

	for(std::size_t variant = 0U; variant < 12U; ++variant) {
		auto config = bindings();
		auto backend = fake.backend();
		auto window = main_window;
		switch(variant) {
		case 0U: config.keyboard_count = config.keyboard.size() + 1U; break;
		case 1U: config.gamepad_button_count = config.gamepad_buttons.size() + 1U; break;
		case 2U: config.gamepad_axis_count = config.gamepad_axes.size() + 1U; break;
		case 3U: config.axis_release_threshold = 0; break;
		case 4U: config.axis_press_threshold = config.axis_release_threshold; break;
		case 5U: config.keyboard[0].scancode = SDL_SCANCODE_UNKNOWN; break;
		case 6U: config.keyboard[0].player = static_cast<SdlInputPlayer>(0xFFU); break;
		case 7U: config.gamepad_buttons[0].button = SDL_GAMEPAD_BUTTON_INVALID; break;
		case 8U: config.gamepad_axes[0].axis = SDL_GAMEPAD_AXIS_INVALID; break;
		case 9U: config.gamepad_axes[0].direction = static_cast<SdlAxisDirection>(0xFFU); break;
		case 10U: backend.open = nullptr; break;
		case 11U: window = 0U; break;
		default: break;
		}
		const auto rejected = SdlInputMapper::create(config, window, backend);
		expect(
			!rejected && rejected.error().code ==
				SdlInputMapperErrorCode::invalid_bindings,
			"invalid mapper configuration is rejected"
		);
	}

	auto invalid_action = bindings();
	invalid_action.keyboard[0].action = static_cast<SdlLogicalAction>(0xFFU);
	expect(
		!SdlInputMapper::create(invalid_action, main_window, fake.backend()),
		"invalid logical actions are rejected"
	);
	auto invalid_assignment = bindings();
	invalid_assignment.gamepad_players[0] = static_cast<SdlInputPlayer>(0xFFU);
	expect(
		!SdlInputMapper::create(invalid_assignment, main_window, fake.backend()),
		"invalid gamepad player assignments are rejected"
	);
}

void test_keyboard_snapshots_charge_and_timestamp_order()
{
	FakeGamepads fake{};
	auto made = SdlInputMapper::create(bindings(), main_window, fake.backend());
	expect(static_cast<bool>(made), "keyboard mapper is created");
	if(!made) {
		return;
	}
	auto mapper = std::move(*made);
	expect(mapper.main_window_focused(), "main window starts focused");
	auto input = scheduler();

	auto edge = expect_event(mapper, input, key_event(5U, SDL_SCANCODE_A, true),
		"first duplicate key press is accepted");
	expect(edge.handled && edge.physical_event_queued,
		"first duplicate key press emits a source snapshot");
	expect(mapper.keyboard_scancode_held(SDL_SCANCODE_A) &&
		!mapper.keyboard_scancode_held(SDL_SCANCODE_B),
		"host-only shortcuts can query an exact held scancode");
	expect(expect_tick(input, 10U, "first duplicate key press is due").input.p1 == input_shot,
		"first duplicate key contributes Shot");
	edge = expect_event(mapper, input, key_event(15U, SDL_SCANCODE_B, true),
		"second duplicate key press is accepted");
	expect(edge.handled && !edge.physical_event_queued,
		"unchanged recomputed contribution needs no scheduler event");
	expect(expect_tick(input, 20U, "both duplicate keys are held").input.p1 == input_shot,
		"two keys bound to Shot still contribute one Shot bit");
	edge = expect_event(mapper, input, key_event(25U, SDL_SCANCODE_A, false),
		"first duplicate key release is accepted");
	expect(edge.handled && !edge.physical_event_queued,
		"release retaining the logical action needs no scheduler event");
	expect(!mapper.keyboard_scancode_held(SDL_SCANCODE_A) &&
		mapper.keyboard_scancode_held(SDL_SCANCODE_B),
		"exact held-scancode queries distinguish duplicate logical bindings");
	expect(expect_tick(input, 30U, "second duplicate key remains held").input.p1 == input_shot,
		"releasing A cannot false-release B's Shot contribution");
	(void)expect_event(mapper, input, key_event(35U, SDL_SCANCODE_B, false),
		"last duplicate key release is accepted");
	expect(expect_tick(input, 40U, "last duplicate key release is due").input.p1 == 0U,
		"releasing the final bound key clears Shot");

	(void)expect_event(mapper, input, key_event(41U, SDL_SCANCODE_C, true),
		"charge key press is accepted");
	(void)expect_event(mapper, input, key_event(42U, SDL_SCANCODE_I, true),
		"P2 cardinal key press is accepted");
	(void)expect_event(mapper, input, key_event(43U, SDL_SCANCODE_O, true),
		"P2 bomb key press is accepted");
	const auto mixed = expect_tick(input, 50U, "mixed keyboard state is due");
	expect(
		mixed.input.p1 == input_shot && mixed.input.p2 == (input_up | input_bomb) &&
			mixed.input.charge_mask == 0x01U && mixed.input.system == 0U,
		"charge adds Shot and charge intent while P2/cardinal bits remain independent"
	);
	(void)expect_event(mapper, input, key_event(51U, SDL_SCANCODE_D, true),
		"source-global Cancel key press is accepted");
	(void)expect_event(mapper, input, key_event(52U, SDL_SCANCODE_E, true),
		"source-global OK key press is accepted");
	const auto system = expect_tick(input, 60U, "global controls are due");
	expect(system.input.system == (input_cancel | input_ok),
		"Cancel and OK are sampled in CoreInput.system without changing players");
	const auto repeated = expect_event(
		mapper, input, key_event(61U, SDL_SCANCODE_W, true, true),
		"keyboard repeat is accepted and ignored"
	);
	expect(repeated.handled && !repeated.physical_event_queued,
		"keyboard repeat does not emit a snapshot");

	SDL_Event raw{};
	raw.common.timestamp = 0xFEDC'BA98'7654'3210ULL;
	expect(
		th03::platform::sdl_event_timestamp_ns(raw) == 0xFEDC'BA98'7654'3210ULL,
		"SDL event timestamps preserve all 64 monotonic nanosecond bits"
	);
	const auto ticks_before = th03::platform::sdl_ticks_now_ns();
	const auto ticks_after = th03::platform::sdl_ticks_now_ns();
	expect(ticks_after >= ticks_before,
		"native SDL nanosecond clock observations are monotonic");
}

void test_complete_source_global_and_legacy_diagonal_domain()
{
	FakeGamepads fake{};
	SdlInputBindings complete{};
	const auto bind = [&complete](
		SDL_Scancode scancode,
		SdlInputPlayer player,
		SdlLogicalAction action
	) {
		complete.keyboard[complete.keyboard_count++] = {
			.scancode = scancode,
			.player = player,
			.action = action,
		};
	};
	bind(SDL_SCANCODE_A, SdlInputPlayer::p1, SdlLogicalAction::up_left);
	bind(SDL_SCANCODE_B, SdlInputPlayer::p2, SdlLogicalAction::down_right);
	bind(SDL_SCANCODE_C, SdlInputPlayer::p1, SdlLogicalAction::single_up);
	bind(SDL_SCANCODE_D, SdlInputPlayer::p1, SdlLogicalAction::single_down);
	bind(SDL_SCANCODE_E, SdlInputPlayer::p1, SdlLogicalAction::single_left);
	bind(SDL_SCANCODE_F, SdlInputPlayer::p1, SdlLogicalAction::single_right);
	bind(SDL_SCANCODE_G, SdlInputPlayer::p1, SdlLogicalAction::single_bomb);
	bind(SDL_SCANCODE_H, SdlInputPlayer::p1, SdlLogicalAction::single_shot);
	bind(SDL_SCANCODE_I, SdlInputPlayer::p1, SdlLogicalAction::single_up_left);
	bind(SDL_SCANCODE_J, SdlInputPlayer::p1, SdlLogicalAction::single_down_left);
	bind(SDL_SCANCODE_K, SdlInputPlayer::p1, SdlLogicalAction::single_up_right);
	bind(SDL_SCANCODE_L, SdlInputPlayer::p1, SdlLogicalAction::single_down_right);
	bind(SDL_SCANCODE_M, SdlInputPlayer::p1, SdlLogicalAction::cancel);
	bind(SDL_SCANCODE_N, SdlInputPlayer::p1, SdlLogicalAction::ok);
	bind(SDL_SCANCODE_O, SdlInputPlayer::p1, SdlLogicalAction::q);

	auto made = SdlInputMapper::create(complete, main_window, fake.backend());
	expect(static_cast<bool>(made),
		"complete input_sp mapper accepts every explicit logical action");
	if(!made) {
		return;
	}
	auto mapper = std::move(*made);
	auto input = scheduler(20U);
	for(std::size_t index = 0U; index < complete.keyboard_count; ++index) {
		(void)expect_event(
			mapper,
			input,
			key_event(
				static_cast<std::uint64_t>(index + 1U),
				complete.keyboard[index].scancode,
				true
			),
			"complete input_sp key edge is accepted"
		);
	}
	const auto sampled = expect_tick(
		input, 20U, "complete input_sp contribution is due"
	);
	expect(
		sampled.input.p1 == input_up_left &&
			sampled.input.p2 == input_down_right &&
			sampled.input.system == 0x7F3FU &&
			(sampled.input.system & (input_cancel | input_ok | input_q)) ==
				(input_cancel | input_ok | input_q),
		"player legacy diagonals and every donor input_sp bit remain distinct"
	);
}

void test_charge_enable_mask_precedes_shot_contribution()
{
	FakeGamepads fake{};
	auto configured = bindings();
	configured.charge_enabled_players = 0x02U;
	auto made = SdlInputMapper::create(configured, main_window, fake.backend());
	expect(static_cast<bool>(made), "charge-mask mapper is created");
	if(!made) {
		return;
	}
	auto mapper = std::move(*made);
	auto input = scheduler();
	(void)expect_event(mapper, input,
		key_event(1U, SDL_SCANCODE_C, true),
		"disabled P1 Charge key is held");
	auto tick = expect_tick(input, 10U, "disabled P1 Charge is due");
	expect(tick.input.p1 == 0U && tick.input.charge_mask == 0U,
		"disabled Charge contributes neither Shot nor charge intent");
	(void)expect_event(mapper, input,
		gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 11U, 11U),
		"P1 controller is connected for Charge mask");
	(void)expect_event(mapper, input,
		gamepad_button_event(12U, 11U, SDL_GAMEPAD_BUTTON_NORTH, true),
		"disabled P1 controller Charge is held");
	tick = expect_tick(input, 20U, "disabled controller Charge is due");
	expect(tick.input.p1 == 0U && tick.input.charge_mask == 0U,
		"controller Charge obeys the assigned player's enable bit");
	(void)expect_event(mapper, input,
		gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 21U, 12U),
		"P2 controller is connected for Charge mask");
	(void)expect_event(mapper, input,
		gamepad_button_event(22U, 12U, SDL_GAMEPAD_BUTTON_NORTH, true),
		"enabled P2 controller Charge is held");
	tick = expect_tick(input, 30U, "enabled P2 controller Charge is due");
	expect(tick.input.p2 == input_shot && tick.input.charge_mask == 0x02U,
		"enabled Charge contributes Shot and intent only to its assigned player");
}

void test_equal_and_regressed_event_order()
{
	FakeGamepads fake{};
	auto made = SdlInputMapper::create(bindings(), main_window, fake.backend());
	expect(static_cast<bool>(made), "event-order mapper is created");
	if(!made) {
		return;
	}
	auto mapper = std::move(*made);
	auto input = scheduler(100U);
	(void)expect_event(mapper, input, key_event(10U, SDL_SCANCODE_A, true),
		"equal-time key press is accepted");
	(void)expect_event(mapper, input, key_event(10U, SDL_SCANCODE_A, false),
		"equal-time key release is accepted after its press");
	expect(expect_tick(input, 100U, "equal-time events are due").input.p1 == 0U,
		"equal SDL timestamps retain polling/injection order");

	auto second_made = SdlInputMapper::create(bindings(), main_window, fake.backend());
	expect(static_cast<bool>(second_made), "regression mapper is created");
	if(!second_made) {
		return;
	}
	auto second = std::move(*second_made);
	auto second_input = scheduler(100U);
	(void)expect_event(second, second_input, key_event(10U, SDL_SCANCODE_A, true),
		"ordered key press is accepted");
	const auto regressed = second.handle_event(
		key_event(9U, SDL_SCANCODE_A, false), second_input
	);
	expect(
		regressed && regressed->physical_event_queued,
		"regressed live SDL timestamp preserves poll order without terminating"
	);
	expect(expect_tick(second_input, 100U, "normalized live order is due").input.p1 == 0U,
		"normalized press/release retains SDL polling order");

	auto late_made = SdlInputMapper::create(bindings(), main_window, fake.backend());
	expect(static_cast<bool>(late_made), "rapid late-input mapper is created");
	if(late_made) {
		auto late = std::move(*late_made);
		auto late_input = scheduler(100U);
		(void)expect_tick(late_input, 100U,
			"rapid-input test first emits the old logical boundary");
		(void)expect_event(late, late_input,
			key_event(100U, SDL_SCANCODE_A, true),
			"rapid key-down delivered at an emitted boundary is accepted");
		(void)expect_event(late, late_input,
			key_event(100U, SDL_SCANCODE_A, false),
			"rapid key-up at the same emitted boundary is accepted");
		expect(expect_tick(late_input, 110U,
			"rapid late edges are sampled at the next boundary").input.p1 == 0U,
			"an entirely between-ticks tap is nonfatal and non-retroactive");
	}
}

void test_keyboard_window_scope()
{
	FakeGamepads fake{};
	auto made = SdlInputMapper::create(bindings(), main_window, fake.backend());
	expect(static_cast<bool>(made), "window-scope mapper is created");
	if(!made) {
		return;
	}
	auto mapper = std::move(*made);
	auto input = scheduler();
	auto foreign = key_event(5U, SDL_SCANCODE_A, true);
	foreign.key.windowID = main_window + 1U;
	const auto ignored = expect_event(
		mapper, input, foreign, "foreign-window key edge is inspected"
	);
	expect(
		!ignored.handled && !ignored.physical_event_queued &&
			input.pending_event_count() == 0U,
		"foreign-window key edge cannot enter the gameplay input source"
	);
	auto global = key_event(6U, SDL_SCANCODE_A, true);
	global.key.windowID = 0U;
	const auto global_ignored = expect_event(
		mapper, input, global, "windowless key edge is inspected"
	);
	expect(!global_ignored.handled && input.pending_event_count() == 0U,
		"windowless key edge remains outside the configured game window");
	(void)expect_event(
		mapper, input, key_event(7U, SDL_SCANCODE_A, true),
		"main-window key edge is accepted after ignored foreign edges"
	);
	expect(expect_tick(input, 10U, "main-window key edge is due").input.p1 == input_shot,
		"only the configured window contributes keyboard gameplay input");
}

void test_gamepad_buttons_axes_and_lifecycle()
{
	FakeGamepads fake{};
	fake.fail_id = 99U;
	{
		auto made = SdlInputMapper::create(bindings(), main_window, fake.backend());
		expect(static_cast<bool>(made), "gamepad mapper is created");
		if(!made) {
			return;
		}
		auto mapper = std::move(*made);
		auto input = scheduler();
		(void)expect_event(mapper, input,
			gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 1U, 11U),
			"first gamepad is opened");
		(void)expect_event(mapper, input,
			gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 2U, 22U),
			"second gamepad is opened");
		(void)expect_event(mapper, input,
			gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 3U, 11U),
			"duplicate add is idempotent");
		expect(
			mapper.connected_gamepad_count() == 2U && fake.open_count == 2U &&
				mapper.source_for(11U) == 1U && mapper.source_for(22U) == 2U,
			"gamepads receive stable first-free physical source slots"
		);
		const auto open_failed = mapper.handle_event(
			gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 4U, 99U), input
		);
		expect(
			!open_failed && open_failed.error().code ==
				SdlInputMapperErrorCode::gamepad_open_failed &&
				open_failed.error().gamepad_id == 99U &&
				mapper.connected_gamepad_count() == 2U,
			"failed gamepad open publishes no device slot"
		);

		(void)expect_event(mapper, input,
			gamepad_button_event(5U, 11U, SDL_GAMEPAD_BUTTON_SOUTH, true),
			"first gamepad Shot button is pressed");
		(void)expect_event(mapper, input,
			gamepad_button_event(6U, 11U, SDL_GAMEPAD_BUTTON_WEST, true),
			"duplicate gamepad Shot button is pressed");
		(void)expect_event(mapper, input,
			gamepad_button_event(7U, 11U, SDL_GAMEPAD_BUTTON_SOUTH, false),
			"first duplicate gamepad Shot button is released");
		(void)expect_event(mapper, input,
			gamepad_button_event(8U, 22U, SDL_GAMEPAD_BUTTON_EAST, true),
			"second gamepad P2 Bomb button is pressed");
		const auto buttons = expect_tick(input, 10U, "gamepad button state is due");
		expect(
			buttons.input.p1 == input_shot && buttons.input.p2 == input_bomb,
			"same-device duplicate button release retains Shot and slot two maps P2"
		);
		expect(buttons.physical_domains &&
			buttons.physical_domains->first_joystick_present &&
			buttons.physical_domains->first_joystick == input_shot,
			"first connected gamepad alone supplies PC-98 joystick zero");

		(void)expect_event(mapper, input,
			gamepad_button_event(11U, 11U, SDL_GAMEPAD_BUTTON_DPAD_LEFT, true),
			"D-pad Left is pressed");
		(void)expect_event(mapper, input,
			gamepad_axis_event(12U, 11U, SDL_GAMEPAD_AXIS_LEFTX, -16384),
			"axis activates at the inclusive negative press threshold");
		(void)expect_event(mapper, input,
			gamepad_button_event(13U, 11U, SDL_GAMEPAD_BUTTON_DPAD_LEFT, false),
			"D-pad Left releases while the axis remains active");
		expect(
			(expect_tick(input, 20U, "axis and D-pad duplicate state is due").input.p1 &
				input_left) != 0U,
			"D-pad release cannot false-release the matching active axis"
		);
		(void)expect_event(mapper, input,
			gamepad_axis_event(21U, 11U, SDL_GAMEPAD_AXIS_LEFTX, -12289),
			"active axis remains held just beyond the release threshold");
		expect(
			(expect_tick(input, 30U, "axis hysteresis hold is due").input.p1 &
				input_left) != 0U,
			"axis hysteresis retains the direction above its release magnitude"
		);
		(void)expect_event(mapper, input,
			gamepad_axis_event(31U, 11U, SDL_GAMEPAD_AXIS_LEFTX, -12288),
			"axis releases at the inclusive release threshold");
		expect(
			(expect_tick(input, 40U, "axis hysteresis release is due").input.p1 &
				input_left) == 0U,
			"axis direction clears exactly at the release threshold"
		);

		(void)expect_event(mapper, input,
			gamepad_device_event(SDL_EVENT_GAMEPAD_REMAPPED, 41U, 11U),
			"gamepad remap clears its held contribution");
		expect(mapper.source_for(11U) == 1U && fake.close_count == 0U,
			"remap retains the open device and physical source slot");
		(void)expect_event(mapper, input,
			gamepad_device_event(SDL_EVENT_GAMEPAD_REMOVED, 42U, 22U),
			"gamepad removal clears and closes its source");
		expect(
			mapper.connected_gamepad_count() == 1U && !mapper.source_for(22U) &&
				fake.close_count == 1U,
			"device removal publishes close only after its release is queued"
		);
		const auto stale = expect_event(mapper, input,
			gamepad_button_event(43U, 22U, SDL_GAMEPAD_BUTTON_EAST, true),
			"event from a removed gamepad is ignored");
		expect(!stale.handled && !stale.physical_event_queued,
			"stale gamepad event cannot recreate a source");
		const auto remapped = expect_tick(
			input, 50U, "remap and removal lifecycle is due"
		);
		expect(remapped.physical_domains &&
			remapped.physical_domains->first_joystick_present &&
			remapped.physical_domains->first_joystick == 0U,
			"remap retains neutral presence while removal clears only its source");
	}
	expect(fake.close_count == 2U,
		"mapper destruction closes every still-connected gamepad exactly once");
}

void test_stable_identity_assignment_and_catalog()
{
	FakeGamepads fake{};
	auto configured = bindings();
	configured.identity_assignment_enabled = true;
	configured.device_assignment.mode = {
		th03::app::NewClassicControllerAssignmentMode::specific,
		th03::app::NewClassicControllerAssignmentMode::specific,
	};
	configured.device_assignment.identity[0].bytes[0] = 22U;
	configured.device_assignment.identity[1].bytes[0] = 11U;
	auto made = SdlInputMapper::create(
		configured, main_window, fake.backend()
	);
	expect(static_cast<bool>(made), "identity mapper is created");
	if(!made) {
		return;
	}
	auto mapper = std::move(*made);
	auto input = scheduler(20U);
	(void)expect_event(mapper, input,
		gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 1U, 11U),
		"P2-specific controller connects first");
	(void)expect_event(mapper, input,
		gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 2U, 22U),
		"P1-specific controller connects second");
	(void)expect_event(mapper, input,
		gamepad_button_event(3U, 11U, SDL_GAMEPAD_BUTTON_SOUTH, true),
		"specific P2 controller presses Shot");
	auto tick = expect_tick(input, 20U, "specific assignment tick is due");
	expect(tick.input.p1 == 0U && tick.input.p2 == input_shot,
		"stable identity overrides connection-order player parity");
	auto catalog = mapper.controller_catalog();
	expect(catalog.count == 2U &&
		catalog.controllers[0].player ==
			th03::app::NewClassicControllerPlayer::p2 &&
		catalog.controllers[1].player ==
			th03::app::NewClassicControllerPlayer::p1 &&
		catalog.controllers[0].p2 == input_shot,
		"catalog exposes stable assignment and current logical contribution");

	(void)expect_event(mapper, input,
		gamepad_device_event(SDL_EVENT_GAMEPAD_REMOVED, 21U, 11U),
		"specific controller disconnects");
	(void)expect_event(mapper, input,
		gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 22U, 111U),
		"specific controller reconnects with a new transient instance ID");
	(void)expect_event(mapper, input,
		gamepad_button_event(23U, 111U, SDL_GAMEPAD_BUTTON_SOUTH, true),
		"reconnected specific controller presses Shot");
	tick = expect_tick(input, 30U, "reconnected assignment tick is due");
	expect(tick.input.p1 == 0U && tick.input.p2 == input_shot,
		"hotplug retains the identity-selected player");
	(void)expect_event(mapper, input,
		gamepad_button_event(31U, 111U, SDL_GAMEPAD_BUTTON_SOUTH, false),
		"specific controller releases Shot before reassignment");
	auto only_p2 = configured;
	only_p2.device_assignment = {};
	only_p2.device_assignment.mode[0] =
		th03::app::NewClassicControllerAssignmentMode::disabled;
	expect(static_cast<bool>(mapper.replace_bindings(only_p2)),
		"one-player Automatic policy replaces between released boundaries");
	catalog = mapper.controller_catalog();
	expect(catalog.count == 2U &&
		catalog.controllers[0].player ==
			th03::app::NewClassicControllerPlayer::p2 &&
		catalog.controllers[1].player ==
			th03::app::NewClassicControllerPlayer::p2,
		"one Automatic player owns every unmatched connected controller");
	auto disabled = configured;
	disabled.device_assignment = {};
	disabled.device_assignment.mode = {
		th03::app::NewClassicControllerAssignmentMode::disabled,
		th03::app::NewClassicControllerAssignmentMode::disabled,
	};
	disabled.gamepad_buttons[disabled.gamepad_button_count++] = {
		.button = SDL_GAMEPAD_BUTTON_START,
		.action = SdlLogicalAction::ok,
	};
	expect(static_cast<bool>(mapper.replace_bindings(disabled)),
		"live device assignment replaces between released input boundaries");
	(void)expect_event(mapper, input,
		gamepad_button_event(32U, 111U, SDL_GAMEPAD_BUTTON_START, true),
		"unassigned controller presses the fixed Menu OK button");
	tick = expect_tick(input, 40U, "unassigned menu input tick is due");
	expect(tick.input.p1 == 0U && tick.input.p2 == 0U &&
		tick.input.system == input_ok,
		"unassigned controllers retain menu access without controlling a player");
}

void test_focus_keyboard_device_release_and_quit_separation()
{
	FakeGamepads fake{};
	auto made = SdlInputMapper::create(bindings(), main_window, fake.backend());
	expect(static_cast<bool>(made), "focus mapper is created");
	if(!made) {
		return;
	}
	auto mapper = std::move(*made);
	auto input = scheduler();
	(void)expect_event(mapper, input,
		gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 1U, 11U),
		"focus test gamepad is connected");
	(void)expect_event(mapper, input, key_event(2U, SDL_SCANCODE_W, true),
		"keyboard direction is held before focus loss");
	(void)expect_event(mapper, input,
		gamepad_button_event(3U, 11U, SDL_GAMEPAD_BUTTON_EAST, true),
		"gamepad button is held before focus loss");
	const auto other_focus = expect_event(mapper, input,
		window_event(SDL_EVENT_WINDOW_FOCUS_LOST, 4U, main_window + 1U),
		"focus loss from another window is ignored");
	expect(!other_focus.handled, "only the configured main window controls focus release");
	const auto held = expect_tick(input, 10U, "pre-focus held state is due");
	expect(held.input.p1 == (input_up | input_bomb),
		"keyboard and gamepad source contributions combine before focus loss");
	expect(held.physical_domains &&
		held.physical_domains->keyboard.p1 == input_up &&
		held.physical_domains->first_joystick_present &&
		held.physical_domains->first_joystick == input_bomb,
		"keyboard and first gamepad remain separately observable");
	(void)expect_event(mapper, input,
		window_event(SDL_EVENT_WINDOW_FOCUS_LOST, 11U, main_window),
		"main-window focus loss releases every source");
	expect(!mapper.main_window_focused(),
		"main-window focus loss is retained for output policy");
	const auto released = expect_tick(input, 20U, "focus release is due");
	expect(
		released.input.p1 == 0U && released.input.p2 == 0U &&
			released.input.charge_mask == 0U && mapper.connected_gamepad_count() == 1U &&
			fake.close_count == 0U,
		"focus loss clears held state without disconnecting devices"
	);
	expect(released.physical_domains &&
		released.physical_domains->first_joystick_present &&
		released.physical_domains->first_joystick == 0U,
		"focus loss retains neutral first-gamepad presence");
	const auto gained = expect_event(mapper, input,
		window_event(SDL_EVENT_WINDOW_FOCUS_GAINED, 20U, main_window),
		"main-window focus gain is handled without an input edge");
	expect(gained.handled && !gained.physical_event_queued &&
		mapper.main_window_focused(),
		"focus gain restores output policy without synthesizing gameplay input");

	(void)expect_event(mapper, input, key_event(21U, SDL_SCANCODE_W, true),
		"keyboard state can be held again after focus loss");
	(void)expect_event(mapper, input, keyboard_removed(22U),
		"keyboard-device removal releases keyboard source zero");
	(void)expect_event(mapper, input,
		gamepad_button_event(23U, 11U, SDL_GAMEPAD_BUTTON_EAST, true),
		"gamepad remains usable after keyboard removal");
	expect(expect_tick(input, 30U, "keyboard removal state is due").input.p1 == input_bomb,
		"keyboard removal clears only source zero, not a connected controller");

	const auto other_close = expect_event(mapper, input,
		window_event(SDL_EVENT_WINDOW_CLOSE_REQUESTED, 31U, main_window + 1U),
		"other-window close request is ignored");
	expect(!other_close.handled, "other-window close cannot request app quit");
	(void)expect_event(mapper, input,
		window_event(SDL_EVENT_WINDOW_CLOSE_REQUESTED, 32U, main_window),
		"main-window close requests application quit");
	const auto quit = expect_tick(input, 40U, "window quit request is due");
	expect(
		quit.app_quit_requested && quit.input.system == 0U &&
			quit.input.p1 == input_bomb,
		"application quit is sticky and separate from gameplay Cancel/input state"
	);
	(void)expect_event(mapper, input, quit_event(41U),
		"global SDL quit event is accepted");
	const auto global_quit = expect_tick(input, 50U, "global quit is due");
	expect(global_quit.app_quit_requested && global_quit.input.system == 0U,
		"global quit also remains outside CoreInput.system");
}

void test_modal_reset_releases_input_and_retains_devices()
{
	FakeGamepads fake{};
	auto made = SdlInputMapper::create(bindings(), main_window, fake.backend());
	expect(static_cast<bool>(made), "modal-reset mapper is created");
	if(!made) {
		return;
	}
	auto mapper = std::move(*made);
	auto input = scheduler();
	(void)expect_event(mapper, input,
		gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 1U, 11U),
		"modal-reset gamepad is connected");
	(void)expect_event(mapper, input, key_event(2U, SDL_SCANCODE_A, true),
		"save-confirm key is held before modal ownership");
	(void)expect_event(mapper, input,
		gamepad_button_event(3U, 11U, SDL_GAMEPAD_BUTTON_EAST, true),
		"gamepad button is held before modal ownership");
	const auto held = expect_tick(input, 10U, "pre-modal held state is due");
	expect(held.tick_index == 0U && held.input.p1 == (input_shot | input_bomb),
		"both held sources are visible before modal ownership");

	expect(input.rebase_after_modal(1'000U).has_value(),
		"modal return rebases the gameplay scheduler");
	expect(mapper.reset_after_modal(input, 1'000U).has_value(),
		"modal return clears mapper state and republishes neutral sources");
	const auto neutral = expect_tick(input, 1'000U,
		"modal return immediately produces one neutral contiguous tick");
	expect(
		neutral.tick_index == 1U && neutral.deadline_ns == 1'000U &&
			neutral.input.p1 == 0U && neutral.input.p2 == 0U &&
			input.pending_event_count() == 0U,
		"modal return preserves logical order without catch-up or held input"
	);
	expect(neutral.physical_domains &&
		neutral.physical_domains->keyboard.p1 == 0U &&
		neutral.physical_domains->first_joystick_present &&
		neutral.physical_domains->first_joystick == 0U &&
		mapper.connected_gamepad_count() == 1U,
		"modal reset retains a connected controller as a neutral domain");

	(void)expect_event(mapper, input, key_event(1'001U, SDL_SCANCODE_W, true),
		"an unrelated post-modal direction is queued");
	const auto later = expect_tick(input, 100'001'000U,
		"the first ordinary post-modal deadline is due");
	expect(later.input.p1 == input_up,
		"an unrelated edge cannot resurrect pre-modal confirm or bomb input");
}

void test_pause_reset_reuses_host_iteration_cutoff()
{
	FakeGamepads fake{};
	auto made = SdlInputMapper::create(bindings(), main_window, fake.backend());
	expect(static_cast<bool>(made), "Pause-reset mapper is created");
	if(!made) {
		return;
	}
	auto mapper = std::move(*made);
	auto input = scheduler();
	(void)expect_event(mapper, input,
		key_event(5U, SDL_SCANCODE_D, true),
		"opening Cancel is queued before the Pause boundary");
	const auto opening = expect_tick(input, 10U, "opening Pause input is due");
	expect(opening.input.system == input_cancel,
		"the opening logical sample owns Cancel before modal reset");

	constexpr std::uint64_t loop_cutoff = 15U;
	expect(input.rebase_after_modal(loop_cutoff).has_value(),
		"Pause entry rebases at the current host-iteration cutoff");
	expect(mapper.reset_after_modal(input, loop_cutoff).has_value(),
		"Pause entry resets mapper state at the same cutoff");
	const auto released = input.pop_due(loop_cutoff);
	expect(
		released && *released && (**released).deadline_ns == loop_cutoff &&
			(**released).input.system == 0U,
		"the remainder of the same host iteration observes neutral Pause input "
		"without a timestamp regression"
	);

	(void)expect_event(mapper, input,
		key_event(16U, SDL_SCANCODE_D, false),
		"physical opening Cancel is released after Pause entry");
	(void)expect_event(mapper, input,
		key_event(17U, SDL_SCANCODE_D, true),
		"fresh Cancel is pressed for Resume");
	const auto resumed = expect_tick(input, 25U, "fresh Resume input is due");
	expect(resumed.input.system == input_cancel,
		"only the fresh post-release Cancel reaches the Resume barrier");
}

void test_malformed_stale_and_capacity_events()
{
	FakeGamepads fake{};
	auto made = SdlInputMapper::create(bindings(), main_window, fake.backend());
	expect(static_cast<bool>(made), "malformed-event mapper is created");
	if(!made) {
		return;
	}
	auto mapper = std::move(*made);
	auto input = scheduler(100U);

	auto malformed_key = key_event(1U, SDL_SCANCODE_A, true);
	malformed_key.key.down = false;
	for(const auto &event : std::array{
		malformed_key,
		key_event(
			1U, static_cast<SDL_Scancode>(SDL_SCANCODE_COUNT), true
		),
		gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 1U, 0U),
		gamepad_button_event(1U, 0U, SDL_GAMEPAD_BUTTON_SOUTH, true),
		gamepad_button_event(1U, 1U, SDL_GAMEPAD_BUTTON_INVALID, true),
		gamepad_axis_event(1U, 0U, SDL_GAMEPAD_AXIS_LEFTX, 0),
		gamepad_axis_event(1U, 1U, SDL_GAMEPAD_AXIS_INVALID, 0),
	}) {
		const auto rejected = mapper.handle_event(event, input);
		expect(
			!rejected && rejected.error().code ==
				SdlInputMapperErrorCode::malformed_event &&
				input.pending_event_count() == 0U,
			"malformed SDL event is rejected without scheduler mutation"
		);
	}

	const auto sysrq = expect_event(
		mapper, input,
		key_event(2U, SDL_SCANCODE_UNKNOWN, true),
		"Windows Alt+PrintScreen SysRq event is accepted"
	);
	expect(
		sysrq.handled && !sysrq.physical_event_queued &&
			input.pending_event_count() == 0U &&
			!mapper.take_keyboard_scancode_press() &&
			!mapper.any_keyboard_scancode_held(),
		"unknown raw key is ignored without gameplay or KeyConfig state"
	);

	SDL_Event mouse{};
	mouse.type = SDL_EVENT_MOUSE_MOTION;
	mouse.motion.timestamp = 2U;
	const auto unknown = expect_event(mapper, input, mouse,
		"unrelated SDL event is ignored");
	expect(!unknown.handled, "unrelated SDL events remain outside the mapper");
	const auto stale = expect_event(mapper, input,
		gamepad_button_event(3U, 77U, SDL_GAMEPAD_BUTTON_SOUTH, true),
		"well-formed event for an unknown gamepad is ignored");
	expect(!stale.handled, "unknown gamepad cannot mutate scheduler state");

	for(std::size_t index = 0U; index < th03::platform::sdl_gamepad_capacity; ++index) {
		const auto id = static_cast<SDL_JoystickID>(index + 1U);
		(void)expect_event(mapper, input,
			gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 4U + index, id),
			"fixed gamepad slot is filled");
	}
	const auto capacity = mapper.handle_event(
		gamepad_device_event(
			SDL_EVENT_GAMEPAD_ADDED,
			4U + th03::platform::sdl_gamepad_capacity,
			100U
		),
		input
	);
	expect(
		!capacity && capacity.error().code ==
			SdlInputMapperErrorCode::gamepad_capacity &&
			mapper.connected_gamepad_count() == th03::platform::sdl_gamepad_capacity,
		"fixed gamepad capacity reports an error without replacing a device"
	);
}

void test_raw_key_capture_and_binding_replacement()
{
	FakeGamepads fake{};
	auto made = SdlInputMapper::create(bindings(), main_window, fake.backend());
	expect(static_cast<bool>(made), "raw-capture mapper is created");
	if(!made) {
		return;
	}
	auto mapper = std::move(*made);
	auto input = scheduler(100U);
	const auto unbound_down = expect_event(
		mapper, input, key_event(1U, SDL_SCANCODE_Z, true),
		"unbound key is accepted for KeyConfig capture"
	);
	expect(unbound_down.handled && !unbound_down.physical_event_queued &&
		mapper.any_keyboard_scancode_held() &&
		mapper.keyboard_scancode_held(SDL_SCANCODE_Z) &&
		mapper.take_keyboard_scancode_press() == SDL_SCANCODE_Z &&
		!mapper.take_keyboard_scancode_press(),
		"raw capture reports one press and held state without logical input");
	(void)expect_event(
		mapper, input, key_event(2U, SDL_SCANCODE_Z, false),
		"unbound key release is accepted"
	);
	expect(!mapper.any_keyboard_scancode_held(),
		"raw release completes KeyConfig's release barrier");

	auto replacement = bindings();
	replacement.keyboard[replacement.keyboard_count++] = {
		.scancode = SDL_SCANCODE_Z,
		.player = SdlInputPlayer::p1,
		.action = SdlLogicalAction::bomb,
	};
	expect(mapper.replace_bindings(replacement).has_value(),
		"valid keyboard policy replaces bindings between schedulers");
	const auto mapped_down = expect_event(
		mapper, input, key_event(3U, SDL_SCANCODE_Z, true),
		"newly configured key emits logical input"
	);
	expect(mapped_down.physical_event_queued &&
		expect_tick(input, 100U, "replacement key state is due").input.p1 ==
			input_bomb,
		"binding replacement takes effect without rebuilding the mapper");
}

void test_physical_text_input_queue()
{
	FakeGamepads fake{};
	auto made = SdlInputMapper::create(bindings(), main_window, fake.backend());
	expect(static_cast<bool>(made), "text-input mapper is created");
	if(!made) {
		return;
	}
	auto mapper = std::move(*made);
	auto input = scheduler(100U);
	const auto typed = expect_event(
		mapper, input, text_event(1U, "X:7"),
		"SDL text composition accepts uppercase and shifted punctuation"
	);
	expect(typed.handled && !typed.physical_event_queued &&
		mapper.take_keyboard_text_character() == 'X' &&
		mapper.take_keyboard_text_character() == ':' &&
		mapper.take_keyboard_text_character() == '7' &&
		!mapper.take_keyboard_text_character() &&
		input.pending_event_count() == 0U,
		"text characters are retained in order without becoming gameplay input");
	const auto foreign = expect_event(
		mapper, input, text_event(2U, "BAD", main_window + 1U),
		"foreign-window text is ignored"
	);
	expect(!foreign.handled && !mapper.take_keyboard_text_character(),
		"only the game window can populate the editor queue");
	(void)expect_event(
		mapper, input, text_event(3U, "pending"),
		"modal-reset text is staged"
	);
	(void)expect_event(
		mapper, input, keyboard_removed(4U),
		"keyboard removal clears text composition"
	);
	expect(!mapper.take_keyboard_text_character(),
		"device loss cannot leak stale text into a later editor");
}

void test_queue_full_atomicity()
{
	FakeGamepads fake{};
	{
		auto made = SdlInputMapper::create(bindings(), main_window, fake.backend());
		expect(static_cast<bool>(made), "queue-full key mapper is created");
		if(!made) {
			return;
		}
		auto mapper = std::move(*made);
		auto input = scheduler(2'000U);
		for(std::size_t index = 0U;
			index < th03::app::pending_physical_event_capacity;
			++index) {
			const auto pushed = input.push_event(PhysicalInputEvent{
				.timestamp_ns = 2'001U + index,
				.kind = PhysicalInputEventKind::app_quit,
			});
			expect(static_cast<bool>(pushed), "scheduler queue slot is filled");
		}
		const auto rejected = mapper.handle_event(
			key_event(2'256U, SDL_SCANCODE_A, true), input
		);
		expect(
			!rejected && rejected.error().code == SdlInputMapperErrorCode::tick_input &&
				rejected.error().tick_input == TickInputError::event_queue_full,
			"full scheduler queue rejects a mapper snapshot explicitly"
		);
		(void)expect_tick(input, 2'000U, "one due tick frees one queue slot");
		(void)expect_event(mapper, input,
			key_event(2'256U, SDL_SCANCODE_A, true),
			"same key edge can be retried after draining one slot");
		LogicalTickInput last{};
		while(input.next_deadline_ns() &&
			*input.next_deadline_ns() <= 2'260U) {
			last = expect_tick(
				input,
				*input.next_deadline_ns(),
				"queued history deadline remains visible"
			);
		}
		expect(last.input.p1 == input_shot,
			"queue-full rejection did not prematurely commit held keyboard state");
	}

	{
		auto made = SdlInputMapper::create(bindings(), main_window, fake.backend());
		expect(static_cast<bool>(made), "queue-full removal mapper is created");
		if(!made) {
			return;
		}
		auto mapper = std::move(*made);
		auto input = scheduler(2'000U);
		(void)expect_event(mapper, input,
			gamepad_device_event(SDL_EVENT_GAMEPAD_ADDED, 1U, 11U),
			"atomic-removal gamepad is opened");
		const auto close_before = fake.close_count;
		for(std::size_t index = 0U;
			index < th03::app::pending_physical_event_capacity;
			++index) {
			const auto pushed = input.push_event(PhysicalInputEvent{
				.timestamp_ns = 2'001U + index,
				.kind = PhysicalInputEventKind::app_quit,
			});
			expect(static_cast<bool>(pushed), "removal test queue slot is filled");
		}
		const auto removal = gamepad_device_event(
			SDL_EVENT_GAMEPAD_REMOVED, 2'256U, 11U
		);
		const auto rejected = mapper.handle_event(removal, input);
		expect(
			!rejected && rejected.error().code == SdlInputMapperErrorCode::tick_input &&
				rejected.error().tick_input == TickInputError::event_queue_full &&
				mapper.source_for(11U) == 1U && fake.close_count == close_before,
			"queue-full device removal leaves source and native handle published"
		);
		(void)expect_tick(input, 2'000U, "removal retry frees one queue slot");
		(void)expect_event(mapper, input, removal,
			"same device removal can be retried after draining");
		expect(!mapper.source_for(11U) && fake.close_count == close_before + 1U,
			"successful retry closes and clears the exact device once");
	}
}

} // namespace

int main()
{
	test_binding_validation();
	test_keyboard_snapshots_charge_and_timestamp_order();
	test_complete_source_global_and_legacy_diagonal_domain();
	test_charge_enable_mask_precedes_shot_contribution();
	test_equal_and_regressed_event_order();
	test_keyboard_window_scope();
	test_gamepad_buttons_axes_and_lifecycle();
	test_stable_identity_assignment_and_catalog();
	test_focus_keyboard_device_release_and_quit_separation();
	test_modal_reset_releases_input_and_retains_devices();
	test_pause_reset_reuses_host_iteration_cutoff();
	test_malformed_stale_and_capacity_events();
	test_raw_key_capture_and_binding_replacement();
	test_physical_text_input_queue();
	test_queue_full_atomicity();

	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << "All " << checks << " SDL input mapper checks passed\n";
	return 0;
}
