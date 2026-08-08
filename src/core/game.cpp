#include "th03/core/game.hpp"

#include <utility>

namespace th03 {

Game::Game(GameInit init) : init_(std::move(init)) {}

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
	return canonical_;
}

std::span<const PresentationEvent> Game::presentation_events() const
{
	return presentation_events_;
}

} // namespace th03
