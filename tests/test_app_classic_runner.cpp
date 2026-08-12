#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "th03/app/classic_cli.hpp"
#include "th03/app/classic_runner.hpp"
#include "th03/app/classic_score_store.hpp"

#include "classic_runner_internal.hpp"

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

std::expected<th03::app::ClassicCliRequest, th03::app::ClassicCliError>
parse(std::initializer_list<std::string_view> arguments)
{
	return th03::app::parse_classic_cli(
		std::span<const std::string_view>(arguments.begin(), arguments.size())
	);
}

std::vector<std::string_view> valid_arguments()
{
	return {
		"--classic",
		"--data", "data-root",
		"--main", "MAIN.EXE",
		"--mainl", "MAINL.EXE",
		"--op", "OP.EXE",
		"--font", "FONT.ROM",
	};
}

void test_cli()
{
	const auto help = parse({"--help"});
	expect(help && help->show_help, "help is a no-I/O Classic parse result");
	const auto selected_help = parse({"--classic", "--help"});
	expect(selected_help && selected_help->show_help,
		"explicit Classic help is also a no-I/O parse result");
	const auto arguments = valid_arguments();
	const auto parsed = th03::app::parse_classic_cli(arguments);
	expect(parsed && !parsed->show_help &&
		parsed->launch.content.data_root == "data-root" &&
		parsed->launch.content.main_executable == "MAIN.EXE" &&
		parsed->launch.content.mainl_executable == "MAINL.EXE" &&
		parsed->launch.content.op_executable == "OP.EXE" &&
		parsed->launch.content.font_rom == "FONT.ROM" &&
		parsed->launch.content.config == "YUME.CFG" &&
		parsed->launch.content.score == "YUME.NEM" &&
		parsed->launch.seed == 1U && !parsed->launch.fm_available &&
		!parsed->rhythm_rom &&
		parsed->tick_rate.ticks == 526'315U &&
		parsed->tick_rate.seconds == 9'328U,
		"valid Classic CLI preserves external paths and the nominal PC-98 rate");
	const auto complete = parse({
		"--classic", "--data", "d", "--main", "m", "--mainl", "l",
		"--op", "o", "--font", "f", "--tick-rate", "3/2",
		"--rhythm-rom", "ym2608.rom",
		"--config", "c", "--score", "s", "--seed", "4294967295",
		"--kotohime-zero-red", "--smoke-ticks", "7",
	});
	expect(complete && complete->launch.content.config == "c" &&
		complete->launch.content.score == "s" &&
		complete->rhythm_rom == std::filesystem::path("ym2608.rom") &&
		complete->launch.seed == 0xFFFF'FFFFU &&
		complete->tick_rate.ticks == 3U && complete->tick_rate.seconds == 2U &&
		complete->kotohime_zero_red && complete->smoke_ticks == 7U,
		"optional Classic paths, rate override, seed, policy, and smoke bound parse");

	const auto unknown = parse({"--classic", "--wat"});
	const auto missing_value = parse({"--classic", "--data"});
	const auto duplicate = parse({"--classic", "--classic"});
	const auto missing_required = parse({"--classic"});
	const auto bad_rate = parse({
		"--classic", "--data", "d", "--main", "m", "--mainl", "l",
		"--op", "o", "--font", "f", "--tick-rate", "60",
	});
	const auto bad_seed = parse({
		"--classic", "--data", "d", "--main", "m", "--mainl", "l",
		"--op", "o", "--font", "f", "--tick-rate", "1/1",
		"--seed", "-1",
	});
	const auto bad_smoke = parse({
		"--classic", "--data", "d", "--main", "m", "--mainl", "l",
		"--op", "o", "--font", "f", "--tick-rate", "1/1",
		"--smoke-ticks", "0",
	});
	expect(!unknown && unknown.error().code ==
		th03::app::ClassicCliErrorCode::unknown_option,
		"unknown Classic CLI options are rejected");
	expect(!missing_value && missing_value.error().code ==
		th03::app::ClassicCliErrorCode::missing_value,
		"missing Classic CLI values are rejected");
	expect(!duplicate && duplicate.error().code ==
		th03::app::ClassicCliErrorCode::duplicate_option,
		"duplicate Classic CLI options are rejected");
	expect(!missing_required && missing_required.error().code ==
		th03::app::ClassicCliErrorCode::missing_required_option,
		"every required Classic content input remains explicit");
	expect(!bad_rate && bad_rate.error().code ==
		th03::app::ClassicCliErrorCode::invalid_tick_rate,
		"malformed Classic cadence is rejected");
	expect(!bad_seed && bad_seed.error().code ==
		th03::app::ClassicCliErrorCode::invalid_seed,
		"negative Classic seed is rejected");
	expect(!bad_smoke && bad_smoke.error().code ==
		th03::app::ClassicCliErrorCode::invalid_smoke_ticks,
		"Classic smoke bound must contain at least one logical tick");
}

