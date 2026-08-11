#include "classic_story_session_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "main_process_internal.hpp"
#include "main_process_static_content_internal.hpp"
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
	case ClassicMainlScene::continue_menu:
		return ClassicStorySessionScene::continue_menu;
	case ClassicMainlScene::game_over:
		return ClassicStorySessionScene::game_over;
	case ClassicMainlScene::interstage_cutscene:
	case ClassicMainlScene::high_score_registration:
	case ClassicMainlScene::ending:
	case ClassicMainlScene::extra_ending:
		return ClassicStorySessionScene::external_request;
	case ClassicMainlScene::exit_to_operator:
		return ClassicStorySessionScene::complete;
	}
	return ClassicStorySessionScene::failed;
}

[[nodiscard]] ClassicStoryExternalKind external_kind(
	const ClassicMainlStep &step
) noexcept
{
	switch(step.scene) {
	case ClassicMainlScene::interstage_cutscene:
		return (step.cutscene == ClassicInterstageCutscene::chiyuri) ?
			ClassicStoryExternalKind::interstage_chiyuri :
			ClassicStoryExternalKind::interstage_yumemi;
	case ClassicMainlScene::high_score_registration:
		return ClassicStoryExternalKind::high_score_registration;
	case ClassicMainlScene::ending:
		return ClassicStoryExternalKind::ending;
	case ClassicMainlScene::extra_ending:
		return ClassicStoryExternalKind::extra_ending;
	case ClassicMainlScene::stage_splash:
	case ClassicMainlScene::start_main:
	case ClassicMainlScene::win:
	case ClassicMainlScene::continue_menu:
	case ClassicMainlScene::game_over:
	case ClassicMainlScene::exit_to_operator:
		break;
	}
	return ClassicStoryExternalKind::high_score_registration;
}

