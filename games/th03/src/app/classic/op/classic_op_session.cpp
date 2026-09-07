#include "classic/op/classic_op_session_backend_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "th03/app/new_classic/replay/new_classic_replay.hpp"

namespace th03::app {
namespace {

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
	ClassicOpSessionHostOperations host_operations{};
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
		bool new_classic_practice_route_,
		ClassicOpSessionHostOperations host_operations_
	) :
		backend_owner(std::move(owner)),
		operations(operations_),
		host_operations(host_operations_),
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
					return std::unexpected(detail::session_error(
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
					return std::unexpected(detail::session_error(
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
					return std::unexpected(detail::session_error(
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
					return std::unexpected(detail::session_error(
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
				return std::unexpected(detail::session_error(
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
				return std::unexpected(detail::session_error(
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
				return std::unexpected(detail::session_error(
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
					return std::unexpected(detail::session_error(
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
				return std::unexpected(detail::session_error(
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
			return std::unexpected(detail::session_error(
				ClassicOpSessionErrorCode::backend_contract_failed
			));
		}
		boundary_ready = false;
		return {};
	}
};

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
	auto backend = detail::create_classic_op_session_production_backend(
		std::move(resident),
		std::move(loader),
		std::move(compiled_content),
		font_rom,
		mainl_executable,
		op_executable,
		std::move(config_path),
		std::move(score_path),
		fm_available,
		kotohime_compatibility,
		replay_recorder,
		new_classic_practice_route,
		std::move(new_classic_score_path),
		std::move(language_overlay_path),
		std::move(patch_assets),
		runner_request
	);
	if(!backend) {
		return std::unexpected(std::move(backend.error()));
	}
	const auto initial_scene = backend->initial_scene;
	auto session = create_with_backend(
		std::move(backend->owner),
		backend->operations,
		backend->test_hooks,
		replay_recorder,
		new_classic_practice_route,
		backend->host_operations
	);
	if(session) {
		(*session)->impl_->scene = initial_scene;
	}
	return session;
}

std::expected<std::unique_ptr<ClassicOpSession>, ClassicOpSessionError>
ClassicOpSession::create_with_backend(
	std::shared_ptr<void> backend_owner,
	ClassicOpSessionBackendOperations operations,
	ClassicOpSessionTestHooks test_hooks,
	NewClassicReplayRecorder *replay_recorder,
	bool new_classic_practice_route,
	ClassicOpSessionHostOperations host_operations
)
{
	if(!backend_owner || !valid_backend(operations)) {
		return std::unexpected(detail::session_error(
			ClassicOpSessionErrorCode::invalid_backend
		));
	}
	auto impl = std::make_unique<Impl>(
		std::move(backend_owner), operations, test_hooks, replay_recorder,
		new_classic_practice_route, host_operations
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
		return std::unexpected(detail::session_error(
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
		return std::unexpected(detail::session_error(
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
		return std::unexpected(detail::session_error(
			ClassicOpSessionErrorCode::audio_command_pending
		));
	}

	const auto identity_before = pixel_oracle_source_identity();
	ClassicOpSessionBackendTick backend_tick{};
	bool transitioned = false;
	const auto scene_before = impl_->scene;
	if(!impl_->boundary_ready) {
		std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError> tick =
			std::unexpected(detail::session_error(
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
				impl_->fail(detail::session_error(
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
				detail::normalize_replay_interstitial_input(input.main)
			); !sampled) {
				impl_->fail(detail::replay_capture_error());
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
	if(!impl_ || !impl_->host_operations.take_key_config_update) {
		return std::nullopt;
	}
	return impl_->host_operations.take_key_config_update(
		impl_->host_operations.context
	);
}

std::optional<NewClassicJoyConfig>
ClassicOpSession::take_joy_config_update() noexcept
{
	if(!impl_ || !impl_->host_operations.take_joy_config_update) {
		return std::nullopt;
	}
	return impl_->host_operations.take_joy_config_update(
		impl_->host_operations.context
	);
}

std::optional<NewClassicDeviceAssignment>
ClassicOpSession::take_device_assignment_update() noexcept
{
	if(!impl_ || !impl_->host_operations.take_device_assignment_update) {
		return std::nullopt;
	}
	return impl_->host_operations.take_device_assignment_update(
		impl_->host_operations.context
	);
}

std::optional<bool> ClassicOpSession::take_reduced_flash_update() noexcept
{
	if(!impl_ || !impl_->host_operations.take_reduced_flash_update) {
		return std::nullopt;
	}
	return impl_->host_operations.take_reduced_flash_update(
		impl_->host_operations.context
	);
}

std::optional<std::uint8_t>
ClassicOpSession::take_master_volume_update() noexcept
{
	if(!impl_ || !impl_->host_operations.take_master_volume_update) {
		return std::nullopt;
	}
	return impl_->host_operations.take_master_volume_update(
		impl_->host_operations.context
	);
}

std::optional<bool>
ClassicOpSession::take_mute_when_unfocused_update() noexcept
{
	if(!impl_ || !impl_->host_operations.take_mute_when_unfocused_update) {
		return std::nullopt;
	}
	return impl_->host_operations.take_mute_when_unfocused_update(
		impl_->host_operations.context
	);
}

std::optional<NewClassicWindowPresentation>
ClassicOpSession::take_window_presentation_update() noexcept
{
	if(!impl_ || !impl_->host_operations.take_window_presentation_update) {
		return std::nullopt;
	}
	return impl_->host_operations.take_window_presentation_update(
		impl_->host_operations.context
	);
}

std::optional<std::filesystem::path>
ClassicOpSession::take_open_folder_request() noexcept
{
	if(!impl_ || !impl_->host_operations.take_open_folder_request) {
		return std::nullopt;
	}
	return impl_->host_operations.take_open_folder_request(
		impl_->host_operations.context
	);
}

bool ClassicOpSession::take_online_identity_reset_request() noexcept
{
	if(!impl_ || !impl_->host_operations.take_online_identity_reset_request) {
		return false;
	}
	return impl_->host_operations.take_online_identity_reset_request(
		impl_->host_operations.context
	);
}

bool ClassicOpSession::take_update_check_request() noexcept
{
	if(!impl_ || !impl_->host_operations.take_update_check_request) {
		return false;
	}
	return impl_->host_operations.take_update_check_request(
		impl_->host_operations.context
	);
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::begin_update_check()
{
	if(!impl_ || !impl_->host_operations.begin_update_check) {
		return std::unexpected(detail::session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return impl_->host_operations.begin_update_check(
		impl_->host_operations.context
	);
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::report_update_check(NewClassicUpdateCheckOutcome outcome)
{
	if(!impl_ || !impl_->host_operations.report_update_check) {
		return std::unexpected(detail::session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return impl_->host_operations.report_update_check(
		impl_->host_operations.context, outcome
	);
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::merge_online_board(NewClassicOnlineCacheDocument document)
{
	if(!impl_ || !impl_->operations.merge_online_board) {
		return std::unexpected(detail::session_error(
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
		return std::unexpected(detail::session_error(
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
	if(!impl_ || !impl_->host_operations.take_online_replay_download_request) {
		return std::nullopt;
	}
	return impl_->host_operations.take_online_replay_download_request(
		impl_->host_operations.context
	);
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::report_online_replay_download(
	const NewClassicOnlineReplayId &replay_id,
	NewClassicOnlineReplayDownloadOutcome outcome
)
{
	if(!impl_ || !impl_->host_operations.report_online_replay_download) {
		return std::unexpected(detail::session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return impl_->host_operations.report_online_replay_download(
		impl_->host_operations.context, replay_id, outcome
	);
}

std::optional<NewClassicReplayUploadRequest>
ClassicOpSession::take_online_replay_upload_request() noexcept
{
	if(!impl_ || !impl_->host_operations.take_online_replay_upload_request) {
		return std::nullopt;
	}
	return impl_->host_operations.take_online_replay_upload_request(
		impl_->host_operations.context
	);
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::report_online_replay_upload(
	std::uint8_t slot,
	NewClassicReplayUploadOutcome outcome
)
{
	if(!impl_ || !impl_->host_operations.report_online_replay_upload) {
		return std::unexpected(detail::session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return impl_->host_operations.report_online_replay_upload(
		impl_->host_operations.context, slot, outcome
	);
}

std::optional<NewClassicOnlineReplayRecord>
ClassicOpSession::take_online_replay_watch_request() noexcept
{
	if(!impl_ || !impl_->host_operations.take_online_replay_watch_request) {
		return std::nullopt;
	}
	return impl_->host_operations.take_online_replay_watch_request(
		impl_->host_operations.context
	);
}

std::expected<void, ClassicOpSessionError>
ClassicOpSession::report_online_replay_watch(
	const NewClassicOnlineReplayId &replay_id,
	std::optional<formats::ReplayDocument> document
)
{
	if(!impl_ || !impl_->host_operations.report_online_replay_watch) {
		return std::unexpected(detail::session_error(
			ClassicOpSessionErrorCode::backend_contract_failed
		));
	}
	return impl_->host_operations.report_online_replay_watch(
		impl_->host_operations.context, replay_id, std::move(document)
	);
}

std::shared_ptr<const NewClassicReplaySaveArt>
ClassicOpSession::replay_save_art() const noexcept
{
	if(!impl_ || !impl_->host_operations.replay_save_art) {
		return {};
	}
	return impl_->host_operations.replay_save_art(
		impl_->host_operations.context
	);
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
	return detail::vs_surface_source(win, win_completion, main_process);
}

GameInit ClassicOpSessionTestAccess::apply_key_config_autofire(
	GameInit resident,
	const std::optional<NewClassicKeyConfig> &key_config
) noexcept
{
	detail::apply_key_config_autofire(resident, key_config);
	return resident;
}

CoreInput ClassicOpSessionTestAccess::vs_cpu_main_input(
	const GameInit &resident,
	const ClassicMainInputSample &input,
	InputContext context,
	bool new_classic
) noexcept
{
	return detail::normalize_vs_cpu_main_input(resident, input, context, new_classic);
}

bool ClassicOpSessionTestAccess::attract_input_present(
	const ClassicMainInputSample &input
) noexcept
{
	return detail::attract_input_present(input);
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
		return std::unexpected(detail::session_error(
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
		return std::unexpected(detail::session_error(
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