void test_input_projection()
{
	const th03::app::LogicalTickInput sample{
		.tick_index = 9U,
		.deadline_ns = 123U,
		.input = {
			.p1 = 0x0F3FU,
			.p2 = 0x000AU,
			.system = 0x703FU,
			.charge_mask = 0x03U,
		},
		.app_quit_requested = true,
	};
	const th03::app::ClassicAudioStatus audio{
		.bgm_active = true,
		.measure = 7U,
		.volume = 8U,
	};
	const auto input =
		th03::app::ClassicRunnerTestAccess::project_input(sample, audio);
	expect(input.op.up && input.op.down && input.op.left && input.op.right &&
		input.op.ok && input.op.shot && input.op.cancel,
		"P1 and source-global bits project to every OP control");
	expect(input.music_room.controls.any(),
		"Music Room receives the same source interface controls");
	expect(input.high_scores.up && input.high_scores.down &&
		input.high_scores.left && input.high_scores.right &&
		input.high_scores.ok && input.high_scores.shot &&
		input.high_scores.bomb && input.high_scores.cancel,
		"registration receives all distinct source controls");
	expect(input.selection.single_player == 0x703FU &&
		input.selection.player_one == 0x0F3FU &&
		input.selection.player_two == 0x000AU,
		"selection keeps global and two-player source input domains distinct");
	expect(input.main.aggregate.p1 == sample.input.p1 &&
		input.main.aggregate.p2 == sample.input.p2 &&
		input.main.aggregate.system == sample.input.system &&
		input.main.aggregate.charge_mask == sample.input.charge_mask &&
		!input.main.physical_domains,
		"MAIN receives the exact latched CoreInput");
	expect(input.story.main.aggregate.system == 0x703FU &&
		input.story.cutscene.script.cancel_held &&
		input.story.cutscene.script.any_key_held &&
		input.story.cutscene.shot_or_ok_held &&
		input.story.staff.any_input &&
		!input.story.staff.vsync_count_above_one,
		"Story child inputs preserve source semantics without inventing overrun");
	expect(input.story.continue_menu.left &&
		input.story.continue_menu.right && input.story.continue_menu.ok &&
		input.story.continue_menu.shot && input.story.continue_menu.cancel,
		"Continue receives its exact directional/confirm/cancel projection");
	expect(input.audio.bgm_active && input.audio.measure == 7U &&
		input.audio.volume == 8U && input.story.audio.bgm_active &&
		input.story.audio.measure == 7U && input.story.audio.volume == 8U &&
		input.story.cutscene.audio.bgm_active &&
		input.story.cutscene.audio.measure == 7U &&
		input.story.cutscene.audio.volume == 8U &&
		input.story.staff.audio.bgm_active &&
		input.story.staff.audio.measure == 7U &&
		input.story.staff.audio.volume == 8U,
		"one audio snapshot reaches every active Classic child");
	expect(input.dismiss_pressed && input.app_quit_requested &&
		input.story.dismiss_pressed && input.story.app_quit_requested,
		"input_sp dismisses source waits while app quit stays separate");

	const auto player_only =
		th03::app::ClassicRunnerTestAccess::project_input({
			.tick_index = 10U,
			.deadline_ns = 124U,
			.input = {.p1 = 0x000FU, .p2 = 0x0030U},
		});
	expect(
		!player_only.op.any() && !player_only.high_scores.up &&
			!player_only.selection.single_player &&
			!player_only.dismiss_pressed &&
			!player_only.story.dismiss_pressed,
		"player-only controls cannot masquerade as the donor input_sp domain"
	);

	const auto live = th03::app::ClassicRunnerTestAccess::project_input({
		.tick_index = 11U,
		.deadline_ns = 125U,
		.input = {
			.p1 = 0x0029U,
			.p2 = 0x0002U,
			.system = 0x202CU,
			.charge_mask = 0x03U,
		},
		.physical_domains = th03::app::PhysicalInputDomains{
			.keyboard = {
				.p1 = 0x0001U,
				.p2 = 0x0002U,
				.system = 0x2004U,
				.charge_mask = 0x03U,
			},
			.first_joystick = 0x0028U,
			.first_joystick_present = true,
		},
	});
	expect(live.op.up && live.op.left && live.op.right && live.op.shot &&
		live.op.ok && live.dismiss_pressed,
		"live interface input merges keyboard system, keyboard P1, and joystick zero");
	expect(live.selection.single_player == 0x202DU &&
		live.selection.player_one == 0x0001U &&
		live.selection.player_two == 0x0002U &&
		live.selection.physical_domains.has_value(),
		"live selection preserves its raw keyboard and joystick source domains");
	expect(live.main.aggregate.p1 == 0x0029U &&
		live.main.physical_domains.has_value() &&
		live.story.main.physical_domains.has_value(),
		"live MAIN projections retain aggregate replay input beside physical domains");
}

void test_fresh_resident_state()
{
	const auto resident =
		th03::app::ClassicRunnerTestAccess::initial_resident(0xFEDCBA98U);
	expect(resident.remaining_credits == 0U && resident.story_stage == 0U &&
		resident.show_score_menu == 0U && resident.game_mode ==
			th03::game_mode::none &&
		std::bit_cast<std::uint32_t>(resident.resident_rand) == 0xFEDCBA98U &&
		resident.random_seed == 0xFEDCBA98U,
		"fresh OP resident preserves zero credits until Story selection");
}

