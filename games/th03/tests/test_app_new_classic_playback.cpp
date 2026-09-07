#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/new_classic/replay/new_classic_playback.hpp"
#include "th03/formats/v14.hpp"
#include "../src/app/classic/mainl/classic_mainl_stage_internal.hpp"
#include "../src/app/new_classic/replay/new_classic_playback_internal.hpp"

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

// Slot 07 from Replay Patch 0.4.6-rc8 exposed 21,200 replay-owned Pause
// samples.  Waiting one host frame per sample turned this transaction into a
// six-minute stall, so retain the same order of magnitude as a regression.
constexpr std::uint32_t recorded_pause_neutral_samples = 21'200U;
constexpr std::uint32_t recorded_pause_transaction_samples =
	recorded_pause_neutral_samples + 3U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

std::vector<std::byte> read_file(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if(!input) {
		return {};
	}
	const auto end = input.tellg();
	if(end <= 0) {
		return {};
	}
	std::vector<std::byte> result(static_cast<std::size_t>(end));
	input.seekg(0);
	input.read(
		reinterpret_cast<char *>(result.data()),
		static_cast<std::streamsize>(result.size())
	);
	return input ? result : std::vector<std::byte>{};
}

bool process_start_checkpoint(
	const th03::formats::ReplayDocument &document,
	std::size_t checkpoint_index
) noexcept
{
	const auto sample = document.checkpoints[checkpoint_index].sample_index;
	if(sample == 0U) {
		return true;
	}
	for(const auto &record : document.session.records) {
		const auto *boundary = std::get_if<th03::replay::ProcessBoundary>(&record);
		if(boundary && boundary->sample_index == sample &&
			boundary->completed_process == th03::replay::ProcessId::mainl) {
			return true;
		}
	}
	return false;
}

struct ReplayEntryStageTrace final {
	std::size_t ticks = 0U;
	bool saw_black_in = false;
	bool saw_waiting = false;
	bool saw_white_out = false;
	bool saw_white_in = false;
	std::vector<th03::app::ClassicAudioCommand> audio{};
};

ReplayEntryStageTrace finish_replay_entry_stage(
	th03::app::NewClassicReplayPlayback &playback
)
{
	ReplayEntryStageTrace trace{};
	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	while(const auto phase = th03::app::NewClassicReplayPlaybackTestAccess::
		replay_entry_stage_phase(playback)) {
		switch(*phase) {
		case th03::app::ClassicMainlStagePhase::black_in:
			trace.saw_black_in = true;
			break;
		case th03::app::ClassicMainlStagePhase::waiting:
			trace.saw_waiting = true;
			break;
		case th03::app::ClassicMainlStagePhase::white_out:
			trace.saw_white_out = true;
			break;
		case th03::app::ClassicMainlStagePhase::white_in:
			trace.saw_white_in = true;
			break;
		case th03::app::ClassicMainlStagePhase::ready:
		case th03::app::ClassicMainlStagePhase::consumed:
		case th03::app::ClassicMainlStagePhase::failed:
			break;
		}
		auto surface = playback.compose_shown_surface(std::span<
			th03::present::ComposedPixel,
			th03::present::graphics_page_pixels
		>(pixels.data(), pixels.size()));
		expect(surface.has_value(),
			"replay entry character interstitial composes every fade boundary");
		auto advanced = playback.advance({}, false, false);
		const bool gameplay_published =
			!th03::app::NewClassicReplayPlaybackTestAccess::
				replay_entry_stage_phase(playback);
		expect(advanced && advanced->surface_changed &&
			(advanced->process_advanced == gameplay_published),
			"replay entry advances only the handoff tick that publishes gameplay");
		if(!surface || !advanced || ++trace.ticks > 128U) {
			break;
		}
		while(auto command = playback.take_audio_command()) {
			trace.audio.push_back(*command);
		}
	}
	while(auto command = playback.take_audio_command()) {
		trace.audio.push_back(*command);
	}
	return trace;
}

std::optional<th03::formats::ReplayDocument> insert_recorded_pause(
	th03::formats::ReplayDocument document
)
{
	std::vector<th03::replay::SessionRecord> records;
	records.reserve(
		document.session.records.size() + recorded_pause_transaction_samples
	);
	std::optional<std::uint32_t> pause_sample{};
	for(auto record : document.session.records) {
		if(!pause_sample) {
			if(auto *sample = std::get_if<th03::replay::InputSample>(&record);
				sample && sample->phase == th03::replay::InputPhase::gameplay) {
				pause_sample = sample->frame_index;
				sample->input.system = static_cast<std::uint16_t>(
					sample->input.system | 0x1000U
				);
				records.push_back(record);
				records.push_back(th03::replay::InputSample{
					.frame_index = *pause_sample + 1U,
					.phase = th03::replay::InputPhase::interstitial,
					.round_frame = std::nullopt,
					.round_or_result_frame = std::nullopt,
				});
				for(std::uint32_t index = 0U;
					index < recorded_pause_neutral_samples; ++index) {
					records.push_back(th03::replay::InputSample{
						.frame_index = *pause_sample + 2U + index,
						.phase = th03::replay::InputPhase::interstitial,
						.round_frame = std::nullopt,
						.round_or_result_frame = std::nullopt,
					});
				}
				records.push_back(th03::replay::InputSample{
					.frame_index = *pause_sample + 2U +
						recorded_pause_neutral_samples,
					.phase = th03::replay::InputPhase::interstitial,
					.input = {.system = 0x1000U},
					.round_frame = std::nullopt,
					.round_or_result_frame = std::nullopt,
				});
				records.push_back(th03::replay::InputSample{
					.frame_index = *pause_sample + 3U +
						recorded_pause_neutral_samples,
					.phase = th03::replay::InputPhase::interstitial,
					.round_frame = std::nullopt,
					.round_or_result_frame = std::nullopt,
				});
				continue;
			}
			records.push_back(std::move(record));
			continue;
		}
		if(auto *sample = std::get_if<th03::replay::InputSample>(&record)) {
			sample->frame_index += recorded_pause_transaction_samples;
		} else {
			std::get<th03::replay::ProcessBoundary>(record).sample_index +=
				recorded_pause_transaction_samples;
		}
		records.push_back(std::move(record));
	}
	if(!pause_sample) {
		return std::nullopt;
	}
	document.session.records = std::move(records);
	for(auto &checkpoint : document.checkpoints) {
		if(checkpoint.sample_index > *pause_sample) {
			checkpoint.sample_index += recorded_pause_transaction_samples;
		}
	}
	if(document.metadata) {
		document.metadata->sample_count += recorded_pause_transaction_samples;
	}
	return document;
}

