#include "classic_story_session_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "main_process_internal.hpp"
#include "main_process_static_content_internal.hpp"
#include "th03/app/new_classic_pause.hpp"
#include "th03/app/new_classic_replay.hpp"
#include "th03/present/text.hpp"

namespace th03::app {
namespace {

[[nodiscard]] ClassicStorySessionError session_error(
	ClassicStorySessionErrorCode code
) noexcept
{
	return {.code = code};
}

[[nodiscard]] ClassicStorySessionError resource_error(
	ResourceLoadError error
)
{
	auto result = session_error(
		ClassicStorySessionErrorCode::resource_load_failed
	);
	result.resource_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicStorySessionError main_error(MainProcessError error)
{
	auto result = session_error(ClassicStorySessionErrorCode::main_failed);
	result.main_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicStorySessionError stage_error(
	ClassicMainlStageError error
)
{
	auto result = session_error(ClassicStorySessionErrorCode::stage_failed);
	result.stage_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicStorySessionError win_error(ClassicMainlWinError error)
{
	auto result = session_error(ClassicStorySessionErrorCode::win_failed);
	result.win_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicStorySessionError cutscene_error(
	ClassicCutsceneError error
)
{
	auto result = session_error(
		ClassicStorySessionErrorCode::cutscene_failed
	);
	result.cutscene_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicStorySessionError staff_error(ClassicStaffError error)
{
	auto result = session_error(ClassicStorySessionErrorCode::staff_failed);
	result.staff_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicStorySessionError registration_error(
	ClassicRegistrationError error
)
{
	auto result = session_error(
		ClassicStorySessionErrorCode::registration_failed
	);
	result.registration_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicStorySessionError new_score_store_error(
	NewClassicScoreStoreError error
)
{
	auto result = session_error(
		ClassicStorySessionErrorCode::score_store_failed
	);
	result.new_score_store_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicStorySessionError continue_error(
	ClassicMainlContinueError error,
	ClassicStorySessionErrorCode code =
		ClassicStorySessionErrorCode::continue_failed
)
{
	auto result = session_error(code);
	result.continue_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicStorySessionError plan_error(ClassicMainlPlanError error)
{
	auto result = session_error(ClassicStorySessionErrorCode::plan_failed);
	result.plan_error.emplace(error);
	return result;
}

[[nodiscard]] ClassicStorySessionError replay_capture_error() noexcept
{
	return session_error(
		ClassicStorySessionErrorCode::backend_contract_failed
	);
}

[[nodiscard]] ClassicStorySessionError surface_error(
	present::SourceSurfaceError error
) noexcept
{
	auto result = session_error(ClassicStorySessionErrorCode::surface_failed);
	result.surface_error.emplace(error);
	return result;
}

[[nodiscard]] MainStoryConfig story_config(
	const MainStoryConfig &base,
	const GameInit &resident
) noexcept
{
	auto result = base;
	result.resident = resident;
	return result;
}

[[nodiscard]] CoreInput normalize_story_main_input(
	const ClassicMainInputSample &sample,
	InputContext context,
	bool new_classic = false
) noexcept
{
	auto input = sample.aggregate;
	if(!sample.physical_domains) {
		if(context != InputContext::gameplay) {
			return input;
		}
		input.p1 = static_cast<std::uint16_t>(input.p1 | input.system);
		input.p2 = 0U;
		input.charge_mask = static_cast<std::uint8_t>(
			input.charge_mask & 0x01U
		);
		return input;
	}
	const auto &physical = *sample.physical_domains;
	input.system = physical.keyboard.system;
	if(context != InputContext::gameplay) {
		return input;
	}
	// input_mode_1p_vs_cpu() merges the source-global keyboard word into P1
	// and joystick zero after sensing the multiplayer P1 word, then clears P2.
	constexpr std::uint16_t movement_mask = 0x0F0FU;
	const auto keyboard_p1 = new_classic ? static_cast<std::uint16_t>(
		physical.keyboard.p1 & ~movement_mask
	) : physical.keyboard.p1;
	input.p1 = static_cast<std::uint16_t>(
		keyboard_p1 | physical.keyboard.system |
		(physical.first_joystick_present ? physical.first_joystick : 0U)
	);
	input.p2 = 0U;
	input.charge_mask = static_cast<std::uint8_t>(
		physical.keyboard.charge_mask & 0x01U
	);
	return input;
}

[[nodiscard]] CoreInput normalize_replay_interstitial_input(
	const ClassicMainInputSample &sample
) noexcept
{
	auto input = sample.aggregate;
	if(!sample.physical_domains) {
		return input;
	}
	const auto &physical = *sample.physical_domains;
	input.system = static_cast<std::uint16_t>(
		physical.keyboard.system | physical.keyboard.p1 |
		(physical.first_joystick_present ? physical.first_joystick : 0U)
	);
	return input;
}

[[nodiscard]] present::Raster normalize_main_cutscene_seed(
	const present::Raster &raster
)
{
	auto result = raster;
	// MAINL starts as a fresh process. game_init_main() preserves PC-98 VRAM
	// across execl while restoring graph_400line(), which also selects access
	// page zero, before any interstage or ending cutscene code runs.
	static_cast<void>(result.set_raster_mode(present::RasterMode::lines_400));
	return result;
}

[[nodiscard]] std::optional<MainlStageResourceRequest> stage_request(
	const MainStoryConfig &config
) noexcept
{
	const auto player_one = main_player_selection_from_paletted(
		config.resident.paletted_character[0U]
	);
	const auto player_two = main_player_selection_from_paletted(
		config.resident.paletted_character[1U]
	);
	if(!player_one || !player_two) {
		return std::nullopt;
	}
	return MainlStageResourceRequest{
		.player_one = *player_one,
		.player_two = *player_two,
		.game_mode = config.resident.game_mode,
		.story_stage = config.resident.story_stage,
	};
}

[[nodiscard]] bool valid_backend(
	const ClassicStorySessionBackendOperations &operations
) noexcept
{
	return operations.context != nullptr && operations.start_stage != nullptr &&
		operations.advance_stage != nullptr && operations.start_main != nullptr &&
		operations.advance_main != nullptr && operations.main_terminal != nullptr &&
		operations.start_win != nullptr && operations.advance_win != nullptr &&
		operations.preload_stage != nullptr && operations.finish_win != nullptr &&
		operations.start_cutscene != nullptr &&
		operations.advance_cutscene != nullptr &&
		operations.finish_cutscene != nullptr &&
		operations.start_staff != nullptr &&
		operations.advance_staff != nullptr &&
		operations.finish_staff != nullptr &&
		operations.start_registration != nullptr &&
		operations.advance_registration != nullptr &&
		operations.finish_registration != nullptr &&
		operations.start_continue != nullptr &&
		operations.advance_continue != nullptr &&
		operations.finish_continue != nullptr &&
		operations.resolve_continue != nullptr &&
		operations.start_game_over != nullptr &&
		operations.advance_game_over != nullptr &&
		operations.finish_game_over != nullptr &&
		operations.take_audio_command != nullptr &&
		operations.compose_shown_surface != nullptr;
}

[[nodiscard]] ClassicStorySessionScene session_scene(
	ClassicMainlScene scene
) noexcept
{
	switch(scene) {
	case ClassicMainlScene::stage_splash:
		return ClassicStorySessionScene::stage_splash;
	case ClassicMainlScene::start_main:
		return ClassicStorySessionScene::main;
	case ClassicMainlScene::win:
		return ClassicStorySessionScene::win;
	case ClassicMainlScene::interstage_cutscene:
	case ClassicMainlScene::ending:
	case ClassicMainlScene::extra_ending:
		return ClassicStorySessionScene::cutscene;
	case ClassicMainlScene::staff_verdict:
		return ClassicStorySessionScene::staff_verdict;
	case ClassicMainlScene::high_score_registration:
		return ClassicStorySessionScene::high_score_registration;
	case ClassicMainlScene::continue_menu:
		return ClassicStorySessionScene::continue_menu;
	case ClassicMainlScene::game_over:
		return ClassicStorySessionScene::game_over;
	case ClassicMainlScene::exit_to_operator:
		return ClassicStorySessionScene::complete;
	}
	return ClassicStorySessionScene::failed;
}

[[nodiscard]] std::optional<MainlCutsceneResourceRequest> cutscene_request(
	const ClassicMainlStep &step,
	const GameInit &resident
) noexcept
{
	const auto player = main_player_selection_from_paletted(
		resident.paletted_character[0U]
	);
	if(!player) {
		return std::nullopt;
	}
	if(step.scene == ClassicMainlScene::interstage_cutscene) {
		if(step.cutscene == ClassicInterstageCutscene::chiyuri) {
			return MainlCutsceneResourceRequest{
				.kind = MainlCutsceneKind::interstage_chiyuri,
				.character = player->character,
			};
		}
		if(step.cutscene == ClassicInterstageCutscene::yumemi) {
			return MainlCutsceneResourceRequest{
				.kind = MainlCutsceneKind::interstage_yumemi,
				.character = player->character,
			};
		}
		return std::nullopt;
	}
	if(step.scene == ClassicMainlScene::ending &&
		step.cutscene == ClassicInterstageCutscene::none) {
		return MainlCutsceneResourceRequest{
			.kind = MainlCutsceneKind::ending,
			.character = player->character,
		};
	}
	if(step.scene == ClassicMainlScene::extra_ending &&
		step.cutscene == ClassicInterstageCutscene::none) {
		return MainlCutsceneResourceRequest{
			.kind = MainlCutsceneKind::extra_ending,
			.character = player->character,
		};
	}
	return std::nullopt;
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

} // namespace

struct ClassicStoryBackend final {
	std::shared_ptr<MainResourceLoader> loader{};
	std::shared_ptr<const MainProcessStaticContent> static_content{};
	MainlStageTextAssets stage_text{};
	MainlStaffCompiledAssets staff_compiled{};
	present::Tram blank_tram{};
	std::unique_ptr<ClassicMainlStageTransition> stage{};
	std::unique_ptr<ClassicMainlStageTransition> pending_stage{};
	std::unique_ptr<MainProcess> process{};
	std::unique_ptr<MainProcessDriver> driver{};
	std::unique_ptr<ClassicMainlWinScene> win{};
	std::unique_ptr<ClassicCutsceneScene> cutscene{};
	std::unique_ptr<ClassicStaffScene> staff{};
	std::unique_ptr<ClassicRegistrationScene> registration{};
	std::optional<ClassicRegistrationCompletion> registration_completion{};
	std::optional<present::Raster> cutscene_handoff{};
	std::unique_ptr<ClassicMainlContinueScene> continue_scene{};
	std::optional<ClassicMainlContinueCompletion> continue_completion{};
	std::unique_ptr<ClassicMainlGameOverScene> game_over{};
	std::optional<ClassicMainlGameOverCompletion> game_over_completion{};
	std::filesystem::path score_path{};
	std::filesystem::path legacy_score_path{};
	GameInit process_start{};
	std::optional<ClassicStoryTerminal> forced_terminal{};
	std::optional<ClassicStoryTerminal> forced_continue_terminal{};
	std::optional<NewClassicPauseMenu> pause_menu{};
	std::array<ClassicAudioCommand, 2U> pause_audio{};
	std::size_t pause_audio_index = 0U;
	std::size_t pause_audio_count = 0U;
	bool restart_requested = false;
	NewClassicReplayRecorder *replay_recorder = nullptr;
	bool new_classic = false;
	bool replay_playback = false;
	bool replay_capture_closed = false;
	bool cutscene_replay_input_due = true;
	std::optional<NewClassicScoreRun> score_run{};
	std::uint8_t score_run_rank = 0U;
	std::uint8_t score_run_character = 0U;

	ClassicStoryBackend(
		std::shared_ptr<MainResourceLoader> loader_,
		std::shared_ptr<const MainProcessStaticContent> static_content_,
		MainlStageTextAssets stage_text_,
		MainlStaffCompiledAssets staff_compiled_,
		std::filesystem::path score_path_,
		NewClassicReplayRecorder *replay_recorder_,
		std::filesystem::path legacy_score_path_,
		bool new_classic_ = false,
		bool replay_playback_ = false
	) :
		loader(std::move(loader_)),
		static_content(std::move(static_content_)),
		stage_text(std::move(stage_text_)),
		staff_compiled(std::move(staff_compiled_)),
		score_path(std::move(score_path_)),
		legacy_score_path(std::move(legacy_score_path_)),
		replay_recorder(replay_recorder_),
		new_classic(new_classic_),
		replay_playback(replay_playback_)
	{
		blank_tram.fill({
			' ', static_cast<present::TramAttribute>(present::tram_black)
		});
	}

	std::expected<void, ClassicStorySessionError> adopt_replay_terminal(
		const GameInit &start,
		std::unique_ptr<MainProcess> process_,
		std::unique_ptr<MainProcessDriver> driver_
	)
	{
		if(!process_ || !driver_ || process || driver || stage || win || cutscene ||
			staff || registration || continue_scene || game_over ||
			driver_->status() != MainProcessDriverStatus::terminal ||
			!driver_->transition()) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		process_start = start;
		process = std::move(process_);
		driver = std::move(driver_);
		return {};
	}

	std::expected<void, ClassicStorySessionError> begin_score_run(
		const GameInit &resident
	)
	{
		if(!new_classic || replay_playback || score_run) {
			return {};
		}
		const auto player = main_player_selection_from_paletted(
			resident.paletted_character[0U]
		);
		if(!player || resident.rank >= new_classic_score_rank_count) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::invalid_config
			));
		}
		auto loaded = load_new_classic_score_store(score_path, legacy_score_path);
		if(!loaded) {
			return std::unexpected(new_score_store_error(
				std::move(loaded.error())
			));
		}
		auto run_id = loaded->document.last_run_id + 1U;
		if(run_id == 0U) {
			run_id = 1U;
		}
		score_run = NewClassicScoreRun{.run_id = run_id};
		score_run_rank = resident.rank;
		score_run_character = static_cast<std::uint8_t>(player->character);
		return {};
	}

	std::expected<void, ClassicStorySessionError> continue_score_run(
		const GameInit &resident
	)
	{
		if(!new_classic || replay_playback) {
			return {};
		}
		score_run.reset();
		if(auto begun = begin_score_run(resident); !begun) {
			return begun;
		}
		if(!score_run) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		score_run->continues_used = 1U;
		return {};
	}

	std::expected<void, ClassicStorySessionError> checkpoint_score_run()
	{
		if(!score_run) {
			return {};
		}
		auto loaded = load_new_classic_score_store(score_path, legacy_score_path);
		if(!loaded) {
			return std::unexpected(new_score_store_error(
				std::move(loaded.error())
			));
		}
		auto applied = new_classic_score_apply_run(
			loaded->document, score_run_rank, score_run_character, *score_run
		);
		if(!applied) {
			return std::unexpected(registration_error({
				.code = ClassicRegistrationErrorCode::score_mutation_failed,
				.new_score_error = applied.error(),
			}));
		}
		if(*applied) {
			if(auto committed = commit_new_classic_score_store(
				score_path, loaded->document
			); !committed) {
				return std::unexpected(new_score_store_error(
					std::move(committed.error())
				));
			}
		}
		score_run.reset();
		return {};
	}

	[[nodiscard]] std::expected<
		std::unique_ptr<ClassicMainlStageTransition>,
		ClassicStorySessionError
	> create_stage(const MainStoryConfig &config)
	{
		const auto request = stage_request(config);
		if(!request) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::invalid_config
			));
		}
		auto resources = loader->load_mainl_stage(*request);
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = ClassicMainlStageTransition::create(
			config, *resources, stage_text, static_content->font()
		);
		if(!created) {
			return std::unexpected(stage_error(std::move(created.error())));
		}
		return std::move(*created);
	}

	std::expected<void, ClassicStorySessionError> start_stage(
		const MainStoryConfig &config
	)
	{
		if(process || driver) {
			const bool releasable = process && driver &&
				driver->status() == MainProcessDriverStatus::terminal &&
				driver->transition() && driver->transition()->action !=
					SessionAction::request_continue;
			if(!releasable) {
				return std::unexpected(session_error(
					ClassicStorySessionErrorCode::backend_contract_failed
				));
			}
			driver.reset();
			process.reset();
		}
		if(stage || win || cutscene || staff || registration || continue_scene ||
			game_over) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		if(pending_stage) {
			if(cutscene_handoff) {
				return std::unexpected(session_error(
					ClassicStorySessionErrorCode::backend_contract_failed
				));
			}
			if(!pending_stage->matches_process_config(config)) {
				return std::unexpected(session_error(
					ClassicStorySessionErrorCode::backend_contract_failed
				));
			}
			stage = std::move(pending_stage);
			return {};
		}
		auto created = create_stage(config);
		if(!created) {
			return std::unexpected(std::move(created.error()));
		}
		if(cutscene_handoff) {
			auto candidate = *cutscene_handoff;
			if(!(*created)->preload_splash_into(candidate) ||
				!(*created)->adopt_preloaded_raster(
					std::move(candidate)
				)) {
				return std::unexpected(session_error(
					ClassicStorySessionErrorCode::stage_failed
				));
			}
			cutscene_handoff.reset();
		}
		stage = std::move(*created);
		return {};
	}

	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	advance_stage(bool dismiss)
	{
		if(!stage) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		const bool sampled = stage->phase() ==
			ClassicMainlStagePhase::waiting &&
			(!new_classic || stage->wait_count() >= 32U);
		auto advanced = stage->advance(dismiss);
		if(!advanced) {
			return std::unexpected(stage_error(std::move(advanced.error())));
		}
		return ClassicStoryBackendTick{
			.surface_changed = true,
			.input_sampled = sampled,
			.ready = advanced->handoff_ready,
		};
	}

	std::expected<void, ClassicStorySessionError> start_main(
		const MainStoryConfig &config
	)
	{
		if(!stage || process || driver) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		const auto player_one = main_player_selection_from_paletted(
			config.resident.paletted_character[0U]
		);
		const auto player_two = main_player_selection_from_paletted(
			config.resident.paletted_character[1U]
		);
		if(!player_one || !player_two) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::invalid_config
			));
		}
		auto resources = loader->load_main_private(*player_one, *player_two);
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = MainProcessStoryAccess::create_story(
			config, *stage, *resources, static_content
		);
		if(!created) {
			return std::unexpected(main_error(std::move(created.error())));
		}
		process = std::move(*created);
		stage.reset();
		process_start = config.resident;
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
			process.reset();
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		driver = std::move(*created_driver);
		if(replay_recorder && !replay_capture_closed) {
			if(replay_recorder->recording()) {
				if(auto boundary = replay_recorder->process_completed(
					replay::ProcessId::mainl
				); !boundary) {
					return std::unexpected(replay_capture_error());
				}
			}
			if(auto checkpoint = replay_recorder->round_started(
				process->round_start_observation()
			); !checkpoint) {
				return std::unexpected(replay_capture_error());
			}
			if(auto run = begin_score_run(config.resident); !run) {
				return std::unexpected(std::move(run.error()));
			}
		}
		return {};
	}

	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	advance_main(const ClassicMainInputSample &input)
	{
		if(replay_recorder && replay_recorder->recording()) {
			replay_recorder->source_vsync();
		}
		if(forced_terminal) {
			return ClassicStoryBackendTick{
				.surface_changed = true,
				.ready = true,
			};
		}
		if(!driver || !process) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		const auto input_context = driver->input_context();
		const auto normalized = normalize_story_main_input(
			input, input_context, replay_recorder != nullptr
		);
		const bool source_frame_io = !pause_menu;
		std::expected<MainProcessDriverTick, MainProcessDriverError> advanced =
			std::unexpected(MainProcessDriverError{});
		if(pause_menu) {
			const auto status = pause_menu->consume(
				project_new_classic_pause_input(
					normalize_replay_interstitial_input(input),
					input.restart_requested
				)
			);
			if(pause_menu->take_rejected_save_effect()) {
				pause_audio = {{
					{.type = ClassicAudioCommandType::play_effect, .parameter = 21U},
					{},
				}};
				pause_audio_index = 0U;
				pause_audio_count = 1U;
			}
			if(status == NewClassicPauseStatus::running) {
				if(replay_recorder && replay_recorder->recording()) {
					if(auto sampled = replay_recorder->sample(
						replay::InputPhase::interstitial,
						normalize_replay_interstitial_input(input)
					); !sampled) {
						return std::unexpected(replay_capture_error());
					}
				}
				return ClassicStoryBackendTick{
					.surface_changed = true,
					.input_sampled = true,
					.process_advanced = true,
					.step_result = {.status = StepStatus::paused},
				};
			}
			const bool resume = status == NewClassicPauseStatus::resume;
			const bool restart = status == NewClassicPauseStatus::restart;
			if(!resume && status != NewClassicPauseStatus::save_exit &&
				status != NewClassicPauseStatus::discard_exit) {
				return std::unexpected(session_error(
					ClassicStorySessionErrorCode::backend_contract_failed
				));
			}
			if((restart || status == NewClassicPauseStatus::discard_exit) &&
				replay_recorder) {
				replay_recorder->discard();
			}
			restart_requested = restart;
			advanced = driver->finish_deferred_frame(
				resume ? DeferredFrameAction::resume :
					DeferredFrameAction::exit_session
			);
			pause_menu.reset();
			if(resume) {
				pause_audio = {{
					{.type = ClassicAudioCommandType::resume_song},
					{.type = ClassicAudioCommandType::play_effect, .parameter = 21U},
				}};
				pause_audio_index = 0U;
				pause_audio_count = replay_recorder ? pause_audio.size() : 1U;
			}
		} else {
			advanced = driver->consume(normalized);
			if(advanced && driver->deferred_frame_pending()) {
				pause_menu.emplace(
					replay_recorder && replay_recorder->recording()
				);
				pause_audio = {{
					{.type = ClassicAudioCommandType::pause_song},
					{.type = ClassicAudioCommandType::play_effect, .parameter = 21U},
				}};
				pause_audio_index = 0U;
				pause_audio_count = replay_recorder ? pause_audio.size() : 1U;
			}
		}
		if(!advanced) {
			if(advanced.error().process_error) {
				return std::unexpected(main_error(
					std::move(*advanced.error().process_error)
				));
			}
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		if(replay_recorder && replay_recorder->recording() &&
			source_frame_io && input_context == InputContext::gameplay &&
			advanced->process_advanced) {
			replay_recorder->frame_io(
				advanced->step_result, !driver->deferred_frame_pending()
			);
		}
		if(replay_recorder && replay_recorder->recording() &&
			advanced->process_advanced) {
			const auto recorded_input = input_context == InputContext::gameplay ?
				normalized : normalize_replay_interstitial_input(input);
			if(auto sampled = replay_recorder->sample(
				input_context == InputContext::gameplay ?
					replay::InputPhase::gameplay :
					replay::InputPhase::interstitial,
				recorded_input
			); !sampled) {
				return std::unexpected(replay_capture_error());
			}
			if(advanced->step_result.status == StepStatus::round_transition ||
				advanced->step_result.status == StepStatus::terminal) {
				if(auto split = replay_recorder->round_completed(
					advanced->step_result.input_boundary_state, std::nullopt,
					process->last_result_state()
				); !split) {
					return std::unexpected(replay_capture_error());
				}
			}
			if(advanced->round_started) {
				if(auto checkpoint = replay_recorder->round_started(
					process->round_start_observation()
				); !checkpoint) {
					return std::unexpected(replay_capture_error());
				}
			}
		}
		if(score_run && advanced->process_advanced &&
			advanced->step_result.status != StepStatus::paused &&
			score_run->play_frames != 0xFFFFFFFFU) {
			++score_run->play_frames;
		}
		return ClassicStoryBackendTick{
			.surface_changed = advanced->surface_changed,
			.input_sampled = advanced->process_advanced,
			.ready = advanced->terminal,
			.process_advanced = advanced->process_advanced,
			.round_started = advanced->round_started,
			.restart_requested = restart_requested && advanced->terminal,
			.step_result = advanced->step_result,
		};
	}

	std::expected<ClassicStoryTerminal, ClassicStorySessionError>
	main_terminal()
	{
		if(forced_terminal) {
			auto terminal = std::move(*forced_terminal);
			forced_terminal.reset();
			if(terminal.terminal.route == 0U) {
				if(auto checkpoint = checkpoint_score_run(); !checkpoint) {
					return std::unexpected(std::move(checkpoint.error()));
				}
			}
			return terminal;
		}
		if(!driver || !process ||
			driver->status() != MainProcessDriverStatus::terminal ||
			!driver->transition()) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto terminal = ClassicStoryTerminal{
			.process_start = process_start,
			.terminal = process->canonical_state(),
			.transition = *driver->transition(),
		};
		if(terminal.terminal.route == 0U) {
			if(auto checkpoint = checkpoint_score_run(); !checkpoint) {
				return std::unexpected(std::move(checkpoint.error()));
			}
		}
		return terminal;
	}

	std::expected<void, ClassicStorySessionError>
	force_main_outcome_for_route_test(ClassicStoryRouteTestOutcome outcome)
	{
		if(!process || !driver || forced_terminal || forced_continue_terminal ||
			process_start.game_mode != game_mode::story ||
			process_start.story_stage > 8U) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}

		auto terminal = process->canonical_state();
		terminal.route = outcome ==
			ClassicStoryRouteTestOutcome::exit_to_operator ? 0U : 2U;
		terminal.game_mode = game_mode::story;
		terminal.story_stage = process_start.story_stage;
		terminal.winner = outcome == ClassicStoryRouteTestOutcome::player_loss ?
			1 : 0;
		terminal.p1_score.fill(9U);
		// finish_main_process() performs this write before the resident
		// transition is constructed.
		terminal.resident_rand = static_cast<std::int32_t>(
			terminal.round_frame
		);

		SessionTransition transition{};
		transition.resident = process_start;
		transition.resident.global_frame = terminal.global_frame;
		transition.resident.resident_rand = terminal.resident_rand;
		transition.resident.random_seed = terminal.round_frame;
		// This route-only seam is armed after exactly one real MAIN frame, a
		// controlled prefix that cannot change either resident-only field.
		// CanonicalState deliberately does not duplicate them.
		transition.resident.story_lives = process_start.story_lives;
		transition.resident.skill = process_start.skill;
		transition.resident.winner = terminal.winner;
		transition.resident.runtime_snapshot.reset();
		transition.resident.compact_runtime_snapshot.reset();
		transition.resident.round_start_snapshot.reset();
		if(outcome == ClassicStoryRouteTestOutcome::exit_to_operator) {
			transition.action = SessionAction::exit_to_operator;
			forced_terminal.emplace(ClassicStoryTerminal{
				.process_start = process_start,
				.terminal = terminal,
				.transition = std::move(transition),
			});
			driver.reset();
			process.reset();
			return {};
		}
		transition.resident.story_stage = static_cast<std::uint8_t>(
			process_start.story_stage + 1U
		);
		transition.resident.last_score = {};
		transition.resident.last_score[0U] = terminal.p1_score;
		if(outcome == ClassicStoryRouteTestOutcome::player_loss) {
			transition.high_score_registration = transition.resident;
			transition.resident.last_score = {};
			transition.action = transition.resident.remaining_credits == 0U ?
				SessionAction::exit_to_operator :
				SessionAction::request_continue;
		} else if(process_start.story_stage == 8U) {
			transition.resident.story_stage = 99U;
			transition.high_score_registration = transition.resident;
			transition.action = SessionAction::ending;
		} else {
			transition.resident.paletted_character[1U] =
				process_start.story_opponents[
					transition.resident.story_stage
				];
			const auto opponent = main_player_selection_from_paletted(
				transition.resident.paletted_character[1U]
			);
			if(!opponent) {
				return std::unexpected(session_error(
					ClassicStorySessionErrorCode::backend_contract_failed
				));
			}
			if(transition.resident.story_stage < 7U &&
				opponent->character >= MainCharacter::chiyuri) {
				transition.resident.paletted_character[1U] = 1;
			}
			transition.action = SessionAction::start_gameplay;
		}

		// Stages 7 and 8 route directly into a cutscene without the winner
		// scene that would otherwise publish a handoff Raster.
		if(outcome == ClassicStoryRouteTestOutcome::player_win &&
			process_start.story_stage >= 7U) {
			if(cutscene_handoff) {
				return std::unexpected(session_error(
					ClassicStorySessionErrorCode::backend_contract_failed
				));
			}
			cutscene_handoff.emplace(normalize_main_cutscene_seed(
				process->raster()
			));
		}
		ClassicStoryTerminal forced{
			.process_start = process_start,
			.terminal = terminal,
			.transition = std::move(transition),
		};
		if(outcome == ClassicStoryRouteTestOutcome::player_loss &&
			forced.transition.action == SessionAction::request_continue) {
			forced_continue_terminal = forced;
		}
		forced_terminal.emplace(std::move(forced));
		if(outcome == ClassicStoryRouteTestOutcome::player_win) {
			driver.reset();
			process.reset();
		}
		return {};
	}

	std::expected<void, ClassicStorySessionError> start_win(
		const ClassicMainlWinContext &context
	)
	{
		const auto winner = main_player_selection_from_paletted(
			context.winner_paletted_character
		);
		if(!winner || win) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto resources = loader->load_mainl_win(*winner);
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = ClassicMainlWinScene::create(
			context, *resources, static_content->font()
		);
		if(!created) {
			return std::unexpected(win_error(std::move(created.error())));
		}
		win = std::move(*created);
		return {};
	}

	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	advance_win(ClassicAudioStatus audio, bool dismiss)
	{
		if(!win) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		if(win->phase() == ClassicMainlWinPhase::ready) {
			return ClassicStoryBackendTick{.ready = true};
		}
		auto advanced = win->advance(audio, dismiss);
		if(!advanced) {
			return std::unexpected(win_error(std::move(advanced.error())));
		}
		return ClassicStoryBackendTick{
			.surface_changed = advanced->surface_changed,
			.input_sampled = advanced->input_sampled,
			// The ready-producing tick also queues the winner-song stop.
			// Readiness is published after the owner drains that command.
			.ready = false,
			.stage_preload_requested = advanced->stage_preload_requested,
		};
	}

	std::expected<void, ClassicStorySessionError> preload_stage(
		const MainStoryConfig &config
	)
	{
		if(!win || pending_stage) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto created = create_stage(config);
		if(!created) {
			return std::unexpected(std::move(created.error()));
		}
		if(!(*created)->preload_splash_into(win->stage_preload_raster())) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::stage_failed
			));
		}
		auto acknowledged = win->acknowledge_stage_preload();
		if(!acknowledged) {
			return std::unexpected(win_error(std::move(acknowledged.error())));
		}
		pending_stage = std::move(*created);
		return {};
	}

	std::expected<void, ClassicStorySessionError> finish_win()
	{
		if(!win) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto completion = win->take_completion();
		if(!completion || completion->stage_splash_preloaded() !=
			static_cast<bool>(pending_stage)) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		if(pending_stage && !pending_stage->adopt_preloaded_raster(
			std::move(completion->raster_)
		)) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::stage_failed
			));
		}
		if(!pending_stage) {
			cutscene_handoff.emplace(std::move(completion->raster_));
		}
		win.reset();
		return {};
	}

	std::expected<void, ClassicStorySessionError> start_cutscene(
		MainlCutsceneResourceRequest request
	)
	{
		if(cutscene || staff || registration || stage || win || continue_scene ||
			game_over) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		std::optional<present::Raster> initial{};
		if(registration_completion) {
			if(request.kind != MainlCutsceneKind::extra_ending ||
				registration_completion->next_screen_ !=
					MainlRegistrationNextScreen::game_over ||
				cutscene_handoff) {
				return std::unexpected(session_error(
					ClassicStorySessionErrorCode::backend_contract_failed
				));
			}
			initial.emplace(registration_completion->raster_);
		} else if(cutscene_handoff) {
			initial.emplace(*cutscene_handoff);
		} else if(process && driver &&
			driver->status() == MainProcessDriverStatus::terminal) {
			initial.emplace(normalize_main_cutscene_seed(process->raster()));
		} else {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto resources = loader->load_mainl_cutscene(request);
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = ClassicCutsceneScene::create(
			request, *resources, static_content->font(), std::move(*initial)
		);
		if(!created) {
			return std::unexpected(cutscene_error(std::move(created.error())));
		}
		cutscene = std::move(*created);
		cutscene_replay_input_due = true;
		registration_completion.reset();
		cutscene_handoff.reset();
		return {};
	}

	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	advance_cutscene(const ClassicCutsceneInput &input)
	{
		if(!cutscene) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		if(cutscene->phase() == ClassicCutscenePhase::ready) {
			return ClassicStoryBackendTick{.ready = true};
		}
		auto advanced = cutscene->advance(input);
		if(!advanced) {
			return std::unexpected(cutscene_error(
				std::move(advanced.error())
			));
		}
		bool input_sampled = advanced->input_sampled;
		if(new_classic) {
			// Replay Patch 0.4.10 records at most one interface sample per
			// VSync even though cutscene_animate() can poll several times in
			// one interpreter iteration.  Reuse the last sample for those
			// immediate polls, then arm the next sample after time advances.
			if(input_sampled) {
				input_sampled = cutscene_replay_input_due;
				cutscene_replay_input_due = false;
			}
			if(advanced->boundary == ClassicCutsceneBoundary::vsync ||
				advanced->boundary == ClassicCutsceneBoundary::audio_measure) {
				cutscene_replay_input_due = true;
			}
		}
		return ClassicStoryBackendTick{
			.surface_changed = advanced->surface_changed,
			.input_sampled = input_sampled,
			.ready = advanced->ready,
		};
	}

	std::expected<void, ClassicStorySessionError> finish_cutscene()
	{
		if(!cutscene) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto completion = cutscene->take_completion();
		if(!completion) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		cutscene_handoff.emplace(std::move(completion->raster_));
		cutscene.reset();
		return {};
	}

	std::expected<void, ClassicStorySessionError> start_staff(
		const GameInit &resident
	)
	{
		if(staff || cutscene || !cutscene_handoff) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto resources = loader->load_mainl_staff();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		bool fast_forward_unlocked = false;
		if(new_classic) {
			auto score = load_new_classic_score_store_read_only(
				score_path, legacy_score_path
			);
			if(!score) {
				return std::unexpected(new_score_store_error(
					std::move(score.error())
				));
			}
			// Match the donor's entry-time snapshot. The current first clear is
			// not committed until registration, so it cannot accelerate itself.
			fast_forward_unlocked = score->document.extra_unlocked;
		}
		auto created = ClassicStaffScene::create(
			resident,
			*resources,
			stage_text,
			staff_compiled,
			static_content->font(),
			static_content->trig(),
			*cutscene_handoff,
			fast_forward_unlocked
		);
		if(!created) {
			return std::unexpected(staff_error(std::move(created.error())));
		}
		cutscene_handoff.reset();
		staff = std::move(*created);
		return {};
	}

	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	advance_staff(const ClassicStaffInput &input)
	{
		if(!staff) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		if(staff->phase() == ClassicStaffPhase::ready) {
			return ClassicStoryBackendTick{.ready = true};
		}
		auto advanced = staff->advance(input);
		if(!advanced) {
			return std::unexpected(staff_error(std::move(advanced.error())));
		}
		return ClassicStoryBackendTick{
			.surface_changed = advanced->surface_changed,
			.input_sampled = advanced->input_sampled,
			.ready = advanced->ready,
		};
	}

	std::expected<void, ClassicStorySessionError> finish_staff()
	{
		if(!staff) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto completion = staff->take_completion();
		if(!completion) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		cutscene_handoff.emplace(std::move(completion->raster_));
		staff.reset();
		return {};
	}

	std::expected<void, ClassicStorySessionError> start_registration(
		const GameInit &resident
	)
	{
		if(registration || registration_completion || stage || pending_stage ||
			win || cutscene || staff ||
			continue_scene || game_over || !cutscene_handoff ||
			resident.rank >= 4U) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		const auto next_screen = (resident.remaining_credits != 0U &&
			resident.story_stage != 99U) ?
			MainlRegistrationNextScreen::continue_menu :
			MainlRegistrationNextScreen::game_over;
		const auto score_player = main_player_selection_from_paletted(
			resident.paletted_character[0U]
		);
		if(new_classic && !replay_playback && (!score_run || !score_player ||
			score_run_rank != resident.rank ||
			score_run_character != static_cast<std::uint8_t>(
				score_player->character
			))) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto resources = loader->load_mainl_registration({
			.rank = static_cast<MainRegistrationRank>(resident.rank + 1U),
			.next_screen = next_screen,
		});
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = new_classic ? (replay_playback ?
			ClassicRegistrationScene::create_new_classic_playback(
				resident, score_path, legacy_score_path, *resources,
				stage_text, static_content->font(), *cutscene_handoff
			) : ClassicRegistrationScene::create_new_classic(
				resident, score_path, legacy_score_path, *resources,
				stage_text, static_content->font(), *cutscene_handoff,
				[&]() {
					auto run = score_run.value_or(NewClassicScoreRun{});
					run.one_cc = resident.story_stage == 99U &&
						resident.remaining_credits == 3U;
					return run;
				}()
			)) : ClassicRegistrationScene::create(
				resident, score_path, *resources, stage_text,
				static_content->font(), *cutscene_handoff
			);
		if(!created) {
			return std::unexpected(registration_error(
				std::move(created.error())
			));
		}
		if(replay_recorder) {
			score_run.reset();
		}
		cutscene_handoff.reset();
		registration = std::move(*created);
		return {};
	}

	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	advance_registration(
		ClassicRegistrationInput input,
		ClassicAudioStatus audio
	)
	{
		if(!registration) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		if(registration->phase() == ClassicRegistrationPhase::ready) {
			return ClassicStoryBackendTick{.ready = true};
		}
		auto advanced = registration->advance(input, audio);
		if(!advanced) {
			return std::unexpected(registration_error(
				std::move(advanced.error())
			));
		}
		return ClassicStoryBackendTick{
			.surface_changed = advanced->surface_changed,
			.input_sampled = advanced->input_sampled,
			// The ready-producing tick may coincide with an atomic store commit.
			// Readiness is published on the next owner tick.
			.ready = false,
		};
	}

	std::expected<void, ClassicStorySessionError> finish_registration()
	{
		if(!registration) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto completion = registration->take_completion();
		if(!completion) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		registration_completion.emplace(std::move(*completion));
		registration.reset();
		// The Replay Patch closes a Story recording after registration and
		// before Continue (or the post-clear epilogue). Saving is a host UI
		// interruption; the live MAINL owner remains intact and resumes with
		// capture disabled if the player spends another credit.
		if(replay_recorder && replay_recorder->recording()) {
			if(auto boundary = replay_recorder->process_completed(
				replay::ProcessId::mainl
			); !boundary) {
				return std::unexpected(replay_capture_error());
			}
			const auto &resident = registration_completion->resident();
			const auto reason = resident.story_stage == 99U ?
				formats::ReplayEndReason::complete :
				formats::ReplayEndReason::game_over;
			if(auto finished = replay_recorder->finish(reason, resident);
				!finished) {
				return std::unexpected(replay_capture_error());
			}
			replay_capture_closed = true;
		}
		return {};
	}

	std::expected<bool, ClassicStorySessionError> start_continue(
		std::uint8_t remaining_credits
	)
	{
		if(!registration_completion) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto created = ClassicMainlContinueScene::create_from_registration(
			std::move(*registration_completion), remaining_credits,
			static_content->font()
		);
		if(!created) {
			return std::unexpected(continue_error(std::move(created.error())));
		}
		registration_completion.reset();
		continue_scene = std::move(*created);
		cutscene_handoff.reset();
		return continue_scene->phase() == ClassicMainlContinuePhase::ready;
	}

	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	advance_continue(ClassicMainlContinueInput input)
	{
		if(!continue_scene) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		if(continue_scene->phase() == ClassicMainlContinuePhase::ready) {
			return ClassicStoryBackendTick{.ready = true};
		}
		auto advanced = continue_scene->advance(input);
		if(!advanced) {
			return std::unexpected(continue_error(std::move(advanced.error())));
		}
		if(advanced->gameover_preload_requested) {
			auto resources = loader->load_mainl_gameover();
			if(!resources) {
				return std::unexpected(resource_error(std::move(resources.error())));
			}
			auto preloaded = continue_scene->preload_gameover(*resources);
			if(!preloaded) {
				return std::unexpected(continue_error(
					std::move(preloaded.error())
				));
			}
			return ClassicStoryBackendTick{
				.surface_changed = true,
				.input_sampled = advanced->input_sampled,
				.ready = false,
			};
		}
		return ClassicStoryBackendTick{
			.surface_changed = advanced->surface_changed,
			.input_sampled = advanced->input_sampled,
			// The ready-producing tick queues a stop command.
			.ready = false,
		};
	}

	std::expected<ContinueChoice, ClassicStorySessionError> finish_continue()
	{
		if(!continue_scene) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto completion = continue_scene->take_completion();
		if(!completion) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		const auto choice = completion->choice();
		continue_completion.emplace(std::move(*completion));
		continue_scene.reset();
		return choice;
	}

	std::expected<SessionTransition, ClassicStorySessionError>
	resolve_continue(ContinueChoice choice)
	{
		if(forced_continue_terminal) {
			auto transition = forced_continue_terminal->transition;
			if(transition.action != SessionAction::request_continue ||
				transition.resident.story_stage == 0U ||
				transition.resident.remaining_credits == 0U) {
				return std::unexpected(session_error(
					ClassicStorySessionErrorCode::backend_contract_failed
				));
			}
			--transition.resident.story_stage;
			transition.resident.story_lives = 2U;
			if(choice == ContinueChoice::accept) {
				--transition.resident.remaining_credits;
				transition.action = SessionAction::start_gameplay;
				continue_completion.reset();
				if(auto continued = continue_score_run(transition.resident);
					!continued) {
					return std::unexpected(std::move(continued.error()));
				}
			} else {
				transition.action = SessionAction::exit_to_operator;
			}
			forced_continue_terminal.reset();
			driver.reset();
			process.reset();
			return transition;
		}
		if(!driver) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto resolved = driver->resolve_continue(choice);
		if(!resolved) {
			if(resolved.error().process_error) {
				return std::unexpected(main_error(
					std::move(*resolved.error().process_error)
				));
			}
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		if(choice == ContinueChoice::accept) {
			continue_completion.reset();
		}
		if(choice == ContinueChoice::accept) {
			if(auto continued = continue_score_run(resolved->resident); !continued) {
				return std::unexpected(std::move(continued.error()));
			}
		}
		driver.reset();
		process.reset();
		return *resolved;
	}

	std::expected<void, ClassicStorySessionError> start_game_over()
	{
		if(!continue_completion || game_over || game_over_completion) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto created = ClassicMainlGameOverScene::create(
			std::move(*continue_completion)
		);
		continue_completion.reset();
		if(!created) {
			return std::unexpected(continue_error(
				std::move(created.error()),
				ClassicStorySessionErrorCode::game_over_failed
			));
		}
		game_over = std::move(*created);
		return {};
	}

	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	advance_game_over(ClassicAudioStatus audio)
	{
		if(!game_over) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		if(game_over->phase() == ClassicMainlGameOverPhase::ready) {
			return ClassicStoryBackendTick{.ready = true};
		}
		auto advanced = game_over->advance(audio);
		if(!advanced) {
			return std::unexpected(continue_error(
				std::move(advanced.error()),
				ClassicStorySessionErrorCode::game_over_failed
			));
		}
		return ClassicStoryBackendTick{
			.surface_changed = advanced->surface_changed,
			// The ready-producing tick queues the final stop command.
			.ready = false,
		};
	}

	std::expected<void, ClassicStorySessionError> finish_game_over()
	{
		if(!game_over) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto completion = game_over->take_completion();
		if(!completion) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		game_over_completion.emplace(std::move(*completion));
		game_over.reset();
		return {};
	}

	std::optional<ClassicAudioCommand> take_audio_command()
	{
		if(stage) {
			if(auto command = stage->take_audio_command()) {
				return command;
			}
		}
		if(pause_audio_count != 0U) {
			const auto command = pause_audio[pause_audio_index++];
			--pause_audio_count;
			if(pause_audio_count == 0U) {
				pause_audio_index = 0U;
			}
			return command;
		}
		if(process) {
			if(auto command = process->take_audio_command()) {
				return command;
			}
		}
		if(staff) {
			return staff->take_audio_command();
		}
		if(registration) {
			return registration->take_audio_command();
		}
		if(cutscene) {
			return cutscene->take_audio_command();
		}
		if(win) {
			return win->take_audio_command();
		}
		if(continue_scene) {
			return continue_scene->take_audio_command();
		}
		if(game_over) {
			return game_over->take_audio_command();
		}
		return std::nullopt;
	}

	std::expected<present::SourceSurface, ClassicStorySessionError>
	compose(std::span<
		present::ComposedPixel,
		present::graphics_page_pixels
	> output)
	{
		const present::Raster *raster = nullptr;
		const present::Tram *tram = &blank_tram;
		if(stage) {
			raster = &stage->raster();
			tram = &stage->tram();
		} else if(staff) {
			raster = &staff->raster();
		} else if(registration) {
			raster = &registration->raster();
		} else if(registration_completion) {
			raster = &registration_completion->raster();
		} else if(cutscene) {
			raster = &cutscene->raster();
			tram = &cutscene->tram();
		} else if(win) {
			raster = &win->raster();
		} else if(continue_scene) {
			raster = &continue_scene->raster();
		} else if(continue_completion) {
			raster = &continue_completion->raster();
		} else if(game_over) {
			raster = &game_over->raster();
		} else if(game_over_completion) {
			raster = &game_over_completion->raster();
		} else if(cutscene_handoff) {
			raster = &*cutscene_handoff;
		}
		if(!raster && process) {
			auto composed = process->compose_shown_surface(output);
			if(!composed) {
				return std::unexpected(surface_error(composed.error()));
			}
			return pause_menu ? pause_menu->compose_over(*composed) : *composed;
		}
		if(!raster) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::surface_unavailable
			));
		}
		auto composed = present::compose_source_surface(
			output, *raster, *tram, static_content->font()
		);
		if(!composed) {
			return std::unexpected(surface_error(composed.error()));
		}
		return *composed;
	}
};

namespace {

template <typename Result, typename Method, typename... Args>
Result backend_call(void *context, Method method, Args &&...args)
{
	return (static_cast<ClassicStoryBackend *>(context)->*method)(
		std::forward<Args>(args)...
	);
}

ClassicStorySessionBackendOperations backend_operations(
	ClassicStoryBackend &backend
) noexcept
{
	return {
		.context = &backend,
		.start_stage = [](void *context, const MainStoryConfig &config) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::start_stage, config
			);
		},
		.advance_stage = [](void *context, bool dismiss) {
			return backend_call<std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>>(
				context, &ClassicStoryBackend::advance_stage, dismiss
			);
		},
		.start_main = [](void *context, const MainStoryConfig &config) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::start_main, config
			);
		},
		.advance_main = [](
			void *context,
			const ClassicMainInputSample &input
		) {
			return backend_call<std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>>(
				context, &ClassicStoryBackend::advance_main, input
			);
		},
		.main_terminal = [](void *context) {
			return backend_call<std::expected<ClassicStoryTerminal,
				ClassicStorySessionError>>(
				context, &ClassicStoryBackend::main_terminal
			);
		},
		.start_win = [](void *context, const ClassicMainlWinContext &win) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::start_win, win
			);
		},
		.advance_win = [](void *context, ClassicAudioStatus audio, bool dismiss) {
			return backend_call<std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>>(
				context, &ClassicStoryBackend::advance_win, audio, dismiss
			);
		},
		.preload_stage = [](void *context, const MainStoryConfig &config) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::preload_stage, config
			);
		},
		.finish_win = [](void *context) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::finish_win
			);
		},
		.start_cutscene = [](void *context, MainlCutsceneResourceRequest request) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::start_cutscene, request
			);
		},
		.advance_cutscene = [](void *context, const ClassicCutsceneInput &input) {
			return backend_call<std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>>(
				context, &ClassicStoryBackend::advance_cutscene, input
			);
		},
		.finish_cutscene = [](void *context) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::finish_cutscene
			);
		},
		.start_staff = [](void *context, const GameInit &resident) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::start_staff, resident
			);
		},
		.advance_staff = [](void *context, const ClassicStaffInput &input) {
			return backend_call<std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>>(
				context, &ClassicStoryBackend::advance_staff, input
			);
		},
		.finish_staff = [](void *context) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::finish_staff
			);
		},
		.start_registration = [](void *context, const GameInit &resident) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::start_registration, resident
			);
		},
		.advance_registration = [](
			void *context,
			ClassicRegistrationInput input,
			ClassicAudioStatus audio
		) {
			return backend_call<std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>>(
				context, &ClassicStoryBackend::advance_registration, input, audio
			);
		},
		.finish_registration = [](void *context) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::finish_registration
			);
		},
		.start_continue = [](void *context, std::uint8_t credits) {
			return backend_call<std::expected<bool, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::start_continue, credits
			);
		},
		.advance_continue = [](void *context, ClassicMainlContinueInput input) {
			return backend_call<std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>>(
				context, &ClassicStoryBackend::advance_continue, input
			);
		},
		.finish_continue = [](void *context) {
			return backend_call<std::expected<ContinueChoice,
				ClassicStorySessionError>>(
				context, &ClassicStoryBackend::finish_continue
			);
		},
		.resolve_continue = [](void *context, ContinueChoice choice) {
			return backend_call<std::expected<SessionTransition,
				ClassicStorySessionError>>(
				context, &ClassicStoryBackend::resolve_continue, choice
			);
		},
		.start_game_over = [](void *context) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::start_game_over
			);
		},
		.advance_game_over = [](void *context, ClassicAudioStatus audio) {
			return backend_call<std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>>(
				context, &ClassicStoryBackend::advance_game_over, audio
			);
		},
		.finish_game_over = [](void *context) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::finish_game_over
			);
		},
		.checkpoint_score_run = [](void *context) {
			return backend_call<std::expected<void, ClassicStorySessionError>>(
				context, &ClassicStoryBackend::checkpoint_score_run
			);
		},
		.take_audio_command = [](void *context) {
			return static_cast<ClassicStoryBackend *>(context)->
				take_audio_command();
		},
		.compose_shown_surface = [](void *context, auto output) {
			return static_cast<ClassicStoryBackend *>(context)->compose(output);
		},
	};
}

