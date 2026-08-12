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
	return document;
}

void test_landing()
{
	using namespace th03::app;
	NewClassicLandingMenu menu;
	expect(
		menu.choice() == NewClassicLandingChoice::play,
		"New Classic hub begins at Play Game"
	);
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
	expect(
		commit_replay_slot(root, 0U, document).has_value(),
		"fixture commits through the real atomic V13 store"
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
		catalog->slots[2U].state == ReplayCatalogSlotState::empty,
		"catalog distinguishes ready, invalid, and empty slots"
	);
	auto browser = NewClassicReplayBrowser::load(root);
	expect(browser.has_value(), "browser loads the complete 100-slot catalog");
	if(browser) {
		static_cast<void>((*browser)->consume({}));
		auto opened = (*browser)->consume({.confirm = true});
		expect(opened && opened->surface_changed,
			"confirm on an occupied slot opens replay details");
		static_cast<void>((*browser)->consume({}));
		auto play = (*browser)->consume({.confirm = true});
		expect(
			play && play->status == NewClassicReplayBrowserStatus::play_selected &&
			(*browser)->selected_checkpoint() == 0U &&
			(*browser)->take_selected_document().has_value(),
			"detail confirmation publishes the selected checkpoint document"
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
	NewClassicReplaySaveTestAccess::set_slot(save, 2U);
	auto requested = save.consume({.confirm = true}, *catalog);
	expect(
		requested.status == NewClassicReplaySaveStatus::save_requested &&
		save.selected_slot() == 2U,
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

} // namespace

int main()
{
	test_landing();
	test_catalog_browser_and_save();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
