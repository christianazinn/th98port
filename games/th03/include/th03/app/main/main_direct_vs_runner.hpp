#ifndef TH03_APP_MAIN_DIRECT_VS_RUNNER_HPP
#define TH03_APP_MAIN_DIRECT_VS_RUNNER_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "th03/app/main/main_process.hpp"
#include "th03/app/runtime/tick_input.hpp"

namespace th03::app {

enum class MainDirectVsRunnerStatus : std::uint8_t {
	running,
	quit_requested,
	session_complete,
	failed,
};

enum class MainDirectVsRunnerErrorCode : std::uint8_t {
	invalid_process,
	unsupported_input_protocol,
	invalid_sample_order,
	runner_not_running,
	main_process_failed,
	unexpected_step_status,
	unexpected_session_action,
};

struct MainDirectVsRunnerError {
	MainDirectVsRunnerErrorCode code =
		MainDirectVsRunnerErrorCode::invalid_process;
	std::optional<MainProcessError> main_process_error{};
	StepStatus step_status = StepStatus::running;
	SessionAction session_action = SessionAction::exit_to_operator;
};

struct MainDirectVsConsumeResult {
	std::uint64_t tick_index = 0U;
	TickTimestampNs deadline_ns = 0U;
	std::optional<StepResult> step_result{};
	bool process_advanced = false;
	bool surface_changed = false;
	bool round_started = false;
	MainDirectVsRunnerStatus status = MainDirectVsRunnerStatus::running;
};

// Platform-neutral owner for one live direct-VS process. The host owns its
// clock and TickInputScheduler and passes every returned LogicalTickInput here
// separately. Round-repeat startup is synchronous and consumes no host tick.
class MainDirectVsRunner final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainDirectVsRunner>,
		MainDirectVsRunnerError
	> create(
		std::unique_ptr<MainProcess> process
	);

	~MainDirectVsRunner();
	MainDirectVsRunner(const MainDirectVsRunner &) = delete;
	MainDirectVsRunner &operator=(const MainDirectVsRunner &) = delete;
	MainDirectVsRunner(MainDirectVsRunner &&) = delete;
	MainDirectVsRunner &operator=(MainDirectVsRunner &&) = delete;

	[[nodiscard]] std::expected<
		MainDirectVsConsumeResult,
		MainDirectVsRunnerError
	> consume(const LogicalTickInput &sample) noexcept;

	[[nodiscard]] MainDirectVsRunnerStatus status() const noexcept;
	[[nodiscard]] InputContext input_context() const noexcept;
	[[nodiscard]] std::uint8_t warning_samples_remaining() const noexcept;
	[[nodiscard]] const std::optional<SessionTransition> &transition()
		const noexcept;
	[[nodiscard]] const MainProcess &process() const noexcept;
	[[nodiscard]] std::expected<
		present::SourceSurface,
		present::SourceSurfaceError
	> compose_shown_surface(
		std::span<
			present::ComposedPixel,
			present::graphics_page_pixels
		> output
	) const;

private:
	friend struct MainDirectVsRunnerTestAccess;
	struct Impl;
	explicit MainDirectVsRunner(std::unique_ptr<Impl> impl) noexcept;

	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif
