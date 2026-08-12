#include "th03/replay/run.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace th03::replay {
namespace {

std::optional<std::string> observation_mismatch(
	const RoundStartObservation &expected,
	const RoundStartObservation &actual
)
{
#define TH03_CHECK(path) \
	if(expected.path != actual.path) { \
		return #path; \
	}
	TH03_CHECK(start.global_frame)
	TH03_CHECK(start.resident_rand)
	TH03_CHECK(start.random_seed)
	TH03_CHECK(start.game_mode)
	TH03_CHECK(start.rank)
	TH03_CHECK(start.key_mode)
	TH03_CHECK(start.story_stage)
	TH03_CHECK(start.story_lives)
	TH03_CHECK(start.remaining_credits)
	TH03_CHECK(start.skill)
	TH03_CHECK(start.demo_number)
	TH03_CHECK(start.is_cpu)
	TH03_CHECK(start.paletted_character)
	TH03_CHECK(start.story_opponents)
	TH03_CHECK(start.winner)
	TH03_CHECK(start.show_score_menu)
	TH03_CHECK(start.fast_op_animation)
	TH03_CHECK(start.last_score)
	TH03_CHECK(start.autofire_mask)
	if(expected.start.practice_rules.has_value() !=
		actual.start.practice_rules.has_value()) {
		return "start.practice_rules";
	}
	if(expected.start.practice_rules) {
		TH03_CHECK(start.practice_rules->uses_stock)
		TH03_CHECK(start.practice_rules->retry_cpu_timer)
		TH03_CHECK(start.practice_rules->retry_cpu_damage)
	}
	TH03_CHECK(effective_reset_seed)
	TH03_CHECK(runtime.random_seed)
	TH03_CHECK(runtime.randring)
	TH03_CHECK(runtime.randring_cursor)
	TH03_CHECK(runtime.formation_type_ring)
	TH03_CHECK(runtime.formation_position_ring)
	TH03_CHECK(runtime.formation_cursor)
	TH03_CHECK(runtime.cpu_charge_ring)
	TH03_CHECK(runtime.cpu_charge_cursor)
	for(std::size_t player = 0U; player < expected.runtime.players.size(); ++player) {
		const auto &expected_player = expected.runtime.players[player];
		const auto &actual_player = actual.runtime.players[player];
#define TH03_CHECK_PLAYER(path) \
		if(expected_player.path != actual_player.path) { \
			return "runtime.players[" + std::to_string(player) + "]." #path; \
		}
		TH03_CHECK_PLAYER(center_x)
		TH03_CHECK_PLAYER(center_y)
		TH03_CHECK_PLAYER(halfhearts)
		TH03_CHECK_PLAYER(invincibility_time)
		TH03_CHECK_PLAYER(gauge_charge_speed)
		TH03_CHECK_PLAYER(gauge_charged)
		TH03_CHECK_PLAYER(gauge_avail)
		TH03_CHECK_PLAYER(bombs)
		TH03_CHECK_PLAYER(shot_active)
		TH03_CHECK_PLAYER(cpu_frame)
#undef TH03_CHECK_PLAYER
	}
	TH03_CHECK(round.state.round_id)
	TH03_CHECK(round.state.rounds_won)
	TH03_CHECK(round.state.score)
	TH03_CHECK(round.state.round_speed)
	TH03_CHECK(round.state.bullet_base_speed)
	TH03_CHECK(round.state.gauge_attack_level)
	TH03_CHECK(round.state.boss_attack_level)
	TH03_CHECK(round.state.cpu_hit_damage_additional)
	TH03_CHECK(round.state.extends_gained)
	TH03_CHECK(round.state.cpu_safety_frames)
	TH03_CHECK(round.carry.cpu_dodge_strategy)
	TH03_CHECK(round.carry.human_movement_last)
	TH03_CHECK(round.carry.spell_ready_frames)
	TH03_CHECK(round.carry.combo_bonus_max)
	TH03_CHECK(round.carry.combo_hits_max)
	TH03_CHECK(round.carry.shot_cycle)
	TH03_CHECK(round.carry.boss_panic_fired)
	TH03_CHECK(round.carry.gauge_attack_next)
	TH03_CHECK(round.carry.cpu_shot_decision)
	TH03_CHECK(round.carry.background_phase)
	for(std::size_t player = 0U; player < expected.round.carry.combo.size(); ++player) {
		const auto &expected_combo = expected.round.carry.combo[player];
		const auto &actual_combo = actual.round.carry.combo[player];
		if(expected_combo.time != actual_combo.time) {
			return "round.carry.combo[" + std::to_string(player) + "].time";
		}
		if(expected_combo.hits_highest != actual_combo.hits_highest) {
			return "round.carry.combo[" + std::to_string(player) + "].hits_highest";
		}
		if(expected_combo.bonus_total != actual_combo.bonus_total) {
			return "round.carry.combo[" + std::to_string(player) + "].bonus_total";
		}
	}
	TH03_CHECK(round.carry.chain_cursor)
	TH03_CHECK(round.carry.chain_hits)
	TH03_CHECK(round.carry.chain_pellet_and_fireball_value)
	TH03_CHECK(round.carry.chain_charge_fireball)
	TH03_CHECK(round.carry.chain_charge_exatt)
	TH03_CHECK(round.carry.randring_cursor)
	TH03_CHECK(round.carry.result_phase)
	TH03_CHECK(round.carry.fireball_generation_previous)
#undef TH03_CHECK
	return std::nullopt;
}

constexpr std::uint16_t input_up = 0x0001U;
constexpr std::uint16_t input_down = 0x0002U;
constexpr std::uint16_t input_shot = 0x0020U;
constexpr std::uint16_t input_cancel = 0x1000U;
constexpr std::uint16_t input_ok = 0x2000U;
constexpr std::uint16_t input_q = 0x4000U;

enum class PausePhase : std::uint8_t {
	inactive,
	wait_initial_release,
	menu,
	wait_menu_release,
	wait_resume_release,
};

class PauseCoordinator {
public:
	void begin() noexcept
	{
		phase_ = PausePhase::wait_initial_release;
		selection_ = 0U;
	}

