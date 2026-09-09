#include "presentation.hpp"
#include "th02/formats/stage_assets.hpp"
#include <charconv>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace th02::core;
std::vector<std::uint8_t> read(const std::filesystem::path &path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) throw std::runtime_error("cannot open " + path.string());
	const auto size = file.tellg();
	if (size < 0 || size > 16 * 1024 * 1024) throw std::runtime_error("invalid file size: " + path.string());
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
		throw std::runtime_error("cannot read " + path.string());
	return bytes;
}
std::uint32_t number(std::string_view value)
{
	std::uint32_t result{};
	const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
	if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
		throw std::runtime_error("expected unsigned decimal number");
	return result;
}
void help()
{
	std::cout << "TH02 headless Stage 1 MVP (not a replay verifier)\n"
		"--data DIR --input FILE [--seed N] [--rank 0..3] [--shot 0..2]\n"
		"[--lives 0..5] [--bombs 0..5] [--power 0..128]\n"
		"DIR contains extracted MIKO.BFT, MIKO32.BFT, MIKO16.BFT,\n"
		"STAGE0.BFT/.BMT/.BBT/.MAP/.DT1. Input: little-endian uint16 masks,\n"
		"one per gameplay iteration. Stops at UI/lifecycle boundaries.\n";
}
const char *boundary(Stage1RunPhase phase)
{
	switch (phase) {
	case Stage1RunPhase::not_started:
	case Stage1RunPhase::ready: return "input-exhausted";
	case Stage1RunPhase::input: return "input-sample";
	case Stage1RunPhase::paused: return "pause";
	case Stage1RunPhase::intro: return "boss-intro";
	case Stage1RunPhase::ending: return "stage-ending";
	case Stage1RunPhase::cleared: return "stage-cleared";
	case Stage1RunPhase::quit: return "quit-or-gameover";
	case Stage1RunPhase::refused: return "core-refused";
	}
	return "unknown";
}
}

