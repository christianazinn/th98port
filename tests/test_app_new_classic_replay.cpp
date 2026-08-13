#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

#include "th03/app/new_classic_replay.hpp"
#include "th03/app/replay_slot_store.hpp"
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

std::vector<std::byte> read_file(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if(!input) {
		return {};
	}
	const auto end = input.tellg();
	if(end < 0) {
		return {};
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(end));
	input.seekg(0);
	if(!bytes.empty()) {
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	return input ? bytes : std::vector<std::byte>{};
}

void test_durable_fallback_lifecycle()
{
	using th03::app::NewClassicReplayErrorCode;
	using th03::app::ReplaySlotStoreFaultPoint;
	using th03::app::ReplaySlotStoreTestAccess;
	constexpr auto mode = th03::game_mode::vs_1p_cpu;
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto root = std::filesystem::temp_directory_path() /
		("th03-replay-recorder-" + std::to_string(nonce));
	std::error_code ignored;
	std::filesystem::remove_all(root, ignored);

	th03::app::NewClassicReplayRecorder recorder(root, 0x5D0CU);
	check(
		recorder.round_started(round_start(mode, 0U, 0U, 101U)).has_value() &&
		!std::filesystem::exists(th03::app::replay_fallback_path(root)),
		"checkpoint zero is held until V13 has its first representable sample"
	);
	check(
		recorder.sample(th03::replay::InputPhase::gameplay,
			{.p1 = 1U}).has_value(),
		"the first input atomically claims TH3LAST"
	);
	auto first = th03::app::load_replay_fallback(root);
	check(
		first && first->document && first->document->metadata &&
		first->document->metadata->status ==
			th03::formats::ReplayStatus::recording &&
		first->document->metadata->end_reason ==
			th03::formats::ReplayEndReason::partial &&
		first->document->session.sample_count == 1U,
		"the live fallback is a parseable Recording/Partial V13 artifact"
	);
	check(
		recorder.sample(th03::replay::InputPhase::gameplay,
			{.p1 = 2U}).has_value(),
		"ordinary samples remain in the source-shaped in-memory write buffer"
	);
	auto still_first = th03::app::load_replay_fallback(root);
	check(
		still_first && still_first->document &&
		still_first->document->session.sample_count == 1U,
		"ordinary input does not force a disk flush every frame"
	);
	ReplaySlotStoreTestAccess::fail_next(ReplaySlotStoreFaultPoint::replace_target);
	auto pause_failed = recorder.periodic_flush();
	check(
		!pause_failed && pause_failed.error().code ==
			NewClassicReplayErrorCode::journal_store && recorder.document() &&
		recorder.document()->session.sample_count == 2U,
		"a failed Replay Pause flush preserves the complete in-memory prefix"
	);
	check(
		recorder.periodic_flush().has_value(),
		"Replay Pause periodic flush retry publishes the buffered prefix"
	);
	auto paused = th03::app::load_replay_fallback(root);
	check(
		paused && paused->document &&
		paused->document->session.sample_count == 2U,
		"the periodic checkpoint contains every accepted sample"
	);
	check(
		recorder.round_completed(terminal(mode, 0U, 0U, 2U, 0)).has_value(),
		"the terminal summary precedes the MAIN_END checkpoint"
	);
	ReplaySlotStoreTestAccess::fail_next(ReplaySlotStoreFaultPoint::replace_target);
	auto handoff_failed = recorder.process_completed(
		th03::replay::ProcessId::main
	);
	check(
		!handoff_failed && handoff_failed.error().code ==
			NewClassicReplayErrorCode::journal_store && recorder.document() &&
		std::holds_alternative<th03::replay::ProcessBoundary>(
			recorder.document()->session.records.back()),
		"a failed MAIN_END flush retains the process control for retry"
	);
	check(
		recorder.process_completed(th03::replay::ProcessId::main).has_value(),
		"repeating MAIN_END idempotently retries the recoverable handoff"
	);
	auto handoff = th03::app::load_replay_fallback(root);
	check(
		handoff && handoff->document &&
		std::holds_alternative<th03::replay::ProcessBoundary>(
			handoff->document->session.records.back()) &&
		handoff->document->metadata->status ==
			th03::formats::ReplayStatus::recording,
		"the handoff fallback terminates at the exact process control"
	);
	check(
		recorder.sample(th03::replay::InputPhase::interstitial, {}).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::mainl).has_value(),
		"MAINL_END flushes the next continuation boundary"
	);
	auto resident = round_start(mode, 0U, 0U, 101U).start;
	const auto partial_before_finish = read_file(
		th03::app::replay_fallback_path(root)
	);
	ReplaySlotStoreTestAccess::fail_next(ReplaySlotStoreFaultPoint::replace_target);
	auto finish_failed = recorder.finish(
		th03::formats::ReplayEndReason::game_over, resident
	);
	check(
		!finish_failed && finish_failed.error().code ==
			NewClassicReplayErrorCode::journal_store && recorder.recording() &&
		read_file(th03::app::replay_fallback_path(root)) == partial_before_finish,
		"failed finalization preserves the active recorder and partial fallback"
	);
	check(
		recorder.finish(th03::formats::ReplayEndReason::game_over, resident)
			.has_value(),
		"finalization retry publishes the final replay before UI save"
	);
	const auto finalized_bytes = read_file(th03::app::replay_fallback_path(root));
	auto finalized = th03::app::load_replay_fallback(root);
	check(
		!finalized_bytes.empty() && finalized && finalized->document &&
		finalized->document->metadata->status ==
			th03::formats::ReplayStatus::finalized &&
		finalized->document->metadata->end_reason ==
			th03::formats::ReplayEndReason::game_over &&
		!recorder.recording() && recorder.take_completed().has_value(),
		"post-run save owns a durable finalized fallback after capture closes"
	);

	const auto automatic_root = root / "automatic";
	th03::app::NewClassicReplayRecorder automatic(automatic_root);
	check(
		automatic.round_started(round_start(mode, 0U, 0U, 102U)).has_value() &&
		automatic.sample(th03::replay::InputPhase::gameplay, {}).has_value(),
		"automatic-flush fixture claims an independent fallback"
	);
	for(std::uint16_t sample = 0U; sample < 130U; ++sample) {
		const auto odd = (sample & 1U) != 0U;
		check(
			automatic.sample(
				th03::replay::InputPhase::gameplay,
				{
					.p1 = static_cast<std::uint16_t>(odd ? 0x155U : 0x2AAU),
					.p2 = static_cast<std::uint16_t>(odd ? 0x2AAU : 0x155U),
					.system = static_cast<std::uint16_t>(odd ? 0x101U : 0x202U),
					.charge_mask = static_cast<std::uint8_t>(odd ? 1U : 2U),
				}
			).has_value(),
			"high-entropy input remains recordable across the 128-sample boundary"
		);
	}
	const auto automatically_flushed = th03::app::load_replay_fallback(
		automatic_root
	);
	check(
		automatically_flushed && automatically_flushed->document &&
		automatically_flushed->document->session.sample_count == 128U &&
		automatically_flushed->document->session.sample_count <
			automatic.document()->session.sample_count,
		"the source 128-sample cadence flushes without Pause or a handoff"
	);
	check(
		automatic.discard_persistent().has_value(),
		"automatic-flush fixture discards its fallback"
	);

	// A second recorder must not destroy pending crash recovery.
	th03::app::NewClassicReplayRecorder conflict(root);
	check(
		conflict.round_started(round_start(mode, 0U, 0U, 103U)).has_value(),
		"a new capture can build state before it attempts to claim the journal"
	);
	auto conflict_result = conflict.sample(
		th03::replay::InputPhase::gameplay, {}
	);
	check(
		!conflict_result && conflict_result.error().code ==
			NewClassicReplayErrorCode::journal_conflict &&
		read_file(th03::app::replay_fallback_path(root)) == finalized_bytes,
		"a valid preexisting TH3LAST is reported and never overwritten"
	);

	check(
		th03::app::remove_replay_fallback(root).has_value(),
		"the conflict fixture can explicitly discard the pending artifact"
	);
	th03::app::NewClassicReplayRecorder discard(root);
	check(
		discard.round_started(round_start(mode, 0U, 0U, 107U)).has_value() &&
		discard.sample(th03::replay::InputPhase::gameplay, {}).has_value(),
		"restart/discard fixture claims a fresh fallback"
	);
	ReplaySlotStoreTestAccess::fail_next(ReplaySlotStoreFaultPoint::remove_target);
	auto discard_failed = discard.discard_persistent();
	check(
		!discard_failed && discard_failed.error().code ==
			NewClassicReplayErrorCode::journal_store && discard.recording() &&
		std::filesystem::exists(th03::app::replay_fallback_path(root)),
		"failed restart/discard removal preserves both memory and disk recovery"
	);
	check(
		discard.discard_persistent().has_value() && !discard.recording() &&
		!std::filesystem::exists(th03::app::replay_fallback_path(root)),
		"restart/discard retry removes TH3LAST before closing capture"
	);

	std::filesystem::create_directories(root);
	{
		std::ofstream malformed(
			th03::app::replay_fallback_path(root),
			std::ios::binary | std::ios::trunc
		);
		malformed << "bad";
	}
	const auto malformed_bytes = read_file(th03::app::replay_fallback_path(root));
	th03::app::NewClassicReplayRecorder malformed(root);
	check(
		malformed.round_started(round_start(mode, 0U, 0U, 109U)).has_value(),
		"malformed conflict fixture reaches its first input"
	);
	auto malformed_result = malformed.sample(
		th03::replay::InputPhase::gameplay, {}
	);
	check(
		!malformed_result && malformed_result.error().code ==
			NewClassicReplayErrorCode::journal_store &&
		malformed_result.error().store &&
		malformed_result.error().store->code ==
			th03::app::ReplaySlotStoreErrorCode::format &&
		read_file(th03::app::replay_fallback_path(root)) == malformed_bytes,
		"a malformed preexisting TH3LAST fails explicitly without replacement"
	);
	std::filesystem::remove_all(root, ignored);
}

} // namespace

int main()
{
	test_vs_capture();
	test_story_capture();
	test_timing_scope();
	test_invalid_sequences();
	test_durable_fallback_lifecycle();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
