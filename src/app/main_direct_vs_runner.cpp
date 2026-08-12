#include "th03/app/main_direct_vs_runner.hpp"

#include "main_direct_vs_runner_internal.hpp"
#include "main_process_driver_internal.hpp"

#include <utility>

namespace th03::app {
namespace {

MainDirectVsRunnerError runner_error(
	MainDirectVsRunnerErrorCode code
) noexcept
{
	return MainDirectVsRunnerError{.code = code};
}

std::expected<StepResult, MainProcessError> process_step(
	void *context,
	CoreInput input,
	InputContext input_context
) noexcept
{
	return static_cast<MainProcess *>(context)->step(input, input_context);
}

bool process_deferred_pending(void *context) noexcept
{
	return static_cast<MainProcess *>(context)->deferred_frame_pending();
}

std::expected<StepResult, MainProcessError> process_finish_deferred(
	void *context,
	DeferredFrameAction action
) noexcept
{
	return static_cast<MainProcess *>(context)->finish_deferred_frame(action);
}

std::expected<void, MainProcessError> process_start_next_round(
	void *context
) noexcept
{
	return static_cast<MainProcess *>(context)->start_next_round();
}

std::expected<void, MainProcessError> process_finish_main(
	void *context
) noexcept
{
	return static_cast<MainProcess *>(context)->finish_main_process();
}

std::expected<SessionTransition, MainProcessError> process_transition(
	void *context,
	std::optional<ContinueChoice> choice
) noexcept
{
	return static_cast<MainProcess *>(context)->session_transition(choice);
}

MainProcessDriverOperations driver_operations(
	const MainDirectVsRunnerOperations &operations
) noexcept
{
	return {
		.context = operations.context,
		.step = operations.step,
		.start_next_round = operations.start_next_round,
		.finish_main_process = operations.finish_main_process,
		.session_transition = operations.session_transition,
	};
}

MainDirectVsRunnerError driver_error(MainProcessDriverError error) noexcept
{
	switch(error.code) {
	case MainProcessDriverErrorCode::process_failed:
		return MainDirectVsRunnerError{
			.code = MainDirectVsRunnerErrorCode::main_process_failed,
			.main_process_error = std::move(error.process_error),
		};
	case MainProcessDriverErrorCode::unexpected_step_status:
		return MainDirectVsRunnerError{
			.code = MainDirectVsRunnerErrorCode::unexpected_step_status,
			.step_status = error.step_status,
		};
	case MainProcessDriverErrorCode::invalid_operations:
		return runner_error(MainDirectVsRunnerErrorCode::invalid_process);
	case MainProcessDriverErrorCode::not_running:
	case MainProcessDriverErrorCode::continue_not_pending:
	case MainProcessDriverErrorCode::continue_already_resolved:
		return runner_error(MainDirectVsRunnerErrorCode::runner_not_running);
	}
	return runner_error(MainDirectVsRunnerErrorCode::runner_not_running);
}

} // namespace

struct MainDirectVsRunner::Impl {
	std::unique_ptr<MainProcess> process{};
	std::unique_ptr<MainProcessDriver> driver{};
	MainDirectVsRunnerStatus status = MainDirectVsRunnerStatus::running;
	std::optional<MainDirectVsRunnerError> failure{};
	std::uint64_t expected_tick_index = 0U;
	std::optional<TickTimestampNs> last_deadline{};

