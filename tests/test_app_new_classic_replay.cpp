#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <variant>

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
		document->metadata->final_winner == 1,
		"VS summaries preserve repeat and terminal results"
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
		recorder.round_started(round_start(mode, 0U, 0U, 41U)).has_value() &&
		recorder.sample(th03::replay::InputPhase::gameplay, {}).has_value() &&
		recorder.round_completed(terminal(mode, 0U, 0U, 2U, 0)).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::main).has_value() &&
		recorder.sample(th03::replay::InputPhase::interstitial, {}).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::mainl).has_value(),
		"Story stage 0 records MAIN and MAINL as separate source processes"
	);
	check(
		recorder.round_started(round_start(mode, 1U, 0U, 53U)).has_value() &&
		recorder.sample(th03::replay::InputPhase::gameplay, {}).has_value() &&
		recorder.round_completed(terminal(mode, 1U, 0U, 0U, 1)).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::main).has_value() &&
		recorder.sample(th03::replay::InputPhase::interstitial, {}).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::mainl).has_value(),
		"Story stage 1 appends its checkpoint and terminal processes"
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

} // namespace

int main()
{
	test_vs_capture();
	test_story_capture();
	test_invalid_sequences();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
