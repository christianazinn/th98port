#include <algorithm>
#include <array>
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
		"--tick-rate", "56423/1000",
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
		parsed->tick_rate.ticks == 56423U &&
		parsed->tick_rate.seconds == 1000U,
		"valid Classic CLI preserves external paths and deterministic defaults");
	const auto complete = parse({
		"--classic", "--data", "d", "--main", "m", "--mainl", "l",
		"--op", "o", "--font", "f", "--tick-rate", "3/2",
		"--config", "c", "--score", "s", "--seed", "4294967295",
		"--kotohime-zero-red", "--smoke-ticks", "7",
	});
	expect(complete && complete->launch.content.config == "c" &&
		complete->launch.content.score == "s" &&
		complete->launch.seed == 0xFFFF'FFFFU &&
		complete->kotohime_zero_red && complete->smoke_ticks == 7U,
		"optional Classic paths, full seed domain, policy, and smoke bound parse");

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
		"every Classic content input and cadence remains explicit");
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
	expect(input.main.p1 == sample.input.p1 &&
		input.main.p2 == sample.input.p2 &&
		input.main.system == sample.input.system &&
		input.main.charge_mask == sample.input.charge_mask,
		"MAIN receives the exact latched CoreInput");
	expect(input.story.main.system == 0x703FU &&
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
	while(runner.take_audio_command()) {
	}
	for(std::uint64_t tick = 0U; tick < 8U; ++tick) {
		auto advanced = runner.consume({
			.tick_index = tick,
			.deadline_ns = tick * 10U,
		});
		if(!advanced || advanced->status !=
			th03::app::ClassicRunnerStatus::running) {
			return false;
		}
		while(runner.take_audio_command()) {
		}
	}
	return runner.compose_shown_surface(pixels).has_value();
}

} // namespace

int main(int argc, char **argv)
{
	test_cli();
	test_input_projection();
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
