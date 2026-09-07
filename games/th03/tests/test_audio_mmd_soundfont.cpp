#include "th03/audio/mmd_soundfont.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

namespace fs = std::filesystem;

static_assert(th03::audio::mmd_default_pitch_bend_range_semitones == 12.0F);

[[nodiscard]] std::vector<std::byte> read_file(const fs::path &path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if(!input || input.tellg() <= 0) {
		return {};
	}
	const auto size = static_cast<std::size_t>(input.tellg());
	std::vector<std::byte> result(size);
	input.seekg(0);
	if(!input.read(
		reinterpret_cast<char *>(result.data()),
		static_cast<std::streamsize>(result.size())
	)) {
		return {};
	}
	return result;
}

[[nodiscard]] std::vector<std::byte> minimal_song()
{
	constexpr std::size_t track_count = 18U;
	constexpr std::size_t track_offset = 0x51U;
	std::vector<std::byte> result(track_offset + 9U, std::byte{0U});
	result[0U] = std::byte{120U};
	for(std::size_t track = 0U; track < track_count; ++track) {
		const auto header = 2U + (track * 4U);
		result[header] = std::byte{track_offset};
		result[header + 1U] = std::byte{0U};
		result[header + 2U] = std::byte{0U};
		result[header + 3U] = track == 0U ? std::byte{0U} : std::byte{0xFFU};
	}
	const std::array<std::uint8_t, 9U> events{{
		0xECU, 0U, 0U, 0U,
		60U, 48U, 24U, 100U,
		0xFEU,
	}};
	for(std::size_t index = 0U; index < events.size(); ++index) {
		result[track_offset + index] = std::byte{events[index]};
	}
	return result;
}

[[nodiscard]] std::vector<std::byte> finite_nested_loop_song()
{
	constexpr std::size_t track_count = 18U;
	constexpr std::size_t track_offset = 0x51U;
	const std::array<std::uint8_t, 29U> events{{
		0xECU, 0U, 0U, 0U,
		0xF9U, 0U, 0U, 0U,
		60U, 1U, 1U, 100U,
		0xF9U, 0U, 0U, 0U,
		62U, 1U, 1U, 100U,
		0xF8U, 2U, 0U, 0U,
		0xF8U, 2U, 0U, 0U,
		0xFEU,
	}};
	std::vector<std::byte> result(track_offset + events.size(), std::byte{0U});
	result[0U] = std::byte{120U};
	for(std::size_t track = 0U; track < track_count; ++track) {
		const auto header = 2U + (track * 4U);
		result[header] = std::byte{track_offset};
		result[header + 1U] = std::byte{0U};
		result[header + 2U] = std::byte{0U};
		result[header + 3U] = track == 0U ? std::byte{0U} : std::byte{0xFFU};
	}
	for(std::size_t index = 0U; index < events.size(); ++index) {
		result[track_offset + index] = std::byte{events[index]};
	}
	return result;
}

[[nodiscard]] bool render_song(
	th03::audio::MmdSoundfontEngine &engine,
	std::span<const std::byte> song,
	bool require_audible
)
{
	if(!engine.load_song(song) || !engine.play()) {
		return false;
	}
	std::vector<th03::audio::PmdCanonicalStereoSample> output(48'000U);
	if(!engine.render_mix(output)) {
		return false;
	}
	return !require_audible || std::ranges::any_of(output, [](const auto &sample) {
		return sample.left != 0 || sample.right != 0;
	});
}

[[nodiscard]] bool render_finite_nested_loop_song(
	th03::audio::MmdSoundfontEngine &engine
)
{
	const auto song = finite_nested_loop_song();
	if(!engine.load_song(song) || !engine.play()) {
		return false;
	}
	std::vector<th03::audio::PmdCanonicalStereoSample> output(48'000U);
	return engine.render_mix(output) && !engine.status().playing;
}

} // namespace

int main(int argc, char **argv)
{
	if(argc < 2 || argc > 3) {
		return 2;
	}
	const auto soundfont = read_file(argv[1]);
	if(soundfont.empty()) {
		return 3;
	}
	auto created = th03::audio::MmdSoundfontEngine::create({
		.soundfont = soundfont,
	});
	if(!created || !render_song(*created, minimal_song(), true) ||
		!render_finite_nested_loop_song(*created)) {
		return 4;
	}
	if(argc == 3) {
		std::vector<fs::path> songs{};
		for(const auto &entry : fs::directory_iterator(argv[2])) {
			if(entry.is_regular_file() && entry.path().extension() == ".MMD") {
				songs.push_back(entry.path());
			}
		}
		std::ranges::sort(songs);
		if(songs.size() != 22U) {
			return 5;
		}
		for(const auto &path : songs) {
			// The PC-98 launcher plays GMINIT.MMD for its GM System On SysEx.
			// TinySoundFont is reset directly and does not consume that driver-only
			// setup sequence.
			if(path.filename() == "GMINIT.MMD") {
				continue;
			}
			const auto song = read_file(path);
			if(song.empty() || !render_song(*created, song, true)) {
				std::cerr << "mmd-song=" << path.string() << '\n';
				return 6;
			}
		}
	}
	return 0;
}