ClassicStorySessionTestHooks backend_test_hooks(
	ClassicStoryBackend &backend
) noexcept
{
	return {
		.context = &backend,
		.force_main_outcome = [](void *context, auto outcome) {
			return static_cast<ClassicStoryBackend *>(context)->
				force_main_outcome_for_route_test(outcome);
		},
	};
}

} // namespace

struct ClassicStorySession::Impl {
	MainStoryConfig base_config{};
	std::shared_ptr<void> backend_owner{};
	ClassicStorySessionBackendOperations operations{};
	ClassicStorySessionTestHooks test_hooks{};
	ClassicMainlPlan plan{};
	std::size_t cursor = 0U;
	ClassicStorySessionScene scene = ClassicStorySessionScene::failed;
	ClassicStorySessionStatus status = ClassicStorySessionStatus::running;
	std::uint32_t source_sample_count = 0U;
	bool continue_pending = false;
	bool restart_requested = false;
	NewClassicReplayRecorder *replay_recorder = nullptr;
	bool stop_before_main = false;
	std::optional<ClassicStorySessionError> failure{};

	Impl(
		MainStoryConfig config,
		std::shared_ptr<void> owner,
		ClassicStorySessionBackendOperations operations_,
		ClassicStorySessionTestHooks test_hooks_,
		ClassicMainlPlan plan_,
		NewClassicReplayRecorder *replay_recorder_,
		bool stop_before_main_
	) :
		base_config(std::move(config)),
		backend_owner(std::move(owner)),
		operations(operations_),
		test_hooks(test_hooks_),
		plan(std::move(plan_)),
		replay_recorder(replay_recorder_),
		stop_before_main(stop_before_main_)
	{}

