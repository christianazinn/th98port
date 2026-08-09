#include "th03/replay/run.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace th03::replay {
namespace {

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
	RunResult result{};
	std::optional<Game> simulation;
	ProcessId process = ProcessId::main;
	bool main_terminal = false;
	std::optional<GameInit> next_story_stage;
	PauseCoordinator pause;
	const GameContent &content;

	std::expected<void, Error> handle_step(const StepResult &step)
	{
		if(step.status == StepStatus::round_transition) {
			result.completed_rounds.push_back(CompletedRound{
				result.consumed_samples, simulation->canonical_state(),
			});
			auto next_round = simulation->start_next_round();
			if(!next_round) {
				return std::unexpected(next_round.error());
			}
			return {};
		}
		if(step.status != StepStatus::terminal) {
			return {};
		}

		result.completed_rounds.push_back(CompletedRound{
			result.consumed_samples, simulation->canonical_state(),
		});
		auto finished = simulation->finish_main_process();
		if(!finished) {
			return std::unexpected(finished.error());
		}
		result.terminal_state = simulation->canonical_state();
		auto transition = simulation->session_transition();
		if(!transition) {
			return std::unexpected(transition.error());
		}
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
		result.completed_rounds.push_back(CompletedRound{
			result.consumed_samples, simulation->canonical_state(),
		});
		auto finished = simulation->finish_main_process();
		if(!finished) {
			return std::unexpected(finished.error());
		}
		result.terminal_state = simulation->canonical_state();
		auto transition = simulation->session_transition();
		if(!transition) {
			return std::unexpected(transition.error());
		}
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
		.content = content,
	};

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
			auto step = runner.simulation->step(sample->input, context);
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
					auto finished = runner.simulation->finish_deferred_frame(*action);
					if(!finished) {
						return std::unexpected(finished.error());
					}
					step = *finished;
				}
			}
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
		runner.process = ProcessId::main;
		runner.main_terminal = false;
		runner.next_story_stage.reset();
		runner.result.terminal_state.reset();
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

} // namespace th03::replay
