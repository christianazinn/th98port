#include "classic/classic_story_session_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "main/main_process_internal.hpp"
#include "main/main_process_static_content_internal.hpp"
#include "th03/app/new_classic/replay/new_classic_replay.hpp"

namespace th03::app {
namespace classic_story_session_detail {

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

[[nodiscard]] ClassicStorySessionError cutscene_text_error(
	NewClassicCutsceneTextPackError error
)
{
	auto result = session_error(
		ClassicStorySessionErrorCode::cutscene_text_failed
	);
	result.cutscene_text_error.emplace(std::move(error));
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
	ClassicStorySessionErrorCode code
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
	bool new_classic
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

[[nodiscard]] ClassicCutsceneInput replay_cutscene_input(
	const ClassicCutsceneInput &current,
	const ClassicCutsceneInput &latched,
	bool input_due
) noexcept
{
	if(input_due) {
		return current;
	}
	auto result = latched;
	// PMD advances independently of the once-per-VSync Replay input latch.
	result.audio = current.audio;
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

} // namespace classic_story_session_detail

using namespace classic_story_session_detail;


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
	auto backend = create_classic_story_backend({
		.loader = std::move(shared_loader),
		.static_content = std::move(*static_content),
		.stage_text = std::move(stage_text),
		.staff_compiled = std::move(staff_compiled),
		.score_path = std::move(score_path),
		.legacy_score_path = std::move(legacy_score_path),
		.replay_recorder = replay_recorder,
		.new_classic = replay_recorder != nullptr,
	});
	if(!backend) {
		return std::unexpected(std::move(backend.error()));
	}
	return create_with_backend(
		config, std::move(backend->owner), backend->operations,
		backend->test_hooks, replay_recorder
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
	std::filesystem::path legacy_score_path,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	std::shared_ptr<const NewClassicCutsceneTextCatalog> cutscene_text,
	const NewClassicGameTextCatalog *game_text,
	std::shared_ptr<const NewClassicPicturePatchCatalog> picture_patch
)
{
	if(!loader || !static_content) {
		return std::unexpected(session_error(
			ClassicStorySessionErrorCode::invalid_config
		));
	}
	auto backend = create_classic_story_backend({
		.loader = std::move(loader),
		.static_content = std::move(static_content),
		.stage_text = std::move(stage_text),
		.staff_compiled = std::move(staff_compiled),
		.score_path = std::move(score_path),
		.legacy_score_path = std::move(legacy_score_path),
		.replay_recorder = replay_recorder,
		.patch_assets = std::move(patch_assets),
		.cutscene_text = std::move(cutscene_text),
		.game_text = game_text,
		.picture_patch = std::move(picture_patch),
		.new_classic = replay_recorder != nullptr,
	});
	if(!backend) {
		return std::unexpected(std::move(backend.error()));
	}
	return create_with_backend(
		config, std::move(backend->owner), backend->operations,
		backend->test_hooks, replay_recorder
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
	std::unique_ptr<MainProcessDriver> driver,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	std::shared_ptr<const NewClassicCutsceneTextCatalog> cutscene_text,
	const NewClassicGameTextCatalog *game_text,
	std::shared_ptr<const NewClassicPicturePatchCatalog> picture_patch
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
	auto backend = create_classic_story_backend({
		.loader = std::move(loader),
		.static_content = std::move(static_content),
		.stage_text = std::move(stage_text),
		.staff_compiled = std::move(staff_compiled),
		.score_path = std::move(score_path),
		.legacy_score_path = std::move(legacy_score_path),
		.patch_assets = std::move(patch_assets),
		.cutscene_text = std::move(cutscene_text),
		.game_text = game_text,
		.picture_patch = std::move(picture_patch),
		.new_classic = true,
		.replay_playback = true,
		.replay_resident = process_config.resident,
		.replay_process = std::move(process),
		.replay_driver = std::move(driver),
	});
	if(!backend) {
		return std::unexpected(std::move(backend.error()));
	}
	return create_with_backend(
		process_config, std::move(backend->owner), backend->operations,
		backend->test_hooks, nullptr, true, std::move(*plan),
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
		if(impl_->status == ClassicStorySessionStatus::complete &&
			impl_->operations.checkpoint_score_run) {
			auto checkpoint = impl_->operations.checkpoint_score_run(
				impl_->operations.context
			);
			if(!checkpoint) {
				impl_->fail(std::move(checkpoint.error()));
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

ClassicCutsceneInput ClassicStorySessionTestAccess::replay_cutscene_input(
	const ClassicCutsceneInput &current,
	const ClassicCutsceneInput &latched,
	bool input_due
) noexcept
{
	return th03::app::replay_cutscene_input(current, latched, input_due);
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

std::optional<CanonicalState>
ClassicStorySession::main_canonical_state() const noexcept
{
	return impl_->operations.main_canonical_state ?
		impl_->operations.main_canonical_state(impl_->operations.context) :
		std::nullopt;
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