	bool active() const noexcept
	{
		return phase_ != PausePhase::inactive;
	}

	void reset() noexcept
	{
		phase_ = PausePhase::inactive;
		selection_ = 0U;
	}

	std::optional<DeferredFrameAction> sample(const CoreInput &input) noexcept
	{
		const bool held = input.p1 != 0U || input.p2 != 0U ||
			input.system != 0U;
		switch(phase_) {
		case PausePhase::wait_initial_release:
			if(!held) {
				phase_ = PausePhase::menu;
			}
			return std::nullopt;
		case PausePhase::wait_menu_release:
			if(!held) {
				phase_ = PausePhase::menu;
			}
			return std::nullopt;
		case PausePhase::wait_resume_release:
			if(held) {
				return std::nullopt;
			}
			phase_ = PausePhase::inactive;
			return DeferredFrameAction::resume;
		case PausePhase::menu:
			break;
		case PausePhase::inactive:
			return std::nullopt;
		}
		if((input.system & input_q) != 0U) {
			phase_ = PausePhase::inactive;
			return DeferredFrameAction::exit_session;
		}
		if((input.system & input_cancel) != 0U) {
			phase_ = PausePhase::wait_resume_release;
			return std::nullopt;
		}
		if((input.system & input_up) != 0U) {
			selection_ = (selection_ == 0U) ? 3U :
				static_cast<std::uint8_t>(selection_ - 1U);
			phase_ = PausePhase::wait_menu_release;
			return std::nullopt;
		}
		if((input.system & input_down) != 0U) {
			selection_ = (selection_ == 3U) ? 0U :
				static_cast<std::uint8_t>(selection_ + 1U);
			phase_ = PausePhase::wait_menu_release;
			return std::nullopt;
		}
		if((input.system & (input_ok | input_shot)) == 0U) {
			return std::nullopt;
		}
		if(selection_ == 0U) {
			phase_ = PausePhase::wait_resume_release;
			return std::nullopt;
		}
		phase_ = PausePhase::inactive;
		return DeferredFrameAction::exit_session;
	}

private:
	PausePhase phase_ = PausePhase::inactive;
	std::uint8_t selection_ = 0U;
};

struct Runner {
	class TelemetryObserver final : public GameplayObserver {
	public:
		void reset_round() noexcept
		{
			real_frames = 0U;
			timed_frames = 0U;
			timer_active = true;
			defeat_started_this_step = false;
			result_summary.reset();
		}

		void begin_step() noexcept
		{
			defeat_started_this_step = false;
		}