int main(int argc, char **argv)
{
	try {
		std::filesystem::path data, input_path;
		std::uint32_t seed = 0, rank = 1, shot = 0, lives = 2, bombs = 3, power = 1;
		for (int i = 1; i < argc; ++i) {
			const std::string_view key(argv[i]);
			if (key == "--help") { help(); return 0; }
			if (++i == argc) throw std::runtime_error("missing option value");
			const std::string_view value(argv[i]);
			if (key == "--data") data = argv[i];
			else if (key == "--input") input_path = argv[i];
			else if (key == "--seed") seed = number(value);
			else if (key == "--rank") rank = number(value);
			else if (key == "--shot") shot = number(value);
			else if (key == "--lives") lives = number(value);
			else if (key == "--bombs") bombs = number(value);
			else if (key == "--power") power = number(value);
			else throw std::runtime_error("unknown option: " + std::string(key));
		}
		if (data.empty() || input_path.empty() || rank > 3 || shot > 2 || lives > 5 || bombs > 5 || power > 128)
			throw std::runtime_error("missing data/input or option out of range; use --help");
		const auto inputs = read(input_path);
		if (inputs.empty() || inputs.size() % 2) throw std::runtime_error("input must contain complete uint16 masks");
		// cfg_load/gameplay_init settings, then main_entry's stage seed. No
		// synthetic replay metadata or claim that seed=0 matches an actual run.
		StageState state;
		state.rank = static_cast<std::int8_t>(rank);
		state.shottype = static_cast<std::uint8_t>(shot);
		state.start_lives = static_cast<std::uint8_t>(lives);
		state.player_miss.start_bombs = static_cast<std::uint8_t>(bombs);
		state.player_miss.power = static_cast<std::uint8_t>(power == 0 ? 1 : power);
		state.score.lives = static_cast<std::int8_t>(lives);
		state.score.bombs = static_cast<std::int8_t>(bombs);
		state.bomb_selection = static_cast<BombSelection>(shot);
		state.random.seed = seed;
		state.resident_frame = std::bit_cast<std::int32_t>(seed);
		state.max_enemy_vm_steps = 1024; // Host runaway-script cap, not a source timer.
		gameplay_options_apply(state, false, 0);
		if (!hud_shot_level_from_power(state.player_miss.power, state.player_stream.shot_level))
			throw std::runtime_error("invalid power");
		th02::headless::Presentation sink(state);
		Stage1RunState run;
		if (!stage1_setup_before_assets(state, false, sink)) throw std::runtime_error("setup refused");
		th02::formats::BftSpriteMetrics metrics;
		for (const auto name : {"MIKO.BFT", "MIKO32.BFT", "MIKO16.BFT", "STAGE0.BFT", "STAGE0.BMT", "STAGE0.BBT"}) {
			const auto result = th02::formats::bft_sprite_metrics_append(read(data / name), metrics);
			if (result != th02::formats::BftSpriteMetricsResult::completed) throw std::runtime_error(std::string(name) + " metrics refused code=" + std::to_string(static_cast<unsigned>(result)));
		}
		if (th02::formats::stage_map_decode(read(data / "STAGE0.MAP"), state.tile_sections) != th02::formats::StageMapDecodeResult::completed)
			throw std::runtime_error("MAP decode refused");
		const auto decoded = enemy_stage_data_decode(read(data / "STAGE0.DT1"), metrics.sizes, state.rank, state.stage_data);
		if (decoded != EnemyStageDataResult::completed)
			throw std::runtime_error("DT1 decode refused code=" + std::to_string(static_cast<unsigned>(decoded)));
		tiles_stuff_reset(state.tiles, state.frame.page_back, state.tile_mode, state.tiles_egc_render_all);
		stage1_setup_after_assets(state, run.frame, run.stage_title_enabled);
		if (!stage1_setup_items_and_score(state, sink) ||
			tiles_fill_and_put_initial(state.tiles, state.stage_data, state.tile_sections, sink) != TileScrollResult::completed)
			throw std::runtime_error("stage initialization refused");
		state.frame.page_front = 1; state.frame.page_back = 0;
		auto &bullet_sink = static_cast<BombPresentationCallbacks &>(sink);
		Stage1InvalidationPresentation invalidation{sink, sink, sink, sink, bullet_sink, sink, sink, sink, sink};
		Stage1EntitiesPresentation entities{sink, sink, sink, sink, sink, sink, sink};
		Stage1RunPresentation presentation{sink, invalidation, sink, sink, sink, entities, sink, sink, sink};
		std::size_t consumed = 0;
		auto wait = Stage1RunWait::ready_for_frame;
		while (consumed < inputs.size() / 2 && wait == Stage1RunWait::ready_for_frame) {
			wait = stage1_run_prepare(state, run, presentation);
			if (wait != Stage1RunWait::input_sample) break;
			const auto mask = static_cast<std::uint16_t>(inputs[consumed * 2] |
				(static_cast<std::uint16_t>(inputs[consumed * 2 + 1]) << 8));
			++consumed;
			wait = stage1_run_input(state, run, mask, presentation);
		}
		std::cout << "status=" << (wait == Stage1RunWait::refused ? "error" : "stopped")
			<< "\nboundary=" << boundary(run.phase)
			<< "\ninput-consumed=" << consumed << "\nstage-frame=" << state.stage_frame
			<< "\nwait=" << static_cast<unsigned>(wait) << "\nphase=" << static_cast<unsigned>(run.phase)
			<< "\npreparation=" << static_cast<unsigned>(run.preparation)
			<< "\nentities=" << static_cast<unsigned>(run.entities.result)
			<< "\nscore=" << state.score.score << "\nlives=" << static_cast<int>(state.score.lives)
			<< "\nrng-cursor=" << static_cast<unsigned>(state.random.cursor) << '\n';
		return wait == Stage1RunWait::refused ? 1 : 0;
	} catch (const std::exception &error) {
		std::cerr << "status=error\ndetail=" << error.what() << '\n'; return 1;
	}
}