struct FakeRunnerBackend final {
	std::size_t consume_count = 0U;
	std::size_t compose_count = 0U;
	th03::app::ClassicOpSessionInput last_input{};
	th03::app::ClassicOpSessionStatus next_status =
		th03::app::ClassicOpSessionStatus::running;
	bool fail_consume = false;
	bool fail_compose = false;
	std::optional<th03::app::ClassicAudioCommand> audio{};
};

th03::app::ClassicRunnerOperations runner_operations(
	FakeRunnerBackend &backend
) noexcept
{
	return {
		.context = &backend,
		.consume = [](void *context, const auto &input) {
			auto &self = *static_cast<FakeRunnerBackend *>(context);
			++self.consume_count;
			self.last_input = input;
			if(self.fail_consume) {
				return std::expected<
					th03::app::ClassicOpSessionTick,
					th03::app::ClassicOpSessionError
				>{std::unexpected(th03::app::ClassicOpSessionError{
					.code = th03::app::ClassicOpSessionErrorCode::title_failed,
				})};
			}
			const auto status = input.app_quit_requested ?
				th03::app::ClassicOpSessionStatus::quit_requested :
				self.next_status;
			return std::expected<
				th03::app::ClassicOpSessionTick,
				th03::app::ClassicOpSessionError
			>{th03::app::ClassicOpSessionTick{
				.scene = th03::app::ClassicOpSessionScene::title_menu,
				.status = status,
				.surface_changed = true,
				.input_sampled = true,
			}};
		},
		.take_audio_command = [](void *context) {
			auto &self = *static_cast<FakeRunnerBackend *>(context);
			auto command = self.audio;
			self.audio.reset();
			return command;
		},
		.compose_shown_surface = [](void *context, auto output) {
			auto &self = *static_cast<FakeRunnerBackend *>(context);
			++self.compose_count;
			if(self.fail_compose) {
				return std::expected<
					th03::present::SourceSurface,
					th03::app::ClassicOpSessionError
				>{std::unexpected(th03::app::ClassicOpSessionError{
					.code = th03::app::ClassicOpSessionErrorCode::surface_failed,
				})};
			}
			std::fill(
				output.begin(), output.end(), th03::present::ComposedPixel{}
			);
			return std::expected<
				th03::present::SourceSurface,
				th03::app::ClassicOpSessionError
			>{th03::present::SourceSurface{.pixels = output}};
		},
	};
}

std::unique_ptr<th03::app::ClassicRunner> make_fake_runner(
	const std::shared_ptr<FakeRunnerBackend> &backend
)
{
	auto created = th03::app::ClassicRunnerTestAccess::create(
		std::static_pointer_cast<void>(backend), runner_operations(*backend)
	);
	expect(static_cast<bool>(created), "private fake Classic runner creates");
	return created ? std::move(*created) : nullptr;
}