	void fail(ClassicStorySessionError error)
	{
		failure.emplace(std::move(error));
		scene = ClassicStorySessionScene::failed;
		status = ClassicStorySessionStatus::failed;
	}

	[[nodiscard]] MainStoryConfig current_config() const noexcept
	{
		auto config = story_config(base_config, plan.resident());
		config.resident.global_frame = source_sample_count;
		return config;
	}

	[[nodiscard]] std::expected<void, ClassicStorySessionError> activate()
	{
		while(cursor < plan.steps().size()) {
			const auto step = plan.steps()[cursor];
			scene = session_scene(step.scene);
			switch(step.scene) {
			case ClassicMainlScene::stage_splash: {
				auto started = operations.start_stage(
					operations.context, current_config()
				);
				if(!started) {
					return std::unexpected(std::move(started.error()));
				}
				return {};
			}
			case ClassicMainlScene::start_main: {
				if(stop_before_main) {
					scene = ClassicStorySessionScene::complete;
					status = ClassicStorySessionStatus::complete;
					return {};
				}
				auto started = operations.start_main(
					operations.context, current_config()
				);
				if(!started) {
					return std::unexpected(std::move(started.error()));
				}
				return {};
			}
			case ClassicMainlScene::win: {
				if(!plan.win()) {
					return std::unexpected(session_error(
						ClassicStorySessionErrorCode::backend_contract_failed
					));
				}
				auto started = operations.start_win(
					operations.context, *plan.win()
				);
				if(!started) {
					return std::unexpected(std::move(started.error()));
				}
				return {};
			}
			case ClassicMainlScene::continue_menu: {
				auto started = operations.start_continue(
					operations.context,
					plan.resident().remaining_credits
				);
				if(!started) {
					return std::unexpected(std::move(started.error()));
				}
				if(!*started) {
					return {};
				}
				auto choice = operations.finish_continue(
					operations.context
				);
				if(!choice || *choice != ContinueChoice::decline ||
					continue_pending) {
					return std::unexpected(choice ? session_error(
						ClassicStorySessionErrorCode::backend_contract_failed
					) : std::move(choice.error()));
				}
				++cursor;
				continue;
			}
			case ClassicMainlScene::game_over: {
				auto started = operations.start_game_over(operations.context);
				if(!started) {
					return std::unexpected(std::move(started.error()));
				}
				return {};
			}
			case ClassicMainlScene::interstage_cutscene:
			case ClassicMainlScene::ending:
			case ClassicMainlScene::extra_ending: {
				const auto request = cutscene_request(step, plan.resident());
				if(!request) {
					return std::unexpected(session_error(
						ClassicStorySessionErrorCode::backend_contract_failed
					));
				}
				auto started = operations.start_cutscene(
					operations.context, *request
				);
				if(!started) {
					return std::unexpected(std::move(started.error()));
				}
				return {};
			}
			case ClassicMainlScene::staff_verdict: {
				auto resident = plan.resident();
				resident.global_frame = source_sample_count;
				auto started = operations.start_staff(
					operations.context, resident
				);
				if(!started) {
					return std::unexpected(std::move(started.error()));
				}
				return {};
			}
			case ClassicMainlScene::high_score_registration: {
				if(!plan.high_score_registration()) {
					return std::unexpected(session_error(
						ClassicStorySessionErrorCode::backend_contract_failed
					));
				}
				auto resident = *plan.high_score_registration();
				resident.global_frame = source_sample_count;
				auto started = operations.start_registration(
					operations.context, resident
				);
				if(!started) {
					return std::unexpected(std::move(started.error()));
				}
				return {};
			}
			case ClassicMainlScene::exit_to_operator:
				status = ClassicStorySessionStatus::complete;
				return {};
			}
		}
		return std::unexpected(session_error(
			ClassicStorySessionErrorCode::backend_contract_failed
		));
	}

