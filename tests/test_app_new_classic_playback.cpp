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
#include <vector>

#include "th03/app/new_classic_playback.hpp"
#include "th03/formats/v13.hpp"
#include "../src/app/new_classic_playback_internal.hpp"

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

std::optional<th03::formats::ReplayDocument> insert_recorded_pause(
	th03::formats::ReplayDocument document
)
{
	std::vector<th03::replay::SessionRecord> records;
	records.reserve(document.session.records.size() + 3U);
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
				records.push_back(th03::replay::InputSample{
					.frame_index = *pause_sample + 2U,
					.phase = th03::replay::InputPhase::interstitial,
					.input = {.system = 0x1000U},
					.round_frame = std::nullopt,
					.round_or_result_frame = std::nullopt,
				});
				records.push_back(th03::replay::InputSample{
					.frame_index = *pause_sample + 3U,
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
			sample->frame_index += 3U;
		} else {
			std::get<th03::replay::ProcessBoundary>(record).sample_index += 3U;
		}
		records.push_back(std::move(record));
	}
	if(!pause_sample) {
		return std::nullopt;
	}
	document.session.records = std::move(records);
	for(auto &checkpoint : document.checkpoints) {
		if(checkpoint.sample_index > *pause_sample) {
			checkpoint.sample_index += 3U;
		}
	}
	if(document.metadata) {
		document.metadata->sample_count += 3U;
	}
	return document;
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

void test_authentic(
	const std::filesystem::path &data,
	const std::filesystem::path &main,
	const std::filesystem::path &font,
	const std::filesystem::path &replay
)
{
	auto bytes = read_file(replay);
	auto document = th03::formats::parse_v13_replay(bytes);
	expect(document.has_value(), "authentic V13 replay parses for visual playback");
	if(!document) {
		return;
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
	std::size_t audio_commands = 0U;
	while((*loaded)->take_audio_command()) {
		++audio_commands;
	}
	expect(audio_commands == 3U,
		"playback publishes load/reset/play before the first recorded sample");
	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	auto surface = (*loaded)->compose_shown_surface(std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(pixels.data(), pixels.size()));
	expect(surface.has_value(),
		"visual playback composes the shared MAIN surface and replay overlay");
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
			++audio_commands;
		}
	}
	expect(advanced != 0U && (*loaded)->sample_index() != 0U,
		"authentic playback advances recorded MAIN state and sample cursor");
	if(later_checkpoint != 0U) {
		auto later = th03::app::NewClassicReplayPlayback::load({
			.content = {
				.data_root = data,
				.main_executable = main,
				.mainl_executable = mainl,
				.font_rom = font,
				.score = score,
			},
			.document = later_document,
			.checkpoint_index = later_checkpoint,
		});
		expect(
			later && (*later)->sample_index() == later_sample,
			"later checkpoint performs hidden process-start pre-roll to its exact cursor"
		);
		if(later) {
			std::vector<th03::present::ComposedPixel> later_pixels(
				th03::present::graphics_page_pixels
			);
			expect(
				(*later)->compose_shown_surface(std::span<
					th03::present::ComposedPixel,
					th03::present::graphics_page_pixels
				>(later_pixels.data(), later_pixels.size())).has_value(),
				"pre-rolled checkpoint retains a composable shared presentation surface"
			);
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
			bool saw_resume = false;
			for(std::size_t guard = 0U; guard < 512U && !saw_resume; ++guard) {
				auto tick = (*paused)->advance({}, false, false);
				expect(tick.has_value(),
					"recorded pause and release samples preserve playback contract");
				if(!tick) {
					break;
				}
				while(auto command = (*paused)->take_audio_command()) {
					saw_pause = saw_pause || command->type ==
						th03::app::ClassicAudioCommandType::pause_song;
					saw_resume = saw_resume || command->type ==
						th03::app::ClassicAudioCommandType::resume_song;
				}
			}
			expect(saw_pause && saw_resume,
				"playback reconstructs pause and resume audio at recorded boundaries");
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
	expect(full.has_value(),
		"complete authentic Story replay constructs from its first checkpoint");
	if(full) {
		std::array<bool, 11U> owners{};
		std::array<std::uint32_t, 11U> owner_first_samples{};
		owner_first_samples.fill(UINT32_MAX);
		bool advance_failed = false;
		std::uint8_t failure_code = 0U;
		std::optional<std::uint8_t> failure_detail{};
		std::size_t guard = 0U;
		for(; guard < 300000U && (*full)->status() ==
			th03::app::NewClassicPlaybackStatus::running; ++guard) {
			const auto owner =
				th03::app::NewClassicReplayPlaybackTestAccess::owner(**full);
			const auto owner_index = static_cast<std::size_t>(owner);
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
			auto tick = (*full)->advance({}, false, false);
			if(!tick) {
				advance_failed = true;
				failure_code = static_cast<std::uint8_t>(tick.error().code);
				failure_detail = tick.error().interstitial_detail;
				break;
			}
			while((*full)->take_audio_command()) {
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
		expect(guard < 300000U && (*full)->status() ==
				th03::app::NewClassicPlaybackStatus::complete,
			"complete authentic Story replay reaches its recorded terminal boundary");
		expect(owners[static_cast<std::size_t>(
				th03::app::NewClassicPlaybackOwner::story_win)],
			"authentic Story playback visits the shared win interstitial");
		expect(owners[static_cast<std::size_t>(
				th03::app::NewClassicPlaybackOwner::story_cutscene)],
			"authentic Story playback visits the shared cutscene interstitial");
		expect(owners[static_cast<std::size_t>(
				th03::app::NewClassicPlaybackOwner::story_stage_splash)],
			"authentic Story playback visits the shared next-stage splash");
		expect(owners[static_cast<std::size_t>(
				th03::app::NewClassicPlaybackOwner::final_clear_skip)],
			"final Story clear drains the recorded MAINL tail without replaying the ending");
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
	auto document = th03::formats::parse_v13_replay(read_file(replay));
	expect(document.has_value(),
		"additional authentic V13 route parses for typed playback");
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
		"additional authentic V13 route constructs its shared owners");
	if(!loaded) {
		std::filesystem::remove_all(score_directory, filesystem_error);
		return;
	}
	bool saw_required = !required_owner.has_value();
	bool failed = false;
	std::size_t guard = 0U;
	for(; guard < 300000U && (*loaded)->status() ==
		th03::app::NewClassicPlaybackStatus::running; ++guard) {
		if(required_owner &&
			th03::app::NewClassicReplayPlaybackTestAccess::owner(**loaded) ==
				*required_owner) {
			saw_required = true;
		}
		const std::uint16_t live_input = required_owner ==
			th03::app::NewClassicPlaybackOwner::story_registration ?
			static_cast<std::uint16_t>((guard % 2U) == 0U ? 0x0020U : 0U) :
			static_cast<std::uint16_t>(0U);
		auto advanced = (*loaded)->advance(
			{}, false, false, live_input
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
				) << '\n';
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
	expect(saw_required, owner_message);
	expect(!std::filesystem::exists(score_directory / "YUME.NEW.NEM"),
		"additional replay viewing leaves New Classic score persistence untouched");
	std::filesystem::remove_all(score_directory, filesystem_error);
}

} // namespace

int main(int argc, char **argv)
{
	test_invalid_request();
	if(argc == 6 && std::string_view(argv[4]) == "--game-over") {
		test_additional_authentic_route(
			argv[1], argv[2], argv[3], argv[5],
			th03::app::NewClassicPlaybackOwner::story_registration,
			"authentic Story Game Over replay reaches its terminal boundary",
			"authentic Story Game Over replay visits the shared registration owner"
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
				th03::app::NewClassicPlaybackOwner::story_registration,
				"authentic Story Game Over replay reaches its terminal boundary",
				"authentic Story Game Over replay visits the shared registration owner"
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
		std::cerr << "usage: test [data main font clear [game-over [vs-win [menu-return]]]]\n";
		return 2;
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
