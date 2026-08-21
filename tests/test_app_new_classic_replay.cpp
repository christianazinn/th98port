#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <variant>

#include "th03/app/new_classic_netplay_replay_capture.hpp"
#include "th03/app/new_classic_replay.hpp"
#include "th03/core/game.hpp"
#include "th03/formats/replay.hpp"
#include "th03/formats/v13.hpp"
#include "th03/replay/session.hpp"

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const std::string &message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

th03::RoundStartObservation round_start(
	std::uint8_t mode,
	std::uint8_t stage,
	std::uint8_t round,
	std::uint32_t seed
)
{
	th03::RoundStartObservation observation{};
	observation.start.resident_rand = static_cast<std::int32_t>(seed);
	observation.start.random_seed = seed;
	observation.start.game_mode = mode;
	observation.start.rank = 2U;
	observation.start.key_mode = 1U;
	observation.start.story_stage = stage;
	observation.start.story_lives = 2U;
	observation.start.is_cpu = mode == th03::game_mode::vs_1p_2p
		? std::array<std::uint8_t, 2>{0U, 0U}
		: std::array<std::uint8_t, 2>{0U, 1U};
	observation.start.paletted_character = {1, 4};
	observation.start.story_opponents.fill(4);
	observation.start.autofire_mask = 1U;
	observation.effective_reset_seed = seed;
	observation.runtime.random_seed = seed;
	observation.runtime.formation_type_ring[0U] = 1U;
	observation.runtime.players[0U].halfhearts = 10U;
	observation.runtime.players[1U].halfhearts = 10U;
	observation.round.state.round_id = round;
	return observation;
}

th03::CanonicalState terminal(
	std::uint8_t mode,
	std::uint8_t stage,
	std::uint8_t round,
	std::uint8_t route,
	std::int8_t winner
)
{
	th03::CanonicalState state{};
	state.game_mode = mode;
	state.story_stage = stage;
	state.round_id = round;
	state.route = route;
	state.winner = winner;
	state.p1_score = {8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U};
	state.p2_score = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
	return state;
}

void test_vs_capture()
{
	constexpr auto mode = th03::game_mode::vs_1p_2p;
	th03::app::NewClassicReplayRecorder recorder;
	check(
		recorder.round_started(round_start(mode, 0U, 0U, 29U), 0x5D0CU)
			.has_value(),
		"VS capture begins at the first MAIN round, not the initial splash"
	);
	check(recorder.recording(), "the recorder becomes active at checkpoint 0");
	check(
		recorder.sample(
			th03::replay::InputPhase::gameplay,
			{.p1 = 0x20U, .p2 = 0x08U, .system = 0U, .charge_mask = 1U}
		).has_value(),
		"VS gameplay records normalized dual-player input"
	);
	check(
		recorder.round_completed(terminal(mode, 0U, 0U, 1U, 0), 120U)
			.has_value(),
		"a repeat-round terminal adds a split"
	);
	check(
		recorder.round_started(round_start(mode, 0U, 1U, 37U)).has_value(),
		"a repeat round adds a checkpoint without a process control"
	);
	check(
		recorder.sample(th03::replay::InputPhase::gameplay, {}).has_value() &&
		recorder.round_completed(terminal(mode, 0U, 1U, 2U, 1), 90U)
			.has_value() &&
		recorder.process_completed(th03::replay::ProcessId::main).has_value(),
		"the terminal MAIN sample, split, and MAIN_END retain source order"
	);
	check(
		recorder.sample(
			th03::replay::InputPhase::interstitial,
			{.system = 0x20U}
		).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::mainl).has_value(),
		"WIN input and MAINL_END are recorded after MAIN_END"
	);
	auto resident = round_start(mode, 0U, 1U, 37U).start;
	check(
		recorder.finish(th03::formats::ReplayEndReason::game_over, resident)
			.has_value(),
		"the completed VS route finalizes through the frozen V13 writer"
	);
	check(!recorder.recording(), "finalization closes the active capture");
	auto document = recorder.take_completed();
	check(document.has_value(), "finalization publishes one completed document");
	if(!document) {
		return;
	}
	check(
		document->metadata->status == th03::formats::ReplayStatus::finalized &&
		document->metadata->end_reason ==
			th03::formats::ReplayEndReason::game_over &&
		document->metadata->autofire == 1U,
		"VS metadata retains final status, reason, and Replay Patch Autofire"
	);
	check(
		document->checkpoints.size() == 2U &&
		document->checkpoints[0U].sample_index == 0U &&
		document->checkpoints[1U].sample_index == 1U &&
		document->checkpoints[1U].round == 1U,
		"VS checkpoint cursors straddle repeat-round input exactly"
	);
	check(
		document->expected_round_splits.size() == 2U &&
		document->expected_round_splits[0U].route == 1U &&
		document->expected_round_splits[1U].route == 2U &&
		document->metadata->final_winner == 1 &&
		document->metadata->final_score ==
			document->expected_round_splits[1U].p1_score &&
		document->metadata->final_score !=
			document->expected_round_splits[1U].p2_score,
		"VS summaries preserve terminal results while final_score remains the documented P1 field"
	);
	check(
		std::holds_alternative<th03::replay::ProcessBoundary>(
			document->session.records[2U]
		) &&
		std::holds_alternative<th03::replay::ProcessBoundary>(
			document->session.records.back()
		),
		"VS stream contains MAIN_END and terminal MAINL_END controls"
	);
	check(
		th03::formats::write_v13_replay(*document).has_value(),
		"the completed VS document remains serializable"
	);
	check(
		!recorder.take_completed(),
		"a completed document can only be taken once"
	);
}

