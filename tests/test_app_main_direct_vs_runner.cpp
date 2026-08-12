#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <optional>

#include "th03/app/main_direct_vs_runner.hpp"

#include "main_direct_vs_runner_internal.hpp"
#include "main_process_driver_internal.hpp"

namespace {

using th03::app::LogicalTickInput;
using th03::app::MainDirectVsRunner;
using th03::app::MainDirectVsRunnerErrorCode;
using th03::app::MainDirectVsRunnerOperations;
using th03::app::MainDirectVsRunnerStatus;
using th03::app::MainDirectVsRunnerTestAccess;
using th03::app::MainProcessDriver;
using th03::app::MainProcessDriverErrorCode;
using th03::app::MainProcessDriverOperations;
using th03::app::MainProcessDriverStatus;

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

struct FakeProcess {
	static constexpr std::size_t capacity = 128U;
	std::array<th03::StepStatus, capacity> results{};
	std::array<std::uint8_t, capacity> result_vsyncs{};
	std::size_t result_count = 0U;
	std::size_t result_cursor = 0U;
	std::array<th03::CoreInput, capacity> inputs{};
	std::array<th03::InputContext, capacity> contexts{};
	std::size_t step_count = 0U;
	std::size_t repeat_count = 0U;
	std::size_t finish_count = 0U;
	std::size_t transition_count = 0U;
	bool fail_step = false;
	bool fail_repeat = false;
	bool fail_finish = false;
	bool fail_transition = false;
	bool deferred_pending = false;
	std::size_t deferred_finish_count = 0U;
	th03::StepStatus deferred_result = th03::StepStatus::running;
	std::optional<th03::DeferredFrameAction> deferred_action{};
	th03::SessionAction transition_action = th03::SessionAction::exit_to_operator;
	std::optional<th03::ContinueChoice> last_continue_choice{};
};

th03::app::MainProcessError fake_process_error()
{
	return th03::app::MainProcessError{
		.code = th03::app::MainProcessErrorCode::core_operation_failed,
	};
}

std::expected<th03::StepResult, th03::app::MainProcessError> fake_step(
	void *context,
	th03::CoreInput input,
	th03::InputContext input_context
) noexcept
{
	auto &fake = *static_cast<FakeProcess *>(context);
	if(fake.fail_step || fake.step_count >= fake.inputs.size() ||
		fake.result_cursor >= fake.result_count) {
		return std::unexpected(fake_process_error());
	}
	fake.inputs[fake.step_count] = input;
	fake.contexts[fake.step_count] = input_context;
	fake.step_count++;
	const auto result_index = fake.result_cursor++;
	auto result = th03::StepResult{fake.results[result_index]};
	if(fake.result_vsyncs[result_index] != 0U) {
		result.next_step_vsyncs = fake.result_vsyncs[result_index];
	}
	return result;
}

std::expected<void, th03::app::MainProcessError> fake_repeat(
	void *context
) noexcept
{
	auto &fake = *static_cast<FakeProcess *>(context);
	fake.repeat_count++;
	if(fake.fail_repeat) {
		return std::unexpected(fake_process_error());
	}
	return {};
}

std::expected<void, th03::app::MainProcessError> fake_finish(
	void *context
) noexcept
{
	auto &fake = *static_cast<FakeProcess *>(context);
	fake.finish_count++;
	if(fake.fail_finish) {
		return std::unexpected(fake_process_error());
	}
	return {};
}

std::expected<th03::SessionTransition, th03::app::MainProcessError>
fake_transition(
	void *context,
	std::optional<th03::ContinueChoice> choice
) noexcept
{
	auto &fake = *static_cast<FakeProcess *>(context);
	fake.transition_count++;
	fake.last_continue_choice = choice;
	if(fake.fail_transition) {
		return std::unexpected(fake_process_error());
	}
	th03::SessionTransition transition{};
	transition.action = choice ? ((*choice == th03::ContinueChoice::accept) ?
		th03::SessionAction::start_gameplay :
		th03::SessionAction::exit_to_operator) : fake.transition_action;
	return transition;
}

MainDirectVsRunnerOperations operations(FakeProcess &fake) noexcept
{
	return MainDirectVsRunnerOperations{
		.context = &fake,
		.step = &fake_step,
		.start_next_round = &fake_repeat,
		.finish_main_process = &fake_finish,
		.session_transition = &fake_transition,
	};
}

MainProcessDriverOperations driver_operations(FakeProcess &fake) noexcept
{
	const auto runner = operations(fake);
	return {
		.context = runner.context,
		.step = runner.step,
		.start_next_round = runner.start_next_round,
		.finish_main_process = runner.finish_main_process,
		.session_transition = runner.session_transition,
	};
}

bool fake_deferred_pending(void *context) noexcept
{
	return static_cast<FakeProcess *>(context)->deferred_pending;
}

std::expected<th03::StepResult, th03::app::MainProcessError>
fake_finish_deferred(
	void *context,
	th03::DeferredFrameAction action
) noexcept
{
	auto &fake = *static_cast<FakeProcess *>(context);
	if(!fake.deferred_pending) {
		return std::unexpected(fake_process_error());
	}
	fake.deferred_pending = false;
	fake.deferred_action = action;
	++fake.deferred_finish_count;
	return th03::StepResult{fake.deferred_result};
}

MainProcessDriverOperations deferred_driver_operations(
	FakeProcess &fake
) noexcept
{
	auto result = driver_operations(fake);
	result.deferred_frame_pending = &fake_deferred_pending;
	result.finish_deferred_frame = &fake_finish_deferred;
	return result;
}

LogicalTickInput sample(
	std::uint64_t index,
	std::uint64_t deadline,
	th03::CoreInput input = {},
	bool quit = false
) noexcept
{
	return LogicalTickInput{
		.tick_index = index,
		.deadline_ns = deadline,
		.input = input,
		.app_quit_requested = quit,
	};
}

void test_invalid_construction_and_sample_order()
{
	auto null_runner = MainDirectVsRunner::create(nullptr);
	expect(!null_runner && null_runner.error().code ==
		MainDirectVsRunnerErrorCode::invalid_process,
		"null process is rejected");
	auto invalid_fake = MainDirectVsRunnerTestAccess::create({});
	expect(!invalid_fake && invalid_fake.error().code ==
		MainDirectVsRunnerErrorCode::invalid_process,
		"incomplete test operations are rejected");

	FakeProcess fake{};
	fake.results[0] = th03::StepStatus::running;
	fake.results[1] = th03::StepStatus::running;
	fake.result_count = 2U;
	auto created = MainDirectVsRunnerTestAccess::create(operations(fake));
	expect(static_cast<bool>(created), "valid operations construct runner");
	if(!created) {
		return;
	}
	auto &runner = **created;
	auto skipped = runner.consume(sample(1U, 10U));
	expect(!skipped && skipped.error().code ==
		MainDirectVsRunnerErrorCode::invalid_sample_order &&
		fake.step_count == 0U,
		"skipped first index is rejected before process mutation");
	auto first = runner.consume(sample(0U, 10U));
	auto regressed = runner.consume(sample(1U, 9U));
	auto equal = runner.consume(sample(1U, 10U));
	expect(first && regressed.error().code ==
		MainDirectVsRunnerErrorCode::invalid_sample_order && equal &&
		fake.step_count == 2U,
		"deadline regression is atomic and equal deadlines remain legal");
}

void test_warning_exactly_twenty_seven_samples()
{
	FakeProcess fake{};
	fake.results[0] = th03::StepStatus::phase_transition;
	for(std::size_t index = 1U; index < 27U; ++index) {
		fake.results[index] = th03::StepStatus::running;
	}
	fake.results[27] = th03::StepStatus::phase_transition;
	fake.results[28] = th03::StepStatus::running;
	fake.result_count = 29U;
	auto created = MainDirectVsRunnerTestAccess::create(operations(fake));
	expect(static_cast<bool>(created), "warning runner constructs");
	if(!created) {
		return;
	}
	auto &runner = **created;
	auto opened = runner.consume(sample(0U, 0U));
	expect(opened && runner.input_context() == th03::InputContext::interstitial &&
		runner.warning_samples_remaining() == 27U,
		"gameplay phase transition arms exactly 27 warning samples");
	for(std::uint64_t index = 1U; index <= 26U; ++index) {
		auto held = runner.consume(sample(index, index));
		expect(held && runner.input_context() == th03::InputContext::interstitial &&
			runner.warning_samples_remaining() == 27U - index,
			"warning intermediate sample remains interstitial");
	}
	auto closed = runner.consume(sample(27U, 27U));
	auto next = runner.consume(sample(28U, 28U));
	expect(closed && next &&
		runner.input_context() == th03::InputContext::gameplay &&
		runner.warning_samples_remaining() == 0U &&
		fake.step_count == 29U &&
		fake.contexts[0] == th03::InputContext::gameplay,
		"sample 27 closes warning and the next scheduled tick is gameplay");
	for(std::size_t index = 1U; index <= 27U; ++index) {
		expect(fake.contexts[index] == th03::InputContext::interstitial,
			"all 27 warning samples use interstitial context");
	}
}

void test_invalid_warning_sequence_is_sticky()
{
	FakeProcess fake{};
	fake.results[0] = th03::StepStatus::phase_transition;
	fake.results[1] = th03::StepStatus::phase_transition;
	fake.result_count = 2U;
	auto created = MainDirectVsRunnerTestAccess::create(operations(fake));
	if(!created) {
		expect(false, "invalid-warning runner constructs");
		return;
	}
	auto &runner = **created;
	(void)runner.consume(sample(0U, 0U));
	const auto early = runner.consume(sample(1U, 1U));
	const auto retry = runner.consume(sample(2U, 2U));
	expect(!early && early.error().code ==
		MainDirectVsRunnerErrorCode::unexpected_step_status &&
		runner.status() == MainDirectVsRunnerStatus::failed &&
		!retry && retry.error().code == early.error().code &&
		fake.step_count == 2U,
		"early warning completion is sticky fatal and cannot retry");
}

void test_repeat_and_terminal_routing()
{
	FakeProcess fake{};
	fake.results[0] = th03::StepStatus::round_transition;
	fake.results[1] = th03::StepStatus::terminal;
	fake.result_count = 2U;
	auto created = MainDirectVsRunnerTestAccess::create(operations(fake));
	if(!created) {
		expect(false, "route runner constructs");
		return;
	}
	auto &runner = **created;
	auto repeated = runner.consume(sample(0U, 0U));
	expect(repeated && repeated->round_started && repeated->surface_changed &&
		fake.repeat_count == 1U && fake.finish_count == 0U &&
		runner.input_context() == th03::InputContext::gameplay,
		"route 1 performs synchronous repeat without another tick or finish");
	auto terminal = runner.consume(sample(1U, 1U));
	expect(terminal && terminal->status ==
		MainDirectVsRunnerStatus::session_complete &&
		fake.finish_count == 1U && fake.transition_count == 1U &&
		runner.transition() && runner.transition()->action ==
			th03::SessionAction::exit_to_operator,
		"terminal finalizes MAIN and obtains the direct-VS operator transition once");
	auto extra = runner.consume(sample(2U, 2U));
	expect(!extra && extra.error().code ==
		MainDirectVsRunnerErrorCode::runner_not_running &&
		fake.step_count == 2U && fake.finish_count == 1U,
		"finished session rejects later samples without mutation");
}

void test_terminal_from_final_warning_sample()
{
	FakeProcess fake{};
	fake.results[0] = th03::StepStatus::phase_transition;
	for(std::size_t index = 1U; index < 27U; ++index) {
		fake.results[index] = th03::StepStatus::running;
	}
	fake.results[27] = th03::StepStatus::terminal;
	fake.result_count = 28U;
	auto created = MainDirectVsRunnerTestAccess::create(operations(fake));
	if(!created) {
		expect(false, "terminal-warning runner constructs");
		return;
	}
	auto &runner = **created;
	for(std::uint64_t index = 0U; index <= 27U; ++index) {
		auto consumed = runner.consume(sample(index, index));
		expect(static_cast<bool>(consumed),
			"warning-terminal sequence consumes every scheduled sample");
	}
	expect(runner.status() == MainDirectVsRunnerStatus::session_complete &&
		fake.finish_count == 1U && fake.transition_count == 1U &&
		fake.contexts[27] == th03::InputContext::interstitial,
		"terminal result on warning sample 27 finalizes in the same sample");
}

void test_app_quit_is_not_gameplay_cancel()
{
	FakeProcess fake{};
	fake.results[0] = th03::StepStatus::running;
	fake.result_count = 1U;
	auto created = MainDirectVsRunnerTestAccess::create(operations(fake));
	if(!created) {
		expect(false, "quit runner constructs");
		return;
	}
	auto &runner = **created;
	auto quit = runner.consume(sample(
		0U, 10U, th03::CoreInput{.system = 0x1000U}, true
	));
	expect(quit && !quit->process_advanced && !quit->surface_changed &&
		quit->status == MainDirectVsRunnerStatus::quit_requested &&
		fake.step_count == 0U && fake.finish_count == 0U &&
		fake.transition_count == 0U,
		"app quit stops before core mutation and never forges a terminal route");
}

void test_defeat_cadence_holds_every_second_vsync()
{
	FakeProcess fake{};
	for(std::size_t index = 0U; index < 50U; ++index) {
		fake.results[index] = th03::StepStatus::running;
	}
	for(std::size_t index = 1U; index <= 48U; ++index) {
		fake.result_vsyncs[index] = 2U;
	}
	fake.result_count = 50U;
	auto created = MainDirectVsRunnerTestAccess::create(operations(fake));
	expect(created.has_value(), "defeat-cadence runner constructs");
	if(!created) {
		return;
	}
	auto &runner = **created;
	std::size_t advances = 0U;
	std::size_t holds = 0U;
	for(std::uint64_t index = 0U; index <= 96U; ++index) {
		const auto transient = (index == 2U) ?
			th03::CoreInput{.p1 = 0x0020U} : th03::CoreInput{};
		const auto consumed = runner.consume(sample(index, index, transient));
		const bool expected_advance = index == 0U || (index & 1U) != 0U;
		expect(
			consumed && consumed->process_advanced == expected_advance &&
				consumed->surface_changed == expected_advance &&
				consumed->step_result.has_value() == expected_advance,
			"fatal frame advances once, then defeat updates alternate with holds"
		);
		if(consumed && consumed->process_advanced) {
			++advances;
		} else {
			++holds;
		}
	}
	expect(
		advances == 49U && holds == 48U && fake.step_count == 49U,
		"fatal publication through banner consumes 49 MAIN advances in 97 VSyncs"
	);
	expect(
		fake.inputs[2U].p1 == 0U,
		"input that exists only during a defeat hold is not sampled"
	);
	const auto ordinary = runner.consume(sample(97U, 97U));
	expect(
		ordinary && ordinary->process_advanced && ordinary->surface_changed &&
			ordinary->step_result && fake.step_count == 50U,
		"ordinary one-VSync cadence resumes immediately after the banner frame"
	);
}

void test_live_pause_context_and_surface_contract()
{
	FakeProcess fake{};
	fake.results[0U] = th03::StepStatus::paused;
	fake.results[1U] = th03::StepStatus::paused;
	fake.results[2U] = th03::StepStatus::paused;
	fake.results[3U] = th03::StepStatus::paused;
	fake.results[4U] = th03::StepStatus::paused;
	fake.results[5U] = th03::StepStatus::running;
	fake.result_count = 6U;
	auto created = MainProcessDriver::create(driver_operations(fake));
	expect(created.has_value(), "live-pause MAIN driver creates");
	if(!created) {
		return;
	}
	auto &driver = **created;
	const auto opened = driver.consume(
		th03::CoreInput{.system = 0x1000U}
	);
	const auto held = driver.consume(
		th03::CoreInput{.system = 0x1000U}
	);
	const auto released = driver.consume({});
	const auto resumed = driver.consume(
		th03::CoreInput{.system = 0x1000U}
	);
	const auto resume_held = driver.consume(
		th03::CoreInput{.system = 0x0020U}
	);
	const auto completed = driver.consume({});
	expect(
		opened && held && released && resumed && resume_held && completed &&
		!opened->surface_changed && !held->surface_changed &&
		!released->surface_changed && !resumed->surface_changed &&
		!resume_held->surface_changed && completed->surface_changed &&
		driver.input_context() == th03::InputContext::gameplay,
		"live pause freezes presentation until resume completes the source frame"
	);
	expect(
		fake.step_count == 6U &&
		fake.contexts[0U] == th03::InputContext::gameplay &&
		fake.contexts[1U] == th03::InputContext::pause &&
		fake.contexts[2U] == th03::InputContext::pause &&
		fake.contexts[3U] == th03::InputContext::pause &&
		fake.contexts[4U] == th03::InputContext::pause &&
		fake.contexts[5U] == th03::InputContext::pause,
		"MAIN driver routes every blocking sample through the pause context"
	);

	FakeProcess exit{};
	exit.results[0U] = th03::StepStatus::paused;
	exit.results[1U] = th03::StepStatus::paused;
	exit.results[2U] = th03::StepStatus::terminal;
	exit.result_count = 3U;
	auto exit_created = MainProcessDriver::create(driver_operations(exit));
	expect(exit_created.has_value(), "live-pause exit driver creates");
	if(!exit_created) {
		return;
	}
	auto &exit_driver = **exit_created;
	const auto exit_opened = exit_driver.consume(
		th03::CoreInput{.system = 0x1000U}
	);
	const auto exit_released = exit_driver.consume({});
	const auto exited = exit_driver.consume(
		th03::CoreInput{.system = 0x4000U}
	);
	expect(
		exit_opened && exit_released && exited && exited->terminal &&
		exit_driver.status() == MainProcessDriverStatus::terminal &&
		exit.finish_count == 1U && exit.transition_count == 1U &&
		exit.contexts[2U] == th03::InputContext::pause,
		"pause Q follows the ordinary terminal MAIN ownership path"
	);
}

void test_recorded_pause_deferred_frame_contract()
{
	FakeProcess resumed{};
	resumed.results[0U] = th03::StepStatus::phase_transition;
	resumed.result_count = 1U;
	resumed.deferred_pending = true;
	auto created = MainProcessDriver::create(
		deferred_driver_operations(resumed)
	);
	expect(created.has_value(), "recorded-pause MAIN driver creates");
	if(!created) {
		return;
	}
	auto &driver = **created;
	const auto opened = driver.consume({.system = 0x1000U});
	expect(opened && opened->process_advanced && !opened->surface_changed &&
		driver.deferred_frame_pending() &&
		driver.input_context() == th03::InputContext::pause,
		"recorded Cancel advances the prefix once and freezes before publication");
	const auto finished = driver.finish_deferred_frame(
		th03::DeferredFrameAction::resume
	);
	expect(finished && finished->process_advanced && finished->surface_changed &&
		!driver.deferred_frame_pending() &&
		driver.input_context() == th03::InputContext::gameplay &&
		resumed.deferred_finish_count == 1U && resumed.deferred_action ==
			th03::DeferredFrameAction::resume && resumed.step_count == 1U,
		"Resume completes only the retained suffix and restores gameplay input");
	expect(!driver.finish_deferred_frame(th03::DeferredFrameAction::resume),
		"a deferred suffix cannot be completed twice");

	FakeProcess exited{};
	exited.results[0U] = th03::StepStatus::phase_transition;
	exited.result_count = 1U;
	exited.deferred_pending = true;
	exited.deferred_result = th03::StepStatus::terminal;
	auto exit_created = MainProcessDriver::create(
		deferred_driver_operations(exited)
	);
	expect(exit_created.has_value(), "recorded-pause exit driver creates");
	if(!exit_created) {
		return;
	}
	auto &exit_driver = **exit_created;
	const auto exit_opened = exit_driver.consume({.system = 0x1000U});
	const auto exit_finished = exit_driver.finish_deferred_frame(
		th03::DeferredFrameAction::exit_session
	);
	expect(exit_opened && exit_finished && exit_finished->terminal &&
		exit_driver.status() == MainProcessDriverStatus::terminal &&
		exited.finish_count == 1U && exited.transition_count == 1U &&
		exited.deferred_action == th03::DeferredFrameAction::exit_session,
		"recorded pause exit completes the source frame before normal terminal ownership");
}

void test_warning_pause_returns_to_interstitial()
{
	FakeProcess fake{};
	fake.results[0U] = th03::StepStatus::phase_transition;
	fake.results[1U] = th03::StepStatus::paused;
	fake.results[2U] = th03::StepStatus::paused;
	fake.results[3U] = th03::StepStatus::paused;
	fake.results[4U] = th03::StepStatus::paused;
	fake.results[5U] = th03::StepStatus::running;
	for(std::size_t index = 6U; index < 31U; ++index) {
		fake.results[index] = th03::StepStatus::running;
	}
	fake.results[31U] = th03::StepStatus::phase_transition;
	fake.result_count = 32U;
	auto created = MainProcessDriver::create(driver_operations(fake));
	expect(created.has_value(), "warning-pause MAIN driver creates");
	if(!created) {
		return;
	}
	auto &driver = **created;
	const auto warning = driver.consume({});
	const auto paused = driver.consume(
		th03::CoreInput{.system = 0x1000U}
	);
	const auto held = driver.consume(
		th03::CoreInput{.system = 0x1000U}
	);
	const auto released = driver.consume({});
	const auto selected = driver.consume(
		th03::CoreInput{.system = 0x1000U}
	);
	const auto resumed = driver.consume({});
	expect(
		warning && paused && held && released && selected && resumed &&
		!paused->surface_changed && !held->surface_changed &&
		!released->surface_changed && !selected->surface_changed &&
		resumed->surface_changed &&
		driver.input_context() == th03::InputContext::interstitial &&
		driver.warning_samples_remaining() == 26U,
		"resumed warning pause returns to its interrupted interstitial sample"
	);
	for(std::size_t index = 0U; index < 25U; ++index) {
		const auto intermediate = driver.consume({});
		expect(
			intermediate &&
			driver.input_context() == th03::InputContext::interstitial,
			"post-pause warning intermediate sample remains interstitial"
		);
	}
	const auto closed = driver.consume({});
	expect(
		closed && driver.input_context() == th03::InputContext::gameplay &&
		driver.warning_samples_remaining() == 0U &&
		fake.contexts[1U] == th03::InputContext::interstitial &&
		fake.contexts[2U] == th03::InputContext::pause &&
		fake.contexts[5U] == th03::InputContext::pause &&
		fake.contexts[6U] == th03::InputContext::interstitial &&
		fake.contexts[31U] == th03::InputContext::interstitial,
		"warning pause preserves all 27 interstitial samples and their return context"
	);
}

void test_process_failures_and_transition_domain()
{
	FakeProcess fake{};
	fake.results[0] = th03::StepStatus::round_transition;
	fake.result_count = 1U;
	fake.fail_repeat = true;
	auto created = MainDirectVsRunnerTestAccess::create(operations(fake));
	if(!created) {
		expect(false, "failure runner constructs");
		return;
	}
	auto &runner = **created;
	auto failed = runner.consume(sample(0U, 0U));
	auto retry = runner.consume(sample(1U, 1U));
	expect(!failed && failed.error().code ==
		MainDirectVsRunnerErrorCode::main_process_failed &&
		failed.error().main_process_error &&
		runner.status() == MainDirectVsRunnerStatus::failed &&
		!retry && fake.repeat_count == 1U,
		"repeat failure is sticky because core may already have reset");

	FakeProcess wrong_transition{};
	wrong_transition.results[0] = th03::StepStatus::terminal;
	wrong_transition.result_count = 1U;
	wrong_transition.transition_action = th03::SessionAction::start_gameplay;
	auto wrong = MainDirectVsRunnerTestAccess::create(
		operations(wrong_transition)
	);
	if(!wrong) {
		expect(false, "wrong-transition runner constructs");
		return;
	}
	auto result = (*wrong)->consume(sample(0U, 0U));
	expect(!result && result.error().code ==
		MainDirectVsRunnerErrorCode::unexpected_session_action &&
		result.error().session_action == th03::SessionAction::start_gameplay,
		"direct VS rejects a non-operator terminal transition");
}

void test_scheduler_catchup_keeps_distinct_samples()
{
	auto scheduler = th03::app::TickInputScheduler::create(
		th03::app::RationalTickRate{.ticks = 100'000'000U, .seconds = 1U},
		10U
	);
	expect(static_cast<bool>(scheduler), "10ns catch-up scheduler constructs");
	if(!scheduler) {
		return;
	}
	const auto pressed = scheduler->push_event(th03::app::PhysicalInputEvent{
		.timestamp_ns = 10U,
		.kind = th03::app::PhysicalInputEventKind::source_state_changed,
		.source = 0U,
		.state = {.p1 = 0x0020U},
	});
	const auto released = scheduler->push_event(th03::app::PhysicalInputEvent{
		.timestamp_ns = 21U,
		.kind = th03::app::PhysicalInputEventKind::source_released,
		.source = 0U,
	});
	const auto quit = scheduler->push_event(th03::app::PhysicalInputEvent{
		.timestamp_ns = 31U,
		.kind = th03::app::PhysicalInputEventKind::app_quit,
	});
	expect(pressed && released && quit,
		"timestamped catch-up input history queues");

	FakeProcess fake{};
	fake.results[0] = th03::StepStatus::running;
	fake.results[1] = th03::StepStatus::running;
	fake.results[2] = th03::StepStatus::running;
	fake.result_count = 3U;
	auto created = MainDirectVsRunnerTestAccess::create(operations(fake));
	if(!created) {
		expect(false, "catch-up runner constructs");
		return;
	}
	auto &runner = **created;
	std::size_t deadlines = 0U;
	while(runner.status() == MainDirectVsRunnerStatus::running) {
		auto due = scheduler->pop_due(50U);
		if(!due || !*due) {
			break;
		}
		deadlines++;
		auto consumed = runner.consume(**due);
		if(!consumed) {
			break;
		}
	}
	expect(deadlines == 4U && fake.step_count == 3U &&
		fake.inputs[0].p1 == 0x0020U && fake.inputs[1].p1 == 0x0020U &&
		fake.inputs[2].p1 == 0U &&
		runner.status() == MainDirectVsRunnerStatus::quit_requested,
		"four overdue deadlines remain distinct and quit stops before its process sample");
}

void test_story_continue_capability()
{
	FakeProcess fake{};
	fake.results[0U] = th03::StepStatus::terminal;
	fake.result_count = 1U;
	fake.transition_action = th03::SessionAction::request_continue;
	auto created = MainProcessDriver::create(driver_operations(fake));
	expect(created.has_value(), "neutral MAIN driver creates for Story");
	if(!created) {
		return;
	}
	auto &driver = **created;
	const auto terminal = driver.consume({});
	expect(terminal && terminal->terminal &&
		driver.status() == MainProcessDriverStatus::terminal &&
		driver.transition() && driver.transition()->action ==
			th03::SessionAction::request_continue &&
		fake.finish_count == 1U && fake.transition_count == 1U &&
		!fake.last_continue_choice,
		"terminal MAIN retains the initial Continue capability");
	const auto accepted = driver.resolve_continue(
		th03::ContinueChoice::accept
	);
	expect(accepted && accepted->action ==
		th03::SessionAction::start_gameplay &&
		fake.transition_count == 2U && fake.last_continue_choice ==
			th03::ContinueChoice::accept,
		"Continue acceptance resolves through the retained process once");
	const auto repeated = driver.resolve_continue(
		th03::ContinueChoice::decline
	);
	expect(!repeated && repeated.error().code ==
		MainProcessDriverErrorCode::continue_already_resolved &&
		fake.transition_count == 2U,
		"resolved Continue capability rejects a second choice");
	const auto extra_tick = driver.consume({});
	expect(!extra_tick && extra_tick.error().code ==
		MainProcessDriverErrorCode::not_running && fake.step_count == 1U,
		"terminal driver rejects further MAIN samples");
}

} // namespace

int main()
{
	test_invalid_construction_and_sample_order();
	test_warning_exactly_twenty_seven_samples();
	test_invalid_warning_sequence_is_sticky();
	test_repeat_and_terminal_routing();
	test_terminal_from_final_warning_sample();
	test_app_quit_is_not_gameplay_cancel();
	test_defeat_cadence_holds_every_second_vsync();
	test_live_pause_context_and_surface_contract();
	test_recorded_pause_deferred_frame_contract();
	test_warning_pause_returns_to_interstitial();
	test_process_failures_and_transition_domain();
	test_scheduler_catchup_keeps_distinct_samples();
	test_story_continue_capability();

	if(failures != 0U) {
		std::cerr << failures << " of " << checks
			<< " app direct-VS runner checks failed\n";
		return 1;
	}
	std::cout << "all " << checks
		<< " app direct-VS runner checks passed\n";
	return 0;
}
