#include "th03/core/game.hpp"

#include "game_internal.hpp"

#include <memory>
#include <utility>

namespace th03 {

Game::Impl::Impl(const GameInit &init) : state(detail::initialize_first_round(init)) {}

Game::Game(GameInit init) : impl_(std::make_unique<Impl>(init)) {}

Game::Game(Game &&) noexcept = default;
Game &Game::operator=(Game &&) noexcept = default;
Game::~Game() = default;

std::expected<Game, Error> Game::create(const GameInit &init)
{
	if((init.autofire_mask & 0xFCU) != 0U) {
		return std::unexpected(Error{
			ErrorCode::invalid_value,
			"autofire mask contains bits outside P1/P2",
		});
	}
	return Game(init);
}

StepResult Game::step(CoreInput input)
{
	(void)input;
	return StepResult{StepStatus::simulation_not_implemented};
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
