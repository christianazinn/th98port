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

#include "th03/app/classic_audio_session.hpp"
#include "th03/app/classic_cli.hpp"
#include "th03/app/classic_config_store.hpp"
#include "th03/app/classic_runner.hpp"
#include "th03/app/classic_score_store.hpp"
#include "th03/app/new_classic_language.hpp"
#include "th03/app/new_classic_score_store.hpp"
#include "th03/app/new_classic_settings_store.hpp"
#include "th03/app/replay_slot_store.hpp"
#include "th03/assets/decode.hpp"
#include "th03/formats/v13.hpp"

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

std::optional<std::vector<std::byte>> read_bytes(
	const std::filesystem::path &path
)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if(!input) {
		return std::nullopt;
	}
	const auto end = input.tellg();
	if(end < 0) {
		return std::nullopt;
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(end));
	input.seekg(0, std::ios::beg);
	if(!bytes.empty() && !input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	)) {
		return std::nullopt;
	}
	return bytes;
}

bool valid_completed_replay(
	std::optional<th03::formats::ReplayDocument> document,
	th03::formats::ReplayEndReason reason,
	std::size_t minimum_checkpoints = 1U,
	bool exact_checkpoint_count = false
)
{
	if(!document || !document->metadata ||
		document->metadata->status != th03::formats::ReplayStatus::finalized ||
		document->metadata->end_reason != reason ||
		document->checkpoints.size() < minimum_checkpoints ||
		(exact_checkpoint_count &&
			document->checkpoints.size() != minimum_checkpoints) ||
		document->expected_round_splits.empty()) {
		return false;
	}
	auto encoded = th03::formats::write_v13_replay(*document);
	return encoded && th03::formats::parse_v13_replay(*encoded).has_value();
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
	const auto shell_help = parse({"--shell", "--help"});
	expect(shell_help && shell_help->show_help,
		"explicit product-shell help is also a no-I/O parse result");
	const auto arguments = valid_arguments();
	const auto parsed = th03::app::parse_classic_cli(arguments);
	expect(parsed && !parsed->show_help &&
		parsed->mode == th03::app::ClassicLaunchMode::classic &&
		parsed->launch.content.data_root == "data-root" &&
		parsed->launch.content.main_executable == "MAIN.EXE" &&
		parsed->launch.content.mainl_executable == "MAINL.EXE" &&
		parsed->launch.content.op_executable == "OP.EXE" &&
		parsed->launch.content.font_rom == "FONT.ROM" &&
		parsed->launch.content.config ==
			std::filesystem::path("data-root") / "YUME.CFG" &&
		parsed->launch.content.new_classic_settings ==
			std::filesystem::path("data-root") / "PODD.CFG" &&
		parsed->launch.content.score ==
			std::filesystem::path("data-root") / "YUME.NEM" &&
		parsed->replay_root ==
			std::filesystem::path("data-root") / "REPLAY" &&
		parsed->launch.seed == 0U && !parsed->launch.fm_available &&
		!parsed->rhythm_rom && !parsed->rhythm_pcm &&
		parsed->tick_rate.ticks == 526'315U &&
		parsed->tick_rate.seconds == 9'328U,
		"valid Classic CLI keeps source-zero power-on seed and persistence beside the content root");
	const auto new_classic = parse({
		"--new-classic", "--data", "d", "--main", "m", "--mainl", "l",
		"--op", "o", "--font", "f",
	});
	const auto shell = parse({
		"--shell", "--data", "d", "--main", "m", "--mainl", "l",
		"--op", "o", "--font", "f",
	});
	expect(
		new_classic && shell &&
		new_classic->mode == th03::app::ClassicLaunchMode::new_classic &&
		shell->mode == th03::app::ClassicLaunchMode::shell,
		"New Classic and the shared shell are explicit launch identities"
	);
	const auto complete = parse({
		"--classic", "--data", "d", "--main", "m", "--mainl", "l",
		"--op", "o", "--font", "f", "--tick-rate", "3/2",
		"--language-overlay", "TH3EN.DAT",
		"--language-pack", "locale.t3lang",
		"--rhythm-rom", "ym2608.rom",
		"--config", "c", "--score", "s", "--replay-root", "r",
		"--seed", "4294967295",
		"--smoke-ticks", "7",
	});
	expect(complete && complete->launch.content.config == "c" &&
		complete->launch.content.new_classic_settings == "PODD.CFG" &&
		complete->launch.content.score == "s" &&
		complete->replay_root == "r" &&
		complete->launch.content.language_overlay == "TH3EN.DAT" &&
		complete->launch.content.language_pack == "locale.t3lang" &&
		complete->rhythm_rom == std::filesystem::path("ym2608.rom") &&
		complete->launch.seed == 0xFFFF'FFFFU &&
		complete->tick_rate.ticks == 3U && complete->tick_rate.seconds == 2U &&
		complete->smoke_ticks == 7U,
		"optional Classic paths, language, rate, seed, and smoke bound parse");
	const auto decoded_rhythm = parse({
		"--classic", "--data", "d", "--main", "m", "--mainl", "l",
		"--op", "o", "--font", "f", "--rhythm-pcm", "t98.pcm",
	});
	expect(
		decoded_rhythm &&
			decoded_rhythm->rhythm_pcm == std::filesystem::path("t98.pcm"),
		"decoded six-voice rhythm profile path parses"
	);

	const auto unknown = parse({"--classic", "--wat"});
	const auto missing_value = parse({"--classic", "--data"});
	const auto duplicate = parse({"--classic", "--classic"});
	const auto conflicting_mode = parse({"--classic", "--shell"});
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
	expect(!conflicting_mode && conflicting_mode.error().code ==
		th03::app::ClassicCliErrorCode::duplicate_option,
		"conflicting experience selectors are rejected");
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
		.fast_forward_requested = true,
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
		input.story.staff.fast_forward_held &&
		!input.story.staff.vsync_count_above_one,
		"Story child inputs preserve physical staff acceleration without inventing overrun");
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
	constexpr std::array<std::uint16_t, 3U> raw_only_cases{
		0x0010U, 0x0100U, 0x4000U,
	};
	for(const auto raw_only : raw_only_cases) {
		const auto projected =
			th03::app::ClassicRunnerTestAccess::project_input({
				.tick_index = 10U,
				.deadline_ns = 124U,
				.input = {.system = raw_only},
			});
		expect(
			projected.op.any() && projected.music_room.controls.any() &&
				projected.high_scores.any() && projected.dismiss_pressed,
			"Bomb, diagonal, and Q retain complete input_sp presence"
		);
	}

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

void test_classic_catch_up_policy()
{
	expect(
		th03::app::classic_interactive_catch_up_policy ==
			th03::app::TickInputCatchUpPolicy::preserve_all,
		"Classic production retains source deadlines across host presentation"
	);
	auto created = th03::app::TickInputScheduler::create(
		th03::app::pc98_24khz_nominal_tick_rate, 0U,
		th03::app::classic_interactive_catch_up_policy
	);
	expect(created.has_value(), "Classic nominal scheduler creates");
	if(!created) {
		return;
	}
	auto scheduler = std::move(*created);
	std::size_t ticks = 0U;
	std::uint64_t canonical_fraction = 0U;
	std::int64_t queued_frames = 0;
	bool saw_two_tick_catch_up = false;
	for(std::uint64_t cutoff = 0U; cutoff <= 1'000'000'000U;
		cutoff += 20'000'000U) {
		if(cutoff != 0U) {
			queued_frames -= 960;
			expect(
				queued_frames >= 0,
				"three-interval prefill survives each 50 Hz presentation period"
			);
		}
		std::size_t cutoff_ticks = 0U;
		for(;;) {
			const auto due = scheduler.pop_due(cutoff);
			expect(due.has_value(), "50 Hz host cutoff remains valid");
			if(!due || !*due) {
				break;
			}
			canonical_fraction += 48'000ULL *
				th03::app::pc98_24khz_nominal_tick_rate.seconds;
			const auto interval_frames = canonical_fraction /
				th03::app::pc98_24khz_nominal_tick_rate.ticks;
			canonical_fraction %=
				th03::app::pc98_24khz_nominal_tick_rate.ticks;
			if(ticks == 0U) {
				queued_frames += static_cast<std::int64_t>(interval_frames * 2U);
			}
			queued_frames += static_cast<std::int64_t>(interval_frames);
			++ticks;
			++cutoff_ticks;
		}
		saw_two_tick_catch_up = saw_two_tick_catch_up || cutoff_ticks >= 2U;
	}
	expect(
		ticks == 57U,
		"50 Hz presentation still advances 57 nominal samples through one second"
	);
	expect(
		saw_two_tick_catch_up,
		"50 Hz presentation reaches a two-tick source catch-up without underrun"
	);
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
	std::size_t netplay_resume_count = 0U;
	th03::app::ClassicOpSessionInput last_input{};
	th03::app::ClassicOpSessionStatus next_status =
		th03::app::ClassicOpSessionStatus::running;
	bool fail_consume = false;
	bool fail_compose = false;
	bool clock_rebase_next = false;
	std::optional<th03::app::ClassicAudioCommand> audio{};
	std::optional<th03::app::NewClassicKeyConfig> key_config_update{};
	std::optional<th03::app::NewClassicJoyConfig> joy_config_update{};
	std::optional<th03::app::NewClassicDeviceAssignment>
		device_assignment_update{};
	std::optional<bool> reduced_flash_update{};
	std::optional<std::uint8_t> master_volume_update{};
	std::optional<bool> mute_when_unfocused_update{};
	std::optional<th03::app::NewClassicWindowPresentation>
		window_presentation_update{};
	th03::app::PixelOracleSourceIdentity pixel_oracle_source_identity{
		.process = th03::app::PixelOracleProcess::op,
		.scene = th03::app::PixelOracleScene::title,
	};
	th03::present::Page shown_page = th03::present::Page::zero;
	bool oracle_main_sequence = false;
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
			const auto publication_identity = self.pixel_oracle_source_identity;
			if(self.oracle_main_sequence) {
				self.shown_page = self.shown_page == th03::present::Page::zero ?
					th03::present::Page::one : th03::present::Page::zero;
				const auto next = self.pixel_oracle_source_identity.logical_sample
					.value_or(0U) + 1U;
				self.pixel_oracle_source_identity.logical_sample = next;
				self.pixel_oracle_source_identity.replay_global_frame = next;
				self.pixel_oracle_source_identity.round_frame = next;
			}
			const bool clock_rebase = std::exchange(
				self.clock_rebase_next, false
			);
			return std::expected<
				th03::app::ClassicOpSessionTick,
				th03::app::ClassicOpSessionError
			>{th03::app::ClassicOpSessionTick{
				.scene = th03::app::ClassicOpSessionScene::title_menu,
				.status = status,
				.surface_changed = true,
				.input_sampled = true,
				.clock_rebase_requested = clock_rebase,
				.pixel_oracle_source_identity = publication_identity,
			}};
		},
		.resume_from_netplay = [](void *context) {
			auto &self = *static_cast<FakeRunnerBackend *>(context);
			++self.netplay_resume_count;
			self.next_status = th03::app::ClassicOpSessionStatus::running;
			return std::expected<void, th03::app::ClassicOpSessionError>{};
		},
		.take_audio_command = [](void *context) {
			auto &self = *static_cast<FakeRunnerBackend *>(context);
			auto command = self.audio;
			self.audio.reset();
			return command;
		},
		.take_key_config_update = [](void *context) {
			auto &self = *static_cast<FakeRunnerBackend *>(context);
			return std::exchange(self.key_config_update, std::nullopt);
		},
		.take_joy_config_update = [](void *context) {
			auto &self = *static_cast<FakeRunnerBackend *>(context);
			return std::exchange(self.joy_config_update, std::nullopt);
		},
		.take_device_assignment_update = [](void *context) {
			auto &self = *static_cast<FakeRunnerBackend *>(context);
			return std::exchange(
				self.device_assignment_update, std::nullopt
			);
		},
		.take_reduced_flash_update = [](void *context) {
			auto &self = *static_cast<FakeRunnerBackend *>(context);
			return std::exchange(self.reduced_flash_update, std::nullopt);
		},
		.take_master_volume_update = [](void *context) {
			auto &self = *static_cast<FakeRunnerBackend *>(context);
			return std::exchange(self.master_volume_update, std::nullopt);
		},
		.take_mute_when_unfocused_update = [](void *context) {
			auto &self = *static_cast<FakeRunnerBackend *>(context);
			return std::exchange(
				self.mute_when_unfocused_update, std::nullopt
			);
		},
		.take_window_presentation_update = [](void *context) {
			auto &self = *static_cast<FakeRunnerBackend *>(context);
			return std::exchange(
				self.window_presentation_update, std::nullopt
			);
		},
		.pixel_oracle_source_identity = [](void *context) {
			return static_cast<FakeRunnerBackend *>(context)->
				pixel_oracle_source_identity;
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
			>{th03::present::SourceSurface{
				.pixels = output,
				.graphics_palette = {},
				.shown_page = self.shown_page,
				.palette_tone = 150,
				.reduced_flash_palette = {{th03::assets::Rgb4{3U, 4U, 5U}}},
			}};
		},
	};
}