	[[nodiscard]] std::expected<void, ClassicStorySessionError>
	advance_cursor()
	{
		++cursor;
		return activate();
	}

	[[nodiscard]] std::expected<void, ClassicStorySessionError>
	replace_plan(ClassicMainlPlan replacement)
	{
		plan = std::move(replacement);
		cursor = 0U;
		status = ClassicStorySessionStatus::running;
		return activate();
	}
};

std::expected<std::unique_ptr<ClassicStorySession>, ClassicStorySessionError>
ClassicStorySession::create(
	const MainStoryConfig &config,
	std::unique_ptr<MainResourceLoader> loader,
	MainCompiledContent compiled_content,
	std::span<const std::byte> font_rom,
	MainlStageTextAssets stage_text,
	MainlStaffCompiledAssets staff_compiled,
	std::filesystem::path score_path,
	NewClassicReplayRecorder *replay_recorder
)
{
	if(!loader) {
		return std::unexpected(session_error(
			ClassicStorySessionErrorCode::invalid_config
		));
	}
	if(auto start = classic_story_start_plan(config); !start) {
		return std::unexpected(plan_error(start.error()));
	}
	auto static_resources = loader->load_main_static();
	if(!static_resources) {
		return std::unexpected(resource_error(
			std::move(static_resources.error())
		));
	}
	auto static_content = MainProcessStaticContentAccess::create(
		*static_resources,
		std::move(compiled_content),
		font_rom
	);
	if(!static_content) {
		auto error = session_error(
			ClassicStorySessionErrorCode::static_content_failed
		);
		error.main_error.emplace(std::move(static_content.error()));
		return std::unexpected(std::move(error));
	}
	auto shared_loader = std::shared_ptr<MainResourceLoader>(std::move(loader));
	auto legacy_score_path = std::filesystem::path{};
	if(replay_recorder) {
		legacy_score_path = score_path;
		score_path = score_path.parent_path() / "YUME.NEW.NEM";
	}
	auto backend = std::make_shared<ClassicStoryBackend>(
		std::move(shared_loader), std::move(*static_content), std::move(stage_text),
		std::move(staff_compiled), std::move(score_path), replay_recorder,
		std::move(legacy_score_path), replay_recorder != nullptr, false
	);
	return create_with_backend(
		config,
		std::static_pointer_cast<void>(backend),
		backend_operations(*backend),
		backend_test_hooks(*backend), replay_recorder
	);
}

