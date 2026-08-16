#include "th03/app/new_classic_playback.hpp"

#include "classic_mainl_stage_internal.hpp"
#include "classic_input_bits_internal.hpp"
#include "classic_mainl_win_internal.hpp"
#include "classic_story_session_internal.hpp"
#include "main_process_static_content_internal.hpp"
#include "main_process_internal.hpp"
#include "new_classic_language_internal.hpp"
#include "new_classic_playback_internal.hpp"
#include "main_process_driver_internal.hpp"
#include "th03/app/new_classic_pause.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace th03::app {
namespace {

[[nodiscard]] NewClassicPlaybackError playback_error(
	NewClassicPlaybackErrorCode code
) noexcept
{
	return {.code = code};
}

[[nodiscard]] constexpr bool replay_patch_0412_game_over_playback(
	std::uint8_t route,
	int winner
) noexcept
{
	return route != 0U && winner != 0;
}

static_assert(replay_patch_0412_game_over_playback(2U, 1));

[[nodiscard]] constexpr bool replay_patch_final_story_clear_playback(
	const GameInit &process_start,
	const CanonicalState &terminal,
	const SessionTransition &transition
) noexcept
{
	return process_start.game_mode == game_mode::story &&
		process_start.story_stage == 8U && terminal.route == 2U &&
		terminal.winner == 0 && transition.action == SessionAction::ending;
}

[[nodiscard]] std::expected<std::vector<std::byte>, NewClassicPlaybackError>
read_file(const std::filesystem::path &path)
{
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system || !std::filesystem::is_regular_file(status)) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::file_missing
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::size_t>::max()
	) || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::streamsize>::max()
	)) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::file_read_failed
		));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::file_read_failed
		));
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	if(!bytes.empty()) {
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::file_read_failed
		));
	}
	return bytes;
}

std::expected<StepResult, MainProcessError> process_step(
	void *context,
	CoreInput input,
	InputContext input_context
) noexcept
{
	return static_cast<MainProcess *>(context)->step(input, input_context);
}

bool process_deferred_pending(void *context) noexcept
{
	return static_cast<MainProcess *>(context)->deferred_frame_pending();
}

std::expected<StepResult, MainProcessError> process_finish_deferred(
	void *context,
	DeferredFrameAction action
) noexcept
{
	return static_cast<MainProcess *>(context)->finish_deferred_frame(action);
}

std::expected<void, MainProcessError> process_repeat(void *context) noexcept
{
	return static_cast<MainProcess *>(context)->start_next_round();
}

std::expected<void, MainProcessError> process_finish(void *context) noexcept
{
	return static_cast<MainProcess *>(context)->finish_main_process();
}

std::expected<SessionTransition, MainProcessError> process_transition(
	void *context,
	std::optional<ContinueChoice> choice
) noexcept
{
	return static_cast<MainProcess *>(context)->session_transition(choice);
}

