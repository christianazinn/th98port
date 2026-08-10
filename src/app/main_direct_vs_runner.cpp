#include "th03/app/main_direct_vs_runner.hpp"

#include "main_direct_vs_runner_internal.hpp"

#include <utility>

namespace th03::app {
namespace {

MainDirectVsRunnerError runner_error(
	MainDirectVsRunnerErrorCode code
) noexcept
{
	return MainDirectVsRunnerError{.code = code};
}

MainDirectVsRunnerError process_error(MainProcessError error) noexcept
{
	return MainDirectVsRunnerError{
		.code = MainDirectVsRunnerErrorCode::main_process_failed,
		.main_process_error = std::move(error),
	};
}

std::expected<StepResult, MainProcessError> process_step(
	void *context,
	CoreInput input,
	InputContext input_context
) noexcept
{
	return static_cast<MainProcess *>(context)->step(input, input_context);
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
	void *context
) noexcept
{
	return static_cast<MainProcess *>(context)->session_transition();
}

bool valid_operations(const MainDirectVsRunnerOperations &operations) noexcept
{
	return operations.context != nullptr && operations.step != nullptr &&
		operations.start_next_round != nullptr &&
		operations.finish_main_process != nullptr &&
		operations.session_transition != nullptr;
}

} // namespace

struct MainDirectVsRunner::Impl {
	std::unique_ptr<MainProcess> process{};
	MainDirectVsRunnerOperations operations{};
	MainDirectVsRunnerStatus status = MainDirectVsRunnerStatus::running;
	InputContext input_context = InputContext::gameplay;
	std::uint8_t warning_samples_remaining = 0U;
	std::optional<SessionTransition> transition{};
	std::optional<MainDirectVsRunnerError> failure{};
	std::uint64_t expected_tick_index = 0U;
	std::optional<TickTimestampNs> last_deadline{};

	Impl(
		std::unique_ptr<MainProcess> process_,
		MainDirectVsRunnerOperations operations_
	) noexcept :
		process(std::move(process_)),
		operations(operations_)
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
	auto impl = std::make_unique<Impl>(
		std::move(process),
		MainDirectVsRunnerOperations{
			.context = context,
			.step = &process_step,
			.start_next_round = &process_start_next_round,
			.finish_main_process = &process_finish_main,
			.session_transition = &process_transition,
		}
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

	const auto context_before = impl_->input_context;
	auto stepped = impl_->operations.step(
		impl_->operations.context,
		sample.input,
		context_before
	);
	if(!stepped) {
		impl_->failure = process_error(std::move(stepped.error()));
		impl_->status = MainDirectVsRunnerStatus::failed;
		return std::unexpected(*impl_->failure);
	}
	consumed.surface_changed = true;
	consumed.process_advanced = true;
	consumed.step_result = *stepped;

	if(context_before == InputContext::interstitial) {
		const bool final_sample = impl_->warning_samples_remaining == 1U;
		const bool valid_intermediate = !final_sample &&
			stepped->status == StepStatus::running;
		const bool valid_final = final_sample &&
			(stepped->status == StepStatus::phase_transition ||
			 stepped->status == StepStatus::round_transition ||
			 stepped->status == StepStatus::terminal);
		if(impl_->warning_samples_remaining == 0U ||
			(!valid_intermediate && !valid_final)) {
			impl_->failure = MainDirectVsRunnerError{
				.code = MainDirectVsRunnerErrorCode::unexpected_step_status,
				.step_status = stepped->status,
			};
			impl_->status = MainDirectVsRunnerStatus::failed;
			return std::unexpected(*impl_->failure);
		}
		impl_->warning_samples_remaining--;
	}

	switch(stepped->status) {
	case StepStatus::running:
		break;
	case StepStatus::phase_transition:
		if(context_before == InputContext::gameplay) {
			impl_->input_context = InputContext::interstitial;
			impl_->warning_samples_remaining = 27U;
		} else {
			impl_->input_context = InputContext::gameplay;
		}
		break;
	case StepStatus::round_transition: {
		auto repeated = impl_->operations.start_next_round(
			impl_->operations.context
		);
		if(!repeated) {
			impl_->failure = process_error(std::move(repeated.error()));
			impl_->status = MainDirectVsRunnerStatus::failed;
			return std::unexpected(*impl_->failure);
		}
		impl_->input_context = InputContext::gameplay;
		consumed.round_started = true;
		break;
	}
	case StepStatus::terminal: {
		auto finished = impl_->operations.finish_main_process(
			impl_->operations.context
		);
		if(!finished) {
			impl_->failure = process_error(std::move(finished.error()));
			impl_->status = MainDirectVsRunnerStatus::failed;
			return std::unexpected(*impl_->failure);
		}
		auto transition = impl_->operations.session_transition(
			impl_->operations.context
		);
		if(!transition) {
			impl_->failure = process_error(std::move(transition.error()));
			impl_->status = MainDirectVsRunnerStatus::failed;
			return std::unexpected(*impl_->failure);
		}
		if(transition->action != SessionAction::exit_to_operator) {
			impl_->failure = MainDirectVsRunnerError{
				.code = MainDirectVsRunnerErrorCode::unexpected_session_action,
				.session_action = transition->action,
			};
			impl_->status = MainDirectVsRunnerStatus::failed;
			return std::unexpected(*impl_->failure);
		}
		impl_->transition = std::move(*transition);
		impl_->status = MainDirectVsRunnerStatus::session_complete;
		break;
	}
	case StepStatus::invalid_input_context:
	case StepStatus::simulation_not_implemented:
		impl_->failure = MainDirectVsRunnerError{
			.code = MainDirectVsRunnerErrorCode::unexpected_step_status,
			.step_status = stepped->status,
		};
		impl_->status = MainDirectVsRunnerStatus::failed;
		return std::unexpected(*impl_->failure);
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
	return impl_->input_context;
}

std::uint8_t MainDirectVsRunner::warning_samples_remaining() const noexcept
{
	return impl_->warning_samples_remaining;
}

const std::optional<SessionTransition> &MainDirectVsRunner::transition()
	const noexcept
{
	return impl_->transition;
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
	if(!valid_operations(operations)) {
		return std::unexpected(runner_error(
			MainDirectVsRunnerErrorCode::invalid_process
		));
	}
	auto impl = std::make_unique<MainDirectVsRunner::Impl>(
		nullptr,
		operations
	);
	return std::unique_ptr<MainDirectVsRunner>(
		new MainDirectVsRunner(std::move(impl))
	);
}

} // namespace th03::app
