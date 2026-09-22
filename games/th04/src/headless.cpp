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
	std::filesystem::path data, input_path, pair_path;
	unsigned frames = 10000, rank = 1, character = 0, shot = 0, seed = 318, stage = 1;
	unsigned lives = 3, bombs = 3;
	bool demo = false, turbo = false;
	for (int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		if (arg == "--help") {
			std::cout << "TH04 partial headless core (not replay-validated)\n"
				"--data DIR [--demo2 | --input FILE | --input-pairs FILE] [--frames N] [--rank 0..3, 4 for Extra]\n"
				"[--character 0..1] [--shot 0..1] [--seed N] [--stage 1..7 (7=Extra)]\n"
				"[--lives 0..9] [--bombs 0..9] [--turbo]\n"
				"Input: one original low-byte input mask per gameplay tick.\n"
				"Input pairs: little-endian uint16 mask and one 0/1 Shift byte per tick.\n"
				"Stops explicitly at pause, dialog, Game Over or stage clear.\n";
			return 0;
		}
		if (arg == "--demo2") { demo = true; continue; }
		if (arg == "--turbo") { turbo = true; continue; }
		if (++i == argc) throw std::runtime_error("Missing option value");
		const std::string_view value = argv[i];
		if (arg == "--data") data = argv[i];
		else if (arg == "--input") input_path = argv[i];
		else if (arg == "--input-pairs") pair_path = argv[i];
		else {
			unsigned *target = arg == "--frames" ? &frames : arg == "--rank" ? &rank :
				arg == "--character" ? &character : arg == "--shot" ? &shot :
				arg == "--seed" ? &seed : arg == "--stage" ? &stage :
				arg == "--lives" ? &lives : arg == "--bombs" ? &bombs : nullptr;
			if (!target || !number(value, *target)) throw std::runtime_error("Invalid option");
		}
	}
	if (data.empty() || stage < 1 || stage > 7 || rank > (stage == 7 ? 4U : 3U) ||
		character > 1 || shot > 1 ||
		lives > 9 || bombs > 9 || (!input_path.empty() && !pair_path.empty()) ||
		(demo && (!input_path.empty() || !pair_path.empty() || stage != 1 || turbo)))
		throw std::runtime_error("Invalid configuration; see --help");
	th04::core::State s;
	th04::core::Presentation p;
	if (!th04::core::gameplay_init(s, static_cast<std::uint8_t>(stage == 7 ? 4 : rank),
		static_cast<std::uint8_t>(demo ? 1 : character),
		static_cast<std::uint8_t>(demo ? 0 : shot),
		static_cast<std::uint8_t>(lives), static_cast<std::uint8_t>(bombs), demo, turbo)) return 2;
	s.random.seed = demo ? 318 : seed;
	s.stage_id = static_cast<std::uint8_t>(stage - 1);
	if (demo) s.power = 128;
	th04::core::stage_init(s, p);
	const auto stem = "ST0" + std::to_string(stage - 1);
	if (!th04::core::std_load(s.std_data, read(data / (std::string(stem) + ".STD"))) ||
		!th04::core::map_load(s, read(data / (std::string(stem) + ".MAP"))) ||
		!th04::core::tiles_fill_initial(s)) throw std::runtime_error("Invalid stage data");
	if (!th04::core::stage_fights_setup(s)) return 2;
	s.page_back = 1;
	s.scroll_active = true;
	const auto inputs = demo ? read(data / "DEMO2.REC") :
		!pair_path.empty() ? read(pair_path) :
		input_path.empty() ? std::vector<std::uint8_t>{} : read(input_path);
	if (demo && inputs.size() != 8000) throw std::runtime_error("Invalid demo size");
	if (!pair_path.empty() && (inputs.empty() || inputs.size() % 3U != 0U))
		throw std::runtime_error("Input pairs must contain complete mask/Shift triples");
	th04::core::StageResult result = th04::core::StageResult::running;
	std::string_view stopped = "frame-limit";
	unsigned ticks = 0;
	unsigned consumed = 0;
	for (; ticks < frames; ++ticks) {
		// DemoPlay reads frame 3996 but exits before simulating that frame.
		if (demo && s.stage_frame >= 3996) { stopped = "demo-end"; break; }
		if (!demo && ((!input_path.empty() && ticks >= inputs.size()) ||
			(!pair_path.empty() && ticks >= inputs.size() / 3U))) { stopped = "input-end"; break; }
		const auto mask = demo ? static_cast<std::uint16_t>(inputs[s.stage_frame]) :
			!pair_path.empty() ? static_cast<std::uint16_t>(inputs[ticks * 3U] |
				(static_cast<std::uint16_t>(inputs[ticks * 3U + 1U]) << 8U)) :
			inputs.empty() ? 0U : static_cast<std::uint16_t>(inputs[ticks]);
		const bool shift = demo ? inputs[4000U + s.stage_frame] != 0 :
			!pair_path.empty() && inputs[ticks * 3U + 2U] != 0;
		if (!pair_path.empty() && inputs[ticks * 3U + 2U] > 1U)
			throw std::runtime_error("Input pair Shift byte must be 0 or 1");
		++consumed;
		result = th04::core::stage_tick(s, static_cast<std::uint16_t>(mask), shift, p);
		if (result != th04::core::StageResult::running) { stopped = "game-boundary"; break; }
	}
	constexpr std::array names{"running", "pause", "dialog", "gameover", "cleared", "unsupported"};
	std::cout << "stopped=" << stopped << "\nboundary=" << names[static_cast<std::size_t>(result)]
		<< "\ninput-consumed=" << consumed << "\nframe=" << s.stage_frame << "\nscore=";
	for (std::size_t i = s.score.score.size(); i-- > 0;) std::cout << unsigned(s.score.score[i]);
	std::cout << "\nlives=" << unsigned(s.resources.rem_lives)
		<< "\nrng-cursor=" << s.random.cursor << '\n';
	return result == th04::core::StageResult::unsupported ? 3 : 0;
} catch (const std::exception &error) {
	std::cerr << error.what() << '\n';
	return 2;
}