[[nodiscard]] bool put_main_replay_overlay(
	present::Raster &raster,
	present::Tram &tram
) noexcept
{
	// The source draws this overlay immediately before replay_frame_publish(),
	// which leaves the following frame's hidden page accessed. Native playback
	// installs the same two-page result immediately after the committed footer,
	// so restore that following hidden page explicitly.
	const auto back = raster.shown_page() == present::Page::zero ?
		present::Page::one : present::Page::zero;
	for(const auto page : {present::Page::zero, present::Page::one}) {
		if(!raster.set_access_page(page) ||
			!raster.clear({296, 0, 48U, 8U}, 15U)) {
			return false;
		}
	}
	if(!raster.set_access_page(back)) {
		return false;
	}
	constexpr std::array<char, 6U> text{{'R', 'E', 'P', 'L', 'A', 'Y'}};
	constexpr auto attribute = static_cast<present::TramAttribute>(
		present::tram_black | present::tram_reverse
	);
	for(std::size_t index = 0U; index < text.size(); ++index) {
		if(!tram.put_cell(
			{37 + static_cast<std::int32_t>(index), 0},
			{static_cast<present::TramCode>(text[index]), attribute}
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] ClassicStorySessionInput playback_story_input(
	std::uint16_t source,
	ClassicAudioStatus audio
) noexcept
{
	const auto pressed = [source](std::uint16_t bit) noexcept {
		return (source & bit) != 0U;
	};
	const bool any = source != 0U;
	ClassicStorySessionInput input{};
	input.main.aggregate.system = source;
	input.cutscene.script = {
		.cancel_held = pressed(input_cancel),
		.any_key_held = any,
	};
	input.cutscene.shot_or_ok_held =
		pressed(input_shot) || pressed(input_ok);
	input.cutscene.audio = audio;
	input.staff = {
		.audio = audio,
		.any_input = any,
		.fast_forward_held = false,
		.vsync_count_above_one = false,
	};
	input.registration = {
		.raw_any = any,
		.up = pressed(input_up),
		.down = pressed(input_down),
		.left = pressed(input_left),
		.right = pressed(input_right),
		.ok = pressed(input_ok),
		.shot = pressed(input_shot),
		.bomb = pressed(input_bomb),
		.cancel = pressed(input_cancel),
	};
	input.continue_menu = {
		.left = pressed(input_left),
		.right = pressed(input_right),
		.ok = pressed(input_ok),
		.shot = pressed(input_shot),
		.cancel = pressed(input_cancel),
	};
	input.audio = audio;
	input.dismiss_pressed = any;
	return input;
}

[[nodiscard]] constexpr CoreInput mainl_end_input(
	CoreInput replay_input,
	CoreInput live_interface_input,
	bool registration
) noexcept
{
	// regist_menu() polls the unwrapped hardware interface. Every other MAINL
	// owner remains behind mainl_replay_input_mode_interface(), which keeps
	// publishing the last decoded replay sample once MAINL_END is pending.
	return registration ? live_interface_input : replay_input;
}

[[nodiscard]] std::optional<MainlStageResourceRequest> stage_request(
	const GameInit &resident
) noexcept
{
	const auto player_one = main_player_selection_from_paletted(
		resident.paletted_character[0U]
	);
	const auto player_two = main_player_selection_from_paletted(
		resident.paletted_character[1U]
	);
	if(!player_one || !player_two) {
		return std::nullopt;
	}
	return MainlStageResourceRequest{
		.player_one = *player_one,
		.player_two = *player_two,
		.game_mode = resident.game_mode,
		.story_stage = resident.story_stage,
	};
}

} // namespace

struct NewClassicReplayPlayback::Impl final {
	formats::ReplayDocument document{};
	std::shared_ptr<MainResourceLoader> loader{};
	MainCompiledContent compiled_content;
	std::vector<std::byte> font{};
	std::shared_ptr<const MainProcessStaticContent> static_content{};
	MainlStageTextAssets stage_text{};
	MainlStaffCompiledAssets staff_compiled{};
	std::filesystem::path score_path{};
	std::filesystem::path legacy_score_path{};
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets{};
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
	std::unique_ptr<MainProcess> process{};
	std::unique_ptr<MainProcessDriver> driver{};
	std::unique_ptr<ClassicMainlStageTransition> replay_entry_stage{};
	std::unique_ptr<ClassicStorySession> story{};
	std::unique_ptr<ClassicMainlWinScene> vs_win{};
	present::Tram blank_tram{};
	GameInit process_start{};
	std::size_t active_checkpoint = 0U;
	std::size_t record_index = 0U;
	std::uint32_t sample_index = 0U;
	CoreInput mainl_replay_input{};
	bool in_mainl = false;
	bool terminal_mainl_bypass = false;
	bool terminal_mainl_bypass_taken = false;
	bool mainl_error_progression = false;
	bool preroll_simulating = false;
	bool suppress_preroll_presentation = true;
	std::uint32_t preroll_sample_count = 0U;
	std::uint32_t stage_splash_end_drain_ticks = 0U;
	bool fast_forward = false;
	bool reduced_flash = false;
	NewClassicPlaybackStatus status = NewClassicPlaybackStatus::running;
	std::optional<GameInit> last_main_exit_resident{};
	std::optional<GameInit> exit_resident{};
	std::optional<ClassicAudioCommand> load_song{};
	std::array<ClassicAudioCommand, 2U> preroll_audio{};
	std::size_t preroll_audio_index = 0U;
	std::size_t preroll_audio_count = 0U;
	std::array<ClassicAudioCommand, 2U> ui_audio{};
	std::size_t ui_audio_index = 0U;
	std::size_t ui_audio_count = 0U;
	std::optional<NewClassicPauseMenu> pause_menu{};

	[[nodiscard]] PixelOracleSourceIdentity pixel_oracle_source_identity(
	) const noexcept
	{
		PixelOracleSourceIdentity identity{
			.process = (in_mainl || replay_entry_stage) ?
				PixelOracleProcess::mainl :
				PixelOracleProcess::main,
			.scene = PixelOracleScene::unknown,
			.logical_sample = sample_index,
			.replay_global_frame = sample_index,
		};
		if(replay_entry_stage) {
			identity.scene = PixelOracleScene::stage_splash;
		} else if(pause_menu) {
			identity.scene = PixelOracleScene::pause;
		} else if(story) {
			identity.replay_global_frame = story->source_sample_count();
			switch(story->scene()) {
			case ClassicStorySessionScene::stage_splash:
				identity.scene = PixelOracleScene::stage_splash;
				break;
			case ClassicStorySessionScene::main:
				identity.process = PixelOracleProcess::main;
				identity.scene = PixelOracleScene::gameplay;
				break;
			case ClassicStorySessionScene::win:
				identity.scene = PixelOracleScene::win;
				break;
			case ClassicStorySessionScene::cutscene:
				identity.scene = PixelOracleScene::ending;
				break;
			case ClassicStorySessionScene::staff_verdict:
				identity.scene = PixelOracleScene::staff_roll;
				break;
			case ClassicStorySessionScene::high_score_registration:
				identity.scene = PixelOracleScene::high_score;
				break;
			case ClassicStorySessionScene::continue_menu:
				identity.scene = PixelOracleScene::continue_menu;
				break;
			case ClassicStorySessionScene::game_over:
				identity.scene = PixelOracleScene::game_over;
				break;
			case ClassicStorySessionScene::complete:
			case ClassicStorySessionScene::failed:
				break;
			}
			if(identity.process == PixelOracleProcess::main) {
				if(const auto state = story->main_canonical_state()) {
					identity.replay_global_frame = state->global_frame;
					identity.round_frame = state->round_frame;
				}
			}
		} else if(vs_win) {
			identity.scene = PixelOracleScene::win;
		} else if(terminal_mainl_bypass) {
			identity.scene = PixelOracleScene::game_over;
		} else if(process) {
			identity.scene = PixelOracleScene::gameplay;
		}
		if(process && identity.process == PixelOracleProcess::main) {
			const auto state = process->canonical_state();
			identity.replay_global_frame = state.global_frame;
			identity.round_frame = state.round_frame;
		}
		return identity;
	}

	[[nodiscard]] std::expected<void, NewClassicPlaybackError>
	start_replay_entry_stage(std::size_t checkpoint_index)
	{
		if(checkpoint_index >= document.checkpoints.size() || replay_entry_stage ||
			story || vs_win) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		auto resident = document.checkpoints[checkpoint_index].init;
		resident.runtime_snapshot.reset();
		resident.compact_runtime_snapshot.reset();
		resident.round_start_snapshot.reset();
		resident.practice_rules.reset();
		const auto request = stage_request(resident);
		if(!request) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::invalid_request
			));
		}
		auto resources = loader->load_mainl_stage(*request);
		if(!resources) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::resource_failed
			);
			error.resource = std::move(resources.error());
			return std::unexpected(std::move(error));
		}
		const GameOptions game_options{
			.input_protocol = InputProtocol::recorded_replay,
			.three_digit_combos = true,
		};
		std::expected<
			std::unique_ptr<ClassicMainlStageTransition>,
			ClassicMainlStageError
		> created = std::unexpected(ClassicMainlStageError{
			.code = ClassicMainlStageErrorCode::invalid_config,
		});
		if(resident.game_mode == game_mode::story) {
			created = ClassicMainlStageTransition::create(
				{
					.resident = resident,
					.game_options = game_options,
					.kotohime_compatibility = kotohime_compatibility,
				},
				*resources, stage_text, static_content->font()
			);
		} else if(resident.game_mode >= game_mode::vs_1p_cpu &&
			resident.game_mode <= game_mode::vs_cpu_cpu) {
			created = ClassicMainlStageTransition::create_from_restart(
				resident, kotohime_compatibility, *resources, stage_text,
				static_content->font(), game_options,
				document.metadata ? document.metadata->practice : std::nullopt
			);
		}
		if(!created) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::interstitial_failed
			);
			error.interstitial_detail = static_cast<std::uint8_t>(
				created.error().code
			);
			return std::unexpected(std::move(error));
		}
		replay_entry_stage = std::move(*created);
		// MAIN's source start only plays the song prepared by MAINL. The old
		// direct-MAIN path had to synthesize that load itself; suppress it while
		// the authentic entry stage owns stop/load/effect preparation.
		load_song.reset();
		return {};
	}

	[[nodiscard]] bool put_replay_overlay() noexcept
	{
		if(preroll_simulating) {
			return true;
		}
		return process && !in_mainl && put_main_replay_overlay(
			MainProcessReplayPresentationAccess::raster(*process),
			MainProcessReplayPresentationAccess::tram(*process)
		);
	}

	[[nodiscard]] bool set_exit_resident(GameInit resident) noexcept
	{
		resident.game_mode = game_mode::none;
		resident.runtime_snapshot.reset();
		resident.compact_runtime_snapshot.reset();
		resident.round_start_snapshot.reset();
		resident.practice_rules.reset();
		exit_resident = std::move(resident);
		return true;
	}

	[[nodiscard]] bool capture_abort_resident() noexcept
	{
		if(!in_mainl && process) {
			auto transition = process->abort_transition();
			return transition && set_exit_resident(
				std::move(transition->resident)
			);
		}
		if(story) {
			return set_exit_resident(story->resident());
		}
		return last_main_exit_resident &&
			set_exit_resident(*last_main_exit_resident);
	}

	[[nodiscard]] bool queue_abort_stop() noexcept
	{
		if(load_song || preroll_audio_count != 0U || ui_audio_count != 0U) {
			return false;
		}
		ui_audio[0U] = {.type = ClassicAudioCommandType::stop_song};
		ui_audio_index = 0U;
		ui_audio_count = 1U;
		return true;
	}

	Impl(
		formats::ReplayDocument document_,
		std::shared_ptr<MainResourceLoader> loader_,
		MainCompiledContent compiled_content_,
		std::vector<std::byte> font_,
		std::shared_ptr<const MainProcessStaticContent> static_content_,
		MainlStageTextAssets stage_text_,
		MainlStaffCompiledAssets staff_compiled_,
		std::filesystem::path score_path_,
		std::filesystem::path legacy_score_path_,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_,
		present::KotohimeBombFadeCompatibility compatibility,
		bool reduced_flash_
	) :
		document(std::move(document_)),
		loader(std::move(loader_)),
		compiled_content(std::move(compiled_content_)),
		font(std::move(font_)),
		static_content(std::move(static_content_)),
		stage_text(std::move(stage_text_)),
		staff_compiled(std::move(staff_compiled_)),
		score_path(std::move(score_path_)),
		legacy_score_path(std::move(legacy_score_path_)),
		patch_assets(std::move(patch_assets_)),
		kotohime_compatibility(compatibility),
		reduced_flash(reduced_flash_)
	{
		blank_tram.fill({
			' ', static_cast<present::TramAttribute>(present::tram_black)
		});
	}

	[[nodiscard]] std::expected<void, NewClassicPlaybackError>
	start_checkpoint(std::size_t checkpoint_index)
	{
		if(checkpoint_index >= document.checkpoints.size()) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::invalid_request
			));
		}
		const auto &checkpoint = document.checkpoints[checkpoint_index];
		const auto player_one = main_player_selection_from_paletted(
			checkpoint.init.paletted_character[0U]
		);
		const auto player_two = main_player_selection_from_paletted(
			checkpoint.init.paletted_character[1U]
		);
		if(!player_one || !player_two) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::invalid_request
			));
		}
		auto resources = loader->load_direct_vs(*player_one, *player_two);
		if(!resources) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::resource_failed
			);
			error.resource = std::move(resources.error());
			return std::unexpected(std::move(error));
		}
		auto created = MainProcess::create_replay(
			{
				.checkpoint = checkpoint.init,
				.game_options = {
					.input_protocol = InputProtocol::recorded_replay,
					.three_digit_combos = true,
				},
				.initialize_fresh = !document.practice,
				.kotohime_compatibility = kotohime_compatibility,
			},
			*resources,
			compiled_content,
			font
		);
		if(!created) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::process_failed
			);
			error.process = std::move(created.error());
			return std::unexpected(std::move(error));
		}
		process = std::move(*created);
		MainProcessReplayPresentationAccess::set_preroll_simulating(
			*process,
			preroll_simulating && suppress_preroll_presentation
		);
		auto created_driver = MainProcessDriver::create({
			.context = process.get(),
			.step = &process_step,
			.deferred_frame_pending = &process_deferred_pending,
			.finish_deferred_frame = &process_finish_deferred,
			.start_next_round = &process_repeat,
			.finish_main_process = &process_finish,
			.session_transition = &process_transition,
		});
		if(!created_driver) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::driver_failed
			));
		}
		driver = std::move(*created_driver);
		active_checkpoint = checkpoint_index;
		process_start = checkpoint.init;
		// MainProcess::create_replay() applies the same fresh-start projection
		// before constructing the process.  Keep the retained handoff resident
		// identical to that effective process start so the shared Story MAINL
		// owner receives a source-valid resident without replay-only snapshots.
		if(!document.practice) {
			process_start.compact_runtime_snapshot.reset();
			process_start.round_start_snapshot.reset();
			process_start.practice_rules.reset();
		}
		sample_index = checkpoint.sample_index;
		mainl_replay_input = {};
		in_mainl = false;
		terminal_mainl_bypass = false;
		mainl_error_progression = false;
		story.reset();
		vs_win.reset();
		load_song = classic_main_gameplay_song(checkpoint.init);
		if(!load_song) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::invalid_request
			));
		}
		return {};
	}

	[[nodiscard]] bool rearm_gameplay_audio_after_preroll() noexcept
	{
		const auto song = classic_main_gameplay_song(process_start);
		if(!song || load_song || preroll_audio_count != 0U) {
			return false;
		}
		load_song = *song;
		auto play = *song;
		play.type = ClassicAudioCommandType::play_song;
		preroll_audio = {{
			{.type = ClassicAudioCommandType::reset_effects},
			play,
		}};
		preroll_audio_index = 0U;
		preroll_audio_count = preroll_audio.size();
		return true;
	}

	[[nodiscard]] std::expected<void, NewClassicPlaybackError>
	start_mainl_owner()
	{
		if(!process || !driver ||
			driver->status() != MainProcessDriverStatus::terminal ||
			!driver->transition()) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		const auto terminal = process->canonical_state();
		// Replay Patch 0.4.12 marks every recorded P2 win as Game Over
		// playback, including Practice and VS. MAINL consumes that marker and
		// returns to OP before constructing any winner or registration owner.
		// Since 0.2.3, final Story playback likewise exits at the entry to
		// ending_staff_and_regist(), before constructing the ending, staff,
		// verdict, or registration owner.
		if(replay_patch_0412_game_over_playback(
			terminal.route, terminal.winner
		) || replay_patch_final_story_clear_playback(
			process_start, terminal, *driver->transition()
		)) {
			terminal_mainl_bypass = true;
			return {};
		}
		if(process_start.game_mode == game_mode::story) {
			auto created = ClassicStorySession::create_replay_interstitial(
				{
					.resident = process_start,
					.game_options = {
						.input_protocol = InputProtocol::recorded_replay,
						.three_digit_combos = true,
					},
					.kotohime_compatibility = kotohime_compatibility,
				},
				loader, static_content, stage_text, staff_compiled, score_path,
				legacy_score_path, std::move(process), std::move(driver)
			);
			if(!created) {
				auto error = playback_error(
					NewClassicPlaybackErrorCode::interstitial_failed
				);
				error.interstitial_detail = static_cast<std::uint8_t>(
					created.error().code
				);
				return std::unexpected(std::move(error));
			}
			story = std::move(*created);
			return {};
		}
		if(process_start.game_mode < game_mode::vs_1p_cpu ||
			process_start.game_mode > game_mode::vs_cpu_cpu ||
			terminal.route != 2U || terminal.winner < 0 || terminal.winner > 1) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		const auto winner = static_cast<std::size_t>(terminal.winner);
		const auto loser = 1U - winner;
		const auto winner_selection = main_player_selection_from_paletted(
			process_start.paletted_character[winner]
		);
		const auto loser_selection = main_player_selection_from_paletted(
			process_start.paletted_character[loser]
		);
		if(!winner_selection || !loser_selection) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		auto resources = loader->load_mainl_win(*winner_selection);
		if(!resources) {
			auto error = playback_error(NewClassicPlaybackErrorCode::resource_failed);
			error.resource = std::move(resources.error());
			return std::unexpected(std::move(error));
		}
		auto created = ClassicMainlWinScene::create(
			{
				.winner_paletted_character =
					process_start.paletted_character[winner],
				.loser_paletted_character =
					process_start.paletted_character[loser],
				.message_block = static_cast<std::uint8_t>(
					(static_cast<std::uint8_t>(
						process_start.paletted_character[loser]
					) - 1U) / 2U
				),
				.preload_stage_splash = false,
			},
			*resources, static_content->font()
		);
		if(!created) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::interstitial_failed
			);
			error.interstitial_detail = static_cast<std::uint8_t>(
				created.error().code
			);
			return std::unexpected(std::move(error));
		}
		vs_win = std::move(*created);
		return {};
	}

	[[nodiscard]] bool process_start_checkpoint(std::size_t checkpoint_index)
		const noexcept
	{
		if(checkpoint_index >= document.checkpoints.size()) {
			return false;
		}
		const auto sample = document.checkpoints[checkpoint_index].sample_index;
		if(sample == 0U) {
			return true;
		}
		return std::any_of(
			document.session.records.begin(), document.session.records.end(),
			[sample](const replay::SessionRecord &record) {
				const auto *boundary = std::get_if<replay::ProcessBoundary>(&record);
				return boundary && boundary->sample_index == sample &&
					boundary->completed_process == replay::ProcessId::mainl;
			}
		);
	}

	[[nodiscard]] std::optional<std::size_t> next_process_checkpoint() const
		noexcept
	{
		for(std::size_t index = active_checkpoint + 1U;
			index < document.checkpoints.size(); ++index) {
			if(document.checkpoints[index].sample_index == sample_index) {
				return index;
			}
			if(document.checkpoints[index].sample_index > sample_index) {
				break;
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] std::expected<NewClassicPlaybackTick, NewClassicPlaybackError>
	drain_terminal_mainl_bypass()
	{
		if(!in_mainl || !terminal_mainl_bypass ||
			(!story && !last_main_exit_resident)) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		while(record_index < document.session.records.size()) {
			const auto &record = document.session.records[record_index];
			if(const auto *sample = std::get_if<replay::InputSample>(&record)) {
				if(sample->frame_index != sample_index ||
					sample->phase != replay::InputPhase::interstitial) {
					return std::unexpected(playback_error(
						NewClassicPlaybackErrorCode::replay_contract_failed
					));
				}
				++record_index;
				++sample_index;
				continue;
			}
			const auto &boundary = std::get<replay::ProcessBoundary>(record);
			if(boundary.sample_index != sample_index ||
				boundary.completed_process != replay::ProcessId::mainl ||
				next_process_checkpoint()) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			auto completed_exit_resident = story ?
				std::optional<GameInit>(story->resident()) : last_main_exit_resident;
			if(!completed_exit_resident ||
				!set_exit_resident(std::move(*completed_exit_resident))) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			++record_index;
			story.reset();
			vs_win.reset();
			process.reset();
			driver.reset();
			terminal_mainl_bypass = false;
			terminal_mainl_bypass_taken = true;
			status = NewClassicPlaybackStatus::complete;
			return NewClassicPlaybackTick{.status = status};
		}
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
};

std::expected<std::unique_ptr<NewClassicReplayPlayback>, NewClassicPlaybackError>
NewClassicReplayPlayback::load(NewClassicPlaybackRequest request)
{
	return load_internal(std::move(request), true);
}

std::expected<std::unique_ptr<NewClassicReplayPlayback>, NewClassicPlaybackError>
NewClassicReplayPlayback::load_internal(
	NewClassicPlaybackRequest request,
	bool suppress_preroll_presentation
)
{
	if(request.content.data_root.empty() ||
		request.content.main_executable.empty() ||
		request.content.mainl_executable.empty() ||
		request.content.font_rom.empty() ||
		request.content.score.empty() ||
		!formats::replay_can_be_verified(request.document) ||
		request.checkpoint_index >= request.document.checkpoints.size()) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::invalid_request
		));
	}
	auto main = read_file(request.content.main_executable);
	if(!main) {
		return std::unexpected(std::move(main.error()));
	}
	auto compiled = import_main_compiled_content(*main);
	if(!compiled) {
		auto error = playback_error(
			NewClassicPlaybackErrorCode::compiled_content_failed
		);
		error.compiled_content = std::move(compiled.error());
		return std::unexpected(std::move(error));
	}
	auto font = read_file(request.content.font_rom);
	if(!font) {
		return std::unexpected(std::move(font.error()));
	}
	auto mainl = read_file(request.content.mainl_executable);
	if(!mainl) {
		return std::unexpected(std::move(mainl.error()));
	}
	auto mainl_text = import_mainl_stage_text(*mainl);
	if(!mainl_text) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::mainl_content_failed
		));
	}
	auto loader = std::make_shared<MainResourceLoader>(
		request.content.data_root
	);
	if(request.language == ClassicLanguage::english) {
		auto overlay = load_new_classic_language_overlay(
			request.content.language_overlay
		);
		if(!overlay) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::language_overlay_failed
			);
			error.language_overlay.emplace(std::move(overlay.error()));
			return std::unexpected(std::move(error));
		}
		if(!loader->install_authenticated_overrides(
			std::move(*overlay).release_members()
		)) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::language_overlay_failed
			));
		}
		OpMusicRoomTextImport unused_music{};
		apply_new_classic_english_text(
			*compiled, mainl_text->assets, unused_music
		);
	}
	auto static_resources = loader->load_main_static();
	if(!static_resources) {
		auto error = playback_error(NewClassicPlaybackErrorCode::resource_failed);
		error.resource = std::move(static_resources.error());
		return std::unexpected(std::move(error));
	}
	auto static_content = MainProcessStaticContentAccess::create(
		*static_resources, *compiled, *font
	);
	if(!static_content) {
		auto error = playback_error(
			NewClassicPlaybackErrorCode::static_content_failed
		);
		error.process = std::move(static_content.error());
		return std::unexpected(std::move(error));
	}
	if(request.content.replay_patch_assets.empty()) {
		request.content.replay_patch_assets =
			request.content.data_root / "AZINN.DAT";
	}
	auto loaded_patch_assets = load_new_classic_replay_patch_assets(
		request.content.replay_patch_assets
	);
	if(!loaded_patch_assets) {
		auto error = playback_error(
			NewClassicPlaybackErrorCode::replay_patch_assets_failed
		);
		error.replay_patch_assets.emplace(
			std::move(loaded_patch_assets.error())
		);
		return std::unexpected(std::move(error));
	}
	auto patch_assets = std::make_shared<const NewClassicReplayPatchAssets>(
		std::move(*loaded_patch_assets)
	);
	const auto legacy_score_path = request.content.score;
	const auto score_path = legacy_score_path.parent_path() / "YUME.NEW.NEM";
	auto impl = std::make_unique<Impl>(
		std::move(request.document), std::move(loader),
		std::move(*compiled), std::move(*font), std::move(*static_content),
		std::move(mainl_text->assets), std::move(mainl_text->staff),
		score_path, legacy_score_path, std::move(patch_assets),
		request.kotohime_compatibility,
		request.reduced_flash
	);
	const auto target_sample =
		impl->document.checkpoints[request.checkpoint_index].sample_index;
	std::size_t start_checkpoint = request.checkpoint_index;
	while(start_checkpoint != 0U &&
		!impl->process_start_checkpoint(start_checkpoint)) {
		--start_checkpoint;
	}
	if(!impl->process_start_checkpoint(start_checkpoint)) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	const auto sample_index =
		impl->document.checkpoints[start_checkpoint].sample_index;
	const auto found = std::find_if(
		impl->document.session.records.begin(),
		impl->document.session.records.end(),
		[sample_index](const replay::SessionRecord &record) {
			const auto *sample = std::get_if<replay::InputSample>(&record);
			return sample && sample->frame_index == sample_index;
		}
	);
	if(found == impl->document.session.records.end()) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	impl->record_index = static_cast<std::size_t>(std::distance(
		impl->document.session.records.begin(), found
	));
	if(auto started = impl->start_checkpoint(start_checkpoint); !started) {
		return std::unexpected(std::move(started.error()));
	}
	auto result = std::unique_ptr<NewClassicReplayPlayback>(
		new NewClassicReplayPlayback(std::move(impl))
	);
	result->impl_->suppress_preroll_presentation =
		suppress_preroll_presentation;
	result->impl_->preroll_simulating = true;
	if(suppress_preroll_presentation) {
		MainProcessReplayPresentationAccess::set_preroll_simulating(
			*result->impl_->process, true
		);
	}
	while(result->sample_index() < target_sample &&
		result->status() == NewClassicPlaybackStatus::running) {
		const auto sample_before = result->sample_index();
		auto advanced = result->advance({}, false, false);
		if(!advanced) {
			return std::unexpected(std::move(advanced.error()));
		}
		while(result->take_audio_command()) {
		}
		result->impl_->preroll_sample_count +=
			result->sample_index() - sample_before;
	}
	result->impl_->preroll_simulating = false;
	if(result->impl_->process && suppress_preroll_presentation) {
		MainProcessReplayPresentationAccess::set_preroll_simulating(
			*result->impl_->process, false
		);
	}
	if(result->sample_index() != target_sample ||
		result->status() != NewClassicPlaybackStatus::running) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	// Synchronous hidden preroll has no live audio owner, so every command
	// emitted while reconstructing the selected source checkpoint was consumed
	// above. Re-arm the same gameplay load/reset/play transaction that starts a
	// visible MAIN process; otherwise the retained OP song (or silence) would
	// continue when a later checkpoint becomes public.
	if(target_sample != sample_index &&
		!result->impl_->rearm_gameplay_audio_after_preroll()) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	// Hidden checkpoint preroll owns no Replay Patch overlay. Install the
	// native MAIN backing/TRAM only once the selected frame becomes public.
	if(!result->impl_->put_replay_overlay()) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::surface_failed
		));
	}
	// OP always launches replay playback through MAINL. Even checkpoint zero
	// first shows the selected character/stage splash; the replay cursor begins
	// only after its mandatory 32-VSync hold and complete white transition.
	if(auto started = result->impl_->start_replay_entry_stage(
		request.checkpoint_index
	); !started) {
		return std::unexpected(std::move(started.error()));
	}
	return result;
}