void test_story_capture()
{
	constexpr auto mode = th03::game_mode::story;
	th03::app::NewClassicReplayRecorder recorder;
	check(
		recorder.round_started(round_start(mode, 0U, 0U, 41U)).has_value(),
		"Story stage 0 starts"
	);
	recorder.source_vsync();
	recorder.source_vsync();
	recorder.frame_io({.status = th03::StepStatus::running});
	recorder.classify_pending_frame(true);
	th03::MainResultState clear{};
	clear.summary_active = true;
	clear.winner = th03::PlayerId::p1;
	clear.story_stage = 0U;
	clear.story_lives = 2U;
	clear.winner_stats = {
		.combo_hits_max = 12U,
		.gauge_attacks_fired = 3U,
		.boss_attacks_fired = 2U,
		.boss_attacks_reversed = 1U,
		.boss_panics_fired = 4U,
	};
	check(
		recorder.sample(th03::replay::InputPhase::gameplay, {}).has_value() &&
		recorder.round_completed(
			terminal(mode, 0U, 0U, 2U, 0), std::nullopt, clear
		).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::main).has_value() &&
		recorder.sample(th03::replay::InputPhase::interstitial, {}).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::mainl).has_value(),
		"Story stage 0 records MAIN and MAINL as separate source processes"
	);
	check(
		recorder.round_started(round_start(mode, 1U, 0U, 53U)).has_value(),
		"Story stage 1 appends its checkpoint"
	);
	check(
		recorder.document() && recorder.document()->metadata &&
		recorder.document()->metadata->stage_reached_count == 1U,
		"Story reached-stage metadata advances only after the round route is captured"
	);
	check(
		recorder.sample(th03::replay::InputPhase::gameplay, {}).has_value() &&
		recorder.round_completed(terminal(mode, 1U, 0U, 0U, 1)).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::main).has_value() &&
		recorder.sample(th03::replay::InputPhase::interstitial, {}).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::mainl).has_value(),
		"Story stage 1 appends its terminal processes"
	);
	auto resident = round_start(mode, 1U, 0U, 53U).start;
	resident.story_lives = 1U;
	check(
		recorder.finish(th03::formats::ReplayEndReason::game_over, resident)
			.has_value(),
		"Story Game Over finalizes"
	);
	auto document = recorder.take_completed();
	check(document.has_value(), "Story publishes a completed document");
	if(!document) {
		return;
	}
	check(
		document->metadata->story.has_value() &&
		document->metadata->stage_reached_count == 2U &&
		document->metadata->story->stage_opponents[1U] == 4U &&
		document->metadata->story->stage_scores[0U] ==
			document->expected_round_splits[0U].p1_score,
		"Story summary retains opponents, reached stages, and clear scores"
	);
	check(
		document->expected_round_splits[0U].real_frames == 2U &&
		document->slowdown && document->slowdown->timed_frames == 1U &&
		document->slowdown->slow_frames == 1U,
		"live capture records source-VSync time and native missed deadlines"
	);
	const auto &bonus = (*document->stage_clear_bonuses)[0U];
	check(
		bonus.max_combo == 12U && bonus.gauge_attacks == 3U &&
		bonus.boss_attacks == 2U && bonus.boss_reversals == 1U &&
		bonus.boss_panics == 4U && bonus.remaining_lives == 0U &&
		bonus.total == std::array<std::uint8_t, 4U>{0x00U, 0x20U, 0x21U, 0x00U},
		"a P1 Story clear captures the Replay Patch bonus components and total"
	);
	check(
		document->metadata->final_story_stage == 1U &&
		document->metadata->final_story_lives == 1U &&
		document->metadata->final_route == 0U,
		"Story terminal metadata retains the last gameplay result"
	);
	check(
		th03::formats::write_v13_replay(*document).has_value(),
		"the completed Story document remains serializable"
	);
}

