#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "th03/app/new_classic_frontend.hpp"
#include "th03/formats/v13.hpp"

namespace {

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

void release(th03::app::NewClassicLandingMenu &menu)
{
	static_cast<void>(menu.consume({}));
}

template <typename Menu, typename Catalog>
void release(Menu &menu, const Catalog &catalog)
{
	static_cast<void>(menu.consume({}, catalog));
}

th03::formats::ReplayDocument make_document()
{
	th03::GameInit init{};
	init.resident_rand = 29;
	init.random_seed = 0x12345678U;
	init.game_mode = th03::game_mode::vs_1p_cpu;
	init.is_cpu = {0U, 1U};
	init.paletted_character = {1, 4};
	init.compact_runtime_snapshot = th03::CompactRuntimeSnapshot{
		.round_reset_seed = 29U,
		.random_seed = init.random_seed,
		.first_formation = 1U,
	};
	init.round_start_snapshot = th03::RoundStartSnapshot{};

	th03::formats::ReplayMetadata metadata{};
	metadata.flags = 3U;
	metadata.status = th03::formats::ReplayStatus::finalized;
	metadata.end_reason = th03::formats::ReplayEndReason::menu_return;
	metadata.game_mode = init.game_mode;
	metadata.paletted_character = init.paletted_character;
	metadata.is_cpu = init.is_cpu;
	metadata.sample_count = 1U;
	metadata.final_frame_count = 1U;
	metadata.resident_rand = init.resident_rand;
	metadata.random_seed = init.random_seed;
	metadata.summary_flags = 0x7FU;
	metadata.final_route = std::uint8_t{0U};
	metadata.final_game_mode = init.game_mode;
	metadata.final_round_id = std::uint8_t{0U};
	metadata.final_winner = std::int8_t{0};
	metadata.dos_date = static_cast<std::uint16_t>(
		((2026U - 1980U) << 9U) | (8U << 5U) | 12U
	);
	metadata.name = {{'A','L','P','H','A',' ',' ',' '}};
	metadata.final_score = {1U, 2U, 3U, 4U};

	th03::formats::ReplayDocument document{};
	document.session.source_version = th03::formats::replay_v13_version;
	document.session.init = init;
	document.session.records = {
		th03::replay::InputSample{
			.frame_index = 0U,
			.phase = th03::replay::InputPhase::gameplay,
			.input = {},
			.round_frame = std::nullopt,
			.round_or_result_frame = std::nullopt,
		},
		th03::replay::ProcessBoundary{
			.sample_index = 1U,
			.completed_process = th03::replay::ProcessId::main,
		},
	};
	document.session.sample_count = 1U;
	document.metadata = metadata;
	document.checkpoints.push_back({
		.stage = 0x0FU,
		.round = 0U,
		.sample_index = 0U,
		.global_frame = 0U,
		.input_byte_cursor = 0U,
		.init = init,
	});
	document.expected_round_splits.push_back({
		.story_stage = 0x0FU,
		.round_id = 0U,
		.route = 2U,
		.winner = 0,
		.p1_score = {0x34U, 0x12U, 0U, 0U},
		.p2_score = {0x67U, 0x05U, 0U, 0U},
		.real_frames = 3'948U,
	});
	document.stage_clear_bonuses.emplace();
	document.slowdown = th03::formats::ReplaySlowdown{
		.timed_frames = 100U,
		.slow_frames = 25U,
	};
	return document;
}

th03::formats::ReplayDocument make_story_document()
{
	auto document = make_document();
	auto &metadata = *document.metadata;
	metadata.game_mode = th03::game_mode::story;
	metadata.final_game_mode = th03::game_mode::story;
	metadata.story_stage = 0U;
	metadata.final_story_stage = std::uint8_t{0U};
	metadata.stage_reached_count = 1U;
	metadata.story.emplace();
	metadata.story->stage_opponents[0U] = 4U;
	metadata.story->stage_scores[0U] = {0x34U, 0x12U, 0U, 0U};
	auto &checkpoint = document.checkpoints[0U];
	checkpoint.stage = 0U;
	checkpoint.init.game_mode = th03::game_mode::story;
	checkpoint.init.story_stage = 0U;
	checkpoint.init.story_opponents[0U] = 4;
	document.session.init = checkpoint.init;
	document.expected_round_splits[0U].story_stage = 0U;
	auto &bonus = (*document.stage_clear_bonuses)[0U];
	bonus.total = {0U, 0x20U, 0U, 0U};
	bonus.max_combo = 2U;
	return document;
}

th03::formats::ReplayDocument make_practice_document()
{
	auto document = make_document();
	auto &metadata = *document.metadata;
	metadata.flags = 0x0007U;
	metadata.practice.emplace();
	metadata.practice->preset = 0U;
	metadata.practice->stage = 0U;
	metadata.practice->round = 0U;
	metadata.practice->stock = 0xFFU;
	metadata.practice->p1_spell = 1U;
	metadata.practice->cpu_spell = 1U;
	metadata.practice->p1_gauge = 64U;
	metadata.practice->cpu_gauge = 64U;
	document.practice = true;
	auto &checkpoint = document.checkpoints[0U];
	checkpoint.stage = 0U;
	checkpoint.init.story_stage = 0U;
	checkpoint.init.practice_rules = th03::PracticeRules{};
	checkpoint.init.round_start_snapshot->state.gauge_attack_level = {1U, 1U};
	checkpoint.init.compact_runtime_snapshot->players[0U].gauge_avail = 1'024U;
	checkpoint.init.compact_runtime_snapshot->players[1U].gauge_avail = 1'024U;
	document.session.init = checkpoint.init;
	document.expected_round_splits[0U].story_stage = 0U;
	auto &bonus = (*document.stage_clear_bonuses)[0U];
	bonus.total = {0U, 0x20U, 0U, 0U};
	bonus.max_combo = 2U;
	return document;
}

std::uint64_t surface_hash(th03::present::SourceSurface surface)
{
	std::uint64_t hash = 1'469'598'103'934'665'603ULL;
	for(const auto pixel : surface.pixels) {
		hash ^= static_cast<std::uint8_t>(pixel.layer);
		hash *= 1'099'511'628'211ULL;
		hash ^= pixel.value;
		hash *= 1'099'511'628'211ULL;
	}
	return hash;
}

void test_landing()
{
	using namespace th03::app;
	NewClassicLandingMenu menu;
	expect(
		menu.choice() == NewClassicLandingChoice::play,
		"New Classic hub begins at Play Game"
	);
	const auto landing = menu.surface();
	bool credit_ink = false;
	for(std::size_t y = 380U; y < 388U; ++y) {
		for(std::size_t x = 0U; x < th03::present::graphics_width; ++x) {
			credit_ink = credit_ink || landing.pixels[
				(y * th03::present::graphics_width) + x
			].value != 0U;
		}
	}
	expect(credit_ink,
		"minimal New Classic landing retains the Replay Patch author credit");
	release(menu);
	auto moved = menu.consume({.down = true});
	expect(
		moved.surface_changed && moved.choice ==
			NewClassicLandingChoice::replay,
		"hub navigates to Replay with edge-locked input"
	);
	release(menu);
	auto selected = menu.consume({.confirm = true});
	expect(
		selected.status == NewClassicLandingStatus::selected &&
		selected.choice == NewClassicLandingChoice::replay,
		"hub publishes the selected Replay route"
	);
	NewClassicLandingMenu cancel;
	release(cancel);
	expect(
		cancel.consume({.cancel = true}).choice ==
			NewClassicLandingChoice::back,
		"hub Cancel returns to the product menu"
	);
}

void test_catalog_browser_and_save()
{
	using namespace th03::app;
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto root = std::filesystem::temp_directory_path() /
		("th03-new-classic-ui-" + std::to_string(nonce));
	std::error_code ignored;
	std::filesystem::remove_all(root, ignored);
	auto document = make_document();
	auto base_commit = commit_replay_slot(root, 0U, document);
	expect(
		base_commit.has_value(),
		"fixture commits through the real atomic V13 store"
	);
	auto story_commit = commit_replay_slot(root, 2U, make_story_document());
	expect(
		story_commit.has_value(),
		"Story telemetry fixture commits through the real V13 store"
	);
	auto practice_commit = commit_replay_slot(root, 3U, make_practice_document());
	expect(
		practice_commit.has_value(),
		"Practice telemetry fixture commits through the real V13 store"
	);
	std::filesystem::create_directories(root);
	{
		std::ofstream malformed(root / "TH3R01.RPY", std::ios::binary);
		malformed << "bad";
	}
	auto catalog = scan_new_classic_replays(root);
	expect(catalog.has_value(), "catalog scan tolerates malformed individual slots");
	if(!catalog) {
		return;
	}
	expect(
		catalog->slots[0U].state == ReplayCatalogSlotState::ready &&
		catalog->slots[0U].name[0U] == 'A' &&
		catalog->slots[1U].state == ReplayCatalogSlotState::invalid &&
		catalog->slots[2U].state == ReplayCatalogSlotState::ready &&
		catalog->slots[3U].state == ReplayCatalogSlotState::ready &&
		catalog->slots[4U].state == ReplayCatalogSlotState::empty,
		"catalog distinguishes ready, invalid, and empty slots"
	);
	expect(
		new_classic_replay_timer_label(0U) == "0:00.00" &&
		new_classic_replay_timer_label(563U) == "0:09.98" &&
		new_classic_replay_timer_label(564U) == "0:10.00" &&
		new_classic_replay_timer_label(3'948U) == "1:10.00",
		"timer labels preserve Replay Patch 0.4.9 integer formatting"
	);
	auto browser = NewClassicReplayBrowser::load(root);
	expect(browser.has_value(), "browser loads the complete 100-slot catalog");
	if(browser) {
		static_cast<void>((*browser)->consume({}));
		auto opened = (*browser)->consume({.confirm = true});
		expect(opened && opened->surface_changed,
			"confirm on an occupied slot opens replay details");
		static_cast<void>((*browser)->consume({}));
		const auto split_hash = surface_hash((*browser)->surface());
		auto clears = (*browser)->consume({.right = true});
		expect(
			clears && clears->surface_changed && (*browser)->detail_page() ==
				NewClassicReplayDetailPage::clear_bonuses &&
			surface_hash((*browser)->surface()) != split_hash,
			"detail Right selects and redraws the Clears telemetry tab"
		);
		static_cast<void>((*browser)->consume({}));
		const auto selected_before = (*browser)->selected_checkpoint();
		auto ignored_up = (*browser)->consume({.up = true});
		expect(
			ignored_up && !ignored_up->surface_changed &&
			(*browser)->selected_checkpoint() == selected_before,
			"Clears is informative and does not move checkpoint focus"
		);
		static_cast<void>((*browser)->consume({}));
		auto ignored_play = (*browser)->consume({.confirm = true});
		expect(
			ignored_play && ignored_play->status ==
				NewClassicReplayBrowserStatus::running,
			"Clears cannot launch a checkpoint"
		);
		static_cast<void>((*browser)->consume({}));
		auto timers = (*browser)->consume({.right = true});
		expect(
			timers && timers->surface_changed && (*browser)->detail_page() ==
				NewClassicReplayDetailPage::timers &&
			surface_hash((*browser)->surface()) != split_hash,
			"detail Right selects and redraws the Timers telemetry tab"
		);
		static_cast<void>((*browser)->consume({}));
		auto play = (*browser)->consume({.confirm = true});
		expect(
			play && play->status == NewClassicReplayBrowserStatus::play_selected &&
			(*browser)->selected_checkpoint() == 0U &&
			(*browser)->take_selected_document().has_value(),
			"detail confirmation publishes the selected checkpoint document"
		);
	}

	for(const auto [slot, practice] : std::array{
		std::pair<std::uint8_t, bool>{std::uint8_t{2U}, false},
		std::pair<std::uint8_t, bool>{std::uint8_t{3U}, true},
	}) {
		auto telemetry = NewClassicReplayBrowser::load(root);
		expect(telemetry.has_value(), "telemetry browser reload succeeds");
		if(!telemetry) {
			continue;
		}
		static_cast<void>((*telemetry)->consume({}));
		for(std::uint8_t index = 0U; index < slot; ++index) {
			static_cast<void>((*telemetry)->consume({.down = true}));
			static_cast<void>((*telemetry)->consume({}));
		}
		auto opened = (*telemetry)->consume({.confirm = true});
		expect(
			opened && opened->surface_changed &&
			(*telemetry)->selected_slot() == slot &&
			(*telemetry)->detail_page() == NewClassicReplayDetailPage::splits,
			practice ? "Practice detail telemetry renders" :
				"Story detail telemetry renders"
		);
		static_cast<void>((*telemetry)->consume({}));
		expect(
			(*telemetry)->consume({.right = true})->surface_changed,
			practice ? "Practice clear telemetry renders" :
				"Story clear telemetry renders"
		);
	}

	NewClassicReplaySaveMenu save(make_document());
	release(save, *catalog);
	expect(
		save.consume({.confirm = true}, *catalog).surface_changed,
		"save prompt defaults to Yes and opens the name screen"
	);
	release(save, *catalog);
	NewClassicReplaySaveTestAccess::select_end(save);
	expect(
		save.consume({.confirm = true}, *catalog).surface_changed,
		"name End advances to the slot list"
	);
	release(save, *catalog);
	NewClassicReplaySaveTestAccess::set_slot(save, 4U);
	auto requested = save.consume({.confirm = true}, *catalog);
	expect(
		requested.status == NewClassicReplaySaveStatus::save_requested &&
		save.selected_slot() == 4U,
		"an empty slot requests exactly one atomic commit"
	);
	save.report_commit(false);
	expect(save.commit_failed(), "failed commits retain the document for retry");
	static_cast<void>(save.consume({} , *catalog));
	expect(
		save.consume({.confirm = true}, *catalog).surface_changed,
		"failure acknowledgement returns to the slot list"
	);

	NewClassicReplaySaveMenu overwrite(make_document());
	release(overwrite, *catalog);
	static_cast<void>(overwrite.consume({.confirm = true}, *catalog));
	release(overwrite, *catalog);
	NewClassicReplaySaveTestAccess::select_end(overwrite);
	static_cast<void>(overwrite.consume({.confirm = true}, *catalog));
	release(overwrite, *catalog);
	auto existing = overwrite.consume({.confirm = true}, *catalog);
	expect(existing.surface_changed,
		"occupied slot enters explicit overwrite confirmation");
	release(overwrite, *catalog);
	static_cast<void>(overwrite.consume({.up = true}, *catalog));
	release(overwrite, *catalog);
	expect(
		overwrite.consume({.confirm = true}, *catalog).status ==
			NewClassicReplaySaveStatus::save_requested,
		"overwrite requires selecting and confirming Yes"
	);
	overwrite.report_commit(true);
	expect(
		overwrite.consume({}, *catalog).status ==
			NewClassicReplaySaveStatus::saved,
		"successful commit makes the save transaction terminal"
	);
	std::filesystem::remove_all(root, ignored);
}

std::string_view key_name(void *, std::uint16_t key) noexcept
{
	return key == 25U ? "TWENTY FIVE" : "KEY";
}

void test_key_config_menu()
{
	using namespace th03::app;
	NewClassicKeyConfig defaults{};
	for(std::size_t index = 0U; index < defaults.bindings.size(); ++index) {
		defaults.bindings[index] = static_cast<std::uint16_t>(index + 1U);
	}
	auto initial = defaults;
	initial.autofire = 2U;
	NewClassicKeyConfigMenu menu(initial, defaults, 512U, 41U, key_name);
	static_cast<void>(menu.consume({}));
	// Story Up begins selected. Capture Story's existing fourth binding to
	// prove source duplicate assignment swaps within the Story range.
	static_cast<void>(menu.consume({.ui = {.confirm = true}}));
	static_cast<void>(menu.consume({.any_scancode_held = false}));
	static_cast<void>(menu.consume({
		.scancode_pressed = static_cast<std::uint16_t>(25U),
		.any_scancode_held = true,
	}));
	static_cast<void>(menu.consume({.any_scancode_held = false}));
	expect(
		menu.config().bindings[22U] == 25U &&
		menu.config().bindings[24U] == 23U,
		"KeyConfig swaps duplicate Story assignments"
	);

	// A released Right changes to P1 and selects Shot, matching the patch's
	// initial player-page action column.
	static_cast<void>(menu.consume({}));
	auto page = menu.consume({.ui = {.right = true}});
	expect(page.surface_changed, "KeyConfig changes among all three pages");
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.ui = {.confirm = true}}));
	static_cast<void>(menu.consume({.any_scancode_held = false}));
	static_cast<void>(menu.consume({
		.scancode_pressed = static_cast<std::uint16_t>(41U),
		.any_scancode_held = true,
	}));
	static_cast<void>(menu.consume({.any_scancode_held = false}));
	expect(
		menu.config().bindings[static_cast<std::size_t>(
			NewClassicPlayerAction::shot
		)] == defaults.bindings[static_cast<std::size_t>(
			NewClassicPlayerAction::shot
		)],
		"Escape cancels raw key capture without changing the binding"
	);

	// Move from Shot to Apply and exercise the retryable persistence handoff.
	for(std::size_t row = 0U; row < 4U; ++row) {
		static_cast<void>(menu.consume({}));
		static_cast<void>(menu.consume({.ui = {.down = true}}));
	}
	static_cast<void>(menu.consume({}));
	auto apply = menu.consume({.ui = {.confirm = true}});
	expect(
		apply.status == NewClassicKeyConfigStatus::apply_requested,
		"KeyConfig publishes a retryable Apply transaction"
	);
	menu.report_commit(false);
	expect(
		menu.status() == NewClassicKeyConfigStatus::running,
		"failed KeyConfig store retains the edited document"
	);
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.ui = {.confirm = true}}));
	menu.report_commit(true);
	expect(
		menu.status() == NewClassicKeyConfigStatus::applied,
		"successful KeyConfig store completes the transaction"
	);

	NewClassicKeyConfigMenu discard(initial, defaults, 512U, 41U, key_name);
	static_cast<void>(discard.consume({}));
	static_cast<void>(discard.consume({.ui = {.down = true}}));
	static_cast<void>(discard.consume({}));
	static_cast<void>(discard.consume({.ui = {.confirm = true}}));
	static_cast<void>(discard.consume({.any_scancode_held = false}));
	static_cast<void>(discard.consume({
		.scancode_pressed = static_cast<std::uint16_t>(30U),
		.any_scancode_held = true,
	}));
	static_cast<void>(discard.consume({.any_scancode_held = false}));
	static_cast<void>(discard.consume({}));
	expect(
		discard.consume({.ui = {.cancel = true}}).surface_changed,
		"Cancel on edited KeyConfig opens discard confirmation"
	);
	expect(
		discard.consume({.ui = {.confirm = true}}).status ==
			NewClassicKeyConfigStatus::back && discard.config() == initial,
		"discard confirmation restores the complete original document"
	);
}

