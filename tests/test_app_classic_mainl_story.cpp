#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <iostream>
#include <optional>

#include "th03/app/main/main_process.hpp"

#include "classic/mainl/classic_mainl_story_internal.hpp"

namespace {

using th03::CanonicalState;
using th03::ContinueChoice;
using th03::GameInit;
using th03::SessionAction;
using th03::SessionTransition;
using th03::app::ClassicInterstageCutscene;
using th03::app::ClassicMainlPlan;
using th03::app::ClassicMainlPlanErrorCode;
using th03::app::ClassicMainlScene;
using th03::app::ClassicMainlStep;
using th03::app::MainStoryConfig;
using th03::app::classic_story_continue_plan;
using th03::app::classic_story_mainl_plan;
using th03::app::classic_story_start_plan;
using th03::app::main_paletted_character;
using th03::app::main_player_selection_from_paletted;
using th03::app::valid_main_story_config;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

GameInit story_resident(
	std::uint8_t stage,
	std::uint8_t credits = 3U
) noexcept
{
	GameInit resident{};
	resident.global_frame = 1000U + stage;
	resident.resident_rand = 12345;
	resident.random_seed = 12345U;
	resident.game_mode = th03::game_mode::story;
	resident.rank = 1U;
	resident.key_mode = 0U;
	resident.story_stage = stage;
	resident.story_lives = 2U;
	resident.remaining_credits = credits;
	resident.skill = 3U;
	resident.is_cpu = {0U, 1U};
	resident.paletted_character = {2, 1};
	// Deliberately not stage-ordered: normal winner-text tests must derive the
	// block from the losing character rather than accidentally using stage.
	resident.story_opponents = {5, 1, 7, 9, 11, 13, 3, 15, 17};
	resident.paletted_character[1U] = resident.story_opponents[stage];
	return resident;
}

CanonicalState terminal_state(
	const GameInit &start,
	std::int8_t winner,
	std::uint8_t route = 2U
) noexcept
{
	CanonicalState terminal{};
	terminal.route = route;
	terminal.game_mode = th03::game_mode::story;
	terminal.story_stage = start.story_stage;
	terminal.winner = winner;
	terminal.global_frame = start.global_frame + 300U;
	terminal.round_frame = 77U;
	terminal.resident_rand = 54321;
	terminal.p1_score = {8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U};
	return terminal;
}

SessionTransition win_transition(
	const GameInit &start,
	const CanonicalState &terminal
) noexcept
{
	SessionTransition transition{};
	transition.resident = start;
	transition.resident.global_frame = terminal.global_frame;
	transition.resident.resident_rand = terminal.resident_rand;
	transition.resident.random_seed = terminal.round_frame;
	transition.resident.winner = terminal.winner;
	transition.resident.story_stage = static_cast<std::uint8_t>(
		start.story_stage + 1U
	);
	transition.resident.last_score = {};
	transition.resident.last_score[0U] = terminal.p1_score;
	if(start.story_stage == 8U) {
		transition.resident.story_stage = 99U;
		transition.high_score_registration = transition.resident;
		transition.action = SessionAction::ending;
	} else {
		transition.resident.paletted_character[1U] =
			start.story_opponents[transition.resident.story_stage];
		transition.action = SessionAction::start_gameplay;
	}
	return transition;
}

SessionTransition loss_transition(
	const GameInit &start,
	const CanonicalState &terminal
) noexcept
{
	SessionTransition transition{};
	transition.resident = start;
	transition.resident.global_frame = terminal.global_frame;
	transition.resident.resident_rand = terminal.resident_rand;
	transition.resident.random_seed = terminal.round_frame;
	transition.resident.winner = 1;
	transition.resident.story_stage = static_cast<std::uint8_t>(
		start.story_stage + 1U
	);
	transition.resident.last_score = {};
	transition.resident.last_score[0U] = terminal.p1_score;
	transition.high_score_registration = transition.resident;
	transition.resident.last_score = {};
	transition.action = (start.remaining_credits == 0U) ?
		SessionAction::exit_to_operator : SessionAction::request_continue;
	return transition;
}

ClassicMainlStep step(
	ClassicMainlScene scene,
	ClassicInterstageCutscene cutscene = ClassicInterstageCutscene::none
) noexcept
{
	return {scene, cutscene};
}

void expect_steps(
	const ClassicMainlPlan &plan,
	std::initializer_list<ClassicMainlStep> expected,
	const char *message
)
{
	const auto actual = plan.steps();
	expect(actual.size() == expected.size(), message);
	std::size_t index = 0U;
	for(const auto expected_step : expected) {
		expect(index < actual.size() && actual[index] == expected_step, message);
		++index;
	}
}

bool kotohime_fade(
	void *,
	const th03::BombFrameStep &,
	std::uint8_t,
	std::uint8_t &red_out
) noexcept
{
	red_out = 0U;
	return true;
}

MainStoryConfig story_config(
	std::uint8_t stage = 0U,
	std::int8_t player_one = 2
) noexcept
{
	MainStoryConfig config{};
	config.resident = story_resident(stage);
	config.resident.paletted_character[0U] = player_one;
	constexpr std::array<std::uint8_t, 9U> fixed_base{
		1U, 0U, 0U, 2U, 0U, 3U, 5U, 4U, 6U,
	};
	const auto player_base = static_cast<std::size_t>(
		(static_cast<std::uint8_t>(player_one) - 1U) / 2U
	);
	const auto fixed = fixed_base[player_base];
	std::size_t early_stage = 0U;
	for(std::uint8_t base = 0U; base < 7U; ++base) {
		if(base == fixed) {
			continue;
		}
		auto paletted = static_cast<std::int8_t>((base * 2U) + 1U);
		if(paletted == player_one) {
			++paletted;
		}
		config.resident.story_opponents[early_stage++] = paletted;
	}
	for(std::size_t story_stage = 6U; story_stage < 9U; ++story_stage) {
		const auto base = (story_stage == 6U) ? fixed :
			static_cast<std::uint8_t>(story_stage);
		auto paletted = static_cast<std::int8_t>((base * 2U) + 1U);
		if(paletted == player_one) {
			++paletted;
		}
		config.resident.story_opponents[story_stage] = paletted;
	}
	config.resident.paletted_character[1U] =
		config.resident.story_opponents[stage];
	config.kotohime_compatibility.component_provider = &kotohime_fade;
	return config;
}

void test_story_config_and_selection_boundary()
{
	for(std::int8_t paletted = 1; paletted <= 18; ++paletted) {
		const auto selection = main_player_selection_from_paletted(paletted);
		expect(selection.has_value(), "paletted character converts to selection");
		if(selection) {
			expect(main_paletted_character(*selection) == paletted,
				"selection converts back to the exact paletted character");
		}
	}
	expect(!main_player_selection_from_paletted(0),
		"paletted character zero is rejected");
	expect(!main_player_selection_from_paletted(19),
		"paletted character 19 is rejected");
	expect(!main_paletted_character({
		.character = th03::app::MainCharacter::none,
		.palette = th03::app::MainPaletteVariant::first,
	}), "selection without a character is rejected");

	for(std::int8_t player = 1; player <= 18; ++player) {
		for(std::uint8_t stage = 0U; stage <= 8U; ++stage) {
			expect(valid_main_story_config(story_config(stage, player)),
				"every source Story player/stage config is accepted");
		}
	}
	auto invalid = story_config();
	invalid.resident.game_mode = th03::game_mode::vs_1p_cpu;
	expect(!valid_main_story_config(invalid),
		"Story config rejects another game mode");
	invalid = story_config();
	invalid.game_options.input_protocol = th03::InputProtocol::recorded_replay;
	expect(valid_main_story_config(invalid),
		"Story config accepts New Classic recording from process start");
	invalid = story_config();
	invalid.resident.rank = 4U;
	expect(!valid_main_story_config(invalid),
		"Story config rejects invalid rank");
	invalid = story_config();
	invalid.resident.key_mode = 3U;
	expect(!valid_main_story_config(invalid),
		"Story config rejects invalid key mode");
	invalid = story_config();
	invalid.resident.story_stage = 9U;
	expect(!valid_main_story_config(invalid),
		"Story config rejects post-clear gameplay stage");
	invalid = story_config();
	invalid.resident.remaining_credits = 4U;
	expect(!valid_main_story_config(invalid),
		"Story config rejects invalid credit count");
	invalid = story_config();
	invalid.resident.demo_number = 1U;
	expect(!valid_main_story_config(invalid),
		"Story config rejects demo state");
	invalid = story_config();
	invalid.resident.is_cpu = {1U, 0U};
	expect(!valid_main_story_config(invalid),
		"Story config rejects non-Story player ownership");
	invalid = story_config();
	invalid.resident.winner = 2;
	expect(!valid_main_story_config(invalid),
		"Story config rejects invalid resident winner");
	invalid = story_config();
	invalid.resident.show_score_menu = 2U;
	expect(!valid_main_story_config(invalid),
		"Story config rejects invalid score-menu flag");
	invalid = story_config();
	invalid.resident.fast_op_animation = 2U;
	expect(!valid_main_story_config(invalid),
		"Story config rejects invalid OP-animation flag");
	invalid = story_config();
	invalid.resident.autofire_mask = 4U;
	expect(!valid_main_story_config(invalid),
		"Story config rejects invalid autofire bits");
	invalid = story_config();
	invalid.resident.runtime_snapshot.emplace();
	expect(!valid_main_story_config(invalid),
		"Story config rejects full runtime snapshot");
	invalid = story_config();
	invalid.resident.compact_runtime_snapshot.emplace();
	expect(!valid_main_story_config(invalid),
		"Story config rejects compact runtime snapshot");
	invalid = story_config();
	invalid.resident.round_start_snapshot.emplace();
	expect(!valid_main_story_config(invalid),
		"Story config rejects round-start snapshot");
	invalid = story_config();
	invalid.resident.practice_rules.emplace();
	expect(!valid_main_story_config(invalid),
		"Story config rejects Practice policy");
	invalid = story_config(3U);
	invalid.resident.paletted_character[1U] = 1;
	expect(!valid_main_story_config(invalid),
		"Story config requires the current stage opponent");
	invalid = story_config();
	invalid.resident.story_opponents[3U] = 0;
	expect(!valid_main_story_config(invalid),
		"Story config rejects an invalid opponent ID");
	invalid = story_config();
	invalid.resident.story_opponents[3U] =
		invalid.resident.story_opponents[2U];
	expect(!valid_main_story_config(invalid),
		"Story config rejects duplicate early base characters");
	invalid = story_config();
	invalid.resident.story_opponents[6U] = 13;
	expect(!valid_main_story_config(invalid),
		"Story config requires P1's fixed stage-6 opponent");
	invalid = story_config();
	invalid.resident.story_opponents[0U] = 6;
	expect(!valid_main_story_config(invalid),
		"Story config rejects an invented opponent palette");
	invalid = story_config();
	invalid.resident.story_opponents[7U] = 13;
	expect(!valid_main_story_config(invalid),
		"Story config requires Chiyuri at stage 7");
	invalid = story_config();
	invalid.resident.story_opponents[8U] = 15;
	expect(!valid_main_story_config(invalid),
		"Story config requires Yumemi at stage 8");
	invalid = story_config();
	invalid.resident.last_score[1U][7U] = 10U;
	expect(!valid_main_story_config(invalid),
		"Story config rejects non-decimal score digits");
	invalid = story_config();
	invalid.resident.last_score[1U][7U] = 1U;
	expect(!valid_main_story_config(invalid),
		"Story config rejects impossible retained P2 score");
	invalid = story_config(3U);
	invalid.kotohime_compatibility = {};
	expect(valid_main_story_config(invalid),
		"Kotohime Story config accepts the deterministic normalization");
	invalid.kotohime_compatibility.component_provider = &kotohime_fade;
	expect(valid_main_story_config(invalid),
		"Kotohime Story config retains an optional research provider");
}

void test_fresh_story_start()
{
	const auto config = story_config();
	const auto plan = classic_story_start_plan(config);
	expect(plan.has_value(), "fresh Story entry is accepted");
	if(plan) {
		expect_steps(*plan, {
			step(ClassicMainlScene::stage_splash),
			step(ClassicMainlScene::start_main),
		}, "fresh Story enters splash before MAIN");
		expect(plan->resident().story_stage == 0U,
			"fresh Story plan retains stage zero");
	}

	auto invalid = config;
	invalid.resident.story_stage = 1U;
	invalid.resident.paletted_character[1U] =
		invalid.resident.story_opponents[1U];
	const auto bad_stage = classic_story_start_plan(invalid);
	expect(!bad_stage && bad_stage.error().code ==
		ClassicMainlPlanErrorCode::invalid_story_start,
		"fresh Story rejects a later stage");
	invalid = config;
	invalid.resident.runtime_snapshot.emplace();
	expect(!classic_story_start_plan(invalid),
		"fresh Story rejects snapshot-backed entry");
	invalid = config;
	invalid.resident.game_mode = th03::game_mode::vs_1p_cpu;
	expect(!classic_story_start_plan(invalid),
		"fresh Story rejects non-Story entry");
}

void test_story_win_matrix()
{
	for(std::uint8_t stage = 0U; stage < 8U; ++stage) {
		const auto start = story_resident(stage);
		const auto terminal = terminal_state(start, 0);
		const auto transition = win_transition(start, terminal);
		const auto plan = classic_story_mainl_plan(
			start,
			terminal,
			transition
		);
		expect(plan.has_value(), "Story win transition is accepted");
		if(!plan) {
			continue;
		}
		expect(plan->resident().story_stage == stage + 1U,
			"Story win carries the next stage");
		if(stage == 7U) {
			expect_steps(*plan, {
				step(
					ClassicMainlScene::interstage_cutscene,
					ClassicInterstageCutscene::yumemi
				),
				step(ClassicMainlScene::stage_splash),
				step(ClassicMainlScene::start_main),
			}, "Stage 8 uses the Yumemi cutscene without a win screen");
			expect(!plan->win(), "Stage 8 has no generic win context");
			continue;
		}
		if(stage == 6U) {
			expect_steps(*plan, {
				step(ClassicMainlScene::win),
				step(
					ClassicMainlScene::interstage_cutscene,
					ClassicInterstageCutscene::chiyuri
				),
				step(ClassicMainlScene::stage_splash),
				step(ClassicMainlScene::start_main),
			}, "Stage 7 shows win, then the Chiyuri cutscene");
		} else {
			expect_steps(*plan, {
				step(ClassicMainlScene::win),
				step(ClassicMainlScene::stage_splash),
				step(ClassicMainlScene::start_main),
			}, "ordinary Story win shows win before splash");
		}
		expect(plan->win().has_value(), "win screen has source context");
		if(plan->win()) {
			const auto loser = static_cast<std::uint8_t>(
				start.paletted_character[1U]
			);
			const auto loser_base = static_cast<std::uint8_t>(
				(loser - 1U) / 2U
			);
			const auto expected_message = (stage == 5U) ? 9U :
				((stage == 6U) ? 10U : loser_base);
			expect(plan->win()->message_block == expected_message,
				"win screen chooses the source message block");
			expect(plan->win()->winner_paletted_character == 2,
				"win screen snapshots the old P1 identity");
			expect(plan->win()->loser_paletted_character ==
				start.paletted_character[1U],
				"win screen snapshots the old P2 identity");
			expect(
				plan->win()->preload_stage_splash == (stage < 6U),
				"ordinary winner preloads only an immediately following splash"
			);
		}
	}
}

void test_story_ending_matrix()
{
	constexpr std::array<std::uint8_t, 2U> credit_values{3U, 2U};
	constexpr std::array<std::int8_t, 4U> player_values{1, 14, 15, 18};
	for(const std::uint8_t credits : credit_values) {
		for(const std::int8_t p1 : player_values) {
			auto start = story_resident(8U, credits);
			start.paletted_character[0U] = p1;
			const auto terminal = terminal_state(start, 0);
			const auto transition = win_transition(start, terminal);
			const auto plan = classic_story_mainl_plan(
				start,
				terminal,
				transition
			);
			expect(plan.has_value(), "final Story clear is accepted");
			if(!plan) {
				continue;
			}
			const bool extra = credits == 3U && p1 < 15;
			if(extra) {
				expect_steps(*plan, {
					step(ClassicMainlScene::ending),
					step(ClassicMainlScene::staff_verdict),
					step(ClassicMainlScene::high_score_registration),
					step(ClassicMainlScene::extra_ending),
					step(ClassicMainlScene::exit_to_operator),
				}, "untouched credits and early character add extra ending");
			} else {
				expect_steps(*plan, {
					step(ClassicMainlScene::ending),
					step(ClassicMainlScene::staff_verdict),
					step(ClassicMainlScene::high_score_registration),
					step(ClassicMainlScene::exit_to_operator),
				}, "ordinary clear omits the extra ending");
			}
			expect(plan->resident().story_stage == 99U,
				"ending carries source stage 99");
			expect(plan->high_score_registration().has_value(),
				"ending retains registration snapshot");
			expect(!plan->win(), "final clear has no generic win screen");
		}
	}
}

void test_story_loss_and_continue_matrix()
{
	for(std::uint8_t stage = 0U; stage <= 8U; ++stage) {
		for(std::uint8_t credits = 0U; credits <= 3U; ++credits) {
			const auto start = story_resident(stage, credits);
			const auto terminal = terminal_state(start, 1);
			const auto transition = loss_transition(start, terminal);
			const auto plan = classic_story_mainl_plan(
				start,
				terminal,
				transition
			);
			expect(plan.has_value(), "Story loss transition is accepted");
			if(!plan) {
				continue;
			}
			expect(plan->win().has_value(),
				"Story loss shows the opponent win screen");
			if(plan->win()) {
				expect(!plan->win()->preload_stage_splash,
					"opponent win never preloads a Continue-dependent splash");
			}
			expect(plan->high_score_registration().has_value(),
				"Story loss retains the pre-Continue registration");
			if(credits == 0U) {
				expect_steps(*plan, {
					step(ClassicMainlScene::win),
					step(ClassicMainlScene::high_score_registration),
					step(ClassicMainlScene::continue_menu),
					step(ClassicMainlScene::game_over),
					step(ClassicMainlScene::exit_to_operator),
				}, "zero-credit loss preserves Continue's early-return screen");
				continue;
			}
			expect_steps(*plan, {
				step(ClassicMainlScene::win),
				step(ClassicMainlScene::high_score_registration),
				step(ClassicMainlScene::continue_menu),
			}, "credited loss waits for a Continue choice");

			auto accepted = transition;
			accepted.action = SessionAction::start_gameplay;
			accepted.resident.story_stage = stage;
			accepted.resident.story_lives = 2U;
			accepted.resident.remaining_credits =
				static_cast<std::uint8_t>(credits - 1U);
			const auto accept_plan = classic_story_continue_plan(
				ContinueChoice::accept,
				accepted
			);
			expect(accept_plan.has_value(), "Continue Yes is accepted");
			if(accept_plan) {
				expect_steps(*accept_plan, {
					step(ClassicMainlScene::stage_splash),
					step(ClassicMainlScene::start_main),
				}, "Continue Yes returns through the splash");
			}

			auto declined = transition;
			declined.action = SessionAction::exit_to_operator;
			declined.resident.story_stage = stage;
			declined.resident.story_lives = 2U;
			const auto decline_plan = classic_story_continue_plan(
				ContinueChoice::decline,
				declined
			);
			expect(decline_plan.has_value(), "Continue No is accepted");
			if(decline_plan) {
				expect_steps(*decline_plan, {
					step(ClassicMainlScene::game_over),
					step(ClassicMainlScene::exit_to_operator),
				}, "Continue No shows Game Over before OP");
			}
		}
	}
}

void test_route_zero_and_invalid_transitions()
{
	const auto start = story_resident(4U);
	auto terminal = terminal_state(start, 0, 0U);
	SessionTransition route_zero{};
	route_zero.resident = start;
	route_zero.resident.global_frame = terminal.global_frame;
	route_zero.resident.resident_rand = terminal.resident_rand;
	route_zero.resident.random_seed = terminal.round_frame;
	route_zero.action = SessionAction::exit_to_operator;
	const auto plan = classic_story_mainl_plan(start, terminal, route_zero);
	expect(plan.has_value(), "route zero is accepted");
	if(plan) {
		expect_steps(*plan, {
			step(ClassicMainlScene::exit_to_operator),
		}, "route zero bypasses all MAINL screens");
	}

	terminal.route = 1U;
	const auto invalid_route = classic_story_mainl_plan(
		start,
		terminal,
		route_zero
	);
	expect(!invalid_route && invalid_route.error().code ==
		ClassicMainlPlanErrorCode::invalid_terminal_state,
		"nonterminal route is rejected");

	terminal = terminal_state(start, 0);
	auto transition = win_transition(start, terminal);
	transition.action = SessionAction::exit_to_operator;
	expect(!classic_story_mainl_plan(start, terminal, transition),
		"wrong Story-win action is rejected");
	transition = win_transition(start, terminal);
	transition.resident.story_stage = start.story_stage;
	expect(!classic_story_mainl_plan(start, terminal, transition),
		"wrong next stage is rejected");
	transition = win_transition(start, terminal);
	transition.high_score_registration = transition.resident;
	expect(!classic_story_mainl_plan(start, terminal, transition),
		"ordinary win rejects an invented registration");

	auto continue_transition = loss_transition(
		start,
		terminal_state(start, 1)
	);
	continue_transition.action = SessionAction::start_gameplay;
	continue_transition.resident.story_stage = start.story_stage;
	expect(!classic_story_continue_plan(
		ContinueChoice::decline,
		continue_transition
	), "Continue choice/action mismatch is rejected");
	continue_transition.high_score_registration.reset();
	expect(!classic_story_continue_plan(
		ContinueChoice::accept,
		continue_transition
	), "Continue transition without registration is rejected");
}

} // namespace

int main()
{
	test_story_config_and_selection_boundary();
	test_fresh_story_start();
	test_story_win_matrix();
	test_story_ending_matrix();
	test_story_loss_and_continue_matrix();
	test_route_zero_and_invalid_transitions();

	if(failures == 0U) {
		std::cout << "all " << checks
			<< " Classic MAINL Story route checks passed\n";
		return 0;
	}
	std::cerr << failures << " of " << checks
		<< " Classic MAINL Story route checks failed\n";
	return 1;
}
