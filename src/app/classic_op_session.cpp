#include "classic_op_session_internal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "main_process_driver_internal.hpp"
#include "main_process_internal.hpp"
#include "main_process_static_content_internal.hpp"
#include "new_classic_language_internal.hpp"
#include "th03/app/authentic_text.hpp"
#include "th03/app/new_classic_pause.hpp"
#include "th03/app/new_classic_frontend.hpp"
#include "th03/app/new_classic_device_scene.hpp"
#include "th03/app/new_classic_key_config_scene.hpp"
#include "th03/app/new_classic_joy_config_scene.hpp"
#include "th03/app/new_classic_playback.hpp"
#include "th03/app/new_classic_replay.hpp"
#include "th03/app/new_classic_settings_store.hpp"
#include "th03/assets/decode.hpp"
#include "th03/present/source_surface.hpp"

namespace th03::app {
namespace {

[[nodiscard]] ClassicOpSessionError session_error(
	ClassicOpSessionErrorCode code
) noexcept
{
	return {.code = code};
}

[[nodiscard]] ClassicOpSessionError resource_error(ResourceLoadError error)
{
	auto result = session_error(
		ClassicOpSessionErrorCode::resource_load_failed
	);
	result.resource_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError config_error(
	ClassicConfigStoreError error
)
{
	auto result = session_error(
		ClassicOpSessionErrorCode::config_store_failed
	);
	result.config_store_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError new_settings_error(
	NewClassicSettingsStoreError error
)
{
	auto result = session_error(
		ClassicOpSessionErrorCode::config_store_failed
	);
	result.new_settings_store_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError score_error(
	ClassicScoreStoreError error
)
{
	auto result = session_error(
		ClassicOpSessionErrorCode::score_store_failed
	);
	result.score_store_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError new_score_error(
	NewClassicScoreStoreError error
)
{
	auto result = session_error(
		ClassicOpSessionErrorCode::score_store_failed
	);
	result.new_score_store_error.emplace(std::move(error));
	return result;
}

void apply_key_config_autofire(
	GameInit &resident,
	const std::optional<NewClassicKeyConfig> &key_config
) noexcept
{
	if(key_config) {
		resident.autofire_mask = key_config->autofire;
	}
}

[[nodiscard]] ClassicOpSessionError font_error(AuthenticTextError error)
{
	auto result = session_error(ClassicOpSessionErrorCode::font_failed);
	result.font_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError mainl_text_error(
	MainlCompiledTextError error
)
{
	auto result = session_error(
		ClassicOpSessionErrorCode::mainl_text_failed
	);
	result.mainl_text_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError op_text_error(OpCompiledTextError error)
{
	auto result = session_error(ClassicOpSessionErrorCode::op_text_failed);
	result.op_text_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError language_overlay_error(
	NewClassicLanguageOverlayError error
)
{
	auto result = session_error(
		ClassicOpSessionErrorCode::language_overlay_failed
	);
	result.language_overlay_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError main_error(MainProcessError error)
{
	auto result = session_error(ClassicOpSessionErrorCode::main_failed);
	result.main_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError title_error(ClassicOpError error)
{
	auto result = session_error(ClassicOpSessionErrorCode::title_failed);
	result.title_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError music_error(
	ClassicOpMusicRoomError error
)
{
	auto result = session_error(
		ClassicOpSessionErrorCode::music_room_failed
	);
	result.music_room_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError high_score_error(
	ClassicRegistrationError error
)
{
	auto result = session_error(
		ClassicOpSessionErrorCode::high_scores_failed
	);
	result.registration_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError selection_error(
	ClassicOpSelectionError error
)
{
	auto result = session_error(
		ClassicOpSessionErrorCode::selection_failed
	);
	result.selection_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError stage_error(
	ClassicMainlStageError error
)
{
	auto result = session_error(ClassicOpSessionErrorCode::stage_failed);
	result.stage_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError story_error(
	ClassicStorySessionError error
)
{
	auto result = session_error(ClassicOpSessionErrorCode::story_failed);
	result.story_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError win_error(ClassicMainlWinError error)
{
	auto result = session_error(ClassicOpSessionErrorCode::win_failed);
	result.win_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError surface_error(
	present::SourceSurfaceError error
)
{
	auto result = session_error(ClassicOpSessionErrorCode::surface_failed);
	result.surface_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpSessionError replay_capture_error() noexcept
{
	return session_error(ClassicOpSessionErrorCode::backend_contract_failed);
}

[[nodiscard]] bool valid_backend(
	const ClassicOpSessionBackendOperations &operations
) noexcept
{
	return operations.context != nullptr && operations.advance_title &&
		operations.finish_title && operations.resume_from_netplay &&
		operations.start_options &&
		operations.advance_options && operations.finish_options &&
		operations.start_music_room && operations.advance_music_room &&
		operations.finish_music_room && operations.start_high_scores &&
		operations.advance_high_scores && operations.finish_high_scores &&
		operations.start_selection &&
		operations.advance_selection && operations.finish_selection &&
		operations.return_from_canceled_selection &&
		operations.commit_config && operations.start_story &&
		operations.advance_story && operations.finish_story &&
		operations.start_demo_stage && operations.advance_demo_stage &&
		operations.start_demo_main && operations.advance_demo_main &&
		operations.finish_demo_main &&
		operations.start_vs_stage && operations.advance_vs_stage &&
		operations.start_vs_main && operations.advance_vs_main &&
		operations.finish_vs_main && operations.restart_vs_stage &&
		operations.start_vs_win &&
		operations.advance_vs_win && operations.finish_vs_win &&
		operations.start_direct_vs_selection && operations.audio_pending &&
		operations.take_audio_command && operations.compose_shown_surface;
}

[[nodiscard]] std::optional<ClassicOpSelectionMode> selection_mode_for_game(
	std::uint8_t mode
) noexcept
{
	switch(mode) {
	case game_mode::vs_1p_cpu:
		return ClassicOpSelectionMode::vs_1p_cpu;
	case game_mode::vs_1p_2p:
		return ClassicOpSelectionMode::vs_1p_2p;
	case game_mode::vs_cpu_cpu:
		return ClassicOpSelectionMode::vs_cpu_cpu;
	default:
		return std::nullopt;
	}
}

[[nodiscard]] CoreInput normalize_vs_cpu_main_input(
	const GameInit &resident,
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
		switch(resident.game_mode) {
		case game_mode::vs_1p_cpu:
			input.p1 = static_cast<std::uint16_t>(input.p1 | input.system);
			input.p2 = 0U;
			input.charge_mask = static_cast<std::uint8_t>(
				input.charge_mask & 0x01U
			);
			break;
		case game_mode::vs_cpu_cpu:
			if((input.system & 0x3000U) != 0U) {
				input.system = 0x1000U;
			}
			input.p1 = 0U;
			input.p2 = 0U;
			input.charge_mask = 0U;
			break;
		default:
			break;
		}
		return input;
	}
	const auto &physical = *sample.physical_domains;
	input.system = physical.keyboard.system;
	if(context != InputContext::gameplay) {
		return input;
	}
	switch(resident.game_mode) {
	case game_mode::vs_1p_cpu: {
		// input_mode_1p_vs_cpu(): merge input_sp and joystick zero into the
		// human side and clear the CPU side before both player updates.
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
		break;
	}
	case game_mode::vs_1p_2p:
		input.p1 = physical.keyboard.p1;
		input.p2 = physical.keyboard.p2;
		input.charge_mask = physical.keyboard.charge_mask;
		if(physical.first_joystick_present && resident.key_mode == 1U) {
			input.p1 = physical.first_joystick;
			input.p2 = physical.keyboard.system;
			input.charge_mask = static_cast<std::uint8_t>(
				input.charge_mask & 0x02U
			);
		} else if(physical.first_joystick_present && resident.key_mode == 2U) {
			input.p1 = physical.keyboard.system;
			input.p2 = physical.first_joystick;
			input.charge_mask = static_cast<std::uint8_t>(
				input.charge_mask & 0x01U
			);
		}
		break;
	case game_mode::vs_cpu_cpu:
		// input_mode_cpu_vs_cpu(): either keyboard Cancel or OK becomes the
		// single Cancel exit word; all other input_sp bits remain raw.
		if((input.system & 0x3000U) != 0U) {
			input.system = 0x1000U;
		}
		input.p1 = 0U;
		input.p2 = 0U;
		input.charge_mask = 0U;
		break;
	default:
		break;
	}
	return input;
}

[[nodiscard]] bool attract_input_present(
	const ClassicMainInputSample &input
) noexcept
{
	if(!input.physical_domains) {
		const auto &aggregate = input.aggregate;
		return aggregate.p1 != 0U || aggregate.p2 != 0U ||
			aggregate.system != 0U || aggregate.charge_mask != 0U;
	}
	const auto &physical = *input.physical_domains;
	const auto interface_word = static_cast<std::uint16_t>(
		physical.keyboard.system | physical.keyboard.p1 |
		(physical.first_joystick_present ? physical.first_joystick : 0U)
	);
	return interface_word != 0U;
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

[[nodiscard]] constexpr ClassicOpVsSurfaceSource vs_surface_source(
	bool win,
	bool win_completion,
	bool main_process
) noexcept
{
	if(win_completion) {
		return ClassicOpVsSurfaceSource::win_completion;
	}
	if(win) {
		return ClassicOpVsSurfaceSource::win;
	}
	return main_process ? ClassicOpVsSurfaceSource::main_process :
		ClassicOpVsSurfaceSource::none;
}

[[nodiscard]] MainlStageResourceRequest stage_request(
	const GameInit &resident
) noexcept
{
	const auto p1 = main_player_selection_from_paletted(
		resident.paletted_character[0U]
	);
	const auto p2 = main_player_selection_from_paletted(
		resident.paletted_character[1U]
	);
	return {
		.player_one = p1.value_or(MainPlayerSelection{}),
		.player_two = p2.value_or(MainPlayerSelection{}),
		.game_mode = resident.game_mode,
		.story_stage = resident.story_stage,
	};
}

[[nodiscard]] MainProcessDriverOperations process_operations(
	MainProcess &process
) noexcept
{
	return {
		.context = &process,
		.step = [](void *context, CoreInput input,
			InputContext input_context) noexcept {
			return static_cast<MainProcess *>(context)->step(input, input_context);
		},
		.deferred_frame_pending = [](void *context) noexcept {
			return static_cast<MainProcess *>(context)->deferred_frame_pending();
		},
		.finish_deferred_frame = [](
			void *context, DeferredFrameAction action
		) noexcept {
			return static_cast<MainProcess *>(context)->finish_deferred_frame(action);
		},
		.start_next_round = [](void *context) noexcept {
			return static_cast<MainProcess *>(context)->start_next_round();
		},
		.finish_main_process = [](void *context) noexcept {
			return static_cast<MainProcess *>(context)->finish_main_process();
		},
		.session_transition = [](void *context,
			std::optional<ContinueChoice> choice) noexcept {
			return static_cast<MainProcess *>(context)->session_transition(choice);
		},
	};
}

[[nodiscard]] constexpr PixelOracleScene pixel_oracle_scene(
	ClassicOpSessionScene scene
) noexcept
{
	switch(scene) {
	case ClassicOpSessionScene::title_menu: return PixelOracleScene::title;
	case ClassicOpSessionScene::options: return PixelOracleScene::options;
	case ClassicOpSessionScene::key_config: return PixelOracleScene::key_config;
	case ClassicOpSessionScene::replay_browser:
		return PixelOracleScene::replay_browser;
	case ClassicOpSessionScene::replay_playback:
	case ClassicOpSessionScene::story:
		return PixelOracleScene::unknown;
	case ClassicOpSessionScene::music_room:
		return PixelOracleScene::music_room;
	case ClassicOpSessionScene::high_scores:
		return PixelOracleScene::high_score;
	case ClassicOpSessionScene::character_selection:
		return PixelOracleScene::character_select;
	case ClassicOpSessionScene::demo_stage_splash:
	case ClassicOpSessionScene::vs_stage_splash:
		return PixelOracleScene::stage_splash;
	case ClassicOpSessionScene::demo_main:
	case ClassicOpSessionScene::vs_main:
		return PixelOracleScene::gameplay;
	case ClassicOpSessionScene::vs_win: return PixelOracleScene::win;
	case ClassicOpSessionScene::complete:
	case ClassicOpSessionScene::failed:
		return PixelOracleScene::unknown;
	}
	return PixelOracleScene::unknown;
}

} // namespace

struct ClassicOpSession::Impl final {
	std::shared_ptr<void> backend_owner{};
	ClassicOpSessionBackendOperations operations{};
	ClassicOpSessionTestHooks test_hooks{};
	ClassicOpSessionScene scene = ClassicOpSessionScene::title_menu;
	ClassicOpSessionStatus status = ClassicOpSessionStatus::running;
	std::optional<ClassicOpSessionError> failure{};
	bool boundary_ready = false;
	bool clock_rebase_requested = false;
	NewClassicReplayRecorder *replay_recorder = nullptr;
	bool new_classic_practice_route = false;

	Impl(
		std::shared_ptr<void> owner,
		ClassicOpSessionBackendOperations operations_,
		ClassicOpSessionTestHooks test_hooks_,
		NewClassicReplayRecorder *replay_recorder_,
		bool new_classic_practice_route_
	) :
		backend_owner(std::move(owner)),
		operations(operations_),
		test_hooks(test_hooks_),
		replay_recorder(replay_recorder_),
		new_classic_practice_route(new_classic_practice_route_)
	{}

	void fail(ClassicOpSessionError error)
	{
		failure = std::move(error);
		status = ClassicOpSessionStatus::failed;
		scene = ClassicOpSessionScene::failed;
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	transition()
	{
		switch(scene) {
		case ClassicOpSessionScene::title_menu: {
			auto result = operations.finish_title(operations.context);
			if(!result) {
				return std::unexpected(std::move(result.error()));
			}
			switch(result->route) {
			case ClassicOpRoute::story:
				if(result->selection_mode != ClassicOpSelectionMode::story) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				if(auto started = operations.start_selection(
					operations.context, *result->selection_mode
				); !started) {
					return std::unexpected(std::move(started.error()));
				}
				scene = ClassicOpSessionScene::character_selection;
				break;
			case ClassicOpRoute::versus:
				if(!result->selection_mode ||
					*result->selection_mode == ClassicOpSelectionMode::story) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				if(auto started = operations.start_selection(
					operations.context, *result->selection_mode
				); !started) {
					return std::unexpected(std::move(started.error()));
				}
				scene = ClassicOpSessionScene::character_selection;
				break;
			case ClassicOpRoute::netplay:
				if(result->selection_mode) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				if(auto committed = operations.commit_config(operations.context);
					!committed) {
					return std::unexpected(std::move(committed.error()));
				}
				status = ClassicOpSessionStatus::netplay_requested;
				scene = ClassicOpSessionScene::complete;
				break;
			case ClassicOpRoute::options:
				if(auto started = operations.start_options(operations.context);
					!started) {
					return std::unexpected(std::move(started.error()));
				}
				scene = ClassicOpSessionScene::options;
				break;
			case ClassicOpRoute::music_room:
				if(auto started = operations.start_music_room(operations.context);
					!started) {
					return std::unexpected(std::move(started.error()));
				}
				scene = ClassicOpSessionScene::music_room;
				break;
			case ClassicOpRoute::high_scores:
				if(auto committed = operations.commit_config(operations.context);
					!committed) {
					return std::unexpected(std::move(committed.error()));
				}
				if(auto started = operations.start_high_scores(operations.context);
					!started) {
					return std::unexpected(std::move(started.error()));
				}
				scene = ClassicOpSessionScene::high_scores;
				break;
			case ClassicOpRoute::replay:
				if(!operations.start_replay_browser) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				if(auto started = operations.start_replay_browser(
					operations.context
				); !started) {
					return std::unexpected(std::move(started.error()));
				}
				scene = ClassicOpSessionScene::replay_browser;
				break;
			case ClassicOpRoute::demo:
				if(auto committed = operations.commit_config(operations.context);
					!committed) {
					return std::unexpected(std::move(committed.error()));
				}
				if(auto started = operations.start_demo_stage(operations.context);
					!started) {
					return std::unexpected(std::move(started.error()));
				}
				scene = ClassicOpSessionScene::demo_stage_splash;
				break;
			case ClassicOpRoute::quit:
				if(auto committed = operations.commit_config(operations.context);
					!committed) {
					return std::unexpected(std::move(committed.error()));
				}
				status = ClassicOpSessionStatus::complete;
				scene = ClassicOpSessionScene::complete;
				break;
			}
			break;
		}
		case ClassicOpSessionScene::options: {
			auto finished = operations.finish_options(operations.context);
			if(!finished) {
				return std::unexpected(std::move(finished.error()));
			}
			if(*finished) {
				status = ClassicOpSessionStatus::switch_to_classic;
				scene = ClassicOpSessionScene::complete;
				break;
			}
			scene = operations.key_config_active &&
				operations.key_config_active(operations.context) ?
				ClassicOpSessionScene::key_config :
				ClassicOpSessionScene::title_menu;
			break;
		}
		case ClassicOpSessionScene::key_config: {
			if(!operations.finish_key_config) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			auto finished = operations.finish_key_config(operations.context);
			if(!finished) {
				return std::unexpected(std::move(finished.error()));
			}
			scene = *finished ? ClassicOpSessionScene::options :
				ClassicOpSessionScene::title_menu;
			break;
		}
		case ClassicOpSessionScene::replay_browser: {
			if(!operations.finish_replay_browser) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			auto finished = operations.finish_replay_browser(operations.context);
			if(!finished) {
				return std::unexpected(std::move(finished.error()));
			}
			switch(*finished) {
			case ClassicOpSessionReplayResult::back:
				scene = ClassicOpSessionScene::title_menu;
				break;
			case ClassicOpSessionReplayResult::continue_browser:
				break;
			case ClassicOpSessionReplayResult::preparing_playback:
				// Keep the browser owner for one complete black/silent source
				// boundary. The next tick performs hidden reconstruction.
				break;
			case ClassicOpSessionReplayResult::play:
				scene = ClassicOpSessionScene::replay_playback;
				clock_rebase_requested = true;
				break;
			case ClassicOpSessionReplayResult::application_quit:
				status = ClassicOpSessionStatus::quit_requested;
				scene = ClassicOpSessionScene::complete;
				break;
			}
			break;
		}
		case ClassicOpSessionScene::replay_playback: {
			if(!operations.finish_replay_playback) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			auto quit = operations.finish_replay_playback(operations.context);
			if(!quit) {
				return std::unexpected(std::move(quit.error()));
			}
			if(*quit) {
				status = ClassicOpSessionStatus::quit_requested;
				scene = ClassicOpSessionScene::complete;
			} else {
				scene = ClassicOpSessionScene::title_menu;
			}
			break;
		}
		case ClassicOpSessionScene::music_room:
			if(auto finished = operations.finish_music_room(operations.context);
				!finished) {
				return std::unexpected(std::move(finished.error()));
			}
			scene = ClassicOpSessionScene::title_menu;
			break;
		case ClassicOpSessionScene::high_scores:
			if(auto finished = operations.finish_high_scores(operations.context)) {
				switch(*finished) {
				case ClassicOpSessionReplayResult::back:
					scene = ClassicOpSessionScene::title_menu;
					break;
				case ClassicOpSessionReplayResult::continue_browser:
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				case ClassicOpSessionReplayResult::preparing_playback:
					break;
				case ClassicOpSessionReplayResult::play:
					scene = ClassicOpSessionScene::replay_playback;
					clock_rebase_requested = true;
					break;
				case ClassicOpSessionReplayResult::application_quit:
					status = ClassicOpSessionStatus::quit_requested;
					scene = ClassicOpSessionScene::complete;
					break;
				}
			} else {
				return std::unexpected(std::move(finished.error()));
			}
			break;
		case ClassicOpSessionScene::character_selection: {
			auto result = operations.finish_selection(operations.context);
			if(!result) {
				return std::unexpected(std::move(result.error()));
			}
			if(result->outcome == ClassicOpSelectionOutcome::canceled) {
				if(auto returned = operations.return_from_canceled_selection(
					operations.context
				); !returned) {
					return std::unexpected(std::move(returned.error()));
				}
				if(new_classic_practice_route) {
					status = ClassicOpSessionStatus::complete;
					scene = ClassicOpSessionScene::complete;
				} else {
					scene = ClassicOpSessionScene::title_menu;
				}
				break;
			}
			if(auto committed = operations.commit_config(operations.context);
				!committed) {
				return std::unexpected(std::move(committed.error()));
			}
			if(result->mode == ClassicOpSelectionMode::story) {
				if(auto started = operations.start_story(operations.context);
					!started) {
					return std::unexpected(std::move(started.error()));
				}
				scene = ClassicOpSessionScene::story;
			} else {
				if(auto started = operations.start_vs_stage(operations.context);
					!started) {
					return std::unexpected(std::move(started.error()));
				}
				scene = ClassicOpSessionScene::vs_stage_splash;
			}
			break;
		}
		case ClassicOpSessionScene::story:
			if(auto finished = operations.finish_story(operations.context);
				!finished) {
				return std::unexpected(std::move(finished.error()));
			} else if(*finished) {
				scene = ClassicOpSessionScene::story;
				break;
			}
			scene = ClassicOpSessionScene::title_menu;
			break;
		case ClassicOpSessionScene::demo_stage_splash:
			if(auto started = operations.start_demo_main(operations.context);
				!started) {
				return std::unexpected(std::move(started.error()));
			}
			scene = ClassicOpSessionScene::demo_main;
			break;
		case ClassicOpSessionScene::demo_main:
			if(auto finished = operations.finish_demo_main(operations.context);
				!finished) {
				return std::unexpected(std::move(finished.error()));
			}
			scene = ClassicOpSessionScene::title_menu;
			break;
		case ClassicOpSessionScene::vs_stage_splash:
			if(auto started = operations.start_vs_main(operations.context);
				!started) {
				return std::unexpected(std::move(started.error()));
			}
			scene = ClassicOpSessionScene::vs_main;
			break;
		case ClassicOpSessionScene::vs_main: {
			auto terminal = operations.finish_vs_main(operations.context);
			if(!terminal) {
				return std::unexpected(std::move(terminal.error()));
			}
			if(terminal->restart_requested) {
				if(auto restarted = operations.restart_vs_stage(
					operations.context, *terminal
				); !restarted) {
					return std::unexpected(std::move(restarted.error()));
				}
				scene = ClassicOpSessionScene::vs_stage_splash;
			} else if(terminal->terminal.route == 0U) {
				if(auto started = operations.start_direct_vs_selection(
					operations.context
				); !started) {
					return std::unexpected(std::move(started.error()));
				}
				scene = ClassicOpSessionScene::character_selection;
			} else if(terminal->terminal.route == 2U) {
				if(auto started = operations.start_vs_win(
					operations.context, *terminal
				); !started) {
					return std::unexpected(std::move(started.error()));
				}
				scene = ClassicOpSessionScene::vs_win;
			} else {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			break;
		}
		case ClassicOpSessionScene::vs_win:
			if(auto finished = operations.finish_vs_win(operations.context);
				!finished) {
				return std::unexpected(std::move(finished.error()));
			}
			if(auto started = operations.start_direct_vs_selection(
				operations.context
			); !started) {
				return std::unexpected(std::move(started.error()));
			}
			scene = ClassicOpSessionScene::character_selection;
			break;
		case ClassicOpSessionScene::complete:
		case ClassicOpSessionScene::failed:
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		boundary_ready = false;
		return {};
	}
};

struct ClassicOpSessionBackend final {
	std::shared_ptr<MainResourceLoader> loader{};
	MainCompiledContent compiled;
	std::vector<std::byte> font_rom{};
	std::optional<Pc98FontRomProvider> font{};
	MainlStageTextAssets stage_text{};
	MainlStaffCompiledAssets staff_compiled{};
	OpMusicRoomTextImport op_music_labels{};
	std::optional<MainCompiledContent> japanese_compiled{};
	std::optional<MainlStageTextAssets> japanese_stage_text{};
	std::optional<OpMusicRoomTextImport> japanese_op_music_labels{};
	std::map<std::string, ResourceBytes> english_resources{};
	ClassicLanguage active_language = ClassicLanguage::japanese;
	std::shared_ptr<const MainProcessStaticContent> static_content{};
	std::filesystem::path config_path{};
	std::filesystem::path score_path{};
	std::filesystem::path new_classic_score_path{};
	ClassicConfigOptions config{};
	std::optional<NewClassicSettings> new_classic_settings{};
	bool fm_available = true;
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
	NewClassicReplayRecorder *replay_recorder = nullptr;
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets{};
	std::shared_ptr<const NewClassicReplaySaveArt> replay_save_art{};
	std::optional<ClassicContentPaths> content_paths{};
	std::optional<NewClassicKeyConfig> key_config{};
	NewClassicKeyConfig key_defaults{};
	std::optional<NewClassicJoyConfig> joy_config{};
	NewClassicJoyConfig joy_defaults{};
	std::optional<NewClassicDeviceAssignment> device_assignment{};
	NewClassicDeviceAssignment device_defaults{};
	NewClassicControllerCatalogRead controller_catalog = nullptr;
	void *controller_context = nullptr;
	std::uint16_t maximum_key = 0U;
	std::uint16_t capture_cancel_key = 0U;
	NewClassicHostKeyName key_name = nullptr;
	void *key_name_context = nullptr;
	std::vector<NewClassicOnlineCacheDocument> online_boards{};
	std::vector<NewClassicOnlineCacheDocument> online_my_records{};
	bool online_upload_available = false;
	bool online_identity_reset_available = false;
	bool update_available = false;
	std::unique_ptr<NewClassicReplayHub> replay_hub{};
	std::optional<NewClassicReplayHubResult> replay_hub_result{};
	bool replay_hub_refresh_pending = false;
	std::unique_ptr<NewClassicReplayBrowser> replay_browser{};
	std::optional<NewClassicReplayBrowserResult> replay_browser_result{};
	std::optional<formats::ReplayDocument> replay_playback_document{};
	std::size_t replay_playback_checkpoint = 0U;
	bool replay_playback_preparing = false;
	std::unique_ptr<NewClassicReplayPlayback> replay_playback{};
	GameInit replay_return_resident{};
	ClassicOpMusicRoomCarry music_carry{};
	ClassicOpSelectionCarry selection_carry{};

	std::unique_ptr<ClassicOpTitleMenuScene> title{};
	std::optional<ClassicOpCompletion> title_completion{};
	std::unique_ptr<ClassicOpOptionsScene> options{};
	std::optional<ClassicOpOptionsCompletion> options_completion{};
	std::unique_ptr<NewClassicKeyConfigScene> key_config_scene{};
	std::unique_ptr<NewClassicJoyConfigScene> joy_config_scene{};
	std::unique_ptr<NewClassicDeviceScene> device_scene{};
	std::optional<NewClassicKeyConfig> pending_key_config_update{};
	std::optional<NewClassicJoyConfig> pending_joy_config_update{};
	std::optional<NewClassicDeviceAssignment>
		pending_device_assignment_update{};
	std::optional<bool> pending_reduced_flash_update{};
	std::optional<std::uint8_t> pending_master_volume_update{};
	std::optional<bool> pending_mute_when_unfocused_update{};
	std::optional<NewClassicWindowPresentation>
		pending_window_presentation_update{};
	std::optional<std::filesystem::path> pending_open_folder{};
	bool pending_online_identity_reset = false;
	bool pending_update_check = false;
	bool update_check_in_progress = false;
	std::optional<NewClassicUpdateCheckOutcome> update_check_outcome{};
	std::optional<ClassicOpOptionsCompletion> key_config_return_options{};
	std::unique_ptr<ClassicOpMusicRoomScene> music_room{};
	std::optional<ClassicOpMusicRoomCompletion> music_completion{};
	std::unique_ptr<ClassicRegistrationScene> high_scores{};
	std::optional<ClassicRegistrationCompletion> high_score_completion{};
	std::unique_ptr<NewClassicHighScoreMenu> high_score_menu{};
	std::optional<NewClassicOnlineReplayRecord>
		pending_online_replay_download{};
	std::optional<NewClassicOnlineReplayId> active_online_replay_download{};
	std::optional<NewClassicReplayUploadRequest>
		pending_online_replay_upload{};
	std::optional<std::uint8_t> active_online_replay_upload{};
	std::optional<NewClassicOnlineReplayRecord> pending_online_replay_watch{};
	std::optional<NewClassicOnlineReplayId> active_online_replay_watch{};
	std::optional<formats::ReplayDocument> online_replay_playback_document{};
	std::size_t online_replay_playback_checkpoint = 0U;
	bool online_replay_playback_preparing = false;
	bool online_replay_black_published = false;
	std::unique_ptr<ClassicOpCharacterSelection> selection{};
	std::optional<ClassicOpSelectionCompletion> selection_completion{};
	std::unique_ptr<ClassicStorySession> story{};
	std::unique_ptr<ClassicMainlStageTransition> vs_stage{};
	std::unique_ptr<MainProcess> vs_process{};
	std::unique_ptr<MainProcessDriver> vs_driver{};
	std::optional<ClassicOpSessionVsTerminal> vs_terminal{};
	std::optional<NewClassicPauseMenu> vs_pause_menu{};
	std::optional<NewClassicPracticeMenu> practice_menu{};
	std::optional<formats::ReplayPracticeConfig> practice_config{};
	bool vs_restart_requested = false;
	std::unique_ptr<ClassicMainlWinScene> vs_win{};
	std::optional<ClassicMainlWinCompletion> vs_win_completion{};
	GameInit current_main_resident{};
	bool direct_vs_selection = false;
	bool new_classic_practice_route = false;
	std::uint8_t title_extra_unlock_step = 0U;

	std::array<ClassicAudioCommand, 64U> audio_commands{};
	std::size_t audio_head = 0U;
	std::size_t audio_count = 0U;
	present::Tram blank_tram{};

	ClassicOpSessionBackend(
		std::shared_ptr<MainResourceLoader> loader_,
		MainCompiledContent compiled_,
		std::vector<std::byte> font_rom_,
		MainlStageTextAssets stage_text_,
		MainlStaffCompiledAssets staff_compiled_,
		OpMusicRoomTextImport op_music_labels_,
		std::filesystem::path config_path_,
		std::filesystem::path score_path_,
		ClassicConfigOptions config_,
		std::optional<NewClassicSettings> new_classic_settings_,
		bool fm_available_,
		present::KotohimeBombFadeCompatibility compatibility,
		NewClassicReplayRecorder *replay_recorder_,
		bool new_classic_practice_route_,
		std::filesystem::path new_classic_score_path_,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_,
		std::map<std::string, ResourceBytes> english_resources_,
		const ClassicRunnerRequest *runner_request
	) :
		loader(std::move(loader_)),
		compiled(std::move(compiled_)),
		font_rom(std::move(font_rom_)),
		stage_text(std::move(stage_text_)),
		staff_compiled(std::move(staff_compiled_)),
		op_music_labels(std::move(op_music_labels_)),
		english_resources(std::move(english_resources_)),
		config_path(std::move(config_path_)),
		score_path(std::move(score_path_)),
		new_classic_score_path(std::move(new_classic_score_path_)),
		config(config_),
		new_classic_settings(std::move(new_classic_settings_)),
		fm_available(fm_available_),
		kotohime_compatibility(compatibility),
		replay_recorder(replay_recorder_),
		patch_assets(std::move(patch_assets_)),
		new_classic_practice_route(new_classic_practice_route_)
	{
		japanese_compiled = compiled;
		japanese_stage_text = stage_text;
		japanese_op_music_labels = op_music_labels;
		if(runner_request) {
			content_paths = runner_request->content;
			key_config = runner_request->new_classic_key_config;
			key_defaults = runner_request->new_classic_key_defaults;
			joy_config = runner_request->new_classic_joy_config;
			joy_defaults = runner_request->new_classic_joy_defaults;
			device_assignment = runner_request->new_classic_device_assignment;
			device_defaults = runner_request->new_classic_device_defaults;
			controller_catalog = runner_request->new_classic_controller_catalog;
			controller_context = runner_request->new_classic_controller_context;
			maximum_key = runner_request->new_classic_maximum_key;
			capture_cancel_key = runner_request->new_classic_capture_cancel_key;
			key_name = runner_request->new_classic_key_name;
			key_name_context = runner_request->new_classic_key_name_context;
			online_boards = runner_request->new_classic_online_boards;
			online_my_records =
				runner_request->new_classic_online_my_records;
			online_upload_available =
				runner_request->new_classic_online_upload_available;
			online_identity_reset_available =
				runner_request->new_classic_online_identity_reset_available;
			update_available = runner_request->new_classic_update_available;
		}
		if(replay_recorder && new_classic_score_path.empty()) {
			new_classic_score_path =
				score_path.parent_path() / "YUME.NEW.NEM";
		}
		blank_tram.fill({
			' ', static_cast<present::TramAttribute>(present::tram_black)
		});
	}

	[[nodiscard]] std::shared_ptr<const NewClassicCutsceneTextCatalog>
	cutscene_text_for(ClassicLanguage language) const noexcept
	{
		if(!patch_assets || !patch_assets->custom_cutscene_text) {
			return {};
		}
		const bool japanese_catalog =
			patch_assets->custom_cutscene_text->source_profile_fnv1a64() ==
				new_classic_cutscene_original_profile_fnv1a64;
		if((language == ClassicLanguage::japanese) != japanese_catalog) {
			return {};
		}
		return patch_assets->custom_cutscene_text;
	}

	[[nodiscard]] const NewClassicGameTextCatalog *game_text_for(
		ClassicLanguage language
	) const noexcept
	{
		if(patch_assets && patch_assets->custom_game_text) {
			const bool japanese =
				patch_assets->custom_game_text->locale() == "ja-JP";
			if((language == ClassicLanguage::japanese) == japanese) {
				return patch_assets->custom_game_text.get();
			}
		}
		return language == ClassicLanguage::english ?
			&new_classic_builtin_english_game_text() :
			&new_classic_builtin_japanese_game_text();
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	apply_language_profile(ClassicLanguage language)
	{
		if(!japanese_compiled || !japanese_stage_text ||
			!japanese_op_music_labels ||
			(language == ClassicLanguage::english && english_resources.empty())) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(active_language == language) {
			return {};
		}
		if(!loader->switch_authenticated_overrides(
			language == ClassicLanguage::english ? english_resources :
				std::map<std::string, ResourceBytes>{}
		)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		const auto cutscene_text = cutscene_text_for(language);
		if(cutscene_text) {
			auto profile = std::uint64_t{14695981039346656037ULL};
			for(std::size_t index = 0U;
				index < new_classic_cutscene_script_count; ++index) {
				auto loaded = loader->load({
					.role = MainResourceRole::cutscene_script,
					.cutscene_script = static_cast<MainCutsceneScript>(index + 1U),
				});
				if(!loaded) {
					return std::unexpected(resource_error(
						std::move(loaded.error())
					));
				}
				const auto bytes = std::span<const std::byte>{**loaded};
				const auto source = std::span<const std::uint8_t>{
					reinterpret_cast<const std::uint8_t *>(bytes.data()),
					bytes.size(),
				};
				for(const auto value : source) {
					profile ^= value;
					profile *= 1099511628211ULL;
				}
				if(auto replacements =
					cutscene_text->replacement_glyphs(index, source);
					!replacements) {
					ClassicStorySessionError nested{
						.code = ClassicStorySessionErrorCode::cutscene_text_failed,
					};
					nested.cutscene_text_error.emplace(
						std::move(replacements.error())
					);
					return std::unexpected(story_error(std::move(nested)));
				}
			}
			if(profile != cutscene_text->source_profile_fnv1a64()) {
				ClassicStorySessionError nested{
					.code = ClassicStorySessionErrorCode::cutscene_text_failed,
				};
				nested.cutscene_text_error.emplace(
					NewClassicCutsceneTextPackError{
						.code = NewClassicCutsceneTextPackErrorCode::source_profile_mismatch,
					}
				);
				return std::unexpected(story_error(std::move(nested)));
			}
		}
		compiled = *japanese_compiled;
		stage_text = *japanese_stage_text;
		op_music_labels = *japanese_op_music_labels;
		if(const auto *game_text = game_text_for(language)) {
			apply_new_classic_english_text(
				compiled, stage_text, op_music_labels,
				*game_text
			);
		}
		static_content.reset();
		active_language = language;
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	start_local_replay_browser(
		NewClassicReplayBrowserMode mode =
			NewClassicReplayBrowserMode::playback
	)
	{
		if(replay_browser || !patch_assets || !content_paths ||
			content_paths->replay_root.empty()) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto loaded = NewClassicReplayBrowser::load(
			content_paths->replay_root, patch_assets, mode
		);
		if(!loaded) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::replay_browser_failed
			));
		}
		replay_browser = std::move(*loaded);
		replay_browser_result.reset();
		return {};
	}

	[[nodiscard]] bool replay_library_available() const noexcept
	{
		return std::ranges::any_of(
			online_boards, [this](const auto &board) {
				return board.board_category &&
					board.board_category->mode == NewClassicOnlineMode::story &&
					board.board_category->rank == replay_return_resident.rank;
			}
		);
	}

	[[nodiscard]] bool my_records_available() const noexcept
	{
		return std::ranges::any_of(
			online_my_records, [this](const auto &board) {
				return board.board_category &&
					board.board_category->mode == NewClassicOnlineMode::story &&
					board.board_category->rank == replay_return_resident.rank;
			}
		);
	}

	[[nodiscard]] bool replay_hub_available() const noexcept
	{
		return online_upload_available || replay_library_available() ||
			my_records_available();
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	start_replay_hub()
	{
		if(replay_hub || !patch_assets || !replay_hub_available()) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto hub = std::make_unique<NewClassicReplayHub>(
			patch_assets, replay_library_available(), online_upload_available,
			my_records_available()
		);
		if(!hub->valid()) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::replay_browser_failed
			));
		}
		replay_hub = std::move(hub);
		replay_hub_result.reset();
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	start_replay_library(NewClassicOnlineMenuMode mode)
	{
		const auto &source = mode == NewClassicOnlineMenuMode::my_records ?
			online_my_records : online_boards;
		if(high_score_menu || !patch_assets || source.empty() ||
			mode == NewClassicOnlineMenuMode::high_scores) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto menu = std::make_unique<NewClassicHighScoreMenu>(
			source, replay_return_resident.rank, patch_assets, mode
		);
		if(!menu->valid()) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::replay_browser_failed
			));
		}
		high_score_menu = std::move(menu);
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	start_replay_browser()
	{
		if(!title_completion || replay_browser || replay_hub ||
			high_score_menu || !patch_assets) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		replay_return_resident = title_completion->resident();
		auto started = replay_hub_available() ?
			start_replay_hub() : start_local_replay_browser();
		if(!started) {
			return std::unexpected(std::move(started.error()));
		}
		title.reset();
		title_completion.reset();
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	start_initial_replay(
		const formats::ReplayDocument &document,
		std::size_t checkpoint_index
	)
	{
		if(replay_playback || !content_paths || !patch_assets) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto loaded = NewClassicReplayPlayback::load({
			.content = *content_paths,
			.document = document,
			.checkpoint_index = checkpoint_index,
			.kotohime_compatibility = kotohime_compatibility,
			.reduced_flash = config.reduced_flash,
			.language = config.language,
		});
		if(!loaded) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::replay_playback_failed
			));
		}
		replay_playback = std::move(*loaded);
		if(!drain_audio(*replay_playback)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_replay_browser(NewClassicUiInput input)
	{
		if(replay_playback_preparing || online_replay_playback_preparing) {
			return ClassicOpSessionBackendTick{.ready = true};
		}
		if(online_replay_playback_document) {
			if(!high_score_menu ||
				high_score_menu->mode() == NewClassicOnlineMenuMode::high_scores ||
				!high_score_menu->prepare_playback_transition() ||
				!push_audio({.type = ClassicAudioCommandType::stop_song})) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::replay_playback_failed
				));
			}
			online_replay_playback_preparing = true;
			online_replay_black_published = false;
			return ClassicOpSessionBackendTick{
				.surface_changed = true,
				.ready = true,
			};
		}
		if(replay_hub) {
			const bool refreshed = std::exchange(
				replay_hub_refresh_pending, false
			) && replay_hub->refresh_capabilities(
				replay_library_available(), online_upload_available,
				my_records_available()
			);
			const auto result = replay_hub->consume(input);
			replay_hub_result = result;
			switch(result.status) {
			case NewClassicReplayHubStatus::local_selected:
				replay_hub.reset();
				replay_hub_result.reset();
				if(auto started = start_local_replay_browser(
					online_upload_available ? NewClassicReplayBrowserMode::upload :
						NewClassicReplayBrowserMode::playback
				); !started) {
					return std::unexpected(std::move(started.error()));
				}
				return ClassicOpSessionBackendTick{
					.surface_changed = true,
					.input_sampled = true,
				};
			case NewClassicReplayHubStatus::upload_selected:
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			case NewClassicReplayHubStatus::online_selected:
			case NewClassicReplayHubStatus::downloaded_selected: {
				const auto mode = result.status ==
					NewClassicReplayHubStatus::downloaded_selected ?
					NewClassicOnlineMenuMode::downloaded_replays :
					NewClassicOnlineMenuMode::online_replays;
				replay_hub.reset();
				replay_hub_result.reset();
				if(auto started = start_replay_library(mode); !started) {
					return std::unexpected(std::move(started.error()));
				}
				return ClassicOpSessionBackendTick{
					.surface_changed = true,
					.input_sampled = true,
				};
			}
			case NewClassicReplayHubStatus::my_records_selected:
				replay_hub.reset();
				replay_hub_result.reset();
				if(auto started = start_replay_library(
					NewClassicOnlineMenuMode::my_records
				); !started) {
					return std::unexpected(std::move(started.error()));
				}
				return ClassicOpSessionBackendTick{
					.surface_changed = true,
					.input_sampled = true,
				};
			case NewClassicReplayHubStatus::back:
			case NewClassicReplayHubStatus::application_quit:
				return ClassicOpSessionBackendTick{
					.input_sampled = true,
					.ready = true,
				};
			case NewClassicReplayHubStatus::running:
				return ClassicOpSessionBackendTick{
					.surface_changed = refreshed || result.surface_changed,
					.input_sampled = input.up || input.down || input.left ||
						input.right || input.confirm || input.bomb || input.cancel,
				};
			}
		}
		if(high_score_menu && high_score_menu->mode() !=
			NewClassicOnlineMenuMode::high_scores) {
			const auto result = high_score_menu->consume(input);
			if(auto request = high_score_menu->take_replay_download_request()) {
				if(pending_online_replay_download ||
					active_online_replay_download) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				active_online_replay_download = request->replay_id;
				pending_online_replay_download = std::move(*request);
			}
			if(auto request = high_score_menu->take_replay_watch_request()) {
				if(pending_online_replay_watch || active_online_replay_watch) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				active_online_replay_watch = request->replay_id;
				pending_online_replay_watch = std::move(*request);
			}
			if(auto request =
				high_score_menu->take_replay_playback_request()) {
				if(online_replay_playback_document ||
					online_replay_playback_preparing) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				online_replay_playback_checkpoint = request->checkpoint_index;
				online_replay_playback_document = std::move(request->document);
			}
			return ClassicOpSessionBackendTick{
				.surface_changed = result.surface_changed,
				.input_sampled = input.up || input.down || input.left ||
					input.right || input.confirm || input.bomb || input.cancel,
				.ready = result.status == NewClassicHighScoreMenuStatus::back ||
					result.status ==
						NewClassicHighScoreMenuStatus::application_quit,
			};
		}
		if(!replay_browser) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto advanced = replay_browser->consume(input);
		if(!advanced) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::replay_browser_failed
			));
		}
		replay_browser_result = *advanced;
		if(auto request = replay_browser->take_upload_request()) {
			if(pending_online_replay_upload || active_online_replay_upload) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			active_online_replay_upload = request->slot;
			pending_online_replay_upload = std::move(*request);
		}
		return ClassicOpSessionBackendTick{
			.surface_changed = advanced->surface_changed,
			.input_sampled = true,
			.ready = advanced->status != NewClassicReplayBrowserStatus::running,
		};
	}

	[[nodiscard]] std::expected<ClassicOpSessionReplayResult,
		ClassicOpSessionError> finish_replay_browser()
	{
		if(online_replay_playback_preparing) {
			if(!online_replay_black_published) {
				online_replay_black_published = true;
				return ClassicOpSessionReplayResult::preparing_playback;
			}
			if(!online_replay_playback_document || !content_paths) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			auto loaded = NewClassicReplayPlayback::load({
				.content = *content_paths,
				.document = *online_replay_playback_document,
				.checkpoint_index = online_replay_playback_checkpoint,
				.kotohime_compatibility = kotohime_compatibility,
				.reduced_flash = config.reduced_flash,
				.language = config.language,
			});
			if(!loaded) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::replay_playback_failed
				));
			}
			replay_playback = std::move(*loaded);
			if(!drain_audio(*replay_playback)) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
			}
			high_score_menu.reset();
			online_replay_playback_document.reset();
			online_replay_playback_checkpoint = 0U;
			online_replay_playback_preparing = false;
			online_replay_black_published = false;
			active_online_replay_watch.reset();
			return ClassicOpSessionReplayResult::play;
		}
		if(replay_playback_preparing) {
			if(!replay_browser || !replay_browser_result) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			if(!replay_playback_document || !content_paths) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			auto loaded = NewClassicReplayPlayback::load({
				.content = *content_paths,
				.document = *replay_playback_document,
				.checkpoint_index = replay_playback_checkpoint,
				.kotohime_compatibility = kotohime_compatibility,
				.reduced_flash = config.reduced_flash,
				.language = config.language,
			});
			if(!loaded) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::replay_playback_failed
				));
			}
			replay_playback = std::move(*loaded);
			if(!drain_audio(*replay_playback)) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
			}
			replay_playback_document.reset();
			replay_playback_preparing = false;
			replay_browser.reset();
			replay_browser_result.reset();
			return ClassicOpSessionReplayResult::play;
		}
		if(replay_hub) {
			if(!replay_hub_result) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			if(replay_hub_result->status ==
				NewClassicReplayHubStatus::application_quit) {
				return ClassicOpSessionReplayResult::application_quit;
			}
			if(replay_hub_result->status != NewClassicReplayHubStatus::back) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			auto resources = loader->load_op_title_menu();
			if(!resources) {
				return std::unexpected(resource_error(std::move(resources.error())));
			}
			auto returned = ClassicOpTitleMenuScene::return_from_other_screen(
				replay_return_resident, *resources, patch_assets, false
			);
			if(!returned) {
				return std::unexpected(title_error(std::move(returned.error())));
			}
			title = std::move(*returned);
			replay_hub.reset();
			replay_hub_result.reset();
			return ClassicOpSessionReplayResult::back;
		}
		if(high_score_menu && high_score_menu->mode() !=
			NewClassicOnlineMenuMode::high_scores) {
			if(high_score_menu->status() ==
				NewClassicHighScoreMenuStatus::application_quit) {
				return ClassicOpSessionReplayResult::application_quit;
			}
			if(high_score_menu->status() !=
				NewClassicHighScoreMenuStatus::back) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			high_score_menu.reset();
			if(auto started = start_replay_hub(); !started) {
				return std::unexpected(std::move(started.error()));
			}
			return ClassicOpSessionReplayResult::continue_browser;
		}
		if(!replay_browser || !replay_browser_result) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		switch(replay_browser_result->status) {
		case NewClassicReplayBrowserStatus::back: {
			replay_browser.reset();
			replay_browser_result.reset();
			if(replay_hub_available()) {
				if(auto started = start_replay_hub(); !started) {
					return std::unexpected(std::move(started.error()));
				}
				return ClassicOpSessionReplayResult::continue_browser;
			}
			auto resources = loader->load_op_title_menu();
			if(!resources) {
				return std::unexpected(resource_error(std::move(resources.error())));
			}
			auto returned = ClassicOpTitleMenuScene::return_from_other_screen(
				replay_return_resident, *resources, patch_assets, false
			);
			if(!returned) {
				return std::unexpected(title_error(std::move(returned.error())));
			}
			title = std::move(*returned);
			return ClassicOpSessionReplayResult::back;
		}
		case NewClassicReplayBrowserStatus::play_selected: {
			if(audio_count >= audio_commands.size()) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
			}
			const auto checkpoint = replay_browser->selected_checkpoint();
			auto document = replay_browser->take_selected_document();
			if(!document || !content_paths) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			if(!replay_browser->prepare_playback_transition() || !push_audio({
				.type = ClassicAudioCommandType::stop_song,
			})) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::replay_playback_failed
				));
			}
			replay_playback_document = std::move(*document);
			replay_playback_checkpoint = checkpoint;
			replay_playback_preparing = true;
			return ClassicOpSessionReplayResult::preparing_playback;
		}
		case NewClassicReplayBrowserStatus::application_quit:
			return ClassicOpSessionReplayResult::application_quit;
		case NewClassicReplayBrowserStatus::running:
			break;
		}
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_replay_playback(
		ClassicAudioStatus audio,
		NewClassicUiInput input,
		CoreInput live_interface_input,
		bool fast_forward
	)
	{
		if(!replay_playback) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		replay_playback->set_fast_forward(fast_forward);
		bool surface_changed = false;
		bool process_advanced = false;
		std::optional<PixelOracleSourceIdentity> publication_identity{};
		const auto steps = replay_playback->fast_forward_step_count();
		for(std::size_t step_index = 0U; step_index < steps; ++step_index) {
			const auto identity_before =
				replay_playback->pixel_oracle_source_identity();
			auto advanced = replay_playback->advance(
				audio,
				step_index == 0U && input.cancel,
				step_index == 0U && input.app_quit_requested,
				live_interface_input
			);
			if(!advanced) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::replay_playback_failed
				));
			}
			if(advanced->surface_changed) {
				if(advanced->pixel_oracle_source_identity) {
					publication_identity =
						advanced->pixel_oracle_source_identity;
				} else {
				const auto identity_after =
					replay_playback->pixel_oracle_source_identity();
				const bool owner_replaced =
					identity_before.process != identity_after.process ||
					identity_before.scene != identity_after.scene;
				// MAIN publishes after advancing its counters, but the frame still
				// owns replay_frame_io()'s pre-sample identity. A process or scene
				// replacement instead exposes the incoming owner's startup surface.
				publication_identity = owner_replaced ?
					identity_after : identity_before;
				if(owner_replaced) {
					publication_identity->boundary =
						PixelOracleBoundary::scene_enter;
				}
				}
			}
			surface_changed = surface_changed || advanced->surface_changed;
			process_advanced = process_advanced || advanced->process_advanced;
			if(!drain_audio(*replay_playback)) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
			}
			if(advanced->status != NewClassicPlaybackStatus::running) {
				break;
			}
		}
		const bool ready = replay_playback->status() !=
			NewClassicPlaybackStatus::running;
		return ClassicOpSessionBackendTick{
			// A fast-forward batch can publish on an early substep and destroy its
			// final drawable owner on a later terminal substep. The fresh OP title
			// owns the next publication; never ask the runner to compose the now-
			// terminal playback object while pending audio delays that transition.
			.surface_changed = !ready && surface_changed,
			.input_sampled = process_advanced,
			.ready = ready,
			.process_advanced = process_advanced,
			.pixel_oracle_source_identity = ready ? std::nullopt :
				publication_identity,
		};
	}

	[[nodiscard]] std::optional<PixelOracleSourceIdentity>
	pixel_oracle_source_identity() const noexcept
	{
		return replay_playback ? std::optional<PixelOracleSourceIdentity>(
			replay_playback->pixel_oracle_source_identity()
		) : std::nullopt;
	}

	[[nodiscard]] std::expected<bool, ClassicOpSessionError>
	finish_replay_playback()
	{
		if(!replay_playback) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(replay_playback->status() ==
			NewClassicPlaybackStatus::application_quit) {
			replay_playback.reset();
			return true;
		}
		auto resident = replay_playback->take_exit_resident();
		if(!resident) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto returned = build_title(std::move(*resident));
		if(!returned) {
			return std::unexpected(std::move(returned.error()));
		}
		title = std::move(*returned);
		replay_playback.reset();
		return false;
	}

	[[nodiscard]] bool push_audio(ClassicAudioCommand command) noexcept
	{
		if(audio_count >= audio_commands.size()) {
			return false;
		}
		const auto tail = (audio_head + audio_count) % audio_commands.size();
		audio_commands[tail] = command;
		++audio_count;
		return true;
	}

	template <typename Scene>
	[[nodiscard]] bool drain_audio(Scene &scene) noexcept
	{
		while(auto command = scene.take_audio_command()) {
			if(!push_audio(*command)) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	ensure_static_content()
	{
		if(static_content) {
			return {};
		}
		auto resources = loader->load_main_static();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto prepared = MainProcessStaticContentAccess::create(
			*resources, compiled, font_rom
		);
		if(!prepared) {
			auto error = main_error(std::move(prepared.error()));
			error.code = ClassicOpSessionErrorCode::static_content_failed;
			return std::unexpected(std::move(error));
		}
		static_content = std::move(*prepared);
		return {};
	}

	[[nodiscard]] std::expected<
		std::unique_ptr<ClassicOpTitleMenuScene>,
		ClassicOpSessionError
	> build_title(GameInit resident)
	{
		resident.rank = static_cast<std::uint8_t>(config.rank);
		resident.key_mode = static_cast<std::uint8_t>(config.key_mode);
		apply_key_config_autofire(resident, key_config);
		auto resources = loader->load_op_title_menu();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		const auto mode = resident.fast_op_animation == 0U ?
			ClassicOpTitleMode::long_animation : ClassicOpTitleMode::fast_fade;
		auto created = ClassicOpTitleMenuScene::create(
			std::move(resident), mode, *resources, patch_assets
		);
		if(!created) {
			return std::unexpected(title_error(std::move(created.error())));
		}
		if(!drain_audio(**created)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		return std::move(*created);
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> create_title(
		GameInit resident,
		bool reset_process_carry
	)
	{
		if(title || options || key_config_scene || joy_config_scene || device_scene ||
			replay_browser || replay_hub ||
			replay_playback || music_room || high_scores || high_score_menu ||
			selection || story ||
			vs_stage || vs_process || vs_driver || vs_win) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto created = build_title(std::move(resident));
		if(!created) {
			return std::unexpected(std::move(created.error()));
		}
		if(reset_process_carry) {
			music_carry = {};
			selection_carry = {};
		}
		title = std::move(*created);
		title_completion.reset();
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_title(ClassicOpInput input)
	{
		if(!title) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(replay_recorder && replay_recorder->capture_enabled() &&
			title->phase_ == ClassicOpPhase::main_menu &&
			title->menu_input_allowed_ && input.raw_any) {
			const bool expected = (title_extra_unlock_step & 1U) != 0U ?
				input.right : input.left;
			if(expected) {
				++title_extra_unlock_step;
			} else {
				title_extra_unlock_step = input.left ? 1U : 0U;
			}
			if(title_extra_unlock_step >= 4U) {
				title_extra_unlock_step = 0U;
				auto loaded = load_new_classic_score_store(
					new_classic_score_path, score_path
				);
				if(!loaded) {
					return std::unexpected(new_score_error(
						std::move(loaded.error())
					));
				}
				if(!loaded->document.extra_unlocked) {
					const bool play_feedback = config.sfx_enabled;
					if(play_feedback && audio_count + 2U > audio_commands.size()) {
						return std::unexpected(session_error(
							ClassicOpSessionErrorCode::audio_command_pending
						));
					}
					loaded->document.extra_unlocked = true;
					if(auto committed = commit_new_classic_score_store(
						new_classic_score_path, loaded->document
					); !committed) {
						return std::unexpected(new_score_error(
							std::move(committed.error())
						));
					}
					if(play_feedback) {
						static_cast<void>(push_audio({
							.type = ClassicAudioCommandType::load_effects,
						}));
						static_cast<void>(push_audio({
							.type = ClassicAudioCommandType::play_effect,
							.parameter = 8U,
						}));
					}
				}
			}
		}
		auto tick = title->advance(input);
		if(!tick) {
			return std::unexpected(title_error(std::move(tick.error())));
		}
		if(!drain_audio(*title)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		return ClassicOpSessionBackendTick{
			.surface_changed = tick->surface_changed || tick->tram_changed,
			.input_sampled = tick->input_sampled,
			.ready = tick->ready,
		};
	}

	[[nodiscard]] std::expected<ClassicOpSessionTitleResult,
		ClassicOpSessionError> finish_title()
	{
		if(!title_completion) {
			if(!title) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			title_completion = title->take_completion();
		}
		if(!title_completion) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		const ClassicOpSessionTitleResult result{
			.route = title_completion->route(),
			.selection_mode = title_completion->selection_mode(),
		};
		// Every ordinary title route transfers ownership in start_*(). Online
		// Match is the sole product-boundary route without a child scene, so it
		// must suspend the completed title here before resume_from_netplay().
		if(result.route == ClassicOpRoute::netplay) {
			title.reset();
		}
		return result;
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	resume_from_netplay()
	{
		if(!title_completion || title) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto resources = loader->load_op_title_menu();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto returned = ClassicOpTitleMenuScene::return_from_other_screen(
			title_completion->resident(), *resources, patch_assets, false
		);
		if(!returned) {
			return std::unexpected(title_error(std::move(returned.error())));
		}
		title = std::move(*returned);
		title_completion.reset();
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> start_options()
	{
		if(!title_completion || options) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto resources = loader->load_op_title_menu();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = ClassicOpOptionsScene::create_from_op(
			*title_completion, config, fm_available, *resources,
			patch_assets, font ? &*font : nullptr, !english_resources.empty(),
			new_classic_settings ?
				new_classic_settings->master_volume_percent : 100U,
			new_classic_settings ? new_classic_settings->window :
				NewClassicWindowPresentation{},
			new_classic_settings ?
				new_classic_settings->music_volume_percent : 100U,
			new_classic_settings ?
				new_classic_settings->effect_volume_percent : 100U,
			new_classic_settings && new_classic_settings->mute_when_unfocused,
			online_identity_reset_available, NewClassicUpdateUiState{
				.available = update_available,
				.automatic = new_classic_settings ?
					new_classic_settings->automatic_updates : true,
				.check_in_progress = update_check_in_progress,
				.outcome = update_check_outcome,
			}
		);
		if(!created) {
			return std::unexpected(title_error(std::move(created.error())));
		}
		options = std::move(*created);
		title.reset();
		title_completion.reset();
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_options(ClassicOpInput input)
	{
		if(!options) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto tick = options->advance(input);
		if(!tick) {
			auto error = title_error(std::move(tick.error()));
			error.code = ClassicOpSessionErrorCode::options_failed;
			return std::unexpected(std::move(error));
		}
		if(!drain_audio(*options)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		if(tick->open_folder) {
			if(pending_open_folder || !content_paths) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			switch(*tick->open_folder) {
			case ClassicOpOptionsOpenFolder::user_data:
				pending_open_folder = new_classic_score_path.parent_path();
				if(pending_open_folder->empty()) {
					*pending_open_folder = ".";
				}
				break;
			case ClassicOpOptionsOpenFolder::replays:
				pending_open_folder = content_paths->replay_root;
				break;
			}
			if(pending_open_folder->empty()) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
		}
		if(tick->forget_online_identity) {
			if(!online_identity_reset_available ||
				pending_online_identity_reset) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			pending_online_identity_reset = true;
			online_identity_reset_available = false;
		}
		if(tick->check_for_updates) {
			if(!update_available || pending_update_check ||
				update_check_in_progress) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			pending_update_check = true;
			update_check_in_progress = true;
			update_check_outcome.reset();
		}
		if(new_classic_settings && new_classic_settings->master_volume_percent !=
			options->master_volume_percent()) {
			new_classic_settings->master_volume_percent =
				options->master_volume_percent();
			pending_master_volume_update =
				new_classic_settings->master_volume_percent;
		}
		if(new_classic_settings && new_classic_settings->window !=
			options->window_presentation()) {
			new_classic_settings->window = options->window_presentation();
			pending_window_presentation_update = new_classic_settings->window;
		}
		if(new_classic_settings) {
			new_classic_settings->music_volume_percent =
				options->music_volume_percent();
			new_classic_settings->effect_volume_percent =
				options->effect_volume_percent();
			new_classic_settings->automatic_updates =
				options->automatic_updates();
			if(new_classic_settings->mute_when_unfocused !=
				options->mute_when_unfocused()) {
				new_classic_settings->mute_when_unfocused =
					options->mute_when_unfocused();
				pending_mute_when_unfocused_update =
					new_classic_settings->mute_when_unfocused;
			}
		}
		bool language_background_refreshed = false;
		if(tick->phase == ClassicOpOptionsPhase::shrink_window &&
			options->config().language != active_language) {
			if(auto applied = apply_language_profile(options->config().language);
				!applied) {
				return std::unexpected(std::move(applied.error()));
			}
			auto resources = loader->load_op_title_menu();
			if(!resources) {
				return std::unexpected(resource_error(std::move(resources.error())));
			}
			if(auto refreshed = options->refresh_patch_language_background(
				*resources
			); !refreshed) {
				return std::unexpected(title_error(std::move(refreshed.error())));
			}
			language_background_refreshed = true;
		}
		return ClassicOpSessionBackendTick{
			.surface_changed = tick->surface_changed || tick->tram_changed ||
				language_background_refreshed,
			.input_sampled = tick->input_sampled,
			.ready = tick->ready,
		};
	}

	[[nodiscard]] std::expected<bool, ClassicOpSessionError> finish_options()
	{
		if(!options_completion) {
			if(!options) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			options_completion = options->take_completion();
		}
		if(!options_completion) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		config = options_completion->config();
		pending_reduced_flash_update = config.reduced_flash;
		if(new_classic_settings && new_classic_settings->master_volume_percent !=
			options_completion->master_volume_percent()) {
			new_classic_settings->master_volume_percent =
				options_completion->master_volume_percent();
			pending_master_volume_update =
				new_classic_settings->master_volume_percent;
		}
		if(new_classic_settings && new_classic_settings->window !=
			options_completion->window_presentation()) {
			new_classic_settings->window =
				options_completion->window_presentation();
			pending_window_presentation_update = new_classic_settings->window;
		}
		if(new_classic_settings) {
			new_classic_settings->gameplay_ruleset =
				options_completion->resident().gameplay_ruleset;
			if(!replay_recorder || !replay_recorder->set_capture_enabled(
				stock_gameplay_rules(new_classic_settings->gameplay_ruleset)
			)) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			new_classic_settings->music_volume_percent =
				options_completion->music_volume_percent();
			new_classic_settings->effect_volume_percent =
				options_completion->effect_volume_percent();
			if(new_classic_settings->mute_when_unfocused !=
				options_completion->mute_when_unfocused()) {
				new_classic_settings->mute_when_unfocused =
					options_completion->mute_when_unfocused();
				pending_mute_when_unfocused_update =
					new_classic_settings->mute_when_unfocused;
			}
		}
		if(config.language != active_language) {
			if(auto applied = apply_language_profile(config.language); !applied) {
				return std::unexpected(std::move(applied.error()));
			}
		}
		// Native New Classic settings commit when their retained Options owner
		// closes. Classic keeps the original deferred YUME.CFG ownership.
		if(new_classic_settings) {
			if(auto committed = commit_config(); !committed) {
				return std::unexpected(std::move(committed.error()));
			}
		}
		if(options_completion->outcome() ==
			ClassicOpOptionsOutcome::key_config) {
			if(!key_config || !patch_assets || !key_name || maximum_key == 0U) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			key_config_return_options.emplace(std::move(*options_completion));
			auto scene = std::make_unique<NewClassicKeyConfigScene>(
				*key_config, key_defaults, maximum_key, capture_cancel_key,
				key_name, patch_assets, key_name_context
			);
			if(!scene->valid()) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			key_config_scene = std::move(scene);
			options.reset();
			options_completion.reset();
			return false;
		}
		if(options_completion->outcome() ==
			ClassicOpOptionsOutcome::joy_config) {
			if(!joy_config || !patch_assets) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			key_config_return_options.emplace(std::move(*options_completion));
			auto scene = std::make_unique<NewClassicJoyConfigScene>(
				*joy_config, joy_defaults, patch_assets
			);
			if(!scene->valid()) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			joy_config_scene = std::move(scene);
			options.reset();
			options_completion.reset();
			return false;
		}
		if(options_completion->outcome() ==
			ClassicOpOptionsOutcome::device_assignment ||
			options_completion->outcome() ==
				ClassicOpOptionsOutcome::input_test) {
			if(!device_assignment || !patch_assets || !controller_catalog) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			const auto mode = options_completion->outcome() ==
				ClassicOpOptionsOutcome::device_assignment ?
				NewClassicDeviceSceneMode::assignment :
				NewClassicDeviceSceneMode::input_test;
			key_config_return_options.emplace(std::move(*options_completion));
			auto scene = std::make_unique<NewClassicDeviceScene>(
				mode,
				*device_assignment, device_defaults, patch_assets
			);
			if(!scene->valid()) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			device_scene = std::move(scene);
			options.reset();
			options_completion.reset();
			return false;
		}
		if(options_completion->outcome() ==
			ClassicOpOptionsOutcome::switch_classic) {
			options.reset();
			options_completion.reset();
			return true;
		}
		auto resources = loader->load_op_title_menu();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto resident = options_completion->resident();
		apply_key_config_autofire(resident, key_config);
		auto resumed = ClassicOpTitleMenuScene::resume_from_options(
			*options_completion, *resources, patch_assets, std::move(resident)
		);
		if(!resumed) {
			return std::unexpected(title_error(std::move(resumed.error())));
		}
		title = std::move(*resumed);
		options.reset();
		options_completion.reset();
		return false;
	}

	[[nodiscard]] bool key_config_active() const noexcept
	{
		return static_cast<bool>(key_config_scene) ||
			static_cast<bool>(joy_config_scene) ||
			static_cast<bool>(device_scene);
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_key_config(
		NewClassicUiInput input,
		CoreInput logical_input,
		std::optional<std::uint16_t> scancode_pressed,
		bool any_scancode_held
	)
	{
		if(!key_config_scene && !joy_config_scene && !device_scene) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(device_scene) {
			NewClassicControllerCatalog catalog{};
			if(!controller_catalog || !controller_catalog(
				controller_context, catalog
			) || catalog.count > catalog.controllers.size()) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			auto advanced = device_scene->consume(input, catalog, logical_input);
			if(advanced.status == NewClassicDeviceSceneStatus::surface_failed) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			if(advanced.status == NewClassicDeviceSceneStatus::apply_requested) {
				const auto path = config_path.parent_path() / "TH3DEV.CFG";
				auto stored = store_new_classic_device_assignment(
					path, device_scene->assignment()
				);
				device_scene->report_commit(static_cast<bool>(stored));
				if(stored) {
					device_assignment = device_scene->assignment();
				}
				return ClassicOpSessionBackendTick{
					.surface_changed = true,
					.input_sampled = true,
				};
			}
			const bool ready = advanced.status ==
				NewClassicDeviceSceneStatus::applied || advanced.status ==
				NewClassicDeviceSceneStatus::back || advanced.status ==
				NewClassicDeviceSceneStatus::application_quit;
			return ClassicOpSessionBackendTick{
				.surface_changed = advanced.surface_changed,
				.input_sampled = true,
				.ready = ready,
			};
		}
		if(joy_config_scene) {
			auto advanced = joy_config_scene->consume(input);
			if(advanced.status == NewClassicJoyConfigSceneStatus::surface_failed) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			if(advanced.status ==
				NewClassicJoyConfigSceneStatus::apply_requested) {
				const auto joy_path = config_path.parent_path() / "TH3JOY.CFG";
				auto stored = store_new_classic_joy_config(
					joy_path, joy_config_scene->config()
				);
				joy_config_scene->report_commit(static_cast<bool>(stored));
				if(stored) {
					joy_config = joy_config_scene->config();
				}
				return ClassicOpSessionBackendTick{
					.surface_changed = true,
					.input_sampled = true,
				};
			}
			const bool ready = advanced.status ==
				NewClassicJoyConfigSceneStatus::applied ||
				advanced.status == NewClassicJoyConfigSceneStatus::back ||
				advanced.status ==
					NewClassicJoyConfigSceneStatus::application_quit;
			return ClassicOpSessionBackendTick{
				.surface_changed = advanced.surface_changed,
				.input_sampled = true,
				.ready = ready,
			};
		}
		auto advanced = key_config_scene->consume({
			.ui = input,
			.scancode_pressed = scancode_pressed,
			.any_scancode_held = any_scancode_held,
		});
		if(advanced.status == NewClassicKeyConfigSceneStatus::surface_failed) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(advanced.status == NewClassicKeyConfigSceneStatus::apply_requested) {
			const auto key_path = config_path.parent_path() / "TH3KEY.CFG";
			auto stored = store_new_classic_key_config(
				key_path, key_config_scene->config(), maximum_key
			);
			if(stored) {
				config.legacy_autofire =
					key_config_scene->config().autofire != 0U;
				auto config_stored = commit_config();
				stored = config_stored ?
					std::expected<void, NewClassicKeyConfigStoreError>{} :
					std::unexpected(NewClassicKeyConfigStoreError{
						.code = NewClassicKeyConfigStoreErrorCode::write_failed,
					});
			}
			key_config_scene->report_commit(static_cast<bool>(stored));
			if(stored) {
				key_config = key_config_scene->config();
			}
			return ClassicOpSessionBackendTick{
				.surface_changed = true,
				.input_sampled = true,
			};
		}
		const bool ready = advanced.status ==
			NewClassicKeyConfigSceneStatus::applied ||
			advanced.status == NewClassicKeyConfigSceneStatus::back ||
			advanced.status == NewClassicKeyConfigSceneStatus::application_quit;
		return ClassicOpSessionBackendTick{
			.surface_changed = advanced.surface_changed,
			.input_sampled = true,
			.ready = ready,
		};
	}

	[[nodiscard]] std::expected<bool, ClassicOpSessionError>
	finish_key_config()
	{
		if((!key_config_scene && !joy_config_scene && !device_scene) ||
			!key_config_return_options || !patch_assets ||
			!font) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto resources = loader->load_op_title_menu();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto returned = device_scene ?
			ClassicOpOptionsScene::resume_after_device_scene(
				*key_config_return_options, fm_available, *resources, patch_assets,
				&*font, !english_resources.empty(),
				key_config_return_options->master_volume_percent(),
				key_config_return_options->window_presentation(),
				key_config_return_options->music_volume_percent(),
				key_config_return_options->effect_volume_percent(),
				key_config_return_options->mute_when_unfocused(),
				online_identity_reset_available, NewClassicUpdateUiState{
					.available = update_available,
					.automatic = new_classic_settings ?
						new_classic_settings->automatic_updates : true,
					.check_in_progress = update_check_in_progress,
					.outcome = update_check_outcome,
				}
			) : joy_config_scene ?
			ClassicOpOptionsScene::resume_after_joy_config(
				*key_config_return_options, fm_available, *resources, patch_assets,
				&*font, !english_resources.empty(),
				key_config_return_options->master_volume_percent(),
				key_config_return_options->window_presentation(),
				key_config_return_options->music_volume_percent(),
				key_config_return_options->effect_volume_percent(),
				key_config_return_options->mute_when_unfocused(),
				online_identity_reset_available, NewClassicUpdateUiState{
					.available = update_available,
					.automatic = new_classic_settings ?
						new_classic_settings->automatic_updates : true,
					.check_in_progress = update_check_in_progress,
					.outcome = update_check_outcome,
				}
			) : ClassicOpOptionsScene::resume_after_key_config(
				*key_config_return_options, fm_available, *resources, patch_assets,
				&*font, !english_resources.empty(),
				key_config_return_options->master_volume_percent(),
				key_config_return_options->window_presentation(),
				key_config_return_options->music_volume_percent(),
				key_config_return_options->effect_volume_percent(),
				key_config_return_options->mute_when_unfocused(),
				online_identity_reset_available, NewClassicUpdateUiState{
					.available = update_available,
					.automatic = new_classic_settings ?
						new_classic_settings->automatic_updates : true,
					.check_in_progress = update_check_in_progress,
					.outcome = update_check_outcome,
				}
			);
		if(!returned) {
			return std::unexpected(title_error(std::move(returned.error())));
		}
		if(key_config_scene && key_config_scene->status() ==
			NewClassicKeyConfigSceneStatus::applied && key_config) {
			pending_key_config_update = *key_config;
		}
		if(joy_config_scene && joy_config_scene->status() ==
			NewClassicJoyConfigSceneStatus::applied && joy_config) {
			pending_joy_config_update = *joy_config;
		}
		if(device_scene && device_scene->status() ==
			NewClassicDeviceSceneStatus::applied && device_assignment) {
			pending_device_assignment_update = *device_assignment;
		}
		options = std::move(*returned);
		key_config_scene.reset();
		joy_config_scene.reset();
		device_scene.reset();
		key_config_return_options.reset();
		return true;
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> start_music_room()
	{
		if(!title_completion || music_room) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto resources = loader->load_op_music_room();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = ClassicOpMusicRoomScene::create_from_imported_text(
			*title_completion, music_carry, *resources, op_music_labels, *font,
			game_text_for(config.language)
		);
		if(!created) {
			return std::unexpected(music_error(std::move(created.error())));
		}
		if(!drain_audio(**created)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		music_room = std::move(*created);
		title.reset();
		title_completion.reset();
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_music_room(
		const ClassicOpMusicRoomInput &input
	)
	{
		if(!music_room) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto tick = music_room->advance(input);
		if(!tick) {
			return std::unexpected(music_error(std::move(tick.error())));
		}
		if(!drain_audio(*music_room)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		return ClassicOpSessionBackendTick{
			.surface_changed = tick->surface_changed,
			.input_sampled = tick->input_sampled,
			.ready = tick->ready,
		};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	finish_music_room()
	{
		if(!music_completion) {
			if(!music_room) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			music_completion = music_room->take_completion();
		}
		if(!music_completion) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto resources = loader->load_op_title_menu();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto returned = ClassicOpTitleMenuScene::return_from_music_room(
			*music_completion, *resources, patch_assets
		);
		if(!returned) {
			return std::unexpected(title_error(std::move(returned.error())));
		}
		music_carry = music_completion->carry();
		title = std::move(*returned);
		music_room.reset();
		music_completion.reset();
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	start_local_high_scores(bool preserve_hub_return)
	{
		if(!title_completion || high_scores || high_score_menu ||
			title_completion->route() != ClassicOpRoute::high_scores ||
			title_completion->selection_mode()) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		const auto &resident = title_completion->resident();
		if(resident.rank >= 4U) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		const auto next_screen = resident.remaining_credits != 0U ?
			MainlRegistrationNextScreen::continue_menu :
			MainlRegistrationNextScreen::game_over;
		auto resources = loader->load_mainl_registration({
			.rank = static_cast<MainRegistrationRank>(resident.rank + 1U),
			.next_screen = next_screen,
		});
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = replay_recorder ?
			ClassicRegistrationScene::create_new_classic_viewer(
				*title_completion, new_classic_score_path, score_path,
				*resources, stage_text, *font, patch_assets
			) : ClassicRegistrationScene::create_viewer(
				*title_completion, score_path, *resources, stage_text, *font
			);
		if(!created) {
			return std::unexpected(high_score_error(std::move(created.error())));
		}
		if(!drain_audio(**created)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		high_scores = std::move(*created);
		title.reset();
		if(!preserve_hub_return) {
			title_completion.reset();
		}
		return {};
	}

	[[nodiscard]] std::expected<bool, ClassicOpSessionError>
	start_high_score_menu(bool fade_in = true)
	{
		if(!title_completion || !replay_recorder || !patch_assets ||
			high_scores || high_score_menu) {
			return false;
		}
		auto personal_bests = load_new_classic_score_store_read_only(
			new_classic_score_path, score_path
		);
		if(!personal_bests) {
			return std::unexpected(new_score_error(
				std::move(personal_bests.error())
			));
		}
		auto menu = std::make_unique<NewClassicHighScoreMenu>(
			online_boards, title_completion->resident().rank, patch_assets,
			NewClassicOnlineMenuMode::high_scores,
			std::move(personal_bests->document), fade_in
		);
		if(!menu->valid()) {
			return false;
		}
		if(!push_audio({
			.type = ClassicAudioCommandType::load_song,
			.cue = ClassicMusicCue::high_score,
		}) || !push_audio({
			.type = ClassicAudioCommandType::play_song,
			.cue = ClassicMusicCue::high_score,
		})) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		high_score_menu = std::move(menu);
		title.reset();
		return true;
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	start_high_scores()
	{
		auto hub = start_high_score_menu();
		if(!hub) {
			return std::unexpected(std::move(hub.error()));
		}
		if(*hub) {
			return {};
		}
		return start_local_high_scores(false);
	}

	[[nodiscard]] std::expected<bool, ClassicOpSessionError>
	return_local_high_scores_to_menu()
	{
		if(!title_completion) {
			return false;
		}
		high_score_completion = high_scores->take_completion();
		if(!high_score_completion || high_score_completion->entered_place()) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		high_scores.reset();
		high_score_completion.reset();
		auto hub = start_high_score_menu(true);
		if(!hub) {
			return std::unexpected(std::move(hub.error()));
		}
		if(!*hub) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		return true;
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_high_scores(
		ClassicRegistrationInput input,
		ClassicAudioStatus audio
	)
	{
		if(online_replay_playback_preparing) {
			return ClassicOpSessionBackendTick{.ready = true};
		}
		if(online_replay_playback_document) {
			if(!high_score_menu ||
				!high_score_menu->prepare_playback_transition() ||
				!push_audio({.type = ClassicAudioCommandType::stop_song})) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::replay_playback_failed
				));
			}
			online_replay_playback_preparing = true;
			online_replay_black_published = false;
			return ClassicOpSessionBackendTick{
				.surface_changed = true,
				.ready = true,
			};
		}
		if(high_score_menu) {
			const auto result = high_score_menu->consume({
				.up = input.up,
				.down = input.down,
				.left = input.left,
				.right = input.right,
				.confirm = input.ok || input.shot,
				.bomb = input.bomb,
				.cancel = input.cancel,
			});
			if(auto request = high_score_menu->take_replay_download_request()) {
				if(pending_online_replay_download ||
					active_online_replay_download) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				active_online_replay_download = request->replay_id;
				pending_online_replay_download = std::move(*request);
			}
			if(auto request = high_score_menu->take_replay_watch_request()) {
				if(pending_online_replay_watch || active_online_replay_watch) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				active_online_replay_watch = request->replay_id;
				pending_online_replay_watch = std::move(*request);
			}
			if(auto request =
				high_score_menu->take_replay_playback_request()) {
				if(online_replay_playback_document ||
					online_replay_playback_preparing) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				online_replay_playback_checkpoint = request->checkpoint_index;
				online_replay_playback_document = std::move(request->document);
			}
			if(result.status == NewClassicHighScoreMenuStatus::local_selected) {
				high_score_menu.reset();
				if(auto started = start_local_high_scores(true); !started) {
					return std::unexpected(std::move(started.error()));
				}
				return ClassicOpSessionBackendTick{
					.surface_changed = true,
					.input_sampled = true,
				};
			}
			if(result.status == NewClassicHighScoreMenuStatus::back) {
				return ClassicOpSessionBackendTick{
					.input_sampled = true,
					.ready = true,
				};
			}
			if(result.status ==
				NewClassicHighScoreMenuStatus::application_quit) {
				return ClassicOpSessionBackendTick{.ready = true};
			}
			return ClassicOpSessionBackendTick{
				.surface_changed = result.surface_changed,
				.input_sampled = input.any(),
			};
		}
		if(!high_scores) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(high_scores->phase() == ClassicRegistrationPhase::ready) {
			auto returned = return_local_high_scores_to_menu();
			if(!returned) {
				return std::unexpected(std::move(returned.error()));
			}
			if(*returned) {
				return ClassicOpSessionBackendTick{.surface_changed = true};
			}
			return ClassicOpSessionBackendTick{.ready = true};
		}
		auto advanced = high_scores->advance(input, audio);
		if(!advanced) {
			return std::unexpected(high_score_error(
				std::move(advanced.error())
			));
		}
		if(!drain_audio(*high_scores)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		if(advanced->ready) {
			auto returned = return_local_high_scores_to_menu();
			if(!returned) {
				return std::unexpected(std::move(returned.error()));
			}
			if(*returned) {
				return ClassicOpSessionBackendTick{
					.surface_changed = true,
					.input_sampled = advanced->input_sampled,
				};
			}
		}
		return ClassicOpSessionBackendTick{
			.surface_changed = advanced->surface_changed,
			.input_sampled = advanced->input_sampled,
			.ready = advanced->ready,
		};
	}

	[[nodiscard]] std::expected<ClassicOpSessionReplayResult,
		ClassicOpSessionError>
	finish_high_scores()
	{
		if(online_replay_playback_preparing) {
			if(!online_replay_black_published) {
				online_replay_black_published = true;
				return ClassicOpSessionReplayResult::preparing_playback;
			}
			if(!online_replay_playback_document || !content_paths) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			auto loaded = NewClassicReplayPlayback::load({
				.content = *content_paths,
				.document = *online_replay_playback_document,
				.checkpoint_index = online_replay_playback_checkpoint,
				.kotohime_compatibility = kotohime_compatibility,
				.reduced_flash = config.reduced_flash,
				.language = config.language,
			});
			if(!loaded) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::replay_playback_failed
				));
			}
			replay_playback = std::move(*loaded);
			if(!drain_audio(*replay_playback)) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
			}
			high_score_menu.reset();
			title_completion.reset();
			online_replay_playback_document.reset();
			online_replay_playback_checkpoint = 0U;
			online_replay_playback_preparing = false;
			online_replay_black_published = false;
			active_online_replay_watch.reset();
			return ClassicOpSessionReplayResult::play;
		}
		if(high_score_menu) {
			if(high_score_menu->status() ==
				NewClassicHighScoreMenuStatus::application_quit) {
				return ClassicOpSessionReplayResult::application_quit;
			}
			if(high_score_menu->status() !=
				NewClassicHighScoreMenuStatus::back || !title_completion) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			auto resources = loader->load_op_title_menu();
			if(!resources) {
				return std::unexpected(resource_error(std::move(resources.error())));
			}
			auto returned = ClassicOpTitleMenuScene::return_from_other_screen(
				title_completion->resident(), *resources, patch_assets, true
			);
			if(!returned) {
				return std::unexpected(title_error(std::move(returned.error())));
			}
			if(!drain_audio(**returned)) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
			}
			title = std::move(*returned);
			high_score_menu.reset();
			title_completion.reset();
			music_carry = {};
			selection_carry = {};
			return ClassicOpSessionReplayResult::back;
		}
		if(!high_score_completion) {
			if(!high_scores) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			high_score_completion = high_scores->take_completion();
		}
		if(!high_score_completion || high_score_completion->entered_place()) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto resources = loader->load_op_title_menu();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto returned = ClassicOpTitleMenuScene::return_from_other_screen(
			high_score_completion->resident(), *resources, patch_assets, true
		);
		if(!returned) {
			return std::unexpected(title_error(std::move(returned.error())));
		}
		if(!drain_audio(**returned)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		title = std::move(*returned);
		high_scores.reset();
		high_score_completion.reset();
		music_carry = {};
		selection_carry = {};
		return ClassicOpSessionReplayResult::back;
	}

	[[nodiscard]] std::expected<std::uint8_t, ClassicOpSessionError>
	available_character_count()
	{
		if(replay_recorder) {
			auto loaded = load_new_classic_score_store(
				new_classic_score_path, score_path
			);
			if(!loaded) {
				return std::unexpected(new_score_error(std::move(loaded.error())));
			}
			return static_cast<std::uint8_t>(
				loaded->document.extra_unlocked ? 9U : 7U
			);
		}
		auto loaded = load_classic_score_store(score_path);
		if(!loaded) {
			return std::unexpected(score_error(std::move(loaded.error())));
		}
		for(const auto &board : loaded->document.boards) {
			if(board.cleared == ClassicScoreCleared::cleared) {
				return std::uint8_t{9U};
			}
		}
		return std::uint8_t{7U};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> start_selection(
		ClassicOpSelectionMode mode
	)
	{
		if(!title_completion || selection) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto resources = loader->load_op_character_selection();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto available = available_character_count();
		if(!available) {
			return std::unexpected(std::move(available.error()));
		}
		music_carry.random_seed = std::bit_cast<std::uint32_t>(
			title_completion->resident().resident_rand
		);
		ClassicOpSelectionLocalizedText localized_text{};
		const ClassicOpSelectionLocalizedText *localized_text_pointer = nullptr;
		if(replay_recorder) {
			if(!font || !patch_assets) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			const auto profile = active_language == ClassicLanguage::english ?
				NewClassicLanguageAssetProfile::english :
				NewClassicLanguageAssetProfile::japanese;
			const auto &messages = (patch_assets &&
				patch_assets->custom_messages &&
				patch_assets->custom_messages->asset_profile() == profile) ?
				*patch_assets->custom_messages :
				new_classic_builtin_messages(profile);
			constexpr std::array<NewClassicMessageId, 9U> name_ids{
				NewClassicMessageId::replay_character_reimu,
				NewClassicMessageId::replay_character_mima,
				NewClassicMessageId::replay_character_marisa,
				NewClassicMessageId::replay_character_ellen,
				NewClassicMessageId::replay_character_kotohime,
				NewClassicMessageId::replay_character_kana,
				NewClassicMessageId::replay_character_rikako,
				NewClassicMessageId::replay_character_chiyuri,
				NewClassicMessageId::replay_character_yumemi,
			};
			for(std::size_t character = 0U; character < name_ids.size(); ++character) {
				const auto name = messages.text(name_ids[character]);
				localized_text.character_names[character] = {
					reinterpret_cast<const std::uint8_t *>(name.data()),
					name.size(),
				};
			}
			constexpr std::array<NewClassicMessageId, 3U> stat_ids{
				NewClassicMessageId::selection_stat_speed,
				NewClassicMessageId::selection_stat_power,
				NewClassicMessageId::selection_stat_charge_speed,
			};
			for(std::size_t row = 0U; row < stat_ids.size(); ++row) {
				localized_text.stat_labels[row] = messages.text(stat_ids[row]);
			}
			localized_text.font = &*font;
			localized_text.menu_font = &patch_assets->menu_font;
			localized_text_pointer = &localized_text;
		}
		auto created = ClassicOpCharacterSelection::create_from_op(
			*title_completion, mode, *available, selection_carry, *resources,
			replay_recorder && replay_recorder->capture_enabled() &&
				mode == ClassicOpSelectionMode::vs_1p_cpu,
			localized_text_pointer
		);
		if(!created) {
			return std::unexpected(selection_error(std::move(created.error())));
		}
		if(!drain_audio(**created)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		selection = std::move(*created);
		title.reset();
		title_completion.reset();
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_selection(ClassicOpSelectionInput input)
	{
		if(!selection) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto tick = selection->advance(input);
		if(!tick) {
			return std::unexpected(selection_error(std::move(tick.error())));
		}
		if(!drain_audio(*selection)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		return ClassicOpSessionBackendTick{
			.surface_changed = tick->surface_changed,
			.input_sampled = tick->input_sampled,
			.ready = tick->ready,
		};
	}

	[[nodiscard]] std::expected<ClassicOpSessionSelectionResult,
		ClassicOpSessionError> finish_selection()
	{
		if(!selection_completion) {
			if(!selection) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			selection_completion = selection->take_completion();
		}
		if(!selection_completion) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		selection_carry = selection_completion->carry();
		return ClassicOpSessionSelectionResult{
			.outcome = selection_completion->outcome(),
			.mode = selection_completion->mode(),
		};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	return_from_canceled_selection()
	{
		if(!selection_completion ||
			selection_completion->outcome() !=
				ClassicOpSelectionOutcome::canceled) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(new_classic_practice_route) {
			selection.reset();
			direct_vs_selection = false;
			return {};
		}
		std::expected<
			std::unique_ptr<ClassicOpTitleMenuScene>,
			ClassicOpSessionError
		> returned = std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
		if(direct_vs_selection) {
			returned = build_title(selection_completion->resident());
		} else {
			auto resources = loader->load_op_title_menu();
			if(!resources) {
				return std::unexpected(resource_error(
					std::move(resources.error())
				));
			}
			auto created =
				ClassicOpTitleMenuScene::return_from_canceled_selection(
					*selection_completion, *resources, patch_assets
				);
			if(!created) {
				return std::unexpected(title_error(std::move(created.error())));
			}
			if(!drain_audio(**created)) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
			}
			returned = std::move(*created);
		}
		if(!returned) {
			return std::unexpected(std::move(returned.error()));
		}
		title = std::move(*returned);
		selection.reset();
		selection_completion.reset();
		direct_vs_selection = false;
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> commit_config()
	{
		if(new_classic_settings) {
			new_classic_settings->options = config;
			auto committed = commit_new_classic_settings_store(
				config_path, *new_classic_settings
			);
			if(!committed) {
				return std::unexpected(new_settings_error(
					std::move(committed.error())
				));
			}
			return {};
		}
		auto committed = commit_classic_config_store(config_path, config);
		if(!committed) {
			return std::unexpected(config_error(std::move(committed.error())));
		}
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> start_story()
	{
		if(!selection_completion || story ||
			selection_completion->outcome() !=
				ClassicOpSelectionOutcome::selected ||
			selection_completion->mode() != ClassicOpSelectionMode::story) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(auto prepared = ensure_static_content(); !prepared) {
			return std::unexpected(std::move(prepared.error()));
		}
		MainStoryConfig story_config{
			.resident = selection_completion->resident(),
			.game_options = {
				.input_protocol = replay_recorder ?
					InputProtocol::recorded_replay : InputProtocol::live,
				.three_digit_combos = replay_recorder != nullptr,
			},
			.kotohime_compatibility = kotohime_compatibility,
		};
		auto created = ClassicStorySession::create_with_prepared_content(
			story_config, loader, static_content, stage_text, staff_compiled,
			replay_recorder ? new_classic_score_path : score_path,
			replay_recorder, replay_recorder ? score_path : std::filesystem::path{},
			patch_assets,
			cutscene_text_for(config.language),
			game_text_for(config.language)
		);
		if(!created) {
			return std::unexpected(story_error(std::move(created.error())));
		}
		if(!drain_audio(**created)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		story = std::move(*created);
		selection.reset();
		selection_completion.reset();
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_story(
		const ClassicStorySessionInput &input
	)
	{
		if(!story) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto tick = story->consume(input);
		if(!tick) {
			return std::unexpected(story_error(std::move(tick.error())));
		}
		if(!drain_audio(*story)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		return ClassicOpSessionBackendTick{
			.surface_changed = tick->surface_changed,
			.input_sampled = tick->input_sampled,
			.ready = tick->status == ClassicStorySessionStatus::complete,
			.process_advanced = tick->process_advanced,
			.round_started = tick->round_started,
		};
	}

	[[nodiscard]] std::expected<bool, ClassicOpSessionError> finish_story()
	{
		if(!story || story->status() != ClassicStorySessionStatus::complete) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(story->restart_requested()) {
			auto resident = prepare_new_classic_story_restart(story->resident());
			MainStoryConfig story_config{
				.resident = std::move(resident),
				.game_options = {
					.input_protocol = InputProtocol::recorded_replay,
					.three_digit_combos = true,
				},
				.kotohime_compatibility = kotohime_compatibility,
			};
			auto created = ClassicStorySession::create_with_prepared_content(
				story_config, loader, static_content, stage_text, staff_compiled,
				replay_recorder ? new_classic_score_path : score_path,
				replay_recorder,
				replay_recorder ? score_path : std::filesystem::path{},
				patch_assets,
				cutscene_text_for(config.language),
				game_text_for(config.language)
			);
			if(!created) {
				return std::unexpected(story_error(std::move(created.error())));
			}
			if(!drain_audio(**created)) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
			}
			story = std::move(*created);
			return true;
		}
		if(replay_recorder && replay_recorder->recording()) {
			const auto *terminal = replay_recorder->last_terminal();
			const auto reason = terminal && terminal->route == 0U
				? formats::ReplayEndReason::menu_return
				: terminal &&
				terminal->game_mode == game_mode::story &&
				terminal->route == 2U && terminal->story_stage >= 8U
				? formats::ReplayEndReason::complete
				: formats::ReplayEndReason::game_over;
			if(auto finished = replay_recorder->finish(reason, story->resident());
				!finished) {
				return std::unexpected(replay_capture_error());
			}
		}
		auto created = build_title(story->resident());
		if(!created) {
			return std::unexpected(std::move(created.error()));
		}
		music_carry = {};
		selection_carry = {};
		title = std::move(*created);
		title_completion.reset();
		story.reset();
		return false;
	}

	[[nodiscard]] std::optional<ClassicStorySessionScene>
	story_scene_for_route_test() const noexcept
	{
		return story ? std::optional{story->scene()} : std::nullopt;
	}

	[[nodiscard]] std::optional<std::uint32_t>
	story_source_sample_count_for_route_test() const noexcept
	{
		return story ? std::optional{story->source_sample_count()} :
			std::nullopt;
	}

	[[nodiscard]] std::optional<ClassicStorySessionErrorCode>
	story_failure_code_for_route_test() const noexcept
	{
		return story ? ClassicStorySessionTestAccess::failure_code(*story) :
			std::nullopt;
	}

	[[nodiscard]] std::optional<std::uint8_t>
	story_failure_detail_code_for_route_test() const noexcept
	{
		return story ? ClassicStorySessionTestAccess::failure_detail_code(
			*story
		) : std::nullopt;
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	force_story_main_outcome_for_route_test(
		ClassicStoryRouteTestOutcome outcome
	)
	{
		if(!story) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto forced = ClassicStorySessionTestAccess::force_main_outcome(
			*story, outcome
		);
		if(!forced) {
			return std::unexpected(story_error(std::move(forced.error())));
		}
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> start_demo_stage()
	{
		if(!title_completion || vs_stage ||
			title_completion->route() != ClassicOpRoute::demo ||
			title_completion->selection_mode()) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(auto prepared = ensure_static_content(); !prepared) {
			return std::unexpected(std::move(prepared.error()));
		}
		const auto request = stage_request(title_completion->resident());
		auto resources = loader->load_mainl_stage(request);
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = ClassicMainlStageTransition::create_from_demo(
			*title_completion, kotohime_compatibility, *resources,
			stage_text, *font, {
				.input_protocol = InputProtocol::live,
				.three_digit_combos = replay_recorder != nullptr,
			}
		);
		if(!created) {
			return std::unexpected(stage_error(std::move(created.error())));
		}
		current_main_resident = title_completion->resident();
		vs_stage = std::move(*created);
		title.reset();
		title_completion.reset();
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_demo_stage(bool dismiss_pressed)
	{
		return advance_vs_stage(ClassicOpInput{.raw_any = dismiss_pressed});
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> start_demo_main()
	{
		if(!vs_stage || vs_stage->phase() != ClassicMainlStagePhase::ready ||
			vs_process || vs_driver) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		const auto player_one = main_player_selection_from_paletted(
			current_main_resident.paletted_character[0U]
		);
		const auto player_two = main_player_selection_from_paletted(
			current_main_resident.paletted_character[1U]
		);
		if(!player_one || !player_two) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::invalid_config
			));
		}
		if((audio_commands.size() - audio_count) < 2U) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		auto resources = loader->load_main_private(*player_one, *player_two);
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = MainProcessDemoAccess::create_demo(
			*vs_stage, *resources, static_content
		);
		if(!created) {
			return std::unexpected(main_error(std::move(created.error())));
		}
		if(!drain_audio(**created)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		auto driver = MainProcessDriver::create(process_operations(**created));
		if(!driver) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		vs_process = std::move(*created);
		vs_driver = std::move(*driver);
		vs_stage.reset();
		vs_terminal.reset();
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_demo_main(
			const ClassicMainInputSample &input
		)
	{
		ClassicMainInputSample projected{};
		if(attract_input_present(input)) {
			// Source attract mode exits for any physical input. The neutral core's
			// architecture-independent early-exit signal is gameplay Cancel.
			projected.aggregate = {.system = 0x1000U};
		}
		return advance_vs_main(projected);
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> finish_demo_main()
	{
		if(!vs_process || !vs_driver || vs_driver->status() !=
			MainProcessDriverStatus::terminal || !vs_driver->transition()) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		const auto &terminal = vs_process->canonical_state();
		const auto &transition = *vs_driver->transition();
		if(terminal.game_mode != game_mode::demo || terminal.route != 0U ||
			transition.action != SessionAction::exit_to_operator ||
			transition.high_score_registration) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto created = build_title(transition.resident);
		if(!created) {
			return std::unexpected(std::move(created.error()));
		}
		title = std::move(*created);
		music_carry = {};
		selection_carry = {};
		vs_stage.reset();
		vs_process.reset();
		vs_driver.reset();
		vs_terminal.reset();
		current_main_resident = {};
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> start_vs_stage()
	{
		if(!selection_completion || vs_stage ||
			selection_completion->outcome() !=
				ClassicOpSelectionOutcome::selected ||
			selection_completion->mode() == ClassicOpSelectionMode::story) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(new_classic_practice_route ||
			(replay_recorder && replay_recorder->capture_enabled() &&
			 selection_completion->resident().game_mode ==
				game_mode::vs_1p_cpu)) {
			practice_menu.emplace(
				selection_completion->resident(), patch_assets
			);
			direct_vs_selection = false;
			return {};
		}
		if(auto prepared = ensure_static_content(); !prepared) {
			return std::unexpected(std::move(prepared.error()));
		}
		const auto request = stage_request(selection_completion->resident());
		const bool new_classic_input = replay_recorder &&
			selection_completion->resident().game_mode != game_mode::vs_cpu_cpu;
		auto resources = loader->load_mainl_stage(request);
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = ClassicMainlStageTransition::create_from_selection(
			*selection_completion, kotohime_compatibility, *resources,
			stage_text, *font, {
				.input_protocol = new_classic_input ?
					InputProtocol::recorded_replay : InputProtocol::live,
				.three_digit_combos = replay_recorder != nullptr,
			}
		);
		if(!created) {
			return std::unexpected(stage_error(std::move(created.error())));
		}
		current_main_resident = selection_completion->resident();
		vs_stage = std::move(*created);
		selection.reset();
		selection_completion.reset();
		direct_vs_selection = false;
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_vs_stage(ClassicOpInput input)
	{
		if(practice_menu) {
			const NewClassicPracticeInput menu_input{
				.raw_any = input.raw_any,
				.up = input.up,
				.down = input.down,
				.left = input.left,
				.right = input.right,
				.confirm = input.ok || input.shot,
				.cancel = input.cancel,
			};
			auto status = practice_menu->status();
			const bool sampled_from_running =
				status == NewClassicPracticeStatus::running;

			if(status == NewClassicPracticeStatus::start_fade) {
				status = practice_menu->consume(menu_input);
				if(status == NewClassicPracticeStatus::start_fade) {
					return ClassicOpSessionBackendTick{
						.surface_changed =
							practice_menu->consume_surface_changed(),
					};
				}
			} else if(status ==
				NewClassicPracticeStatus::cancel_wait_release) {
				status = practice_menu->consume(menu_input);
				if(status == NewClassicPracticeStatus::cancel_wait_release) {
					return ClassicOpSessionBackendTick{
						.input_sampled = true,
					};
				}
			} else if(status == NewClassicPracticeStatus::running ||
				status == NewClassicPracticeStatus::cancel_frame) {
				// Replay Patch redraws and publishes the retained selector's
				// curves before each Practice Setup input poll. Portraits,
				// names, and the completed-selection HUD are absent.
				if(!selection) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				const auto background =
					selection->advance_new_classic_practice_background();
				if(background == ClassicOpPracticeBackgroundStatus::failed) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				if(background == ClassicOpPracticeBackgroundStatus::waiting) {
					return ClassicOpSessionBackendTick{};
				}
				practice_menu->mark_background_published();
				status = practice_menu->consume(menu_input);
				if(status == NewClassicPracticeStatus::start_fade) {
					return ClassicOpSessionBackendTick{
						.surface_changed =
							practice_menu->consume_surface_changed(),
						.input_sampled = sampled_from_running && input.raw_any,
					};
				}
			}

			if(status == NewClassicPracticeStatus::running ||
				status == NewClassicPracticeStatus::cancel_frame ||
				status == NewClassicPracticeStatus::cancel_wait_release) {
				return ClassicOpSessionBackendTick{
					.surface_changed = true,
					.input_sampled = sampled_from_running && input.raw_any,
				};
			}
			if(status == NewClassicPracticeStatus::canceled) {
				if(!selection_completion) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				practice_menu.reset();
				practice_config.reset();
				selection_completion.reset();
				if(!selection ||
					!selection->resume_from_new_classic_practice()) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				return ClassicOpSessionBackendTick{
					.surface_changed = true,
					.input_sampled = true,
					.return_to_selection = true,
				};
			}
			if(status != NewClassicPracticeStatus::started) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			if(!selection_completion || !font) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			const auto practice_setup = practice_menu->config();
			if(audio_count >= audio_commands.size()) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
			}
			practice_config = practice_menu->practice_active() ?
				std::optional{practice_setup} : std::nullopt;
			if(replay_recorder) {
				if(auto configured = replay_recorder->configure_practice(
					practice_config
				); !configured) {
					return std::unexpected(replay_capture_error());
				}
			}
			auto resident = selection ? selection->resident() :
				selection_completion->resident();
			resident.story_stage = practice_setup.stage;
			resident.story_lives = practice_setup.stock == 0xFFU ? 0U :
				practice_setup.stock;
			resident.practice_rules.reset();
			if(auto prepared = ensure_static_content(); !prepared) {
				return std::unexpected(std::move(prepared.error()));
			}
			const auto request = stage_request(resident);
			auto resources = loader->load_mainl_stage(request);
			if(!resources) {
				return std::unexpected(resource_error(std::move(resources.error())));
			}
			auto created = ClassicMainlStageTransition::create_from_restart(
				resident, kotohime_compatibility, *resources, stage_text, *font,
				{
					.input_protocol = InputProtocol::recorded_replay,
					.three_digit_combos = true,
				},
				practice_config
			);
			if(!created) {
				return std::unexpected(stage_error(std::move(created.error())));
			}
			current_main_resident = std::move(resident);
			vs_stage = std::move(*created);
			if(!push_audio({
				.type = ClassicAudioCommandType::stop_song,
				.cue = ClassicMusicCue::character_select,
			})) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
			}
			practice_menu.reset();
			selection.reset();
			selection_completion.reset();
			return ClassicOpSessionBackendTick{
				.surface_changed = true,
				.input_sampled = true,
			};
		}
		if(!vs_stage) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto tick = vs_stage->advance(input.raw_any);
		if(!tick) {
			return std::unexpected(stage_error(std::move(tick.error())));
		}
		if(!drain_audio(*vs_stage)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		return ClassicOpSessionBackendTick{
			.surface_changed = true,
			.input_sampled = tick->input_accepted,
			.ready = tick->handoff_ready,
		};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> start_vs_main()
	{
		if(!vs_stage || vs_stage->phase() != ClassicMainlStagePhase::ready ||
			vs_process || vs_driver) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		const auto player_one = main_player_selection_from_paletted(
			current_main_resident.paletted_character[0U]
		);
		const auto player_two = main_player_selection_from_paletted(
			current_main_resident.paletted_character[1U]
		);
		if(!player_one || !player_two) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::invalid_config
			));
		}
		if((audio_commands.size() - audio_count) < 2U) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		auto resources = loader->load_main_private(*player_one, *player_two);
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = MainProcessClassicVsAccess::create_vs(
			*vs_stage, *resources, static_content
		);
		if(!created) {
			return std::unexpected(main_error(std::move(created.error())));
		}
		if(!drain_audio(**created)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		auto driver = MainProcessDriver::create(process_operations(**created));
		if(!driver) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		vs_process = std::move(*created);
		current_main_resident = vs_process->round_start_observation().start;
		vs_driver = std::move(*driver);
		vs_stage.reset();
		vs_terminal.reset();
		vs_pause_menu.reset();
		vs_restart_requested = false;
		if(replay_recorder && replay_recorder->capture_enabled() &&
			current_main_resident.game_mode !=
			game_mode::vs_cpu_cpu) {
			if(auto checkpoint = replay_recorder->round_started(
				vs_process->round_start_observation()
			); !checkpoint) {
				return std::unexpected(replay_capture_error());
			}
		}
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_vs_main(
			const ClassicMainInputSample &input
		)
	{
		if(vs_terminal) {
			if(!push_audio({
				.type = ClassicAudioCommandType::stop_song,
			})) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
			}
			return ClassicOpSessionBackendTick{
				.ready = true,
			};
		}
		if(!vs_process || !vs_driver) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(replay_recorder && replay_recorder->recording()) {
			replay_recorder->source_vsync();
		}
		const auto input_context = vs_driver->input_context();
		const auto normalized = normalize_vs_cpu_main_input(
			current_main_resident, input, input_context,
			replay_recorder && replay_recorder->recording()
		);
		const bool source_frame_io = !vs_pause_menu;
		std::array<ClassicAudioCommand, 2U> ui_audio{};
		std::size_t ui_audio_count = 0U;
		std::expected<MainProcessDriverTick, MainProcessDriverError> tick =
			std::unexpected(MainProcessDriverError{});
		if(vs_pause_menu) {
			const auto status = vs_pause_menu->consume(
				project_new_classic_pause_input(
					normalize_replay_interstitial_input(input),
					input.restart_requested
				)
			);
			if(vs_pause_menu->take_rejected_save_effect() && !push_audio({
				.type = ClassicAudioCommandType::play_effect,
				.parameter = 21U,
			})) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
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
				return ClassicOpSessionBackendTick{
					.surface_changed = true,
					.input_sampled = true,
					.process_advanced = true,
				};
			}
			const bool resume = status == NewClassicPauseStatus::resume;
			const bool restart = status == NewClassicPauseStatus::restart;
			if(!new_classic_pause_terminal(status)) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			if((restart || status == NewClassicPauseStatus::discard_exit) &&
				replay_recorder) {
				replay_recorder->discard();
			}
			vs_restart_requested = restart;
			tick = vs_driver->finish_deferred_frame(
				resume ? DeferredFrameAction::resume :
					DeferredFrameAction::exit_session
			);
			vs_pause_menu.reset();
			if(resume) {
				ui_audio[ui_audio_count++] = {
					.type = ClassicAudioCommandType::resume_song,
				};
				if(replay_recorder && current_main_resident.game_mode !=
					game_mode::vs_cpu_cpu) {
					ui_audio[ui_audio_count++] = {
						.type = ClassicAudioCommandType::play_effect,
						.parameter = 21U,
					};
				}
			}
		} else {
			tick = vs_driver->consume(normalized);
			if(tick && vs_driver->deferred_frame_pending()) {
				vs_pause_menu.emplace(
					replay_recorder && replay_recorder->recording(), patch_assets
				);
				ui_audio[ui_audio_count++] = ClassicAudioCommand{
					.type = ClassicAudioCommandType::pause_song,
				};
				if(replay_recorder && current_main_resident.game_mode !=
					game_mode::vs_cpu_cpu) {
					ui_audio[ui_audio_count++] = {
						.type = ClassicAudioCommandType::play_effect,
						.parameter = 21U,
					};
				}
			}
		}
		if(!tick) {
			if(tick.error().process_error) {
				return std::unexpected(main_error(
					std::move(*tick.error().process_error)
				));
			}
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		for(std::size_t index = 0U; index < ui_audio_count; ++index) {
			if(!push_audio(ui_audio[index])) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::audio_command_pending
				));
			}
		}
		if(!drain_audio(*vs_process)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		if(replay_recorder && replay_recorder->recording() &&
			source_frame_io && input_context == InputContext::gameplay &&
			tick->process_advanced) {
			replay_recorder->frame_io(
				tick->step_result, !vs_driver->deferred_frame_pending()
			);
		}
		if(replay_recorder && replay_recorder->recording() &&
			tick->process_advanced) {
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
			if(tick->step_result.status == StepStatus::round_transition ||
				tick->step_result.status == StepStatus::terminal) {
				if(!tick->step_result.completed_round_state) {
					return std::unexpected(session_error(
						ClassicOpSessionErrorCode::backend_contract_failed
					));
				}
				if(auto split = replay_recorder->round_completed(
					*tick->step_result.completed_round_state, std::nullopt,
					vs_process->last_result_state()
				); !split) {
					return std::unexpected(replay_capture_error());
				}
			}
			if(tick->round_started) {
				if(auto checkpoint = replay_recorder->round_started(
					vs_process->round_start_observation()
				); !checkpoint) {
					return std::unexpected(replay_capture_error());
				}
			}
		}
		return ClassicOpSessionBackendTick{
			.surface_changed = tick->surface_changed,
			.input_sampled = tick->process_advanced,
			.ready = tick->terminal,
			.process_advanced = tick->process_advanced,
			.round_started = tick->round_started,
		};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	force_vs_main_outcome_for_route_test(
		ClassicOpVsRouteTestOutcome outcome
	)
	{
		if(!vs_process || !vs_driver || vs_terminal ||
			vs_driver->status() != MainProcessDriverStatus::running ||
			current_main_resident.game_mode < game_mode::vs_1p_cpu ||
			current_main_resident.game_mode > game_mode::vs_cpu_cpu ||
			audio_pending()) {
			return std::unexpected(session_error(
				audio_pending() ?
					ClassicOpSessionErrorCode::audio_command_pending :
					ClassicOpSessionErrorCode::backend_contract_failed
			));
		}

		auto terminal = vs_process->canonical_state();
		// Arm only after a real process frame. This keeps the route seam from
		// replacing MAIN construction or its first source publication.
		if(terminal.global_frame == current_main_resident.global_frame) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		terminal.route = outcome ==
			ClassicOpVsRouteTestOutcome::exit_to_operator ? 0U : 2U;
		terminal.game_mode = current_main_resident.game_mode;
		terminal.story_stage = current_main_resident.story_stage;
		terminal.winner = outcome ==
			ClassicOpVsRouteTestOutcome::player_two_win ? 1 : 0;
		terminal.p1_score.fill(8U);
		terminal.p2_score.fill(7U);
		// finish_main_process() performs this source write before constructing
		// the resident transition.
		terminal.resident_rand = static_cast<std::int32_t>(
			terminal.round_frame
		);

		SessionTransition transition{};
		transition.action = SessionAction::exit_to_operator;
		transition.resident = current_main_resident;
		transition.resident.global_frame = terminal.global_frame;
		transition.resident.resident_rand = terminal.resident_rand;
		transition.resident.random_seed = terminal.round_frame;
		transition.resident.winner = terminal.winner;
		transition.resident.runtime_snapshot.reset();
		transition.resident.compact_runtime_snapshot.reset();
		transition.resident.round_start_snapshot.reset();
		if(terminal.route == 2U) {
			const auto winner = static_cast<std::size_t>(terminal.winner);
			transition.resident.last_score = {};
			transition.resident.last_score[winner] = winner == 0U ?
				terminal.p1_score : terminal.p2_score;
			transition.resident.story_stage = static_cast<std::uint8_t>(
				transition.resident.story_stage + 1U
			);
		}
		vs_terminal.emplace(ClassicOpSessionVsTerminal{
			.process_start = current_main_resident,
			.terminal = terminal,
			.transition = std::move(transition),
		});
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionVsTerminal,
		ClassicOpSessionError> finish_vs_main()
	{
		if(!vs_terminal) {
			if(!vs_process || !vs_driver || vs_driver->status() !=
				MainProcessDriverStatus::terminal || !vs_driver->transition()) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			vs_terminal.emplace(ClassicOpSessionVsTerminal{
				.process_start = current_main_resident,
				.terminal = vs_process->canonical_state(),
				.transition = *vs_driver->transition(),
				.restart_requested = vs_restart_requested,
			});
			if(vs_restart_requested &&
				vs_terminal->terminal.route == 0U) {
				preserve_new_classic_restart_random_seed(
					vs_terminal->transition.resident,
					vs_process->live_random_seed()
				);
			}
		}
		if(vs_terminal->terminal.game_mode < game_mode::vs_1p_cpu ||
			vs_terminal->terminal.game_mode > game_mode::vs_cpu_cpu ||
			(vs_terminal->terminal.route != 0U &&
			 vs_terminal->terminal.route != 2U) ||
			vs_terminal->transition.action != SessionAction::exit_to_operator ||
			vs_terminal->transition.high_score_registration) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(replay_recorder && replay_recorder->recording()) {
			const auto *last = replay_recorder->last_terminal();
			if(!last || last->global_frame != vs_terminal->terminal.global_frame ||
				last->round_id != vs_terminal->terminal.round_id) {
				if(auto split = replay_recorder->round_completed(
					vs_terminal->terminal
				); !split) {
					return std::unexpected(replay_capture_error());
				}
			}
			if(auto boundary = replay_recorder->process_completed(
				replay::ProcessId::main
			); !boundary) {
				return std::unexpected(replay_capture_error());
			}
			if(vs_terminal->terminal.route == 0U) {
				if(auto finished = replay_recorder->finish(
					formats::ReplayEndReason::menu_return,
					vs_terminal->transition.resident
				); !finished) {
					return std::unexpected(replay_capture_error());
				}
			}
		}
		return *vs_terminal;
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> restart_vs_stage(
		const ClassicOpSessionVsTerminal &terminal
	)
	{
		if(!terminal.restart_requested || !vs_terminal ||
			!replay_recorder || !font) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto resident = terminal.transition.resident;
		if(practice_config) {
			resident.demo_number = 0U;
			resident.winner = 0;
			resident.story_stage = practice_config->stage;
			resident.story_lives = practice_config->stock == 0xFFU ? 0U :
				practice_config->stock;
			resident.show_score_menu = 0U;
			resident.last_score = {};
			resident.global_frame = 0U;
			resident.runtime_snapshot.reset();
			resident.compact_runtime_snapshot.reset();
			resident.round_start_snapshot.reset();
			resident.practice_rules.reset();
			resident.random_seed = std::bit_cast<std::uint32_t>(
				resident.resident_rand
			);
			if(auto configured = replay_recorder->configure_practice(
				practice_config
			); !configured) {
				return std::unexpected(replay_capture_error());
			}
		} else {
			resident = prepare_new_classic_vs_restart(std::move(resident));
		}
		const auto request = stage_request(resident);
		auto resources = loader->load_mainl_stage(request);
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = ClassicMainlStageTransition::create_from_restart(
			resident, kotohime_compatibility, *resources, stage_text, *font,
			{
				.input_protocol = InputProtocol::recorded_replay,
				.three_digit_combos = true,
			}, practice_config
		);
		if(!created) {
			return std::unexpected(stage_error(std::move(created.error())));
		}
		current_main_resident = std::move(resident);
		vs_stage = std::move(*created);
		vs_process.reset();
		vs_driver.reset();
		vs_terminal.reset();
		vs_pause_menu.reset();
		vs_restart_requested = false;
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> start_vs_win(
		const ClassicOpSessionVsTerminal &terminal
	)
	{
		if(!vs_terminal || terminal.terminal.route != 2U || vs_win ||
			terminal.terminal.winner < 0 || terminal.terminal.winner > 1) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		const auto winner = static_cast<std::size_t>(terminal.terminal.winner);
		const auto loser = 1U - winner;
		const auto winner_selection = main_player_selection_from_paletted(
			terminal.process_start.paletted_character[winner]
		);
		const auto loser_selection = main_player_selection_from_paletted(
			terminal.process_start.paletted_character[loser]
		);
		if(!winner_selection || !loser_selection) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto resources = loader->load_mainl_win(*winner_selection);
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = ClassicMainlWinScene::create(
			{
				.winner_paletted_character =
					terminal.process_start.paletted_character[winner],
				.loser_paletted_character =
					terminal.process_start.paletted_character[loser],
				.message_block = static_cast<std::uint8_t>(
					(static_cast<std::uint8_t>(
						terminal.process_start.paletted_character[loser]
					) - 1U) / 2U
				),
				.preload_stage_splash = false,
			},
			*resources,
			*font,
			game_text_for(config.language)
		);
		if(!created) {
			return std::unexpected(win_error(std::move(created.error())));
		}
		if(!drain_audio(**created)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		vs_win = std::move(*created);
		vs_win_completion.reset();
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_vs_win(
		ClassicAudioStatus audio,
		bool dismiss_pressed
	)
	{
		if(!vs_win) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(vs_win->phase() == ClassicMainlWinPhase::ready) {
			return ClassicOpSessionBackendTick{.ready = true};
		}
		auto tick = vs_win->advance(audio, dismiss_pressed);
		if(!tick) {
			return std::unexpected(win_error(std::move(tick.error())));
		}
		if(tick->stage_preload_requested) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(!drain_audio(*vs_win)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		return ClassicOpSessionBackendTick{
			.surface_changed = tick->surface_changed,
			.input_sampled = tick->input_sampled,
		};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> finish_vs_win()
	{
		if(!vs_win_completion) {
			if(!vs_win) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
			vs_win_completion = vs_win->take_completion();
		}
		if(!vs_win_completion ||
			vs_win_completion->stage_splash_preloaded()) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(replay_recorder && replay_recorder->recording()) {
			if(auto boundary = replay_recorder->process_completed(
				replay::ProcessId::mainl
			); !boundary) {
				return std::unexpected(replay_capture_error());
			}
			if(!vs_terminal || !replay_recorder->finish(
				formats::ReplayEndReason::game_over,
				vs_terminal->transition.resident
			)) {
				return std::unexpected(replay_capture_error());
			}
		}
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	create_direct_vs_selection(GameInit resident)
	{
		if(selection || title) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		const auto mode = selection_mode_for_game(resident.game_mode);
		if(!mode) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(title_completion) {
			if(title_completion->route() != ClassicOpRoute::versus ||
				title_completion->selection_mode() != mode) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
			}
		} else {
			auto title_resources = loader->load_op_title_menu();
			if(!title_resources) {
				return std::unexpected(resource_error(
					std::move(title_resources.error())
				));
			}
			resident.rank = static_cast<std::uint8_t>(config.rank);
			resident.key_mode = static_cast<std::uint8_t>(config.key_mode);
			auto direct = ClassicOpTitleMenuScene::direct_vs_reentry(
				std::move(resident), *mode, *title_resources
			);
			if(!direct) {
				return std::unexpected(title_error(std::move(direct.error())));
			}
			title_completion.emplace(std::move(*direct));
			selection_carry = {};
			music_carry = {};
		}
		auto started = start_selection(*mode);
		if(!started) {
			return std::unexpected(std::move(started.error()));
		}
		direct_vs_selection = true;
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	start_direct_vs_selection()
	{
		if(!vs_terminal ||
			(vs_terminal->terminal.route == 2U && !vs_win_completion)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto started = create_direct_vs_selection(
			vs_terminal->transition.resident
		);
		if(!started) {
			return std::unexpected(std::move(started.error()));
		}
		vs_stage.reset();
		vs_process.reset();
		vs_driver.reset();
		vs_pause_menu.reset();
		vs_win.reset();
		vs_win_completion.reset();
		vs_terminal.reset();
		vs_restart_requested = false;
		return {};
	}

	[[nodiscard]] std::optional<GameInit>
	vs_selection_resident_for_route_test() const noexcept
	{
		if(!direct_vs_selection || !selection) {
			return std::nullopt;
		}
		return selection->resident();
	}

	[[nodiscard]] std::optional<GameInit>
	vs_terminal_resident_for_route_test() const noexcept
	{
		if(!vs_terminal) {
			return std::nullopt;
		}
		return vs_terminal->transition.resident;
	}

	[[nodiscard]] bool online_high_score_menu_for_route_test() const noexcept
	{
		return high_score_menu != nullptr;
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	merge_online_board(NewClassicOnlineCacheDocument document)
	{
		if(!document.board_category ||
			!validate_new_classic_online_cache(document)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		const auto existing = std::ranges::find_if(
			online_boards,
			[&document](const NewClassicOnlineCacheDocument &candidate) {
				return candidate.board_category == document.board_category;
			}
		);
		if(existing == online_boards.end()) {
			online_boards.push_back(std::move(document));
		} else {
			*existing = std::move(document);
		}
		replay_hub_refresh_pending = replay_hub != nullptr;
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	merge_online_my_records(NewClassicOnlineCacheDocument document)
	{
		if(!document.board_category ||
			!validate_new_classic_online_cache(document)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		const auto existing = std::ranges::find_if(
			online_my_records,
			[&document](const NewClassicOnlineCacheDocument &candidate) {
				return candidate.board_category == document.board_category;
			}
		);
		if(existing == online_my_records.end()) {
			online_my_records.push_back(std::move(document));
		} else {
			*existing = std::move(document);
		}
		replay_hub_refresh_pending = replay_hub != nullptr;
		return {};
	}

	void clear_online_my_records() noexcept
	{
		online_my_records.clear();
		replay_hub_refresh_pending = replay_hub != nullptr;
	}

	[[nodiscard]] std::optional<NewClassicOnlineReplayRecord>
	take_online_replay_download_request() noexcept
	{
		return std::exchange(pending_online_replay_download, std::nullopt);
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	report_online_replay_download(
		const NewClassicOnlineReplayId &replay_id,
		NewClassicOnlineReplayDownloadOutcome outcome
	)
	{
		if(!active_online_replay_download ||
			*active_online_replay_download != replay_id) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(outcome == NewClassicOnlineReplayDownloadOutcome::downloaded) {
			for(auto *library : {&online_boards, &online_my_records}) {
				for(auto &board : *library) {
					for(auto &record : board.replay_records) {
						if(record.replay_id == replay_id) {
							record.availability =
								NewClassicOnlineReplayAvailability::downloaded;
						}
					}
				}
			}
		}
		if(high_score_menu && !high_score_menu->report_replay_download(
			replay_id, outcome
		)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		active_online_replay_download.reset();
		return {};
	}

	[[nodiscard]] std::optional<NewClassicReplayUploadRequest>
	take_online_replay_upload_request() noexcept
	{
		return std::exchange(pending_online_replay_upload, std::nullopt);
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	report_online_replay_upload(
		std::uint8_t slot,
		NewClassicReplayUploadOutcome outcome
	)
	{
		if(!active_online_replay_upload ||
			*active_online_replay_upload != slot || !replay_browser ||
			!replay_browser->report_upload(slot, outcome)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		active_online_replay_upload.reset();
		return {};
	}

	[[nodiscard]] std::optional<NewClassicOnlineReplayRecord>
	take_online_replay_watch_request() noexcept
	{
		return std::exchange(pending_online_replay_watch, std::nullopt);
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	report_online_replay_watch(
		const NewClassicOnlineReplayId &replay_id,
		std::optional<formats::ReplayDocument> document
	)
	{
		if(!active_online_replay_watch ||
			*active_online_replay_watch != replay_id ||
			online_replay_playback_document ||
			online_replay_playback_preparing) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		const bool ready = document.has_value();
		if(!high_score_menu || !high_score_menu->report_replay_watch(
			replay_id, std::move(document)
		)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(!ready) {
			for(auto *library : {&online_boards, &online_my_records}) {
				for(auto &board : *library) {
					for(auto &record : board.replay_records) {
						if(record.replay_id == replay_id) {
							record.availability =
								NewClassicOnlineReplayAvailability::metadata_only;
						}
					}
				}
			}
		}
		active_online_replay_watch.reset();
		return {};
	}

	[[nodiscard]] bool audio_pending() const noexcept
	{
		return audio_count != 0U;
	}

	[[nodiscard]] std::optional<ClassicAudioCommand> take_audio_command() noexcept
	{
		if(audio_count == 0U) {
			return std::nullopt;
		}
		const auto result = audio_commands[audio_head];
		audio_head = (audio_head + 1U) % audio_commands.size();
		--audio_count;
		return result;
	}

	[[nodiscard]] std::optional<NewClassicKeyConfig>
	take_key_config_update() noexcept
	{
		return std::exchange(pending_key_config_update, std::nullopt);
	}

	[[nodiscard]] std::optional<NewClassicJoyConfig>
	take_joy_config_update() noexcept
	{
		return std::exchange(pending_joy_config_update, std::nullopt);
	}

	[[nodiscard]] std::optional<NewClassicDeviceAssignment>
	take_device_assignment_update() noexcept
	{
		return std::exchange(pending_device_assignment_update, std::nullopt);
	}

	[[nodiscard]] std::optional<bool> take_reduced_flash_update() noexcept
	{
		return std::exchange(pending_reduced_flash_update, std::nullopt);
	}

	[[nodiscard]] std::optional<std::uint8_t>
	take_master_volume_update() noexcept
	{
		return std::exchange(pending_master_volume_update, std::nullopt);
	}

	[[nodiscard]] std::optional<bool> take_mute_when_unfocused_update() noexcept
	{
		return std::exchange(pending_mute_when_unfocused_update, std::nullopt);
	}

	[[nodiscard]] std::optional<NewClassicWindowPresentation>
	take_window_presentation_update() noexcept
	{
		return std::exchange(
			pending_window_presentation_update, std::nullopt
		);
	}

	[[nodiscard]] std::optional<std::filesystem::path>
	take_open_folder_request() noexcept
	{
		return std::exchange(pending_open_folder, std::nullopt);
	}

	[[nodiscard]] bool take_online_identity_reset_request() noexcept
	{
		return std::exchange(pending_online_identity_reset, false);
	}

	[[nodiscard]] bool take_update_check_request() noexcept
	{
		return std::exchange(pending_update_check, false);
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	begin_update_check()
	{
		if(!update_available || update_check_in_progress) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(options) {
			if(auto begun = options->begin_update_check(); !begun) {
				auto error = title_error(std::move(begun.error()));
				error.code = ClassicOpSessionErrorCode::options_failed;
				return std::unexpected(std::move(error));
			}
		}
		update_check_in_progress = true;
		update_check_outcome.reset();
		return {};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	report_update_check(NewClassicUpdateCheckOutcome outcome)
	{
		if(!update_available || !update_check_in_progress) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(options) {
			if(auto reported = options->report_update_check(outcome); !reported) {
				auto error = title_error(std::move(reported.error()));
				error.code = ClassicOpSessionErrorCode::options_failed;
				return std::unexpected(std::move(error));
			}
		}
		update_check_in_progress = false;
		update_check_outcome = outcome;
		return {};
	}

	[[nodiscard]] std::expected<present::SourceSurface, ClassicOpSessionError>
	compose(std::span<
		present::ComposedPixel,
		present::graphics_page_pixels
	> output)
	{
		if(replay_playback) {
			auto composed = replay_playback->compose_shown_surface(output);
			if(!composed) {
				return std::unexpected(session_error(
					ClassicOpSessionErrorCode::replay_playback_failed
				));
			}
			return *composed;
		}
		if(replay_browser) {
			return replay_browser->surface();
		}
		if(replay_hub) {
			return replay_hub->surface();
		}
		if(key_config_scene) {
			return key_config_scene->surface();
		}
		if(joy_config_scene) {
			return joy_config_scene->surface();
		}
		if(device_scene) {
			return device_scene->surface();
		}
		if(story) {
			auto composed = story->compose_shown_surface(output);
			if(!composed) {
				return std::unexpected(story_error(std::move(composed.error())));
			}
			return *composed;
		}

		const present::Raster *raster = nullptr;
		const present::Tram *tram = &blank_tram;
		switch(vs_surface_source(
			static_cast<bool>(vs_win),
			static_cast<bool>(vs_win_completion),
			static_cast<bool>(vs_process)
		)) {
		case ClassicOpVsSurfaceSource::win_completion:
			raster = &vs_win_completion->raster();
			break;
		case ClassicOpVsSurfaceSource::win:
			raster = &vs_win->raster();
			break;
		case ClassicOpVsSurfaceSource::main_process: {
			auto composed = vs_process->compose_shown_surface(output);
			if(!composed) {
				return std::unexpected(surface_error(composed.error()));
			}
			return vs_pause_menu ?
				vs_pause_menu->compose_over(*composed) : *composed;
		}
		case ClassicOpVsSurfaceSource::none:
			break;
		}

		// HiScore retains [title_completion] as its return capability while the
		// hub and the nested local viewer are live. These active owners must win
		// composition; otherwise the completed title fade (normally black) hides
		// the entire HiScore route until Cancel rebuilds OP.
		if(!raster && high_score_completion) {
			raster = &high_score_completion->raster();
		} else if(!raster && high_score_menu) {
			return high_score_menu->surface();
		} else if(!raster && high_scores) {
			raster = &high_scores->raster();
		} else if(!raster && title_completion) {
			raster = &title_completion->raster();
			tram = &title_completion->tram();
		} else if(!raster && title) {
			raster = &title->raster();
			tram = &title->tram();
		} else if(!raster && options_completion) {
			raster = &options_completion->raster();
			tram = &options_completion->tram();
		} else if(!raster && options) {
			raster = &options->raster();
			tram = &options->tram();
		} else if(!raster && music_completion) {
			raster = &music_completion->raster();
			tram = &music_completion->tram();
		} else if(!raster && music_room) {
			raster = &music_room->raster();
			tram = &music_room->tram();
		} else if(!raster && practice_menu && selection) {
			raster = &selection->raster();
			tram = &selection->tram();
		} else if(!raster && selection_completion) {
			raster = &selection_completion->raster();
			tram = &selection_completion->tram();
		} else if(!raster && selection) {
			raster = &selection->raster();
			tram = &selection->tram();
		} else if(!raster && vs_stage) {
			raster = &vs_stage->raster();
			tram = &vs_stage->tram();
		}
		if(!raster || !font) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto composed = present::compose_source_surface(
			output, *raster, *tram, *font
		);
		if(!composed) {
			return std::unexpected(surface_error(composed.error()));
		}
		return practice_menu ? practice_menu->compose_over(*composed, true) :
			*composed;
	}
};

namespace {

template <typename Result, typename Method, typename... Args>
Result backend_call(void *context, Method method, Args &&...args)
{
	return (static_cast<ClassicOpSessionBackend *>(context)->*method)(
		std::forward<Args>(args)...
	);
}

[[nodiscard]] ClassicOpSessionBackendOperations backend_operations(
	ClassicOpSessionBackend &backend
) noexcept
{
	return {
		.context = &backend,
		.advance_title = [](void *context, ClassicOpInput input) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::advance_title, input);
		},
		.finish_title = [](void *context) {
			return backend_call<std::expected<
				ClassicOpSessionTitleResult, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::finish_title);
		},
		.resume_from_netplay = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::resume_from_netplay
			);
		},
		.start_options = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::start_options
			);
		},
		.advance_options = [](void *context, ClassicOpInput input) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::advance_options, input);
		},
		.finish_options = [](void *context) {
			return backend_call<std::expected<bool, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::finish_options
			);
		},
		.key_config_active = [](void *context) {
			return static_cast<ClassicOpSessionBackend *>(context)->
				key_config_active();
		},
		.advance_key_config = [](
			void *context,
			NewClassicUiInput input,
			CoreInput logical_input,
			std::optional<std::uint16_t> scancode_pressed,
			bool any_scancode_held
		) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(
				context, &ClassicOpSessionBackend::advance_key_config,
				input, logical_input, scancode_pressed, any_scancode_held
			);
		},
		.finish_key_config = [](void *context) {
			return backend_call<std::expected<bool, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::finish_key_config
			);
		},
		.start_replay_browser = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::start_replay_browser
			);
		},
		.advance_replay_browser = [](
			void *context, NewClassicUiInput input
		) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(
				context, &ClassicOpSessionBackend::advance_replay_browser, input
			);
		},
		.finish_replay_browser = [](void *context) {
			return backend_call<std::expected<
				ClassicOpSessionReplayResult, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::finish_replay_browser);
		},
		.advance_replay_playback = [](
			void *context,
			ClassicAudioStatus audio,
			NewClassicUiInput input,
			CoreInput live_interface_input,
			bool fast_forward
		) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(
				context, &ClassicOpSessionBackend::advance_replay_playback,
				audio, input, live_interface_input, fast_forward
			);
		},
		.finish_replay_playback = [](void *context) {
			return backend_call<std::expected<bool, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::finish_replay_playback
			);
		},
		.start_music_room = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::start_music_room
			);
		},
		.advance_music_room = [](
			void *context,
			const ClassicOpMusicRoomInput &input
		) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::advance_music_room, input);
		},
		.finish_music_room = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::finish_music_room
			);
		},
		.start_high_scores = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::start_high_scores
			);
		},
		.advance_high_scores = [](
			void *context,
			ClassicRegistrationInput input,
			ClassicAudioStatus audio
		) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(
				context, &ClassicOpSessionBackend::advance_high_scores,
				input, audio
			);
		},
		.finish_high_scores = [](void *context) {
			return backend_call<std::expected<
				ClassicOpSessionReplayResult, ClassicOpSessionError
			>>(
				context, &ClassicOpSessionBackend::finish_high_scores
			);
		},
		.start_selection = [](
			void *context,
			ClassicOpSelectionMode mode
		) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::start_selection, mode
			);
		},
		.advance_selection = [](
			void *context,
			ClassicOpSelectionInput input
		) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::advance_selection, input);
		},
		.finish_selection = [](void *context) {
			return backend_call<std::expected<
				ClassicOpSessionSelectionResult, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::finish_selection);
		},
		.return_from_canceled_selection = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context,
				&ClassicOpSessionBackend::return_from_canceled_selection
			);
		},
		.commit_config = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::commit_config
			);
		},
		.start_story = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::start_story
			);
		},
		.advance_story = [](
			void *context,
			const ClassicStorySessionInput &input
		) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::advance_story, input);
		},
		.finish_story = [](void *context) {
			return backend_call<std::expected<bool, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::finish_story
			);
		},
		.start_demo_stage = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::start_demo_stage
			);
		},
		.advance_demo_stage = [](void *context, bool dismiss) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::advance_demo_stage, dismiss);
		},
		.start_demo_main = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::start_demo_main
			);
		},
		.advance_demo_main = [](
			void *context,
			const ClassicMainInputSample &input
		) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::advance_demo_main, input);
		},
		.finish_demo_main = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::finish_demo_main
			);
		},
		.start_vs_stage = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::start_vs_stage
			);
		},
		.advance_vs_stage = [](void *context, ClassicOpInput input) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::advance_vs_stage, input);
		},
		.start_vs_main = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::start_vs_main
			);
		},
		.advance_vs_main = [](
			void *context,
			const ClassicMainInputSample &input
		) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::advance_vs_main, input);
		},
		.finish_vs_main = [](void *context) {
			return backend_call<std::expected<
				ClassicOpSessionVsTerminal, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::finish_vs_main);
		},
		.restart_vs_stage = [](void *context, const auto &terminal) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::restart_vs_stage, terminal
			);
		},
		.start_vs_win = [](
			void *context,
			const ClassicOpSessionVsTerminal &terminal
		) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::start_vs_win, terminal
			);
		},
		.advance_vs_win = [](
			void *context,
			ClassicAudioStatus audio,
			bool dismiss
		) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(
				context, &ClassicOpSessionBackend::advance_vs_win, audio, dismiss
			);
		},
		.finish_vs_win = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::finish_vs_win
			);
		},
		.start_direct_vs_selection = [](void *context) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::start_direct_vs_selection
			);
		},
		.merge_online_board = [](void *context, auto document) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::merge_online_board,
				std::move(document)
			);
		},
		.merge_online_my_records = [](void *context, auto document) {
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::merge_online_my_records,
				std::move(document)
			);
		},
		.clear_online_my_records = [](void *context) {
			static_cast<ClassicOpSessionBackend *>(context)->
				clear_online_my_records();
		},
		.audio_pending = [](void *context) {
			return static_cast<ClassicOpSessionBackend *>(context)->audio_pending();
		},
		.take_audio_command = [](void *context) {
			return static_cast<ClassicOpSessionBackend *>(context)->
				take_audio_command();
		},
		.pixel_oracle_source_identity = [](void *context) {
			return static_cast<const ClassicOpSessionBackend *>(context)->
				pixel_oracle_source_identity();
		},
		.compose_shown_surface = [](void *context, auto output) {
			return backend_call<std::expected<
				present::SourceSurface, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::compose, output);
		},
	};
}