void test_exact_main_replay_overlay()
{
	th03::present::Raster raster{};
	th03::present::Tram tram{};
	tram.fill({' ', th03::present::tram_white});
	for(const auto page : {th03::present::Page::zero, th03::present::Page::one}) {
		expect(raster.set_access_page(page),
			"overlay fixture selects each graphics page");
		expect(raster.clear({0, 0, 640U, 24U},
			page == th03::present::Page::zero ? 2U : 3U),
			"overlay fixture initializes distinguishable graphics pages");
	}
	expect(raster.set_shown_page(th03::present::Page::one),
		"overlay fixture selects page 1 as the front page");
	expect(raster.set_access_page(th03::present::Page::zero),
		"overlay fixture starts with a non-front access page");
	expect(
		th03::app::NewClassicReplayPlaybackTestAccess::put_main_replay_overlay(
			raster, tram
		),
		"Replay Patch MAIN overlay writes its native Raster/TRAM state"
	);
	for(const auto page : {th03::present::Page::zero, th03::present::Page::one}) {
		for(std::int32_t y = 0; y < 8; ++y) {
			for(std::int32_t x = 296; x <= 343; ++x) {
				expect(raster.pixel(page, {x, y}) == 15U,
					"Replay backing is white across the exact 48x8 box on both pages");
			}
		}
		expect(raster.pixel(page, {295, 0}) ==
			(page == th03::present::Page::zero ? 2U : 3U),
			"Replay backing preserves the left neighbor on each page");
		expect(raster.pixel(page, {344, 7}) ==
			(page == th03::present::Page::zero ? 2U : 3U),
			"Replay backing preserves the right neighbor on each page");
		expect(raster.pixel(page, {296, 8}) ==
			(page == th03::present::Page::zero ? 2U : 3U),
			"Replay backing preserves the row below on each page");
	}
	expect(raster.access_page() == th03::present::Page::zero &&
		raster.shown_page() == th03::present::Page::one,
		"post-footer Replay overlay restores graphics access to the hidden page");
	constexpr std::array<char, 6U> replay{{'R', 'E', 'P', 'L', 'A', 'Y'}};
	for(std::size_t index = 0U; index < replay.size(); ++index) {
		const auto cell = tram.cell({
			37 + static_cast<std::int32_t>(index), 0
		});
		expect(cell && cell->code == static_cast<std::uint16_t>(replay[index]) &&
			cell->attribute == 0x0005U,
			"Replay overlay writes exact reversed-black ANK cells");
	}
	expect(tram.cell({36, 0}) == th03::present::TramCell{
		' ', th03::present::tram_white
	} && tram.cell({43, 0}) == th03::present::TramCell{
		' ', th03::present::tram_white
	}, "Replay overlay preserves adjacent TRAM cells and emits no FF label");
}

void test_invalid_request()
{
	auto invalid = th03::app::NewClassicReplayPlayback::load({});
	expect(
		!invalid && invalid.error().code ==
			th03::app::NewClassicPlaybackErrorCode::invalid_request,
		"playback rejects an empty document and content request"
	);
}

void test_mainl_end_input_ownership()
{
	const auto same_input = [](const th03::CoreInput &left,
		const th03::CoreInput &right) noexcept {
		return left.p1 == right.p1 && left.p2 == right.p2 &&
			left.system == right.system &&
			left.charge_mask == right.charge_mask;
	};
	const th03::CoreInput replay_input{
		.p1 = 0x0001U,
		.p2 = 0x0002U,
		.system = 0x0020U,
		.charge_mask = 0x01U,
	};
	const th03::CoreInput held_live_input{
		.p1 = 0x0004U,
		.p2 = 0x0008U,
		.system = 0x2000U,
		.charge_mask = 0x02U,
	};
	expect(
		same_input(
			th03::app::NewClassicReplayPlaybackTestAccess::mainl_end_input(
				replay_input, held_live_input, false, false
			), replay_input
		),
		"MAINL_END can reassert the last replay sample and discards held live input"
	);
	auto released_replay_input = replay_input;
	released_replay_input.system = 0U;
	expect(
		same_input(
			th03::app::NewClassicReplayPlaybackTestAccess::mainl_end_input(
				replay_input, held_live_input, false, true
			), released_replay_input
		),
		"MAINL_END can release the final replay key before reasserting it"
	);
	expect(
		same_input(
			th03::app::NewClassicReplayPlaybackTestAccess::mainl_end_input(
				replay_input, held_live_input, true, true
			), held_live_input
		),
		"registration remains on the donor's live input interface after MAINL_END"
	);
}

void test_physical_abort_resident(
	const std::filesystem::path &data,
	const std::filesystem::path &main,
	const std::filesystem::path &font,
	const std::filesystem::path &score,
	const th03::formats::ReplayDocument &document
)
{
	auto abort_probe = th03::app::NewClassicReplayPlayback::load({
		.content = {
			.data_root = data,
			.main_executable = main,
			.mainl_executable = main.parent_path() / "MAINL.EXE",
			.font_rom = font,
			.score = score,
		},
		.document = document,
		.checkpoint_index = 0U,
	});
	expect(abort_probe.has_value(),
		"physical-Esc resident probe constructs from checkpoint 0");
	if(!abort_probe) {
		return;
	}
	static_cast<void>(finish_replay_entry_stage(**abort_probe));
	std::size_t guard = 0U;
	while(guard++ < 1024U &&
		th03::app::NewClassicReplayPlaybackTestAccess::main_input_context(
			**abort_probe
		) != static_cast<std::uint8_t>(th03::InputContext::gameplay)) {
		auto advanced_to_gameplay = (*abort_probe)->advance({}, false, false);
		expect(advanced_to_gameplay.has_value(),
			"physical-Esc probe reaches a gameplay input boundary");
		if(!advanced_to_gameplay) {
			return;
		}
		while((*abort_probe)->take_audio_command()) {
		}
	}
	const auto abort_sample = (*abort_probe)->sample_index();
	const auto pre_abort_round_frame =
		th03::app::NewClassicReplayPlaybackTestAccess::main_round_frame(
			**abort_probe
		);
	expect(pre_abort_round_frame.has_value(),
		"physical-Esc probe observes the live pre-suffix round frame");
	auto aborted = (*abort_probe)->advance(
		{}, true, false, {.p1 = 0x0004U, .p2 = 0x0008U, .system = 0x1000U}
	);
	expect(aborted && aborted->status ==
		th03::app::NewClassicPlaybackStatus::aborted &&
		(*abort_probe)->sample_index() == abort_sample,
		"physical Esc completes one neutral MAIN frame without consuming replay input");
	auto abort_resident = (*abort_probe)->take_exit_resident();
	if(pre_abort_round_frame) {
		const auto expected_seed = *pre_abort_round_frame + 1U;
		expect(abort_resident &&
			static_cast<std::uint32_t>(abort_resident->resident_rand) ==
				expected_seed &&
			abort_resident->random_seed == expected_seed,
			"physical Esc projects the donor post-suffix round-frame seed");
	}
	const auto live_suffix_hash =
		th03::app::NewClassicReplayPlaybackTestAccess::main_state_hash(
			**abort_probe
		);
	auto neutral_probe = th03::app::NewClassicReplayPlayback::load({
		.content = {
			.data_root = data,
			.main_executable = main,
			.mainl_executable = main.parent_path() / "MAINL.EXE",
			.font_rom = font,
			.score = score,
		},
		.document = document,
		.checkpoint_index = 0U,
	});
	expect(neutral_probe.has_value(),
		"neutral Esc-suffix comparison constructs from checkpoint 0");
	if(neutral_probe) {
		static_cast<void>(finish_replay_entry_stage(**neutral_probe));
		std::size_t neutral_guard = 0U;
		while(neutral_guard++ < 1024U &&
			((*neutral_probe)->sample_index() != abort_sample ||
			 th03::app::NewClassicReplayPlaybackTestAccess::main_input_context(
				**neutral_probe
			 ) != static_cast<std::uint8_t>(th03::InputContext::gameplay))) {
			auto advanced = (*neutral_probe)->advance({}, false, false);
			expect(advanced.has_value(),
				"neutral Esc-suffix comparison reaches the identical boundary");
			if(!advanced) {
				break;
			}
			while((*neutral_probe)->take_audio_command()) {
			}
		}
		auto neutral_abort = (*neutral_probe)->advance({}, true, false);
		const auto neutral_suffix_hash =
			th03::app::NewClassicReplayPlaybackTestAccess::main_state_hash(
				**neutral_probe
			);
		expect(neutral_abort && live_suffix_hash && neutral_suffix_hash &&
			*live_suffix_hash != *neutral_suffix_hash,
			"physical Esc clears only system input and preserves live P1/P2 through the trailing MAIN suffix");
	}
}

