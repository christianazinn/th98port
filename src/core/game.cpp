#include "th03/core/game.hpp"

#include "game_internal.hpp"

#include <memory>
#include <utility>

namespace th03 {

Game::Impl::Impl(const GameInit &init, GameContent content_) :
	state(detail::initialize_first_round(
		init, content_.enemy_formations.size()
	)),
	initial_init(init),
	autofire_mask(init.autofire_mask),
	content(std::move(content_))
{}

Game::Game(GameInit init, GameContent content) :
	impl_(std::make_unique<Impl>(init, std::move(content)))
{}

Game::Game(Game &&) noexcept = default;
Game &Game::operator=(Game &&) noexcept = default;
Game::~Game() = default;

std::expected<Game, Error> Game::create(const GameInit &init)

{
	return create(init, GameContent{});
}

std::expected<Game, Error> Game::create(const GameInit &init, GameContent content)
{
	if((init.autofire_mask & 0xFCU) != 0U) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"autofire mask contains bits outside P1/P2",
		});
	}
	if(content.enemy_formations.size() > 24U) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"game content contains more than 24 enemy formations",
		});
	}
	return Game(init, std::move(content));
}

StepResult Game::step(CoreInput input, InputContext context)
{
	if(impl_->state.route != 0U) {
		return StepResult{
			StepStatus::terminal,
			detail::canonical_state(impl_->state),
		};
	}
	if(context == InputContext::interstitial) {
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
			if(impl_->state.route != 0U) {
				return StepResult{StepStatus::terminal, input_boundary};
			}
			return StepResult{StepStatus::phase_transition, input_boundary};
		}
		return StepResult{StepStatus::running, input_boundary};
	}
	if(impl_->warning_interstitial_samples_remaining != 0U) {
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
	if(detail::gameplay_frame_suffix_before_interstitial(impl_->state)) {
		impl_->warning_interstitial_samples_remaining = 27U;
		return StepResult{StepStatus::phase_transition, input_boundary};
	}
	detail::gameplay_frame_suffix_after_interstitial(impl_->state);
	if(impl_->state.route != 0U) {
		return StepResult{StepStatus::terminal, input_boundary};
	}
	return StepResult{StepStatus::running, input_boundary};
}

CanonicalState Game::canonical_state() const
{
	return detail::canonical_state(impl_->state);
}

std::expected<GameInit, Error> Game::next_story_stage_init() const
{
	const auto &state = impl_->state;
	if(
		state.game_mode != 1U || state.route != 2U || state.winner != 0 ||
		state.story_stage >= 8U
	) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"game is not at a continuing Story-stage boundary",
		});
	}

	auto next = impl_->initial_init;
	next.global_frame = state.global_frame;
	next.resident_rand = static_cast<std::int32_t>(state.round_frame);
	next.random_seed = state.round_frame;
	next.story_stage = static_cast<std::uint8_t>(state.story_stage + 1U);
	next.story_lives = state.story_lives;
	next.skill = state.skill;
	next.winner = state.winner;
	next.last_score[0] = state.score[0];
	next.last_score[1] = {};
	next.paletted_character[1] = next.story_opponents[next.story_stage];
	const auto opponent = static_cast<std::uint8_t>(
		(static_cast<int>(next.paletted_character[1]) - 1) / 2
	);
	if(next.story_stage < 7U && opponent >= 7U) {
		next.paletted_character[1] = 1;
	}
	next.runtime_snapshot.reset();
	return next;
}

std::span<const PresentationEvent> Game::presentation_events() const
{
	return impl_->presentation_events;
}

} // namespace th03