void test_timing_scope()
{
	constexpr auto mode = th03::game_mode::story;
	th03::app::NewClassicReplayRecorder recorder;
	check(
		recorder.round_started(round_start(mode, 0U, 0U, 91U)).has_value(),
		"timing scope starts at the first source round boundary"
	);
	for(std::size_t frame = 0U; frame < 27U; ++frame) {
		recorder.source_vsync();
	}
	recorder.frame_io({.status = th03::StepStatus::running});
	recorder.classify_pending_frame(false);
	recorder.source_vsync();
	recorder.frame_io({.status = th03::StepStatus::running});
	recorder.classify_pending_frame(true);

	// The frame opening Replay Pause contributes to real VSync time, but its
	// completion and all modal polling are outside the slowdown denominator.
	recorder.source_vsync();
	recorder.frame_io({.status = th03::StepStatus::paused}, false);
	recorder.classify_pending_frame(true);
	for(std::size_t frame = 0U; frame < 50U; ++frame) {
		recorder.source_vsync();
	}
	recorder.frame_io({.status = th03::StepStatus::running});
	recorder.classify_pending_frame(false);

	// The fatal input boundary is pre/post DF_NONE: it contributes to both
	// timing domains and does not begin the two-VSync cadence yet.
	recorder.source_vsync();
	recorder.frame_io({
		.status = th03::StepStatus::running,
	});
	recorder.classify_pending_frame(true);
	// The following update begins with DF_NONE, publishes the first explosion,
	// and is therefore the final real-time sample but not a slowdown sample.
	recorder.source_vsync();
	recorder.frame_io({
		.status = th03::StepStatus::running,
		.next_step_vsyncs = 2U,
		.defeat_none_before_frame = true,
		.defeat_none_after_frame = false,
	});
	recorder.classify_pending_frame(true);
	for(std::size_t frame = 0U; frame < 94U; ++frame) {
		recorder.source_vsync();
	}
	check(
		recorder.sample(th03::replay::InputPhase::gameplay, {}).has_value() &&
		recorder.round_completed(terminal(mode, 0U, 0U, 0U, 1)).has_value(),
		"timing scope closes after the post-fatal defeat cadence"
	);
	const auto *document = recorder.document();
	check(
		document && document->expected_round_splits.size() == 1U &&
		document->expected_round_splits[0U].real_frames == 81U &&
		document->slowdown && document->slowdown->timed_frames == 4U &&
		document->slowdown->slow_frames == 2U,
		"real time includes the first explosion while slowdown ends on the fatal frame"
	);
}

