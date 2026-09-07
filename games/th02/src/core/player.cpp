#include "th02/core/player.hpp"

#include <bit>

namespace th02::core {
namespace {
std::int16_t add_word(std::int16_t value, std::int16_t delta) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(value) + delta));
}
}

bool player_move(PlayerPositionState &state, std::size_t back_page,
	std::int16_t delta_x, std::int16_t delta_y) noexcept
{
	if(back_page >= state.on_page.size()) return false;
	auto &page = state.on_page[back_page];
	state.current.y = add_word(page.y, delta_y);
	if(state.current.y < 16) {
		state.current.y = 16;
	} else if(state.current.y > 344) {
		state.current.y = 344;
	}
	page.y = state.current.y;

	state.current.x = add_word(page.x, delta_x);
	if(state.current.x >= 390) {
		state.current.x = 390;
	} else if(state.current.x <= 28) {
		state.current.x = 28;
	}
	page.x = state.current.x;
	return true;
}

} // namespace th02::core