std::expected<std::unique_ptr<ClassicStorySession>, ClassicStorySessionError>
ClassicStorySession::create_with_prepared_content(
	const MainStoryConfig &config,
	std::shared_ptr<MainResourceLoader> loader,
	std::shared_ptr<const MainProcessStaticContent> static_content,
	MainlStageTextAssets stage_text,
	MainlStaffCompiledAssets staff_compiled,
	std::filesystem::path score_path,
	NewClassicReplayRecorder *replay_recorder,
	std::filesystem::path legacy_score_path
)
{
	if(!loader || !static_content) {
		return std::unexpected(session_error(
			ClassicStorySessionErrorCode::invalid_config
		));
	}
	auto backend = std::make_shared<ClassicStoryBackend>(
		std::move(loader), std::move(static_content), std::move(stage_text),
		std::move(staff_compiled), std::move(score_path), replay_recorder,
		std::move(legacy_score_path), replay_recorder != nullptr, false
	);
	return create_with_backend(
		config, std::static_pointer_cast<void>(backend),
		backend_operations(*backend), backend_test_hooks(*backend), replay_recorder
	);
}

std::expected<std::unique_ptr<ClassicStorySession>, ClassicStorySessionError>
ClassicStorySession::create(
	const MainStoryConfig &config,
	std::unique_ptr<MainResourceLoader> loader,
	MainCompiledContent compiled_content,
	std::span<const std::byte> font_rom,
	std::span<const std::byte> mainl_executable,
	std::filesystem::path score_path,
	NewClassicReplayRecorder *replay_recorder
)
{
	if(!valid_main_story_config(config)) {
		return std::unexpected(session_error(
			ClassicStorySessionErrorCode::invalid_config
		));
	}
	auto text = import_mainl_stage_text(mainl_executable);
	if(!text) {
		auto error = session_error(
			ClassicStorySessionErrorCode::mainl_text_failed
		);
		error.mainl_text_error.emplace(std::move(text.error()));
		return std::unexpected(std::move(error));
	}
	return create(
		config,
		std::move(loader),
		std::move(compiled_content),
		font_rom,
		std::move(text->assets),
		std::move(text->staff),
		std::move(score_path),
		replay_recorder
	);
}