void test_invalid_sequences()
{
	th03::app::NewClassicReplayRecorder recorder;
	check(
		!recorder.sample(th03::replay::InputPhase::gameplay, {}),
		"samples before checkpoint 0 are rejected"
	);
	check(
		!recorder.process_completed(th03::replay::ProcessId::main),
		"process controls before checkpoint 0 are rejected"
	);
	auto invalid = round_start(th03::game_mode::demo, 0U, 0U, 1U);
	check(
		!recorder.round_started(invalid),
		"automatic Demo never starts a user replay recording"
	);
	check(
		!recorder.round_started(
			round_start(th03::game_mode::vs_cpu_cpu, 0U, 0U, 2U)
		),
		"CPU-vs-CPU never starts a Replay Patch recording"
	);
	check(
		recorder.round_started(
			round_start(th03::game_mode::vs_1p_cpu, 0U, 0U, 3U)
		).has_value(),
		"a valid sequence can begin after rejected input"
	);
	check(
		!recorder.round_started(
			round_start(th03::game_mode::vs_1p_cpu, 0U, 1U, 5U)
		),
		"a later checkpoint cannot precede its first sample"
	);
	check(
		!recorder.finish(th03::formats::ReplayEndReason::menu_return, {}),
		"finalization requires a terminal process control"
	);
	recorder.discard();
	check(!recorder.recording(), "discard clears an incomplete capture");
}

void test_memory_only_lifecycle()
{
	constexpr auto mode = th03::game_mode::vs_1p_cpu;
	th03::app::NewClassicReplayRecorder recorder(0x5D0CU);
	check(
		recorder.round_started(round_start(mode, 0U, 0U, 101U)).has_value(),
		"memory-only capture starts without a storage path"
	);
	check(
		recorder.sample(th03::replay::InputPhase::gameplay,
			{.p1 = 1U}).has_value(),
		"the first input is retained in memory"
	);
	check(
		recorder.document() && recorder.document()->metadata &&
		recorder.document()->metadata->status ==
			th03::formats::ReplayStatus::recording &&
		recorder.document()->metadata->end_reason ==
			th03::formats::ReplayEndReason::none &&
		recorder.document()->metadata->dos_date == 0x5D0CU &&
		recorder.document()->session.sample_count == 1U,
		"active recording metadata and samples remain directly available"
	);
	check(
		recorder.sample(th03::replay::InputPhase::gameplay,
			{.p1 = 2U}).has_value(),
		"ordinary samples append to the in-memory document"
	);
	check(
		recorder.document() &&
		recorder.document()->session.sample_count == 2U &&
		recorder.document()->session.records.size() == 2U,
		"the recorder owns the complete live input prefix"
	);
	check(
		recorder.round_completed(terminal(mode, 0U, 0U, 2U, 0)).has_value(),
		"the terminal summary precedes the in-memory MAIN_END boundary"
	);
	check(
		recorder.process_completed(th03::replay::ProcessId::main).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::main).has_value() &&
		recorder.document() &&
		std::holds_alternative<th03::replay::ProcessBoundary>(
			recorder.document()->session.records.back()),
		"MAIN_END is retained exactly once and duplicate handoff is idempotent"
	);
	check(
		recorder.sample(th03::replay::InputPhase::interstitial, {}).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::mainl).has_value(),
		"the interstitial sample and MAINL_END remain in memory"
	);
	auto resident = round_start(mode, 0U, 0U, 101U).start;
	check(
		recorder.finish(th03::formats::ReplayEndReason::game_over, resident)
			.has_value(),
		"finalization validates and closes the in-memory replay"
	);
	auto finalized = recorder.take_completed();
	check(
		finalized && finalized->metadata &&
		finalized->metadata->status ==
			th03::formats::ReplayStatus::finalized &&
		finalized->metadata->end_reason ==
			th03::formats::ReplayEndReason::game_over &&
		finalized->session.sample_count == 3U &&
		!recorder.recording() &&
		th03::formats::write_v13_replay(*finalized).has_value(),
		"post-run Save receives one complete serializable in-memory document"
	);
	check(!recorder.take_completed(), "the completed replay can only be taken once");

	th03::app::NewClassicReplayRecorder buffered;
	check(
		buffered.round_started(round_start(mode, 0U, 0U, 102U)).has_value(),
		"a second recorder starts independently in memory"
	);
	for(std::uint16_t sample = 0U; sample < 130U; ++sample) {
		const auto odd = (sample & 1U) != 0U;
		check(
			buffered.sample(
				th03::replay::InputPhase::gameplay,
				{
					.p1 = static_cast<std::uint16_t>(odd ? 0x155U : 0x2AAU),
					.p2 = static_cast<std::uint16_t>(odd ? 0x2AAU : 0x155U),
					.system = static_cast<std::uint16_t>(odd ? 0x101U : 0x202U),
					.charge_mask = static_cast<std::uint8_t>(odd ? 1U : 2U),
				}
			).has_value(),
			"high-entropy input remains memory-resident past the old flush boundary"
		);
	}
	check(
		buffered.document() &&
		buffered.document()->session.sample_count == 130U &&
		buffered.document()->session.records.size() == 130U,
		"the former 128-sample disk checkpoint is now an ordinary memory boundary"
	);
	buffered.discard();
	check(
		!buffered.recording() && !buffered.document(),
		"restart or discard drops the active replay without storage work"
	);
}