NewClassicReplayPlayback::NewClassicReplayPlayback(
	std::unique_ptr<Impl> impl
) noexcept : impl_(std::move(impl)) {}

NewClassicReplayPlayback::~NewClassicReplayPlayback() = default;

std::expected<NewClassicPlaybackTick, NewClassicPlaybackError>
NewClassicReplayPlayback::advance(
	ClassicAudioStatus audio,
	bool abort_requested,
	bool application_quit_requested,
	CoreInput live_interface_input
)
{
	if(impl_->status != NewClassicPlaybackStatus::running) {
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(application_quit_requested) {
		impl_->status = NewClassicPlaybackStatus::application_quit;
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(impl_->replay_entry_stage) {
		if(abort_requested) {
			if(!impl_->capture_abort_resident() || !impl_->queue_abort_stop()) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			impl_->replay_entry_stage.reset();
			impl_->status = NewClassicPlaybackStatus::aborted;
			return NewClassicPlaybackTick{.status = impl_->status};
		}
		const bool mandatory_hold_complete =
			impl_->replay_entry_stage->phase() ==
				ClassicMainlStagePhase::waiting &&
			impl_->replay_entry_stage->wait_count() >= 32U;
		auto advanced = impl_->replay_entry_stage->advance(
			mandatory_hold_complete
		);
		if(!advanced) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::interstitial_failed
			);
			error.interstitial_detail = static_cast<std::uint8_t>(
				advanced.error().code
			);
			return std::unexpected(std::move(error));
		}
		if(advanced->handoff_ready) {
			impl_->replay_entry_stage.reset();
		}
		return NewClassicPlaybackTick{
			.surface_changed = true,
			.status = impl_->status,
		};
	}
	if(abort_requested && impl_->in_mainl &&
		(impl_->story || impl_->vs_win)) {
		// MAINL's physical-Esc path enters MR_ERROR and forces INPUT_OK at
		// every subsequent replay-aware interface poll. It does not teleport
		// out of the current owner. Keep the owner visible while that exact
		// forced-OK progression reaches its normal process exit.
		impl_->mainl_error_progression = true;
	}
	if(abort_requested && !impl_->mainl_error_progression) {
		if(!impl_->in_mainl && impl_->driver &&
			impl_->driver->input_context() == InputContext::gameplay) {
			// The Replay Patch detects physical Esc inside replay_frame_io(),
			// clears the interface input, and still completes the current MAIN
			// suffix through round_frame++.  Do the same neutral source frame
			// before projecting the fresh-OP resident; the replay sample itself
			// is intentionally not consumed.
			auto suffix_input = live_interface_input;
			suffix_input.system = 0U;
			// replay_frame_io() returns before Replay Autofire on physical Esc.
			// Pin Charge to the live Shot bits so the neutral adapter preserves
			// those bits exactly while still accepting the live P1/P2 movement.
			suffix_input.charge_mask = static_cast<std::uint8_t>(
				((suffix_input.p1 & input_shot) != 0U ? 0x01U : 0U) |
				((suffix_input.p2 & input_shot) != 0U ? 0x02U : 0U)
			);
			auto advanced = impl_->driver->consume(suffix_input);
			if(!advanced || impl_->driver->deferred_frame_pending()) {
				auto error = playback_error(
					NewClassicPlaybackErrorCode::driver_failed
				);
				if(!advanced && advanced.error().process_error) {
					error.process = std::move(*advanced.error().process_error);
				}
				return std::unexpected(std::move(error));
			}
		}
		if(!impl_->capture_abort_resident() || !impl_->queue_abort_stop()) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		impl_->status = NewClassicPlaybackStatus::aborted;
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(impl_->mainl_error_progression) {
		NewClassicPlaybackTick tick{.status = impl_->status};
		if(impl_->story && impl_->story->status() ==
			ClassicStorySessionStatus::running) {
			auto advanced = impl_->story->consume(playback_story_input(
				input_ok, audio
			));
			if(!advanced) {
				auto error = playback_error(
					NewClassicPlaybackErrorCode::interstitial_failed
				);
				error.interstitial_detail =
					ClassicStorySessionTestAccess::failure_detail_code(
						*impl_->story
					).value_or(static_cast<std::uint8_t>(advanced.error().code));
				return std::unexpected(std::move(error));
			}
			tick.surface_changed = advanced->surface_changed;
			tick.process_advanced = advanced->process_advanced;
		}
		if(impl_->vs_win &&
			impl_->vs_win->phase() != ClassicMainlWinPhase::ready) {
			auto advanced = impl_->vs_win->advance(audio, true);
			if(!advanced) {
				auto error = playback_error(
					NewClassicPlaybackErrorCode::interstitial_failed
				);
				error.interstitial_detail = static_cast<std::uint8_t>(
					advanced.error().code
				);
				return std::unexpected(std::move(error));
			}
			tick.surface_changed = tick.surface_changed ||
				advanced->surface_changed;
		}
		const bool ready = (impl_->story && impl_->story->status() ==
			ClassicStorySessionStatus::complete) ||
			(impl_->vs_win &&
			 impl_->vs_win->phase() == ClassicMainlWinPhase::ready);
		if(!ready) {
			return tick;
		}
		auto resident = impl_->story ?
			std::optional<GameInit>(impl_->story->resident()) :
			impl_->last_main_exit_resident;
		if(!resident || !impl_->set_exit_resident(std::move(*resident)) ||
			!impl_->queue_abort_stop()) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		impl_->mainl_error_progression = false;
		impl_->status = NewClassicPlaybackStatus::aborted;
		tick.status = impl_->status;
		return tick;
	}
	if(impl_->record_index >= impl_->document.session.records.size()) {
		impl_->status = NewClassicPlaybackStatus::complete;
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(impl_->pause_menu) {
		// Replay Patch 0.4.6-rc8 removed the VSync wait from replay-owned
		// Pause samples. Consume the complete recorded menu transaction in
		// order at this one host boundary; physical Esc/application quit were
		// already honored above, and PMD remains paused until the final action.
		NewClassicPlaybackTick result{
			.surface_changed = false,
			.process_advanced = false,
			.status = impl_->status,
		};
		for(;;) {
			if(impl_->record_index >= impl_->document.session.records.size()) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			const auto *sample = std::get_if<replay::InputSample>(
				&impl_->document.session.records[impl_->record_index]
			);
			if(!sample || sample->frame_index != impl_->sample_index ||
				impl_->in_mainl ||
				sample->phase != replay::InputPhase::interstitial ||
				impl_->driver->input_context() != InputContext::pause) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			const auto pause_status = impl_->pause_menu->consume(
				project_new_classic_pause_input(sample->input)
			);
			if(impl_->pause_menu->take_rejected_save_effect()) {
				impl_->ui_audio = {{
					{.type = ClassicAudioCommandType::play_effect, .parameter = 21U},
					{},
				}};
				impl_->ui_audio_index = 0U;
				impl_->ui_audio_count = 1U;
			}
			result.surface_changed = true;
			result.process_advanced = true;
			if(pause_status == NewClassicPauseStatus::running) {
				++impl_->record_index;
				++impl_->sample_index;
				continue;
			}
			const bool resume = pause_status == NewClassicPauseStatus::resume;
			if(!resume && pause_status != NewClassicPauseStatus::save_exit) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			auto advanced = impl_->driver->finish_deferred_frame(
				resume ? DeferredFrameAction::resume :
					DeferredFrameAction::exit_session
			);
			impl_->pause_menu.reset();
			if(resume) {
				impl_->ui_audio = {{
					{.type = ClassicAudioCommandType::resume_song},
					{.type = ClassicAudioCommandType::play_effect, .parameter = 21U},
				}};
				impl_->ui_audio_index = 0U;
				impl_->ui_audio_count = impl_->ui_audio.size();
			}
			if(!advanced) {
				auto error = playback_error(
					NewClassicPlaybackErrorCode::driver_failed
				);
				if(advanced.error().process_error) {
					error.process = std::move(*advanced.error().process_error);
				}
				return std::unexpected(std::move(error));
			}
			if(advanced->process_advanced) {
				++impl_->record_index;
				++impl_->sample_index;
				if(!impl_->put_replay_overlay()) {
					return std::unexpected(playback_error(
						NewClassicPlaybackErrorCode::surface_failed
					));
				}
			}
			result.surface_changed = result.surface_changed ||
				advanced->surface_changed;
			result.process_advanced = result.process_advanced ||
				advanced->process_advanced;
			return result;
		}
	}

	const auto &record = impl_->document.session.records[impl_->record_index];
	if(const auto *sample = std::get_if<replay::InputSample>(&record)) {
		if(sample->frame_index != impl_->sample_index) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		if(impl_->in_mainl) {
			if(sample->phase != replay::InputPhase::interstitial) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			if(impl_->terminal_mainl_bypass) {
				return impl_->drain_terminal_mainl_bypass();
			}
			if(impl_->story) {
				if(impl_->story->scene() ==
					ClassicStorySessionScene::high_score_registration) {
					// Replay Patch leaves ordinary regist_menu() on the live
					// input_mode_interface() path.  It therefore contributes no
					// replay samples; any samples left before MAINL_END belong to
					// the preceding replay-aware owner and are only transition
					// padding from the native owner's lower rendering cost.
					impl_->mainl_replay_input = sample->input;
					++impl_->record_index;
					++impl_->sample_index;
					return NewClassicPlaybackTick{.status = impl_->status};
				}
				if(impl_->story->status() ==
					ClassicStorySessionStatus::complete) {
					// Replay Patch's MAINL transition finisher drains any
					// remaining VSync-sampled input up to MAINL_END.  Native
					// raster work is not tied to PC-98 rendering cost, so a
					// typed scene can legitimately finish before that marker.
					impl_->mainl_replay_input = sample->input;
					++impl_->record_index;
					++impl_->sample_index;
					return NewClassicPlaybackTick{.status = impl_->status};
				}
				const auto source = sample->input.system;
				auto input = playback_story_input(source, audio);
				input.main.aggregate = sample->input;
				auto advanced = impl_->story->consume(input);
				if(!advanced) {
					auto error = playback_error(
						NewClassicPlaybackErrorCode::interstitial_failed
					);
					error.interstitial_detail =
						ClassicStorySessionTestAccess::failure_detail_code(
							*impl_->story
						).value_or(static_cast<std::uint8_t>(advanced.error().code));
					return std::unexpected(std::move(error));
				}
				if(advanced->input_sampled) {
					impl_->mainl_replay_input = sample->input;
					++impl_->record_index;
					++impl_->sample_index;
				}
				return NewClassicPlaybackTick{
					.surface_changed = advanced->surface_changed,
					.process_advanced = advanced->process_advanced,
					.status = impl_->status,
				};
			}
			if(impl_->vs_win) {
				if(impl_->vs_win->phase() == ClassicMainlWinPhase::ready) {
					impl_->mainl_replay_input = sample->input;
					++impl_->record_index;
					++impl_->sample_index;
					return NewClassicPlaybackTick{.status = impl_->status};
				}
				auto advanced = impl_->vs_win->advance(
					audio, sample->input.system != 0U
				);
				if(!advanced) {
					auto error = playback_error(
						NewClassicPlaybackErrorCode::interstitial_failed
					);
					error.interstitial_detail = static_cast<std::uint8_t>(
						advanced.error().code
					);
					return std::unexpected(std::move(error));
				}
				if(advanced->input_sampled) {
					impl_->mainl_replay_input = sample->input;
					++impl_->record_index;
					++impl_->sample_index;
				}
				return NewClassicPlaybackTick{
					.surface_changed = advanced->surface_changed,
					.status = impl_->status,
				};
			}
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		const auto context = impl_->driver->input_context();
		const auto expected_phase = context == InputContext::gameplay ?
			replay::InputPhase::gameplay : replay::InputPhase::interstitial;
		if(sample->phase != expected_phase) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		std::expected<MainProcessDriverTick, MainProcessDriverError> advanced =
			std::unexpected(MainProcessDriverError{});
		advanced = impl_->driver->consume(sample->input);
		if(advanced && impl_->driver->deferred_frame_pending()) {
			impl_->pause_menu.emplace(true, impl_->patch_assets);
			impl_->ui_audio = {{
				{.type = ClassicAudioCommandType::pause_song},
				{.type = ClassicAudioCommandType::play_effect, .parameter = 21U},
			}};
			impl_->ui_audio_index = 0U;
			impl_->ui_audio_count = impl_->ui_audio.size();
		}
		if(!advanced) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::driver_failed
			);
			if(advanced.error().process_error) {
				error.process = std::move(*advanced.error().process_error);
			}
			return std::unexpected(std::move(error));
		}
		if(advanced->process_advanced) {
			++impl_->record_index;
			++impl_->sample_index;
			if(!impl_->put_replay_overlay()) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::surface_failed
				));
			}
		}
		return NewClassicPlaybackTick{
			.surface_changed = advanced->surface_changed,
			.process_advanced = advanced->process_advanced,
			.status = impl_->status,
		};
	}

	const auto &boundary = std::get<replay::ProcessBoundary>(record);
	if(boundary.sample_index != impl_->sample_index) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	const bool registration_terminal_ready = impl_->in_mainl && impl_->story &&
		(impl_->story->scene() == ClassicStorySessionScene::continue_menu ||
		 impl_->story->scene() == ClassicStorySessionScene::game_over);
	if(impl_->in_mainl && impl_->story && !registration_terminal_ready &&
		impl_->story->status() ==
			ClassicStorySessionStatus::running) {
		// MAINL_END closes the deterministic replay stream without disabling its
		// replay-aware interface. Those owners keep receiving the last decoded
		// sample until MR_DISABLED; only regist_menu() remains on live input.
		const bool registration = impl_->story->scene() ==
			ClassicStorySessionScene::high_score_registration;
		const bool selected_stage_splash = impl_->story->scene() ==
			ClassicStorySessionScene::stage_splash &&
			impl_->next_process_checkpoint().has_value();
		const auto source = mainl_end_input(
			impl_->mainl_replay_input, live_interface_input, registration
		);
		auto input = playback_story_input(source.system, audio);
		input.main.aggregate = source;
		auto advanced = impl_->story->consume(input);
		if(!advanced) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::interstitial_failed
			);
			error.interstitial_detail =
				ClassicStorySessionTestAccess::failure_detail_code(
					*impl_->story
				).value_or(static_cast<std::uint8_t>(advanced.error().code));
			return std::unexpected(std::move(error));
		}
		// The final recorded interstitial input dismisses the character screen.
		// Count only the following no-input visual drain so the regression owns
		// the exact 18 white-out + 18 white-in publication boundaries.
		if(selected_stage_splash && !advanced->input_sampled) {
			++impl_->stage_splash_end_drain_ticks;
		}
		return NewClassicPlaybackTick{
			.surface_changed = advanced->surface_changed,
			.process_advanced = advanced->process_advanced,
			.status = impl_->status,
		};
	}
	if(impl_->in_mainl && impl_->vs_win &&
		impl_->vs_win->phase() != ClassicMainlWinPhase::ready) {
		const auto source = mainl_end_input(
			impl_->mainl_replay_input, live_interface_input, false
		);
		auto advanced = impl_->vs_win->advance(
			audio, source.system != 0U
		);
		if(!advanced) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::interstitial_failed
			);
			error.interstitial_detail = static_cast<std::uint8_t>(
				advanced.error().code
			);
			return std::unexpected(std::move(error));
		}
		if(advanced->input_sampled) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		return NewClassicPlaybackTick{
			.surface_changed = advanced->surface_changed,
			.status = impl_->status,
		};
	}
	if(!impl_->in_mainl) {
		if(boundary.completed_process != replay::ProcessId::main ||
			impl_->driver->status() != MainProcessDriverStatus::terminal) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		if(!impl_->driver->transition()) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		impl_->last_main_exit_resident =
			impl_->driver->transition()->resident;
		impl_->last_main_exit_resident->game_mode = game_mode::none;
		// MAINL initializes its replay RLE input globals to INPUT_NONE rather
		// than carrying the final MAIN sample across the executable boundary.
		impl_->mainl_replay_input = {};
		impl_->in_mainl = true;
		++impl_->record_index;
		if(impl_->record_index >= impl_->document.session.records.size()) {
			if(!impl_->set_exit_resident(*impl_->last_main_exit_resident)) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			impl_->status = NewClassicPlaybackStatus::complete;
		} else if(auto started = impl_->start_mainl_owner(); !started) {
			return std::unexpected(std::move(started.error()));
		}
		if(impl_->terminal_mainl_bypass) {
			return impl_->drain_terminal_mainl_bypass();
		}
		return NewClassicPlaybackTick{
			.surface_changed = impl_->status == NewClassicPlaybackStatus::running,
			.status = impl_->status,
		};
	}
	if(boundary.completed_process != replay::ProcessId::mainl) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	const auto next = impl_->next_process_checkpoint();
	if((impl_->story && impl_->story->status() !=
			ClassicStorySessionStatus::complete &&
			!registration_terminal_ready) ||
		(impl_->vs_win && impl_->vs_win->phase() !=
			ClassicMainlWinPhase::ready) ||
		(!impl_->story && !impl_->vs_win && !impl_->terminal_mainl_bypass)) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	std::optional<GameInit> completed_exit_resident{};
	if(!next) {
		if(impl_->story) {
			completed_exit_resident = impl_->story->resident();
		} else {
			completed_exit_resident = impl_->last_main_exit_resident;
		}
	}
	++impl_->record_index;
	impl_->story.reset();
	impl_->vs_win.reset();
	impl_->process.reset();
	impl_->driver.reset();
	if(!next) {
		if(!completed_exit_resident ||
			!impl_->set_exit_resident(*completed_exit_resident)) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		impl_->status = NewClassicPlaybackStatus::complete;
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(auto started = impl_->start_checkpoint(*next); !started) {
		return std::unexpected(std::move(started.error()));
	}
	if(!impl_->put_replay_overlay()) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::surface_failed
		));
	}
	return NewClassicPlaybackTick{
		.surface_changed = true,
		.status = impl_->status,
	};
}

