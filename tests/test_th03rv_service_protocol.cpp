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
#include "th03/formats/v14.hpp"

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
	document.session.source_version = th03::formats::replay_v14_version;
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

th03::formats::ReplayDocument stock_netplay_document()
{
	th03::formats::ReplayDocument document{};
	document.session.source_version = th03::formats::replay_v14_version;
	auto &init = document.session.init;
	init.global_frame = 0U;
	init.resident_rand = 79;
	init.random_seed = std::bit_cast<std::uint32_t>(init.resident_rand);
	init.game_mode = th03::game_mode::vs_1p_2p;
	init.rank = 3U;
	init.key_mode = 1U;
	init.remaining_credits = 3U;
	init.is_cpu = {0U, 0U};
	init.paletted_character = {1, 4};
	init.autofire_mask = 2U;
	document.checkpoints.push_back(th03::formats::ReplayCheckpoint{
		.stage = 0U,
		.round = 0U,
		.sample_index = 0U,
		.init = init,
	});
	document.metadata.emplace();
	auto &metadata = *document.metadata;
	metadata.status = th03::formats::ReplayStatus::finalized;
	metadata.end_reason = th03::formats::ReplayEndReason::game_over;
	metadata.game_mode = th03::game_mode::vs_1p_2p;
	metadata.final_game_mode = th03::game_mode::vs_1p_2p;
	metadata.final_route = std::uint8_t{2U};
	metadata.final_winner = static_cast<std::int8_t>(th03::PlayerId::p2);
	metadata.dos_date = 0x5D10U;
	metadata.name = {'Y','U','M','E','M','I',' ',' '};
	metadata.recording_flags = th03::formats::replay_recording_flag_netplay;
	metadata.recorder_role = th03::formats::ReplayRecorderRole::player_two;
	metadata.recorder_source = th03::formats::ReplayRecorderSource::native;
	metadata.player_nametag_snapshot = {"First Player", "Second Player"};
	for(std::size_t index = 0U; index < 16U; ++index) {
		metadata.player_uuid[0U][index] = static_cast<std::uint8_t>(index + 1U);
		metadata.player_uuid[1U][index] = static_cast<std::uint8_t>(index + 17U);
		metadata.match_id[index] = static_cast<std::uint8_t>(index + 33U);
	}
	document.slowdown = th03::formats::ReplaySlowdown{
		.timed_frames = 2'000U,
		.slow_frames = 20U,
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
	th03::GameContent content{};
	content.enemy_formations.resize(2U);
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
	) || !require(
		json.find("\"sourceVersion\":14") != std::string::npos,
		"service report identifies the sole live V14 format"
	)) {
		return EXIT_FAILURE;
	}

	// The recorder stores the live post-initialization seed at the top level of
	// checkpoint zero as well as in its compact recipe. It is therefore normally
	// different from resident_rand even though the complete reconstructed start
	// is stock. This is the shape written by genuine V14 recordings.
	auto recorded = document;
	auto recorded_init = initial->observation.start;
	recorded_init.compact_runtime_snapshot = th03::CompactRuntimeSnapshot{
		.round_reset_seed = initial->observation.effective_reset_seed,
		.random_seed = initial->observation.runtime.random_seed,
		.first_formation =
			initial->observation.runtime.formation_type_ring[0U],
		.players = initial->observation.runtime.players,
	};
	recorded_init.round_start_snapshot = initial->observation.round;
	recorded.session.init = recorded_init;
	recorded.checkpoints.front().init = recorded_init;
	const auto recorded_start = round_start(recorded_init, content);
	if(!require(
		recorded_init.random_seed !=
			std::bit_cast<std::uint32_t>(recorded_init.resident_rand),
		"recorded checkpoint carries a distinct live random seed"
	) || !require(
		recorded_start.has_value(),
		"recorded compact checkpoint reconstructs"
	)) {
		return EXIT_FAILURE;
	}
	auto recorded_run = run;
	recorded_run.round_starts = {*recorded_start};
	if(!require(
		th03rv::make_service_accepted_report(
			recorded, recorded_run, content, true, true
		).has_value(),
		"genuine recorder-shaped stock checkpoint is accepted"
	)) {
		return EXIT_FAILURE;
	}
	auto tampered_live_seed = recorded;
	tampered_live_seed.session.init.random_seed ^= 0x01000000U;
	tampered_live_seed.session.init.compact_runtime_snapshot->random_seed =
		tampered_live_seed.session.init.random_seed;
	tampered_live_seed.checkpoints.front().init =
		tampered_live_seed.session.init;
	const auto tampered_live_start = round_start(
		tampered_live_seed.session.init, content
	);
	if(!require(
		tampered_live_start.has_value(),
		"tampered live-seed checkpoint constructs"
	)) {
		return EXIT_FAILURE;
	}
	auto tampered_live_run = recorded_run;
	tampered_live_run.round_starts = {*tampered_live_start};
	if(!require(
		!th03rv::make_service_accepted_report(
			tampered_live_seed, tampered_live_run, content, true, true
		),
		"stock reconstruction still rejects a forged live seed"
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

	auto netplay_document = stock_netplay_document();
	const auto netplay_start = round_start(netplay_document.session.init, content);
	if(!require(netplay_start.has_value(), "stock netplay first round constructs")) {
		return EXIT_FAILURE;
	}
	th03::replay::RunResult netplay_run{};
	netplay_run.status = th03::replay::RunStatus::complete;
	netplay_run.round_starts = {*netplay_start};
	netplay_run.completed_rounds.push_back({
		.result_summary = th03::MainResultState{
			.winner = th03::PlayerId::p2,
			.summary_active = true,
			.rounds_won = {1U, 2U},
		},
	});
	netplay_run.terminal_state = th03::CanonicalState{
		.route = 2U,
		.game_mode = th03::game_mode::vs_1p_2p,
		.winner = static_cast<std::int8_t>(th03::PlayerId::p2),
		.p1_score = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U},
		.p2_score = {8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U},
	};
	netplay_run.terminal_transition = th03::SessionTransition{
		.action = th03::SessionAction::exit_to_operator,
		.resident = {},
		.high_score_registration = std::nullopt,
	};
	const auto netplay = th03rv::make_service_accepted_report(
		netplay_document, netplay_run, content, true, true
	);
	if(!require(netplay.has_value(), "verified stock netplay report") ||
		!require(netplay->mode == "vs_1p_2p", "netplay mode remains explicit") ||
		!require(netplay->character == 1U, "recorder-side character is projected") ||
		!require(netplay->score == 123'456'780U,
			"recorder-side score is projected in visible units") ||
		!require(netplay->recorder_user_id ==
			"11121314-1516-1718-191a-1b1c1d1e1f20",
			"recorder-side UUID is canonical") ||
		!require(netplay->opponent_user_id ==
			"01020304-0506-0708-090a-0b0c0d0e0f10",
			"opponent UUID is canonical")) {
		return EXIT_FAILURE;
	}
	const auto netplay_json = th03rv::service_accepted_json(*netplay);
	if(!require(
		netplay_json.find("\"mode\":\"vs_1p_2p\"") != std::string::npos,
		"service JSON identifies netplay category"
	) || !require(
		netplay_json.find("\"kind\":\"versus_complete\"") !=
			std::string::npos,
		"service JSON identifies a completed versus match"
	) || !require(
		netplay_json.find("\"recorderUserId\":") != std::string::npos,
		"service JSON binds the authenticated recorder"
	)) {
		return EXIT_FAILURE;
	}

	auto missing_identity = netplay_document;
	missing_identity.metadata->player_uuid[1U].fill(0U);
	if(!require(
		!th03rv::make_service_accepted_report(
			missing_identity, netplay_run, content, true, true
		),
		"netplay without two account identities is not upload evidence"
	)) {
		return EXIT_FAILURE;
	}
	auto wrong_role = netplay_document;
	wrong_role.metadata->recorder_role =
		th03::formats::ReplayRecorderRole::unknown;
	if(!require(
		!th03rv::make_service_accepted_report(
			wrong_role, netplay_run, content, true, true
		),
		"netplay without a recorder role is rejected"
	)) {
		return EXIT_FAILURE;
	}
	auto forged_netplay = netplay_document;
	forged_netplay.session.init.round_start_snapshot = netplay_start->observation.round;
	forged_netplay.session.init.runtime_snapshot = netplay_start->observation.runtime;
	forged_netplay.session.init.round_start_snapshot->state.score[0U][0U] = 1U;
	forged_netplay.checkpoints.front().init = forged_netplay.session.init;
	const auto forged_netplay_start = round_start(
		forged_netplay.session.init, content
	);
	auto forged_netplay_run = netplay_run;
	if(!require(forged_netplay_start.has_value(),
		"forged netplay checkpoint constructs")) {
		return EXIT_FAILURE;
	}
	forged_netplay_run.round_starts = {*forged_netplay_start};
	if(!require(
		!th03rv::make_service_accepted_report(
			forged_netplay, forged_netplay_run, content, true, true
		),
		"netplay checkpoint-zero score injection is not stock authority"
	)) {
		return EXIT_FAILURE;
	}

	std::cout << "th03rv service protocol tests passed\n";
	return EXIT_SUCCESS;
}
