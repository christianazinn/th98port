#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <cstdlib>
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

std::uint16_t injected_dos_date(void *context) noexcept
{
	return *static_cast<const std::uint16_t *>(context);
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

th03::formats::ReplayDocument make_multi_round_story_document()
{
	auto document = make_story_document();
	auto &metadata = *document.metadata;
	metadata.final_story_stage = std::uint8_t{8U};
	metadata.final_round_id = std::uint8_t{2U};
	metadata.stage_reached_count = 9U;
	for(std::size_t stage = 0U; stage < 9U; ++stage) {
		metadata.story->stage_opponents[stage] = 4U;
	}
	document.checkpoints.clear();
	document.expected_round_splits.clear();
	document.session.records.clear();
	for(std::uint32_t sample = 0U; sample < 11U; ++sample) {
		document.session.records.push_back(th03::replay::InputSample{
			.frame_index = sample,
			.phase = th03::replay::InputPhase::gameplay,
			.input = {},
			.round_frame = std::nullopt,
			.round_or_result_frame = std::nullopt,
		});
	}
	document.session.records.push_back(th03::replay::ProcessBoundary{
		.sample_index = 11U,
		.completed_process = th03::replay::ProcessId::main,
	});
	document.session.sample_count = 11U;
	metadata.sample_count = 11U;
	metadata.final_frame_count = 11U;
	std::uint32_t checkpoint_sample = 0U;
	for(std::uint8_t stage = 0U; stage < 9U; ++stage) {
		const auto count = stage == 8U ? 3U : 1U;
		for(std::uint8_t round = 0U; round < count; ++round) {
			auto init = document.session.init;
			init.story_stage = stage;
			init.story_opponents.fill(4);
			init.round_start_snapshot->state.round_id = round;
			document.checkpoints.push_back({
				.stage = stage,
				.round = round,
				.sample_index = checkpoint_sample,
				.global_frame = checkpoint_sample,
				.input_byte_cursor = 0U,
				.init = init,
			});
			document.expected_round_splits.push_back({
				.story_stage = stage,
				.round_id = round,
				.route = 2U,
				.winner = 0,
				.real_frames = std::nullopt,
			});
			++checkpoint_sample;
		}
	}
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

std::shared_ptr<const th03::app::NewClassicReplayPatchAssets>
optional_patch_assets()
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
	const auto *path = std::getenv("TH03_AZINN_DAT");
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
	if(!path || *path == '\0') {
		return {};
	}
	auto loaded = th03::app::load_new_classic_replay_patch_assets(
		std::filesystem::path(path)
	);
	if(!loaded) {
		return {};
	}
	return std::make_shared<const th03::app::NewClassicReplayPatchAssets>(
		std::move(*loaded)
	);
}

std::shared_ptr<const th03::app::NewClassicReplayPatchAssets>
pseudo_localized_patch_assets(
	const std::shared_ptr<const th03::app::NewClassicReplayPatchAssets> &assets
)
{
	if(!assets) {
		return {};
	}
	auto localized = std::make_shared<th03::app::NewClassicReplayPatchAssets>(
		*assets
	);
	localized->custom_messages =
		std::make_shared<const th03::app::NewClassicMessageCatalog>(
			th03::app::detail::new_classic_width_test_messages()
		);
	return localized;
}

template<typename Identity>
Identity online_identity(std::uint8_t seed)
{
	Identity result{};
	for(std::size_t index = 0U; index < result.bytes.size(); ++index) {
		result.bytes[index] = static_cast<std::uint8_t>(seed + index);
	}
	return result;
}

th03::app::NewClassicOnlineCacheDocument online_board(
	std::uint8_t rank,
	std::uint8_t character,
	std::uint8_t seed,
	th03::app::NewClassicOnlineReplayAvailability availability =
		th03::app::NewClassicOnlineReplayAvailability::downloaded
)
{
	using namespace th03::app;
	const NewClassicOnlineCategory category{
		.mode = NewClassicOnlineMode::story,
		.rank = rank,
		.character = character,
	};
	const NewClassicOnlineResult result{
		.kind = NewClassicOnlineResultKind::clear,
		.score = 123456780ULL + static_cast<std::uint64_t>(seed) * 10U,
		.end_stage = 8U,
	};
	NewClassicOnlineReplayRecord replay{
		.replay_id = online_identity<NewClassicOnlineReplayId>(seed),
		.content_id = online_identity<NewClassicOnlineContentId>(0x40U),
		.ruleset_id = online_identity<NewClassicOnlineRulesetId>(0x60U),
		.category = category,
		.result = result,
		.availability = availability,
		.dos_date = 0x5D2FU,
		.payload_bytes = 213592U,
		.autofire = 1U,
		.timed_frames = 10'000U,
		.slow_frames = 200U,
		.title = "ALPHA",
	};
	NewClassicLeaderboardEntry entry{
		.entry_id = online_identity<NewClassicOnlineEntryId>(
			static_cast<std::uint8_t>(seed + 1U)
		),
		.content_id = replay.content_id,
		.ruleset_id = replay.ruleset_id,
		.replay_id = replay.replay_id,
		.verifier_id = online_identity<NewClassicOnlineVerifierId>(0x80U),
		.category = category,
		.result = result,
		.provenance = NewClassicOnlineProvenance::replay_verified,
		.dos_date = replay.dos_date,
		.published_unix_seconds = 1786765432ULL,
		.player = "Alice",
	};
	NewClassicOnlineCacheDocument document{};
	document.generation = 1U;
	document.snapshot_id = online_identity<NewClassicOnlineSnapshotId>(
		static_cast<std::uint8_t>(seed + 2U)
	);
	document.fetched_unix_seconds = 1786766000ULL;
	document.board_category = category;
	document.leaderboard_entries.push_back(std::move(entry));
	document.replay_records.push_back(std::move(replay));
	return document;
}

void finish_entry_fade(th03::app::NewClassicReplayBrowser &browser)
{
	for(std::size_t boundary = 0U; boundary < 19U; ++boundary) {
		static_cast<void>(browser.consume({}));
	}
}

void finish_entry_fade(
	th03::app::NewClassicReplaySaveMenu &menu,
	const th03::app::NewClassicReplayCatalog &catalog
)
{
	for(std::size_t boundary = 0U; boundary < 19U; ++boundary) {
		static_cast<void>(menu.consume({}, catalog));
	}
}

void finish_entry_fade(th03::app::NewClassicHighScoreMenu &menu)
{
	for(std::size_t boundary = 0U; boundary < 19U; ++boundary) {
		static_cast<void>(menu.consume({}));
	}
}

void test_online_high_score_menu()
{
	using namespace th03::app;
	const auto patch_assets = optional_patch_assets();
	NewClassicHighScoreMenu unavailable(
		{online_board(2U, 4U, 0x20U)}, 2U, {}
	);
	expect(!unavailable.valid(),
		"online HiScore UI refuses to invent a non-patch fallback surface");
	if(!patch_assets) {
		return;
	}
	NewClassicHighScoreMenu wrong_rank(
		{online_board(1U, 4U, 0x20U)}, 2U, patch_assets
	);
	expect(!wrong_rank.valid(),
		"online HiScore hub remains hidden without a board for the active rank");

	NewClassicHighScoreMenu local(
		{online_board(2U, 4U, 0x20U)}, 2U, patch_assets
	);
	expect(local.valid() && local.surface().palette_tone == 0,
		"validated online cache opens a source-styled black-in HiScore hub");
	finish_entry_fade(local);
	static_cast<void>(local.consume({}));
	expect(local.consume({.confirm = true}).status ==
		NewClassicHighScoreMenuStatus::local_selected,
		"the hub defaults to the existing local HiScore destination");

	NewClassicHighScoreMenu online(
		{online_board(2U, 4U, 0x20U), online_board(2U, 5U, 0x30U)},
		2U, patch_assets
	);
	finish_entry_fade(online);
	static_cast<void>(online.consume({}));
	static_cast<void>(online.consume({.down = true}));
	static_cast<void>(online.consume({}));
	auto opened = online.consume({.confirm = true});
	expect(opened.surface_changed && online.viewing_online() &&
		!online.viewing_detail() && online.selected_board() == 0U,
		"Online Leaderboards opens the first cached character board");
	static_cast<void>(online.consume({}));
	static_cast<void>(online.consume({.right = true}));
	expect(online.selected_board() == 1U,
		"online board navigation wraps across cached character boards");
	static_cast<void>(online.consume({}));
	const auto list_hash = surface_hash(online.surface());
	static_cast<void>(online.consume({.confirm = true}));
	expect(online.viewing_detail() &&
		surface_hash(online.surface()) != list_hash,
		"a leaderboard row opens its replay telemetry detail surface");
	static_cast<void>(online.consume({}));
	static_cast<void>(online.consume({.cancel = true}));
	expect(online.viewing_online() && !online.viewing_detail(),
		"detail Cancel returns to the same online board");
	static_cast<void>(online.consume({}));
	static_cast<void>(online.consume({.cancel = true}));
	expect(!online.viewing_online(),
		"board Cancel returns to the HiScore hub");
	static_cast<void>(online.consume({}));
	expect(online.consume({.cancel = true}).status ==
		NewClassicHighScoreMenuStatus::back,
		"hub Cancel returns to the New Classic title");

	NewClassicHighScoreMenu download(
		{online_board(
			2U, 4U, 0x50U,
			NewClassicOnlineReplayAvailability::metadata_only
		)},
		2U, patch_assets
	);
	finish_entry_fade(download);
	static_cast<void>(download.consume({}));
	static_cast<void>(download.consume({.down = true}));
	static_cast<void>(download.consume({}));
	static_cast<void>(download.consume({.confirm = true}));
	static_cast<void>(download.consume({}));
	static_cast<void>(download.consume({.confirm = true}));
	static_cast<void>(download.consume({}));
	auto requested = download.consume({.confirm = true});
	auto request = download.take_replay_download_request();
	expect(requested.surface_changed && request &&
		request->availability ==
			NewClassicOnlineReplayAvailability::metadata_only &&
		!download.take_replay_download_request(),
		"online replay detail emits exactly one immutable download request");
	if(request) {
		auto wrong = request->replay_id;
		wrong.bytes[0U] ^= 0xFFU;
		expect(!download.report_replay_download(
			wrong, NewClassicOnlineReplayDownloadOutcome::downloaded
		), "download completion rejects an unrelated replay identity");
		expect(download.report_replay_download(
			request->replay_id,
			NewClassicOnlineReplayDownloadOutcome::failed
		), "matching failed download result is accepted between source ticks");
		expect(download.consume({}).surface_changed,
			"failed result redraws through the next source-owned menu tick");
		static_cast<void>(download.consume({}));
		static_cast<void>(download.consume({.confirm = true}));
		auto retry = download.take_replay_download_request();
		expect(retry && retry->replay_id == request->replay_id,
			"failed download can be retried after a fresh Confirm");
		if(retry) {
			expect(download.report_replay_download(
				retry->replay_id,
				NewClassicOnlineReplayDownloadOutcome::downloaded
			), "validated download marks the matching cached replay available");
			expect(download.consume({}).surface_changed,
				"successful result redraws through the next menu tick");
			static_cast<void>(download.consume({}));
			static_cast<void>(download.consume({.confirm = true}));
			expect(!download.take_replay_download_request(),
				"an already-downloaded replay cannot be downloaded twice");
		}
	}
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
	const auto patch_assets = optional_patch_assets();
	if(!patch_assets) {
		std::filesystem::remove_all(root, ignored);
		return;
	}
	auto browser = NewClassicReplayBrowser::load(root, patch_assets);
	expect(browser.has_value(), "browser loads the complete 100-slot catalog");
	if(browser) {
		finish_entry_fade(**browser);
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
		expect((*browser)->prepare_playback_transition(),
			"selected replay prepares the source-cleared OP exit boundary");
		const auto transition = (*browser)->surface();
		expect(
			transition.palette_tone == 0 &&
			transition.shown_page == th03::present::Page::zero &&
			std::all_of(transition.pixels.begin(), transition.pixels.end(), [](const auto &pixel) {
				return pixel.value == 0U;
			}),
			"playback transition publishes black page 0 before synchronous preroll"
		);
	}
	auto multi_round_document = make_multi_round_story_document();
	auto multi_round_encoded = th03::formats::write_v13_replay(
		multi_round_document
	);
	if(!multi_round_encoded) {
		std::cerr << "multi-round writer: " <<
			multi_round_encoded.error().message << '\n';
	}
	expect(multi_round_encoded.has_value(),
		"multi-round Story fixture encodes as strict V13");
	auto multi_round_commit = commit_replay_slot(
		root, 4U, multi_round_document
	);
	expect(
		multi_round_commit.has_value(),
		"multi-round Story fixture commits through the real V13 store"
	);
	if(multi_round_commit) {
		auto multi_round = NewClassicReplayBrowser::load(root, patch_assets);
		expect(multi_round.has_value(),
			"browser reloads the committed multi-round Story fixture");
		if(multi_round) {
			finish_entry_fade(**multi_round);
			for(std::size_t slot = 0U; slot < 4U; ++slot) {
				static_cast<void>((*multi_round)->consume({}));
				static_cast<void>((*multi_round)->consume({.down = true}));
			}
			static_cast<void>((*multi_round)->consume({}));
			auto opened = (*multi_round)->consume({.confirm = true});
			expect(opened && opened->surface_changed,
				"browser opens the later-round Story fixture");
			for(std::size_t checkpoint = 0U; checkpoint < 10U; ++checkpoint) {
				static_cast<void>((*multi_round)->consume({}));
				static_cast<void>((*multi_round)->consume({.down = true}));
			}
			static_cast<void>((*multi_round)->consume({}));
			auto selected = (*multi_round)->consume({.confirm = true});
			if(!selected || selected->status !=
				NewClassicReplayBrowserStatus::play_selected ||
				(*multi_round)->selected_checkpoint() != 10U) {
				std::cerr << "multi-round selection slot=" <<
					static_cast<unsigned>((*multi_round)->selected_slot()) <<
					" checkpoint=" << (*multi_round)->selected_checkpoint() <<
					" status=" << (selected ? static_cast<unsigned>(selected->status) :
						999U) << '\n';
			}
			expect(
				selected && selected->status ==
					NewClassicReplayBrowserStatus::play_selected &&
				(*multi_round)->selected_checkpoint() == 10U,
				"Story physical-round navigation launches checkpoint index 10"
			);
		}
	}

	{
		auto patch_browser = NewClassicReplayBrowser::load(root, patch_assets);
		std::optional<th03::present::SourceSurface> baseline_list{};
		expect(patch_browser.has_value(),
			"patch-enabled browser loads through authenticated AZINN assets");
		if(patch_browser) {
			const auto list = (*patch_browser)->surface();
			expect(
				list.pixels.size() == th03::present::graphics_width *
					th03::present::graphics_height &&
				!list.host_overlay,
				"patch browser publishes a source-domain 640x400 indexed list surface"
			);
			expect(list.palette_tone == 0,
				"patch browser begins on the source black-in boundary");
			for(std::size_t boundary = 0U; boundary < 19U; ++boundary) {
				static_cast<void>((*patch_browser)->consume({}));
			}
			baseline_list = (*patch_browser)->surface();
			expect((*patch_browser)->surface().palette_tone == 100,
				"patch browser completes palette_black_in(1) before input");
			expect(
				(*patch_browser)->surface().graphics_palette.colors[12U] ==
					th03::assets::Rgb4{15U, 15U, 0U},
				"patch browser applies the source yellow selection color"
			);
			static_cast<void>((*patch_browser)->consume({.down = true}));
			static_cast<void>((*patch_browser)->consume({}));
			static_cast<void>((*patch_browser)->consume({.down = true}));
			static_cast<void>((*patch_browser)->consume({}));
			expect(
				(*patch_browser)->consume({.confirm = true})->surface_changed &&
				NewClassicReplayBrowserTestAccess::physical_row_count(
					**patch_browser
				) == 9U,
				"patch Story detail publishes all nine source physical stage rows"
			);
		}
		auto pseudo_assets = pseudo_localized_patch_assets(patch_assets);
		auto pseudo_one = NewClassicReplayBrowser::load(root, pseudo_assets);
		auto pseudo_two = NewClassicReplayBrowser::load(root, pseudo_assets);
		expect(
			baseline_list && pseudo_one.has_value() && pseudo_two.has_value(),
			"pseudo-localized Replay browsers load through the same authenticated art"
		);
		if(baseline_list && pseudo_one && pseudo_two) {
			finish_entry_fade(**pseudo_one);
			finish_entry_fade(**pseudo_two);
			const auto first = (*pseudo_one)->surface();
			const auto second = (*pseudo_two)->surface();
			expect(
				surface_hash(first) == surface_hash(second) &&
				surface_hash(first) != surface_hash(*baseline_list),
				"pseudo-localized Replay list is deterministic and visibly replaces semantic text"
			);
			expect(
				first.shown_page == baseline_list->shown_page &&
				first.palette_tone == baseline_list->palette_tone &&
				first.graphics_palette == baseline_list->graphics_palette,
				"Replay localization preserves source page and palette ownership"
			);
		}

		NewClassicReplaySaveMenu patch_save(make_document(), patch_assets);
		expect(
			patch_save.surface().palette_tone == 0 &&
			!patch_save.surface().host_overlay,
			"patch save name scene uses authenticated SLB1B palette"
		);
		for(std::size_t boundary = 0U; boundary < 19U; ++boundary) {
			static_cast<void>(patch_save.consume({}, *catalog));
		}
		expect(patch_save.surface().palette_tone == 100,
			"patch save completes replay_name_background_init black-in");
		expect(
			patch_save.surface().graphics_palette ==
				*patch_assets->replay_name_background.palette,
			"patch save applies the authenticated SLB1B palette after black-in"
		);
		NewClassicReplaySaveTestAccess::select_end(patch_save);
		expect(
			patch_save.consume({.confirm = true}, *catalog).surface_changed &&
			NewClassicReplaySaveTestAccess::in_slots(patch_save),
			"patch save transitions from exact name topology to SLB1 slots"
		);
		expect(
			patch_save.surface().graphics_palette.colors[12U] ==
				th03::assets::Rgb4{15U, 15U, 0U},
			"patch save slot list installs Replay Patch selected palette 12"
		);

		auto prompt_document = make_document();
		prompt_document.metadata->end_reason =
			th03::formats::ReplayEndReason::game_over;
		NewClassicReplaySaveMenu baseline_prompt(prompt_document, patch_assets);
		NewClassicReplaySaveMenu localized_prompt(
			prompt_document, pseudo_localized_patch_assets(patch_assets)
		);
		finish_entry_fade(baseline_prompt, *catalog);
		finish_entry_fade(localized_prompt, *catalog);
		expect(
			surface_hash(localized_prompt.surface()) !=
				surface_hash(baseline_prompt.surface()),
			"pseudo-localized Replay save prompt replaces the source-domain dialog copy"
		);
		expect(
			localized_prompt.surface().shown_page ==
				baseline_prompt.surface().shown_page &&
			localized_prompt.surface().palette_tone ==
				baseline_prompt.surface().palette_tone &&
			localized_prompt.surface().graphics_palette ==
				baseline_prompt.surface().graphics_palette,
			"Replay save localization preserves dialog page and palette cadence"
		);
	}

	for(const auto [slot, practice] : std::array{
		std::pair<std::uint8_t, bool>{std::uint8_t{2U}, false},
		std::pair<std::uint8_t, bool>{std::uint8_t{3U}, true},
	}) {
		auto telemetry = NewClassicReplayBrowser::load(root, patch_assets);
		expect(telemetry.has_value(), "telemetry browser reload succeeds");
		if(!telemetry) {
			continue;
		}
		finish_entry_fade(**telemetry);
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
		if(practice) {
			auto settings = (*telemetry)->consume({.left = true});
			expect(
				settings && settings->surface_changed &&
				!NewClassicReplayBrowserTestAccess::checkpoint_focus(**telemetry),
				"Practice Splits Left moves focus to Settings"
			);
			static_cast<void>((*telemetry)->consume({}));
			auto modal = (*telemetry)->consume({.confirm = true});
			expect(
				modal && modal->surface_changed &&
				NewClassicReplayBrowserTestAccess::practice_settings_visible(
					**telemetry
				),
				"Practice Settings opens the complete read-only modal"
			);
			static_cast<void>((*telemetry)->consume({}));
			auto dismissed = (*telemetry)->consume({.bomb = true});
			expect(
				dismissed && dismissed->surface_changed &&
				!NewClassicReplayBrowserTestAccess::practice_settings_visible(
					**telemetry
				),
				"Practice Settings waits for release then closes on any input"
			);
			static_cast<void>((*telemetry)->consume({}));
			expect(
				(*telemetry)->consume({.right = true})->surface_changed &&
				NewClassicReplayBrowserTestAccess::checkpoint_focus(**telemetry) &&
				(*telemetry)->detail_page() ==
					NewClassicReplayDetailPage::splits,
				"Practice Settings Right restores checkpoint focus on Splits"
			);
			static_cast<void>((*telemetry)->consume({}));
		} else {
			expect(
				NewClassicReplayBrowserTestAccess::physical_row_count(**telemetry) ==
					9U,
				"Story detail preserves one physical row for every stage"
			);
		}
		expect(
			(*telemetry)->consume({.right = true})->surface_changed,
			practice ? "Practice clear telemetry renders" :
				"Story clear telemetry renders"
		);
	}

	std::uint16_t save_date = 0x5D0DU;
	NewClassicReplaySaveMenu direct(
		make_document(), patch_assets, {}, &injected_dos_date, &save_date
	);
	finish_entry_fade(direct, *catalog);
	expect(
		NewClassicReplaySaveTestAccess::in_name(direct),
		"Pause Save and Exit begins directly at the name editor"
	);
	expect(
		direct.document().metadata &&
		direct.document().metadata->dos_date == save_date,
		"direct Save and Exit captures the DOS date immediately before name entry"
	);
	const auto name_surface = direct.surface();
	release(direct, *catalog);
	expect(
		direct.consume({.cancel = true}, *catalog).surface_changed &&
		NewClassicReplaySaveTestAccess::in_quit_confirm(direct),
		"name Cancel opens the default-No quit confirmation"
	);
	const auto dialog_surface = direct.surface();
	bool preserved_outside_dialog =
		name_surface.pixels.size() == dialog_surface.pixels.size();
	for(std::size_t y = 0U; preserved_outside_dialog && y < 400U; ++y) {
		for(std::size_t x = 0U; x < 640U; ++x) {
			if(x >= 208U && x < 432U && y >= 160U && y < 240U) {
				continue;
			}
			const auto offset = (y * 640U) + x;
			if(name_surface.pixels[offset] != dialog_surface.pixels[offset]) {
				preserved_outside_dialog = false;
				break;
			}
		}
	}
	expect(
		preserved_outside_dialog,
		"name quit confirmation preserves the complete shown name screen outside the dialog"
	);
	release(direct, *catalog);
	expect(
		direct.consume({.confirm = true}, *catalog).surface_changed &&
		NewClassicReplaySaveTestAccess::in_name(direct),
		"quit confirmation No restores the exact name phase"
	);

	NewClassicReplaySaveMenu full_name(make_document(), patch_assets);
	finish_entry_fade(full_name, *catalog);
	release(full_name, *catalog);
	for(std::size_t index = 0U; index < 8U; ++index) {
		static_cast<void>(full_name.consume({.confirm = true}, *catalog));
		release(full_name, *catalog);
	}
	expect(
		NewClassicReplaySaveTestAccess::name_position(full_name) == 8U,
		"the eighth name character is retained and moves focus to End"
	);
	expect(
		full_name.consume({.confirm = true}, *catalog).surface_changed &&
		NewClassicReplaySaveTestAccess::in_slots(full_name),
		"automatic End focus accepts all eight name characters"
	);

	NewClassicReplaySaveMenu backspace(make_document(), patch_assets);
	finish_entry_fade(backspace, *catalog);
	release(backspace, *catalog);
	static_cast<void>(backspace.consume({.confirm = true}, *catalog));
	release(backspace, *catalog);
	static_cast<void>(backspace.consume({.bomb = true}, *catalog));
	expect(
		NewClassicReplaySaveTestAccess::name_position(backspace) == 0U &&
		backspace.edited_name()[0U] == ' ',
		"Bomb backspaces the replay name"
	);

	auto prompt_document = make_document();
	prompt_document.metadata->end_reason =
		th03::formats::ReplayEndReason::game_over;
	NewClassicReplaySaveMenu prompt_no(prompt_document, patch_assets);
	finish_entry_fade(prompt_no, *catalog);
	release(prompt_no, *catalog);
	static_cast<void>(prompt_no.consume({.down = true}, *catalog));
	release(prompt_no, *catalog);
	expect(
		prompt_no.consume({.confirm = true}, *catalog).status ==
			NewClassicReplaySaveStatus::discarded,
		"the post-run Save Replay prompt still discards immediately on No"
	);

	save_date = 0x5D0EU;
	NewClassicReplaySaveMenu save(
		prompt_document, patch_assets, {}, &injected_dos_date, &save_date
	);
	finish_entry_fade(save, *catalog);
	release(save, *catalog);
	expect(
		save.document().metadata &&
		save.document().metadata->dos_date != save_date,
		"post-run prompt does not capture the replay date before the save decision"
	);
	expect(
		save.consume({.confirm = true}, *catalog).surface_changed,
		"save prompt defaults to Yes and opens the name screen"
	);
	expect(
		save.document().metadata &&
		save.document().metadata->dos_date == save_date,
		"post-run save captures the DOS date on entry to the name screen"
	);
	release(save, *catalog);
	NewClassicReplaySaveTestAccess::select_end(save);
	expect(
		save.consume({.confirm = true}, *catalog).surface_changed,
		"name End advances to the slot list"
	);
	release(save, *catalog);
	NewClassicReplaySaveTestAccess::set_slot(save, 0U);
	static_cast<void>(save.consume({.left = true}, *catalog));
	expect(save.selected_slot() == 0U,
		"save-page Left clamps at slot zero");
	release(save, *catalog);
	NewClassicReplaySaveTestAccess::set_slot(save, 90U);
	static_cast<void>(save.consume({.right = true}, *catalog));
	expect(save.selected_slot() == 99U,
		"save-page Right clamps the last partial page at slot 99");
	release(save, *catalog);
	NewClassicReplaySaveTestAccess::set_slot(save, 4U);
	auto requested = save.consume({.confirm = true}, *catalog);
	expect(
		requested.status == NewClassicReplaySaveStatus::save_requested &&
		save.selected_slot() == 4U,
		"an empty slot requests exactly one atomic commit"
	);
	save.report_commit(false, *catalog);
	expect(save.commit_failed(), "failed commits retain the document for retry");
	static_cast<void>(save.consume({} , *catalog));
	expect(
		NewClassicReplaySaveTestAccess::in_slots(save),
		"failed commit returns immediately to the retryable slot list"
	);

	NewClassicReplaySaveMenu overwrite(make_document(), patch_assets);
	finish_entry_fade(overwrite, *catalog);
	release(overwrite, *catalog);
	static_cast<void>(overwrite.consume({.confirm = true}, *catalog));
	release(overwrite, *catalog);
	NewClassicReplaySaveTestAccess::select_end(overwrite);
	static_cast<void>(overwrite.consume({.confirm = true}, *catalog));
	release(overwrite, *catalog);
	const auto nested_dialog_start_page = overwrite.surface().shown_page;
	auto existing = overwrite.consume({.confirm = true}, *catalog);
	expect(existing.surface_changed,
		"occupied slot enters explicit overwrite confirmation");
	release(overwrite, *catalog);
	expect(
		overwrite.consume({.cancel = true}, *catalog).surface_changed &&
		NewClassicReplaySaveTestAccess::in_quit_confirm(overwrite),
		"overwrite Cancel opens quit confirmation without losing the slot"
	);
	release(overwrite, *catalog);
	expect(
		overwrite.consume({.confirm = true}, *catalog).surface_changed &&
		NewClassicReplaySaveTestAccess::in_slots(overwrite),
		"quit confirmation No dismisses overwrite and returns to the slot list"
	);
	expect(
		overwrite.surface().shown_page != nested_dialog_start_page,
		"nested overwrite and quit dialogs retain the source three-flip list-page identity"
	);
	release(overwrite, *catalog);
	static_cast<void>(overwrite.consume({.confirm = true}, *catalog));
	release(overwrite, *catalog);
	static_cast<void>(overwrite.consume({.up = true}, *catalog));
	release(overwrite, *catalog);
	expect(
		overwrite.consume({.confirm = true}, *catalog).status ==
			NewClassicReplaySaveStatus::save_requested,
		"overwrite requires selecting and confirming Yes"
	);
	overwrite.report_commit(true, *catalog);
	const auto saved_surface = overwrite.surface();
	bool saved_selected_row = false;
	for(std::size_t y = 96U; y < 112U; ++y) {
		for(std::size_t x = 40U; x < 600U; ++x) {
			saved_selected_row = saved_selected_row || saved_surface.pixels[
				(y * th03::present::graphics_width) + x
			].value == 12U;
		}
	}
	expect(
		saved_selected_row,
		"successful save keeps the refreshed selected slot row visible behind its acknowledgement"
	);
	expect(
		overwrite.consume({}, *catalog).status ==
			NewClassicReplaySaveStatus::running,
		"successful commit enters the source-compatible saved acknowledgement"
	);
	release(overwrite, *catalog);
	expect(
		overwrite.consume({.confirm = true}, *catalog).status ==
			NewClassicReplaySaveStatus::saved,
		"the saved acknowledgement waits for one released input"
	);

	NewClassicReplaySaveMenu invalid_slot(make_document(), patch_assets);
	finish_entry_fade(invalid_slot, *catalog);
	release(invalid_slot, *catalog);
	static_cast<void>(invalid_slot.consume({.confirm = true}, *catalog));
	release(invalid_slot, *catalog);
	NewClassicReplaySaveTestAccess::select_end(invalid_slot);
	static_cast<void>(invalid_slot.consume({.confirm = true}, *catalog));
	release(invalid_slot, *catalog);
	NewClassicReplaySaveTestAccess::set_slot(invalid_slot, 1U);
	expect(
		invalid_slot.consume({.confirm = true}, *catalog).surface_changed &&
		NewClassicReplaySaveTestAccess::in_overwrite(invalid_slot),
		"an unreadable occupied slot still requires overwrite confirmation"
	);
	std::filesystem::remove_all(root, ignored);
}

} // namespace

int main()
{
	test_online_high_score_menu();
	test_catalog_browser_and_save();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
