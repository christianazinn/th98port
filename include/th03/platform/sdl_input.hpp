#ifndef TH03_PLATFORM_SDL_INPUT_HPP
#define TH03_PLATFORM_SDL_INPUT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_timer.h>

#include "th03/app/new_classic_controller.hpp"
#include "th03/app/tick_input.hpp"

namespace th03::platform {

inline constexpr std::size_t sdl_keyboard_binding_capacity = 64U;
inline constexpr std::size_t sdl_gamepad_button_binding_capacity = 32U;
inline constexpr std::size_t sdl_gamepad_axis_binding_capacity = 8U;
inline constexpr std::size_t sdl_gamepad_capacity =
	app::physical_input_source_count - 1U;

enum class SdlInputPlayer : std::uint8_t {
	p1,
	p2,
	none,
};

enum class SdlLogicalAction : std::uint8_t {
	up,
	down,
	left,
	right,
	bomb,
	shot,
	up_left,
	down_left,
	up_right,
	down_right,
	// Complete source-global input_sp actions. These are independent from the
	// selected P1/P2 contribution even when one physical key binds to both.
	single_up,
	single_down,
	single_left,
	single_right,
	single_bomb,
	single_shot,
	single_up_left,
	single_down_left,
	single_up_right,
	single_down_right,
	cancel,
	ok,
	q,
	// Charge is the physical intentional-charge action under Replay Autofire:
	// it contributes Shot and the selected player's charge bit together.
	charge,
};

enum class SdlAxisDirection : std::uint8_t {
	negative,
	positive,
};

struct SdlKeyboardBinding {
	SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
	SdlInputPlayer player = SdlInputPlayer::p1;
	SdlLogicalAction action = SdlLogicalAction::up;
};

struct SdlGamepadButtonBinding {
	SDL_GamepadButton button = SDL_GAMEPAD_BUTTON_INVALID;
	SdlLogicalAction action = SdlLogicalAction::up;
};

struct SdlGamepadAxisBinding {
	SDL_GamepadAxis axis = SDL_GAMEPAD_AXIS_INVALID;
	SdlAxisDirection direction = SdlAxisDirection::negative;
	SdlLogicalAction action = SdlLogicalAction::up;
};

// Bindings are copied into the mapper. Duplicate physical-to-logical bindings
// are intentional and remain independently held until their own release edge.
// Controller player assignment follows the fixed physical source slot selected
// at connection time; source 0 is the aggregate keyboard source.
struct SdlInputBindings {
	std::array<SdlKeyboardBinding, sdl_keyboard_binding_capacity> keyboard{};
	std::size_t keyboard_count = 0U;
	std::array<
		SdlGamepadButtonBinding,
		sdl_gamepad_button_binding_capacity
	> gamepad_buttons{};
	std::size_t gamepad_button_count = 0U;
	std::array<SdlGamepadAxisBinding, sdl_gamepad_axis_binding_capacity>
		gamepad_axes{};
	std::size_t gamepad_axis_count = 0U;
	std::array<SdlInputPlayer, sdl_gamepad_capacity> gamepad_players{};
	bool identity_assignment_enabled = false;
	app::NewClassicDeviceAssignment device_assignment{};
	std::int16_t axis_press_threshold = 16384;
	std::int16_t axis_release_threshold = 12288;
};

using SdlGamepadOpen = SDL_Gamepad *(*) (
	void *context,
	SDL_JoystickID instance_id
) noexcept;
using SdlGamepadClose = void (*) (
	void *context,
	SDL_Gamepad *gamepad
) noexcept;
using SdlGamepadIdentify = bool (*) (
	void *context,
	SDL_Gamepad *gamepad,
	SDL_JoystickID instance_id,
	app::NewClassicControllerIdentity &identity,
	std::array<char, app::new_classic_controller_name_bytes> &name
) noexcept;

// Device lifecycle is injectable so the fixed event mapper has hermetic tests.
// The native backend may allocate inside SDL only on add/remove, never on an
// ordinary key, button, or axis edge.
struct SdlGamepadBackend {
	void *context = nullptr;
	SdlGamepadOpen open = nullptr;
	SdlGamepadClose close = nullptr;
	SdlGamepadIdentify identify = nullptr;
};

[[nodiscard]] SdlGamepadBackend native_sdl_gamepad_backend() noexcept;

// SDL event timestamps and SDL_GetTicksNS() already use the same monotonic
// nanosecond domain. These functions preserve all 64 bits without rescaling.
[[nodiscard]] app::TickTimestampNs sdl_ticks_now_ns() noexcept;
[[nodiscard]] app::TickTimestampNs sdl_event_timestamp_ns(
	const SDL_Event &event
) noexcept;

enum class SdlInputMapperErrorCode : std::uint8_t {
	invalid_bindings,
	runtime_unavailable,
	malformed_event,
	gamepad_capacity,
	gamepad_open_failed,
	tick_input,
};

struct SdlInputMapperError {
	SdlInputMapperErrorCode code =
		SdlInputMapperErrorCode::invalid_bindings;
	std::optional<app::TickInputError> tick_input{};
	SDL_JoystickID gamepad_id = 0U;
};

struct SdlInputEventResult {
	bool handled = false;
	bool physical_event_queued = false;
};

struct SdlInputPumpResult {
	std::size_t events_consumed = 0U;
	std::size_t physical_events_queued = 0U;
	// The exact polled event is retained inside the mapper when the app input
	// queue is full. Drain due scheduler history, then call pump_events() again
	// before polling or presenting another host event.
	bool queue_blocked = false;
};

// Fixed-storage SDL edge mapper. If TickInputScheduler rejects an emitted
// snapshot, the mapper commits no corresponding held/device mutation; callers
// can drain the scheduler and retry that same SDL_Event before polling another.
class SdlInputMapper final {
public:
	[[nodiscard]] static std::expected<
		SdlInputMapper,
		SdlInputMapperError
	> create(
		SdlInputBindings bindings,
		SDL_WindowID main_window,
		SdlGamepadBackend gamepad_backend
	) noexcept;

