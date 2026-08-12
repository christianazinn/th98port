#include "classic_op_session_internal.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "main_process_driver_internal.hpp"
#include "main_process_internal.hpp"
#include "main_process_static_content_internal.hpp"
#include "th03/app/authentic_text.hpp"
#include "th03/app/new_classic_replay.hpp"
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
		operations.finish_title && operations.start_options &&
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
		operations.finish_vs_main && operations.start_vs_win &&
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
	InputContext context
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
	case game_mode::vs_1p_cpu:
		// input_mode_1p_vs_cpu(): merge input_sp and joystick zero into the
		// human side and clear the CPU side before both player updates.
		input.p1 = static_cast<std::uint16_t>(
			physical.keyboard.p1 | physical.keyboard.system |
			(physical.first_joystick_present ? physical.first_joystick : 0U)
		);
		input.p2 = 0U;
		input.charge_mask = static_cast<std::uint8_t>(
			physical.keyboard.charge_mask & 0x01U
		);
		break;
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

} // namespace

struct ClassicOpSession::Impl final {
	std::shared_ptr<void> backend_owner{};
	ClassicOpSessionBackendOperations operations{};
	ClassicOpSessionTestHooks test_hooks{};
	ClassicOpSessionScene scene = ClassicOpSessionScene::title_menu;
	ClassicOpSessionStatus status = ClassicOpSessionStatus::running;
	std::optional<ClassicOpRoute> external{};
	std::optional<ClassicOpSessionError> failure{};
	bool boundary_ready = false;
	NewClassicReplayRecorder *replay_recorder = nullptr;

