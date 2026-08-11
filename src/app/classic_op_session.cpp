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
	ClassicOpSessionScene scene = ClassicOpSessionScene::title_menu;
	ClassicOpSessionStatus status = ClassicOpSessionStatus::running;
	std::optional<ClassicOpRoute> external{};
	std::optional<ClassicOpSessionError> failure{};
	bool boundary_ready = false;

	Impl(
		std::shared_ptr<void> owner,
		ClassicOpSessionBackendOperations operations_
	) :
		backend_owner(std::move(owner)),
		operations(operations_)
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
				external = result->route;
				status = ClassicOpSessionStatus::external_route;
				scene = ClassicOpSessionScene::external_route;
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
	std::vector<std::byte> op_executable{};
	std::shared_ptr<const MainProcessStaticContent> static_content{};
	std::filesystem::path config_path{};
	std::filesystem::path score_path{};
	ClassicConfigOptions config{};
	bool fm_available = true;
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
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
	GameInit current_vs_resident{};
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
		std::vector<std::byte> op_executable_,
		std::filesystem::path config_path_,
		std::filesystem::path score_path_,
		ClassicConfigOptions config_,
		bool fm_available_,
		present::KotohimeBombFadeCompatibility compatibility
	) :
		loader(std::move(loader_)),
		compiled(std::move(compiled_)),
		font_rom(std::move(font_rom_)),
		stage_text(std::move(stage_text_)),
		staff_compiled(std::move(staff_compiled_)),
		op_executable(std::move(op_executable_)),
		config_path(std::move(config_path_)),
		score_path(std::move(score_path_)),
		config(config_),
		fm_available(fm_available_),
		kotohime_compatibility(compatibility)
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
			.surface_changed = tick->surface_changed,
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
			.surface_changed = tick->surface_changed,
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
		auto created = ClassicOpMusicRoomScene::create_from_op(
			*title_completion, music_carry, *resources, op_executable, *font
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
			score_path
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
		current_vs_resident = selection_completion->resident();
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
			current_vs_resident.paletted_character[0U]
		);
		const auto player_two = main_player_selection_from_paletted(
			current_vs_resident.paletted_character[1U]
		);
		if(!player_one || !player_two) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::invalid_config
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
		ClassicOpSessionError> advance_vs_main(CoreInput input)
	{
		if(!vs_process || !vs_driver) {
			return std::unexpected(session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		auto tick = vs_driver->consume(input);
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
		return ClassicOpSessionBackendTick{
			.surface_changed = tick->surface_changed,
			.input_sampled = true,
			.ready = tick->terminal,
			.process_advanced = true,
			.round_started = tick->round_started,
		};
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
				.process_start = current_vs_resident,
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
		if(vs_process) {
			auto composed = vs_process->compose_shown_surface(output);
			if(!composed) {
				return std::unexpected(surface_error(composed.error()));
			}
			return *composed;
		}

		const present::Raster *raster = nullptr;
		const present::Tram *tram = &blank_tram;
		if(title) {
			raster = &title->raster();
			tram = &title->tram();
		} else if(options) {
			raster = &options->raster();
			tram = &options->tram();
		} else if(music_room) {
			raster = &music_room->raster();
			tram = &music_room->tram();
		} else if(high_scores) {
			raster = &high_scores->raster();
		} else if(selection) {
			raster = &selection->raster();
			tram = &selection->tram();
		} else if(vs_stage) {
			raster = &vs_stage->raster();
			tram = &vs_stage->tram();
		} else if(vs_win) {
			raster = &vs_win->raster();
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
		.advance_vs_main = [](void *context, CoreInput input) {
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
	present::KotohimeBombFadeCompatibility kotohime_compatibility
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
	if(!fm_available) {
		config.bgm_mode = ClassicBgmMode::off;
	}
	auto imported = import_mainl_stage_text(mainl_executable);
	if(!imported) {
		return std::unexpected(mainl_text_error(std::move(imported.error())));
	}
	auto shared_loader = std::shared_ptr<MainResourceLoader>(std::move(loader));
	auto backend = std::make_shared<ClassicOpSessionBackend>(
		std::move(shared_loader),
		std::move(compiled_content),
		std::vector<std::byte>{font_rom.begin(), font_rom.end()},
		std::move(imported->assets),
		std::move(imported->staff),
		std::vector<std::byte>{op_executable.begin(), op_executable.end()},
		std::move(config_path),
		std::move(score_path),
		config,
		fm_available,
		kotohime_compatibility
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
		std::static_pointer_cast<void>(backend), backend_operations(*backend)
	);
	if(session && backend->direct_vs_selection) {
		(*session)->impl_->scene = ClassicOpSessionScene::character_selection;
	}
	return session;
}

std::expected<std::unique_ptr<ClassicOpSession>, ClassicOpSessionError>
ClassicOpSession::create_with_backend(
	std::shared_ptr<void> backend_owner,
	ClassicOpSessionBackendOperations operations
)
{
	if(!backend_owner || !valid_backend(operations)) {
		return std::unexpected(session_error(
			ClassicOpSessionErrorCode::invalid_backend
		));
	}
	auto impl = std::make_unique<Impl>(
		std::move(backend_owner), operations
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

std::optional<ClassicOpRoute> ClassicOpSession::external_route() const noexcept
{
	return impl_->external;
}

} // namespace th03::app
