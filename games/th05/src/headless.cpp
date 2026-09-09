#include "th05/core/state.hpp"
#include "th05/core/stage.hpp"
#include "th05/core/scroll.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace {
std::vector<std::uint8_t> read(const std::filesystem::path &path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) throw std::runtime_error("Cannot read " + path.string());
	return {std::istreambuf_iterator<char>(file), {}};
}
bool number(std::string_view text, unsigned &value)
{
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
	return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}
}
int main(int argc, char **argv)
try {
	std::filesystem::path data, input_path;
	unsigned frames = 10000, rank = 1, character = 0, seed = 318, power = 1, stage = 1;
	bool shift = false;
	for (int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		if (arg == "--help") {
			std::cout << "TH05 partial headless core (not replay-validated)\n"
				"--data DIR [--input FILE] [--frames N] [--rank 0..3]\n"
				"[--character 0..3] [--seed N] [--power 1..128] [--shift] [--stage 1..2]\n"
				"Input: one original low-byte mask per gameplay tick.\n"
				"No input file means zero input. Stops at pause, dialog, Game Over or clear.\n";
			return 0;
		}
		if (arg == "--shift") { shift = true; continue; }
		if (++i == argc) throw std::runtime_error("Missing option value");
		const std::string_view value = argv[i];
		if (arg == "--data") data = argv[i];
		else if (arg == "--input") input_path = argv[i];
		else {
			unsigned *target = arg == "--frames" ? &frames : arg == "--rank" ? &rank :
				arg == "--character" ? &character : arg == "--seed" ? &seed :
				arg == "--power" ? &power : arg == "--stage" ? &stage : nullptr;
			if (!target || !number(value, *target)) throw std::runtime_error("Invalid option");
		}
	}
	if (data.empty() || rank > 3 || character > 3 || power == 0 || power > 128 || stage < 1 || stage > 2)
		throw std::runtime_error("Invalid configuration; see --help");
	th05::core::State s;
	th05::core::Presentation p;
	if (!th05::core::gameplay_init(s, static_cast<std::uint8_t>(rank),
		static_cast<std::uint8_t>(character), 3, 3, static_cast<std::uint8_t>(stage - 1))) return 2;
	s.random.seed = seed;
	s.power = static_cast<std::uint8_t>(power);
	th05::core::stage_init(s, p);
	if (!th05::core::stage_fights_setup(s)) return 2;
	const auto stem = stage == 1 ? "ST00" : "ST01";
	if (!th05::core::std_load(s.std_data, read(data / (std::string(stem) + ".STD"))) ||
		!th05::core::map_load(s, read(data / (std::string(stem) + ".MAP"))) ||
		!th05::core::tiles_fill_initial(s)) throw std::runtime_error("Invalid stage data");
	s.page_back = 1;
	s.scroll_active = true;
	const auto inputs = input_path.empty() ? std::vector<std::uint8_t>{} : read(input_path);
	th05::core::StageResult result = th05::core::StageResult::running;
	std::string_view stopped = "frame-limit";
	for (unsigned ticks = 0; ticks < frames; ++ticks) {
		if (!input_path.empty() && ticks >= inputs.size()) { stopped = "input-end"; break; }
		const auto mask = inputs.empty() ? 0 : inputs[ticks];
		result = th05::core::stage_tick(s, static_cast<std::uint16_t>(mask), shift, p);
		if (result != th05::core::StageResult::running) { stopped = "game-boundary"; break; }
	}
	constexpr std::array names{"running", "pause", "dialog", "gameover", "cleared", "unsupported"};
	std::cout << "stopped=" << stopped << "\nboundary=" << names[static_cast<std::size_t>(result)]
		<< "\nframe=" << s.stage_frame << "\nscore=";
	for (std::size_t i = s.score.score.size(); i-- > 0;) std::cout << unsigned(s.score.score[i]);
	std::cout << "\nlives=" << unsigned(s.resources.rem_lives)
		<< "\nrng-cursor=" << s.random.cursor << '\n';
	return result == th05::core::StageResult::unsupported ? 3 : 0;
} catch (const std::exception &error) {
	std::cerr << error.what() << '\n';
	return 2;
}
