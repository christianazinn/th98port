#include "th03/core/game.hpp"

#include "game_internal.hpp"

#include <memory>
#include <utility>

namespace th03 {

namespace {

constexpr std::uint16_t input_cancel = 0x1000U;

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

StepResult finish_gameplay_frame(auto &impl)
{
	const auto input_boundary = detail::canonical_state(impl.state);
	if(detail::gameplay_frame_suffix_before_interstitial(impl.state)) {
		impl.warning_interstitial_samples_remaining = 27U;
		return StepResult{StepStatus::phase_transition, input_boundary};
	}
	detail::gameplay_frame_suffix_after_interstitial(impl.state);
	if(impl.state.round_complete) {
		return StepResult{
			(impl.state.route == 1U)
				? StepStatus::round_transition
				: StepStatus::terminal,
			input_boundary,
		};
	}
	return StepResult{StepStatus::running, input_boundary};
}

} // namespace

Game::Impl::Impl(
	const GameInit &init, GameContent content_, GameOptions options
) :
	state(detail::initialize_first_round(
		init, content_.enemy_formations.size()
	)),
	initial_init(init),
	autofire_mask(init.autofire_mask),
	content(std::move(content_)),
	replay_pause_adapter(
		options.input_protocol == InputProtocol::recorded_replay
	),
	effective_reset_seed(initial_effective_reset_seed(init))
{
	round_start = detail::round_start_observation(
		state, initial_init, effective_reset_seed
	);
}

Game::Game(GameInit init, GameContent content, GameOptions options) :
	impl_(std::make_unique<Impl>(init, std::move(content), options))
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
	const GameInit &init, GameContent content, GameOptions options
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
	return Game(init, std::move(content), options);
}

StepResult Game::step(CoreInput input, InputContext context)
{
	if(impl_->state.round_complete) {
		return StepResult{
			(impl_->state.route == 1U)
				? StepStatus::round_transition
				: StepStatus::terminal,
			detail::canonical_state(impl_->state),
		};
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
		const auto input_boundary = detail::canonical_state(impl_->state);
		impl_->warning_interstitial_samples_remaining--;
		if(impl_->warning_interstitial_samples_remaining == 0U) {
			detail::gameplay_frame_suffix_after_interstitial(impl_->state);
			if(impl_->state.round_complete) {
				return StepResult{
					(impl_->state.route == 1U)
						? StepStatus::round_transition
						: StepStatus::terminal,
					input_boundary,
				};
			}
			return StepResult{StepStatus::phase_transition, input_boundary};
		}
		return StepResult{StepStatus::running, input_boundary};
	}
	if(
		impl_->warning_interstitial_samples_remaining != 0U ||
		impl_->replay_pause_deferred
	) {
		return StepResult{
			StepStatus::invalid_input_context,
			detail::canonical_state(impl_->state),
		};
	}
	if(!detail::gameplay_frame_prefix(impl_->state, impl_->content)) {
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
		return StepResult{StepStatus::phase_transition, input_boundary};
	}
	return finish_gameplay_frame(*impl_);
}

std::expected<StepResult, Error> Game::finish_deferred_frame(
	DeferredFrameAction action
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
	auto result = finish_gameplay_frame(*impl_);
	result.input_boundary_state = impl_->replay_pause_input_boundary;
	return result;
}

CanonicalState Game::canonical_state() const
{
	return detail::canonical_state(impl_->state);
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

	SessionTransition transition{};
	transition.resident = initial_init;
	auto &resident = transition.resident;
	resident.global_frame = state.global_frame;
	resident.resident_rand = state.resident_rand;
	resident.random_seed = state.round_frame;
	resident.story_lives = state.story_lives;
	resident.skill = state.skill;
	resident.winner = state.winner;
	resident.runtime_snapshot.reset();
	resident.compact_runtime_snapshot.reset();
	resident.round_start_snapshot.reset();

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