	Impl(
		std::shared_ptr<void> owner,
		ClassicOpSessionBackendOperations operations_,
		ClassicOpSessionTestHooks test_hooks_,
		NewClassicReplayRecorder *replay_recorder_
	) :
		backend_owner(std::move(owner)),
		operations(operations_),
		test_hooks(test_hooks_),
		replay_recorder(replay_recorder_)
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
		case ClassicOpSessionScene::options:
			if(auto finished = operations.finish_options(operations.context);
				!finished) {
				return std::unexpected(std::move(finished.error()));
			}
			scene = ClassicOpSessionScene::title_menu;
			break;
		case ClassicOpSessionScene::music_room:
			if(auto finished = operations.finish_music_room(operations.context);
				!finished) {
				return std::unexpected(std::move(finished.error()));
			}
			scene = ClassicOpSessionScene::title_menu;
			break;
		case ClassicOpSessionScene::high_scores:
			if(auto finished = operations.finish_high_scores(operations.context);
				!finished) {
				return std::unexpected(std::move(finished.error()));
			}
			scene = ClassicOpSessionScene::title_menu;
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
				scene = ClassicOpSessionScene::title_menu;
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
			if(terminal->terminal.route == 0U) {
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
		case ClassicOpSessionScene::external_route:
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
	std::shared_ptr<const MainProcessStaticContent> static_content{};
	std::filesystem::path config_path{};
	std::filesystem::path score_path{};
	ClassicConfigOptions config{};
	bool fm_available = true;
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
	NewClassicReplayRecorder *replay_recorder = nullptr;
	ClassicOpMusicRoomCarry music_carry{};
	ClassicOpSelectionCarry selection_carry{};

	std::unique_ptr<ClassicOpTitleMenuScene> title{};
	std::optional<ClassicOpCompletion> title_completion{};
	std::unique_ptr<ClassicOpOptionsScene> options{};
	std::optional<ClassicOpOptionsCompletion> options_completion{};
	std::unique_ptr<ClassicOpMusicRoomScene> music_room{};
	std::optional<ClassicOpMusicRoomCompletion> music_completion{};
	std::unique_ptr<ClassicRegistrationScene> high_scores{};
	std::optional<ClassicRegistrationCompletion> high_score_completion{};
	std::unique_ptr<ClassicOpCharacterSelection> selection{};
	std::optional<ClassicOpSelectionCompletion> selection_completion{};
	std::unique_ptr<ClassicStorySession> story{};
	std::unique_ptr<ClassicMainlStageTransition> vs_stage{};
	std::unique_ptr<MainProcess> vs_process{};
	std::unique_ptr<MainProcessDriver> vs_driver{};
	std::optional<ClassicOpSessionVsTerminal> vs_terminal{};
	std::unique_ptr<ClassicMainlWinScene> vs_win{};
	std::optional<ClassicMainlWinCompletion> vs_win_completion{};
	GameInit current_main_resident{};
	bool direct_vs_selection = false;

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
		bool fm_available_,
		present::KotohimeBombFadeCompatibility compatibility,
		NewClassicReplayRecorder *replay_recorder_
	) :
		loader(std::move(loader_)),
		compiled(std::move(compiled_)),
		font_rom(std::move(font_rom_)),
		stage_text(std::move(stage_text_)),
		staff_compiled(std::move(staff_compiled_)),
		op_music_labels(std::move(op_music_labels_)),
		config_path(std::move(config_path_)),
		score_path(std::move(score_path_)),
		config(config_),
		fm_available(fm_available_),
		kotohime_compatibility(compatibility),
		replay_recorder(replay_recorder_)
	{
		blank_tram.fill({
			' ', static_cast<present::TramAttribute>(present::tram_black)
		});
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
		auto resources = loader->load_op_title_menu();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		const auto mode = resident.fast_op_animation == 0U ?
			ClassicOpTitleMode::long_animation : ClassicOpTitleMode::fast_fade;
		auto created = ClassicOpTitleMenuScene::create(
			std::move(resident), mode, *resources
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
		if(title || options || music_room || high_scores || selection || story ||
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
		return ClassicOpSessionTitleResult{
			.route = title_completion->route(),
			.selection_mode = title_completion->selection_mode(),
		};
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
			*title_completion, config, fm_available, *resources
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
		return ClassicOpSessionBackendTick{
			.surface_changed = tick->surface_changed || tick->tram_changed,
			.input_sampled = tick->input_sampled,
			.ready = tick->ready,
		};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError> finish_options()
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
		auto resources = loader->load_op_title_menu();
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto resumed = ClassicOpTitleMenuScene::resume_from_options(
			*options_completion, *resources
		);
		if(!resumed) {
			return std::unexpected(title_error(std::move(resumed.error())));
		}
		config = options_completion->config();
		title = std::move(*resumed);
		options.reset();
		options_completion.reset();
		return {};
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
			*title_completion, music_carry, *resources, op_music_labels, *font
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
			*music_completion, *resources
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
	start_high_scores()
	{
		if(!title_completion || high_scores ||
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
		auto created = ClassicRegistrationScene::create_viewer(
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
		title_completion.reset();
		return {};
	}

	[[nodiscard]] std::expected<ClassicOpSessionBackendTick,
		ClassicOpSessionError> advance_high_scores(
		ClassicRegistrationInput input,
		ClassicAudioStatus audio
	)
	{
		if(!high_scores) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		if(high_scores->phase() == ClassicRegistrationPhase::ready) {
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
		return ClassicOpSessionBackendTick{
			.surface_changed = advanced->surface_changed,
			.input_sampled = advanced->input_sampled,
			.ready = advanced->ready,
		};
	}

	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	finish_high_scores()
	{
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
		auto returned = build_title(high_score_completion->resident());
		if(!returned) {
			return std::unexpected(std::move(returned.error()));
		}
		title = std::move(*returned);
		high_scores.reset();
		high_score_completion.reset();
		music_carry = {};
		selection_carry = {};
		return {};
	}

	[[nodiscard]] std::expected<std::uint8_t, ClassicOpSessionError>
	available_character_count()
	{
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
		auto created = ClassicOpCharacterSelection::create_from_op(
			*title_completion, mode, *available, selection_carry, *resources
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
					*selection_completion, *resources
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
			.game_options = {.input_protocol = InputProtocol::live},
			.kotohime_compatibility = kotohime_compatibility,
		};
		auto created = ClassicStorySession::create_with_prepared_content(
			story_config, loader, static_content, stage_text, staff_compiled,
			score_path, replay_recorder
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

	[[nodiscard]] std::expected<void, ClassicOpSessionError> finish_story()
	{
		if(!story || story->status() != ClassicStorySessionStatus::complete) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
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
		return {};
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
			stage_text, *font
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
		return advance_vs_stage(dismiss_pressed);
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
		if(auto prepared = ensure_static_content(); !prepared) {
			return std::unexpected(std::move(prepared.error()));
		}
		const auto request = stage_request(selection_completion->resident());
		auto resources = loader->load_mainl_stage(request);
		if(!resources) {
			return std::unexpected(resource_error(std::move(resources.error())));
		}
		auto created = ClassicMainlStageTransition::create_from_selection(
			*selection_completion, kotohime_compatibility, *resources,
			stage_text, *font
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
		ClassicOpSessionError> advance_vs_stage(bool dismiss_pressed)
	{
		if(!vs_stage) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto tick = vs_stage->advance(dismiss_pressed);
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
		vs_driver = std::move(*driver);
		vs_stage.reset();
		vs_terminal.reset();
		if(replay_recorder) {
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
		const auto normalized = normalize_vs_cpu_main_input(
			current_main_resident, input, vs_driver->input_context()
		);
		auto tick = vs_driver->consume(normalized);
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
		if(!drain_audio(*vs_process)) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::audio_command_pending
			));
		}
		if(replay_recorder && tick->process_advanced) {
			if(auto sampled = replay_recorder->sample(
				replay::InputPhase::gameplay, normalized
			); !sampled) {
				return std::unexpected(replay_capture_error());
			}
			if(tick->step_result.status == StepStatus::round_transition ||
				tick->step_result.status == StepStatus::terminal) {
				if(auto split = replay_recorder->round_completed(
					tick->step_result.input_boundary_state
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
			});
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
			*font
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
		vs_win.reset();
		vs_win_completion.reset();
		vs_terminal.reset();
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

	[[nodiscard]] std::expected<present::SourceSurface, ClassicOpSessionError>
	compose(std::span<
		present::ComposedPixel,
		present::graphics_page_pixels
	> output)
	{
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
			return *composed;
		}
		case ClassicOpVsSurfaceSource::none:
			break;
		}

		if(!raster && title_completion) {
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
		} else if(!raster && high_score_completion) {
			raster = &high_score_completion->raster();
		} else if(!raster && high_scores) {
			raster = &high_scores->raster();
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
		return *composed;
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
			return backend_call<std::expected<void, ClassicOpSessionError>>(
				context, &ClassicOpSessionBackend::finish_options
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
			return backend_call<std::expected<void, ClassicOpSessionError>>(
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
			return backend_call<std::expected<void, ClassicOpSessionError>>(
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
		.advance_vs_stage = [](void *context, bool dismiss) {
			return backend_call<std::expected<
				ClassicOpSessionBackendTick, ClassicOpSessionError
			>>(context, &ClassicOpSessionBackend::advance_vs_stage, dismiss);
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
		.audio_pending = [](void *context) {
			return static_cast<ClassicOpSessionBackend *>(context)->audio_pending();
		},
		.take_audio_command = [](void *context) {
			return static_cast<ClassicOpSessionBackend *>(context)->
				take_audio_command();
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
	NewClassicReplayRecorder *replay_recorder
)
{
	if(!loader || config_path.empty() || score_path.empty() ||
		resident.runtime_snapshot || resident.compact_runtime_snapshot ||
		resident.round_start_snapshot || resident.practice_rules) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::invalid_config
		));
	}
	auto loaded_config = load_classic_config_store(config_path);
	if(!loaded_config) {
		return std::unexpected(config_error(std::move(loaded_config.error())));
	}
	auto config = loaded_config->options;
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
	auto shared_loader = std::shared_ptr<MainResourceLoader>(std::move(loader));
	auto backend = std::make_shared<ClassicOpSessionBackend>(
		std::move(shared_loader),
		std::move(compiled_content),
		std::vector<std::byte>{font_rom.begin(), font_rom.end()},
		std::move(imported->assets),
		std::move(imported->staff),
		std::move(*imported_op),
		std::move(config_path),
		std::move(score_path),
		config,
		fm_available,
		kotohime_compatibility,
		replay_recorder
	);
	auto prepared_font = Pc98FontRomProvider::from_exact_font_rom(
		backend->font_rom
	);
	if(!prepared_font) {
		return std::unexpected(font_error(std::move(prepared_font.error())));
	}
	backend->font.emplace(std::move(*prepared_font));
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
	if(resident.demo_number == 0U && selection_mode_for_game(resident.game_mode)) {
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
		backend_test_hooks(*backend), replay_recorder
	);
	if(session && backend->direct_vs_selection) {
		(*session)->impl_->scene = ClassicOpSessionScene::character_selection;
	}
	return session;
}

std::expected<std::unique_ptr<ClassicOpSession>, ClassicOpSessionError>
ClassicOpSession::create_with_backend(
	std::shared_ptr<void> backend_owner,
	ClassicOpSessionBackendOperations operations,
	ClassicOpSessionTestHooks test_hooks,
	NewClassicReplayRecorder *replay_recorder
)
{
	if(!backend_owner || !valid_backend(operations)) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::invalid_backend
		));
	}
	auto impl = std::make_unique<Impl>(
		std::move(backend_owner), operations, test_hooks, replay_recorder
	);
	return std::unique_ptr<ClassicOpSession>(
		new ClassicOpSession(std::move(impl))
	);
}

ClassicOpSession::ClassicOpSession(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{}

ClassicOpSession::~ClassicOpSession() = default;

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
		};
	}
	if(impl_->operations.audio_pending(impl_->operations.context)) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::audio_command_pending
		));
	}

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
		case ClassicOpSessionScene::options:
			tick = impl_->operations.advance_options(
				impl_->operations.context, input.op
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
				impl_->operations.context, input.dismiss_pressed
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
		case ClassicOpSessionScene::external_route:
		case ClassicOpSessionScene::complete:
		case ClassicOpSessionScene::failed:
			break;
		}
		if(!tick) {
			impl_->fail(std::move(tick.error()));
			return std::unexpected(*impl_->failure);
		}
		backend_tick = *tick;
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

	return ClassicOpSessionTick{
		.scene = impl_->scene,
		.status = impl_->status,
		.surface_changed = backend_tick.surface_changed || transitioned,
		.input_sampled = backend_tick.input_sampled,
		.process_advanced = backend_tick.process_advanced,
		.round_started = backend_tick.round_started,
		.external_route = impl_->external,
	};
}

std::optional<ClassicAudioCommand> ClassicOpSession::take_audio_command()
{
	return impl_->operations.take_audio_command(impl_->operations.context);
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

ClassicOpVsSurfaceSource ClassicOpSessionTestAccess::vs_surface_source(
	bool win,
	bool win_completion,
	bool main_process
) noexcept
{
	return th03::app::vs_surface_source(win, win_completion, main_process);
}

CoreInput ClassicOpSessionTestAccess::vs_cpu_main_input(
	const GameInit &resident,
	const ClassicMainInputSample &input,
	InputContext context
) noexcept
{
	return normalize_vs_cpu_main_input(resident, input, context);
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

std::optional<ClassicOpRoute> ClassicOpSession::external_route() const noexcept
{
	return impl_->external;
}

} // namespace th03::app
