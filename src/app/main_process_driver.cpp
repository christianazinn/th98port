#include "main_process_driver_internal.hpp"

#include <optional>
#include <utility>

namespace th03::app {
namespace {

[[nodiscard]] MainProcessDriverError driver_error(
	MainProcessDriverErrorCode code
) noexcept
{
	return MainProcessDriverError{.code = code};
}

[[nodiscard]] MainProcessDriverError process_error(
	MainProcessError error
) noexcept
{
	return MainProcessDriverError{
		.code = MainProcessDriverErrorCode::process_failed,
		.process_error = std::move(error),
	};
}

[[nodiscard]] bool valid_operations(
	const MainProcessDriverOperations &operations
) noexcept
{
	const bool deferred_pair =
		(operations.deferred_frame_pending == nullptr) ==
		(operations.finish_deferred_frame == nullptr);
	return deferred_pair && operations.context != nullptr &&
		operations.step != nullptr &&
		operations.start_next_round != nullptr &&
		operations.finish_main_process != nullptr &&
		operations.session_transition != nullptr;
}

} // namespace

std::expected<std::unique_ptr<MainProcessDriver>, MainProcessDriverError>
MainProcessDriver::create(MainProcessDriverOperations operations)
{
	if(!valid_operations(operations)) {
		return std::unexpected(driver_error(
			MainProcessDriverErrorCode::invalid_operations
		));
	}
	return std::unique_ptr<MainProcessDriver>(
		new MainProcessDriver(operations)
	);
}

MainProcessDriver::MainProcessDriver(
	MainProcessDriverOperations operations
) noexcept :
	operations_(operations)
{}

void MainProcessDriver::fail(MainProcessDriverError error) noexcept
{
	failure_ = std::move(error);
	status_ = MainProcessDriverStatus::failed;
}

std::expected<MainProcessDriverTick, MainProcessDriverError>
MainProcessDriver::consume(CoreInput input) noexcept
{
	if(failure_) {
		return std::unexpected(*failure_);
	}
	if(status_ != MainProcessDriverStatus::running) {
		return std::unexpected(driver_error(
			MainProcessDriverErrorCode::not_running
		));
	}
	if(hold_samples_remaining_ != 0U) {
		--hold_samples_remaining_;
		return MainProcessDriverTick{};
	}

	const auto context_before = input_context_;
	auto stepped = operations_.step(
		operations_.context, input, context_before
	);
	if(!stepped) {
		fail(process_error(std::move(stepped.error())));
		return std::unexpected(*failure_);
	}

	MainProcessDriverTick result{
		.step_result = *stepped,
		.surface_changed = stepped->status != StepStatus::paused,
		.process_advanced = true,
	};
	const bool deferred = operations_.deferred_frame_pending &&
		operations_.deferred_frame_pending(operations_.context);
	if(deferred && stepped->status != StepStatus::phase_transition) {
		fail(MainProcessDriverError{
			.code = MainProcessDriverErrorCode::unexpected_step_status,
			.step_status = stepped->status,
		});
		return std::unexpected(*failure_);
	}
	if(deferred) {
		result.surface_changed = false;
	}
	const bool completed_paused_warning =
		context_before == InputContext::pause && pause_return_context_ &&
		*pause_return_context_ == InputContext::interstitial &&
		stepped->status != StepStatus::paused;
	if(!deferred && ((context_before == InputContext::interstitial &&
		stepped->status != StepStatus::paused) || completed_paused_warning)) {
		const bool final_sample = warning_samples_remaining_ == 1U;
		const bool valid_intermediate = !final_sample &&
			stepped->status == StepStatus::running;
		const bool valid_final = final_sample &&
			(stepped->status == StepStatus::phase_transition ||
			 stepped->status == StepStatus::round_transition ||
			 stepped->status == StepStatus::terminal);
		if(warning_samples_remaining_ == 0U ||
			(!valid_intermediate && !valid_final)) {
			fail(MainProcessDriverError{
				.code = MainProcessDriverErrorCode::unexpected_step_status,
				.step_status = stepped->status,
			});
			return std::unexpected(*failure_);
		}
		--warning_samples_remaining_;
	}

	switch(stepped->status) {
	case StepStatus::running:
		if(context_before == InputContext::pause) {
			if(!pause_return_context_) {
				fail(MainProcessDriverError{
					.code = MainProcessDriverErrorCode::unexpected_step_status,
					.step_status = stepped->status,
				});
				return std::unexpected(*failure_);
			}
			input_context_ = *pause_return_context_;
			pause_return_context_.reset();
		}
		break;
	case StepStatus::phase_transition:
		if(deferred) {
			pause_return_context_ = context_before;
			input_context_ = InputContext::pause;
		} else if(context_before == InputContext::pause) {
			if(!pause_return_context_) {
				fail(MainProcessDriverError{
					.code = MainProcessDriverErrorCode::unexpected_step_status,
					.step_status = stepped->status,
				});
				return std::unexpected(*failure_);
			}
			if(*pause_return_context_ == InputContext::gameplay) {
				input_context_ = InputContext::interstitial;
				warning_samples_remaining_ = 27U;
			} else {
				input_context_ = InputContext::gameplay;
			}
			pause_return_context_.reset();
		} else if(context_before == InputContext::gameplay) {
			input_context_ = InputContext::interstitial;
			warning_samples_remaining_ = 27U;
		} else {
			input_context_ = InputContext::gameplay;
		}
		break;
	case StepStatus::paused:
		if(context_before != InputContext::pause) {
			pause_return_context_ = context_before;
		} else if(!pause_return_context_) {
			fail(MainProcessDriverError{
				.code = MainProcessDriverErrorCode::unexpected_step_status,
				.step_status = stepped->status,
			});
			return std::unexpected(*failure_);
		}
		input_context_ = InputContext::pause;
		break;
	case StepStatus::round_transition: {
		pause_return_context_.reset();
		auto repeated = operations_.start_next_round(operations_.context);
		if(!repeated) {
			fail(process_error(std::move(repeated.error())));
			return std::unexpected(*failure_);
		}
		input_context_ = InputContext::gameplay;
		result.round_started = true;
		break;
	}
	case StepStatus::terminal: {
		pause_return_context_.reset();
		auto finished = operations_.finish_main_process(operations_.context);
		if(!finished) {
			fail(process_error(std::move(finished.error())));
			return std::unexpected(*failure_);
		}
		auto transition = operations_.session_transition(
			operations_.context, std::nullopt
		);
		if(!transition) {
			fail(process_error(std::move(transition.error())));
			return std::unexpected(*failure_);
		}
		transition_ = std::move(*transition);
		status_ = MainProcessDriverStatus::terminal;
		result.terminal = true;
		break;
	}
	case StepStatus::invalid_input_context:
	case StepStatus::simulation_not_implemented:
		fail(MainProcessDriverError{
			.code = MainProcessDriverErrorCode::unexpected_step_status,
			.step_status = stepped->status,
		});
		return std::unexpected(*failure_);
	}
	if(status_ == MainProcessDriverStatus::running &&
		stepped->next_step_vsyncs > 1U) {
		hold_samples_remaining_ = static_cast<std::uint8_t>(
			stepped->next_step_vsyncs - 1U
		);
	}

	return result;
}

std::expected<MainProcessDriverTick, MainProcessDriverError>
MainProcessDriver::finish_deferred_frame(DeferredFrameAction action) noexcept
{
	if(failure_) {
		return std::unexpected(*failure_);
	}
	if(status_ != MainProcessDriverStatus::running ||
		!operations_.deferred_frame_pending ||
		!operations_.finish_deferred_frame || !pause_return_context_ ||
		input_context_ != InputContext::pause ||
		!operations_.deferred_frame_pending(operations_.context)) {
		return std::unexpected(driver_error(
			MainProcessDriverErrorCode::not_running
		));
	}
	const auto return_context = *pause_return_context_;
	auto stepped = operations_.finish_deferred_frame(
		operations_.context, action
	);
	if(!stepped) {
		fail(process_error(std::move(stepped.error())));
		return std::unexpected(*failure_);
	}
	if(operations_.deferred_frame_pending(operations_.context)) {
		fail(MainProcessDriverError{
			.code = MainProcessDriverErrorCode::unexpected_step_status,
			.step_status = stepped->status,
		});
		return std::unexpected(*failure_);
	}
	pause_return_context_.reset();
	MainProcessDriverTick result{
		.step_result = *stepped,
		.surface_changed = true,
		.process_advanced = true,
	};
	switch(stepped->status) {
	case StepStatus::running:
		input_context_ = return_context;
		break;
	case StepStatus::phase_transition:
		if(return_context == InputContext::gameplay) {
			input_context_ = InputContext::interstitial;
			warning_samples_remaining_ = 27U;
		} else {
			input_context_ = InputContext::gameplay;
		}
		break;
	case StepStatus::round_transition: {
		auto repeated = operations_.start_next_round(operations_.context);
		if(!repeated) {
			fail(process_error(std::move(repeated.error())));
			return std::unexpected(*failure_);
		}
		input_context_ = InputContext::gameplay;
		result.round_started = true;
		break;
	}
	case StepStatus::terminal: {
		auto finished = operations_.finish_main_process(operations_.context);
		if(!finished) {
			fail(process_error(std::move(finished.error())));
			return std::unexpected(*failure_);
		}
		auto transition = operations_.session_transition(
			operations_.context, std::nullopt
		);
		if(!transition) {
			fail(process_error(std::move(transition.error())));
			return std::unexpected(*failure_);
		}
		transition_ = std::move(*transition);
		status_ = MainProcessDriverStatus::terminal;
		result.terminal = true;
		break;
	}
	case StepStatus::paused:
	case StepStatus::invalid_input_context:
	case StepStatus::simulation_not_implemented:
		fail(MainProcessDriverError{
			.code = MainProcessDriverErrorCode::unexpected_step_status,
			.step_status = stepped->status,
		});
		return std::unexpected(*failure_);
	}
	if(status_ == MainProcessDriverStatus::running &&
		stepped->next_step_vsyncs > 1U) {
		hold_samples_remaining_ = static_cast<std::uint8_t>(
			stepped->next_step_vsyncs - 1U
		);
	}
	return result;
}

std::expected<SessionTransition, MainProcessDriverError>
MainProcessDriver::resolve_continue(ContinueChoice choice) noexcept
{
	if(failure_) {
		return std::unexpected(*failure_);
	}
	if(status_ != MainProcessDriverStatus::terminal || !transition_ ||
		transition_->action != SessionAction::request_continue) {
		return std::unexpected(driver_error(
			MainProcessDriverErrorCode::continue_not_pending
		));
	}
	if(continue_transition_) {
		return std::unexpected(driver_error(
			MainProcessDriverErrorCode::continue_already_resolved
		));
	}
	auto transition = operations_.session_transition(
		operations_.context, choice
	);
	if(!transition) {
		fail(process_error(std::move(transition.error())));
		return std::unexpected(*failure_);
	}
	continue_transition_ = std::move(*transition);
	return *continue_transition_;
}

MainProcessDriverStatus MainProcessDriver::status() const noexcept
{
	return status_;
}

InputContext MainProcessDriver::input_context() const noexcept
{
	return input_context_;
}

bool MainProcessDriver::deferred_frame_pending() const noexcept
{
	return status_ == MainProcessDriverStatus::running &&
		operations_.deferred_frame_pending && pause_return_context_ &&
		input_context_ == InputContext::pause &&
		operations_.deferred_frame_pending(operations_.context);
}

std::uint8_t MainProcessDriver::warning_samples_remaining() const noexcept
{
	return warning_samples_remaining_;
}

const std::optional<SessionTransition> &MainProcessDriver::transition()
	const noexcept
{
	return transition_;
}

} // namespace th03::app
