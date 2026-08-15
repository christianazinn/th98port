#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

#include "service_protocol.hpp"
#include "th03/app/new_classic_story_route.hpp"
#include "th03/formats/v13.hpp"

namespace {

bool require(bool condition, const char *message)
{
	if(!condition) {
		std::cerr << "FAIL: " << message << '\n';
	}
	return condition;
}

th03::formats::ReplayDocument stock_document()
{
	th03::formats::ReplayDocument document{};
	document.session.source_version = th03::formats::replay_v13_version;
	auto &init = document.session.init;
	init.global_frame = 0U;
	init.resident_rand = 34;
	init.random_seed = std::bit_cast<std::uint32_t>(init.resident_rand);
	init.game_mode = th03::game_mode::story;
	init.rank = 2U;
	init.story_stage = 0U;
	init.story_lives = 2U;
	init.remaining_credits = 3U;
	init.skill = 120U;
	init.demo_number = 0U;
	init.is_cpu = {0U, 1U};
	init.paletted_character = {4, -1};
	init.story_opponents = *th03::app::new_classic_story_opponents(
		init.paletted_character[0U], init.resident_rand
	);
	init.paletted_character[1U] = init.story_opponents[0U];
	init.winner = 0;
	init.show_score_menu = 0U;
	init.fast_op_animation = 0U;
	init.autofire_mask = 1U;
	document.checkpoints.push_back(th03::formats::ReplayCheckpoint{
		.stage = 0U,
		.round = 0U,
		.sample_index = 0U,
		.init = init,
	});
	document.metadata.emplace();
	document.metadata->status = th03::formats::ReplayStatus::finalized;
	document.metadata->end_reason = th03::formats::ReplayEndReason::complete;
	// Deliberately tampered: the service score must come from simulation.
	document.metadata->final_story_stage = static_cast<std::uint8_t>(1U);
	document.metadata->final_score = {0x99U, 0x99U, 0x99U, 0x99U};
	document.metadata->dos_date = 0x5D0FU;
	document.metadata->name = {'A','Z','I','N','N',' ',' ',' '};
	document.slowdown = th03::formats::ReplaySlowdown{
		.timed_frames = 10'000U,
		.slow_frames = 500U,
	};
	return document;
}

std::optional<th03::replay::RoundStartRecord> round_start(
	const th03::GameInit &init,
	const th03::GameContent &content
)
{
	const auto options = th03::GameOptions{
		.input_protocol = th03::InputProtocol::recorded_replay,
	};
	auto game = th03::Game::create(init, content, options);
	if(!game) {
		return std::nullopt;
	}
	return th03::replay::RoundStartRecord{
		.sample_index = 0U,
		.observation = game->round_start_observation(),
	};
}

} // namespace