void test_mainl_abort_forced_ok_progression(
	const std::filesystem::path &data,
	const std::filesystem::path &main,
	const std::filesystem::path &font,
	const std::filesystem::path &score,
	const th03::formats::ReplayDocument &document
)
{
	auto probe = th03::app::NewClassicReplayPlayback::load({
		.content = {
			.data_root = data,
			.main_executable = main,
			.mainl_executable = main.parent_path() / "MAINL.EXE",
			.font_rom = font,
			.score = score,
		},
		.document = document,
		.checkpoint_index = 0U,
	});
	expect(probe.has_value(),
		"MAINL physical-Esc progression constructs from checkpoint 0");
	if(!probe) {
		return;
	}
	while((*probe)->take_audio_command()) {
	}
	std::size_t reach_guard = 0U;
	while(reach_guard++ < 150000U &&
		th03::app::NewClassicReplayPlaybackTestAccess::owner(**probe) !=
			th03::app::NewClassicPlaybackOwner::story_win) {
		auto advanced = (*probe)->advance({}, false, false);
		if(!advanced) {
			expect(false,
				"MAINL physical-Esc progression reaches an authentic win owner");
			return;
		}
		while((*probe)->take_audio_command()) {
		}
	}
	expect(reach_guard < 150000U,
		"MAINL physical-Esc progression reaches an authentic win owner");
	if(reach_guard >= 150000U) {
		return;
	}
	const auto abort_sample = (*probe)->sample_index();
	auto first = (*probe)->advance(
		{}, true, false, {.system = 0x1000U}
	);
	expect(first && first->status ==
		th03::app::NewClassicPlaybackStatus::running &&
		th03::app::NewClassicReplayPlaybackTestAccess::
			mainl_error_progression_active(**probe) &&
		(*probe)->sample_index() == abort_sample,
		"MAINL physical Esc enters MR_ERROR forced-OK progression without teleporting or consuming replay input");
	bool surface_changed = first && first->surface_changed;
	bool stop_seen = false;
	std::size_t progression_ticks = 0U;
	for(; progression_ticks < 50000U && (*probe)->status() ==
		th03::app::NewClassicPlaybackStatus::running; ++progression_ticks) {
		auto advanced = (*probe)->advance({}, false, false);
		if(!advanced) {
			expect(false,
				"MAINL MR_ERROR forced-OK progression advances its retained owner");
			return;
		}
		surface_changed = surface_changed || advanced->surface_changed;
		while(auto command = (*probe)->take_audio_command()) {
			stop_seen = stop_seen || command->type ==
				th03::app::ClassicAudioCommandType::stop_song;
		}
	}
	expect(progression_ticks > 1U && surface_changed &&
		(*probe)->status() == th03::app::NewClassicPlaybackStatus::aborted &&
		(*probe)->sample_index() == abort_sample,
		"MAINL MR_ERROR visibly progresses under forced OK before reaching the normal exit boundary");
	expect(stop_seen,
		"completed MAINL physical-Esc progression stops the retained playback song");
	auto resident = (*probe)->take_exit_resident();
	expect(resident && resident->game_mode == th03::game_mode::none,
		"MAINL MR_ERROR progression exports the fresh-OP resident only at its normal exit");
}

void test_authentic_interstitials(
	const std::filesystem::path &data,
	const std::filesystem::path &main,
	const std::filesystem::path &font,
	const std::filesystem::path &replay
)
{
	auto document = th03::formats::parse_v14_replay(read_file(replay));
	expect(document && document->checkpoints.size() > 1U,
		"interstitial gate parses a multi-stage authentic V14 replay");
	if(!document || document->checkpoints.size() <= 1U) {
		return;
	}
	const auto score_directory = std::filesystem::temp_directory_path() /
		("th03-new-classic-interstitial-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()
		));
	std::error_code filesystem_error{};
	std::filesystem::create_directories(score_directory, filesystem_error);
	expect(!filesystem_error,
		"interstitial gate creates isolated persistence");
	const auto request = [&](std::size_t checkpoint_index) {
		return th03::app::NewClassicPlaybackRequest{
			.content = {
				.data_root = data,
				.main_executable = main,
				.mainl_executable = main.parent_path() / "MAINL.EXE",
				.font_rom = font,
				.score = score_directory / "YUME.NEM",
			},
			.document = *document,
			.checkpoint_index = checkpoint_index,
		};
	};
	auto playback = th03::app::NewClassicReplayPlayback::load(request(0U));
	expect(playback &&
		th03::app::NewClassicReplayPlaybackTestAccess::owner(**playback) ==
			th03::app::NewClassicPlaybackOwner::story_stage_splash,
		"checkpoint 0 enters MAINL's character interstitial");
	if(!playback) {
		std::filesystem::remove_all(score_directory, filesystem_error);
		return;
	}
	const auto entry = finish_replay_entry_stage(**playback);
	expect(entry.ticks == 87U && entry.saw_black_in && entry.saw_waiting &&
		entry.saw_white_out && entry.saw_white_in &&
		(*playback)->sample_index() == 1U,
		"checkpoint 0 publishes the exact complete entry transition and first gameplay frame");

	auto later = th03::app::NewClassicReplayPlayback::load(request(1U));
	expect(later && (*later)->sample_index() ==
		document->checkpoints[1U].sample_index,
		"later-stage selection lands at its exact checkpoint before entry");
	if(later) {
		const auto later_entry = finish_replay_entry_stage(**later);
		expect(later_entry.ticks == 87U && later_entry.saw_white_out &&
			later_entry.saw_white_in && (*later)->sample_index() ==
				document->checkpoints[1U].sample_index + 1U,
			"later-stage selection completes the entry fade and first gameplay frame");
	}

	bool failed = false;
	std::size_t guard = 0U;
	while(guard++ < 300000U && (*playback)->status() ==
		th03::app::NewClassicPlaybackStatus::running) {
		auto advanced = (*playback)->advance({}, false, false);
		if(!advanced) {
			failed = true;
			break;
		}
		while((*playback)->take_audio_command()) {
		}
	}
	const auto fade_drain = th03::app::NewClassicReplayPlaybackTestAccess::
		stage_splash_end_drain_ticks(**playback);
	expect(!failed && guard < 300000U && (*playback)->status() ==
			th03::app::NewClassicPlaybackStatus::complete,
		"interstitial gate reaches the authentic replay terminal boundary");
	expect(fade_drain >= 36U && (fade_drain % 36U) == 0U,
		"every natural later-stage interstitial drains complete 18-step white fades");
	std::filesystem::remove_all(score_directory, filesystem_error);
	expect(!filesystem_error,
		"interstitial gate removes isolated persistence");
}