std::optional<ClassicAudioCommand>
NewClassicReplayPlayback::take_audio_command()
{
	if(impl_->replay_entry_stage) {
		return impl_->replay_entry_stage->take_audio_command();
	}
	if(impl_->load_song) {
		auto result = impl_->load_song;
		impl_->load_song.reset();
		return result;
	}
	if(impl_->preroll_audio_count != 0U) {
		const auto result = impl_->preroll_audio[impl_->preroll_audio_index++];
		--impl_->preroll_audio_count;
		if(impl_->preroll_audio_count == 0U) {
			impl_->preroll_audio_index = 0U;
		}
		return result;
	}
	if(impl_->ui_audio_count != 0U) {
		const auto result = impl_->ui_audio[impl_->ui_audio_index++];
		--impl_->ui_audio_count;
		if(impl_->ui_audio_count == 0U) {
			impl_->ui_audio_index = 0U;
		}
		return result;
	}
	if(impl_->story) {
		if(auto command = impl_->story->take_audio_command()) {
			return command;
		}
	}
	if(impl_->process) {
		if(auto command = impl_->process->take_audio_command()) {
			return command;
		}
	}
	if(impl_->vs_win) {
		if(auto command = impl_->vs_win->take_audio_command()) {
			return command;
		}
	}
	return std::nullopt;
}