int main()
{
	const th03::GameContent content{};
	auto document = stock_document();
	const auto initial = round_start(document.session.init, content);
	if(!require(initial.has_value(), "stock Story first round constructs")) {
		return EXIT_FAILURE;
	}

	th03::replay::RunResult run{};
	run.status = th03::replay::RunStatus::complete;
	run.completed_rounds.push_back({});
	run.terminal_state = th03::CanonicalState{
		.route = 2U,
		.game_mode = th03::game_mode::story,
		.story_stage = 8U,
		.winner = static_cast<std::int8_t>(th03::PlayerId::p1),
		.p1_score = {0U, 7U, 6U, 5U, 4U, 3U, 2U, 1U},
	};
	run.terminal_transition = th03::SessionTransition{
		.action = th03::SessionAction::ending,
		.high_score_registration = document.session.init,
	};
	run.round_starts.push_back(*initial);
	run.round_starts.push_back(*initial);
	run.round_starts.back().observation.start.remaining_credits = 2U;

	const auto report = th03rv::make_service_accepted_report(
		document, run, content, true, true
	);
	if(!require(report.has_value(), "valid verified stock Story report")) {
		return EXIT_FAILURE;
	}
	const auto json = th03rv::service_accepted_json(*report);
	if(!require(
		json.find("\"score\":123456700") != std::string::npos,
		"visible score comes from simulation with the trailing-zero unit"
	) || !require(
		json.find("999999990") == std::string::npos,
		"header score cannot enter report"
	) || !require(
		json.find("\"character\":1") != std::string::npos,
		"paletted character maps to the zero-based category"
	) || !require(
		json.find("\"endStage\":8") != std::string::npos,
		"end stage comes from simulation"
	) || !require(
		json.find("\"continuesUsed\":1") != std::string::npos,
		"continues derive from reconstructed credit state"
	) || !require(
		json.find("\"kind\":\"clear\"") != std::string::npos,
		"clear derives from simulated terminal transition"
	) || !require(
		json.find("\"protocol\":\"T3RV-SERVICE-1\"") != std::string::npos,
		"protocol is explicit"
	)) {
		return EXIT_FAILURE;
	}

	document.metadata->end_reason = th03::formats::ReplayEndReason::game_over;
	if(!require(
		!th03rv::make_service_accepted_report(
			document, run, content, true, true
		),
		"tampered header end reason cannot bless a conflicting clear"
	)) {
		return EXIT_FAILURE;
	}
	document.metadata->end_reason = th03::formats::ReplayEndReason::complete;

	run.terminal_state->p1_score.fill(9U);
	const auto maximum = th03rv::make_service_accepted_report(
		document, run, content, true, true
	);
	if(!require(
		maximum && maximum->score == 999'999'990U,
		"maximum visible score preserves score units"
	)) {
		return EXIT_FAILURE;
	}

	run.terminal_state->story_stage = 4U;
	run.terminal_state->winner = static_cast<std::int8_t>(th03::PlayerId::p2);
	run.terminal_transition->action = th03::SessionAction::request_continue;
	document.metadata->end_reason = th03::formats::ReplayEndReason::game_over;
	const auto game_over = th03rv::make_service_accepted_report(
		document, run, content, true, true
	);
	if(!require(
		game_over && game_over->result_kind == "game_over",
		"simulated P2 terminal transition derives Game Over"
	)) {
		return EXIT_FAILURE;
	}
	run.terminal_state->route = 0U;
	if(!require(
		!th03rv::make_service_accepted_report(
			document, run, content, true, true
		),
		"menu-return route cannot be reported as Game Over"
	)) {
		return EXIT_FAILURE;
	}
	run.terminal_state->route = 2U;

	if(!require(
		!th03rv::make_service_accepted_report(
			document, run, content, false, true
		),
		"split mismatch cannot produce accepted report"
	)) {
		return EXIT_FAILURE;
	}
	document.metadata->name.fill('\0');
	const auto unnamed = th03rv::make_service_accepted_report(
		document, run, content, true, true
	);
	if(!require(
		unnamed && unnamed->display_name.empty(),
		"valid empty replay name remains empty"
	)) {
		return EXIT_FAILURE;
	}

	auto later = document;
	later.checkpoints.front().stage = 1U;
	if(!require(
		!th03rv::make_service_accepted_report(
			later, run, content, true, true
		),
		"later-stage checkpoint cannot masquerade as a full run"
	)) {
		return EXIT_FAILURE;
	}

	auto tampered_route = document;
	std::swap(
		tampered_route.session.init.story_opponents[1U],
		tampered_route.session.init.story_opponents[2U]
	);
	tampered_route.checkpoints.front().init = tampered_route.session.init;
	if(!require(
		!th03rv::make_service_accepted_report(
			tampered_route, run, content, true, true
		),
		"reordered Story opponents cannot masquerade as the selected route"
	)) {
		return EXIT_FAILURE;
	}

	auto crafted = document;
	crafted.session.init.runtime_snapshot = initial->observation.runtime;
	crafted.session.init.round_start_snapshot = initial->observation.round;
	crafted.session.init.round_start_snapshot->state.score[0U][0U] = 1U;
	crafted.checkpoints.front().init = crafted.session.init;
	const auto crafted_start = round_start(crafted.session.init, content);
	auto crafted_run = run;
	if(!require(crafted_start.has_value(), "crafted checkpoint constructs")) {
		return EXIT_FAILURE;
	}
	crafted_run.round_starts.front() = *crafted_start;
	if(!require(
		!th03rv::make_service_accepted_report(
			crafted, crafted_run, content, true, true
		),
		"checkpoint-zero score/carry injection is not stock authority"
	)) {
		return EXIT_FAILURE;
	}

	run.terminal_state.reset();
	if(!require(
		!th03rv::make_service_accepted_report(
			document, run, content, true, true
		),
		"missing simulated terminal cannot produce accepted report"
	)) {
		return EXIT_FAILURE;
	}

	std::cout << "th03rv service protocol tests passed\n";
	return EXIT_SUCCESS;
}