void test_runner_sequence_audio_surface_and_failure()
{
	auto invalid = th03::app::ClassicRunnerTestAccess::create({}, {});
	expect(
		!invalid && invalid.error().code ==
			th03::app::ClassicRunnerErrorCode::invalid_request,
		"invalid private runner operations are rejected"
	);

	auto backend = std::make_shared<FakeRunnerBackend>();
	backend->audio = th03::app::ClassicAudioCommand{
		.type = th03::app::ClassicAudioCommandType::play_song,
		.cue = th03::app::ClassicMusicCue::menu_main,
	};
	auto runner = make_fake_runner(backend);
	if(!runner) {
		return;
	}
	std::array<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	> pixels{};
	const auto composed = runner->compose_shown_surface(pixels);
	expect(composed && backend->compose_count == 1U,
		"runner delegates one exact source-surface composition");
	expect(
		runner->take_audio_command() == th03::app::ClassicAudioCommand{
			.type = th03::app::ClassicAudioCommandType::play_song,
			.cue = th03::app::ClassicMusicCue::menu_main,
		} && !runner->take_audio_command(),
		"runner forwards each semantic audio command exactly once"
	);

	auto first = runner->consume({
		.tick_index = 0U,
		.deadline_ns = 10U,
		.input = {.system = 0x0020U},
	});
	expect(
		first && first->status == th03::app::ClassicRunnerStatus::running &&
			first->surface_changed && first->input_sampled &&
			backend->consume_count == 1U &&
			backend->last_input.op.shot &&
			backend->last_input.audio.volume == 255U,
		"one logical sample reaches the session with the silent-audio default"
	);
	const auto skipped = runner->consume({
		.tick_index = 2U,
		.deadline_ns = 11U,
	});
	expect(
		!skipped && skipped.error().code ==
			th03::app::ClassicRunnerErrorCode::invalid_sample_order &&
			backend->consume_count == 1U &&
			runner->status() == th03::app::ClassicRunnerStatus::running,
		"skipped tick rejection is atomic and non-sticky"
	);
	backend->next_status = th03::app::ClassicOpSessionStatus::complete;
	const auto completed = runner->consume({
		.tick_index = 1U,
		.deadline_ns = 10U,
	});
	expect(
		completed && completed->status ==
			th03::app::ClassicRunnerStatus::session_complete &&
			backend->consume_count == 2U,
		"equal deadlines remain legal and a complete session closes once"
	);
	const auto after_complete = runner->consume({
		.tick_index = 2U,
		.deadline_ns = 12U,
	});
	expect(
		!after_complete && after_complete.error().code ==
			th03::app::ClassicRunnerErrorCode::runner_not_running &&
			backend->consume_count == 2U,
		"a completed runner cannot advance the retained session again"
	);

	auto quit_backend = std::make_shared<FakeRunnerBackend>();
	auto quit_runner = make_fake_runner(quit_backend);
	if(quit_runner) {
		const auto quit = quit_runner->consume({
			.tick_index = 0U,
			.deadline_ns = 1U,
			.app_quit_requested = true,
		});
		expect(
			quit && quit->status ==
				th03::app::ClassicRunnerStatus::quit_requested &&
				quit_backend->last_input.app_quit_requested,
			"application quit stays separate and reaches the private owner once"
		);
	}

	auto failed_backend = std::make_shared<FakeRunnerBackend>();
	failed_backend->fail_consume = true;
	auto failed_runner = make_fake_runner(failed_backend);
	if(failed_runner) {
		const auto failed = failed_runner->consume({});
		const auto repeated = failed_runner->consume({});
		expect(
			!failed && !repeated && failed.error().code ==
				th03::app::ClassicRunnerErrorCode::session_failed &&
				repeated.error().detail_code == static_cast<std::uint8_t>(
					th03::app::ClassicOpSessionErrorCode::title_failed
				) && failed_backend->consume_count == 1U &&
				failed_runner->status() == th03::app::ClassicRunnerStatus::failed,
			"session failure is sticky and never retries a partially advanced owner"
		);
	}

	auto surface_backend = std::make_shared<FakeRunnerBackend>();
	surface_backend->fail_compose = true;
	auto surface_runner = make_fake_runner(surface_backend);
	if(surface_runner) {
		const auto failed = surface_runner->compose_shown_surface(pixels);
		expect(
			!failed && failed.error().code ==
				th03::app::ClassicRunnerErrorCode::surface_failed,
			"surface failure retains the nested private diagnostic boundary"
		);
	}
}

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		const auto suffix = std::chrono::steady_clock::now()
			.time_since_epoch().count();
		path_ = std::filesystem::temp_directory_path() /
			("th03-classic-runner-" + std::to_string(suffix));
		std::filesystem::create_directories(path_);
	}
	~TemporaryDirectory()
	{
		std::error_code ignored;
		std::filesystem::remove_all(path_, ignored);
	}
	[[nodiscard]] const std::filesystem::path &path() const noexcept
	{
		return path_;
	}

private:
	std::filesystem::path path_{};
};