std::expected<present::SourceSurface, NewClassicPlaybackError>
NewClassicReplayPlayback::compose_shown_surface(
	std::span<present::ComposedPixel, present::graphics_page_pixels> output
) const
{
	std::expected<present::SourceSurface, NewClassicPlaybackError> surface =
		std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::surface_failed
		));
	if(impl_->replay_entry_stage) {
		auto composed = present::compose_source_surface(
			output, impl_->replay_entry_stage->raster(),
			impl_->replay_entry_stage->tram(), impl_->static_content->font()
		);
		if(!composed) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::surface_failed
			);
			error.surface = std::move(composed.error());
			return std::unexpected(std::move(error));
		}
		surface = *composed;
	} else if(impl_->story) {
		auto composed = impl_->story->compose_shown_surface(output);
		if(!composed) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::interstitial_failed
			);
			error.interstitial_detail = static_cast<std::uint8_t>(
				composed.error().code
			);
			return std::unexpected(std::move(error));
		}
		surface = *composed;
	} else if(impl_->vs_win) {
		auto composed = present::compose_source_surface(
			output, impl_->vs_win->raster(), impl_->blank_tram,
			impl_->static_content->font()
		);
		if(!composed) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::surface_failed
			);
			error.surface = std::move(composed.error());
			return std::unexpected(std::move(error));
		}
		surface = *composed;
	} else if(impl_->process) {
		auto composed = impl_->process->compose_shown_surface(output);
		if(!composed) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::surface_failed
			);
			error.surface = std::move(composed.error());
			return std::unexpected(std::move(error));
		}
		surface = *composed;
	}
	if(!surface) {
		return std::unexpected(std::move(surface.error()));
	}
	if(impl_->pause_menu) {
		return impl_->pause_menu->compose_over(*surface);
	}
	auto result = *surface;
	if(impl_->reduced_flash && result.palette_tone == 150) {
		result.graphics_palette = result.reduced_flash_palette;
		result.reduced_flash_applied = true;
	}
	return result;
}

