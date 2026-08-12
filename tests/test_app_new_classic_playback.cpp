#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
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