[[nodiscard]] ClassicOpSessionTestHooks backend_test_hooks(
	ClassicOpSessionBackend &backend
) noexcept
{
	return {
		.context = &backend,
		.story_scene = [](void *context) {
			return static_cast<ClassicOpSessionBackend *>(context)->
				story_scene_for_route_test();
		},
		.story_source_sample_count = [](void *context) {
			return static_cast<ClassicOpSessionBackend *>(context)->
				story_source_sample_count_for_route_test();
		},
		.story_failure_code = [](void *context) {
			return static_cast<ClassicOpSessionBackend *>(context)->
				story_failure_code_for_route_test();
		},
		.story_failure_detail_code = [](void *context) {
			return static_cast<ClassicOpSessionBackend *>(context)->
				story_failure_detail_code_for_route_test();
		},
		.force_story_main_outcome = [](void *context, auto outcome) {
			return static_cast<ClassicOpSessionBackend *>(context)->
				force_story_main_outcome_for_route_test(outcome);
		},
		.force_vs_main_outcome = [](void *context, auto outcome) {
			return static_cast<ClassicOpSessionBackend *>(context)->
				force_vs_main_outcome_for_route_test(outcome);
		},
		.vs_selection_resident = [](void *context) {
			return static_cast<ClassicOpSessionBackend *>(context)->
				vs_selection_resident_for_route_test();
		},
		.vs_terminal_resident = [](void *context) {
			return static_cast<ClassicOpSessionBackend *>(context)->
				vs_terminal_resident_for_route_test();
		},
		.online_high_score_menu = [](void *context) {
			return static_cast<const ClassicOpSessionBackend *>(context)->
				online_high_score_menu_for_route_test();
		},
	};
}

} // namespace