void test_authentic(
	const std::filesystem::path &data,
	const std::filesystem::path &main,
	const std::filesystem::path &font,
	const std::filesystem::path &replay
)
{
	auto bytes = read_file(replay);
	auto document = th03::formats::parse_v14_replay(bytes);
	expect(document.has_value(), "authentic V14 replay parses for visual playback");
	if(!document) {
		return;
	}
	struct AcceleratorExpectation {
		std::uint8_t checkpoint;
		std::uint8_t page_back;
		std::uint8_t page_front;
		std::int8_t shared_velocity_x;
		std::int16_t background_x;
		std::int16_t background_y;
		std::int16_t background_velocity_y;
		th03::formats::ReplayAcceleratorRgb8 palette_zero;
	};
	constexpr std::array accelerator_expectations{
		AcceleratorExpectation{8U, 0U, 1U, 0, 3'614, 5'701, 86, {0x22U, 0x22U, 0U}},
		AcceleratorExpectation{10U, 1U, 0U, 80, 7'821, 3'084, 82, {0x55U, 0U, 0U}},
		AcceleratorExpectation{11U, 0U, 1U, 0, 6'269, 3'195, 82, {0x55U, 0U, 0U}},
		AcceleratorExpectation{12U, 0U, 1U, 0, 5'569, 2'359, 82, {0x55U, 0U, 0U}},
	};
	expect(
		document->accelerators.size() == accelerator_expectations.size(),
		"authentic V14 exposes exactly the four donor-valid accelerators"
	);
	for(const auto &expected : accelerator_expectations) {
		const auto accelerator = std::find_if(
			document->accelerators.begin(), document->accelerators.end(),
			[checkpoint = expected.checkpoint](const auto &candidate) {
				return candidate.checkpoint == checkpoint;
			}
		);
		expect(
			accelerator != document->accelerators.end() &&
			accelerator->page_back == expected.page_back &&
			accelerator->page_front == expected.page_front &&
			accelerator->shared_velocity_x_q4_4 == expected.shared_velocity_x &&
			accelerator->palette[0U] == expected.palette_zero &&
			accelerator->background_direction == 0x40U &&
			accelerator->background[26U].x_q12_4 == expected.background_x &&
			accelerator->background[26U].y_q12_4 == expected.background_y &&
			accelerator->background[26U].velocity_x_q12_4 == 0 &&
			accelerator->background[26U].velocity_y_q12_4 ==
				expected.background_velocity_y &&
			accelerator->background[26U].speed_q12_4 ==
				static_cast<std::uint8_t>(expected.background_velocity_y) &&
			accelerator->background[26U].sprite_variant == 0U,
			"authentic V14 preserves accelerator palette, page, velocity, and background state"
		);
	}
	const auto later_document = *document;
	auto paused_document = insert_recorded_pause(*document);
	const auto later_checkpoint = later_document.checkpoints.size() - 1U;
	const auto later_sample =
		later_document.checkpoints[later_checkpoint].sample_index;
	const auto mainl = main.parent_path() / "MAINL.EXE";
	const auto score_directory = std::filesystem::temp_directory_path() /
		("th03-new-classic-playback-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()
		));
	std::error_code filesystem_error{};
	std::filesystem::create_directories(score_directory, filesystem_error);
	expect(!filesystem_error,
		"authentic playback test creates an isolated persistence directory");
	const auto score = score_directory / "YUME.NEM";
	const auto new_score = score_directory / "YUME.NEW.NEM";
	auto loaded = th03::app::NewClassicReplayPlayback::load({
		.content = {
			.data_root = data,
			.main_executable = main,
			.mainl_executable = mainl,
			.font_rom = font,
			.score = score,
		},
		.document = std::move(*document),
		.checkpoint_index = 0U,
	});
	expect(loaded.has_value(),
		"authentic replay constructs the shared MAIN presenter from checkpoint 0");
	if(!loaded) {
		std::cerr << "playback-error=" << static_cast<unsigned>(loaded.error().code);
		if(loaded.error().process) {
			std::cerr << " process-error=" <<
				static_cast<unsigned>(loaded.error().process->code);
		}
		if(loaded.error().resource) {
			std::cerr << " resource-error=" <<
				static_cast<unsigned>(loaded.error().resource->code);
		}
		std::cerr << '\n';
		return;
	}
	expect(
		th03::app::NewClassicReplayPlaybackTestAccess::owner(**loaded) ==
			th03::app::NewClassicPlaybackOwner::story_stage_splash &&
		(*loaded)->sample_index() == 0U,
		"Stage 1 playback enters MAINL's character interstitial before MAIN"
	);
	const auto entry_trace = finish_replay_entry_stage(**loaded);
	expect(
		entry_trace.ticks == 87U && entry_trace.saw_black_in &&
			entry_trace.saw_waiting && entry_trace.saw_white_out &&
			entry_trace.saw_white_in && (*loaded)->sample_index() == 1U &&
			th03::app::NewClassicReplayPlaybackTestAccess::owner(**loaded) ==
				th03::app::NewClassicPlaybackOwner::main,
		"Stage 1 character interstitial keeps the mandatory hold and complete black/white fade before publishing the first gameplay frame"
	);
	expect(
		entry_trace.audio.size() >= 5U &&
		entry_trace.audio[0U].type ==
			th03::app::ClassicAudioCommandType::stop_song &&
		entry_trace.audio[1U].type ==
			th03::app::ClassicAudioCommandType::load_song &&
		entry_trace.audio[2U].type ==
			th03::app::ClassicAudioCommandType::load_effects &&
		entry_trace.audio[3U].type ==
			th03::app::ClassicAudioCommandType::reset_effects &&
		entry_trace.audio[4U].type ==
			th03::app::ClassicAudioCommandType::play_song,
		"entry MAINL prepares gameplay audio before MAIN resets effects, starts the song, and emits any first-frame feedback"
	);
	test_physical_abort_resident(data, main, font, score, later_document);
	test_mainl_abort_forced_ok_progression(
		data, main, font, score, later_document
	);
	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	auto surface = (*loaded)->compose_shown_surface(std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(pixels.data(), pixels.size()));
	expect(surface.has_value(),
		"visual playback composes the shared MAIN surface and replay overlay after the entry fade");
	if(surface) {
		expect(!surface->host_overlay,
			"Replay indicator remains native source state, not a host overlay");
		const auto released_pixels = pixels;
		(*loaded)->set_fast_forward(true);
		std::vector<th03::present::ComposedPixel> held_pixels(
			th03::present::graphics_page_pixels
		);
		auto held = (*loaded)->compose_shown_surface(std::span<
			th03::present::ComposedPixel,
			th03::present::graphics_page_pixels
		>(held_pixels.data(), held_pixels.size()));
		expect(held && held_pixels == released_pixels,
			"physical-Z fast-forward changes cadence without adding FF pixels");
		(*loaded)->set_fast_forward(false);
	}
	std::size_t advanced = 0U;
	for(std::size_t tick = 0U; tick < 240U &&
		(*loaded)->status() == th03::app::NewClassicPlaybackStatus::running;
		++tick) {
		auto result = (*loaded)->advance({}, false, false);
		expect(result.has_value(),
			"authentic recorded sample advances without replay-contract failure");
		if(!result) {
			break;
		}
		advanced += result->process_advanced ? 1U : 0U;
		while((*loaded)->take_audio_command()) {
		}
	}
	expect(advanced != 0U && (*loaded)->sample_index() != 0U,
		"authentic playback advances recorded MAIN state and sample cursor");
	for(std::size_t checkpoint_index = 1U;
		checkpoint_index < later_document.checkpoints.size(); ++checkpoint_index) {
		if(later_document.checkpoints[checkpoint_index].round == 0U) {
			continue;
		}
		std::size_t anchor_index = checkpoint_index;
		while(anchor_index != 0U &&
			!process_start_checkpoint(later_document, anchor_index)) {
			--anchor_index;
		}
		const auto checkpoint_sample =
			later_document.checkpoints[checkpoint_index].sample_index;
		const auto anchor_sample =
			later_document.checkpoints[anchor_index].sample_index;
		auto later = th03::app::NewClassicReplayPlayback::load({
			.content = {
				.data_root = data,
				.main_executable = main,
				.mainl_executable = mainl,
				.font_rom = font,
				.score = score,
			},
			.document = later_document,
			.checkpoint_index = checkpoint_index,
		});
		expect(
			later && (*later)->sample_index() == checkpoint_sample,
			"later checkpoint lands on its exact selected-round cursor"
		);
		if(later) {
			const auto later_entry = finish_replay_entry_stage(**later);
			expect(
				later_entry.ticks == 87U && later_entry.saw_black_in &&
					later_entry.saw_waiting && later_entry.saw_white_out &&
					later_entry.saw_white_in &&
					(*later)->sample_index() == checkpoint_sample + 1U,
				"every selected replay checkpoint shows the complete MAINL character interstitial before publishing its first gameplay frame"
			);
			expect(
				th03::app::NewClassicReplayPlaybackTestAccess::
					preroll_sample_count(**later) ==
						checkpoint_sample - anchor_sample,
				"later checkpoint reconstructs exactly the process-anchor sample span"
			);
			expect(
				th03::app::NewClassicReplayPlaybackTestAccess::
					main_round_frame(**later) == 1U,
				"later checkpoint first becomes public after the selected round's first frame"
			);
			const auto expected_song = th03::app::classic_main_gameplay_song(
				later_document.checkpoints[checkpoint_index].init
			);
			auto expected_play = expected_song.value_or(
				th03::app::ClassicAudioCommand{}
			);
			expected_play.type = th03::app::ClassicAudioCommandType::play_song;
			const std::vector expected_audio_prefix{
				th03::app::ClassicAudioCommand{
					.type = th03::app::ClassicAudioCommandType::stop_song,
				},
				*expected_song,
				th03::app::ClassicAudioCommand{
					.type = th03::app::ClassicAudioCommandType::load_effects,
				},
				th03::app::ClassicAudioCommand{
					.type = th03::app::ClassicAudioCommandType::reset_effects,
				},
				expected_play,
			};
			expect(
				expected_song &&
				later_entry.audio.size() >= expected_audio_prefix.size() &&
				std::equal(
					expected_audio_prefix.begin(), expected_audio_prefix.end(),
					later_entry.audio.begin()
				),
				"selected later checkpoint prepares gameplay audio before any first-frame feedback"
			);
			std::vector<th03::present::ComposedPixel> later_pixels(
				th03::present::graphics_page_pixels
			);
			th03::present::set_source_surface_oracle_hashes_enabled(true);
			auto later_surface = (*later)->compose_shown_surface(std::span<
					th03::present::ComposedPixel,
					th03::present::graphics_page_pixels
				>(later_pixels.data(), later_pixels.size()));
			th03::present::set_source_surface_oracle_hashes_enabled(false);
			const auto accelerator = std::find_if(
				later_document.accelerators.begin(), later_document.accelerators.end(),
				[checkpoint_index](
					const th03::formats::ReplayAcceleratorPresentation &candidate
				) {
					return candidate.checkpoint == checkpoint_index;
				}
			);
			const auto expected_page =
				accelerator != later_document.accelerators.end() &&
				accelerator->page_back != 0U
					? th03::present::Page::one : th03::present::Page::zero;
			expect(
				later_surface && later_surface->shown_page == expected_page,
				"later checkpoint publishes the accelerator-selected source page"
			);
			constexpr std::array<std::pair<std::uint32_t, std::uint64_t>, 4U>
				donor_raster_hashes{{
					{76'175U, 0x2E2787733E987193ULL},
					{133'776U, 0xC1BAFD32B872E7EEULL},
					{143'167U, 0x518E81F36E0CD7C8ULL},
					{153'036U, 0xDD9CC3054A042367ULL},
				}};
			constexpr std::array<std::pair<std::uint32_t, std::uint64_t>, 4U>
				donor_tram_hashes{{
					{76'175U, 0xEFAB2904C7124669ULL},
					{133'776U, 0x4AF3820FA73BA41EULL},
					{143'167U, 0x4AF3820FA73BA41EULL},
					{153'036U, 0x4AF3820FA73BA41EULL},
				}};
			for(const auto &[sample, expected_hash] : donor_raster_hashes) {
				if(checkpoint_sample == sample) {
					expect(
						later_surface && later_surface->oracle_hashes &&
							later_surface->oracle_hashes->raster_display == expected_hash,
						"accelerated checkpoint Raster matches captured 0.4.14 state"
					);
				}
			}
			for(const auto &[sample, expected_hash] : donor_tram_hashes) {
				if(checkpoint_sample == sample) {
					expect(
						later_surface && later_surface->oracle_hashes &&
						later_surface->oracle_hashes->tram_visible == expected_hash,
						"accelerated checkpoint static HUD matches captured 0.4.14 TRAM"
					);
				}
			}
			auto first = (*later)->advance({}, false, false);
			expect(
				first && first->process_advanced &&
					(*later)->sample_index() == checkpoint_sample + 2U,
				"next visible advance consumes the selected round's second sample"
			);
		}
	}
	const auto reference_checkpoint = std::find_if(
		later_document.checkpoints.begin(),
		later_document.checkpoints.end(),
		[](const th03::formats::ReplayCheckpoint &checkpoint) {
			return checkpoint.sample_index == 133'776U;
		}
	);
	if(reference_checkpoint != later_document.checkpoints.end()) {
		const auto checkpoint_index = static_cast<std::size_t>(std::distance(
			later_document.checkpoints.begin(), reference_checkpoint
		));
		const auto playback_request = [&]() {
			return th03::app::NewClassicPlaybackRequest{
				.content = {
					.data_root = data,
					.main_executable = main,
					.mainl_executable = mainl,
					.font_rom = font,
					.score = score,
				},
				.document = later_document,
				.checkpoint_index = checkpoint_index,
			};
		};
		auto optimized = th03::app::NewClassicReplayPlayback::load(
			playback_request()
		);
		auto full = th03::app::NewClassicReplayPlaybackTestAccess::
			load_with_full_preroll(playback_request());
		expect(optimized && full,
			"suppressed and full-presenter checkpoint reconstruction both load");
		if(optimized && full) {
			const auto optimized_entry = finish_replay_entry_stage(**optimized);
			const auto full_entry = finish_replay_entry_stage(**full);
			expect(
				optimized_entry.ticks == 87U &&
				full_entry.ticks == optimized_entry.ticks &&
				full_entry.audio == optimized_entry.audio,
				"suppressed and full-presenter paths complete the same replay entry transition"
			);
			std::vector<th03::present::ComposedPixel> optimized_pixels(
				th03::present::graphics_page_pixels
			);
			std::vector<th03::present::ComposedPixel> full_pixels(
				th03::present::graphics_page_pixels
			);
			for(std::size_t frame = 0U; frame <= 120U; ++frame) {
				auto optimized_surface = (*optimized)->compose_shown_surface(
					std::span<
						th03::present::ComposedPixel,
						th03::present::graphics_page_pixels
					>(optimized_pixels.data(), optimized_pixels.size())
				);
				auto full_surface = (*full)->compose_shown_surface(std::span<
					th03::present::ComposedPixel,
					th03::present::graphics_page_pixels
				>(full_pixels.data(), full_pixels.size()));
				expect(
					optimized_surface && full_surface &&
					optimized_pixels == full_pixels &&
					(*optimized)->sample_index() == (*full)->sample_index() &&
					th03::app::NewClassicReplayPlaybackTestAccess::owner(
						**optimized
					) == th03::app::NewClassicReplayPlaybackTestAccess::owner(
						**full
					) &&
					th03::app::NewClassicReplayPlaybackTestAccess::main_state_hash(
						**optimized
					) == th03::app::NewClassicReplayPlaybackTestAccess::main_state_hash(
						**full
					),
					"suppressed preroll remains pixel/state-exact for 120 visible Stage 9 Round 2 frames"
				);
				if(frame == 120U || !optimized_surface || !full_surface) {
					break;
				}
				auto optimized_tick = (*optimized)->advance({}, false, false);
				auto full_tick = (*full)->advance({}, false, false);
				expect(
					optimized_tick && full_tick &&
					optimized_tick->surface_changed == full_tick->surface_changed &&
					optimized_tick->process_advanced == full_tick->process_advanced &&
					optimized_tick->status == full_tick->status,
					"suppressed preroll preserves the full-presenter visible tick trajectory"
				);
				while((*optimized)->take_audio_command()) {
				}
				while((*full)->take_audio_command()) {
				}
				if(!optimized_tick || !full_tick) {
					break;
				}
			}
		}
	}
	if(paused_document) {
		auto paused = th03::app::NewClassicReplayPlayback::load({
			.content = {
				.data_root = data,
				.main_executable = main,
				.mainl_executable = mainl,
				.font_rom = font,
				.score = score,
			},
			.document = std::move(*paused_document),
			.checkpoint_index = 0U,
		});
		expect(paused.has_value(),
			"recorded pause fixture constructs from the authentic replay route");
		if(paused) {
			bool saw_pause = false;
			bool saw_pause_beep = false;
			for(std::size_t guard = 0U; guard < 512U && !saw_pause; ++guard) {
				auto tick = (*paused)->advance({}, false, false);
				expect(tick.has_value(),
					"recorded pause and release samples preserve playback contract");
				if(!tick) {
					break;
				}
				while(auto command = (*paused)->take_audio_command()) {
					if(command->type ==
						th03::app::ClassicAudioCommandType::pause_song) {
						saw_pause = true;
					} else if(saw_pause && command->type ==
						th03::app::ClassicAudioCommandType::play_effect &&
						command->parameter == 21U) {
						saw_pause_beep = true;
					}
				}
			}
			expect(saw_pause,
				"playback reconstructs Pause audio at the recorded boundary");
			expect(saw_pause_beep,
				"playback queues the source pause beep after pausing BGM");
			const auto pause_cursor = (*paused)->sample_index();
			auto visible_pause = (*paused)->advance({}, false, false);
			expect(visible_pause && visible_pause->surface_changed &&
				visible_pause->process_advanced,
				"recorded long Pause transaction publishes its complete menu once");
			const auto pause_identity =
				(*paused)->pixel_oracle_source_identity();
			expect(
				visible_pause->pixel_oracle_source_identity.has_value() &&
				visible_pause->pixel_oracle_source_identity->process ==
					th03::app::PixelOracleProcess::main &&
				visible_pause->pixel_oracle_source_identity->scene ==
					th03::app::PixelOracleScene::pause &&
				visible_pause->pixel_oracle_source_identity->logical_sample ==
					pause_cursor + 1U &&
				visible_pause->pixel_oracle_source_identity->replay_global_frame ==
					pause_cursor + 1U,
				"Pause mutation tick owns its post-release source identity"
			);
			expect(
				pause_identity.process ==
					th03::app::PixelOracleProcess::main &&
				pause_identity.scene == th03::app::PixelOracleScene::pause &&
				pause_identity.logical_sample == pause_cursor + 1U &&
				pause_identity.replay_global_frame == pause_cursor + 1U &&
				pause_identity.round_frame.has_value(),
				"published Pause identity carries the menu-draw cursor and deferred frame"
			);
			expect(
				(*paused)->sample_index() == pause_cursor + 1U,
				"recorded Pause publishes immediately after its opening release"
			);
			auto resumed = (*paused)->advance({}, false, false);
			expect(resumed && resumed->surface_changed &&
				resumed->process_advanced,
				"recorded long Pause transaction preserves the playback contract");
			bool saw_resume = false;
			bool saw_resume_beep = false;
			while(auto command = (*paused)->take_audio_command()) {
				if(command->type ==
					th03::app::ClassicAudioCommandType::resume_song) {
					saw_resume = true;
				} else if(saw_resume && command->type ==
					th03::app::ClassicAudioCommandType::play_effect &&
					command->parameter == 21U) {
					saw_resume_beep = true;
				}
			}
			expect(saw_resume,
				"playback reconstructs Resume audio at the recorded boundary");
			expect(saw_resume_beep,
				"playback queues the source pause beep after resuming BGM");
			expect(
				(*paused)->sample_index() == pause_cursor +
					recorded_pause_transaction_samples,
				"21,200 replay-owned Pause samples consume at one host boundary"
			);
		}
	}

	auto full = th03::app::NewClassicReplayPlayback::load({
		.content = {
			.data_root = data,
			.main_executable = main,
			.mainl_executable = mainl,
			.font_rom = font,
			.score = score,
		},
		.document = later_document,
		.checkpoint_index = 0U,
	});
	auto released = th03::app::NewClassicReplayPlayback::load({
		.content = {
			.data_root = data,
			.main_executable = main,
			.mainl_executable = mainl,
			.font_rom = font,
			.score = score,
		},
		.document = later_document,
		.checkpoint_index = 0U,
	});
	expect(full.has_value(),
		"complete authentic Story replay constructs from its first checkpoint");
	expect(released.has_value(),
		"authentic held-input comparison constructs from its first checkpoint");
	if(full && released) {
		while((*released)->take_audio_command()) {
		}
		std::array<bool, 11U> owners{};
		std::array<std::uint32_t, 11U> owner_first_samples{};
		owner_first_samples.fill(UINT32_MAX);
		bool advance_failed = false;
		bool live_input_isolated = true;
		bool final_mainl_owner_exposed = false;
		std::uint8_t failure_code = 0U;
		std::optional<std::uint8_t> failure_detail{};
		std::size_t guard = 0U;
		for(; guard < 300000U && (*full)->status() ==
			th03::app::NewClassicPlaybackStatus::running; ++guard) {
			const auto owner =
				th03::app::NewClassicReplayPlaybackTestAccess::owner(**full);
			const auto owner_index = static_cast<std::size_t>(owner);
			if((*full)->sample_index() >= later_sample &&
				owner != th03::app::NewClassicPlaybackOwner::main &&
				owner != th03::app::NewClassicPlaybackOwner::none) {
				final_mainl_owner_exposed = true;
			}
			if(owner_index < owners.size() && !owners[owner_index]) {
				owners[owner_index] = true;
				owner_first_samples[owner_index] = (*full)->sample_index();
				auto owner_surface = (*full)->compose_shown_surface(std::span<
					th03::present::ComposedPixel,
					th03::present::graphics_page_pixels
				>(pixels.data(), pixels.size()));
				expect(owner_surface.has_value(),
					"every authentic playback owner composes a shared surface");
			}
			const auto released_owner =
				th03::app::NewClassicReplayPlaybackTestAccess::owner(**released);
			live_input_isolated = live_input_isolated &&
				released_owner == owner &&
				(*released)->sample_index() == (*full)->sample_index();
			auto released_tick = (*released)->advance({}, false, false);
			const auto live_input = owner ==
				th03::app::NewClassicPlaybackOwner::story_registration ?
				static_cast<std::uint16_t>(0U) :
				static_cast<std::uint16_t>(0x2000U);
			auto tick = (*full)->advance(
				{}, false, false, {.system = live_input}
			);
			if(!tick || !released_tick) {
				advance_failed = true;
				if(!tick) {
					failure_code = static_cast<std::uint8_t>(tick.error().code);
					failure_detail = tick.error().interstitial_detail;
				} else {
					failure_code = static_cast<std::uint8_t>(
						released_tick.error().code
					);
					failure_detail = released_tick.error().interstitial_detail;
				}
				break;
			}
			live_input_isolated = live_input_isolated &&
				released_tick->surface_changed == tick->surface_changed &&
				released_tick->process_advanced == tick->process_advanced &&
				released_tick->status == tick->status &&
				(*released)->sample_index() == (*full)->sample_index() &&
				th03::app::NewClassicReplayPlaybackTestAccess::owner(**released) ==
					th03::app::NewClassicReplayPlaybackTestAccess::owner(**full);
			while((*full)->take_audio_command()) {
			}
			while((*released)->take_audio_command()) {
			}
		}
		if(advance_failed) {
			std::cerr << "full-playback-error=" <<
				static_cast<unsigned>(failure_code);
			if(failure_detail) {
				std::cerr << " interstitial-detail=" <<
					static_cast<unsigned>(*failure_detail);
			}
			std::cerr << " sample=" << (*full)->sample_index() <<
				" owner-sample=" <<
				th03::app::NewClassicReplayPlaybackTestAccess::
					interstitial_sample_count(**full) <<
				" owner=" << static_cast<unsigned>(
					th03::app::NewClassicReplayPlaybackTestAccess::owner(**full)
				) << '\n';
			for(std::size_t index = 0U;
				index < later_document.session.records.size(); ++index) {
				const auto &record = later_document.session.records[index];
				if(const auto *nearby =
					std::get_if<th03::replay::InputSample>(&record);
					nearby && nearby->frame_index + 5U >= (*full)->sample_index() &&
					nearby->frame_index < (*full)->sample_index()) {
					std::cerr << "nearby=" << nearby->frame_index << " phase=" <<
						static_cast<unsigned>(nearby->phase) << " system=" <<
						nearby->input.system << " p1=" << nearby->input.p1 <<
						" p2=" << nearby->input.p2 << '\n';
				}
				if(const auto *sample = std::get_if<th03::replay::InputSample>(&record);
					sample && sample->frame_index == (*full)->sample_index()) {
					std::cerr << "record=" << index << " phase=" <<
						static_cast<unsigned>(sample->phase) << '\n';
				}
				if(const auto *boundary =
					std::get_if<th03::replay::ProcessBoundary>(&record);
					boundary && boundary->sample_index == (*full)->sample_index()) {
					std::cerr << "record=" << index << " boundary=" <<
						static_cast<unsigned>(boundary->completed_process) << '\n';
				}
			}
			for(std::size_t index = 0U; index < owners.size(); ++index) {
				if(owners[index]) {
					std::cerr << "owner-first[" << index << "]=" <<
						owner_first_samples[index] << '\n';
				}
			}
		}
		expect(!advance_failed,
			"complete authentic Story replay has no process or MAINL contract failure");
		expect(live_input_isolated,
			"held live OK cannot alter any replay-owned MAINL owner outside registration");
		expect(guard < 300000U && (*full)->status() ==
				th03::app::NewClassicPlaybackStatus::complete,
			"complete authentic Story replay reaches its recorded terminal boundary");
		auto exit_resident = (*full)->take_exit_resident();
		expect(exit_resident.has_value() &&
			exit_resident->game_mode == th03::game_mode::none,
			"completed playback exports the evolved fresh-OP resident");
		expect(owners[static_cast<std::size_t>(
				th03::app::NewClassicPlaybackOwner::story_win)],
			"authentic Story playback visits the shared win interstitial");
		expect(owners[static_cast<std::size_t>(
				th03::app::NewClassicPlaybackOwner::story_cutscene)],
			"authentic Story playback visits the shared cutscene interstitial");
		expect(owners[static_cast<std::size_t>(
				th03::app::NewClassicPlaybackOwner::story_stage_splash)],
			"authentic Story playback visits the shared next-stage splash");
		const auto fade_drain = th03::app::NewClassicReplayPlaybackTestAccess::
			stage_splash_end_drain_ticks(**full);
		expect(fade_drain >= 36U && (fade_drain % 36U) == 0U,
			"every later-stage character interstitial publishes all 36 post-dismissal white-out/white-in boundaries before MAINL_END");
		expect(!owners[static_cast<std::size_t>(
				th03::app::NewClassicPlaybackOwner::story_staff_verdict)] &&
			!owners[static_cast<std::size_t>(
				th03::app::NewClassicPlaybackOwner::story_registration)] &&
			!final_mainl_owner_exposed &&
			th03::app::NewClassicReplayPlaybackTestAccess::
				terminal_mainl_bypass_taken(**full),
			"final Story playback bypasses ending, staff, verdict, and registration at the donor ending entry");
	}
	expect(!std::filesystem::exists(new_score),
		"replay viewing never creates or mutates New Classic score persistence");
	std::filesystem::remove_all(score_directory, filesystem_error);
	expect(!filesystem_error,
		"authentic playback test removes its isolated persistence directory");
}

void test_additional_authentic_route(
	const std::filesystem::path &data,
	const std::filesystem::path &main,
	const std::filesystem::path &font,
	const std::filesystem::path &replay,
	std::optional<th03::app::NewClassicPlaybackOwner> required_owner,
	const char *completion_message,
	const char *owner_message
)
{
	auto document = th03::formats::parse_v14_replay(read_file(replay));
	expect(document.has_value(),
		"additional authentic V14 route parses for typed playback");
	if(!document) {
		return;
	}
	const auto debug_document = *document;
	const auto score_directory = std::filesystem::temp_directory_path() /
		("th03-new-classic-playback-extra-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()
		));
	std::error_code filesystem_error{};
	std::filesystem::create_directories(score_directory, filesystem_error);
	test_physical_abort_resident(
		data, main, font, score_directory / "YUME.NEM", debug_document
	);
	auto loaded = th03::app::NewClassicReplayPlayback::load({
		.content = {
			.data_root = data,
			.main_executable = main,
			.mainl_executable = main.parent_path() / "MAINL.EXE",
			.font_rom = font,
			.score = score_directory / "YUME.NEM",
		},
		.document = std::move(*document),
		.checkpoint_index = 0U,
	});
	expect(loaded.has_value(),
		"additional authentic V14 route constructs its shared owners");
	if(!loaded) {
		std::filesystem::remove_all(score_directory, filesystem_error);
		return;
	}
	bool saw_required = !required_owner.has_value();
	bool saw_game_over_ui = false;
	bool exposed_terminal_bypass = false;
	bool failed = false;
	std::size_t guard = 0U;
	for(; guard < 300000U && (*loaded)->status() ==
		th03::app::NewClassicPlaybackStatus::running; ++guard) {
		const auto owner =
			th03::app::NewClassicReplayPlaybackTestAccess::owner(**loaded);
		exposed_terminal_bypass = exposed_terminal_bypass || owner ==
			th03::app::NewClassicPlaybackOwner::terminal_mainl_bypass;
		if(required_owner && owner == *required_owner) {
			saw_required = true;
		}
		saw_game_over_ui = saw_game_over_ui ||
			owner == th03::app::NewClassicPlaybackOwner::story_registration ||
			owner == th03::app::NewClassicPlaybackOwner::story_continue ||
			owner == th03::app::NewClassicPlaybackOwner::story_game_over;
		const std::uint16_t live_input = required_owner ==
			th03::app::NewClassicPlaybackOwner::story_registration ?
			static_cast<std::uint16_t>((guard % 2U) == 0U ? 0x0020U : 0U) :
			static_cast<std::uint16_t>(0U);
		auto advanced = (*loaded)->advance(
			{}, false, false, {.system = live_input}
		);
		if(!advanced) {
			failed = true;
			std::cerr << "additional-playback-error=" <<
				static_cast<unsigned>(advanced.error().code) << " sample=" <<
				(*loaded)->sample_index() << " owner=" <<
				static_cast<unsigned>(
					th03::app::NewClassicReplayPlaybackTestAccess::owner(**loaded)
				) << " context=" << static_cast<unsigned>(
					th03::app::NewClassicReplayPlaybackTestAccess::
						main_input_context(**loaded)
				) << '\n';
			if(advanced.error().process) {
				std::cerr << "process-error=" << static_cast<unsigned>(
					advanced.error().process->code
				) << " presentation=" << static_cast<unsigned>(
					advanced.error().process->presentation_status
				) << '\n';
				if(advanced.error().process->core_error) {
					std::cerr << "core-error=" << static_cast<unsigned>(
						advanced.error().process->core_error->code
					) << " message=" <<
						advanced.error().process->core_error->message << '\n';
				}
			}
			if(advanced.error().interstitial_detail) {
				std::cerr << "interstitial-detail=" << static_cast<unsigned>(
					*advanced.error().interstitial_detail
				) << '\n';
			}
			for(std::size_t index = 0U;
				index < debug_document.session.records.size(); ++index) {
				const auto &record = debug_document.session.records[index];
				if(const auto *nearby =
					std::get_if<th03::replay::InputSample>(&record);
					nearby && nearby->frame_index + 35U >= (*loaded)->sample_index() &&
					nearby->frame_index <= (*loaded)->sample_index() + 3U) {
					std::cerr << "nearby=" << nearby->frame_index << " phase=" <<
						static_cast<unsigned>(nearby->phase) << " system=" <<
						nearby->input.system << '\n';
				}
				if(const auto *sample = std::get_if<th03::replay::InputSample>(&record);
					sample && sample->frame_index == (*loaded)->sample_index()) {
					std::cerr << "record=" << index << " phase=" <<
						static_cast<unsigned>(sample->phase) << " system=" <<
						sample->input.system << '\n';
				}
				if(const auto *boundary =
					std::get_if<th03::replay::ProcessBoundary>(&record);
					boundary && boundary->sample_index == (*loaded)->sample_index()) {
					std::cerr << "record=" << index << " boundary=" <<
						static_cast<unsigned>(boundary->completed_process) << '\n';
				}
			}
			break;
		}
		while((*loaded)->take_audio_command()) {
		}
	}
	expect(!failed && guard < 300000U && (*loaded)->status() ==
		th03::app::NewClassicPlaybackStatus::complete, completion_message);
	if(required_owner ==
		th03::app::NewClassicPlaybackOwner::terminal_mainl_bypass) {
		saw_required = th03::app::NewClassicReplayPlaybackTestAccess::
			terminal_mainl_bypass_taken(**loaded);
		expect(!saw_game_over_ui,
			"0.4.12 Game Over playback never constructs registration, Continue, or Game Over UI");
		expect(!exposed_terminal_bypass,
			"terminal MAINL bypass drains atomically without a host-visible frozen owner");
	}
	expect(saw_required, owner_message);
	auto exit_resident = (*loaded)->take_exit_resident();
	expect(exit_resident.has_value() &&
		exit_resident->game_mode == th03::game_mode::none,
		"every completed replay route exports a fresh-OP resident");
	expect(!std::filesystem::exists(score_directory / "YUME.NEW.NEM"),
		"additional replay viewing leaves New Classic score persistence untouched");
	std::filesystem::remove_all(score_directory, filesystem_error);
}

} // namespace

int main(int argc, char **argv)
{
	test_exact_main_replay_overlay();
	test_invalid_request();
	test_mainl_end_input_ownership();
	if(argc == 6 && std::string_view(argv[4]) == "--interstitials") {
		test_authentic_interstitials(argv[1], argv[2], argv[3], argv[5]);
	} else if(argc == 6 && std::string_view(argv[4]) == "--game-over") {
		test_additional_authentic_route(
			argv[1], argv[2], argv[3], argv[5],
			th03::app::NewClassicPlaybackOwner::terminal_mainl_bypass,
			"authentic Story Game Over replay reaches its terminal boundary",
			"0.4.12 Game Over playback bypasses registration and Continue"
		);
	} else if(argc == 6 && std::string_view(argv[4]) == "--vs-win") {
		test_additional_authentic_route(
			argv[1], argv[2], argv[3], argv[5],
			th03::app::NewClassicPlaybackOwner::vs_win,
			"authentic Practice win replay reaches its terminal boundary",
			"authentic Practice win replay visits the shared winner owner"
		);
	} else if(argc >= 5 && argc <= 8) {
		test_authentic(argv[1], argv[2], argv[3], argv[4]);
		if(argc >= 6) {
			test_additional_authentic_route(
				argv[1], argv[2], argv[3], argv[5],
				th03::app::NewClassicPlaybackOwner::terminal_mainl_bypass,
				"authentic Story Game Over replay reaches its terminal boundary",
				"0.4.12 Game Over playback bypasses registration and Continue"
			);
		}
		if(argc >= 7) {
			test_additional_authentic_route(
				argv[1], argv[2], argv[3], argv[6],
				th03::app::NewClassicPlaybackOwner::vs_win,
				"authentic Practice win replay reaches its terminal boundary",
				"authentic Practice win replay visits the shared winner owner"
			);
		}
		if(argc >= 8) {
			test_additional_authentic_route(
				argv[1], argv[2], argv[3], argv[7], std::nullopt,
				"authentic menu-return replay reaches its terminal boundary",
				"menu-return route requires no MAINL scene owner"
			);
		}
	} else if(argc != 1) {
		std::cerr << "usage: test [data main font clear [game-over [vs-win [menu-return]]]]\n"
			"       test data main font --interstitials clear\n";
		return 2;
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