void test_expanded_options_menu()
{
	using namespace th03::app;
	ClassicConfigOptions initial{};
	initial.bgm_mode = ClassicBgmMode::midi;
	initial.sfx_enabled = true;
	initial.language = ClassicLanguage::english;
	NewClassicOptionsMenu menu(initial);
	expect(menu.config().bgm_mode == ClassicBgmMode::off &&
		menu.config().language == ClassicLanguage::japanese,
		"New Classic Options normalizes unsupported MIDI and English profiles");
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.right = true}));
	expect(menu.config().rank == ClassicRank::hard,
		"Options cycles Replay Patch rank choices");
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.down = true}));
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.confirm = true}));
	expect(menu.config().bgm_mode == ClassicBgmMode::fm,
		"Options independently toggles FM music");
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.down = true}));
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.confirm = true}));
	expect(!menu.config().sfx_enabled,
		"Options independently toggles sound effects");
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.down = true}));
	static_cast<void>(menu.consume({}));
	const auto language = menu.consume({.confirm = true});
	expect(language.surface_changed &&
		menu.config().language == ClassicLanguage::japanese,
		"unavailable English selection is disclosed without faking resources");
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.down = true}));
	static_cast<void>(menu.consume({}));
	expect(menu.consume({.confirm = true}).status ==
		NewClassicOptionsStatus::key_config_requested,
		"expanded Options hands KeyConfig to its dedicated transaction");
	menu.resume_after_key_config();
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.down = true}));
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.confirm = true}));
	expect(menu.config().reduced_flash,
		"Options toggles Replay Patch reduced-flash policy");
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.down = true}));
	static_cast<void>(menu.consume({}));
	expect(menu.consume({.confirm = true}).status ==
		NewClassicOptionsStatus::apply_requested,
		"Options Back publishes an atomic config commit");
	menu.report_commit(false);
	expect(menu.status() == NewClassicOptionsStatus::running,
		"failed Options commit retains edits for retry");
	static_cast<void>(menu.consume({}));
	static_cast<void>(menu.consume({.confirm = true}));
	menu.report_commit(true);
	expect(menu.status() == NewClassicOptionsStatus::applied,
		"successful Options retry completes the transaction");

	initial.bgm_mode = ClassicBgmMode::fm;
	initial.language = ClassicLanguage::english;
	NewClassicOptionsMenu translated(initial, true);
	expect(translated.config().language == ClassicLanguage::english,
		"authenticated overlay preserves a persisted English selection");
	static_cast<void>(translated.consume({}));
	for(std::size_t row = 0U; row < 3U; ++row) {
		static_cast<void>(translated.consume({.down = true}));
		static_cast<void>(translated.consume({}));
	}
	static_cast<void>(translated.consume({.confirm = true}));
	expect(translated.config().language == ClassicLanguage::japanese,
		"available language row toggles English and Japanese");
	static_cast<void>(translated.consume({}));
	static_cast<void>(translated.consume({.right = true}));
	expect(translated.config().language == ClassicLanguage::english,
		"available language row accepts arrow toggles too");
}

} // namespace

int main()
{
	test_landing();
	test_catalog_browser_and_save();
	test_key_config_menu();
	test_expanded_options_menu();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
