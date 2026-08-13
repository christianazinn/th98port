#include "th03/core/game.hpp"

#include "game_internal.hpp"

#include <memory>
#include <utility>

namespace th03 {

namespace detail {

StepResult gameplay_step_result(
	StepStatus status,
	const CanonicalState &input_boundary,
	const DeterministicState &state,
	bool defeat_none_before_frame,
	bool defeat_wait_before_frame
) noexcept
{
	StepResult result{status, input_boundary};
	result.defeat_none_before_frame = defeat_none_before_frame;
	result.defeat_none_after_frame = state.defeat_flag == DefeatFlag::none;
	if(defeat_wait_before_frame) {
		// defeat.cpp raises the MAIN loop's VSync wait during the update that
		// begins with lose_anim_time 0x30..1. The fatal collision only arms
		// 0x30 after that update, and the pre-time-1 update still waits twice.
		result.next_step_vsyncs = 2U;
	}
	return result;
}

} // namespace detail

namespace {

constexpr std::uint16_t input_cancel = 0x1000U;
constexpr std::uint16_t input_q = 0x4000U;

std::uint32_t initial_effective_reset_seed(const GameInit &init) noexcept
{
	if(init.compact_runtime_snapshot) {
		return init.compact_runtime_snapshot->round_reset_seed;
	}
	if(init.runtime_snapshot) {
		// A full runtime image bypasses reconstruction. `random_seed` is the only
		// format-neutral pre-reset provenance carried alongside that image.
		return init.random_seed;
	}
	return static_cast<std::uint32_t>(init.resident_rand);
}

void observe_live_pause_transition(GameplayObserver *observer) noexcept
{
	if(observer != nullptr) {
		observer->main_feedback_cue_requested(MainFeedbackCueRequested{
			.site = MainFeedbackCueSite::live_pause_transition,
		});
	}
}

StepResult finish_gameplay_frame_after_control(
	auto &impl,
	const CanonicalState &input_boundary,
	GameplayObserver *observer
)
{
	if(detail::warning_interstitial_begin(impl.state, observer)) {
		impl.warning_interstitial_samples_remaining = 27U;
		return detail::gameplay_step_result(
			StepStatus::phase_transition, input_boundary,
			impl.state, impl.defeat_none_before_frame,
			impl.defeat_wait_before_frame
		);
	}
	detail::gameplay_frame_suffix_after_interstitial(impl.state, observer);
	if(impl.state.round_complete) {
		return detail::gameplay_step_result(
			(impl.state.route == 1U)
				? StepStatus::round_transition
				: StepStatus::terminal,
			input_boundary,
			impl.state, impl.defeat_none_before_frame,
			impl.defeat_wait_before_frame
		);
	}
	return detail::gameplay_step_result(
		StepStatus::running, input_boundary, impl.state,
		impl.defeat_none_before_frame, impl.defeat_wait_before_frame
	);
}

StepResult finish_gameplay_frame(auto &impl, GameplayObserver *observer)
{
	const auto input_boundary = detail::canonical_state(impl.state);
	const auto live_pause_requested =
		detail::gameplay_frame_suffix_through_feedback_boundary(
			impl.state, observer
		);
	if(!impl.replay_pause_adapter && live_pause_requested) {
		impl.live_pause_phase = LivePausePhase::release_before_choice;
		impl.live_pause_return_context = InputContext::gameplay;
		impl.live_pause_input_boundary = input_boundary;
		observe_live_pause_transition(observer);
		auto result = StepResult{StepStatus::paused, input_boundary};
		result.defeat_none_before_frame = impl.defeat_none_before_frame;
		result.defeat_none_after_frame =
			impl.state.defeat_flag == detail::DefeatFlag::none;
		return result;
	}
	return finish_gameplay_frame_after_control(
		impl, input_boundary, observer
	);
}

[[nodiscard]] bool any_pause_input(const CoreInput &input) noexcept
{
	// sub_C7A5() waits on input_sp, not on either gameplay-player mask.
	// The platform mapper mirrors only the physical keys that the donor's
	// input_reset_sense_key_held() contributes to input_sp into [system].
	return input.system != 0U;
}

void live_pause_input_latch(auto &impl, const CoreInput &input) noexcept
{
	// The blocking loop repeatedly overwrites all three source input words but
	// does not pass through t3case_frame_io() or advance a gameplay frame.
	impl.state.input_player = {input.p1, input.p2};
	impl.state.input_system = input.system;
}

StepResult finish_warning_interstitial_sample(
	auto &impl,
	GameplayObserver *observer
)
{
	if(observer != nullptr) {
		observer->warning_interstitial_sample(WarningInterstitialSample{
			.ordinal = static_cast<std::uint8_t>(
				27U - impl.warning_interstitial_samples_remaining
			),
		});
	}
	const auto input_boundary = detail::canonical_state(impl.state);
	--impl.warning_interstitial_samples_remaining;
	if(impl.warning_interstitial_samples_remaining != 0U) {
		return StepResult{StepStatus::running, input_boundary};
	}
	detail::gameplay_frame_suffix_after_interstitial(impl.state, observer);
	if(impl.state.round_complete) {
		return detail::gameplay_step_result(
			(impl.state.route == 1U)
				? StepStatus::round_transition
				: StepStatus::terminal,
			input_boundary, impl.state, impl.defeat_none_before_frame,
			impl.defeat_wait_before_frame
		);
	}
	return detail::gameplay_step_result(
		StepStatus::phase_transition, input_boundary, impl.state,
		impl.defeat_none_before_frame, impl.defeat_wait_before_frame
	);
}

StepResult finish_live_pause(
	auto &impl,
	bool resumed,
	GameplayObserver *observer
)
{
	const auto return_context = impl.live_pause_return_context;
	impl.live_pause_phase = LivePausePhase::inactive;
	if(resumed) {
		observe_live_pause_transition(observer);
	}
	if(return_context == InputContext::interstitial) {
		return finish_warning_interstitial_sample(impl, observer);
	}
	return finish_gameplay_frame_after_control(
		impl, impl.live_pause_input_boundary, observer
	);
}

StepResult step_live_pause(
	auto &impl,
	const CoreInput &input,
	GameplayObserver *observer
)
{
	if(impl.live_pause_phase == LivePausePhase::inactive) {
		return StepResult{
			StepStatus::invalid_input_context,
			detail::canonical_state(impl.state),
		};
	}
	live_pause_input_latch(impl, input);
	const auto paused = [&]() {
		return StepResult{
			StepStatus::paused,
			impl.live_pause_input_boundary,
		};
	};
	switch(impl.live_pause_phase) {
	case LivePausePhase::release_before_choice:
		if(!any_pause_input(input)) {
			impl.live_pause_phase = LivePausePhase::wait_for_choice;
		}
		return paused();
	case LivePausePhase::wait_for_choice:
		if((input.system & input_q) != 0U) {
			impl.state.session_exit_requested = true;
			return finish_live_pause(impl, false, observer);
		}
		if((input.system & input_cancel) != 0U) {
			impl.live_pause_phase = LivePausePhase::release_after_resume;
		}
		return paused();
	case LivePausePhase::release_after_resume:
		if(any_pause_input(input)) {
			return paused();
		}
		return finish_live_pause(impl, true, observer);
	case LivePausePhase::inactive:
		break;
	}
	return StepResult{
		StepStatus::invalid_input_context,
		detail::canonical_state(impl.state),
	};
}

} // namespace

Game::Impl::Impl(
	const GameInit &init,
	GameContent content_,
	GameOptions options,
	GameplayObserver *observer
) :
	state(detail::initialize_first_round(
		init, content_.enemy_formations.size(), observer
	)),
	initial_init(init),
	autofire_mask(init.autofire_mask),
	content(std::move(content_)),
	replay_pause_adapter(
		options.input_protocol == InputProtocol::recorded_replay
	),
	effective_reset_seed(initial_effective_reset_seed(init))
{
	state.three_digit_combos = options.three_digit_combos;
	round_start = detail::round_start_observation(
		state, initial_init, effective_reset_seed
	);
}

Game::Game(
	GameInit init,
	GameContent content,
	GameOptions options,
	GameplayObserver *observer
) :
	impl_(std::make_unique<Impl>(init, std::move(content), options, observer))
{}

Game::Game(Game &&) noexcept = default;
Game &Game::operator=(Game &&) noexcept = default;
Game::~Game() = default;

std::expected<Game, Error> Game::create(const GameInit &init)

{
	return create(init, GameContent{}, GameOptions{});
}

std::expected<Game, Error> Game::create(const GameInit &init, GameContent content)
{
	return create(init, std::move(content), GameOptions{});
}

std::expected<Game, Error> Game::create(
	const GameInit &init,
	GameContent content,
	GameOptions options,
	GameplayObserver *observer
)
{
	if((init.autofire_mask & 0xFCU) != 0U) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"autofire mask contains bits outside P1/P2",
		});
	}
	if(
		init.game_mode != detail::game_mode_story &&
		init.game_mode != detail::game_mode_demo &&
		init.game_mode != detail::game_mode_vs_1p_cpu &&
		init.game_mode != detail::game_mode_vs_1p_2p &&
		init.game_mode != detail::game_mode_vs_cpu_cpu
	) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"game mode is not a source TH03 gameplay mode",
		});
	}
	if(init.rank > 3U) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"rank is outside the source range 0..3",
		});
	}
	if(
		init.demo_number != 0U &&
		init.game_mode != detail::game_mode_demo
	) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"nonzero demo number requires GM_DEMO",
		});
	}
	if(
		init.game_mode == detail::game_mode_story &&
		init.remaining_credits > 3U
	) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"Story remaining credits exceed the source resident range 0..3",
		});
	}
	if(
		init.game_mode == detail::game_mode_story &&
		init.story_stage >= detail::story_stage_after_clear
	) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"Story gameplay stage must be in the source range 0..8",
		});
	}
	if(content.enemy_formations.size() > 24U) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"game content contains more than 24 enemy formations",
		});
	}
	if(content.enemy_formations.size() == 1U) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"one enemy formation cannot satisfy the source non-repeat draw",
		});
	}
	if(init.practice_rules) {
		if(init.game_mode != detail::game_mode_vs_1p_cpu) {
			return std::unexpected(Error{
				ErrorCode::invalid_value,
				"Practice rules require GM_VS_1P_CPU",
			});
		}
		if(
			static_cast<std::uint8_t>(init.practice_rules->retry_cpu_timer) > 2U ||
			init.practice_rules->retry_cpu_damage > 3U
		) {
			return std::unexpected(Error{
				ErrorCode::invalid_value,
				"Practice retry rules are outside the source configuration range",
			});
		}
		if(
			init.practice_rules->retry_cpu_timer ==
				CpuTimerPolicy::story_native &&
			init.story_stage >= detail::story_stage_after_clear
		) {
			return std::unexpected(Error{
				ErrorCode::invalid_value,
				"Story-native Practice timing requires a source Story stage 0..8",
			});
		}
		if(!init.round_start_snapshot) {
			return std::unexpected(Error{
				ErrorCode::invalid_value,
				"Practice rules require a round-start snapshot",
			});
		}
	}
	if(init.runtime_snapshot && init.compact_runtime_snapshot) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"full and compact runtime snapshots are mutually exclusive",
		});
	}
	if(
		init.compact_runtime_snapshot &&
		!detail::compact_runtime_snapshot_is_valid(
			*init.compact_runtime_snapshot, content.enemy_formations.size()
		)
	) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"compact runtime snapshot cannot reconstruct the supplied formations",
		});
	}
	for(const auto paletted_character : init.paletted_character) {
		if(paletted_character < 1 || paletted_character > 18) {
			return std::unexpected(Error{
				ErrorCode::invalid_value,
				"paletted character must be in the source range 1..18",
			});
		}
	}
	if(init.round_start_snapshot) {
		if(!init.runtime_snapshot && !init.compact_runtime_snapshot) {
			return std::unexpected(Error{
				ErrorCode::invalid_value,
				"round-start snapshot requires its matching runtime snapshot",
			});
		}
		const auto &snapshot = *init.round_start_snapshot;
		if(init.practice_rules) {
			const auto expected = detail::practice_cpu_safety_frames(
				init, snapshot.state.round_id
			);
			if(
				snapshot.state.cpu_safety_frames !=
					std::array<std::uint16_t, 2>{expected, expected}
			) {
				return std::unexpected(Error{
					ErrorCode::invalid_value,
					"Practice CPU safety does not match its timer policy",
				});
			}
		}
		if(snapshot.carry.background_phase > 2U) {
			return std::unexpected(Error{
				ErrorCode::invalid_value,
				"round-start background phase is outside the source range 0..2",
			});
		}
		if(snapshot.carry.result_phase > 2U) {
			return std::unexpected(Error{
				ErrorCode::invalid_value,
				"round-start result phase is outside the source range 0..2",
			});
		}
		for(std::size_t player = 0U; player < 2U; ++player) {
			if(snapshot.carry.chain_cursor[player] >= 16U) {
				return std::unexpected(Error{
					ErrorCode::invalid_value,
					"round-start chain cursor is outside the source range 0..15",
				});
			}
			if(snapshot.carry.gauge_attack_next[player] > 5U) {
				return std::unexpected(Error{
					ErrorCode::invalid_value,
					"round-start gauge-attack flag is outside the source range 0..5",
				});
			}
		}
	}
	return Game(init, std::move(content), options, observer);
}

