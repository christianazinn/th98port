#include "presentation.hpp"
#include "th02/formats/stage_assets.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace th02::core;

constexpr std::size_t demo_input_count = 7000U;
constexpr std::size_t demo_gameplay_input_count = demo_input_count - 50U;

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
	std::cout << "TH02 headless stage runner (not a replay verifier)\n"
		"--data DIR (--input FILE [--stage 0..5] [--seed N] [--rank 0..4] [--shot 0..2]\n"
		"[--lives 0..5] [--bombs 0..5] [--power 0..80] | --demo 1..3)\n"
		"DIR contains extracted MIKO*.BFT and STAGE0 through STAGE5 BFT/BMT/BBT/MAP/DT1.\n"
		"Input is little-endian uint16 masks, one per gameplay iteration. --demo loads\n"
		"the corresponding DEMO#.REC with its source-defined starting state.\n"
		"Extra defaults to rank 4 and one bomb; direct options may override these.\n"
		"--allow-truncated-spawn-tail permits testing only the available spawn rows.\n"
		"Stops at pause, boss dialogue, ending, quit, or other UI/lifecycle boundaries.\n";
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

const char *boundary(Stage5RunPhase phase)
{
	switch (phase) {
	case Stage5RunPhase::not_started:
	case Stage5RunPhase::ready: return "input-exhausted";
	case Stage5RunPhase::input: return "input-sample";
	case Stage5RunPhase::paused: return "pause";
	case Stage5RunPhase::intro: return "boss-intro";
	case Stage5RunPhase::ending: return "stage-ending";
	case Stage5RunPhase::maine: return "maine";
	case Stage5RunPhase::quit: return "quit-or-gameover";
	case Stage5RunPhase::refused: return "core-refused";
	}
	return "unknown";
}

void load_stage_assets(StageState &state, const std::filesystem::path &data,
	th02::core::EnemyStageDataTailPolicy tail_policy)
{
	th02::formats::BftSpriteMetrics metrics;
	for (const auto name : {"MIKO.BFT", "MIKO32.BFT", "MIKO16.BFT"}) {
		const auto result = th02::formats::bft_sprite_metrics_append(read(data / name), metrics);
		if (result != th02::formats::BftSpriteMetricsResult::completed)
			throw std::runtime_error(std::string(name) + " metrics refused code=" +
				std::to_string(static_cast<unsigned>(result)));
	}
	const auto stage_name = std::string("STAGE") + std::to_string(state.stage_id);
	for (const auto extension : {"BFT", "BMT", "BBT"}) {
		const auto name = stage_name + '.' + extension;
		const auto result = th02::formats::bft_sprite_metrics_append(read(data / name), metrics);
		if (result != th02::formats::BftSpriteMetricsResult::completed)
			throw std::runtime_error(name + " metrics refused code=" +
				std::to_string(static_cast<unsigned>(result)));
	}
	if (th02::formats::stage_map_decode(read(data / (stage_name + ".MAP")), state.tile_sections) !=
		th02::formats::StageMapDecodeResult::completed)
		throw std::runtime_error("MAP decode refused");
	const auto decoded = enemy_stage_data_decode(read(data / (stage_name + ".DT1")),
		metrics.sizes, state.rank, state.stage_data, tail_policy);
	if (decoded != EnemyStageDataResult::completed)
		throw std::runtime_error("DT1 decode refused code=" +
			std::to_string(static_cast<unsigned>(decoded)));
}

bool setup_before_assets(StageState &state, bool demo,
	Stage1SetupPresentation &presentation) noexcept
{
	switch (state.stage_id) {
	case 0U: return stage1_setup_before_assets(state, demo, presentation);
	case 1U: return stage2_setup_before_assets(state, demo, presentation);
	case 2U: return stage3_setup_before_assets(state, demo, presentation);
	case 3U: return stage4_setup_before_assets(state, demo, presentation);
	case 4U: return stage5_setup_before_assets(state, demo, presentation);
	case 5U: return stage_extra_setup_before_assets(state, demo, presentation);
	default: return false;
	}
}

