#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

#include "th03/app/host_menu_canvas.hpp"
#include "th03/app/new_classic_practice.hpp"
#include "th03/app/new_classic_replay.hpp"
#include "th03/formats/v13.hpp"

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

th03::GameInit selected(std::int8_t p1 = 1, std::int8_t cpu = 3)
{
	th03::GameInit resident{};
	resident.resident_rand = 37;
	resident.random_seed = 37U;
	resident.game_mode = th03::game_mode::vs_1p_cpu;
	resident.rank = 1U;
	resident.key_mode = 0U;
	resident.is_cpu = {0U, 1U};
	resident.paletted_character = {p1, cpu};
	return resident;
}

th03::GameContent content()
{
	th03::GameContent value{};
	value.enemy_formations.resize(2U);
	for(auto &formation : value.enemy_formations) {
		formation.enemy_scripts.push_back({0U});
	}
	return value;
}

void release(th03::app::NewClassicPracticeMenu &menu)
{
	static_cast<void>(menu.consume({}));
}

void press(
	th03::app::NewClassicPracticeMenu &menu,
	th03::app::NewClassicPracticeInput input
)
{
	static_cast<void>(menu.consume(input));
	release(menu);
}

void test_source_menu()
{
	th03::app::NewClassicPracticeMenu menu(selected());
	check(
		menu.config().preset == 1U && menu.config().stage == 6U &&
		menu.config().round == 0U && menu.config().p1_spell == 7U &&
		menu.config().cpu_spell == 7U && menu.config().stock == 2U,
		"Practice defaults use the selected Reimu-vs-Mima fixed Story stage"
	);
	check(
		menu.config().initial_cpu_safety_frames == 1000U,
		"Story-native stage 7 round 1 safety comes from the shared source table"
	);

	release(menu);
	for(int row = 0; row < 3; ++row) {
		press(menu, {.raw_any = true, .down = true});
	}
	press(menu, {.raw_any = true, .confirm = true});
	press(menu, {.raw_any = true, .right = true});
	check(
		menu.config().cpu_timer == 2U &&
		menu.config().initial_cpu_safety_frames == 0xFFFFU,
		"advanced CPU Timer steps Story to Infinite and refreshes the anchor"
	);
	press(menu, {.raw_any = true, .cancel = true});
	press(menu, {.raw_any = true, .down = true});
	press(menu, {.raw_any = true, .down = true});
	check(
		menu.consume({.raw_any = true, .confirm = true}) ==
			th03::app::NewClassicPracticeStatus::started &&
		menu.practice_active(),
		"main Start completes a customized Practice transaction"
	);

	th03::app::HostMenuCanvas base;
	base.clear();
	auto surface = menu.compose_over(base.surface());
	const auto ink = std::count_if(
		surface.pixels.begin(), surface.pixels.end(),
		[](const auto &pixel) { return pixel.value != 0U; }
	);
	check(ink > 100U, "Practice setup composes visible host-owned text");
}

void test_exact_vs_default()
{
	th03::app::NewClassicPracticeMenu menu(selected(1, 5));
	release(menu);
	press(menu, {.raw_any = true, .right = true});
	check(
		menu.config().preset == 0U && menu.config().stage == 0U &&
		menu.config().stock == 0xFFU && !menu.practice_active(),
		"Mode VS resets to the patch's exact ordinary-VS configuration"
	);
	for(int row = 0; row < 5; ++row) {
		press(menu, {.raw_any = true, .down = true});
	}
	check(
		menu.consume({.raw_any = true, .confirm = true}) ==
			th03::app::NewClassicPracticeStatus::started &&
		!menu.practice_active(),
		"exact VS Start stays distinguishable from an active Practice overlay"
	);

	th03::app::NewClassicPracticeMenu chiyuri(selected(1, 15));
	th03::app::NewClassicPracticeMenu yumemi(selected(1, 17));
	check(
		chiyuri.config().stage == 7U && yumemi.config().stage == 8U,
		"Chiyuri and Yumemi force their source Story stages"
	);
}

void test_checkpoint_and_recording()
{
	th03::app::NewClassicPracticeMenu menu(selected());
	const auto config = menu.config();
	auto prepared = th03::app::prepare_new_classic_practice_init(
		selected(), config, content()
	);
	check(prepared.has_value(), "Practice first-round checkpoint prepares");
	if(!prepared) {
		return;
	}
	check(
		prepared->practice_rules && prepared->practice_rules->uses_stock &&
		prepared->round_start_snapshot &&
		prepared->round_start_snapshot->state.round_id == config.round &&
		prepared->round_start_snapshot->state.gauge_attack_level ==
			std::array<std::uint8_t, 2U>{7U, 7U} &&
		prepared->compact_runtime_snapshot &&
		prepared->compact_runtime_snapshot->players[0U].gauge_avail == 1024U,
		"checkpoint applies source stock, round, spell, and gauge fields"
	);
	auto game = th03::Game::create(
		*prepared, content(),
		th03::GameOptions{.input_protocol = th03::InputProtocol::recorded_replay}
	);
	check(game.has_value(), "the shared deterministic Game accepts Practice");
	if(!game) {
		return;
	}

	th03::app::NewClassicReplayRecorder recorder(0x5D0CU);
	check(
		recorder.configure_practice(config).has_value() &&
		recorder.round_started(game->round_start_observation()).has_value() &&
		recorder.sample(th03::replay::InputPhase::gameplay, {}).has_value(),
		"live recorder accepts the exact Practice anchor"
	);
	auto terminal = game->canonical_state();
	terminal.route = 0U;
	terminal.winner = 0;
	check(
		recorder.round_completed(terminal).has_value() &&
		recorder.process_completed(th03::replay::ProcessId::main).has_value() &&
		recorder.finish(
			th03::formats::ReplayEndReason::menu_return, *prepared
		).has_value(),
		"Practice terminal finalizes through V13"
	);
	auto document = recorder.take_completed();
	check(
		document && document->practice && document->metadata->practice &&
		document->metadata->flags == 0x0007U &&
		document->checkpoints[0U].stage == config.stage &&
		document->expected_round_splits[0U].story_stage == config.stage &&
		th03::formats::write_v13_replay(*document).has_value(),
		"Practice V13 retains flag, header, checkpoint stage, and split identity"
	);

	auto bad = config;
	++bad.initial_cpu_safety_frames;
	check(
		!th03::app::prepare_new_classic_practice_init(
			selected(), bad, content()
		),
		"Practice checkpoint rejects a safety value outside its selected policy"
	);
}

} // namespace

int main()
{
	test_source_menu();
	test_exact_vs_default();
	test_checkpoint_and_recording();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