bool write_bytes(
	const std::filesystem::path &path,
	std::span<const std::byte> bytes
)
{
	std::ofstream output(path, std::ios::binary);
	if(!output) {
		return false;
	}
	if(!bytes.empty()) {
		output.write(
			reinterpret_cast<const char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	return static_cast<bool>(output);
}

void test_file_and_request_boundaries()
{
	TemporaryDirectory temporary;
	const auto missing = th03::app::ClassicRunnerTestAccess::read_file(
		temporary.path() / "missing.bin",
		th03::app::ClassicContentFile::mainl_executable
	);
	const auto directory = th03::app::ClassicRunnerTestAccess::read_file(
		temporary.path(), th03::app::ClassicContentFile::op_executable
	);
	const std::array bytes{
		std::byte{0x11U}, std::byte{0x22U}, std::byte{0x33U},
	};
	const auto path = temporary.path() / "bytes.bin";
	expect(write_bytes(path, bytes), "temporary Classic input is written");
	const auto wrong_size = th03::app::ClassicRunnerTestAccess::read_file(
		path, th03::app::ClassicContentFile::font_rom, 4U
	);
	const auto loaded = th03::app::ClassicRunnerTestAccess::read_file(
		path, th03::app::ClassicContentFile::font_rom, bytes.size()
	);
	expect(!missing && missing.error().code ==
		th03::app::ClassicRunnerErrorCode::file_missing &&
		missing.error().file ==
			th03::app::ClassicContentFile::mainl_executable,
		"missing Classic executable reports only its semantic role");
	expect(!directory && directory.error().code ==
		th03::app::ClassicRunnerErrorCode::file_not_regular &&
		directory.error().file == th03::app::ClassicContentFile::op_executable,
		"Classic directory input is rejected as non-regular");
	expect(!wrong_size && wrong_size.error().code ==
		th03::app::ClassicRunnerErrorCode::file_size_mismatch,
		"Classic exact-size boundary rejects a wrong FONT length");
	expect(loaded && *loaded == std::vector<std::byte>(bytes.begin(), bytes.end()),
		"Classic checked file boundary preserves every caller-owned byte");
	const auto invalid = th03::app::ClassicRunner::load({});
	expect(!invalid && invalid.error().code ==
		th03::app::ClassicRunnerErrorCode::invalid_request,
		"invalid Classic request fails before touching external content");
}

[[nodiscard]] std::uint64_t surface_digest(
	std::span<
		const th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	> pixels,
	const th03::present::SourceSurface &surface
) noexcept
{
	constexpr std::uint64_t offset = 1'469'598'103'934'665'603ULL;
	constexpr std::uint64_t prime = 1'099'511'628'211ULL;
	auto result = offset;
	const auto add = [&](std::uint8_t value) {
		result = (result ^ value) * prime;
	};
	for(const auto &pixel : pixels) {
		add(static_cast<std::uint8_t>(pixel.layer));
		add(pixel.value);
	}
	for(const auto &color : surface.graphics_palette.colors) {
		add(color.r);
		add(color.g);
		add(color.b);
	}
	add(static_cast<std::uint8_t>(surface.shown_page));
	add(static_cast<std::uint8_t>(surface.raster_mode));
	add(static_cast<std::uint8_t>(surface.effective_scroll_y & 0xFFU));
	add(static_cast<std::uint8_t>(surface.effective_scroll_y >> 8U));
	return result;
}

struct AuthenticRouteTraceEvent final {
	std::uint64_t host_tick = 0U;
	std::uint32_t source_sample = 0U;
	th03::app::ClassicOpSessionScene op_scene =
		th03::app::ClassicOpSessionScene::failed;
	std::optional<th03::app::ClassicStorySessionScene> story_scene{};
	std::uint64_t surface = 0U;
	std::size_t audio_command_count = 0U;
};

bool normalized_kotohime_red(
	void *,
	const th03::BombFrameStep &,
	std::uint8_t fade,
	std::uint8_t &red
) noexcept
{
	red = static_cast<std::uint8_t>(fade << 1U);
	return true;
}

bool authentic_gate(
	const std::filesystem::path &data_root,
	const std::filesystem::path &main_executable,
	const std::filesystem::path &mainl_executable,
	const std::filesystem::path &op_executable,
	const std::filesystem::path &font_rom
)
{
	TemporaryDirectory temporary;
	auto loaded = th03::app::ClassicRunner::load({
		.content = {
			.data_root = data_root,
			.main_executable = main_executable,
			.mainl_executable = mainl_executable,
			.op_executable = op_executable,
			.font_rom = font_rom,
			.config = temporary.path() / "YUME.CFG",
			.score = temporary.path() / "YUME.NEM",
		},
		.seed = 1U,
		.fm_available = false,
		.kotohime_compatibility = {
			.red_provider = &normalized_kotohime_red,
		},
	});
	if(!loaded) {
		std::cerr << "authentic Classic runner load failed code="
			<< static_cast<unsigned>(loaded.error().code)
			<< " detail=" << static_cast<unsigned>(loaded.error().detail_code)
			<< '\n';
		return false;
	}
	auto &runner = **loaded;
	std::array<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	> pixels{};
	if(!runner.compose_shown_surface(pixels)) {
		return false;
	}
	std::vector<th03::app::ClassicAudioCommand> audio_commands{};
	auto drain_audio = [&runner, &audio_commands]() {
		while(auto command = runner.take_audio_command()) {
			audio_commands.push_back(*command);
		}
	};
	const auto graphics_ink = [&pixels](
		std::int32_t left,
		std::int32_t top,
		std::uint16_t width,
		std::uint16_t height
	) {
		for(std::int32_t y = top; y < top + height; ++y) {
			for(std::int32_t x = left; x < left + width; ++x) {
				const auto &pixel = pixels[
					(static_cast<std::size_t>(y) *
						th03::present::graphics_width) +
					static_cast<std::size_t>(x)
				];
				if(pixel.layer ==
						th03::present::ComposedPixelLayer::graphics &&
					pixel.value == 15U) {
					return true;
				}
			}
		}
		return false;
	};
	drain_audio();
	std::uint64_t tick_index = 0U;
	std::optional<th03::app::ClassicRunnerConsumeResult> last{};
	std::optional<th03::present::SourceSurface> last_surface{};
	const auto composed_initial = runner.compose_shown_surface(pixels);
	if(!composed_initial) {
		return false;
	}
	last_surface = *composed_initial;
	const auto step = [&](std::uint16_t system = 0U, bool allow_complete = false) {
		const auto story_before =
			th03::app::ClassicRunnerTestAccess::story_scene(runner);
		th03::CoreInput input{};
		input.system = system;
		auto advanced = runner.consume({
			.tick_index = tick_index,
			.deadline_ns = tick_index * 10U,
			.input = input,
		});
		++tick_index;
		if(!advanced || (advanced->status !=
			th03::app::ClassicRunnerStatus::running && !(allow_complete &&
			advanced->status ==
				th03::app::ClassicRunnerStatus::session_complete))) {
			std::cerr << "authentic-script-step=" << tick_index - 1U;
			if(!advanced) {
				std::cerr << " code=" << static_cast<unsigned>(
					advanced.error().code
				) << " detail=" << static_cast<unsigned>(
					advanced.error().detail_code
				);
			}
			std::cerr << " op=" << static_cast<unsigned>(
				th03::app::ClassicRunnerTestAccess::scene(runner).value_or(
					th03::app::ClassicOpSessionScene::failed
				)
			) << " story=" << static_cast<unsigned>(
				th03::app::ClassicRunnerTestAccess::story_scene(runner).value_or(
					th03::app::ClassicStorySessionScene::failed
				)
			) << " story-error=" << static_cast<unsigned>(
				th03::app::ClassicRunnerTestAccess::story_failure_code(
					runner
				).value_or(
					th03::app::ClassicStorySessionErrorCode::invalid_config
				)
			) << " story-before=" << static_cast<unsigned>(
				story_before.value_or(
					th03::app::ClassicStorySessionScene::failed
				)
			) << " story-detail=" << static_cast<unsigned>(
				th03::app::ClassicRunnerTestAccess::story_failure_detail_code(
					runner
				).value_or(0xFFU)
			);
			std::cerr << '\n';
			return false;
		}
		last = *advanced;
		drain_audio();
		if(!advanced->surface_changed) {
			return true;
		}
		auto composed = runner.compose_shown_surface(pixels);
		if(!composed) {
			return false;
		}
		last_surface = *composed;
		return true;
	};

	// Reach the first title input poll, open the menu, and lock the exact
	// TRAM-only publication that was previously dropped by the session wrapper.
	bool reached_input = false;
	for(std::size_t frame = 0U; frame < 300U && !reached_input; ++frame) {
		if(!step()) { return false; }
		reached_input = last->input_sampled;
	}
	if(!reached_input || !step(0x0020U)) { return false; }
	bool menu_drawn = false;
	for(std::size_t frame = 0U; frame < 32U && !menu_drawn; ++frame) {
		if(!step()) { return false; }
		menu_drawn = last->input_sampled;
	}
	if(!menu_drawn || !last->surface_changed || std::ranges::none_of(
		pixels, [](const auto &pixel) {
			return pixel.layer == th03::present::ComposedPixelLayer::text;
		}
	)) {
		std::cerr << "authentic-script-stage=menu tick=" << tick_index << '\n';
		return false;
	}

	// Navigate away and back with the source-global arrow channel, then enter
	// Story. This distinguishes working UI directions from Shot-only control.
	if(!step(0x0002U) || !last->surface_changed || !step() ||
		!step(0x0001U) || !last->surface_changed || !step() ||
		!step(0x0020U) ||
		th03::app::ClassicRunnerTestAccess::scene(runner) !=
			th03::app::ClassicOpSessionScene::character_selection) {
		std::cerr << "authentic-script-stage=title-route tick=" << tick_index
			<< " scene=" << static_cast<unsigned>(
				th03::app::ClassicRunnerTestAccess::scene(runner).value_or(
					th03::app::ClassicOpSessionScene::failed
				)
			) << '\n';
		return false;
	}

	// Wait for the selector's first poll, move Reimu->Mima->Reimu with arrows,
	// release each latch, and confirm. Every changed frame is composed so the
	// ready-with-pending-Stop ownership boundary is exercised, not bypassed.
	reached_input = false;
	for(std::size_t frame = 0U; frame < 64U && !reached_input; ++frame) {
		if(!step()) { return false; }
		reached_input = last->input_sampled;
	}
	const auto hold_until_sampled = [&](std::uint16_t system) {
		for(std::size_t frame = 0U; frame < 8U; ++frame) {
			if(!step(system)) { return false; }
			if(last->input_sampled) { return true; }
		}
		return false;
	};
	if(!reached_input || !hold_until_sampled(0x0002U) ||
		!hold_until_sampled(0U) || !hold_until_sampled(0x0001U) ||
		!hold_until_sampled(0U) || !hold_until_sampled(0x0020U)) {
		return false;
	}
	for(std::size_t frame = 0U; frame < 256U; ++frame) {
		if(!step()) { return false; }
		if(th03::app::ClassicRunnerTestAccess::scene(runner) ==
			th03::app::ClassicOpSessionScene::story) {
			break;
		}
	}
	if(th03::app::ClassicRunnerTestAccess::scene(runner) !=
		th03::app::ClassicOpSessionScene::story) {
		std::cerr << "authentic-script-stage=selection-timeout tick="
			<< tick_index << " scene=" << static_cast<unsigned>(
				th03::app::ClassicRunnerTestAccess::scene(runner).value_or(
					th03::app::ClassicOpSessionScene::failed
				)
			) << '\n';
		return false;
	}

	// Drive the real MAINL splash into its input window, dismiss it, complete
	// both palette fades, and require the first MAIN simulation frame. This is
	// the supplied-content regression for the former moved-from Raster failure
	// that surfaced only after selecting a character in the windowed build.
	reached_input = false;
	for(std::size_t frame = 0U; frame < 64U && !reached_input; ++frame) {
		if(!step()) { return false; }
		reached_input = last->input_sampled;
	}
	if(!reached_input) {
		std::cerr << "authentic-script-stage=splash-input-timeout tick="
			<< tick_index << '\n';
		return false;
	}
	if(!graphics_ink(80, 292, 240U, 16U) ||
		!graphics_ink(128, 308, 176U, 16U) ||
		!graphics_ink(336, 292, 240U, 16U) ||
		!graphics_ink(384, 308, 176U, 16U)) {
		std::cerr << "authentic-script-stage=splash-text-blank tick="
			<< tick_index << '\n';
		return false;
	}
	const auto first_splash_host_tick = tick_index;
	const auto first_splash_source_sample =
		th03::app::ClassicRunnerTestAccess::story_source_sample_count(
			runner
		).value_or(0U);
	const auto first_splash_surface = last_surface ?
		surface_digest(pixels, *last_surface) : 0U;
	const auto first_splash_audio_count = audio_commands.size();
	for(std::size_t frame = 0U; frame < 33U; ++frame) {
		if(!step()) { return false; }
	}
	if(!step(0x0020U)) { return false; }
	bool first_main_frame = false;
	for(std::size_t frame = 0U; frame < 96U; ++frame) {
		if(!step()) { return false; }
		if(last->process_advanced) {
			first_main_frame = true;
			break;
		}
	}
	if(!first_main_frame) {
		std::cerr << "authentic-script-stage=main-start-timeout tick="
			<< tick_index << '\n';
		return false;
	}
	auto first_forced = th03::app::ClassicRunnerTestAccess::
		force_story_player_win(runner);
	if(!first_forced) {
		std::cerr << "authentic-script-stage=force-first-win tick="
			<< tick_index << '\n';
		return false;
	}

	// From this independently verified MAIN boundary, retain every production
	// resource, scene, surface, audio, and persistence path while replacing only
	// the terminal outcome. A scene-transition trace distinguishes host ticks
	// from donor source samples and fingerprints every published milestone.
	std::array<std::size_t, 10U> story_scene_entries{};
	std::vector<AuthenticRouteTraceEvent> trace{{
		.host_tick = first_splash_host_tick,
		.source_sample = first_splash_source_sample,
		.op_scene = th03::app::ClassicOpSessionScene::story,
		.story_scene = th03::app::ClassicStorySessionScene::stage_splash,
		.surface = first_splash_surface,
		.audio_command_count = first_splash_audio_count,
	}};
	++story_scene_entries[static_cast<std::size_t>(
		th03::app::ClassicStorySessionScene::stage_splash
	)];
	auto previous_op_scene = th03::app::ClassicOpSessionScene::story;
	std::optional<th03::app::ClassicStorySessionScene> previous_story_scene =
		th03::app::ClassicStorySessionScene::stage_splash;
	const auto record_transition = [&]() {
		const auto op_scene = th03::app::ClassicRunnerTestAccess::scene(runner);
		if(!op_scene) {
			return false;
		}
		const auto story_scene =
			th03::app::ClassicRunnerTestAccess::story_scene(runner);
		if(*op_scene == previous_op_scene &&
			story_scene == previous_story_scene) {
			return true;
		}
		if(story_scene) {
			const auto index = static_cast<std::size_t>(*story_scene);
			if(index >= story_scene_entries.size()) {
				return false;
			}
			++story_scene_entries[index];
		}
		if(!last_surface) {
			return false;
		}
		trace.push_back({
			.host_tick = tick_index,
			.source_sample = th03::app::ClassicRunnerTestAccess::
				story_source_sample_count(runner).value_or(0U),
			.op_scene = *op_scene,
			.story_scene = story_scene,
			.surface = surface_digest(pixels, *last_surface),
			.audio_command_count = audio_commands.size(),
		});
		previous_op_scene = *op_scene;
		previous_story_scene = story_scene;
		return true;
	};
	if(!record_transition()) {
		return false;
	}

	std::size_t real_main_frames = 1U;
	bool main_forced = true;
	bool returned_to_title = false;
	for(std::size_t frame = 0U; frame < 250'000U; ++frame) {
		const auto before = th03::app::ClassicRunnerTestAccess::story_scene(runner);
		const std::uint16_t system = before && *before !=
			th03::app::ClassicStorySessionScene::main &&
			((tick_index & 1U) == 0U) ? 0x0020U : 0U;
		if(!step(system)) {
			return false;
		}
		const auto after = th03::app::ClassicRunnerTestAccess::story_scene(runner);
		if(after != before && after ==
			th03::app::ClassicStorySessionScene::main) {
			main_forced = false;
		}
		if(after == th03::app::ClassicStorySessionScene::main &&
			last->process_advanced && !main_forced) {
			auto forced = th03::app::ClassicRunnerTestAccess::
				force_story_player_win(runner);
			if(!forced) {
				std::cerr << "authentic-script-stage=force-win tick="
					<< tick_index << " code=" << static_cast<unsigned>(
						forced.error().code
					) << " detail=" << static_cast<unsigned>(
						forced.error().detail_code
					) << '\n';
				return false;
			}
			main_forced = true;
			++real_main_frames;
		}
		if(!record_transition()) {
			return false;
		}
		if(th03::app::ClassicRunnerTestAccess::scene(runner) ==
			th03::app::ClassicOpSessionScene::title_menu) {
			returned_to_title = true;
			break;
		}
	}
	if(!returned_to_title) {
		std::cerr << "authentic-script-stage=route-timeout tick="
			<< tick_index << " story=" << static_cast<unsigned>(
				th03::app::ClassicRunnerTestAccess::story_scene(runner).value_or(
					th03::app::ClassicStorySessionScene::failed
				)
			) << '\n';
		return false;
	}

	const auto scene_count = [&](th03::app::ClassicStorySessionScene scene) {
		return story_scene_entries[static_cast<std::size_t>(scene)];
	};
	const auto trace_valid = real_main_frames == 9U &&
		scene_count(th03::app::ClassicStorySessionScene::stage_splash) == 9U &&
		scene_count(th03::app::ClassicStorySessionScene::main) == 9U &&
		scene_count(th03::app::ClassicStorySessionScene::win) == 7U &&
		scene_count(th03::app::ClassicStorySessionScene::cutscene) == 4U &&
		scene_count(th03::app::ClassicStorySessionScene::staff_verdict) == 1U &&
		scene_count(
			th03::app::ClassicStorySessionScene::high_score_registration
		) == 1U && trace.size() >= 32U &&
		std::ranges::all_of(trace, [](const auto &event) {
			return event.surface != 0U;
		}) && std::ranges::any_of(audio_commands, [](const auto &command) {
			return command.type == th03::app::ClassicAudioCommandType::load_song;
		}) && std::ranges::any_of(audio_commands, [](const auto &command) {
			return command.type == th03::app::ClassicAudioCommandType::play_song;
		});
	if(!trace_valid) {
		std::cerr << "authentic-script-stage=trace main=" << real_main_frames
			<< " splash=" << scene_count(
				th03::app::ClassicStorySessionScene::stage_splash
			) << " win=" << scene_count(
				th03::app::ClassicStorySessionScene::win
			) << " cutscene=" << scene_count(
				th03::app::ClassicStorySessionScene::cutscene
			) << " staff=" << scene_count(
				th03::app::ClassicStorySessionScene::staff_verdict
			) << " registration=" << scene_count(
				th03::app::ClassicStorySessionScene::high_score_registration
			) << " events=" << trace.size() << " audio="
			<< audio_commands.size() << '\n';
		return false;
	}

	const auto score = th03::app::load_classic_score_store(
		temporary.path() / "YUME.NEM"
	);
	const std::array expected_name{
		th03::app::ClassicRegistrationGlyph::space,
		th03::app::ClassicRegistrationGlyph::space,
		th03::app::ClassicRegistrationGlyph::space,
		th03::app::ClassicRegistrationGlyph::u,
		th03::app::ClassicRegistrationGlyph::m,
		th03::app::ClassicRegistrationGlyph::i,
		th03::app::ClassicRegistrationGlyph::e,
		th03::app::ClassicRegistrationGlyph::r,
	};
	if(!score || score->document.boards[1U].cleared !=
		th03::app::ClassicScoreCleared::cleared ||
		score->document.boards[1U].rows[0U].playchar_optional != 1U ||
		score->document.boards[1U].rows[0U].name_backwards != expected_name) {
		std::cerr << "authentic-script-stage=score-persistence";
		if(score) {
			for(std::size_t rank = 0U; rank <
				score->document.boards.size(); ++rank) {
				const auto &board = score->document.boards[rank];
				std::cerr << " rank" << rank << "-cleared="
					<< static_cast<unsigned>(board.cleared)
					<< "-char=" << static_cast<unsigned>(
						board.rows[0U].playchar_optional
					) << "-name=";
				for(const auto glyph : board.rows[0U].name_backwards) {
					std::cerr << static_cast<unsigned>(glyph) << ',';
				}
			}
		}
		std::cerr << '\n';
		return false;
	}

	// The returned title is a fresh real scene. Wait for its source input poll,
	// open the menu, and exit through Cancel instead of ending the harness at an
	// application-private ownership boundary.
	reached_input = false;
	for(std::size_t frame = 0U; frame < 300U && !reached_input; ++frame) {
		if(!step()) { return false; }
		reached_input = last->input_sampled;
	}
	if(!reached_input || !step(0x0020U)) {
		return false;
	}
	menu_drawn = false;
	for(std::size_t frame = 0U; frame < 32U && !menu_drawn; ++frame) {
		if(!step()) { return false; }
		menu_drawn = last->input_sampled;
	}
	if(!menu_drawn || !step() || !step(0x1000U, true)) {
		return false;
	}
	for(std::size_t frame = 0U; frame < 8U && runner.status() ==
		th03::app::ClassicRunnerStatus::running; ++frame) {
		if(!step(0U, true)) {
			return false;
		}
	}
	return runner.status() ==
		th03::app::ClassicRunnerStatus::session_complete;
}

} // namespace

int main(int argc, char **argv)
{
	test_cli();
	test_input_projection();
	test_fresh_resident_state();
	test_runner_sequence_audio_surface_and_failure();
	test_file_and_request_boundaries();
	if(argc == 7 && std::string_view(argv[1]) == "--authentic") {
		expect(authentic_gate(argv[2], argv[3], argv[4], argv[5], argv[6]),
			"supplied originals construct and advance the complete Classic runner");
	} else if(argc != 1) {
		std::cerr << "usage: th03-app-classic-runner-tests [--authentic "
			"<data-root> <MAIN.EXE> <MAINL.EXE> <OP.EXE> <FONT.ROM>]\n";
		return 2;
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