		void count_step(std::uint8_t vsyncs, bool slowdown_eligible) noexcept
		{
			if(!timer_active) {
				return;
			}
			if(defeat_started_this_step) {
				timer_active = false;
				return;
		}
		real_frames += vsyncs;
		if(slowdown_eligible) {
			timed_frames++;
		}
		}

		void main_feedback_cue_requested(
			const MainFeedbackCueRequested &cue
		) noexcept override
		{
			if(cue.site == MainFeedbackCueSite::player_defeat_explode) {
				defeat_started_this_step = true;
			}
		}

		void main_result_state(const MainResultState &state) noexcept override
		{
			if(state.summary_active) {
				result_summary = state;
			}
		}

		std::uint32_t real_frames = 0U;
		std::uint32_t timed_frames = 0U;
		bool timer_active = true;
		bool defeat_started_this_step = false;
		std::optional<MainResultState> result_summary;
	};

	RunResult result{};
	std::optional<Game> simulation;
	ProcessId process = ProcessId::main;
	bool main_terminal = false;
	std::optional<GameInit> next_story_stage;
	PauseCoordinator pause;
	TelemetryObserver telemetry;
	const GameContent &content;

	CompletedRound completed_round() const
	{
		return CompletedRound{
			.sample_index = result.consumed_samples,
			.state = simulation->canonical_state(),
			.real_frames = telemetry.real_frames,
			.timed_frames = telemetry.timed_frames,
			.result_summary = telemetry.result_summary,
		};
	}

	std::expected<void, Error> handle_step(const StepResult &step)
	{
		if(step.status == StepStatus::round_transition) {
			result.completed_rounds.push_back(completed_round());
			auto next_round = simulation->start_next_round();
			if(!next_round) {
				return std::unexpected(next_round.error());
			}
			result.round_starts.push_back(RoundStartRecord{
				.sample_index = result.consumed_samples,
				.observation = simulation->round_start_observation(),
			});
			telemetry.reset_round();
			return {};
		}
		if(step.status != StepStatus::terminal) {
			return {};
		}

		result.completed_rounds.push_back(completed_round());
		auto finished = simulation->finish_main_process();
		if(!finished) {
			return std::unexpected(finished.error());
		}
		result.terminal_state = simulation->canonical_state();
		auto transition = simulation->session_transition();
		if(!transition) {
			return std::unexpected(transition.error());
		}
		result.terminal_transition = *transition;
		if(transition->action == SessionAction::start_gameplay) {
			next_story_stage = transition->resident;
		}
		main_terminal = true;
		return {};
	}

	std::expected<void, Error> finish_main_if_needed()
	{
		if(main_terminal) {
			return {};
		}
		result.completed_rounds.push_back(completed_round());
		auto finished = simulation->finish_main_process();
		if(!finished) {
			return std::unexpected(finished.error());
		}
		result.terminal_state = simulation->canonical_state();
		auto transition = simulation->session_transition();
		if(!transition) {
			return std::unexpected(transition.error());
		}
		result.terminal_transition = *transition;
		if(transition->action == SessionAction::start_gameplay) {
			next_story_stage = transition->resident;
		}
		main_terminal = true;
		return {};
	}
};

} // namespace

