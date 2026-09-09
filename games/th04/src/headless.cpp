#include "th04/core/state.hpp"
#include "th04/core/stage.hpp"
#include "th04/core/scroll.hpp"
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
	unsigned frames = 10000, rank = 1, character = 0, shot = 0, seed = 318;
	bool demo = false;
	for (int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		if (arg == "--help") {
			std::cout << "TH04 partial Stage 1 headless core (not replay-validated)\n"
				"--data DIR [--demo2 | --input FILE] [--frames N] [--rank 0..3]\n"
				"[--character 0..1] [--shot 0..1] [--seed N]\n"
				"Input: one original low-byte input mask per gameplay tick.\n"
				"Stops explicitly at pause, dialog, Game Over or stage clear.\n";
			return 0;
		}
		if (arg == "--demo2") { demo = true; continue; }
		if (++i == argc) throw std::runtime_error("Missing option value");
		const std::string_view value = argv[i];
		if (arg == "--data") data = argv[i];
		else if (arg == "--input") input_path = argv[i];
		else {
			unsigned *target = arg == "--frames" ? &frames : arg == "--rank" ? &rank :
				arg == "--character" ? &character : arg == "--shot" ? &shot :
				arg == "--seed" ? &seed : nullptr;
			if (!target || !number(value, *target)) throw std::runtime_error("Invalid option");
		}
	}
	if (data.empty() || rank > 3 || character > 1 || shot > 1 || (demo && !input_path.empty()))
		throw std::runtime_error("Invalid configuration; see --help");
	th04::core::State s;
	th04::core::Presentation p;
	if (!th04::core::gameplay_init(s, static_cast<std::uint8_t>(rank),
		static_cast<std::uint8_t>(demo ? 1 : character),
		static_cast<std::uint8_t>(demo ? 0 : shot), 3, 3, demo)) return 2;
	s.random.seed = demo ? 318 : seed;
	if (demo) s.power = 128;
	th04::core::stage_init(s, p);
	if (!th04::core::std_load(s.std_data, read(data / "ST00.STD")) ||
		!th04::core::map_load(s, read(data / "ST00.MAP")) ||
		!th04::core::tiles_fill_initial(s)) throw std::runtime_error("Invalid stage data");
	th04::core::stage1_fights_setup(s);
	s.page_back = 1;
	s.scroll_active = true;
	const auto inputs = demo ? read(data / "DEMO2.REC") :
		input_path.empty() ? std::vector<std::uint8_t>{} : read(input_path);
	if (demo && inputs.size() != 8000) throw std::runtime_error("Invalid demo size");
	th04::core::StageResult result = th04::core::StageResult::running;
	std::string_view stopped = "frame-limit";
	unsigned ticks = 0;
	for (; ticks < frames; ++ticks) {
		// DemoPlay reads frame 3996 but exits before simulating that frame.
		if (demo && s.stage_frame >= 3996) { stopped = "demo-end"; break; }
		if (!demo && !input_path.empty() && ticks >= inputs.size()) { stopped = "input-end"; break; }
		const auto mask = demo ? inputs[s.stage_frame] : inputs.empty() ? 0 : inputs[ticks];
		const bool shift = demo && inputs[4000U + s.stage_frame] != 0;
		result = th04::core::stage1_tick(s, static_cast<std::uint16_t>(mask), shift, p);
		if (result != th04::core::StageResult::running) { stopped = "game-boundary"; break; }
	}
	constexpr std::array names{"running", "pause", "dialog", "gameover", "cleared", "unsupported"};
	std::cout << "stopped=" << stopped << "\nboundary=" << names[static_cast<std::size_t>(result)]
		<< "\nframe=" << s.stage_frame << "\nscore=";
	for (std::size_t i = s.score.score.size(); i-- > 0;) std::cout << unsigned(s.score.score[i]);
	std::cout << "\nlives=" << unsigned(s.resources.rem_lives)
		<< "\nrng-cursor=" << s.random.cursor << '\n';
	return result == th04::core::StageResult::unsupported ? 3 : 0;
} catch (const std::exception &error) {
	std::cerr << error.what() << '\n';
	return 2;
}