void NewClassicReplayPlayback::set_fast_forward(bool enabled) noexcept
{
	impl_->fast_forward = enabled;
}

bool NewClassicReplayPlayback::fast_forward() const noexcept
{
	return impl_->fast_forward;
}

std::size_t NewClassicReplayPlayback::fast_forward_step_count() const noexcept
{
	if(!impl_->fast_forward || impl_->replay_entry_stage) {
		return 1U;
	}
	if(!impl_->in_mainl) {
		return 4U;
	}
	return impl_->story &&
		impl_->story->scene() == ClassicStorySessionScene::staff_verdict ? 4U : 1U;
}

NewClassicPlaybackStatus NewClassicReplayPlayback::status() const noexcept
{
	return impl_->status;
}

NewClassicPlaybackOwner NewClassicReplayPlaybackTestAccess::owner(
	const NewClassicReplayPlayback &playback
) noexcept
{
	const auto &impl = *playback.impl_;
	if(impl.replay_entry_stage) {
		return NewClassicPlaybackOwner::story_stage_splash;
	}
	if(impl.terminal_mainl_bypass) {
		return NewClassicPlaybackOwner::terminal_mainl_bypass;
	}
	if(impl.vs_win) {
		return NewClassicPlaybackOwner::vs_win;
	}
	if(impl.story) {
		switch(impl.story->scene()) {
		case ClassicStorySessionScene::stage_splash:
			return NewClassicPlaybackOwner::story_stage_splash;
		case ClassicStorySessionScene::win:
			return NewClassicPlaybackOwner::story_win;
		case ClassicStorySessionScene::cutscene:
			return NewClassicPlaybackOwner::story_cutscene;
		case ClassicStorySessionScene::staff_verdict:
			return NewClassicPlaybackOwner::story_staff_verdict;
		case ClassicStorySessionScene::high_score_registration:
			return NewClassicPlaybackOwner::story_registration;
		case ClassicStorySessionScene::continue_menu:
			return NewClassicPlaybackOwner::story_continue;
		case ClassicStorySessionScene::game_over:
			return NewClassicPlaybackOwner::story_game_over;
		case ClassicStorySessionScene::main:
			return NewClassicPlaybackOwner::main;
		case ClassicStorySessionScene::complete:
		case ClassicStorySessionScene::failed:
			return NewClassicPlaybackOwner::none;
		}
	}
	return impl.process ? NewClassicPlaybackOwner::main :
		NewClassicPlaybackOwner::none;
}