std::expected<RunResult, Error> run_session(
	const ReplaySession &session,
	const GameContent &content
)
{
	const auto options = GameOptions{
		.input_protocol = InputProtocol::recorded_replay,
	};
	auto game = Game::create(session.init, content, options);
	if(!game) {
		return std::unexpected(game.error());
	}
	Runner runner{
		.result = {},
		.simulation = std::move(*game),
		.process = ProcessId::main,
		.main_terminal = false,
		.next_story_stage = std::nullopt,
		.pause = {},
		.telemetry = {},
		.content = content,
	};
	runner.result.round_starts.push_back(RoundStartRecord{
		.sample_index = 0U,
		.observation = runner.simulation->round_start_observation(),
	});

	for(const auto &record : session.records) {
		if(const auto *sample = std::get_if<InputSample>(&record)) {
			if(runner.process == ProcessId::mainl) {
				if(sample->phase != InputPhase::interstitial) {
					runner.result.status = RunStatus::input_context_mismatch;
					return runner.result;
				}
				runner.result.consumed_samples++;
				continue;
			}
			if(runner.main_terminal || !runner.simulation) {
				runner.result.status = RunStatus::process_boundary_mismatch;
				return runner.result;
			}

			const auto context = (sample->phase == InputPhase::gameplay)
				? InputContext::gameplay : InputContext::interstitial;
			runner.telemetry.begin_step();
			auto step = runner.simulation->step(
				sample->input, context, &runner.telemetry
			);
			if(step.status == StepStatus::invalid_input_context) {
				runner.result.status = RunStatus::input_context_mismatch;
				return runner.result;
			}
			if(step.status == StepStatus::simulation_not_implemented) {
				runner.result.status = RunStatus::simulation_not_implemented;
				return runner.result;
			}
			if(
				context == InputContext::gameplay &&
				(sample->input.system & input_cancel) != 0U
			) {
				runner.pause.begin();
			} else if(
				context == InputContext::interstitial && runner.pause.active()
			) {
				if(const auto action = runner.pause.sample(sample->input)) {
					auto finished = runner.simulation->finish_deferred_frame(
						*action, &runner.telemetry
					);
					if(!finished) {
						return std::unexpected(finished.error());
					}
					step = *finished;
				}
			}
			runner.telemetry.count_step(
				step.next_step_vsyncs,
				sample->phase == InputPhase::gameplay
			);
			runner.result.consumed_samples++;
			auto handled = runner.handle_step(step);
			if(!handled) {
				return std::unexpected(handled.error());
			}
			continue;
		}

		const auto &boundary = std::get<ProcessBoundary>(record);
		if(
			boundary.sample_index != runner.result.consumed_samples ||
			boundary.completed_process != runner.process
		) {
			runner.result.status = RunStatus::process_boundary_mismatch;
			return runner.result;
		}
		if(runner.process == ProcessId::main) {
			if(runner.pause.active()) {
				auto finished = runner.simulation->finish_deferred_frame(
					DeferredFrameAction::exit_session
				);
				if(!finished) {
					return std::unexpected(finished.error());
				}
				runner.pause.reset();
				auto handled = runner.handle_step(*finished);
				if(!handled) {
					return std::unexpected(handled.error());
				}
			}
			if(!runner.main_terminal) {
				auto finished = runner.finish_main_if_needed();
				if(!finished) {
					return std::unexpected(finished.error());
				}
			}
			runner.process = ProcessId::mainl;
			continue;
		}

		if(!runner.main_terminal) {
			runner.result.status = RunStatus::process_boundary_mismatch;
			return runner.result;
		}
		if(!runner.next_story_stage) {
			auto transition = runner.simulation->session_transition();
			if(
				!transition ||
				transition->action == SessionAction::start_gameplay
			) {
				runner.result.status = RunStatus::process_boundary_mismatch;
				return runner.result;
			}
			continue;
		}

		runner.next_story_stage->global_frame = runner.result.consumed_samples;
		auto next = Game::create(
			*runner.next_story_stage, runner.content, options
		);
		if(!next) {
			return std::unexpected(next.error());
		}
		runner.simulation = std::move(*next);
		runner.result.round_starts.push_back(RoundStartRecord{
			.sample_index = runner.result.consumed_samples,
			.observation = runner.simulation->round_start_observation(),
		});
		runner.process = ProcessId::main;
		runner.main_terminal = false;
		runner.telemetry.reset_round();
		runner.next_story_stage.reset();
		runner.result.terminal_state.reset();
		runner.result.terminal_transition.reset();
	}

	if(
		!runner.result.terminal_state ||
		runner.result.consumed_samples != session.sample_count
	) {
		runner.result.status = RunStatus::input_exhausted;
		return runner.result;
	}
	runner.result.status = RunStatus::complete;
	return runner.result;
}

std::optional<RoundStartMismatch> first_round_start_mismatch(
	std::span<const RoundStartRecord> expected,
	std::span<const RoundStartRecord> actual
)
{
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto found = std::find_if(
			actual.begin(), actual.end(), [&](const auto &candidate) {
				return candidate.sample_index == expected[index].sample_index;
			}
		);
		if(found == actual.end()) {
			return RoundStartMismatch{
				.checkpoint_index = index,
				.sample_index = expected[index].sample_index,
				.actual_sample_index = std::nullopt,
				.field = "checkpoint.missing",
			};
		}
		if(const auto field = observation_mismatch(
			expected[index].observation, found->observation
		)) {
			return RoundStartMismatch{
				.checkpoint_index = index,
				.sample_index = expected[index].sample_index,
				.actual_sample_index = found->sample_index,
				.field = *field,
			};
		}
	}
	return std::nullopt;
}

} // namespace th03::replay