void test_capture_disabled_owner()
{
	th03::app::NewClassicReplayRecorder recorder(std::uint16_t{0U}, false);
	check(!recorder.capture_enabled() && !recorder.recording() &&
		recorder.document() == nullptr && !recorder.take_completed(),
		"a disabled New Classic recorder preserves identity without capture state");
	check(!recorder.round_started(
		round_start(th03::game_mode::story, 0U, 0U, 29U)
	).has_value(), "disabled capture rejects an accidental recording start");
	check(recorder.configure_practice(std::nullopt).has_value() &&
		!recorder.configure_practice(th03::formats::ReplayPracticeConfig{}),
		"disabled capture accepts only the neutral Practice configuration");
	recorder.source_vsync();
	recorder.discard();
	check(!recorder.recording() && !recorder.take_completed(),
		"disabled capture remains empty across ordinary lifecycle cleanup");
}

void test_netplay_confirmed_capture()
{
	constexpr auto mode = th03::game_mode::vs_1p_2p;
	auto created = th03::app::NewClassicNetplayReplayCapture::create(
		round_start(mode, 0U, 0U, 201U), 0x5D0CU
	);
	check(created.has_value(),
		"netplay V13 capture starts from the source-fresh round observation");
	if(!created) {
		return;
	}
	auto &capture = *created;
	check(
		capture.append_confirmed_frame({
			.frame = 0U,
			.phase = th03::replay::InputPhase::gameplay,
			.input = {.p1 = 0x20U, .p2 = 0x08U, .charge_mask = 1U},
			.step_result = {
				.status = th03::StepStatus::running,
				.input_boundary_state = terminal(mode, 0U, 0U, 0U, 0),
				.next_step_vsyncs = 3U,
			},
			.source_vsyncs = 3U,
		}, true).has_value(),
		"one corrected confirmed frame records input, source waits, and local slowdown"
	);
	check(
		capture.append_confirmed_frame({
			.frame = 1U,
			.phase = th03::replay::InputPhase::gameplay,
			.input = {.p1 = 0x04U, .p2 = 0x02U},
			.step_result = {
				.status = th03::StepStatus::round_transition,
				.input_boundary_state = terminal(mode, 0U, 0U, 0U, 0),
				.next_step_vsyncs = 2U,
				.completed_round_state = terminal(mode, 0U, 0U, 1U, 0),
			},
			.source_vsyncs = 2U,
		}, false).has_value(),
		"the confirmed repeat terminal appends its split before the next checkpoint"
	);
	check(
		capture.round_started(round_start(mode, 0U, 1U, 203U)).has_value(),
		"confirmed route-1 continuation starts the next V13 round"
	);
	check(
		capture.append_confirmed_frame({
			.frame = 2U,
			.phase = th03::replay::InputPhase::gameplay,
			.input = {.p1 = 0x01U, .p2 = 0x10U, .charge_mask = 2U},
			.step_result = {
				.status = th03::StepStatus::terminal,
				.input_boundary_state = terminal(mode, 0U, 1U, 0U, 1),
				.completed_round_state = terminal(mode, 0U, 1U, 2U, 1),
			},
		}, true).has_value(),
		"the confirmed match terminal records the last sample, split, and MAIN_END"
	);
	auto resident = round_start(mode, 0U, 1U, 203U).start;
	auto document = capture.finish_mainl(resident);
	check(document.has_value(),
		"the completed winner MAINL owner finalizes one validated memory V13");
	if(document) {
		const auto main_controls = std::count_if(
			document->session.records.begin(), document->session.records.end(),
			[](const auto &record) {
				const auto *boundary =
					std::get_if<th03::replay::ProcessBoundary>(&record);
				return boundary && boundary->completed_process ==
					th03::replay::ProcessId::main;
			}
		);
		const auto mainl_controls = std::count_if(
			document->session.records.begin(), document->session.records.end(),
			[](const auto &record) {
				const auto *boundary =
					std::get_if<th03::replay::ProcessBoundary>(&record);
				return boundary && boundary->completed_process ==
					th03::replay::ProcessId::mainl;
			}
		);
		check(
			document->session.sample_count == 3U &&
			document->checkpoints.size() == 2U &&
			document->checkpoints[1U].sample_index == 2U &&
			document->expected_round_splits.size() == 2U &&
			main_controls == 1 && mainl_controls == 1,
			"confirmed capture preserves exact samples, repeat checkpoint, splits, and process controls"
		);
		check(
			document->expected_round_splits[0U].real_frames == 4U &&
			document->expected_round_splits[1U].real_frames == 2U &&
			document->expected_round_splits[0U].route == 1U &&
			document->expected_round_splits[1U].route == 2U &&
			document->slowdown && document->slowdown->timed_frames == 3U &&
			document->slowdown->slow_frames == 2U,
			"netplay replay timing charges each source wait to the following frame and keeps first-execution deadlines"
		);
		check(
			document->metadata &&
			document->metadata->status == th03::formats::ReplayStatus::finalized &&
			document->metadata->end_reason ==
				th03::formats::ReplayEndReason::game_over &&
			document->metadata->final_route == 2U &&
			th03::formats::write_v13_replay(*document).has_value(),
			"netplay memory capture publishes ordinary finalized V13 metadata and bytes"
		);
	}

	auto invalid = th03::app::NewClassicNetplayReplayCapture::create(
		round_start(mode, 0U, 0U, 211U)
	);
	check(invalid.has_value(), "invalid-order probe creates independently");
	if(invalid) {
		check(
			!invalid->append_confirmed_frame({.frame = 1U}, false) &&
			!invalid->finish_mainl(round_start(mode, 0U, 0U, 211U).start),
			"out-of-order or incomplete netplay capture fails closed"
		);
	}
}

} // namespace

int main()
{
	test_vs_capture();
	test_story_capture();
	test_timing_scope();
	test_invalid_sequences();
	test_memory_only_lifecycle();
	test_capture_disabled_owner();
	test_netplay_confirmed_capture();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