	Impl(
		std::unique_ptr<MainProcess> process_,
		std::unique_ptr<MainProcessDriver> driver_
	) noexcept :
		process(std::move(process_)),
		driver(std::move(driver_))
	{}
};

std::expected<
	std::unique_ptr<MainDirectVsRunner>,
	MainDirectVsRunnerError
> MainDirectVsRunner::create(
	std::unique_ptr<MainProcess> process
)
{
	if(!process) {
		return std::unexpected(runner_error(
			MainDirectVsRunnerErrorCode::invalid_process
		));
	}
	if(process->input_protocol() != InputProtocol::live) {
		return std::unexpected(runner_error(
			MainDirectVsRunnerErrorCode::unsupported_input_protocol
		));
	}
	auto *context = process.get();
	auto driver = MainProcessDriver::create({
		.context = context,
		.step = &process_step,
		.deferred_frame_pending = &process_deferred_pending,
		.finish_deferred_frame = &process_finish_deferred,
		.start_next_round = &process_start_next_round,
		.finish_main_process = &process_finish_main,
		.session_transition = &process_transition,
	});
	if(!driver) {
		return std::unexpected(driver_error(std::move(driver.error())));
	}
	auto impl = std::make_unique<Impl>(
		std::move(process),
		std::move(*driver)
	);
	return std::unique_ptr<MainDirectVsRunner>(
		new MainDirectVsRunner(std::move(impl))
	);
}

MainDirectVsRunner::MainDirectVsRunner(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{}

MainDirectVsRunner::~MainDirectVsRunner() = default;

std::expected<MainDirectVsConsumeResult, MainDirectVsRunnerError>
MainDirectVsRunner::consume(const LogicalTickInput &sample) noexcept
{
	if(impl_->failure) {
		return std::unexpected(*impl_->failure);
	}
	MainDirectVsConsumeResult consumed{
		.tick_index = sample.tick_index,
		.deadline_ns = sample.deadline_ns,
		.status = impl_->status,
	};
	if(impl_->status != MainDirectVsRunnerStatus::running) {
		return std::unexpected(runner_error(
			MainDirectVsRunnerErrorCode::runner_not_running
		));
	}
	if(sample.tick_index != impl_->expected_tick_index ||
		(impl_->last_deadline && sample.deadline_ns < *impl_->last_deadline)) {
		return std::unexpected(runner_error(
			MainDirectVsRunnerErrorCode::invalid_sample_order
		));
	}
	impl_->expected_tick_index++;
	impl_->last_deadline = sample.deadline_ns;
	if(sample.app_quit_requested) {
		impl_->status = MainDirectVsRunnerStatus::quit_requested;
		consumed.status = impl_->status;
		return consumed;
	}

	auto driven = impl_->driver->consume(sample.input);
	if(!driven) {
		impl_->failure = driver_error(std::move(driven.error()));
		impl_->status = MainDirectVsRunnerStatus::failed;
		return std::unexpected(*impl_->failure);
	}
	consumed.surface_changed = driven->surface_changed;
	consumed.process_advanced = driven->process_advanced;
	if(driven->process_advanced) {
		consumed.step_result = driven->step_result;
	}
	consumed.round_started = driven->round_started;

	if(driven->terminal) {
		const auto &transition = impl_->driver->transition();
		if(!transition ||
			transition->action != SessionAction::exit_to_operator) {
			impl_->failure = MainDirectVsRunnerError{
				.code = MainDirectVsRunnerErrorCode::unexpected_session_action,
				.session_action = transition ? transition->action :
					SessionAction::exit_to_operator,
			};
			impl_->status = MainDirectVsRunnerStatus::failed;
			return std::unexpected(*impl_->failure);
		}
		impl_->status = MainDirectVsRunnerStatus::session_complete;
	}

	consumed.status = impl_->status;
	return consumed;
}

MainDirectVsRunnerStatus MainDirectVsRunner::status() const noexcept
{
	return impl_->status;
}

InputContext MainDirectVsRunner::input_context() const noexcept
{
	return impl_->driver->input_context();
}

std::uint8_t MainDirectVsRunner::warning_samples_remaining() const noexcept
{
	return impl_->driver->warning_samples_remaining();
}

const std::optional<SessionTransition> &MainDirectVsRunner::transition()
	const noexcept
{
	return impl_->driver->transition();
}

const MainProcess &MainDirectVsRunner::process() const noexcept
{
	return *impl_->process;
}

std::expected<present::SourceSurface, present::SourceSurfaceError>
MainDirectVsRunner::compose_shown_surface(
	std::span<present::ComposedPixel, present::graphics_page_pixels> output
) const
{
	return impl_->process->compose_shown_surface(output);
}

std::expected<
	std::unique_ptr<MainDirectVsRunner>,
	MainDirectVsRunnerError
> MainDirectVsRunnerTestAccess::create(
	MainDirectVsRunnerOperations operations
)
{
	auto driver = MainProcessDriver::create(driver_operations(operations));
	if(!driver) {
		return std::unexpected(driver_error(std::move(driver.error())));
	}
	auto impl = std::make_unique<MainDirectVsRunner::Impl>(
		nullptr,
		std::move(*driver)
	);
	return std::unique_ptr<MainDirectVsRunner>(
		new MainDirectVsRunner(std::move(impl))
	);
}

} // namespace th03::app
