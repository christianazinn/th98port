#include "th05/core/state.hpp"
#include "th05/core/stage.hpp"
#include "th05/core/scroll.hpp"
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <vector>

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

// th05/op/start.cpp and th04/main/demo.hpp. Demo numbers here are the retail
// filenames (DEMO0.REC through DEMO3.REC), one less than resident->demo_num.
constexpr std::array<std::uint8_t, 4> demo_playchars{0U, 1U, 2U, 3U};
constexpr std::array<std::uint8_t, 4> demo_stages{3U, 1U, 2U, 4U};
constexpr std::size_t demo_frames = 5000U;
constexpr std::size_t demo_bytes = demo_frames * 2U;
}
int main(int argc, char **argv)
try {
	std::filesystem::path data, input_path, pair_path;
	unsigned frames = 10000, rank = 1, character = 0, seed = 318, power = 1, stage = 1,
		demo = 0, lives = 3, bombs = 3;
	bool shift = false, turbo = false, demo_selected = false, demo_conflicting_option = false;
	for (int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		if (arg == "--help") {
			std::cout << "TH05 partial headless core (not replay-validated)\n"
				"--data DIR [--input FILE | --input-pairs FILE] [--frames N] [--rank 0..3]\n"
				"[--character 0..3] [--seed N] [--power 1..128] [--shift] [--stage 1..7 (7=Extra)]\n"
				"[--lives 0..9] [--bombs 0..9] [--turbo]\n"
				"--demo 0..3 reads DEMO#.REC from DIR with source demo setup.\n"
				"Input: one original low-byte mask per gameplay tick.\n"
				"Input pairs: little-endian uint16 mask and one 0/1 Shift byte per tick.\n"
				"Without --input, ordinary runs use zero input; --demo supplies both source halves.\n"
				"Stops at pause, dialog, Game Over or clear.\n";
			return 0;
		}
		if (arg == "--shift") {
			shift = true;
			demo_conflicting_option = true;
			continue;
		}
		if (arg == "--turbo") {
			turbo = true;
			demo_conflicting_option = true;
			continue;
		}
		if (++i == argc) throw std::runtime_error("Missing option value");
		const std::string_view value = argv[i];
		if (arg == "--data") data = argv[i];
		else if (arg == "--frames") {
			if (!number(value, frames)) throw std::runtime_error("Invalid option");
		} else if (arg == "--demo") {
			if (!number(value, demo)) throw std::runtime_error("Invalid option");
			demo_selected = true;
		} else if (arg == "--input") {
			input_path = argv[i];
			demo_conflicting_option = true;
		} else if (arg == "--input-pairs") {
			pair_path = argv[i];
			demo_conflicting_option = true;
		} else {
			unsigned *target = arg == "--rank" ? &rank : arg == "--character" ? &character :
				arg == "--seed" ? &seed : arg == "--power" ? &power :
				arg == "--stage" ? &stage : arg == "--lives" ? &lives :
				arg == "--bombs" ? &bombs : nullptr;
			if (!target || !number(value, *target)) throw std::runtime_error("Invalid option");
			demo_conflicting_option = true;
		}
	}
	if (data.empty()) throw std::runtime_error("Invalid configuration; see --help");
	if (demo_selected) {
		if (demo > 3U || demo_conflicting_option) {
			throw std::runtime_error("--demo cannot be combined with ordinary input settings");
		}
	} else if (rank > 3 || character > 3 || power == 0 || power > 128 ||
		stage < 1 || stage > 7 || lives > 9 || bombs > 9 ||
		(!input_path.empty() && !pair_path.empty()) || (!pair_path.empty() && shift)) {
		throw std::runtime_error("Invalid configuration; see --help");
	}
	const auto stage_id = static_cast<std::uint8_t>(demo_selected
		? demo_stages[demo] : (stage - 1U));
	const auto playchar = static_cast<std::uint8_t>(demo_selected
		? demo_playchars[demo] : character);
	const auto demo_num = static_cast<std::uint8_t>(demo_selected ? (demo + 1U) : 0U);
	th05::core::State s;
	th05::core::Presentation p;
	if (!th05::core::gameplay_init(s, static_cast<std::uint8_t>(rank),
		playchar, static_cast<std::uint8_t>(lives), static_cast<std::uint8_t>(bombs),
		stage_id, demo_num, turbo)) return 2;
	// stage_setup() applies these stores after demo_load(), but before
	// stage_init() fills the random ring and derives the shot level.
	s.random.seed = demo_selected ? 318U : seed;
	s.power = demo_selected ? 128U : static_cast<std::uint8_t>(power);
	std::vector<std::uint8_t> inputs;
	if (demo_selected) {
		inputs = read(data / ("DEMO" + std::to_string(demo) + ".REC"));
		if (inputs.size() < demo_bytes) {
			throw std::runtime_error("Demo input is shorter than the source 10,000-byte record");
		}
	}
	th05::core::stage_init(s, p);
	if (!th05::core::stage_fights_setup(s)) return 2;
	const auto stem = "ST0" + std::to_string(stage_id);
	if (!th05::core::std_load(s.std_data, read(data / (std::string(stem) + ".STD"))) ||
		!th05::core::map_load(s, read(data / (std::string(stem) + ".MAP"))) ||
		!th05::core::tiles_fill_initial(s)) throw std::runtime_error("Invalid stage data");
	s.page_back = 1;
	s.scroll_active = true;
	if (!demo_selected) inputs = !pair_path.empty() ? read(pair_path) :
		input_path.empty() ? std::vector<std::uint8_t>{} : read(input_path);
	if (!pair_path.empty() && (inputs.empty() || inputs.size() % 3U != 0U))
		throw std::runtime_error("Input pairs must contain complete mask/Shift triples");
	th05::core::StageResult result = th05::core::StageResult::running;
	std::string_view stopped = "frame-limit";
	unsigned consumed = 0;
	for (unsigned ticks = 0; ticks < frames; ++ticks) {
		if (demo_selected && s.stage_frame >= (demo_frames - 4U)) {
			stopped = "demo-end";
			break;
		}
		if (!demo_selected && ((!input_path.empty() && ticks >= inputs.size()) ||
			(!pair_path.empty() && ticks >= inputs.size() / 3U))) {
			stopped = "input-end";
			break;
		}
		const auto cursor = demo_selected ? static_cast<std::size_t>(s.stage_frame) : ticks;
		const auto mask = !pair_path.empty() ? static_cast<std::uint16_t>(
			inputs[ticks * 3U] | (static_cast<std::uint16_t>(inputs[ticks * 3U + 1U]) << 8U)) :
			inputs.empty() ? 0U : static_cast<std::uint16_t>(inputs[cursor]);
		const auto input_shift = demo_selected
			? (inputs[cursor + demo_frames] != 0U)
			: !pair_path.empty() ? inputs[ticks * 3U + 2U] != 0U : shift;
		if (!pair_path.empty() && inputs[ticks * 3U + 2U] > 1U)
			throw std::runtime_error("Input pair Shift byte must be 0 or 1");
		++consumed;
		result = th05::core::stage_tick(s, static_cast<std::uint16_t>(mask), input_shift, p);
		if (result != th05::core::StageResult::running) { stopped = "game-boundary"; break; }
	}
	constexpr std::array names{"running", "pause", "dialog", "gameover", "cleared", "unsupported"};
	std::cout << "stopped=" << stopped << "\nboundary=" << names[static_cast<std::size_t>(result)]
		<< "\ninput-consumed=" << consumed << "\nframe=" << s.stage_frame << "\nscore=";
	for (std::size_t i = s.score.score.size(); i-- > 0;) std::cout << unsigned(s.score.score[i]);
	std::cout << "\nlives=" << unsigned(s.resources.rem_lives)
		<< "\nrng-cursor=" << s.random.cursor << '\n';
	return result == th05::core::StageResult::unsupported ? 3 : 0;
} catch (const std::exception &error) {
	std::cerr << error.what() << '\n';
	return 2;
}