std::expected<StepResult, MainProcessError> process_step(
	void *context,
	CoreInput input,
	InputContext input_context
) noexcept
{
	return static_cast<MainProcess *>(context)->step(input, input_context);
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
	std::unique_ptr<MainResourceLoader> loader{};
	std::shared_ptr<const MainProcessStaticContent> static_content{};
	MainlStageTextAssets stage_text{};
	present::Tram blank_tram{};
	std::unique_ptr<ClassicMainlStageTransition> stage{};
	std::unique_ptr<ClassicMainlStageTransition> pending_stage{};
	std::unique_ptr<MainProcess> process{};
	std::unique_ptr<MainProcessDriver> driver{};
	std::unique_ptr<ClassicMainlWinScene> win{};
	std::unique_ptr<ClassicMainlContinueScene> continue_scene{};
	std::optional<ClassicMainlContinueCompletion> continue_completion{};
	std::unique_ptr<ClassicMainlGameOverScene> game_over{};
	GameInit process_start{};

	ClassicStoryBackend(
		std::unique_ptr<MainResourceLoader> loader_,
		std::shared_ptr<const MainProcessStaticContent> static_content_,
		MainlStageTextAssets stage_text_
	) :
		loader(std::move(loader_)),
		static_content(std::move(static_content_)),
		stage_text(std::move(stage_text_))
	{
		blank_tram.fill({
			' ', static_cast<present::TramAttribute>(present::tram_black)
		});
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
		if(stage || win || continue_scene || game_over) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		if(pending_stage) {
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
			ClassicMainlStagePhase::waiting;
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
		return {};
	}

	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	advance_main(CoreInput input)
	{
		if(!driver || !process) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		auto advanced = driver->consume(input);
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
		return ClassicStoryBackendTick{
			.surface_changed = advanced->surface_changed,
			.input_sampled = true,
			.ready = advanced->terminal,
			.process_advanced = true,
			.round_started = advanced->round_started,
			.step_result = advanced->step_result,
		};
	}

	std::expected<ClassicStoryTerminal, ClassicStorySessionError>
	main_terminal()
	{
		if(!driver || !process ||
			driver->status() != MainProcessDriverStatus::terminal ||
			!driver->transition()) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		return ClassicStoryTerminal{
			.process_start = process_start,
			.terminal = process->canonical_state(),
			.transition = *driver->transition(),
		};
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
		win.reset();
		return {};
	}

	std::expected<bool, ClassicStorySessionError> start_continue(
		std::uint8_t remaining_credits
	)
	{
		auto resources = loader->load_mainl_continue(remaining_credits);
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = ClassicMainlContinueScene::create(
			remaining_credits, *resources, static_content->font()
		);
		if(!created) {
			return std::unexpected(continue_error(std::move(created.error())));
		}
		continue_scene = std::move(*created);
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
		driver.reset();
		process.reset();
		return *resolved;
	}

	std::expected<void, ClassicStorySessionError> start_game_over()
	{
		if(!continue_completion || game_over) {
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
		if(!game_over || !game_over->take_completion()) {
			return std::unexpected(session_error(
				ClassicStorySessionErrorCode::backend_contract_failed
			));
		}
		game_over.reset();
		return {};
	}

	std::optional<ClassicAudioCommand> take_audio_command()
	{
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
		} else if(win) {
			raster = &win->raster();
		} else if(continue_scene) {
			raster = &continue_scene->raster();
		} else if(game_over) {
			raster = &game_over->raster();
		}
		if(!raster && process) {
			auto composed = process->compose_shown_surface(output);
			if(!composed) {
				return std::unexpected(surface_error(composed.error()));
			}
			return *composed;
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
		.advance_main = [](void *context, CoreInput input) {
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
		.take_audio_command = [](void *context) {
			return static_cast<ClassicStoryBackend *>(context)->
				take_audio_command();
		},
		.compose_shown_surface = [](void *context, auto output) {
			return static_cast<ClassicStoryBackend *>(context)->compose(output);
		},
	};
}

} // namespace

struct ClassicStorySession::Impl {
	MainStoryConfig base_config{};
	std::shared_ptr<void> backend_owner{};
	ClassicStorySessionBackendOperations operations{};
	ClassicMainlPlan plan{};
	std::size_t cursor = 0U;
	ClassicStorySessionScene scene = ClassicStorySessionScene::failed;
	ClassicStorySessionStatus status = ClassicStorySessionStatus::running;
	std::uint32_t source_sample_count = 0U;
	std::uint64_t next_external_token = 1U;
	std::optional<ClassicStoryExternalRequest> external{};
	bool continue_pending = false;
	std::optional<ClassicStorySessionError> failure{};

	Impl(
		MainStoryConfig config,
		std::shared_ptr<void> owner,
		ClassicStorySessionBackendOperations operations_,
		ClassicMainlPlan plan_
	) :
		base_config(std::move(config)),
		backend_owner(std::move(owner)),
		operations(operations_),
		plan(std::move(plan_))
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
			case ClassicMainlScene::high_score_registration:
			case ClassicMainlScene::ending:
			case ClassicMainlScene::extra_ending: {
				auto request_resident = plan.resident();
				request_resident.global_frame = source_sample_count;
				external = ClassicStoryExternalRequest{
					.token = next_external_token++,
					.kind = external_kind(step),
					.resident = std::move(request_resident),
					.high_score_registration =
						plan.high_score_registration(),
				};
				status = ClassicStorySessionStatus::waiting_for_external;
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
		external.reset();
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
	MainlStageTextAssets stage_text
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
	auto backend = std::make_shared<ClassicStoryBackend>(
		std::move(loader), std::move(*static_content), std::move(stage_text)
	);
	return create_with_backend(
		config,
		std::static_pointer_cast<void>(backend),
		backend_operations(*backend)
	);
}

std::expected<std::unique_ptr<ClassicStorySession>, ClassicStorySessionError>
ClassicStorySession::create(
	const MainStoryConfig &config,
	std::unique_ptr<MainResourceLoader> loader,
	MainCompiledContent compiled_content,
	std::span<const std::byte> font_rom,
	std::span<const std::byte> mainl_executable
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
		std::move(text->assets)
	);
}

std::expected<std::unique_ptr<ClassicStorySession>, ClassicStorySessionError>
ClassicStorySession::create_with_backend(
	const MainStoryConfig &config,
	std::shared_ptr<void> backend_owner,
	ClassicStorySessionBackendOperations operations
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
	auto plan = classic_story_start_plan(config);
	if(!plan) {
		return std::unexpected(plan_error(plan.error()));
	}
	auto impl = std::make_unique<Impl>(
		config, std::move(backend_owner), operations, std::move(*plan)
	);
	impl->source_sample_count = config.resident.global_frame;
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
		impl_->status = ClassicStorySessionStatus::quit_requested;
		return ClassicStorySessionTick{
			.scene = impl_->scene,
			.status = impl_->status,
			.source_sample_count = impl_->source_sample_count,
		};
	}

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
	case ClassicStorySessionScene::external_request:
	case ClassicStorySessionScene::complete:
	case ClassicStorySessionScene::failed:
		break;
	}
	if(!tick) {
		impl_->fail(std::move(tick.error()));
		return std::unexpected(*impl_->failure);
	}
	if(tick->input_sampled) {
		++impl_->source_sample_count;
	}

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
		case ClassicStorySessionScene::external_request:
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
	}

	return ClassicStorySessionTick{
		.scene = impl_->scene,
		.status = impl_->status,
		.source_sample_count = impl_->source_sample_count,
		.surface_changed = tick->surface_changed,
		.input_sampled = tick->input_sampled,
		.process_advanced = tick->process_advanced,
		.round_started = tick->round_started,
		.step_result = tick->step_result,
	};
}

std::expected<void, ClassicStorySessionError>
ClassicStorySession::complete_external(std::uint64_t token)
{
	if(impl_->failure) {
		return std::unexpected(*impl_->failure);
	}
	if(impl_->status != ClassicStorySessionStatus::waiting_for_external ||
		!impl_->external || impl_->external->token != token) {
		return std::unexpected(session_error(
			ClassicStorySessionErrorCode::invalid_external_token
		));
	}
	impl_->external.reset();
	impl_->status = ClassicStorySessionStatus::running;
	auto advanced = impl_->advance_cursor();
	if(!advanced) {
		impl_->fail(std::move(advanced.error()));
		return std::unexpected(*impl_->failure);
	}
	return {};
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

std::uint32_t ClassicStorySession::source_sample_count() const noexcept
{
	return impl_->source_sample_count;
}

const std::optional<ClassicStoryExternalRequest> &
ClassicStorySession::external_request() const noexcept
{
	return impl_->external;
}

} // namespace th03::app