std::expected<std::unique_ptr<ClassicOpSession>, ClassicOpSessionError>
ClassicOpSession::create(
	GameInit resident,
	std::unique_ptr<MainResourceLoader> loader,
	MainCompiledContent compiled_content,
	std::span<const std::byte> font_rom,
	std::span<const std::byte> mainl_executable,
	std::span<const std::byte> op_executable,
	std::filesystem::path config_path,
	std::filesystem::path score_path,
	bool fm_available,
	present::KotohimeBombFadeCompatibility kotohime_compatibility,
	NewClassicReplayRecorder *replay_recorder,
	bool new_classic_practice_route,
	std::filesystem::path new_classic_score_path,
	std::filesystem::path language_overlay_path,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	const ClassicRunnerRequest *runner_request
)
{
	if(!loader || config_path.empty() || score_path.empty() ||
		resident.runtime_snapshot || resident.compact_runtime_snapshot ||
		resident.round_start_snapshot || resident.practice_rules) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::invalid_config
		));
	}
	ClassicConfigOptions config{};
	std::optional<NewClassicSettings> new_classic_settings{};
	auto persistent_config_path = config_path;
	if(replay_recorder) {
		auto settings_path = runner_request ?
			runner_request->content.new_classic_settings :
			std::filesystem::path{};
		if(settings_path.empty()) {
			settings_path = config_path.parent_path() / "PODD.CFG";
		}
		auto loaded = load_new_classic_settings_store(
			settings_path, config_path
		);
		if(!loaded) {
			return std::unexpected(new_settings_error(
				std::move(loaded.error())
			));
		}
		new_classic_settings = loaded->settings;
		config = new_classic_settings->options;
		if(runner_request && runner_request->initial_route ==
			ClassicRunnerInitialRoute::title &&
			stock_gameplay_rules(runner_request->gameplay_ruleset)) {
			resident.gameplay_ruleset = new_classic_settings->gameplay_ruleset;
		}
		if(!replay_recorder->set_capture_enabled(
			stock_gameplay_rules(resident.gameplay_ruleset)
		)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		persistent_config_path = std::move(settings_path);
	} else {
		auto loaded = load_classic_config_store(config_path);
		if(!loaded) {
			return std::unexpected(config_error(std::move(loaded.error())));
		}
		config = loaded->options;
	}
	if(!replay_recorder) {
		config.language = ClassicLanguage::japanese;
	} else if(patch_assets && patch_assets->custom_messages) {
		config.language = patch_assets->custom_messages->asset_profile() ==
			NewClassicLanguageAssetProfile::english ?
			ClassicLanguage::english : ClassicLanguage::japanese;
	}
	// Stock TH03 ships neither MIDI/MMD songs nor an MMD driver launch path.
	// Keep the source codec value readable, but never claim MIDI while routing
	// the session to the FM-only native engine.
	if(!fm_available || config.bgm_mode == ClassicBgmMode::midi) {
		config.bgm_mode = ClassicBgmMode::off;
	}
	auto imported = import_mainl_stage_text(mainl_executable);
	if(!imported) {
		return std::unexpected(mainl_text_error(std::move(imported.error())));
	}
	auto imported_op = import_op_music_room_text(op_executable);
	if(!imported_op) {
		return std::unexpected(op_text_error(std::move(imported_op.error())));
	}
	std::map<std::string, ResourceBytes> english_resources{};
	if(replay_recorder && !language_overlay_path.empty()) {
		auto overlay = load_new_classic_language_overlay(language_overlay_path);
		if(!overlay) {
			return std::unexpected(language_overlay_error(
				std::move(overlay.error())
			));
		}
		english_resources = std::move(*overlay).release_members();
	}
	if(config.language == ClassicLanguage::english &&
		english_resources.empty()) {
		config.language = ClassicLanguage::japanese;
	}
	auto shared_loader = std::shared_ptr<MainResourceLoader>(std::move(loader));
	auto backend = std::make_shared<ClassicOpSessionBackend>(
		std::move(shared_loader),
		std::move(compiled_content),
		std::vector<std::byte>{font_rom.begin(), font_rom.end()},
		std::move(imported->assets),
		std::move(imported->staff),
		std::move(*imported_op),
		std::move(persistent_config_path),
		std::move(score_path),
		config,
		std::move(new_classic_settings),
		fm_available,
		kotohime_compatibility,
		replay_recorder,
		new_classic_practice_route,
		std::move(new_classic_score_path),
		std::move(patch_assets), std::move(english_resources), runner_request
	);
	if(config.language == ClassicLanguage::english) {
		if(auto applied = backend->apply_language_profile(config.language);
			!applied) {
			return std::unexpected(std::move(applied.error()));
		}
	}
	auto prepared_font = Pc98FontRomProvider::from_exact_font_rom(
		backend->font_rom
	);
	if(!prepared_font) {
		return std::unexpected(font_error(std::move(prepared_font.error())));
	}
	backend->font.emplace(std::move(*prepared_font));
	if(backend->patch_assets) {
		auto normal = backend->loader->load({
			.role = MainResourceRole::registration_normal_glyphs,
		});
		if(!normal) {
			return std::unexpected(resource_error(std::move(normal.error())));
		}
		auto highlight = backend->loader->load({
			.role = MainResourceRole::registration_highlight_glyphs,
		});
		if(!highlight) {
			return std::unexpected(resource_error(std::move(highlight.error())));
		}
		auto save_art = NewClassicReplaySaveArt::prepare(
			std::span<const std::byte>{**normal},
			std::span<const std::byte>{**highlight},
			backend->font_rom
		);
		if(!save_art) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		backend->replay_save_art = std::move(*save_art);
	}
	resident.rank = static_cast<std::uint8_t>(config.rank);
	resident.key_mode = static_cast<std::uint8_t>(config.key_mode);
	if(!backend->push_audio({
		.type = ClassicAudioCommandType::set_bgm_active,
		.parameter = static_cast<std::uint16_t>(
			config.bgm_mode == ClassicBgmMode::off ? 0U : 1U
		),
	})) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::audio_command_pending
		));
	}
	std::expected<void, ClassicOpSessionError> started{};
	const bool initial_replay = runner_request &&
		runner_request->initial_route ==
			ClassicRunnerInitialRoute::new_classic_replay &&
		runner_request->initial_replay;
	if(initial_replay) {
		started = backend->start_initial_replay(
			runner_request->initial_replay->document,
			runner_request->initial_replay->checkpoint_index
		);
	} else if(new_classic_practice_route) {
		resident.game_mode = game_mode::vs_1p_cpu;
		resident.is_cpu = {0U, 1U};
		resident.paletted_character = {1, 1};
		resident.story_stage = 0U;
		resident.story_lives = 0U;
		backend->vs_terminal.emplace(ClassicOpSessionVsTerminal{
			.terminal = {.route = 0U},
			.transition = {
				.action = SessionAction::exit_to_operator,
				.resident = resident,
				.high_score_registration = std::nullopt,
			},
		});
		started = backend->start_direct_vs_selection();
	} else if(resident.demo_number == 0U &&
		selection_mode_for_game(resident.game_mode)) {
		backend->vs_terminal.emplace(ClassicOpSessionVsTerminal{
			.terminal = {.route = 0U},
			.transition = {
				.action = SessionAction::exit_to_operator,
				.resident = std::move(resident),
				.high_score_registration = std::nullopt,
			},
		});
		started = backend->start_direct_vs_selection();
	} else {
		started = backend->create_title(std::move(resident), true);
	}
	if(!started) {
		return std::unexpected(std::move(started.error()));
	}
	auto session = create_with_backend(
		std::static_pointer_cast<void>(backend), backend_operations(*backend),
		backend_test_hooks(*backend), replay_recorder,
		new_classic_practice_route
	);
	if(session && backend->direct_vs_selection) {
		(*session)->impl_->scene = ClassicOpSessionScene::character_selection;
	} else if(session && initial_replay) {
		(*session)->impl_->scene = ClassicOpSessionScene::replay_playback;
	}
	return session;
}

