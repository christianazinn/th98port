#include "th03/core/game.hpp"

#include "game_internal.hpp"

#include <memory>
#include <utility>

namespace th03 {

Game::Impl::Impl(const GameInit &init, GameContent content_) :
	state(detail::initialize_first_round(init)),
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
	return StepResult{StepStatus::running, input_boundary};
}

CanonicalState Game::canonical_state() const
{
	return detail::canonical_state(impl_->state);
}

std::span<const PresentationEvent> Game::presentation_events() const
{
	return impl_->presentation_events;
}

} // namespace th03