StepResult Game::step(
	CoreInput input,
	InputContext context,
	GameplayObserver *observer
)
{
	if(impl_->state.round_complete) {
		return StepResult{
			(impl_->state.route == 1U)
				? StepStatus::round_transition
				: StepStatus::terminal,
			detail::canonical_state(impl_->state),
		};
	}
	if(context == InputContext::pause) {
		return step_live_pause(*impl_, input, observer);
	}
	if(context == InputContext::interstitial) {
		if(impl_->replay_pause_deferred) {
			impl_->state.input_player = {input.p1, input.p2};
			impl_->state.input_system = input.system;
			impl_->state.global_frame++;
			return StepResult{
				StepStatus::running,
				detail::canonical_state(impl_->state),
			};
		}
		if(impl_->warning_interstitial_samples_remaining == 0U) {
			return StepResult{
				StepStatus::invalid_input_context,
				detail::canonical_state(impl_->state),
			};
		}
		detail::interstitial_input_latch(impl_->state, input);
		if(!impl_->replay_pause_adapter &&
			(impl_->state.input_system & input_cancel) != 0U) {
			const bool both_cpu =
				impl_->state.players[0U].is_cpu != 0U &&
				impl_->state.players[1U].is_cpu != 0U;
			if(both_cpu) {
				impl_->state.session_exit_requested = true;
			} else {
				impl_->live_pause_phase = LivePausePhase::release_before_choice;
				impl_->live_pause_return_context = InputContext::interstitial;
				impl_->live_pause_input_boundary =
					detail::canonical_state(impl_->state);
				observe_live_pause_transition(observer);
				return StepResult{
					StepStatus::paused, impl_->live_pause_input_boundary
				};
			}
		}
		return finish_warning_interstitial_sample(*impl_, observer);
	}
	if(
		impl_->warning_interstitial_samples_remaining != 0U ||
		impl_->replay_pause_deferred ||
		impl_->live_pause_phase != LivePausePhase::inactive
	) {
		return StepResult{
			StepStatus::invalid_input_context,
			detail::canonical_state(impl_->state),
		};
	}
	impl_->defeat_none_before_frame =
		impl_->state.defeat_flag == detail::DefeatFlag::none;
	impl_->defeat_wait_before_frame = false;
	if(impl_->state.defeat_flag != detail::DefeatFlag::banner) {
		for(const auto &player : impl_->state.players) {
			if(player.lose_anim_time >= 1U &&
				player.lose_anim_time <= 0x30U) {
				impl_->defeat_wait_before_frame = true;
				break;
			}
		}
	}
	if(!detail::gameplay_frame_prefix(
		impl_->state, impl_->content, observer
	)) {
		return StepResult{
			StepStatus::simulation_not_implemented,
			detail::canonical_state(impl_->state),
		};
	}
	detail::gameplay_input_latch(impl_->state, input, impl_->autofire_mask);
	const auto input_boundary = detail::canonical_state(impl_->state);
	if(
		impl_->replay_pause_adapter &&
		(impl_->state.input_system & input_cancel) != 0U
	) {
		impl_->replay_pause_gameplay_input = impl_->state.input_player;
		impl_->replay_pause_input_boundary = input_boundary;
		impl_->replay_pause_deferred = true;
		auto result = StepResult{StepStatus::phase_transition, input_boundary};
		result.defeat_none_before_frame = impl_->defeat_none_before_frame;
		result.defeat_none_after_frame =
			impl_->state.defeat_flag == detail::DefeatFlag::none;
		return result;
	}
	return finish_gameplay_frame(*impl_, observer);
}