std::expected<std::unique_ptr<ClassicOpSession>, ClassicOpSessionError>
ClassicOpSession::create_with_backend(
	std::shared_ptr<void> backend_owner,
	ClassicOpSessionBackendOperations operations,
	ClassicOpSessionTestHooks test_hooks,
	NewClassicReplayRecorder *replay_recorder,
	bool new_classic_practice_route
)
{
	if(!backend_owner || !valid_backend(operations)) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::invalid_backend
		));
	}
	auto impl = std::make_unique<Impl>(
		std::move(backend_owner), operations, test_hooks, replay_recorder,
		new_classic_practice_route
	);
	return std::unique_ptr<ClassicOpSession>(
		new ClassicOpSession(std::move(impl))
	);
}

ClassicOpSession::ClassicOpSession(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{}

ClassicOpSession::~ClassicOpSession() = default;

std::expected<void, ClassicOpSessionError>
ClassicOpSession::resume_from_netplay()
{
	if(impl_->failure) {
		return std::unexpected(*impl_->failure);
	}
	if(impl_->status != ClassicOpSessionStatus::netplay_requested ||
		impl_->scene != ClassicOpSessionScene::complete) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::session_not_running
		));
	}
	if(auto resumed = impl_->operations.resume_from_netplay(
		impl_->operations.context
	); !resumed) {
		impl_->fail(resumed.error());
		return std::unexpected(resumed.error());
	}
	impl_->status = ClassicOpSessionStatus::running;
	impl_->scene = ClassicOpSessionScene::title_menu;
	impl_->boundary_ready = false;
	impl_->clock_rebase_requested = false;
	return {};
}