std::expected<std::unique_ptr<NewClassicReplayPlayback>, NewClassicPlaybackError>
NewClassicReplayPlaybackTestAccess::load_with_full_preroll(
	NewClassicPlaybackRequest request
)
{
	return NewClassicReplayPlayback::load_internal(
		std::move(request), false
	);
}

std::uint32_t NewClassicReplayPlaybackTestAccess::interstitial_sample_count(
	const NewClassicReplayPlayback &playback
) noexcept
{
	return playback.impl_->story ?
		playback.impl_->story->source_sample_count() : playback.impl_->sample_index;
}

std::uint8_t NewClassicReplayPlaybackTestAccess::main_input_context(
	const NewClassicReplayPlayback &playback
) noexcept
{
	return playback.impl_->driver ? static_cast<std::uint8_t>(
		playback.impl_->driver->input_context()
	) : 0xFFU;
}

std::optional<std::uint32_t>
NewClassicReplayPlaybackTestAccess::main_round_frame(
	const NewClassicReplayPlayback &playback
) noexcept
{
	return playback.impl_->process ? std::optional<std::uint32_t>(
		playback.impl_->process->canonical_state().round_frame
	) : std::nullopt;
}

std::optional<std::uint32_t>
NewClassicReplayPlaybackTestAccess::main_state_hash(
	const NewClassicReplayPlayback &playback
) noexcept
{
	return playback.impl_->process ? std::optional<std::uint32_t>(
		playback.impl_->process->canonical_state().state_hash
	) : std::nullopt;
}