	~SdlInputMapper();
	SdlInputMapper(const SdlInputMapper &) = delete;
	SdlInputMapper &operator=(const SdlInputMapper &) = delete;
	SdlInputMapper(SdlInputMapper &&other) noexcept;
	SdlInputMapper &operator=(SdlInputMapper &&) = delete;

	[[nodiscard]] std::expected<
		SdlInputEventResult,
		SdlInputMapperError
	> handle_event(
		const SDL_Event &event,
		app::TickInputScheduler &scheduler
	) noexcept;
	[[nodiscard]] std::expected<
		SdlInputPumpResult,
		SdlInputMapperError
	> pump_events(
		app::TickInputScheduler &scheduler,
		bool escape_requests_quit
	) noexcept;

	[[nodiscard]] std::size_t connected_gamepad_count() const noexcept;
	[[nodiscard]] app::NewClassicControllerCatalog controller_catalog()
		const noexcept;
	[[nodiscard]] std::optional<std::uint8_t> source_for(
		SDL_JoystickID gamepad_id
	) const noexcept;
	[[nodiscard]] bool keyboard_scancode_held(
		SDL_Scancode scancode
	) const noexcept;
	[[nodiscard]] bool any_keyboard_scancode_held() const noexcept;
	[[nodiscard]] std::optional<SDL_Scancode>
	take_keyboard_scancode_press() noexcept;
	// Product menus replace keyboard policy only between fresh schedulers and
	// after a complete release. Connected controller slots remain assigned.
	[[nodiscard]] std::expected<void, SdlInputMapperError> replace_bindings(
		SdlInputBindings bindings
	) noexcept;
	[[nodiscard]] std::expected<void, SdlInputMapperError>
	reset_after_modal(
		app::TickInputScheduler &scheduler,
		app::TickTimestampNs timestamp_ns
	) noexcept;

private:
	struct KeyboardState {
		std::array<bool, sdl_keyboard_binding_capacity> held{};
		app::PhysicalInputSourceState contribution{};
	};

	struct GamepadState {
		SDL_Gamepad *handle = nullptr;
		SDL_JoystickID id = 0U;
		app::NewClassicControllerIdentity identity{};
		std::array<char, app::new_classic_controller_name_bytes> name{};
		SdlInputPlayer player = SdlInputPlayer::none;
		std::array<bool, sdl_gamepad_button_binding_capacity> button_held{};
		std::array<bool, sdl_gamepad_axis_binding_capacity> axis_held{};
		app::PhysicalInputSourceState contribution{};
	};

	SdlInputMapper(
		SdlInputBindings bindings,
		SDL_WindowID main_window,
		SdlGamepadBackend gamepad_backend,
		bool native_runtime_lease
	) noexcept;

	[[nodiscard]] app::PhysicalInputSourceState keyboard_contribution(
		const std::array<bool, sdl_keyboard_binding_capacity> &held
	) const noexcept;
	[[nodiscard]] app::PhysicalInputSourceState gamepad_contribution(
		std::size_t slot,
		const std::array<bool, sdl_gamepad_button_binding_capacity> &buttons,
		const std::array<bool, sdl_gamepad_axis_binding_capacity> &axes
	) const noexcept;
	[[nodiscard]] SdlInputPlayer assigned_player(
		std::size_t slot,
		const app::NewClassicControllerIdentity &identity
	) const noexcept;
	[[nodiscard]] std::expected<void, SdlInputMapperError> push_event(
		app::TickInputScheduler &scheduler,
		app::PhysicalInputEvent event
	) const noexcept;
	void clear_held_states() noexcept;
	void close_connected_gamepads() noexcept;

	SdlInputBindings bindings_{};
	SDL_WindowID main_window_ = 0U;
	SdlGamepadBackend gamepad_backend_{};
	KeyboardState keyboard_{};
	std::array<bool, static_cast<std::size_t>(SDL_SCANCODE_COUNT)>
		raw_keyboard_held_{};
	std::optional<SDL_Scancode> last_scancode_press_{};
	std::array<GamepadState, sdl_gamepad_capacity> gamepads_{};
	std::size_t connected_gamepad_count_ = 0U;
	bool native_runtime_lease_ = false;
	SDL_Event pending_event_{};
	bool has_pending_event_ = false;
};

} // namespace th03::platform

#endif
