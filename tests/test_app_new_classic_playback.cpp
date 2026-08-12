#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "th03/app/new_classic_playback.hpp"
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
	auto loaded = th03::app::NewClassicReplayPlayback::load({
		.content = {
			.data_root = data,
			.main_executable = main,
			.font_rom = font,
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
		auto result = (*loaded)->advance(false, false);
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
				.font_rom = font,
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
				.font_rom = font,
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
				auto tick = (*paused)->advance(false, false);
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
}

} // namespace

int main(int argc, char **argv)
{
	test_invalid_request();
	if(argc == 5) {
		test_authentic(argv[1], argv[2], argv[3], argv[4]);
	} else if(argc != 1) {
		std::cerr << "usage: test [data main font replay]\n";
		return 2;
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