std::expected<StepResult, Error> Game::finish_deferred_frame(
	DeferredFrameAction action,
	GameplayObserver *observer
)
{
	if(!impl_->replay_pause_deferred || impl_->state.round_complete) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"game has no replay-pause frame awaiting completion",
		});
	}
	impl_->state.input_player = impl_->replay_pause_gameplay_input;
	impl_->state.input_system = 0U;
	impl_->replay_pause_deferred = false;
	if(action == DeferredFrameAction::exit_session) {
		impl_->state.session_exit_requested = true;
	}
	auto result = finish_gameplay_frame(*impl_, observer);
	result.input_boundary_state = impl_->replay_pause_input_boundary;
	return result;
}

bool Game::deferred_frame_pending() const noexcept
{
	return impl_->replay_pause_deferred;
}

CanonicalState Game::canonical_state() const
{
	return detail::canonical_state(impl_->state);
}

std::uint32_t Game::live_random_seed() const noexcept
{
	return impl_->state.random_seed;
}

RoundStartObservation Game::round_start_observation() const
{
	return impl_->round_start;
}

std::expected<void, Error> Game::start_next_round()
{
	if(!impl_->state.round_complete || impl_->state.route != 1U) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"game is not at a route-1 round boundary",
		});
	}
	impl_->effective_reset_seed = impl_->state.random_seed;
	detail::initialize_repeat_round(
		impl_->state,
		impl_->initial_init,
		impl_->content.enemy_formations.size()
	);
	impl_->round_start = detail::round_start_observation(
		impl_->state, impl_->initial_init, impl_->effective_reset_seed
	);
	impl_->warning_interstitial_samples_remaining = 0U;
	impl_->replay_pause_deferred = false;
	impl_->live_pause_phase = LivePausePhase::inactive;
	return {};
}