std::expected<std::unique_ptr<ClassicStorySession>, ClassicStorySessionError>
ClassicStorySession::create_replay_interstitial(
	const MainStoryConfig &process_config,
	std::shared_ptr<MainResourceLoader> loader,
	std::shared_ptr<const MainProcessStaticContent> static_content,
	MainlStageTextAssets stage_text,
	MainlStaffCompiledAssets staff_compiled,
	std::filesystem::path score_path,
	std::filesystem::path legacy_score_path,
	std::unique_ptr<MainProcess> process,
	std::unique_ptr<MainProcessDriver> driver
)
{
	if(!valid_main_story_config(process_config) || !loader || !static_content ||
		!process || !driver ||
		driver->status() != MainProcessDriverStatus::terminal ||
		!driver->transition()) {
		return std::unexpected(session_error(
			ClassicStorySessionErrorCode::invalid_config
		));
	}
	const auto terminal = process->canonical_state();
	auto plan = classic_story_mainl_plan(
		process_config.resident, terminal, *driver->transition()
	);
	if(!plan) {
		return std::unexpected(plan_error(plan.error()));
	}
	auto backend = std::make_shared<ClassicStoryBackend>(
		std::move(loader), std::move(static_content), std::move(stage_text),
		std::move(staff_compiled), std::move(score_path), nullptr,
		std::move(legacy_score_path), true, true
	);
	if(auto adopted = backend->adopt_replay_terminal(
		process_config.resident, std::move(process), std::move(driver)
	); !adopted) {
		return std::unexpected(std::move(adopted.error()));
	}
	return create_with_backend(
		process_config, std::static_pointer_cast<void>(backend),
		backend_operations(*backend), {}, nullptr, true, std::move(*plan),
		terminal.global_frame
	);
}

std::expected<std::unique_ptr<ClassicStorySession>, ClassicStorySessionError>
ClassicStorySession::create_with_backend(
	const MainStoryConfig &config,
	std::shared_ptr<void> backend_owner,
	ClassicStorySessionBackendOperations operations,
	ClassicStorySessionTestHooks test_hooks,
	NewClassicReplayRecorder *replay_recorder,
	bool stop_before_main,
	std::optional<ClassicMainlPlan> initial_plan,
	std::optional<std::uint32_t> initial_source_sample_count
)
{
	if(!valid_main_story_config(config)) {
		return std::unexpected(session_error(
			ClassicStorySessionErrorCode::invalid_config
		));
	}
	if(!backend_owner || !valid_backend(operations)) {
		return std::unexpected(session_error(
			ClassicStorySessionErrorCode::invalid_backend
		));
	}
	if(!initial_plan) {
		auto plan = classic_story_start_plan(config);
		if(!plan) {
			return std::unexpected(plan_error(plan.error()));
		}
		initial_plan.emplace(std::move(*plan));
	}
	auto impl = std::make_unique<Impl>(
		config, std::move(backend_owner), operations, test_hooks,
		std::move(*initial_plan), replay_recorder, stop_before_main
	);
	impl->source_sample_count = initial_source_sample_count.value_or(
		config.resident.global_frame
	);
	if(auto activated = impl->activate(); !activated) {
		return std::unexpected(std::move(activated.error()));
	}
	return std::unique_ptr<ClassicStorySession>(
		new ClassicStorySession(std::move(impl))
	);
}