void setup_after_assets(StageState &state, Stage1FrameSchedule &frame,
	bool &stage_title_enabled) noexcept
{
	switch (state.stage_id) {
	case 0U: stage1_setup_after_assets(state, frame, stage_title_enabled); break;
	case 1U: stage2_setup_after_assets(state, frame, stage_title_enabled); break;
	case 2U: stage1_setup_after_assets(state, frame, stage_title_enabled); break;
	case 3U: stage4_setup_after_assets(state, frame, stage_title_enabled); break;
	case 4U: stage5_setup_after_assets(state, frame, stage_title_enabled); break;
	case 5U: stage_extra_setup_after_assets(state, frame, stage_title_enabled); break;
	default: break;
	}
}
} // namespace

int main(int argc, char **argv)
{
	try {
		std::filesystem::path data, input_path;
		std::uint32_t stage = 0, demo = 0, seed = 0, rank = 1, shot = 0;
		std::uint32_t lives = 2, bombs = 3, power = 1;
		bool input_specified{};
		bool direct_option_specified{};
		bool rank_specified{}, bombs_specified{};
		auto tail_policy = EnemyStageDataTailPolicy::refuse_truncated;
		for (int i = 1; i < argc; ++i) {
			const std::string_view key(argv[i]);
			if (key == "--help") { help(); return 0; }
			if (key == "--allow-truncated-spawn-tail") {
				tail_policy = EnemyStageDataTailPolicy::materialize_complete_spawn_rows;
				continue;
			}
			if (++i == argc) throw std::runtime_error("missing option value");
			const std::string_view value(argv[i]);
			if (key == "--data") data = argv[i];
			else if (key == "--input") { input_path = argv[i]; input_specified = true; }
			else if (key == "--stage") { stage = number(value); direct_option_specified = true; }
			else if (key == "--demo") demo = number(value);
			else if (key == "--seed") { seed = number(value); direct_option_specified = true; }
			else if (key == "--rank") { rank = number(value); rank_specified = true; direct_option_specified = true; }
			else if (key == "--shot") { shot = number(value); direct_option_specified = true; }
			else if (key == "--lives") { lives = number(value); direct_option_specified = true; }
			else if (key == "--bombs") { bombs = number(value); bombs_specified = true; direct_option_specified = true; }
			else if (key == "--power") { power = number(value); direct_option_specified = true; }
			else throw std::runtime_error("unknown option: " + std::string(key));
		}
		if (data.empty() || stage > 5U || demo > 3U || rank > 4U || shot > 2U ||
			lives > 5U || bombs > 5U || power > 80U || (demo == 0U && input_path.empty()) ||
			(demo != 0U && (input_specified || direct_option_specified)))
			throw std::runtime_error("missing data/input or option out of range; use --help");

		const bool is_demo = demo != 0U;
		if (!is_demo && stage == 5U) {
			// OP start_extra differs from the ordinary starting configuration.
			if (!rank_specified) rank = 4U;
			if (!bombs_specified) bombs = 1U;
		}
		if (is_demo) {
			// demo_load(): source-selected replay and all of its fixed start words.
			constexpr std::array<std::uint32_t, 4U> demo_stage{0U, 3U, 2U, 1U};
			constexpr std::array<std::uint32_t, 4U> demo_shot{0U, 0U, 2U, 1U};
			stage = demo_stage[demo];
			shot = demo_shot[demo];
			seed = 18U;
			rank = 1U;
			lives = 2U;
			bombs = 3U;
			power = 80U;
			input_path = data / (std::string("DEMO") + std::to_string(demo) + ".REC");
		}

		const auto inputs = read(input_path);
		if (inputs.empty() || inputs.size() % 2U != 0U)
			throw std::runtime_error("input must contain complete uint16 masks");
		if (is_demo && inputs.size() != demo_input_count * sizeof(std::uint16_t))
			throw std::runtime_error("demo must contain exactly 7000 uint16 masks");

		// Direct stage starts use cfg_load's normal starting words plus a supplied
		// stage number. They intentionally do not represent a replay seek.
		StageState state;
		state.stage_id = static_cast<std::uint8_t>(stage);
		state.rank = static_cast<std::int8_t>(rank);
		state.shottype = static_cast<std::uint8_t>(shot);
		state.start_lives = static_cast<std::uint8_t>(lives);
		state.player_miss.start_bombs = static_cast<std::uint8_t>(bombs);
		state.player_miss.power = static_cast<std::uint8_t>(power == 0U ? 1U : power);
		state.player_miss.playperf = is_demo ? 12 : 0;
		state.score.lives = static_cast<std::int8_t>(lives);
		state.score.bombs = static_cast<std::int8_t>(bombs);
		state.bomb_selection = static_cast<BombSelection>(shot);
		state.random.seed = seed;
		state.resident_frame = std::bit_cast<std::int32_t>(seed);
		state.max_enemy_vm_steps = 1024; // Host runaway-script cap, not a source timer.
		gameplay_options_apply(state, false, 0U);
		if (!hud_shot_level_from_power(state.player_miss.power, state.player_stream.shot_level))
			throw std::runtime_error("invalid power");

		th02::headless::Presentation sink(state);
		if (!setup_before_assets(state, is_demo, sink)) throw std::runtime_error("setup refused");
		load_stage_assets(state, data, tail_policy);
		tiles_stuff_reset(state.tiles, state.frame.page_back, state.tile_mode, state.tiles_egc_render_all);

		Stage1FrameSchedule setup_frame;
		bool stage_title_enabled{};
		setup_after_assets(state, setup_frame, stage_title_enabled);
		if (!stage1_setup_items_and_score(state, sink) ||
			tiles_fill_and_put_initial(state.tiles, state.stage_data, state.tile_sections, sink) !=
			TileScrollResult::completed)
			throw std::runtime_error("stage initialization refused");
		state.frame.page_front = 1U;
		state.frame.page_back = 0U;

		auto &bullet = static_cast<BombPresentationCallbacks &>(sink);
		auto &bullet_invalidation = static_cast<EnemyBulletInvalidationCallbacks &>(bullet);
		auto &background = static_cast<StageSceneryCallbacks &>(
			static_cast<Stage1PreparationPresentation &>(sink));
		auto &stage1_intro = static_cast<Stage1IntroPresentation &>(
			static_cast<Stage5EndPresentation &>(sink));
		auto &stage2_intro = static_cast<Stage2IntroPresentation &>(
			static_cast<Stage5EndPresentation &>(sink));
		auto &boss_intro = static_cast<BossIntroPresentation &>(
			static_cast<Stage5EndPresentation &>(sink));

		std::size_t consumed{};
		const auto input_limit = is_demo ? demo_gameplay_input_count : inputs.size() / sizeof(std::uint16_t);
		auto next_input = [&]() noexcept {
			auto mask = static_cast<std::uint16_t>(inputs[consumed * 2U] |
				(static_cast<std::uint16_t>(inputs[(consumed * 2U) + 1U]) << 8U));
			++consumed;
			// DemoPlay reads the 6950th word, then overwrites key_det, fades, and
			// sets quit before player_move_and_shoot() in that same frame.
			if (is_demo && consumed == demo_gameplay_input_count) {
				mask = 0U;
				state.quit = true;
			}
			return mask;
		};
		auto drive = [&](auto prepare, auto input) {
			auto wait = Stage1RunWait::ready_for_frame;
			while (consumed < input_limit && wait == Stage1RunWait::ready_for_frame) {
				wait = prepare();
				if (wait != Stage1RunWait::input_sample) break;
				wait = input(next_input());
			}
			return wait;
		};

		Stage1RunWait wait{};
		const char *stopped_at{};
		unsigned phase{};
		unsigned preparation{};
		unsigned entities{};
		switch (state.stage_id) {
		case 0U: {
			Stage1RunState run;
			run.frame = setup_frame;
			run.stage_title_enabled = stage_title_enabled;
			run.demo = is_demo;
			Stage1InvalidationPresentation invalidation{static_cast<RikaCallbacks &>(sink),
				static_cast<Midboss1Callbacks &>(sink), sink, sink, bullet_invalidation,
				sink, sink, sink, bullet};
			Stage1EntitiesPresentation entities_presentation{sink,
				static_cast<RikaCallbacks &>(sink), static_cast<Midboss1Callbacks &>(sink),
				sink, sink, sink, sink};
			Stage1RunPresentation presentation{sink, invalidation, sink, sink, bullet,
				entities_presentation, sink, stage1_intro, sink};
			wait = drive([&] { return stage1_run_prepare(state, run, presentation); },
				[&](std::uint16_t input) { return stage1_run_input(state, run, input, presentation); });
			stopped_at = boundary(run.phase);
			phase = static_cast<unsigned>(run.phase);
			preparation = static_cast<unsigned>(run.preparation);
			entities = static_cast<unsigned>(run.entities.result);
			break;
		}
		case 1U: {
			Stage2RunState run;
			run.frame = setup_frame;
			run.stage_title_enabled = stage_title_enabled;
			run.demo = is_demo;
			Stage2InvalidationPresentation invalidation{static_cast<MeiraCallbacks &>(sink),
				static_cast<Midboss2Callbacks &>(sink), sink, sink, bullet_invalidation,
				sink, sink, sink, bullet};
			Stage2EntitiesPresentation entities_presentation{sink,
				static_cast<MeiraCallbacks &>(sink), static_cast<Midboss2Callbacks &>(sink),
				sink, sink, sink, sink};
			Stage2RunPresentation presentation{sink, invalidation, sink, sink, bullet,
				entities_presentation, sink, stage2_intro, sink};
			wait = drive([&] { return stage2_run_prepare(state, run, presentation); },
				[&](std::uint16_t input) { return stage2_run_input(state, run, input, presentation); });
			stopped_at = boundary(run.phase);
			phase = static_cast<unsigned>(run.phase);
			preparation = static_cast<unsigned>(run.preparation);
			entities = static_cast<unsigned>(run.entities.result);
			break;
		}
		case 2U: {
			Stage3RunState run;
			run.frame = setup_frame;
			run.stage_title_enabled = stage_title_enabled;
			run.demo = is_demo;
			Stage3InvalidationPresentation invalidation{background,
				static_cast<Midboss2Callbacks &>(sink), sink, sink, bullet_invalidation,
				sink, sink, sink, bullet};
			Stage3EntitiesPresentation entities_presentation{sink,
				static_cast<StonesCallbacks &>(sink), static_cast<Midboss2Callbacks &>(sink),
				sink, sink, sink, sink};
			Stage3RunPresentation presentation{sink, invalidation, sink, sink, bullet,
				entities_presentation, sink, stage1_intro, sink};
			wait = drive([&] { return stage3_run_prepare(state, run, presentation); },
				[&](std::uint16_t input) { return stage3_run_input(state, run, input, presentation); });
			stopped_at = boundary(run.phase);
			phase = static_cast<unsigned>(run.phase);
			preparation = static_cast<unsigned>(run.preparation);
			entities = static_cast<unsigned>(run.entities.result);
			break;
		}
		case 3U: {
			Stage4RunState run;
			run.frame = setup_frame;
			run.stage_title_enabled = stage_title_enabled;
			run.demo = is_demo;
			Stage4InvalidationPresentation invalidation{background,
				static_cast<Midboss2Callbacks &>(sink), sink, sink, bullet_invalidation,
				sink, sink, sink, bullet};
			Stage4EntitiesPresentation entities_presentation{sink,
				static_cast<MarisaCallbacks &>(sink), static_cast<Midboss2Callbacks &>(sink),
				static_cast<StonesCallbacks &>(sink), background, sink, sink, sink, sink};
			Stage4RunPresentation presentation{sink, invalidation, sink, sink, bullet,
				entities_presentation, sink, boss_intro, sink};
			wait = drive([&] { return stage4_run_prepare(state, run, presentation); },
				[&](std::uint16_t input) { return stage4_run_input(state, run, input, presentation); });
			stopped_at = boundary(run.phase);
			phase = static_cast<unsigned>(run.phase);
			preparation = static_cast<unsigned>(run.preparation);
			entities = static_cast<unsigned>(run.entities.result);
			break;
		}
		case 4U: {
			Stage5RunState run;
			run.frame = setup_frame;
			run.stage_title_enabled = stage_title_enabled;
			run.demo = is_demo;
			Stage5InvalidationPresentation invalidation{static_cast<MimaCallbacks &>(sink),
				sink, sink, bullet_invalidation, sink, sink, sink, bullet};
			Stage5EntitiesPresentation entities_presentation{sink,
				static_cast<MimaCallbacks &>(sink), sink, sink, sink, sink};
			Stage5RunPresentation presentation{sink, invalidation, sink, sink, bullet,
				entities_presentation, sink, boss_intro, static_cast<Stage5EndPresentation &>(sink)};
			wait = drive([&] { return stage5_run_prepare(state, run, presentation); },
				[&](std::uint16_t input) { return stage5_run_input(state, run, input, presentation); });
			stopped_at = boundary(run.phase);
			phase = static_cast<unsigned>(run.phase);
			preparation = static_cast<unsigned>(run.preparation);
			entities = static_cast<unsigned>(run.entities.result);
			break;
		}
		case 5U: {
			StageExtraRunState run;
			run.frame = setup_frame;
			run.stage_title_enabled = stage_title_enabled;
			run.demo = is_demo;
			StageExtraInvalidationPresentation invalidation{static_cast<SigmaCallbacks &>(sink),
				sink, sink, bullet_invalidation, sink, sink, sink, bullet};
			StageExtraEntitiesPresentation entities_presentation{sink,
				static_cast<SigmaCallbacks &>(sink), static_cast<MidbossxCallbacks &>(sink),
				sink, sink, sink, sink};
			StageExtraRunPresentation presentation{sink, invalidation, sink, sink, bullet,
				entities_presentation, sink, boss_intro, sink};
			wait = drive([&] { return stage_extra_run_prepare(state, run, presentation); },
				[&](std::uint16_t input) { return stage_extra_run_input(state, run, input, presentation); });
			stopped_at = boundary(run.phase);
			phase = static_cast<unsigned>(run.phase);
			preparation = static_cast<unsigned>(run.preparation);
			entities = static_cast<unsigned>(run.entities.result);
			break;
		}
		default: throw std::runtime_error("stage setup refused");
		}

		std::cout << "status=" << (wait == Stage1RunWait::refused ? "error" : "stopped")
			<< "\nboundary=" << stopped_at
			<< "\ninput-consumed=" << consumed << "\nstage-frame=" << state.stage_frame
			<< "\nstage=" << stage << "\nrank=" << rank << "\nshot=" << shot
			<< "\nspawn-rows-declared=" << state.stage_data.spawn_row_count
			<< "\nspawn-rows-available=" << state.stage_data.spawn_rows.size()
			<< "\nwait=" << static_cast<unsigned>(wait) << "\nphase=" << phase
			<< "\npreparation=" << preparation << "\nentities=" << entities
			<< "\nscore=" << state.score.score << "\nlives=" << static_cast<int>(state.score.lives)
			<< "\nrng-cursor=" << static_cast<unsigned>(state.random.cursor) << '\n';
		return wait == Stage1RunWait::refused ? 1 : 0;
	} catch (const std::exception &error) {
		std::cerr << "status=error\ndetail=" << error.what() << '\n'; return 1;
	}
}
