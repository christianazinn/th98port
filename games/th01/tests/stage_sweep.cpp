#include "th01/core/gameplay.hpp"
#include "pc98/random.hpp"

#include <bit>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

// Synthetic integration exercise using locally supplied original stage data.
// Its trace is useful for compiler comparisons, not original-game parity.
int main(int argc, char **argv)
{
	using namespace th01::core;
	if(argc == 2 && std::string_view(argv[1]) == "--help") {
		std::cout << "th01-stage-sweep ORIGINAL-PF-ARCHIVE\n"
			"Synthetic 2-route/4-rank/20-stage source-asset sweep; not replay validation.\n";
		return 0;
	}
	try {
		if(argc != 2) throw std::runtime_error("expected original PF archive path");
		std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
		if(!file) throw std::runtime_error("cannot open archive");
		const auto length = file.tellg();
		if(length < 0 || length > 16 * 1024 * 1024) throw std::runtime_error("bad archive size");
		std::vector<std::uint8_t> archive(static_cast<std::size_t>(length));
		file.seekg(0);
		if(!file.read(reinterpret_cast<char *>(archive.data()), static_cast<std::streamsize>(archive.size())))
			throw std::runtime_error("cannot read archive");
		for(std::int8_t route = 0; route < 2; ++route) {
			for(std::int8_t rank = 0; rank < 4; ++rank) {
				for(std::int16_t stage = 0; stage < 20; ++stage) {
					State s{};
					s.stage_id = stage;
					s.route = route;
					s.rank = rank;
					s.random_seed = s.frame_rand = 0x12345678U;
					const auto scene = static_cast<std::uint8_t>(stage < 5 ? 0 : stage / 5 * 2 - 1 + route);
					if(stage_scene_load_from_archive(archive, scene, s.stage) != StageArchiveResult::completed)
						throw std::runtime_error("archive decode refused");
					shootout_lasers_init(s);
					s.stage.card_flip_cycle = static_cast<std::uint8_t>(pc98::irand(s.random_seed) % 60U);
					if(gameplay_begin_stage(s, true) != StageSetupResult::completed)
						throw std::runtime_error("stage setup refused");
					std::uint64_t hash = 14695981039346656037ULL;
					const auto mix = [&hash](std::uint64_t value) {
						for(int byte = 0; byte < 8; ++byte) {
							hash = (hash ^ (value & 255U)) * 1099511628211ULL;
							value >>= 8;
						}
					};
					auto result = TickResult::running;
					int samples{};
					for(; samples < 2000; ++samples) {
						const RawInput raw{.group5 = static_cast<std::uint8_t>(
							(samples % 3 == 0 ? 2 : 0) | (samples % 61 == 0 ? 4 : 0)),
							.group7 = static_cast<std::uint8_t>(samples / 100 % 2 ? 8 : 16)};
						if(s.waiting_for_shot) { (void)gameplay_poll_start(s, raw); continue; }
						result = gameplay_tick(s, raw);
						mix(s.random_seed); mix(s.frame_rand); mix(s.score); mix(s.stage.timer);
						mix(static_cast<std::uint16_t>(s.orb.horizontal.left));
						mix(static_cast<std::uint16_t>(s.orb.top));
						mix(std::bit_cast<std::uint64_t>(s.orb.vertical.force));
						mix(static_cast<std::uint16_t>(s.boss.hp));
						mix(static_cast<std::uint8_t>(s.boss.phase));
						for(const auto &pellet : s.projectiles.pellets) {
							mix(pellet.moving); mix(static_cast<std::uint16_t>(pellet.cur_left));
							mix(static_cast<std::uint16_t>(pellet.cur_top));
						}
						if(result == TickResult::unsupported_boss || result == TickResult::unsupported_numeric_state)
							throw std::runtime_error("core refused during sweep");
						if(result == TickResult::life_lost && gameplay_restart_life(s)) continue;
						if(result != TickResult::running) break;
					}
					std::cout << static_cast<int>(route) << ',' << static_cast<int>(rank) << ',' << stage
						<< ',' << samples << ',' << static_cast<unsigned>(result) << ',' << s.score
						<< ',' << hash << '\n';
				}
			}
		}
		return 0;
	} catch(const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
