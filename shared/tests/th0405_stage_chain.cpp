// Synthetic connected gameplay fixture, not an original-game or replay oracle.
#if TH_STAGE_CHAIN_GAME == 4
#include "th04/core/state.hpp"
#include "th04/core/stage.hpp"
#include "th04/core/scroll.hpp"
#include "th04/core/stage_bonus.hpp"
namespace core = th04::core;
#else
#include "th05/core/state.hpp"
#include "th05/core/stage.hpp"
#include "th05/core/scroll.hpp"
#include "th05/core/stage_bonus.hpp"
namespace core = th05::core;
#endif
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::vector<std::uint8_t> read(const std::filesystem::path &path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream) throw std::runtime_error("Cannot read " + path.string());
	return {std::istreambuf_iterator<char>(stream), {}};
}

bool run(const std::filesystem::path &data, unsigned stages, bool extra)
{
	const std::uint8_t first_rank = extra ? 4 : 0;
	const std::uint8_t end_rank = extra ? 5 : 4;
	const std::uint8_t first_stage = extra ? 6 : 0;
	for (std::uint8_t rank = first_rank; rank < end_rank; ++rank) {
		for (std::uint8_t profile = 0; profile < 4; ++profile) {
			core::State state;
			core::Presentation presentation;
#if TH_STAGE_CHAIN_GAME == 4
			if (!core::gameplay_init(state, rank, static_cast<std::uint8_t>(profile / 2),
				static_cast<std::uint8_t>(profile % 2), 3, 3, false)) return false;
#else
			if (!core::gameplay_init(state, rank, profile, 3, 3, first_stage)) return false;
#endif
			state.random.seed = 318;
			state.power = 128;
			for (std::uint8_t stage = first_stage; stage < stages; ++stage) {
				state.stage_id = stage;
				core::stage_init(state, presentation);
				const auto stem = "ST0" + std::to_string(stage);
				if (!core::std_load(state.std_data, read(data / (stem + ".STD"))) ||
					!core::map_load(state, read(data / (stem + ".MAP"))) ||
					!core::tiles_fill_initial(state) || !core::stage_fights_setup(state)) {
					throw std::runtime_error("Stage setup failed: " + stem);
				}
				state.page_back = 1;
				state.scroll_active = true;
				bool cleared = false;
				core::StageResult result = core::StageResult::running;
				for (unsigned tick = 0; tick < 60000; ++tick) {
					// Explicit fixture intervention: keep shooting, survive, acknowledge
					// dialogue. All other gameplay state carries across stage boundaries.
					state.player_invincibility_time = 255;
					result = core::stage_tick(state, 0x20, false, presentation);
					if (result == core::StageResult::dialog) {
						if (!core::stage_dialog_complete(state)) break;
					} else if (result == core::StageResult::cleared) {
						cleared = true;
						break;
					} else if (result != core::StageResult::running) {
						break;
					}
				}
				if (!cleared) {
					std::cerr << "failed game=" << TH_STAGE_CHAIN_GAME
						<< " rank=" << unsigned(rank) << " profile=" << unsigned(profile)
						<< " stage=" << unsigned(stage) << " frame=" << state.stage_frame
						<< " boundary=" << static_cast<unsigned>(result)
						<< " bossphase=" << unsigned(state.boss.phase)
						<< " bossactive=" << state.boss_active
						<< " bossframe=" << state.boss.phase_frame
						<< " stdactive=" << state.std_data.active
						<< " scrollspeed=" << unsigned(state.std_data.scroll_speed)
						<< " midphase=" << unsigned(state.midboss.phase) << '\n';
					return false;
				}
				// Exercise final-stage scoring too. This fixture deliberately visits
				// every requested stage even on TH04 Easy (whose real route ends
				// after Stage 5); it is not the game's full-session controller.
				const auto bonus = stage >= 5
					? core::stage_allclear_bonus(state, presentation)
					: core::stage_clear_bonus(state, presentation);
				for (unsigned tick = 0; state.score.score_delta != 0 && tick < 10000; ++tick)
					core::score_update(state, presentation);
				if (state.score.score_delta != 0) throw std::runtime_error("Score transfer did not finish");
				std::cout << TH_STAGE_CHAIN_GAME << ' ' << unsigned(rank) << ' '
					<< unsigned(profile) << " stage=" << unsigned(stage)
					<< " frame=" << state.stage_frame << " bonus=" << bonus
					<< " rng=" << state.random.cursor << '\n';
			}
		}
	}
	return true;
}
}

int main(int argc, char **argv)
try {
	if (argc == 2 && std::string_view(argv[1]) == "--help") {
		std::cout << "Usage: stage-chain DATA_DIR STAGE_COUNT (1..6 or extra)\n"
			"Synthetic invincible held-shot fixture; not replay validation.\n";
		return 0;
	}
	if (argc != 3) throw std::runtime_error("Expected DATA_DIR STAGE_COUNT; see --help");
	unsigned stages = 0;
	const std::string_view text = argv[2];
	if (text == "extra") return run(argv[1], 7, true) ? 0 : 1;
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), stages);
	if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || stages < 1 || stages > 6)
		throw std::runtime_error("STAGE_COUNT must be 1..6");
	return run(argv[1], stages, false) ? 0 : 1;
} catch (const std::exception &error) {
	std::cerr << error.what() << '\n';
	return 2;
}
