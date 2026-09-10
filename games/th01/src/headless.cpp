#include "th01/core/gameplay.hpp"
#include "pc98/random.hpp"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace th01::core;
std::vector<std::uint8_t> read(const std::filesystem::path &path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if(!file) throw std::runtime_error("cannot open " + path.string());
	const auto length = file.tellg();
	if(length < 0 || length > 16 * 1024 * 1024) throw std::runtime_error("invalid file size");
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
	file.seekg(0);
	if(!file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
		throw std::runtime_error("cannot read " + path.string());
	return bytes;
}

std::uint32_t number(std::string_view text)
{
	std::uint32_t result{};
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
	if(parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
		throw std::runtime_error("expected unsigned decimal number");
	return result;
}

const char *name(TickResult result)
{
	switch(result) {
	case TickResult::running: return "input-exhausted";
	case TickResult::waiting_for_shot: return "initial-shot-gate";
	case TickResult::paused: return "pause";
	case TickResult::route_selection: return "route-selection";
	case TickResult::stage_cleared: return "stage-cleared";
	case TickResult::life_lost: return "life-lost";
	case TickResult::game_cleared: return "game-cleared";
	case TickResult::unsupported_numeric_state: return "unsupported-numeric-state";
	case TickResult::unsupported_boss: return "unsupported-boss";
	}
	return "unknown";
}
}

int main(int argc, char **argv)
{
	try {
		std::filesystem::path data, input_path;
		std::uint32_t stage{}, route{}, rank{1}, seed{}, lives{4}, bombs{1};
		for(int i = 1; i < argc; ++i) {
			const std::string_view option(argv[i]);
			if(option == "--help") {
				std::cout << "TH01 headless stage runner (not a replay verifier)\n"
					"--data ARCHIVE --input FILE [--stage 0..19] [--route 0..1]\n"
					"[--rank 0..3] [--seed N] [--lives 0..6] [--bombs 0..5]\n"
					"Input consists of six raw PC-98 key-group bytes (0,3,5,7,8,9) per sample.\n"
					"Consumes the initial Shot gate; stops at pause, life loss, route choice or clear.\n"
					"ARCHIVE is the original Japanese-named TH01 PF archive, not REIIDEN.EXE.\n"
					"No original assets are included. No replay-parity claim is made.\n";
				return 0;
			}
			if(++i == argc) throw std::runtime_error("missing option value");
			const std::string_view value(argv[i]);
			if(option == "--data") data = argv[i];
			else if(option == "--input") input_path = argv[i];
			else if(option == "--stage") stage = number(value);
			else if(option == "--route") route = number(value);
			else if(option == "--rank") rank = number(value);
			else if(option == "--seed") seed = number(value);
			else if(option == "--lives") lives = number(value);
			else if(option == "--bombs") bombs = number(value);
			else throw std::runtime_error("unknown option: " + std::string(option));
		}
		if(data.empty() || input_path.empty() || stage >= 20 || route > 1 || rank > 3 || lives > 6 || bombs > 5)
			throw std::runtime_error("missing data/input or out-of-range start option");
		const auto inputs = read(input_path);
		if(inputs.size() % 6 != 0) throw std::runtime_error("input ends in a partial six-byte sample");
		State s{};
		s.stage_id = static_cast<std::int16_t>(stage);
		s.route = static_cast<std::int8_t>(route);
		s.rank = static_cast<std::int8_t>(rank);
		s.rem_lives = static_cast<std::int16_t>(lives);
		s.rem_bombs = static_cast<std::int8_t>(bombs);
		s.random_seed = s.frame_rand = seed;
		const auto scene = static_cast<std::uint8_t>(stage < 5 ? 0 : (stage / 5 * 2 - 1 + route));
		const auto loaded = stage_scene_load_from_archive(read(data), scene, s.stage);
		if(loaded != StageArchiveResult::completed)
			throw std::runtime_error("stage archive refused code=" + std::to_string(static_cast<unsigned>(loaded)));
		shootout_lasers_init(s);
		s.stage.card_flip_cycle = static_cast<std::uint8_t>(pc98::irand(s.random_seed) % 60U);
		const auto setup = gameplay_begin_stage(s, true);
		if(setup != StageSetupResult::completed)
			throw std::runtime_error("stage setup refused code=" + std::to_string(static_cast<unsigned>(setup)));
		auto result = s.waiting_for_shot ? TickResult::waiting_for_shot : TickResult::running;
		std::size_t consumed{};
		for(std::size_t offset = 0; offset < inputs.size(); offset += 6) {
			const RawInput raw{inputs[offset], inputs[offset + 1], inputs[offset + 2],
				inputs[offset + 3], inputs[offset + 4], inputs[offset + 5]};
			++consumed;
			if(s.waiting_for_shot) {
				result = gameplay_poll_start(s, raw) ? TickResult::running : TickResult::waiting_for_shot;
				continue;
			}
			result = gameplay_tick(s, raw);
			if(result != TickResult::running) break;
		}
		std::cout << "boundary=" << name(result) << "\nsamples=" << consumed
			<< "\nframe=" << s.frame_rand << "\nseed=" << s.random_seed
			<< "\nscore=" << s.score << "\ntimer=" << s.stage.timer
			<< "\norb=" << s.orb.horizontal.left << ',' << s.orb.top
			<< "\nboss=" << static_cast<unsigned>(s.boss.id)
			<< "\nboss-phase=" << static_cast<int>(s.boss.phase)
			<< "\nboss-hp=" << s.boss.hp << '\n';
		return result == TickResult::unsupported_numeric_state || result == TickResult::unsupported_boss ? 1 : 0;
	} catch(const std::exception &e) {
		std::cerr << "error=" << e.what() << '\n';
		return 1;
	}
}
