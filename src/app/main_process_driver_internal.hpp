#ifndef TH03_APP_MAIN_PROCESS_DRIVER_INTERNAL_HPP
#define TH03_APP_MAIN_PROCESS_DRIVER_INTERNAL_HPP

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>

#include "th03/app/main_process.hpp"

namespace th03::app {

struct MainProcessDriverOperations final {
	void *context = nullptr;
	std::expected<StepResult, MainProcessError> (*step)(
		void *, CoreInput, InputContext
	) noexcept = nullptr;
	bool (*deferred_frame_pending)(void *) noexcept = nullptr;
	std::expected<StepResult, MainProcessError> (*finish_deferred_frame)(
		void *, DeferredFrameAction
	) noexcept = nullptr;
	std::expected<void, MainProcessError> (*start_next_round)(void *) noexcept =
		nullptr;
	std::expected<void, MainProcessError> (*finish_main_process)(void *) noexcept =
		nullptr;
	std::expected<SessionTransition, MainProcessError> (*session_transition)(
		void *, std::optional<ContinueChoice>
	) noexcept = nullptr;
};

enum class MainProcessDriverStatus : std::uint8_t {
	running,
	terminal,
	failed,
};

enum class MainProcessDriverErrorCode : std::uint8_t {
	invalid_operations,
	not_running,
	unexpected_step_status,
	process_failed,
	continue_not_pending,
	continue_already_resolved,
};

struct MainProcessDriverError final {
	MainProcessDriverErrorCode code =
		MainProcessDriverErrorCode::invalid_operations;
	std::optional<MainProcessError> process_error{};
	StepStatus step_status = StepStatus::running;
};

struct MainProcessDriverTick final {
	StepResult step_result{};
	bool surface_changed = false;
	bool process_advanced = false;
	bool round_started = false;
	bool terminal = false;
};

// One neutral owner for MAIN's warning/repeat/terminal lifecycle. Direct VS
// applies its exit-only policy after this driver returns; Classic retains the
// same context and may resolve one pending Continue through the cached terminal
// capability. The operations context must outlive the driver.
class MainProcessDriver final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainProcessDriver>,
		MainProcessDriverError
	> create(MainProcessDriverOperations operations);

	MainProcessDriver(const MainProcessDriver &) = delete;
	MainProcessDriver &operator=(const MainProcessDriver &) = delete;
	MainProcessDriver(MainProcessDriver &&) = delete;
	MainProcessDriver &operator=(MainProcessDriver &&) = delete;
	~MainProcessDriver() = default;

	[[nodiscard]] std::expected<
		MainProcessDriverTick,
		MainProcessDriverError
	> consume(CoreInput input) noexcept;
	[[nodiscard]] std::expected<
		MainProcessDriverTick,
		MainProcessDriverError
	> finish_deferred_frame(DeferredFrameAction action) noexcept;
	[[nodiscard]] std::expected<
		SessionTransition,
		MainProcessDriverError
	> resolve_continue(ContinueChoice choice) noexcept;

	[[nodiscard]] MainProcessDriverStatus status() const noexcept;
	[[nodiscard]] InputContext input_context() const noexcept;
	[[nodiscard]] bool deferred_frame_pending() const noexcept;
	[[nodiscard]] std::uint8_t warning_samples_remaining() const noexcept;
	[[nodiscard]] const std::optional<SessionTransition> &transition()
		const noexcept;

private:
	explicit MainProcessDriver(
		MainProcessDriverOperations operations
	) noexcept;
	void fail(MainProcessDriverError error) noexcept;

	MainProcessDriverOperations operations_{};
	MainProcessDriverStatus status_ = MainProcessDriverStatus::running;
	InputContext input_context_ = InputContext::gameplay;
	std::uint8_t warning_samples_remaining_ = 0U;
	std::uint8_t hold_samples_remaining_ = 0U;
	std::optional<InputContext> pause_return_context_{};
	std::optional<SessionTransition> transition_{};
	std::optional<SessionTransition> continue_transition_{};
	std::optional<MainProcessDriverError> failure_{};
};

} // namespace th03::app

#endif