std::unique_ptr<th03::app::ClassicRunner> make_fake_runner(
	const std::shared_ptr<FakeRunnerBackend> &backend
);

void test_pixel_oracle_publication_ownership()
{
	auto backend = std::make_shared<FakeRunnerBackend>();
	backend->pixel_oracle_source_identity = {
		.process = th03::app::PixelOracleProcess::main,
		.scene = th03::app::PixelOracleScene::gameplay,
		.logical_sample = 0U,
		.replay_global_frame = 0U,
		.round_frame = 0U,
	};
	backend->shown_page = th03::present::Page::one;
	backend->oracle_main_sequence = true;
	auto runner = make_fake_runner(backend);
	if(!runner) {
		return;
	}
	std::array<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	> pixels{};
	const auto bootstrap = runner->pixel_oracle_bootstrap_identity();
	const auto bootstrap_surface = runner->compose_shown_surface(pixels);
	const auto first = runner->consume({.tick_index = 0U});
	const auto first_surface = runner->compose_shown_surface(pixels);
	const auto second = runner->consume({.tick_index = 1U});
	const auto second_surface = runner->compose_shown_surface(pixels);
	expect(
		bootstrap.logical_sample == 0U &&
			bootstrap.replay_global_frame == 0U &&
			bootstrap.round_frame == 0U && bootstrap.commit == 0U &&
			bootstrap.boundary == th03::app::PixelOracleBoundary::bootstrap &&
			bootstrap_surface && bootstrap_surface->shown_page ==
				th03::present::Page::one,
		"pixel oracle bootstrap owns MAIN's initial shown page 1"
	);
	expect(
		first && first->pixel_oracle_identity.logical_sample == 0U &&
			first->pixel_oracle_identity.replay_global_frame == 0U &&
			first->pixel_oracle_identity.round_frame == 0U &&
			first->pixel_oracle_identity.commit == 1U && first_surface &&
			first_surface->shown_page == th03::present::Page::zero,
		"first MAIN publication keeps pre-consume IDs 0 on shown page 0"
	);
	expect(
		second && second->pixel_oracle_identity.logical_sample == 1U &&
			second->pixel_oracle_identity.replay_global_frame == 1U &&
			second->pixel_oracle_identity.round_frame == 1U &&
			second->pixel_oracle_identity.commit == 0U && second_surface &&
			second_surface->shown_page == th03::present::Page::one,
		"second MAIN publication advances to IDs 1 on shown page 1"
	);
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
	const auto new_classic_quit = th03::app::product_lifecycle_transition(
		th03::app::ExperienceProfile::new_classic,
		th03::app::ProductLifecycleSignal::return_to_menu
	);
	const auto switch_to_classic = th03::app::product_lifecycle_transition(
		th03::app::ExperienceProfile::new_classic,
		th03::app::ProductLifecycleSignal::switch_to_classic
	);
	const auto classic_quit = th03::app::product_lifecycle_transition(
		th03::app::ExperienceProfile::classic,
		th03::app::ProductLifecycleSignal::return_to_menu
	);
	expect(
		new_classic_quit.exit_application &&
		!switch_to_classic.exit_application &&
		switch_to_classic.next == th03::app::ExperienceProfile::classic &&
		!classic_quit.exit_application &&
		classic_quit.next == th03::app::ExperienceProfile::new_classic,
		"product lifecycle is direct New Classic -> temporary Classic -> fresh New Classic"
	);

	auto invalid = th03::app::ClassicRunnerTestAccess::create({}, {});
	expect(
		!invalid && invalid.error().code ==
			th03::app::ClassicRunnerErrorCode::invalid_request,
		"invalid private runner operations are rejected"
	);
	auto profile_backend = std::make_shared<FakeRunnerBackend>();
	auto new_classic = th03::app::ClassicRunnerTestAccess::create(
		std::static_pointer_cast<void>(profile_backend),
		runner_operations(*profile_backend),
		th03::app::ExperienceProfile::new_classic
	);
	expect(
		new_classic && th03::app::ClassicRunnerTestAccess::experience(
			**new_classic
		) == th03::app::ExperienceProfile::new_classic,
		"the shared runner retains New Classic as host policy only"
	);
	if(new_classic) {
		profile_backend->reduced_flash_update = true;
		profile_backend->joy_config_update = th03::app::NewClassicJoyConfig{
			.dead_zone_percent = 35U,
		};
		th03::app::NewClassicDeviceAssignment assignment{};
		assignment.mode[0] =
			th03::app::NewClassicControllerAssignmentMode::disabled;
		profile_backend->device_assignment_update = assignment;
		profile_backend->master_volume_update = std::uint8_t{35U};
		profile_backend->mute_when_unfocused_update = true;
		profile_backend->window_presentation_update =
			th03::app::NewClassicWindowPresentation{
				.mode = th03::app::NewClassicWindowMode::borderless_fullscreen,
				.scale = th03::app::NewClassicWindowScale::x2,
			};
		auto live_flash = (*new_classic)->consume({});
		std::vector<th03::present::ComposedPixel> live_pixels(
			th03::present::graphics_page_pixels
		);
		const auto live_surface = (*new_classic)->compose_shown_surface(
			std::span<th03::present::ComposedPixel,
				th03::present::graphics_page_pixels>(
				live_pixels.data(), live_pixels.size()
			)
		);
		expect(live_flash && live_surface && live_surface->reduced_flash_applied,
			"New Classic applies a live Flash FX option update in the retained runner");
		expect(
			(*new_classic)->take_new_classic_joy_config_update() ==
				th03::app::NewClassicJoyConfig{.dead_zone_percent = 35U} &&
			!(*new_classic)->take_new_classic_joy_config_update(),
			"New Classic publishes each typed JoyConfig update once"
		);
		expect(
			(*new_classic)->take_new_classic_device_assignment_update() ==
				assignment &&
			!(*new_classic)->take_new_classic_device_assignment_update(),
			"New Classic publishes each typed device assignment update once"
		);
		expect(
			(*new_classic)->take_new_classic_master_volume_update() == 35U &&
			!(*new_classic)->take_new_classic_master_volume_update(),
			"New Classic publishes each typed master-volume update once"
		);
		expect(
			(*new_classic)->take_new_classic_mute_when_unfocused_update() == true &&
			!(*new_classic)->take_new_classic_mute_when_unfocused_update(),
			"New Classic publishes each typed focus-mute update once"
		);
		expect(
			(*new_classic)->take_new_classic_window_presentation_update() ==
				profile_backend->window_presentation_update.value_or(
					th03::app::NewClassicWindowPresentation{
						.mode = th03::app::NewClassicWindowMode::borderless_fullscreen,
						.scale = th03::app::NewClassicWindowScale::x2,
					}
				) &&
			!(*new_classic)->take_new_classic_window_presentation_update(),
			"New Classic publishes each typed window-presentation update once"
		);
	}
	auto reduced = th03::app::ClassicRunnerTestAccess::create(
		std::static_pointer_cast<void>(profile_backend),
		runner_operations(*profile_backend),
		th03::app::ExperienceProfile::new_classic, true
	);
	std::vector<th03::present::ComposedPixel> reduced_pixels(
		th03::present::graphics_page_pixels
	);
	const auto reduced_surface = reduced ?
		(*reduced)->compose_shown_surface(std::span<
			th03::present::ComposedPixel,
			th03::present::graphics_page_pixels
		>(reduced_pixels.data(), reduced_pixels.size())) :
		std::expected<
			th03::present::SourceSurface,
			th03::app::ClassicRunnerError
		>{std::unexpected(th03::app::ClassicRunnerError{})};
	expect(reduced_surface &&
		reduced_surface->graphics_palette.colors[0U] ==
			th03::assets::Rgb4{3U, 4U, 5U},
		"New Classic reduced flash maps only an exact tone-150 publication to tone 100");

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

	backend->clock_rebase_next = true;
	auto first = runner->consume({
		.tick_index = 0U,
		.deadline_ns = 10U,
		.input = {.system = 0x0020U},
	});
	expect(
		first && first->status == th03::app::ClassicRunnerStatus::running &&
			first->surface_changed && first->input_sampled &&
			first->clock_rebase_requested &&
			backend->consume_count == 1U &&
			backend->last_input.op.shot &&
			backend->last_input.audio.volume == 255U,
		"one logical sample forwards a synchronous reconstruction clock rebase"
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

	auto switch_backend = std::make_shared<FakeRunnerBackend>();
	switch_backend->next_status =
		th03::app::ClassicOpSessionStatus::switch_to_classic;
	auto switch_runner = make_fake_runner(switch_backend);
	if(switch_runner) {
		const auto switched = switch_runner->consume({.tick_index = 0U});
		expect(
			switched && switched->status ==
				th03::app::ClassicRunnerStatus::switch_to_classic &&
				switch_runner->status() ==
				th03::app::ClassicRunnerStatus::switch_to_classic,
			"the typed System action crosses the runner without becoming Quit"
		);
	}

	auto netplay_backend = std::make_shared<FakeRunnerBackend>();
	netplay_backend->next_status =
		th03::app::ClassicOpSessionStatus::netplay_requested;
	auto netplay_runner = make_fake_runner(netplay_backend);
	if(netplay_runner) {
		const auto requested = netplay_runner->consume({.tick_index = 0U});
		expect(
			requested && requested->status ==
				th03::app::ClassicRunnerStatus::netplay_requested &&
				netplay_runner->status() ==
				th03::app::ClassicRunnerStatus::netplay_requested,
			"Online Match crosses the runner without becoming Quit or Classic"
		);
		const auto resumed = netplay_runner->resume_from_netplay();
		const auto continued = netplay_runner->consume({.tick_index = 1U});
		expect(
			resumed && continued && continued->status ==
				th03::app::ClassicRunnerStatus::running &&
				netplay_backend->netplay_resume_count == 1U,
			"retained Online Match returns the same runner to its OP title"
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
	const th03::app::ClassicContentPaths placeholder{
		.data_root = temporary.path(),
		.main_executable = temporary.path() / "MAIN.EXE",
		.mainl_executable = temporary.path() / "MAINL.EXE",
		.op_executable = temporary.path() / "OP.EXE",
		.font_rom = temporary.path() / "FONT.ROM",
		.config = temporary.path() / "YUME.CFG",
		.score = temporary.path() / "YUME.NEM",
	};
	const auto classic_autofire = th03::app::ClassicRunner::load({
		.content = placeholder,
		.autofire_mask = 1U,
	});
	const auto invalid_autofire = th03::app::ClassicRunner::load({
		.content = placeholder,
		.experience = th03::app::ExperienceProfile::new_classic,
		.autofire_mask = 4U,
	});
	th03::app::NewClassicDeviceAssignment invalid_device{};
	invalid_device.mode[0] =
		th03::app::NewClassicControllerAssignmentMode::specific;
	const auto invalid_assignment = th03::app::ClassicRunner::load({
		.content = placeholder,
		.experience = th03::app::ExperienceProfile::new_classic,
		.new_classic_device_assignment = invalid_device,
	});
	expect(!classic_autofire && !invalid_autofire &&
		classic_autofire.error().code ==
			th03::app::ClassicRunnerErrorCode::invalid_request &&
		invalid_autofire.error().code ==
			th03::app::ClassicRunnerErrorCode::invalid_request,
		"Autofire is bounded to New Classic's two player bits");
	expect(!invalid_assignment && invalid_assignment.error().code ==
		th03::app::ClassicRunnerErrorCode::invalid_request,
		"invalid stable device identities fail before external content I/O");
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

template<typename Identity>
Identity online_identity(std::uint8_t seed)
{
	Identity result{};
	for(std::size_t index = 0U; index < result.bytes.size(); ++index) {
		result.bytes[index] = static_cast<std::uint8_t>(seed + index);
	}
	return result;
}

th03::app::NewClassicOnlineCacheDocument online_high_score_board()
{
	using namespace th03::app;
	const NewClassicOnlineCategory category{
		.mode = NewClassicOnlineMode::story,
		.rank = 1U,
		.character = 0U,
	};
	NewClassicOnlineCacheDocument document{};
	document.generation = 1U;
	document.snapshot_id = online_identity<NewClassicOnlineSnapshotId>(0x10U);
	document.fetched_unix_seconds = 1'786'765'000ULL;
	document.board_category = category;
	const auto replay_id = online_identity<NewClassicOnlineReplayId>(0x20U);
	const auto content_id = online_identity<NewClassicOnlineContentId>(0x50U);
	const auto ruleset_id = online_identity<NewClassicOnlineRulesetId>(0x70U);
	document.leaderboard_entries.push_back({
		.entry_id = online_identity<NewClassicOnlineEntryId>(0x30U),
		.content_id = content_id,
		.ruleset_id = ruleset_id,
		.replay_id = replay_id,
		.verifier_id = online_identity<NewClassicOnlineVerifierId>(0x90U),
		.category = category,
		.result = {
			.kind = NewClassicOnlineResultKind::clear,
			.score = 123'456'780U,
			.end_stage = 8U,
		},
		.provenance = NewClassicOnlineProvenance::replay_verified,
		.dos_date = 0x5D2FU,
		.published_unix_seconds = 1'786'765'432ULL,
		.player = "ALPHA",
	});
	document.replay_records.push_back({
		.replay_id = replay_id,
		.content_id = content_id,
		.ruleset_id = ruleset_id,
		.category = category,
		.result = document.leaderboard_entries.front().result,
		.availability = NewClassicOnlineReplayAvailability::metadata_only,
		.dos_date = 0x5D2FU,
		.payload_bytes = 213'592U,
		.autofire = 1U,
		.timed_frames = 10'000U,
		.slow_frames = 200U,
		.title = "ALPHA",
	});
	return document;
}

bool authentic_online_high_score_gate(
	const std::filesystem::path &data_root,
	const std::filesystem::path &main_executable,
	const std::filesystem::path &mainl_executable,
	const std::filesystem::path &op_executable,
	const std::filesystem::path &font_rom,
	std::optional<th03::formats::ReplayDocument> watch_document = std::nullopt
)
{
	using namespace th03::app;
	TemporaryDirectory temporary;
	ClassicRunnerRequest request{};
	request.content = {
		.data_root = data_root,
		.main_executable = main_executable,
		.mainl_executable = mainl_executable,
		.op_executable = op_executable,
		.font_rom = font_rom,
		.replay_root = temporary.path() / "REPLAY",
		.config = temporary.path() / "YUME.CFG",
		.new_classic_settings = temporary.path() / "PODD.CFG",
		.score = temporary.path() / "YUME.NEM",
	};
	request.seed = 1U;
	request.fm_available = false;
	request.experience = ExperienceProfile::new_classic;
	request.new_classic_online_boards.push_back(online_high_score_board());
	request.new_classic_online_my_records.push_back(online_high_score_board());
	auto loaded = ClassicRunner::load(std::move(request));
	if(!loaded) {
		std::cerr << "online-hiscore-load code=" << static_cast<unsigned>(
			loaded.error().code) << " detail=" << static_cast<unsigned>(
			loaded.error().detail_code) << '\n';
		return false;
	}
	auto &runner = **loaded;
	auto refreshed_board = online_high_score_board();
	refreshed_board.generation = 2U;
	refreshed_board.leaderboard_entries.front().player = "BETA";
	if(!runner.merge_new_classic_online_board(std::move(refreshed_board))) {
		std::cerr << "online HiScore board merge failed\n";
		return false;
	}
	auto private_board = online_high_score_board();
	private_board.leaderboard_entries.front().player = "OWNER";
	if(!runner.merge_new_classic_online_my_records(std::move(private_board))) {
		std::cerr << "online My Records merge failed\n";
		return false;
	}
	auto invalid_board = online_high_score_board();
	invalid_board.board_category.reset();
	if(runner.merge_new_classic_online_board(std::move(invalid_board))) {
		std::cerr << "invalid online HiScore board merge was accepted\n";
		return false;
	}
	std::array<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	> pixels{};
	std::uint64_t tick_index = 0U;
	std::optional<ClassicRunnerConsumeResult> last{};
	const auto drain_audio = [&]() {
		while(runner.take_audio_command()) {}
	};
	const auto step = [&](std::uint16_t system = 0U) {
		th03::CoreInput input{};
		input.system = system;
		auto advanced = runner.consume({
			.tick_index = tick_index,
			.deadline_ns = tick_index * 10U,
			.input = input,
		});
		++tick_index;
		if(!advanced || advanced->status != ClassicRunnerStatus::running) {
			if(!advanced) {
				std::cerr << "online-hiscore-step-error code="
					<< static_cast<unsigned>(advanced.error().code)
					<< " detail="
					<< static_cast<unsigned>(advanced.error().detail_code)
					<< " tick=" << tick_index << '\n';
			}
			return false;
		}
		last = *advanced;
		drain_audio();
		return !advanced->surface_changed ||
			runner.compose_shown_surface(pixels).has_value();
	};
	const auto wait_for_input = [&](std::size_t limit) {
		for(std::size_t frame = 0U; frame < limit; ++frame) {
			if(!step()) {
				return false;
			}
			if(last->input_sampled) {
				return true;
			}
		}
		return false;
	};
	drain_audio();
	if(!wait_for_input(512U) || !step(0x0020U) ||
		!wait_for_input(64U)) {
		return false;
	}
	for(std::size_t row = 0U; row < 3U; ++row) {
		if(!step(0x0002U) || !step()) {
			return false;
		}
	}
	if(!step(0x0020U)) {
		return false;
	}
	for(std::size_t guard = 0U; guard < 96U &&
		!ClassicRunnerTestAccess::online_high_score_menu(runner); ++guard) {
		if(!step()) {
			return false;
		}
	}
	if(!ClassicRunnerTestAccess::online_high_score_menu(runner)) {
		std::cerr << "online HiScore cache did not select the hub owner\n";
		return false;
	}
	for(std::size_t boundary = 0U; boundary < 24U; ++boundary) {
		if(!step()) {
			return false;
		}
	}

	// The default Local choice must retain a return edge to this hub rather
	// than rebuilding the title directly.
	if(!step(0x0020U) ||
		ClassicRunnerTestAccess::online_high_score_menu(runner)) {
		return false;
	}
	if(!wait_for_input(96U) || !step(0x0020U)) {
		return false;
	}
	for(std::size_t guard = 0U; guard < 128U &&
		!ClassicRunnerTestAccess::online_high_score_menu(runner); ++guard) {
		if(!step()) {
			return false;
		}
	}
	if(!ClassicRunnerTestAccess::online_high_score_menu(runner)) {
		std::cerr << "local HiScore viewer did not return to the cached hub"
			<< " tick=" << tick_index << " scene=" << static_cast<unsigned>(
				ClassicRunnerTestAccess::scene(runner).value_or(
					ClassicOpSessionScene::failed)) << '\n';
		return false;
	}
	for(std::size_t boundary = 0U; boundary < 24U; ++boundary) {
		if(!step()) {
			return false;
		}
	}
	// Personal Bests is a retained local-score page inside the same immutable
	// HiScore owner. It returns to the hub without rebuilding OP or touching the
	// score store during source-time input.
	if(!step(0x0002U) || !step() || !step(0x0002U) || !step() ||
		!step(0x0020U) || !step() ||
		!ClassicRunnerTestAccess::online_high_score_menu(runner) ||
		!step(0x1000U) || !step() ||
		!ClassicRunnerTestAccess::online_high_score_menu(runner) ||
		!step(0x0001U) || !step()) {
		std::cerr << "Personal Bests did not retain the HiScore hub owner\n";
		return false;
	}

	// Online opens as a second real destination. A replay-backed detail emits
	// one host-owned request. A validated download becomes a Watch request; a
	// stale local file is rejected and returns to the retryable detail without
	// leaving source time. Cancel then returns detail -> board -> hub -> title.
	if(!step(0x0020U) || !step() ||
		!ClassicRunnerTestAccess::online_high_score_menu(runner) ||
		!step(0x0020U) || !step() || !step(0x0020U)) {
		return false;
	}
	auto download = runner.take_new_classic_online_replay_download_request();
	if(!download || download->replay_id !=
		online_high_score_board().replay_records.front().replay_id ||
		!runner.report_new_classic_online_replay_download(
			download->replay_id,
			NewClassicOnlineReplayDownloadOutcome::downloaded
		)) {
		std::cerr << "online replay download intent/report failed\n";
		return false;
	}
	if(!step() || !step() || !step(0x0020U)) {
		return false;
	}
	auto watch = runner.take_new_classic_online_replay_watch_request();
	const bool watch_expected_playback = watch_document.has_value();
	if(!watch || watch->replay_id != download->replay_id ||
		!runner.report_new_classic_online_replay_watch(
			watch->replay_id, std::move(watch_document)
		)) {
		std::cerr << "online replay Watch intent/report failed\n";
		return false;
	}
	if(watch_expected_playback) {
		if(!step() || ClassicRunnerTestAccess::scene(runner) !=
			ClassicOpSessionScene::high_scores) {
			std::cerr << "online Watch did not publish its black boundary\n";
			return false;
		}
		if(!step() || ClassicRunnerTestAccess::scene(runner) !=
			ClassicOpSessionScene::high_scores) {
			std::cerr << "online Watch did not retain the stopped black boundary\n";
			return false;
		}
		if(!step() || ClassicRunnerTestAccess::scene(runner) !=
			ClassicOpSessionScene::replay_playback || !last ||
			!last->clock_rebase_requested) {
			std::cerr << "online Watch did not adopt playback scene="
				<< static_cast<unsigned>(ClassicRunnerTestAccess::scene(
					runner
				).value_or(ClassicOpSessionScene::failed))
				<< " rebase=" << (last && last->clock_rebase_requested)
				<< '\n';
			return false;
		}
		return true;
	}
	if(!step() || !step() || !step(0x1000U) || !step() ||
		!ClassicRunnerTestAccess::online_high_score_menu(runner) ||
		!step(0x1000U) || !step() ||
		!ClassicRunnerTestAccess::online_high_score_menu(runner) ||
		!step(0x1000U)) {
		return false;
	}
	for(std::size_t guard = 0U; guard < 128U; ++guard) {
		if(ClassicRunnerTestAccess::scene(runner) ==
			ClassicOpSessionScene::title_menu) {
			return true;
		}
		if(!step()) {
			return false;
		}
	}
	return false;
}

bool authentic_replay_hub_gate(
	const std::filesystem::path &data_root,
	const std::filesystem::path &main_executable,
	const std::filesystem::path &mainl_executable,
	const std::filesystem::path &op_executable,
	const std::filesystem::path &font_rom,
	std::optional<th03::formats::ReplayDocument> watch_document = std::nullopt
)
{
	using namespace th03::app;
	TemporaryDirectory temporary;
	ClassicRunnerRequest request{};
	request.content = {
		.data_root = data_root,
		.main_executable = main_executable,
		.mainl_executable = mainl_executable,
		.op_executable = op_executable,
		.font_rom = font_rom,
		.replay_root = temporary.path() / "REPLAY",
		.config = temporary.path() / "YUME.CFG",
		.new_classic_settings = temporary.path() / "PODD.CFG",
		.score = temporary.path() / "YUME.NEM",
	};
	request.seed = 1U;
	request.fm_available = false;
	request.experience = ExperienceProfile::new_classic;
	request.new_classic_online_boards.push_back(online_high_score_board());
	const bool upload_expected = watch_document.has_value();
	if(upload_expected) {
		if(!commit_replay_slot(
			request.content.replay_root, 0U, *watch_document
		)) {
			return false;
		}
		request.new_classic_online_upload_available = true;
	}
	auto loaded = ClassicRunner::load(std::move(request));
	if(!loaded) {
		return false;
	}
	auto &runner = **loaded;
	std::array<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	> pixels{};
	std::uint64_t tick_index = 0U;
	std::optional<ClassicRunnerConsumeResult> last{};
	const auto step = [&](std::uint16_t system = 0U) {
		th03::CoreInput input{};
		input.system = system;
		auto advanced = runner.consume({
			.tick_index = tick_index,
			.deadline_ns = tick_index * 10U,
			.input = input,
		});
		++tick_index;
		if(!advanced || advanced->status != ClassicRunnerStatus::running) {
			return false;
		}
		last = *advanced;
		while(runner.take_audio_command()) {}
		return !advanced->surface_changed ||
			runner.compose_shown_surface(pixels).has_value();
	};
	const auto wait_for_input = [&](std::size_t limit) {
		for(std::size_t frame = 0U; frame < limit; ++frame) {
			if(!step()) {
				return false;
			}
			if(last && last->input_sampled) {
				return true;
			}
		}
		return false;
	};
	const auto settle_patch_menu = [&]() {
		for(std::size_t boundary = 0U; boundary < 24U; ++boundary) {
			if(!step()) {
				return false;
			}
		}
		return true;
	};
	while(runner.take_audio_command()) {}
	if(!wait_for_input(512U) || !step(0x0020U) || !wait_for_input(64U)) {
		return false;
	}
	// Start, VS Start, Music room, HiScore, Replay.
	for(std::size_t row = 0U; row < 4U; ++row) {
		if(!step(0x0002U) || !step()) {
			return false;
		}
	}
	if(!step(0x0020U) || ClassicRunnerTestAccess::scene(runner) !=
		ClassicOpSessionScene::replay_browser || !settle_patch_menu()) {
		return false;
	}

	// Local Replays remains the default and returns to the retained Replay hub.
	if(!step(0x0020U) || !settle_patch_menu() || !step(0x1000U) ||
		ClassicRunnerTestAccess::scene(runner) !=
			ClassicOpSessionScene::replay_browser || !settle_patch_menu()) {
		return false;
	}
	if(upload_expected) {
		// Upload Replay is capability-gated between Local and Online. The
		// retained owner emits one immutable document and redraws only after the
		// host reports the matching slot.
		if(!step(0x0002U) || !step() || !step(0x0020U) ||
			!settle_patch_menu() || !step(0x0020U)) {
			return false;
		}
		auto upload =
			runner.take_new_classic_online_replay_upload_request();
		if(!upload || upload->slot != 0U ||
			!runner.report_new_classic_online_replay_upload(
				upload->slot, NewClassicReplayUploadOutcome::accepted
			) || !step() || !last || !last->surface_changed || !step() ||
			!step(0x0020U) || !step() || !step(0x1000U) ||
			!settle_patch_menu()) {
			return false;
		}
	}

	// Online Replay Library follows Local (and the capability-gated Upload row).
	// Its detail owns the same bounded
	// Download and Watch requests as the score-oriented cross-link.
	const auto online_downs = upload_expected ? 2U : 1U;
	for(std::size_t row = 0U; row < online_downs; ++row) {
		if(!step(0x0002U) || !step()) {
			return false;
		}
	}
	if(!step(0x0020U) ||
		!settle_patch_menu() || !step(0x0020U) || !step() ||
		!step(0x0020U)) {
		return false;
	}
	auto download = runner.take_new_classic_online_replay_download_request();
	if(!download || !runner.report_new_classic_online_replay_download(
		download->replay_id,
		NewClassicOnlineReplayDownloadOutcome::downloaded
	) || !step() || !step() || !step(0x0020U)) {
		return false;
	}
	auto watch = runner.take_new_classic_online_replay_watch_request();
	const bool playback_expected = watch_document.has_value();
	if(!watch || !runner.report_new_classic_online_replay_watch(
		watch->replay_id, std::move(watch_document)
	)) {
		return false;
	}
	if(playback_expected) {
		if(!step() || ClassicRunnerTestAccess::scene(runner) !=
			ClassicOpSessionScene::replay_browser || !step() ||
			ClassicRunnerTestAccess::scene(runner) !=
				ClassicOpSessionScene::replay_browser || !step()) {
			return false;
		}
		return ClassicRunnerTestAccess::scene(runner) ==
			ClassicOpSessionScene::replay_playback && last &&
			last->clock_rebase_requested;
	}

	// A failed local revalidation remains retryable, and every Cancel returns
	// through detail -> online library -> Replay hub -> title.
	if(!step() || !step() || !step(0x1000U) || !step() ||
		!step(0x1000U) || !settle_patch_menu() || !step(0x1000U)) {
		return false;
	}
	for(std::size_t guard = 0U; guard < 128U; ++guard) {
		if(ClassicRunnerTestAccess::scene(runner) ==
			ClassicOpSessionScene::title_menu) {
			return true;
		}
		if(!step()) {
			return false;
		}
	}
	return false;
}

bool authentic_local_high_score_gate(
	const std::filesystem::path &data_root,
	const std::filesystem::path &main_executable,
	const std::filesystem::path &mainl_executable,
	const std::filesystem::path &op_executable,
	const std::filesystem::path &font_rom
)
{
	using namespace th03::app;
	TemporaryDirectory temporary;
	auto score = new_classic_score_empty();
	score.generation = 7U;
	score.last_run_id = 6U;
	score.boards[3U][1U].stats.play_frames = 242U;
	score.boards[3U][2U].stats.play_frames = 3'649U;
	score.boards[3U][4U].stats.play_frames = 6'569U;
	const auto score_path = temporary.path() / "YUME.NEW.NEM";
	if(!commit_new_classic_score_store(score_path, score)) {
		std::cerr << "stats-only HiScore fixture could not be committed\n";
		return false;
	}
	auto settings = new_classic_settings_defaults();
	settings.options.rank = ClassicRank::lunatic;
	const auto language_overlay = data_root / "TH3EN.DAT";
	settings.options.language = std::filesystem::is_regular_file(language_overlay) ?
		ClassicLanguage::english : ClassicLanguage::japanese;
	settings.master_volume_percent = 100U;
	settings.music_volume_percent = 75U;
	settings.effect_volume_percent = 85U;
	if(!commit_new_classic_settings_store(
		temporary.path() / "PODD.CFG", settings
	)) {
		std::cerr << "stats-only HiScore settings could not be committed\n";
		return false;
	}

	ClassicRunnerRequest request{};
	request.content = {
		.data_root = data_root,
		.main_executable = main_executable,
		.mainl_executable = mainl_executable,
		.op_executable = op_executable,
		.font_rom = font_rom,
		.language_overlay = settings.options.language == ClassicLanguage::english ?
			language_overlay : std::filesystem::path{},
		.replay_root = temporary.path() / "REPLAY",
		.config = temporary.path() / "YUME.CFG",
		.new_classic_settings = temporary.path() / "PODD.CFG",
		.score = temporary.path() / "YUME.NEM",
	};
	request.seed = 1U;
	request.fm_available = true;
	request.experience = ExperienceProfile::new_classic;
	auto loaded = ClassicRunner::load(std::move(request));
	if(!loaded) {
		std::cerr << "stats-only-hiscore-load code=" << static_cast<unsigned>(
			loaded.error().code) << " detail=" << static_cast<unsigned>(
			loaded.error().detail_code) << '\n';
		return false;
	}
	auto &runner = **loaded;
	auto audio = ClassicAudioSession::create({
		.data_root = data_root,
		.tick_rate = {.ticks = 56'423'132U, .seconds = 1'000'000U},
		.music_volume_percent = 100U,
		.effect_volume_percent = 85U,
	});
	if(!audio) {
		std::cerr << "stats-only HiScore audio could not be created code="
			<< static_cast<unsigned>(audio.error().code) << '\n';
		return false;
	}
	std::array<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	> pixels{};
	std::uint64_t tick_index = 0U;
	std::optional<ClassicRunnerConsumeResult> last{};
	const auto drain_audio = [&]() {
		while(const auto command = runner.take_audio_command()) {
			if(!(*audio)->apply(*command)) {
				return false;
			}
		}
		return true;
	};
	const auto step = [&](std::uint16_t system = 0U) {
		th03::CoreInput input{};
		input.system = system;
		auto advanced = runner.consume({
			.tick_index = tick_index,
			.deadline_ns = tick_index * 10U,
			.input = input,
		});
		++tick_index;
		if(!advanced || advanced->status != ClassicRunnerStatus::running) {
			std::cerr << "stats-only HiScore route stopped at tick "
				<< tick_index << '\n';
			return false;
		}
		last = *advanced;
		if(!drain_audio() || !(*audio)->advance_tick()) {
			return false;
		}
		return !advanced->surface_changed ||
			runner.compose_shown_surface(pixels).has_value();
	};
	const auto wait_for_input = [&](std::size_t limit) {
		for(std::size_t frame = 0U; frame < limit; ++frame) {
			if(!step()) {
				return false;
			}
			if(last->input_sampled) {
				return true;
			}
		}
		return false;
	};
	if(!drain_audio()) {
		return false;
	}
	if(!wait_for_input(512U) || !step(0x0020U) || !wait_for_input(64U)) {
		return false;
	}
	for(std::size_t row = 0U; row < 3U; ++row) {
		if(!step(0x0002U) || !step()) {
			return false;
		}
	}
	if(!step(0x0020U)) {
		return false;
	}
	for(std::size_t guard = 0U; guard < 96U; ++guard) {
		if(ClassicRunnerTestAccess::scene(runner) ==
			ClassicOpSessionScene::high_scores) {
			break;
		}
		if(!step()) {
			return false;
		}
	}
	if(ClassicRunnerTestAccess::scene(runner) !=
		ClassicOpSessionScene::high_scores ||
		ClassicRunnerTestAccess::online_high_score_menu(runner) ||
		!wait_for_input(96U) || !step(0x0020U)) {
		std::cerr << "stats-only profile did not traverse direct local HiScore\n";
		return false;
	}
	for(std::size_t guard = 0U; guard < 128U; ++guard) {
		if(ClassicRunnerTestAccess::scene(runner) ==
			ClassicOpSessionScene::title_menu) {
			return runner.compose_shown_surface(pixels).has_value();
		}
		if(!step()) {
			return false;
		}
	}
	return false;
}

bool authentic_vs_gate(
	const std::filesystem::path &data_root,
	const std::filesystem::path &main_executable,
	const std::filesystem::path &mainl_executable,
	const std::filesystem::path &op_executable,
	const std::filesystem::path &font_rom
)
{
	for(std::uint8_t mode_index = 0U; mode_index < 3U; ++mode_index) {
		TemporaryDirectory temporary;
		auto loaded = th03::app::ClassicRunner::load({
			.content = {
				.data_root = data_root,
				.main_executable = main_executable,
				.mainl_executable = mainl_executable,
				.op_executable = op_executable,
				.font_rom = font_rom,
				.replay_root = temporary.path() / "REPLAY",
				.config = temporary.path() / "YUME.CFG",
				.score = temporary.path() / "YUME.NEM",
			},
			.seed = static_cast<std::uint32_t>(10U + mode_index),
			.fm_available = false,
			.experience = th03::app::ExperienceProfile::new_classic,
		});
		if(!loaded) {
			std::cerr << "authentic-vs-load mode="
				<< static_cast<unsigned>(mode_index) << " code="
				<< static_cast<unsigned>(loaded.error().code) << " detail="
				<< static_cast<unsigned>(loaded.error().detail_code) << '\n';
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
		const auto drain_audio = [&]() {
			while(auto command = runner.take_audio_command()) {
				audio_commands.push_back(*command);
			}
		};
		drain_audio();

		std::uint64_t tick_index = 0U;
		std::optional<th03::app::ClassicRunnerConsumeResult> last{};
		const auto scene = [&]() {
			return th03::app::ClassicRunnerTestAccess::scene(runner);
		};
		const auto fail_stage = [&](const char *stage) {
			std::cerr << "authentic-vs-stage=" << stage << " mode="
				<< static_cast<unsigned>(mode_index) << " tick=" << tick_index
				<< " scene=" << static_cast<unsigned>(scene().value_or(
					th03::app::ClassicOpSessionScene::failed
				)) << '\n';
			return false;
		};
		const auto step = [&](th03::CoreInput input = {},
			bool allow_complete = false) {
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
				std::cerr << "authentic-vs-step mode="
					<< static_cast<unsigned>(mode_index) << " tick="
					<< tick_index - 1U;
				if(!advanced) {
					std::cerr << " code=" << static_cast<unsigned>(
						advanced.error().code
					) << " detail=" << static_cast<unsigned>(
						advanced.error().detail_code
					);
				}
				std::cerr << '\n';
				return false;
			}
			last = *advanced;
			drain_audio();
			return !advanced->surface_changed ||
				runner.compose_shown_surface(pixels).has_value();
		};
		const auto wait_for_input = [&](std::size_t limit) {
			for(std::size_t frame = 0U; frame < limit; ++frame) {
				if(!step()) { return false; }
				if(last->input_sampled) { return true; }
			}
			return false;
		};
		const auto pulse = [](std::uint16_t system) {
			th03::CoreInput input{};
			input.system = system;
			return input;
		};
		const auto force_terminal = [&](auto outcome, auto expected_scene) {
			const auto audio_before = audio_commands.size();
			if(!th03::app::ClassicRunnerTestAccess::force_vs_main_outcome(
				runner, outcome
			) || !step() || scene() !=
				th03::app::ClassicOpSessionScene::vs_main ||
				audio_commands.size() != audio_before + 1U ||
				audio_commands.back().type !=
					th03::app::ClassicAudioCommandType::stop_song) {
				return false;
			}
			return step() && scene() == expected_scene;
		};

		// Fresh title -> main menu -> VS submenu. The submenu always starts on
		// 1P-vs-CPU, so [mode_index] Down edges select all three source modes.
		if(!wait_for_input(300U) || !step(pulse(0x0020U)) ||
			!wait_for_input(64U) || !step(pulse(0x0002U)) || !step() ||
			!step(pulse(0x0020U))) {
			return fail_stage("open-vs-submenu");
		}
		bool submenu_input = false;
		bool saw_blocking_frame = false;
		for(std::size_t frame = 0U; frame < 32U && !submenu_input; ++frame) {
			if(!step()) { return false; }
			saw_blocking_frame = saw_blocking_frame || !last->input_sampled;
			submenu_input = saw_blocking_frame && last->input_sampled;
		}
		if(!submenu_input) { return fail_stage("submenu-input"); }
		for(std::uint8_t move = 0U; move < mode_index; ++move) {
			if(!step(pulse(0x0002U)) || !step()) {
				return fail_stage("submenu-move");
			}
		}
		if(!step(pulse(0x0020U)) || scene() !=
			th03::app::ClassicOpSessionScene::character_selection) {
			return fail_stage("submenu-confirm");
		}

		const auto drive_selection = [&]() {
			for(std::size_t frame = 0U; frame < 1'024U; ++frame) {
				if(scene() != th03::app::ClassicOpSessionScene::
					character_selection) {
					return scene() == th03::app::ClassicOpSessionScene::
						vs_stage_splash;
				}
				th03::CoreInput input{};
				if((frame & 1U) == 0U) {
					if(mode_index == 1U) {
						input.p1 = 0x0020U;
						input.p2 = 0x0020U;
					} else {
						input.system = 0x0020U;
					}
				}
				if(!step(input)) { return fail_stage("drive-selection-step"); }
			}
			return fail_stage("drive-selection-timeout");
		};
		const auto drive_stage_to_real_main = [&]() {
			for(std::size_t frame = 0U; frame < 512U; ++frame) {
				th03::CoreInput input{};
				if((frame & 1U) == 0U) { input.system = 0x0020U; }
				if(!step(input)) { return fail_stage("drive-stage-step"); }
				if(scene() == th03::app::ClassicOpSessionScene::vs_main &&
					last->process_advanced) {
					return true;
				}
			}
			return fail_stage("drive-stage-timeout");
		};
		const auto select_ordinary_vs_if_offered = [&]() {
			if(mode_index != 0U) {
				return true;
			}
			const auto practice_poll = [&](th03::CoreInput input = {}) {
				for(std::size_t wait = 0U; wait < 8U; ++wait) {
					if(!step(input)) { return false; }
					if(last && last->surface_changed) { return true; }
				}
				return false;
			};
			// Replay Patch inserts Practice setup after the ordinary 1P-vs-CPU
			// selector. Switch its default Practice preset to VS, move to Start,
			// and confirm so this matrix still exercises the ordinary VS route.
			if(!practice_poll() || !practice_poll(pulse(0x0008U)) ||
				!practice_poll()) {
				return fail_stage("practice-select-vs");
			}
			for(std::size_t row = 0U; row < 5U; ++row) {
				if(!practice_poll(pulse(0x0002U)) || !practice_poll()) {
					return fail_stage("practice-select-start");
				}
			}
			return practice_poll(pulse(0x0020U));
		};

		if(!drive_selection() || !select_ordinary_vs_if_offered() ||
			!drive_stage_to_real_main()) {
			return fail_stage("first-main");
		}
		const auto winner = (mode_index & 1U) == 0U ?
			th03::app::ClassicOpVsRouteTestOutcome::player_one_win :
			th03::app::ClassicOpVsRouteTestOutcome::player_two_win;
		if(!force_terminal(
			winner, th03::app::ClassicOpSessionScene::vs_win
		)) {
			return fail_stage("route-two-transition");
		}
		const auto route_two_terminal = th03::app::ClassicRunnerTestAccess::
			vs_terminal_resident(runner);
		std::array<std::uint8_t, 8U> expected_score{};
		expected_score.fill((mode_index & 1U) == 0U ? 8U : 7U);
		if(!route_two_terminal || route_two_terminal->story_stage != 1U ||
			route_two_terminal->winner != ((mode_index & 1U) == 0U ? 0 : 1) ||
			route_two_terminal->last_score[
				(mode_index & 1U) == 0U ? 0U : 1U
			] != expected_score || route_two_terminal->last_score[
				(mode_index & 1U) == 0U ? 1U : 0U
			] != std::array<std::uint8_t, 8U>{}) {
			return fail_stage("route-two-terminal");
		}
		for(std::size_t frame = 0U; frame < 2'048U && scene() ==
			th03::app::ClassicOpSessionScene::vs_win; ++frame) {
			if(!step(pulse((frame & 1U) == 0U ? 0x0020U : 0U))) {
				return fail_stage("win-step");
			}
		}
		if(scene() !=
			th03::app::ClassicOpSessionScene::character_selection) {
			return fail_stage("route-two-selection");
		}
		const auto route_two_resident = th03::app::ClassicRunnerTestAccess::
			vs_selection_resident(runner);
		const std::array<std::uint8_t, 2U> expected_cpu = mode_index == 0U ?
			std::array<std::uint8_t, 2U>{0U, 1U} : mode_index == 1U ?
			std::array<std::uint8_t, 2U>{0U, 0U} :
			std::array<std::uint8_t, 2U>{1U, 1U};
		if(!route_two_resident || route_two_resident->game_mode !=
			static_cast<std::uint8_t>(th03::game_mode::vs_1p_cpu + mode_index) ||
			route_two_resident->is_cpu != expected_cpu ||
			route_two_resident->story_stage != 0U ||
			route_two_resident->winner != 0 ||
			route_two_resident->last_score[
				(mode_index & 1U) == 0U ? 0U : 1U
			] != expected_score || route_two_resident->last_score[
				(mode_index & 1U) == 0U ? 1U : 0U
			] != std::array<std::uint8_t, 8U>{}) {
			if(route_two_resident) {
				std::cerr << " route-two mode=" <<
					static_cast<unsigned>(route_two_resident->game_mode) <<
					" stage=" << static_cast<unsigned>(
						route_two_resident->story_stage
					) << " winner=" << route_two_resident->winner <<
					" cpu=" << static_cast<unsigned>(
						route_two_resident->is_cpu[0]
					) << ',' << static_cast<unsigned>(
						route_two_resident->is_cpu[1]
					) << '\n';
			}
			return fail_stage("route-two-resident");
		}
		auto route_two_replay = runner.take_completed_replay();
		if((mode_index == 2U && route_two_replay) ||
			(mode_index != 2U && !valid_completed_replay(
				std::move(route_two_replay),
				th03::formats::ReplayEndReason::game_over
			))) {
			return fail_stage("route-two-replay");
		}
		if(!drive_selection() || !select_ordinary_vs_if_offered() ||
			!drive_stage_to_real_main()) {
			return fail_stage("second-main");
		}
		if(mode_index == 2U) {
			// CPU-vs-CPU is deliberately not recordable. Its source Cancel must
			// therefore remain the live MAIN route-0 exit, never Replay Pause.
			if(!step(pulse(0x1000U))) {
				return fail_stage("cpu-cpu-cancel");
			}
			for(std::size_t frame = 0U; frame < 32U && scene() ==
				th03::app::ClassicOpSessionScene::vs_main; ++frame) {
				if(!step()) { return fail_stage("cpu-cpu-cancel-step"); }
			}
			if(scene() != th03::app::ClassicOpSessionScene::character_selection) {
				return fail_stage("cpu-cpu-cancel-route");
			}
		} else if(!force_terminal(
			th03::app::ClassicOpVsRouteTestOutcome::exit_to_operator,
			th03::app::ClassicOpSessionScene::character_selection
		)) {
			return fail_stage("route-zero-transition");
		}
		const auto route_zero_resident = th03::app::ClassicRunnerTestAccess::
			vs_selection_resident(runner);
		if(!route_zero_resident || route_zero_resident->game_mode !=
			route_two_resident->game_mode || route_zero_resident->is_cpu !=
			route_two_resident->is_cpu ||
			route_zero_resident->paletted_character !=
				route_two_resident->paletted_character ||
			route_zero_resident->story_stage != 0U ||
			route_zero_resident->winner != 0 ||
			route_zero_resident->last_score !=
				decltype(route_zero_resident->last_score){}) {
			return fail_stage("route-zero-resident");
		}
		auto route_zero_replay = runner.take_completed_replay();
		if((mode_index == 2U && route_zero_replay) ||
			(mode_index != 2U && !valid_completed_replay(
				std::move(route_zero_replay),
				th03::formats::ReplayEndReason::menu_return
			))) {
			return fail_stage("route-zero-replay");
		}

		// Route 0 also reenters the direct selector. Cancel there constructs a
		// fresh title process; canceling its opened main menu exits normally.
		for(std::size_t frame = 0U; frame < 1'024U && scene() ==
			th03::app::ClassicOpSessionScene::character_selection; ++frame) {
			if(!step(pulse((frame & 1U) == 0U ? 0x1000U : 0U))) {
				return fail_stage("cancel-direct-selector");
			}
		}
		if(scene() != th03::app::ClassicOpSessionScene::title_menu ||
			!wait_for_input(300U) || !step(pulse(0x0020U)) ||
			!wait_for_input(64U) || !step(pulse(0x1000U), true)) {
			return fail_stage("exit-title");
		}
		for(std::size_t frame = 0U; frame < 8U && runner.status() ==
			th03::app::ClassicRunnerStatus::running; ++frame) {
			if(!step({}, true)) { return fail_stage("complete-step"); }
		}
		if(runner.status() !=
			th03::app::ClassicRunnerStatus::session_complete) {
			return fail_stage("complete-status");
		}
		const auto has_audio = [&](th03::app::ClassicAudioCommandType type,
			th03::app::ClassicMusicCue cue = th03::app::ClassicMusicCue::none) {
			return std::ranges::any_of(audio_commands, [&](const auto &command) {
				return command.type == type && (cue ==
					th03::app::ClassicMusicCue::none || command.cue == cue);
			});
		};
		if(!has_audio(th03::app::ClassicAudioCommandType::load_song,
				th03::app::ClassicMusicCue::character_select) ||
			!has_audio(th03::app::ClassicAudioCommandType::load_song,
				th03::app::ClassicMusicCue::gameplay) ||
			!has_audio(th03::app::ClassicAudioCommandType::load_song,
				th03::app::ClassicMusicCue::winner) ||
			!has_audio(th03::app::ClassicAudioCommandType::play_song,
				th03::app::ClassicMusicCue::winner) ||
			!has_audio(th03::app::ClassicAudioCommandType::load_effects) ||
			!has_audio(th03::app::ClassicAudioCommandType::reset_effects) ||
			!has_audio(th03::app::ClassicAudioCommandType::stop_song)) {
			return fail_stage("audio");
		}
	}
	return true;
}

bool authentic_practice_gate(
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
			.replay_root = temporary.path() / "REPLAY",
			.config = temporary.path() / "YUME.CFG",
			.score = temporary.path() / "YUME.NEM",
		},
		.seed = 71U,
		.fm_available = false,
		.experience = th03::app::ExperienceProfile::new_classic,
		.initial_route =
			th03::app::ClassicRunnerInitialRoute::new_classic_practice,
	});
	if(!loaded) {
		std::cerr << "authentic-practice-load code=" << static_cast<unsigned>(
			loaded.error().code
		) << " detail=" << static_cast<unsigned>(loaded.error().detail_code)
			<< '\n';
		return false;
	}
	auto &runner = **loaded;
	if(!runner.new_classic_replay_save_art()) {
		std::cerr << "authentic-practice-stage=replay-save-art\n";
		return false;
	}
	std::array<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	> pixels{};
	if(auto composed = runner.compose_shown_surface(pixels); !composed) {
		std::cerr << "authentic-practice-compose code=" <<
			static_cast<unsigned>(composed.error().code) << " detail=" <<
			static_cast<unsigned>(composed.error().detail_code) << '\n';
		return false;
	}
	const auto drain_audio = [&]() {
		while(runner.take_audio_command()) {}
	};
	drain_audio();
	std::uint64_t tick_index = 0U;
	std::optional<th03::app::ClassicRunnerConsumeResult> last{};
	const auto scene = [&]() {
		return th03::app::ClassicRunnerTestAccess::scene(runner);
	};
	const auto fail_stage = [&](const char *stage) {
		std::cerr << "authentic-practice-stage=" << stage << " tick="
			<< tick_index << " scene=" << static_cast<unsigned>(scene().value_or(
				th03::app::ClassicOpSessionScene::failed
			)) << '\n';
		return false;
	};
	const auto step = [&](th03::CoreInput input = {}, bool allow_complete = false) {
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
			std::cerr << "authentic-practice-step tick=" << tick_index - 1U
				<< " scene=" << static_cast<unsigned>(scene().value_or(
					th03::app::ClassicOpSessionScene::failed
				)) << '\n';
			return false;
		}
		last = *advanced;
		drain_audio();
		if(!advanced->surface_changed) {
			return true;
		}
		auto composed = runner.compose_shown_surface(pixels);
		if(!composed) {
			std::cerr << "authentic-practice-step-compose tick="
				<< tick_index - 1U << " code=" << static_cast<unsigned>(
					composed.error().code
				) << " detail=" << static_cast<unsigned>(
					composed.error().detail_code
				) << " scene=" << static_cast<unsigned>(scene().value_or(
					th03::app::ClassicOpSessionScene::failed
				)) << '\n';
		}
		return composed.has_value();
	};
	const auto pulse = [](std::uint16_t system) {
		th03::CoreInput input{};
		input.system = system;
		return input;
	};
	const auto practice_poll = [&](th03::CoreInput input = {}) {
		for(std::size_t wait = 0U; wait < 8U; ++wait) {
			if(!step(input)) {
				return false;
			}
			if(last && last->surface_changed) {
				return true;
			}
		}
		return false;
	};

	if(scene() != th03::app::ClassicOpSessionScene::character_selection) {
		return fail_stage("initial-selection");
	}
	for(std::size_t frame = 0U; frame < 1'024U && scene() ==
		th03::app::ClassicOpSessionScene::character_selection; ++frame) {
		if(!step(pulse((frame & 1U) == 0U ? 0x0020U : 0U))) {
			return false;
		}
	}
	if(scene() != th03::app::ClassicOpSessionScene::vs_stage_splash) {
		return fail_stage("selection-complete");
	}
	// Setup starts on Mode. Release the selector's confirm, move through the
	// six-row main page to Start, and confirm the Story-native defaults.
	if(!practice_poll()) {
		return fail_stage("setup-release");
	}
	for(std::size_t row = 0U; row < 5U; ++row) {
		if(!practice_poll(pulse(0x0002U)) || !practice_poll()) {
			return fail_stage("setup-move");
		}
	}
	if(!practice_poll(pulse(0x0020U))) {
		return fail_stage("setup-start");
	}
	// The Replay Patch owns a 19-boundary black-out after Start. Input is not
	// sampled during it, so do not let a one-tick test pulse accidentally
	// become the MAINL dismissal.
	for(std::size_t frame = 0U; frame < 24U; ++frame) {
		if(!step()) {
			return fail_stage("setup-start-fade");
		}
	}

	for(std::size_t frame = 0U; frame < 1'024U; ++frame) {
		if(!step(pulse((frame & 1U) == 0U ? 0x0020U : 0U))) {
			return false;
		}
		if(scene() == th03::app::ClassicOpSessionScene::vs_main &&
			last->process_advanced) {
			break;
		}
	}
	if(scene() != th03::app::ClassicOpSessionScene::vs_main ||
		!last || !last->process_advanced ||
		!th03::app::ClassicRunnerTestAccess::force_vs_main_outcome(
			runner, th03::app::ClassicOpVsRouteTestOutcome::exit_to_operator
		) || !step() || !step()) {
		return fail_stage("main-terminal");
	}
	auto replay = runner.take_completed_replay();
	if(!replay || !replay->practice || !replay->metadata ||
		!replay->metadata->practice || replay->metadata->flags != 0x0007U ||
		replay->metadata->practice->preset != 1U ||
		replay->checkpoints.empty() ||
		!replay->checkpoints[0U].init.practice_rules ||
		replay->expected_round_splits.empty() ||
		replay->expected_round_splits[0U].story_stage !=
			replay->metadata->practice->stage ||
		!th03::formats::write_v13_replay(*replay)) {
		return fail_stage("practice-replay");
	}
	// Route 0 recreates the selector; Cancel from this dedicated product entry
	// returns across the New Classic session boundary without exposing Classic OP.
	for(std::size_t frame = 0U; frame < 1'024U && runner.status() ==
		th03::app::ClassicRunnerStatus::running; ++frame) {
		if(!step(pulse((frame & 1U) == 0U ? 0x1000U : 0U), true)) {
			return false;
		}
	}
	return runner.status() == th03::app::ClassicRunnerStatus::session_complete ?
		true : fail_stage("return-to-product-selector");
}