std::expected<ClassicOpSessionTick, ClassicOpSessionError>
ClassicOpSession::consume(const ClassicOpSessionInput &input)
{
	if(impl_->failure) {
		return std::unexpected(*impl_->failure);
	}
	if(impl_->status != ClassicOpSessionStatus::running) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::session_not_running
		));
	}
	if(input.app_quit_requested) {
		impl_->status = ClassicOpSessionStatus::quit_requested;
		return ClassicOpSessionTick{
			.scene = impl_->scene,
			.status = impl_->status,
			.pixel_oracle_source_identity = pixel_oracle_source_identity(),
		};
	}
	if(impl_->operations.audio_pending(impl_->operations.context)) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::audio_command_pending
		));
	}

	const auto identity_before = pixel_oracle_source_identity();
	ClassicOpSessionBackendTick backend_tick{};
	bool transitioned = false;
	const auto scene_before = impl_->scene;
	if(!impl_->boundary_ready) {
		std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError> tick =
			std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		switch(impl_->scene) {
		case ClassicOpSessionScene::title_menu:
			tick = impl_->operations.advance_title(
				impl_->operations.context, input.op
			);
			break;
		case ClassicOpSessionScene::key_config:
			if(!impl_->operations.advance_key_config) {
				break;
			}
			tick = impl_->operations.advance_key_config(
				impl_->operations.context,
				input.replay_ui,
				input.main.aggregate,
				input.scancode_pressed,
				input.any_scancode_held
			);
			break;
		case ClassicOpSessionScene::options:
			tick = impl_->operations.advance_options(
				impl_->operations.context, input.op
			);
			break;
		case ClassicOpSessionScene::replay_browser:
			tick = impl_->operations.advance_replay_browser(
				impl_->operations.context, input.replay_ui
			);
			break;
		case ClassicOpSessionScene::replay_playback:
			tick = impl_->operations.advance_replay_playback(
				impl_->operations.context,
				input.audio,
				input.replay_ui,
				input.main.aggregate,
				input.replay_fast_forward
			);
			break;
		case ClassicOpSessionScene::music_room:
			tick = impl_->operations.advance_music_room(
				impl_->operations.context, input.music_room
			);
			break;
		case ClassicOpSessionScene::high_scores:
			tick = impl_->operations.advance_high_scores(
				impl_->operations.context, input.high_scores, input.audio
			);
			break;
		case ClassicOpSessionScene::character_selection:
			tick = impl_->operations.advance_selection(
				impl_->operations.context, input.selection
			);
			break;
		case ClassicOpSessionScene::story:
			tick = impl_->operations.advance_story(
				impl_->operations.context, input.story
			);
			break;
		case ClassicOpSessionScene::demo_stage_splash:
			tick = impl_->operations.advance_demo_stage(
				impl_->operations.context, input.dismiss_pressed
			);
			break;
		case ClassicOpSessionScene::demo_main:
			tick = impl_->operations.advance_demo_main(
				impl_->operations.context, input.main
			);
			break;
		case ClassicOpSessionScene::vs_stage_splash:
			tick = impl_->operations.advance_vs_stage(
				impl_->operations.context, input.op
			);
			break;
		case ClassicOpSessionScene::vs_main:
			tick = impl_->operations.advance_vs_main(
				impl_->operations.context, input.main
			);
			break;
		case ClassicOpSessionScene::vs_win:
			tick = impl_->operations.advance_vs_win(
				impl_->operations.context, input.audio,
				input.dismiss_pressed
			);
			break;
		case ClassicOpSessionScene::complete:
		case ClassicOpSessionScene::failed:
			break;
		}
		if(!tick) {
			impl_->fail(std::move(tick.error()));
			return std::unexpected(*impl_->failure);
		}
		backend_tick = *tick;
		if(tick->return_to_selection) {
			if(scene_before != ClassicOpSessionScene::vs_stage_splash ||
				tick->ready) {
				impl_->fail(session_error(
					ClassicOpSessionErrorCode::backend_contract_failed
				));
				return std::unexpected(*impl_->failure);
			}
			impl_->scene = ClassicOpSessionScene::character_selection;
			transitioned = true;
		}
		if(tick->input_sampled && impl_->replay_recorder &&
			impl_->replay_recorder->recording() &&
			scene_before != ClassicOpSessionScene::story &&
			scene_before != ClassicOpSessionScene::vs_main) {
			if(auto sampled = impl_->replay_recorder->sample(
				replay::InputPhase::interstitial,
				normalize_replay_interstitial_input(input.main)
			); !sampled) {
				impl_->fail(replay_capture_error());
				return std::unexpected(*impl_->failure);
			}
		}
		impl_->boundary_ready = tick->ready;
	}

	if(impl_->boundary_ready &&
		!impl_->operations.audio_pending(impl_->operations.context)) {
		auto advanced = impl_->transition();
		if(!advanced) {
			// The completed child remains the owning capability until its
			// replacement has been constructed. Keep this boundary retryable so
			// a transient resource or persistence error cannot strand the app,
			// and do not advance the completed child a second time.
			return std::unexpected(std::move(advanced.error()));
		}
		transitioned = true;
	}

	auto publication_identity = backend_tick.pixel_oracle_source_identity.value_or(
		identity_before
	);
	if(transitioned && impl_->status == ClassicOpSessionStatus::running) {
		// transition() constructs the owner selected by compose_shown_surface().
		// Its startup surface, not the completed child's last counters, owns this
		// publication. Terminal routes retain the outgoing surface and therefore
		// keep its identity.
		publication_identity = pixel_oracle_source_identity();
		publication_identity.boundary = PixelOracleBoundary::scene_enter;
	}
	return ClassicOpSessionTick{
		.scene = impl_->scene,
		.status = impl_->status,
		.surface_changed = backend_tick.surface_changed || transitioned,
		.input_sampled = backend_tick.input_sampled,
		.process_advanced = backend_tick.process_advanced,
		.round_started = backend_tick.round_started,
		.clock_rebase_requested = std::exchange(
			impl_->clock_rebase_requested, false
		),
		.pixel_oracle_source_identity = publication_identity,
	};
}