bool NewClassicReplayPlaybackTestAccess::terminal_mainl_bypass_taken(
	const NewClassicReplayPlayback &playback
) noexcept
{
	return playback.impl_->terminal_mainl_bypass_taken;
}

CoreInput NewClassicReplayPlaybackTestAccess::mainl_end_input(
	CoreInput replay_input,
	CoreInput live_interface_input,
	bool registration
) noexcept
{
	return ::th03::app::mainl_end_input(
		replay_input, live_interface_input, registration
	);
}

bool NewClassicReplayPlaybackTestAccess::mainl_error_progression_active(
	const NewClassicReplayPlayback &playback
) noexcept
{
	return playback.impl_->mainl_error_progression;
}

std::uint32_t NewClassicReplayPlaybackTestAccess::preroll_sample_count(
	const NewClassicReplayPlayback &playback
) noexcept
{
	return playback.impl_->preroll_sample_count;
}

std::optional<ClassicMainlStagePhase>
NewClassicReplayPlaybackTestAccess::replay_entry_stage_phase(
	const NewClassicReplayPlayback &playback
) noexcept
{
	return playback.impl_->replay_entry_stage ?
		std::optional{playback.impl_->replay_entry_stage->phase()} :
		std::nullopt;
}

std::uint32_t
NewClassicReplayPlaybackTestAccess::stage_splash_end_drain_ticks(
	const NewClassicReplayPlayback &playback
) noexcept
{
	return playback.impl_->stage_splash_end_drain_ticks;
}

bool NewClassicReplayPlaybackTestAccess::put_main_replay_overlay(
	present::Raster &raster,
	present::Tram &tram
) noexcept
{
	return ::th03::app::put_main_replay_overlay(raster, tram);
}

std::uint32_t NewClassicReplayPlayback::sample_index() const noexcept
{
	return impl_->sample_index;
}

PixelOracleSourceIdentity
NewClassicReplayPlayback::pixel_oracle_source_identity() const noexcept
{
	return impl_->pixel_oracle_source_identity();
}

std::optional<GameInit>
NewClassicReplayPlayback::take_exit_resident() noexcept
{
	if(impl_->status != NewClassicPlaybackStatus::complete &&
		impl_->status != NewClassicPlaybackStatus::aborted) {
		return std::nullopt;
	}
	auto result = std::move(impl_->exit_resident);
	impl_->exit_resident.reset();
	return result;
}

} // namespace th03::app