std::expected<void, Error> Game::finish_main_process()
{
	if(
		!impl_->state.round_complete || impl_->state.route == 1U ||
		impl_->main_process_finished
	) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"game is not at an unfinished terminal MAIN boundary",
		});
	}
	// main_entry() performs this write after t3case_route() and before
	// t3case_finish(), for every route that leaves MAIN.
	impl_->state.resident_rand = static_cast<std::int32_t>(
		impl_->state.round_frame
	);
	impl_->main_process_finished = true;
	return {};
}

namespace detail {

namespace {

SessionTransition project_session_transition(
	const DeterministicState &state,
	const GameInit &initial_init,
	std::int32_t resident_rand
)
{
	SessionTransition transition{};
	transition.resident = initial_init;
	auto &resident = transition.resident;
	resident.global_frame = state.global_frame;
	resident.resident_rand = resident_rand;
	resident.random_seed = state.round_frame;
	resident.story_lives = state.story_lives;
	resident.skill = state.skill;
	resident.winner = state.winner;
	resident.runtime_snapshot.reset();
	resident.compact_runtime_snapshot.reset();
	resident.round_start_snapshot.reset();
	return transition;
}

} // namespace

std::expected<SessionTransition, Error> build_session_transition(
	const DeterministicState &state,
	const GameInit &initial_init,
	bool main_process_finished,
	std::optional<ContinueChoice> continue_choice
)
{
	if(!main_process_finished || state.route == 1U) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"game is not at a finished MAIN boundary",
		});
	}

	auto transition = project_session_transition(
		state, initial_init, state.resident_rand
	);
	auto &resident = transition.resident;

	// Route 0 jumps directly back to OP. In particular, MAIN does not perform
	// its otherwise-unconditional Story-stage increment or score persistence.
	if(state.route == 0U) {
		transition.action = SessionAction::exit_to_operator;
		return transition;
	}

	// sub_9778() persists the appropriate live score before returning route 2.
	// Story always registers P1, including a loss; VS registers its independently
	// selected resident winner. MAIN increments the stage byte after T3CASE's
	// finish row and before starting MAINL, even outside Story mode.
	const auto score_owner = static_cast<std::size_t>(
		(state.game_mode == detail::game_mode_story || state.practice_uses_stock)
			? 0U : static_cast<std::uint8_t>(state.winner)
	);
	if(score_owner >= state.score.size()) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"resident winner is outside the source player range",
		});
	}
	resident.last_score = {};
	resident.last_score[score_owner] = state.score[score_owner];
	resident.story_stage = exact::wrap_add(
		resident.story_stage, std::uint8_t{1U}
	);

	if(state.game_mode != detail::game_mode_story) {
		transition.action = SessionAction::exit_to_operator;
		return transition;
	}

	if(state.winner == 0) {
		if(resident.story_stage == detail::story_stage_after_clear) {
			// sub_9887() first performs a terminal out-of-bounds opponent read.
			// It cannot affect later gameplay, so the architecture-neutral core
			// deliberately retains the last valid P2 value rather than modeling an
			// executable-layout byte. ending_staff_and_regist() then sets stage 99
			// before invoking high-score registration.
			resident.story_stage = detail::story_stage_ending;
			transition.high_score_registration = resident;
			transition.action = SessionAction::ending;
			return transition;
		}
		if(resident.story_stage >= resident.story_opponents.size()) {
			return std::unexpected(Error{
				ErrorCode::invalid_value,
				"Story stage cannot start another gameplay process",
			});
		}
		resident.paletted_character[1] =
			resident.story_opponents[resident.story_stage];
		const auto opponent = static_cast<std::uint8_t>(
			character_index_from_paletted(resident.paletted_character[1])
		);
		if(
			resident.story_stage < detail::story_stage_chiyuri &&
			opponent >= detail::character_chiyuri
		) {
			resident.paletted_character[1] =
				detail::paletted_character_reimu_p1;
		}
		transition.action = SessionAction::start_gameplay;
		return transition;
	}

	// MAINL registers the P1 score before continue_menu() clears both arrays.
	transition.high_score_registration = resident;
	resident.last_score = {};
	if(resident.remaining_credits == 0U) {
		// The no-credit early return happens before stage/lives are restored.
		transition.action = SessionAction::exit_to_operator;
		return transition;
	}
	if(!continue_choice) {
		transition.action = SessionAction::request_continue;
		return transition;
	}

	resident.story_stage = exact::wrap_subtract(
		resident.story_stage, std::uint8_t{1U}
	);
	resident.story_lives = 2U;
	if(*continue_choice == ContinueChoice::accept) {
		resident.remaining_credits = exact::wrap_subtract(
			resident.remaining_credits, std::uint8_t{1U}
		);
		transition.action = SessionAction::start_gameplay;
	} else {
		transition.action = SessionAction::exit_to_operator;
	}
	return transition;
}

std::expected<SessionTransition, Error> build_abort_transition(
	const DeterministicState &state,
	const GameInit &initial_init
)
{
	// main_entry() performs this write after choosing route 0 and before its
	// canonical transition builder runs. Project it without mutating a live
	// Game so host-owned replay abort cannot enter deterministic simulation.
	return project_session_transition(
		state,
		initial_init,
		static_cast<std::int32_t>(state.round_frame)
	);
}

} // namespace detail

std::expected<SessionTransition, Error> Game::session_transition(
	std::optional<ContinueChoice> continue_choice
) const
{
	return detail::build_session_transition(
		impl_->state,
		impl_->initial_init,
		impl_->main_process_finished,
		continue_choice
	);
}

std::expected<SessionTransition, Error> Game::abort_transition() const
{
	return detail::build_abort_transition(
		impl_->state,
		impl_->initial_init
	);
}

std::expected<GameInit, Error> Game::next_story_stage_init() const
{
	auto transition = session_transition();
	if(!transition || transition->action != SessionAction::start_gameplay) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"game is not at a continuing Story-stage boundary",
		});
	}
	return transition->resident;
}

} // namespace th03