std::optional<ClassicAudioCommand> ClassicOpSession::take_audio_command()
{
	return impl_->operations.take_audio_command(impl_->operations.context);
}

std::optional<NewClassicKeyConfig>
ClassicOpSession::take_key_config_update() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return std::nullopt;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_key_config_update();
}

std::optional<NewClassicJoyConfig>
ClassicOpSession::take_joy_config_update() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return std::nullopt;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_joy_config_update();
}

std::optional<NewClassicDeviceAssignment>
ClassicOpSession::take_device_assignment_update() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return std::nullopt;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_device_assignment_update();
}

std::optional<bool> ClassicOpSession::take_reduced_flash_update() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return std::nullopt;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_reduced_flash_update();
}

std::optional<std::uint8_t>
ClassicOpSession::take_master_volume_update() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return std::nullopt;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_master_volume_update();
}

std::optional<bool>
ClassicOpSession::take_mute_when_unfocused_update() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return std::nullopt;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_mute_when_unfocused_update();
}

std::optional<NewClassicWindowPresentation>
ClassicOpSession::take_window_presentation_update() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return std::nullopt;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_window_presentation_update();
}

std::optional<std::filesystem::path>
ClassicOpSession::take_open_folder_request() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return std::nullopt;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_open_folder_request();
}