ClassicStorySession::ClassicStorySession(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{}

ClassicStorySession::~ClassicStorySession() = default;

std::expected<ClassicStorySessionTick, ClassicStorySessionError>
ClassicStorySession::consume(const ClassicStorySessionInput &input)
{
	if(impl_->failure) {
		return std::unexpected(*impl_->failure);
	}
	if(impl_->status != ClassicStorySessionStatus::running) {
		return std::unexpected(session_error(
			ClassicStorySessionErrorCode::session_not_running
		));
	}
	if(input.app_quit_requested) {
		if(impl_->operations.checkpoint_score_run) {
			auto checkpoint = impl_->operations.checkpoint_score_run(
				impl_->operations.context
			);
			if(!checkpoint) {
				impl_->fail(std::move(checkpoint.error()));
				return std::unexpected(*impl_->failure);
			}
		}
		impl_->status = ClassicStorySessionStatus::quit_requested;
		return ClassicStorySessionTick{
			.scene = impl_->scene,
			.status = impl_->status,
			.source_sample_count = impl_->source_sample_count,
		};
	}

	const auto scene_before = impl_->scene;
	std::expected<ClassicStoryBackendTick, ClassicStorySessionError> tick =
		std::unexpected(session_error(
			ClassicStorySessionErrorCode::backend_contract_failed
		));
	switch(impl_->scene) {
	case ClassicStorySessionScene::stage_splash:
		tick = impl_->operations.advance_stage(
			impl_->operations.context, input.dismiss_pressed
		);
		break;
	case ClassicStorySessionScene::main:
		tick = impl_->operations.advance_main(
			impl_->operations.context, input.main
		);
		break;
	case ClassicStorySessionScene::win:
		tick = impl_->operations.advance_win(
			impl_->operations.context, input.audio, input.dismiss_pressed
		);
		break;
	case ClassicStorySessionScene::cutscene:
		tick = impl_->operations.advance_cutscene(
			impl_->operations.context, input.cutscene
		);
		break;
	case ClassicStorySessionScene::staff_verdict:
		tick = impl_->operations.advance_staff(
			impl_->operations.context, input.staff
		);
		break;
	case ClassicStorySessionScene::high_score_registration:
		tick = impl_->operations.advance_registration(
			impl_->operations.context, input.registration, input.audio
		);
		break;
	case ClassicStorySessionScene::continue_menu:
		tick = impl_->operations.advance_continue(
			impl_->operations.context, input.continue_menu
		);
		break;
	case ClassicStorySessionScene::game_over:
		tick = impl_->operations.advance_game_over(
			impl_->operations.context, input.audio
		);
		break;
	case ClassicStorySessionScene::complete:
	case ClassicStorySessionScene::failed:
		break;
	}
	if(!tick) {
		impl_->fail(std::move(tick.error()));
		return std::unexpected(*impl_->failure);
	}
	if(tick->input_sampled && scene_before != ClassicStorySessionScene::main &&
		impl_->replay_recorder && impl_->replay_recorder->recording()) {
		if(auto sampled = impl_->replay_recorder->sample(
			replay::InputPhase::interstitial,
			normalize_replay_interstitial_input(input.main)
		); !sampled) {
			impl_->fail(replay_capture_error());
			return std::unexpected(*impl_->failure);
		}
	}
	if(tick->input_sampled) {
		++impl_->source_sample_count;
	}
	impl_->restart_requested = impl_->restart_requested ||
		tick->restart_requested;

	if(impl_->scene == ClassicStorySessionScene::win &&
		tick->stage_preload_requested) {
		auto preloaded = impl_->operations.preload_stage(
			impl_->operations.context, impl_->current_config()
		);
		if(!preloaded) {
			impl_->fail(std::move(preloaded.error()));
			return std::unexpected(*impl_->failure);
		}
	}

	bool transitioned = false;
	if(tick->ready) {
		std::expected<void, ClassicStorySessionError> advanced{};
		switch(impl_->scene) {
		case ClassicStorySessionScene::stage_splash:
			advanced = impl_->advance_cursor();
			break;
		case ClassicStorySessionScene::main: {
			auto terminal = impl_->operations.main_terminal(
				impl_->operations.context
			);
			if(!terminal) {
				advanced = std::unexpected(std::move(terminal.error()));
				break;
			}
			auto plan = classic_story_mainl_plan(
				terminal->process_start,
				terminal->terminal,
				terminal->transition
			);
			if(!plan) {
				advanced = std::unexpected(plan_error(plan.error()));
				break;
			}
			if(impl_->replay_recorder && impl_->replay_recorder->recording()) {
				const auto *last = impl_->replay_recorder->last_terminal();
				if(!last || last->global_frame != terminal->terminal.global_frame ||
					last->round_id != terminal->terminal.round_id) {
					if(auto split = impl_->replay_recorder->round_completed(
						terminal->terminal
					); !split) {
						advanced = std::unexpected(replay_capture_error());
						break;
					}
				}
				if(auto boundary = impl_->replay_recorder->process_completed(
					replay::ProcessId::main
				); !boundary) {
					advanced = std::unexpected(replay_capture_error());
					break;
				}
			}
			impl_->continue_pending = terminal->transition.action ==
				SessionAction::request_continue;
			advanced = impl_->replace_plan(std::move(*plan));
			break;
		}
		case ClassicStorySessionScene::win:
			advanced = impl_->operations.finish_win(impl_->operations.context);
			if(advanced) {
				advanced = impl_->advance_cursor();
			}
			break;
		case ClassicStorySessionScene::cutscene:
			advanced = impl_->operations.finish_cutscene(
				impl_->operations.context
			);
			if(advanced) {
				advanced = impl_->advance_cursor();
			}
			break;
		case ClassicStorySessionScene::staff_verdict:
			advanced = impl_->operations.finish_staff(
				impl_->operations.context
			);
			if(advanced) {
				advanced = impl_->advance_cursor();
			}
			break;
		case ClassicStorySessionScene::high_score_registration:
			advanced = impl_->operations.finish_registration(
				impl_->operations.context
			);
			if(advanced) {
				advanced = impl_->advance_cursor();
			}
			break;
		case ClassicStorySessionScene::continue_menu: {
			auto choice = impl_->operations.finish_continue(
				impl_->operations.context
			);
			if(!choice) {
				advanced = std::unexpected(std::move(choice.error()));
				break;
			}
			if(impl_->continue_pending) {
				auto transition = impl_->operations.resolve_continue(
					impl_->operations.context, *choice
				);
				if(!transition) {
					advanced = std::unexpected(
						std::move(transition.error())
					);
					break;
				}
				auto plan = classic_story_continue_plan(
					*choice, *transition
				);
				if(!plan) {
					advanced = std::unexpected(plan_error(plan.error()));
					break;
				}
				impl_->continue_pending = false;
				advanced = impl_->replace_plan(std::move(*plan));
			} else if(*choice == ContinueChoice::decline) {
				advanced = impl_->advance_cursor();
			} else {
				advanced = std::unexpected(session_error(
					ClassicStorySessionErrorCode::backend_contract_failed
				));
			}
			break;
		}
		case ClassicStorySessionScene::game_over:
			advanced = impl_->operations.finish_game_over(
				impl_->operations.context
			);
			if(advanced) {
				advanced = impl_->advance_cursor();
			}
			break;
		case ClassicStorySessionScene::complete:
		case ClassicStorySessionScene::failed:
			advanced = std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
			break;
		}
		if(!advanced) {
			impl_->fail(std::move(advanced.error()));
			return std::unexpected(*impl_->failure);
		}
		if(impl_->status == ClassicStorySessionStatus::complete &&
			impl_->replay_recorder && impl_->replay_recorder->recording()) {
			if(auto boundary = impl_->replay_recorder->process_completed(
				replay::ProcessId::mainl
			); !boundary) {
				impl_->fail(replay_capture_error());
				return std::unexpected(*impl_->failure);
			}
		}
		transitioned = true;
	}

	return ClassicStorySessionTick{
		.scene = impl_->scene,
		.status = impl_->status,
		.source_sample_count = impl_->source_sample_count,
		.surface_changed = tick->surface_changed || transitioned,
		.input_sampled = tick->input_sampled,
		.process_advanced = tick->process_advanced,
		.round_started = tick->round_started,
		.step_result = tick->step_result,
	};
}

std::optional<ClassicAudioCommand>
ClassicStorySession::take_audio_command()
{
	return impl_->operations.take_audio_command(impl_->operations.context);
}

std::expected<present::SourceSurface, ClassicStorySessionError>
ClassicStorySession::compose_shown_surface(
	std::span<present::ComposedPixel, present::graphics_page_pixels> output
) const
{
	if(impl_->failure) {
		return std::unexpected(*impl_->failure);
	}
	return impl_->operations.compose_shown_surface(
		impl_->operations.context, output
	);
}

ClassicStorySessionScene ClassicStorySession::scene() const noexcept
{
	return impl_->scene;
}

ClassicStorySessionStatus ClassicStorySession::status() const noexcept
{
	return impl_->status;
}

present::Raster ClassicStorySessionTestAccess::normalize_main_cutscene_seed(
	const present::Raster &raster
)
{
	return th03::app::normalize_main_cutscene_seed(raster);
}

CoreInput ClassicStorySessionTestAccess::story_main_input(
	const ClassicMainInputSample &input,
	InputContext context,
	bool new_classic
) noexcept
{
	return normalize_story_main_input(input, context, new_classic);
}

std::expected<void, ClassicStorySessionError>
ClassicStorySessionTestAccess::force_main_outcome(
	ClassicStorySession &session,
	ClassicStoryRouteTestOutcome outcome
)
{
	if(session.impl_->scene != ClassicStorySessionScene::main ||
		!session.impl_->test_hooks.context ||
		!session.impl_->test_hooks.force_main_outcome) {
		return std::unexpected(session_error(
			ClassicStorySessionErrorCode::backend_contract_failed
		));
	}
	return session.impl_->test_hooks.force_main_outcome(
		session.impl_->test_hooks.context, outcome
	);
}

std::optional<ClassicStorySessionErrorCode>
ClassicStorySessionTestAccess::failure_code(
	const ClassicStorySession &session
) noexcept
{
	return session.impl_->failure ?
		std::optional{session.impl_->failure->code} : std::nullopt;
}

std::optional<std::uint8_t>
ClassicStorySessionTestAccess::failure_detail_code(
	const ClassicStorySession &session
) noexcept
{
	if(!session.impl_->failure) {
		return std::nullopt;
	}
	const auto &error = *session.impl_->failure;
	if(error.stage_error) {
		return static_cast<std::uint8_t>(error.stage_error->code);
	}
	if(error.main_error) {
		return static_cast<std::uint8_t>(error.main_error->code);
	}
	if(error.win_error) {
		return static_cast<std::uint8_t>(error.win_error->code);
	}
	if(error.cutscene_error) {
		return static_cast<std::uint8_t>(error.cutscene_error->code);
	}
	if(error.registration_error) {
		return static_cast<std::uint8_t>(error.registration_error->code);
	}
	return std::nullopt;
}

std::uint32_t ClassicStorySession::source_sample_count() const noexcept
{
	return impl_->source_sample_count;
}

const GameInit &ClassicStorySession::resident() const noexcept
{
	return impl_->plan.resident();
}

bool ClassicStorySession::restart_requested() const noexcept
{
	return impl_->restart_requested;
}

} // namespace th03::app