bool authentic_gate(
	const std::filesystem::path &data_root,
	const std::filesystem::path &main_executable,
	const std::filesystem::path &mainl_executable,
	const std::filesystem::path &op_executable,
	const std::filesystem::path &font_rom,
	const std::filesystem::path &language_overlay = {}
)
{
	TemporaryDirectory temporary;
	const auto classic_config_path = temporary.path() / "YUME.CFG";
	const auto new_classic_settings_path = temporary.path() / "PODD.CFG";
	std::vector<char> original_classic_config{};
	if(!language_overlay.empty()) {
		th03::app::ClassicConfigOptions options{};
		options.language = th03::app::ClassicLanguage::english;
		if(!th03::app::commit_classic_config_store(
			classic_config_path, options
		)) {
			return false;
		}
		std::ifstream stream(classic_config_path, std::ios::binary);
		original_classic_config.assign(
			std::istreambuf_iterator<char>(stream),
			std::istreambuf_iterator<char>()
		);
	}
	auto loaded = th03::app::ClassicRunner::load({
		.content = {
			.data_root = data_root,
			.main_executable = main_executable,
			.mainl_executable = mainl_executable,
			.op_executable = op_executable,
			.font_rom = font_rom,
			.language_overlay = language_overlay,
			.replay_root = temporary.path() / "REPLAY",
			.config = classic_config_path,
			.new_classic_settings = new_classic_settings_path,
			.score = temporary.path() / "YUME.NEM",
		},
		.seed = 1U,
		.fm_available = false,
		.experience = th03::app::ExperienceProfile::new_classic,
	});
	if(!loaded) {
		std::cerr << "authentic Classic runner load failed code="
			<< static_cast<unsigned>(loaded.error().code)
			<< " detail=" << static_cast<unsigned>(loaded.error().detail_code)
			<< '\n';
		return false;
	}
	const auto migrated_settings =
		th03::app::load_new_classic_settings_store(
			new_classic_settings_path, classic_config_path
		);
	if(!migrated_settings ||
		migrated_settings->settings.options.language !=
			(language_overlay.empty() ?
				th03::app::ClassicLanguage::japanese :
				th03::app::ClassicLanguage::english)) {
		std::cerr << "authentic-script-stage=settings-migration\n";
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
	const auto graphics_region_hash = [&pixels](
		std::int32_t left,
		std::int32_t top,
		std::uint16_t width,
		std::uint16_t height
	) {
		std::uint64_t hash = 14695981039346656037ULL;
		for(std::int32_t y = top; y < top + height; ++y) {
			for(std::int32_t x = left; x < left + width; ++x) {
				const auto &pixel = pixels[
					(static_cast<std::size_t>(y) *
						th03::present::graphics_width) +
					static_cast<std::size_t>(x)
				];
				hash ^= static_cast<std::uint8_t>(pixel.value);
				hash *= 1099511628211ULL;
			}
		}
		return hash;
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
	bool menu_surface_published = false;
	for(std::size_t frame = 0U; frame < 32U && !menu_drawn; ++frame) {
		if(!step()) { return false; }
		menu_surface_published = menu_surface_published || last->surface_changed;
		menu_drawn = last->input_sampled;
	}
	if(!menu_drawn || !menu_surface_published ||
		!graphics_ink(160, 264, 136U, 128U)) {
		std::cerr << "authentic-script-stage=menu tick=" << tick_index << '\n';
		return false;
	}

	// Replay Patch 0.4.11 accepts released Left, Right, Left, Right while the
	// title menu owns input. It persists the two Extra characters and plays the
	// extend effect only on the first successful transition.
	const auto unlock_audio_begin = audio_commands.size();
	for(const auto direction : std::array{
		std::uint16_t{0x0004U}, std::uint16_t{0x0008U},
		std::uint16_t{0x0004U}, std::uint16_t{0x0008U}
	}) {
		if(!step(direction) || !step()) { return false; }
	}
	const auto unlocked = th03::app::load_new_classic_score_store(
		temporary.path() / "YUME.NEW.NEM",
		temporary.path() / "YUME.NEM"
	);
	if(!unlocked || !unlocked->document.extra_unlocked ||
		audio_commands.size() != unlock_audio_begin + 2U ||
		audio_commands[unlock_audio_begin] != th03::app::ClassicAudioCommand{
			.type = th03::app::ClassicAudioCommandType::load_effects,
		} || audio_commands[unlock_audio_begin + 1U] !=
			th03::app::ClassicAudioCommand{
				.type = th03::app::ClassicAudioCommandType::play_effect,
				.parameter = 8U,
			}) {
		std::cerr << "authentic-script-stage=extra-unlock\n";
		return false;
	}
	const auto unlocked_generation = unlocked->document.generation;
	for(const auto direction : std::array{
		std::uint16_t{0x0004U}, std::uint16_t{0x0008U},
		std::uint16_t{0x0004U}, std::uint16_t{0x0008U}
	}) {
		if(!step(direction) || !step()) { return false; }
	}
	const auto unlocked_again = th03::app::load_new_classic_score_store(
		temporary.path() / "YUME.NEW.NEM",
		temporary.path() / "YUME.NEM"
	);
	if(!unlocked_again ||
		unlocked_again->document.generation != unlocked_generation ||
		audio_commands.size() != unlock_audio_begin + 2U) {
		std::cerr << "authentic-script-stage=extra-unlock-repeat\n";
		return false;
	}

	if(!language_overlay.empty()) {
		const auto load_menu_background = [&](bool english) ->
			std::optional<th03::assets::PiImage> {
			th03::app::MainResourceLoader background_loader(data_root);
			if(english) {
				auto overlay = th03::app::load_new_classic_language_overlay(
					language_overlay
				);
				if(!overlay || !background_loader.install_authenticated_overrides(
					std::move(*overlay).release_members()
				)) {
					return std::nullopt;
				}
			}
			auto resources = background_loader.load_op_title_menu();
			if(!resources) {
				return std::nullopt;
			}
			for(const auto &resource : *resources) {
				if(resource.spec.key.role ==
					th03::app::MainResourceRole::op_menu_background &&
					resource.bytes) {
					auto decoded = th03::assets::decode_pi(*resource.bytes);
					if(decoded) {
						return std::move(*decoded);
					}
				}
			}
			return std::nullopt;
		};
		const auto japanese_menu_background = load_menu_background(false);
		const auto english_menu_background = load_menu_background(true);
		if(!japanese_menu_background || !english_menu_background) {
			std::cerr << "authentic-script-stage=language-background-load\n";
			return false;
		}
		bool language_profiles_differ = false;
		for(std::size_t offset = 0U;
			offset < th03::present::graphics_page_pixels; ++offset) {
			if(japanese_menu_background->image.indices[offset] !=
				english_menu_background->image.indices[offset]) {
				language_profiles_differ = true;
				break;
			}
		}
		bool language_menu_rows_equal = true;
		for(std::int32_t y = 272; y < 384 && language_menu_rows_equal; ++y) {
			for(std::int32_t x = 162; x < 430; ++x) {
				const auto offset = (static_cast<std::size_t>(y) *
					th03::present::graphics_width) + static_cast<std::size_t>(x);
				if(japanese_menu_background->image.indices[offset] !=
					english_menu_background->image.indices[offset]) {
					language_menu_rows_equal = false;
					break;
				}
			}
		}
		if(!language_profiles_differ || !language_menu_rows_equal) {
			std::cerr << "authentic-script-stage=language-background-fixture\n";
			return false;
		}
		const auto matches_japanese_background = [&](std::int32_t left,
			std::int32_t top, std::int32_t right, std::int32_t bottom) {
			for(std::int32_t y = top; y < bottom; ++y) {
				for(std::int32_t x = left; x < right; ++x) {
					const auto offset = (static_cast<std::size_t>(y) *
						th03::present::graphics_width) + static_cast<std::size_t>(x);
					const auto &actual = pixels[offset];
					if(actual.layer !=
						th03::present::ComposedPixelLayer::graphics ||
						actual.value !=
						japanese_menu_background->image.indices[offset]) {
						return false;
					}
				}
			}
			return true;
		};
		const auto matches_japanese_menu_rows = [&](std::int32_t right) {
			for(std::int32_t y = 272; y < 384; ++y) {
				for(std::int32_t x = 162; x < right; ++x) {
					const auto offset = (static_cast<std::size_t>(y) *
						th03::present::graphics_width) + static_cast<std::size_t>(x);
					const auto expected = ((x + (y - 264)) & 1) == 0 ? 3U :
						japanese_menu_background->image.indices[offset];
					const auto &actual = pixels[offset];
					if(actual.layer !=
						th03::present::ComposedPixelLayer::graphics ||
						actual.value != expected) {
						return false;
					}
				}
			}
			return true;
		};
		const auto english_background = graphics_region_hash(0, 0, 160U, 400U);
		const auto wait_for_scene_input = [&](th03::app::ClassicOpSessionScene scene,
			std::size_t limit) {
			for(std::size_t frame = 0U; frame < limit; ++frame) {
				if(!step()) { return false; }
				if(th03::app::ClassicRunnerTestAccess::scene(runner) == scene &&
					last && last->input_sampled) {
					return true;
				}
			}
			return false;
		};
		const auto enter_language_row = [&]() {
			for(std::size_t row = 0U; row < 4U; ++row) {
				if(!step(0x0002U) || !step()) { return false; }
			}
			return step(0x0020U) && step();
		};
		const auto sample_input = [&](std::uint16_t system) {
			for(std::size_t frame = 0U; frame < 8U; ++frame) {
				if(!step(system)) { return false; }
				if(last && last->input_sampled) { return true; }
			}
			return false;
		};
		const auto settle_options = [&]() {
			for(std::size_t frame = 0U; frame < 32U; ++frame) {
				if(!step()) { return false; }
			}
			return th03::app::ClassicRunnerTestAccess::scene(runner) ==
				th03::app::ClassicOpSessionScene::options && last &&
				last->input_sampled;
		};
		const auto leave_options = [&]() {
			// First Cancel returns from the Language child to its retained
			// category; release, then a fresh Cancel contracts Options.
			return sample_input(0x1000U) && step() &&
				sample_input(0x1000U);
		};
		for(std::size_t row = 0U; row < 5U; ++row) {
			if(!step(0x0002U) || !step()) { return false; }
		}
		const bool opened_japanese = step(0x0020U);
		const bool ready_japanese = opened_japanese && settle_options();
		const bool selected_language = ready_japanese && enter_language_row();
		const bool toggled_japanese = selected_language && step(0x0020U) && step();
		const bool canceled_japanese = toggled_japanese && leave_options();
		// The Cancel tick changes the scene's phase to shrink_window. The session
		// observes that returned phase and publishes the reloaded Japanese TL02
		// before the following tick can unput Option labels from page 1.
		const bool refreshed_before_shrink = canceled_japanese && last &&
			last->surface_changed && last_surface && japanese_menu_background->palette &&
			last_surface->graphics_palette == *japanese_menu_background->palette &&
			matches_japanese_background(0, 0, 160, 400) &&
			step() && matches_japanese_menu_rows(408);
		bool whole_shrink_uses_japanese = refreshed_before_shrink;
		for(std::size_t frame = 1U; frame < 18U && whole_shrink_uses_japanese;
			++frame) {
			whole_shrink_uses_japanese = step() &&
				matches_japanese_menu_rows(272);
		}
		const bool exited_japanese = whole_shrink_uses_japanese &&
			wait_for_scene_input(
				th03::app::ClassicOpSessionScene::title_menu, 256U
			);
		if(!exited_japanese) {
			std::cerr << "authentic-script-stage=language-japanese"
				<< " open=" << opened_japanese
				<< " ready=" << ready_japanese
				<< " selected=" << selected_language
				<< " toggled=" << toggled_japanese
				<< " canceled=" << canceled_japanese
				<< " refreshed=" << refreshed_before_shrink
				<< " shrink=" << whole_shrink_uses_japanese << '\n';
			return false;
		}
		const auto stored_before_commit =
			th03::app::load_new_classic_settings_store(
				new_classic_settings_path, classic_config_path
			);
		std::ifstream classic_stream(classic_config_path, std::ios::binary);
		const std::vector<char> current_classic_config{
			std::istreambuf_iterator<char>(classic_stream),
			std::istreambuf_iterator<char>()
		};
		if(!stored_before_commit ||
			stored_before_commit->settings.options.language !=
				th03::app::ClassicLanguage::english ||
			current_classic_config != original_classic_config) {
			std::cerr << "authentic-script-stage=settings-precommit-isolation\n";
			return false;
		}
		const auto japanese_background = graphics_region_hash(0, 0, 160U, 400U);
		if(japanese_background == english_background ||
			!step(0x0020U) || !settle_options() || !enter_language_row() ||
			!step(0x0020U) || !step() ||
			!leave_options() || !wait_for_scene_input(
				th03::app::ClassicOpSessionScene::title_menu, 256U
			) || graphics_region_hash(0, 0, 160U, 400U) != english_background) {
			std::cerr << "authentic-script-stage=language-english\n";
			return false;
		}
		for(std::size_t row = 0U; row < 5U; ++row) {
			if(!step(0x0001U) || !step()) { return false; }
		}
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
	if(!language_overlay.empty()) {
		const auto stored = th03::app::load_new_classic_settings_store(
			new_classic_settings_path, classic_config_path
		);
		std::ifstream classic_stream(classic_config_path, std::ios::binary);
		const std::vector<char> current_classic_config{
			std::istreambuf_iterator<char>(classic_stream),
			std::istreambuf_iterator<char>()
		};
		if(!stored || stored->settings.options.language !=
			th03::app::ClassicLanguage::english ||
			current_classic_config != original_classic_config) {
			std::cerr << "authentic-script-stage=settings-commit-isolation\n";
			return false;
		}
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
	if(!language_overlay.empty() && (!graphics_ink(80, 276, 240U, 16U) ||
		!graphics_ink(336, 276, 240U, 16U))) {
		std::cerr << "authentic-script-stage=english-splash-title-blank tick="
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
		force_story_main_outcome(
			runner,
			th03::app::ClassicStoryRouteTestOutcome::player_win
		);
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
				force_story_main_outcome(
					runner,
					th03::app::ClassicStoryRouteTestOutcome::player_win
				);
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
	if(!valid_completed_replay(
		runner.take_completed_replay(),
		th03::formats::ReplayEndReason::complete, 9U
	)) {
		std::cerr << "authentic-script-stage=clear-replay\n";
		return false;
	}

	const auto score = th03::app::load_new_classic_score_store(
		temporary.path() / "YUME.NEW.NEM",
		temporary.path() / "YUME.NEM"
	);
	const std::array expected_name{'R', 'E', 'I', 'M', 'U', ' ', ' ', ' '};
	if(!score || !score->document.extra_unlocked ||
		!score->document.boards[1U][0U].entries[0U].valid ||
		score->document.boards[1U][0U].entries[0U].name != expected_name ||
		score->document.boards[1U][0U].stats.play_frames == 0U ||
		score->document.boards[1U][0U].stats.one_ccs != 1U ||
		std::filesystem::exists(temporary.path() / "YUME.NEM")) {
		std::cerr << "authentic-script-stage=score-persistence";
		if(score) {
			const auto &board = score->document.boards[1U][0U];
			std::cerr << " unlock=" << score->document.extra_unlocked
				<< " frames=" << board.stats.play_frames
				<< " one-cc=" << board.stats.one_ccs << " name=";
			for(const auto glyph : board.entries[0U].name) {
				std::cerr << glyph;
			}
		}
		std::cerr << '\n';
		return false;
	}

	// Re-enter Story through the real title and selector for each remaining
	// terminal branch. The helper stops only after the new MAIN has executed its
	// first process frame, preserving the same capability boundary as the clear
	// spine above.
	const auto enter_story_main = [&]() {
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
		if(!menu_drawn || !step(0x0020U) ||
			th03::app::ClassicRunnerTestAccess::scene(runner) !=
				th03::app::ClassicOpSessionScene::character_selection) {
			return false;
		}
		reached_input = false;
		for(std::size_t frame = 0U; frame < 64U && !reached_input; ++frame) {
			if(!step()) { return false; }
			reached_input = last->input_sampled;
		}
		if(!reached_input || !hold_until_sampled(0x0020U)) {
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
			return false;
		}
		for(std::size_t frame = 0U; frame < 512U; ++frame) {
			const auto scene =
				th03::app::ClassicRunnerTestAccess::story_scene(runner);
			const std::uint16_t system = scene && *scene !=
				th03::app::ClassicStorySessionScene::main &&
				((tick_index & 1U) == 0U) ? 0x0020U : 0U;
			if(!step(system)) { return false; }
			if(last->process_advanced &&
				th03::app::ClassicRunnerTestAccess::story_scene(runner) ==
					th03::app::ClassicStorySessionScene::main) {
				return true;
			}
		}
		return false;
	};

	enum class BranchPolicy : std::uint8_t {
		loss_then_clear,
		decline_continue,
		exhaust_continues,
		exit_main,
	};
	struct BranchTrace final {
		std::array<std::size_t, 10U> scenes{};
		std::size_t main_outcomes = 0U;
	};
	const auto drive_branch = [&](BranchPolicy policy) ->
		std::optional<BranchTrace> {
		BranchTrace result{};
		++result.scenes[static_cast<std::size_t>(
			th03::app::ClassicStorySessionScene::stage_splash
		)];
		++result.scenes[static_cast<std::size_t>(
			th03::app::ClassicStorySessionScene::main
		)];
		auto previous = std::optional{
			th03::app::ClassicStorySessionScene::main
		};
		bool main_forced_for_entry = false;
		const auto outcome_for = [&](std::size_t index) {
			switch(policy) {
			case BranchPolicy::loss_then_clear:
				return index == 0U ?
					th03::app::ClassicStoryRouteTestOutcome::player_loss :
					th03::app::ClassicStoryRouteTestOutcome::player_win;
			case BranchPolicy::decline_continue:
			case BranchPolicy::exhaust_continues:
				return th03::app::ClassicStoryRouteTestOutcome::player_loss;
			case BranchPolicy::exit_main:
				return th03::app::ClassicStoryRouteTestOutcome::exit_to_operator;
			}
			return th03::app::ClassicStoryRouteTestOutcome::exit_to_operator;
		};
		auto forced = th03::app::ClassicRunnerTestAccess::
			force_story_main_outcome(runner, outcome_for(0U));
		if(!forced) {
			return std::nullopt;
		}
		main_forced_for_entry = true;
		result.main_outcomes = 1U;
		for(std::size_t frame = 0U; frame < 300'000U; ++frame) {
			const auto before =
				th03::app::ClassicRunnerTestAccess::story_scene(runner);
			std::uint16_t system = 0U;
			if(before == th03::app::ClassicStorySessionScene::continue_menu &&
				policy == BranchPolicy::decline_continue) {
				system = 0x1000U;
			} else if(before && *before !=
				th03::app::ClassicStorySessionScene::main &&
				((tick_index & 1U) == 0U)) {
				system = 0x0020U;
			}
			if(!step(system)) {
				return std::nullopt;
			}
			const auto after =
				th03::app::ClassicRunnerTestAccess::story_scene(runner);
			if(after != previous) {
				if(after) {
					const auto index = static_cast<std::size_t>(*after);
					if(index >= result.scenes.size()) {
						return std::nullopt;
					}
					++result.scenes[index];
				}
				main_forced_for_entry = false;
				previous = after;
			}
			if(after == th03::app::ClassicStorySessionScene::main &&
				last->process_advanced && !main_forced_for_entry) {
				auto next = th03::app::ClassicRunnerTestAccess::
					force_story_main_outcome(
						runner, outcome_for(result.main_outcomes)
					);
				if(!next) {
					return std::nullopt;
				}
				main_forced_for_entry = true;
				++result.main_outcomes;
			}
			if(th03::app::ClassicRunnerTestAccess::scene(runner) ==
				th03::app::ClassicOpSessionScene::title_menu) {
				return result;
			}
		}
		return std::nullopt;
	};
	const auto count_branch_scene = [](const BranchTrace &branch, auto scene) {
		return branch.scenes[static_cast<std::size_t>(scene)];
	};

	if(!enter_story_main()) { return false; }
	const auto continued = drive_branch(BranchPolicy::loss_then_clear);
	if(!continued || continued->main_outcomes != 10U ||
		count_branch_scene(*continued,
			th03::app::ClassicStorySessionScene::stage_splash) != 10U ||
		count_branch_scene(*continued,
			th03::app::ClassicStorySessionScene::win) != 8U ||
		count_branch_scene(*continued,
			th03::app::ClassicStorySessionScene::continue_menu) != 1U ||
		count_branch_scene(*continued,
			th03::app::ClassicStorySessionScene::game_over) != 0U ||
		count_branch_scene(*continued,
			th03::app::ClassicStorySessionScene::cutscene) != 3U ||
		count_branch_scene(*continued,
			th03::app::ClassicStorySessionScene::staff_verdict) != 1U ||
		count_branch_scene(*continued,
			th03::app::ClassicStorySessionScene::high_score_registration) != 2U) {
		std::cerr << "authentic-script-stage=continue-clear-branch\n";
		return false;
	}
	if(!valid_completed_replay(
		runner.take_completed_replay(),
		th03::formats::ReplayEndReason::game_over, 1U, true
	)) {
		return false;
	}

	if(!enter_story_main()) { return false; }
	const auto declined = drive_branch(BranchPolicy::decline_continue);
	if(!declined || declined->main_outcomes != 1U ||
		count_branch_scene(*declined,
			th03::app::ClassicStorySessionScene::win) != 1U ||
		count_branch_scene(*declined,
			th03::app::ClassicStorySessionScene::continue_menu) != 1U ||
		count_branch_scene(*declined,
			th03::app::ClassicStorySessionScene::game_over) != 1U ||
		count_branch_scene(*declined,
			th03::app::ClassicStorySessionScene::high_score_registration) != 1U) {
		std::cerr << "authentic-script-stage=continue-decline-branch\n";
		return false;
	}
	if(!valid_completed_replay(
		runner.take_completed_replay(),
		th03::formats::ReplayEndReason::game_over, 1U, true
	)) {
		return false;
	}

	if(!enter_story_main()) { return false; }
	const auto exhausted = drive_branch(BranchPolicy::exhaust_continues);
	if(!exhausted || exhausted->main_outcomes != 4U ||
		count_branch_scene(*exhausted,
			th03::app::ClassicStorySessionScene::win) != 4U ||
		count_branch_scene(*exhausted,
			th03::app::ClassicStorySessionScene::continue_menu) != 3U ||
		count_branch_scene(*exhausted,
			th03::app::ClassicStorySessionScene::game_over) != 1U ||
		count_branch_scene(*exhausted,
			th03::app::ClassicStorySessionScene::high_score_registration) != 4U) {
		std::cerr << "authentic-script-stage=continue-exhausted-branch\n";
		return false;
	}
	if(!valid_completed_replay(
		runner.take_completed_replay(),
		th03::formats::ReplayEndReason::game_over, 1U, true
	)) {
		return false;
	}

	if(!enter_story_main()) { return false; }
	const auto exited = drive_branch(BranchPolicy::exit_main);
	if(!exited || exited->main_outcomes != 1U ||
		count_branch_scene(*exited,
			th03::app::ClassicStorySessionScene::win) != 0U ||
		count_branch_scene(*exited,
			th03::app::ClassicStorySessionScene::high_score_registration) != 0U) {
		std::cerr << "authentic-script-stage=main-exit-branch\n";
		return false;
	}
	if(!valid_completed_replay(
		runner.take_completed_replay(),
		th03::formats::ReplayEndReason::menu_return, 1U, true
	)) {
		return false;
	}
	const auto final_score = th03::app::load_new_classic_score_store(
		temporary.path() / "YUME.NEW.NEM"
	);
	// Four successful registrations each roll the committed run into a fresh
	// active run. The aggregate includes every source VSync owned by MAIN or
	// MAINL across the complete Story branch matrix above, and excludes OP.
	if(!final_score || final_score->document.last_run_id != 13U ||
		final_score->document.boards[1U][0U].stats.play_frames != 33084U ||
		final_score->document.boards[1U][0U].stats.one_ccs != 1U ||
		final_score->document.boards[1U][0U].stats.continues_used != 4U) {
		std::cerr << "authentic-script-stage=score-statistics";
		if(final_score) {
			const auto &stats = final_score->document.boards[1U][0U].stats;
			std::cerr << " run=" << final_score->document.last_run_id
				<< " frames=" << stats.play_frames
				<< " one-cc=" << stats.one_ccs
				<< " continues=" << stats.continues_used;
		}
		std::cerr << '\n';
		return false;
	}

	// The final returned title is also a fresh real scene. Open its real menu and
	// exit through Cancel rather than ending at an application-private boundary.
	reached_input = false;
	for(std::size_t frame = 0U; frame < 300U && !reached_input; ++frame) {
		if(!step()) { return false; }
		reached_input = last->input_sampled;
	}
	if(!reached_input || !step(0x0020U)) { return false; }
	menu_drawn = false;
	for(std::size_t frame = 0U; frame < 32U && !menu_drawn; ++frame) {
		if(!step()) { return false; }
		menu_drawn = last->input_sampled;
	}
	if(!menu_drawn || !step() || !step(0x1000U, true)) { return false; }
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
	test_classic_catch_up_policy();
	test_fresh_resident_state();
	test_pixel_oracle_publication_ownership();
	test_runner_sequence_audio_surface_and_failure();
	test_file_and_request_boundaries();
	if(argc == 8 && std::string_view(argv[1]) == "--authentic-online-watch") {
		auto bytes = read_bytes(argv[7]);
		std::optional<th03::formats::ReplayDocument> document{};
		if(bytes) {
			if(auto parsed = th03::formats::parse_v13_replay(*bytes)) {
				document = std::move(*parsed);
			}
		}
		auto replay_hub_document = document;
		expect(replay_hub_document && authentic_replay_hub_gate(
			argv[2], argv[3], argv[4], argv[5], argv[6],
			std::move(replay_hub_document)
		), "downloaded Replay library crosses into New Classic playback");
		expect(document && authentic_online_high_score_gate(
			argv[2], argv[3], argv[4], argv[5], argv[6],
			std::move(document)
		), "downloaded online replay crosses into New Classic playback");
	} else if(argc == 8 && std::string_view(argv[1]) == "--authentic-english") {
		expect(authentic_gate(
			argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]
		), "supplied English overlay traverses the complete New Classic runner");
	} else if(argc == 7 && (std::string_view(argv[1]) == "--authentic" ||
		std::string_view(argv[1]) == "--authentic-local-hiscore" ||
		std::string_view(argv[1]) == "--authentic-vs" ||
		std::string_view(argv[1]) == "--authentic-practice" ||
		std::string_view(argv[1]) == "--authentic-online")) {
		if(std::string_view(argv[1]) == "--authentic") {
			expect(authentic_gate(argv[2], argv[3], argv[4], argv[5], argv[6]),
				"supplied originals construct and advance the complete Classic runner");
			expect(authentic_local_high_score_gate(
				argv[2], argv[3], argv[4], argv[5], argv[6]
			), "stats-only profile traverses the direct New Classic HiScore route");
				expect(authentic_online_high_score_gate(
					argv[2], argv[3], argv[4], argv[5], argv[6]
				), "validated cached board traverses the New Classic HiScore hub");
				expect(authentic_replay_hub_gate(
					argv[2], argv[3], argv[4], argv[5], argv[6]
				), "validated cached board traverses the New Classic Replay hub");
		} else if(std::string_view(argv[1]) == "--authentic-local-hiscore") {
			expect(authentic_local_high_score_gate(
				argv[2], argv[3], argv[4], argv[5], argv[6]
			), "stats-only profile traverses the direct New Classic HiScore route");
			} else if(std::string_view(argv[1]) == "--authentic-online") {
				expect(authentic_online_high_score_gate(
					argv[2], argv[3], argv[4], argv[5], argv[6]
				), "validated cached board traverses the New Classic HiScore hub");
				expect(authentic_replay_hub_gate(
					argv[2], argv[3], argv[4], argv[5], argv[6]
				), "validated cached board traverses the New Classic Replay hub");
		} else if(std::string_view(argv[1]) != "--authentic-practice") {
			expect(authentic_vs_gate(argv[2], argv[3], argv[4], argv[5], argv[6]),
				"supplied originals traverse all three complete Classic VS modes");
		} else {
			expect(authentic_practice_gate(
				argv[2], argv[3], argv[4], argv[5], argv[6]
			), "supplied originals traverse complete New Classic Practice");
		}
	} else if(argc != 1) {
		std::cerr << "usage: th03-app-classic-runner-tests "
			"[--authentic|--authentic-local-hiscore|--authentic-vs|"
			"--authentic-practice|--authentic-online "
			"<data-root> <MAIN.EXE> <MAINL.EXE> <OP.EXE> <FONT.ROM> | "
			"--authentic-english <data-root> <MAIN.EXE> <MAINL.EXE> "
			"<OP.EXE> <FONT.ROM> <TH3EN.DAT> | --authentic-online-watch "
			"<data-root> <MAIN.EXE> <MAINL.EXE> <OP.EXE> <FONT.ROM> "
			"<V13.RPY>]\n";
		return 2;
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