bool ClassicOpSession::take_online_identity_reset_request() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return false;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_online_identity_reset_request();
}

bool ClassicOpSession::take_update_check_request() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return false;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_update_check_request();
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::begin_update_check()
{
	if(!impl_ || !impl_->operations.context) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->begin_update_check();
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::report_update_check(NewClassicUpdateCheckOutcome outcome)
{
	if(!impl_ || !impl_->operations.context) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->report_update_check(outcome);
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::merge_online_board(NewClassicOnlineCacheDocument document)
{
	if(!impl_ || !impl_->operations.merge_online_board) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return impl_->operations.merge_online_board(
		impl_->operations.context, std::move(document)
	);
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::merge_online_my_records(
	NewClassicOnlineCacheDocument document
)
{
	if(!impl_ || !impl_->operations.merge_online_my_records) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return impl_->operations.merge_online_my_records(
		impl_->operations.context, std::move(document)
	);
}

void ClassicOpSession::clear_online_my_records() noexcept
{
	if(impl_ && impl_->operations.clear_online_my_records) {
		impl_->operations.clear_online_my_records(impl_->operations.context);
	}
}

std::optional<NewClassicOnlineReplayRecord>
ClassicOpSession::take_online_replay_download_request() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return std::nullopt;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_online_replay_download_request();
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::report_online_replay_download(
	const NewClassicOnlineReplayId &replay_id,
	NewClassicOnlineReplayDownloadOutcome outcome
)
{
	if(!impl_ || !impl_->operations.context) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->report_online_replay_download(replay_id, outcome);
}

std::optional<NewClassicReplayUploadRequest>
ClassicOpSession::take_online_replay_upload_request() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return std::nullopt;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_online_replay_upload_request();
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::report_online_replay_upload(
	std::uint8_t slot,
	NewClassicReplayUploadOutcome outcome
)
{
	if(!impl_ || !impl_->operations.context) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->report_online_replay_upload(slot, outcome);
}

std::optional<NewClassicOnlineReplayRecord>
ClassicOpSession::take_online_replay_watch_request() noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return std::nullopt;
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->take_online_replay_watch_request();
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::report_online_replay_watch(
	const NewClassicOnlineReplayId &replay_id,
	std::optional<formats::ReplayDocument> document
)
{
	if(!impl_ || !impl_->operations.context) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return static_cast<ClassicOpSessionBackend *>(
		impl_->operations.context
	)->report_online_replay_watch(replay_id, std::move(document));
}

std::shared_ptr<const NewClassicReplaySaveArt>
ClassicOpSession::replay_save_art() const noexcept
{
	if(!impl_ || !impl_->operations.context) {
		return {};
	}
	return static_cast<const ClassicOpSessionBackend *>(
		impl_->operations.context
	)->replay_save_art;
}

std::expected<present::SourceSurface, ClassicOpSessionError>
ClassicOpSession::compose_shown_surface(
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

ClassicOpSessionScene ClassicOpSession::scene() const noexcept
{
	return impl_->scene;
}

ClassicOpSessionStatus ClassicOpSession::status() const noexcept
{
	return impl_->status;
}

PixelOracleSourceIdentity
ClassicOpSession::pixel_oracle_source_identity() const noexcept
{
	if(impl_->operations.pixel_oracle_source_identity) {
		if(auto child = impl_->operations.pixel_oracle_source_identity(
			impl_->operations.context
		)) {
			return *child;
		}
	}
	return {
		.process = PixelOracleProcess::op,
		.scene = pixel_oracle_scene(impl_->scene),
	};
}

ClassicOpVsSurfaceSource ClassicOpSessionTestAccess::vs_surface_source(
	bool win,
	bool win_completion,
	bool main_process
) noexcept
{
	return th03::app::vs_surface_source(win, win_completion, main_process);
}

GameInit ClassicOpSessionTestAccess::apply_key_config_autofire(
	GameInit resident,
	const std::optional<NewClassicKeyConfig> &key_config
) noexcept
{
	th03::app::apply_key_config_autofire(resident, key_config);
	return resident;
}

CoreInput ClassicOpSessionTestAccess::vs_cpu_main_input(
	const GameInit &resident,
	const ClassicMainInputSample &input,
	InputContext context,
	bool new_classic
) noexcept
{
	return normalize_vs_cpu_main_input(resident, input, context, new_classic);
}

bool ClassicOpSessionTestAccess::attract_input_present(
	const ClassicMainInputSample &input
) noexcept
{
	return th03::app::attract_input_present(input);
}

std::optional<ClassicStorySessionScene>
ClassicOpSessionTestAccess::story_scene(
	const ClassicOpSession &session
) noexcept
{
	if(!session.impl_->test_hooks.context ||
		!session.impl_->test_hooks.story_scene) {
		return std::nullopt;
	}
	return session.impl_->test_hooks.story_scene(
		session.impl_->test_hooks.context
	);
}

std::optional<std::uint32_t>
ClassicOpSessionTestAccess::story_source_sample_count(
	const ClassicOpSession &session
) noexcept
{
	if(!session.impl_->test_hooks.context ||
		!session.impl_->test_hooks.story_source_sample_count) {
		return std::nullopt;
	}
	return session.impl_->test_hooks.story_source_sample_count(
		session.impl_->test_hooks.context
	);
}

std::optional<ClassicStorySessionErrorCode>
ClassicOpSessionTestAccess::story_failure_code(
	const ClassicOpSession &session
) noexcept
{
	if(!session.impl_->test_hooks.context ||
		!session.impl_->test_hooks.story_failure_code) {
		return std::nullopt;
	}
	return session.impl_->test_hooks.story_failure_code(
		session.impl_->test_hooks.context
	);
}

std::optional<std::uint8_t>
ClassicOpSessionTestAccess::story_failure_detail_code(
	const ClassicOpSession &session
) noexcept
{
	if(!session.impl_->test_hooks.context ||
		!session.impl_->test_hooks.story_failure_detail_code) {
		return std::nullopt;
	}
	return session.impl_->test_hooks.story_failure_detail_code(
		session.impl_->test_hooks.context
	);
}

std::expected<void, ClassicOpSessionError>
ClassicOpSessionTestAccess::force_story_main_outcome(
	ClassicOpSession &session,
	ClassicStoryRouteTestOutcome outcome
)
{
	if(session.impl_->scene != ClassicOpSessionScene::story ||
		!session.impl_->test_hooks.context ||
		!session.impl_->test_hooks.force_story_main_outcome) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return session.impl_->test_hooks.force_story_main_outcome(
		session.impl_->test_hooks.context, outcome
	);
}

std::expected<void, ClassicOpSessionError>
ClassicOpSessionTestAccess::force_vs_main_outcome(
	ClassicOpSession &session,
	ClassicOpVsRouteTestOutcome outcome
)
{
	if(session.impl_->scene != ClassicOpSessionScene::vs_main ||
		!session.impl_->test_hooks.context ||
		!session.impl_->test_hooks.force_vs_main_outcome) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return session.impl_->test_hooks.force_vs_main_outcome(
		session.impl_->test_hooks.context, outcome
	);
}

std::optional<GameInit> ClassicOpSessionTestAccess::vs_selection_resident(
	const ClassicOpSession &session
) noexcept
{
	if(session.impl_->scene != ClassicOpSessionScene::character_selection ||
		!session.impl_->test_hooks.context ||
		!session.impl_->test_hooks.vs_selection_resident) {
		return std::nullopt;
	}
	return session.impl_->test_hooks.vs_selection_resident(
		session.impl_->test_hooks.context
	);
}

std::optional<GameInit> ClassicOpSessionTestAccess::vs_terminal_resident(
	const ClassicOpSession &session
) noexcept
{
	if(session.impl_->scene != ClassicOpSessionScene::vs_win ||
		!session.impl_->test_hooks.context ||
		!session.impl_->test_hooks.vs_terminal_resident) {
		return std::nullopt;
	}
	return session.impl_->test_hooks.vs_terminal_resident(
		session.impl_->test_hooks.context
	);
}

bool ClassicOpSessionTestAccess::online_high_score_menu(
	const ClassicOpSession &session
) noexcept
{
	return session.impl_->scene == ClassicOpSessionScene::high_scores &&
		session.impl_->test_hooks.context &&
		session.impl_->test_hooks.online_high_score_menu &&
		session.impl_->test_hooks.online_high_score_menu(
			session.impl_->test_hooks.context
		);
}

} // namespace th03::app
