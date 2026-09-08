#include "th02/core/player.hpp"

#include <bit>

namespace th02::core {
namespace {
std::int16_t add_word(std::int16_t value, std::int16_t delta) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(value) + delta));
}

void negate_hits_if_invincible(PlayerHitState &state) noexcept
{
	if(state.invincible_via_bomb ||
		((state.invincibility_time != 0) && !state.miss_active)) {
		state.raw_hit = 0;
	}
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

std::optional<MovementIntent> movement_intent(
	std::uint16_t input, MovementSpeeds speeds, std::size_t back_page) noexcept
{
	if(back_page > 1) return std::nullopt;
	MovementIntent result{};
	// Signed byte speeds promote to signed words before negation in the DOS
	// source. The wider intermediates retain -(-128) == 128.
	std::int16_t dx = 0, dy = 0;
	auto &pose = result.pose;
	switch(input & 0xF000U) {
	case 0x1000: dy = -speeds.diagonal_y; dx = -speeds.diagonal_x; pose = PlayerPose::left; break;
	case 0x2000: dy = -speeds.diagonal_y; dx = speeds.diagonal_x; pose = PlayerPose::right; break;
	case 0x4000: dy = speeds.diagonal_y; dx = -speeds.diagonal_x; pose = PlayerPose::left; break;
	case 0x8000: dy = speeds.diagonal_y; dx = speeds.diagonal_x; pose = PlayerPose::right; break;
	default:
		switch(input & 0x000FU) {
		case 1: dy = -speeds.aligned_y; dx = 0; pose = PlayerPose::still; break;
		case 2: dy = speeds.aligned_y; dx = 0; pose = PlayerPose::still; break;
		case 4: dx = -speeds.aligned_x; dy = 0; pose = PlayerPose::left; break;
		case 8: dx = speeds.aligned_x; dy = 0; pose = PlayerPose::right; break;
		case 5: dy = -speeds.diagonal_y; dx = -speeds.diagonal_x; pose = PlayerPose::left; break;
		case 9: dy = -speeds.diagonal_y; dx = speeds.diagonal_x; pose = PlayerPose::right; break;
		case 6: dy = speeds.diagonal_y; dx = -speeds.diagonal_x; pose = PlayerPose::left; break;
		case 10: dy = speeds.diagonal_y; dx = speeds.diagonal_x; pose = PlayerPose::right; break;
		default: dx = 0; dy = 0; pose = PlayerPose::still; break;
		}
	}
	if(speeds.aligned_x == 5) {
		const auto page = static_cast<std::int16_t>(back_page);
		if(dx < 0) dx += page;
		else if(dx > 0) dx -= page;
		if(dy < 0) dy += page;
		else if(dy > 0) dy -= page;
	}
	result.delta = {dx, dy};
	return result;
}

bool player_move_and_shoot_allowed(PlayerHitState &state) noexcept
{
	negate_hits_if_invincible(state);
	return state.raw_hit == 0;
}

void prepare_player_callbacks(PlayerHitState &state) noexcept
{
	negate_hits_if_invincible(state);
	if(state.invincibility_time > 0) {
		--state.invincibility_time;
	}
}

} // namespace th02::core
