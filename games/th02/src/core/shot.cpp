#include "th02/core/shot.hpp"

#include "pc98/masterlib_vector.hpp"

#include <bit>

namespace th02::core {
namespace {

constexpr unsigned subpixel_bits = 4U;

[[nodiscard]] std::int16_t add_word(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) <<
		subpixel_bits
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] bool slot_is_valid(const PlayerShotPoolState &state,
	std::size_t back_page) noexcept
{
	return (state.current_slot >= 0) &&
		(static_cast<std::size_t>(state.current_slot) < state.shots.size()) &&
		(back_page < state.shots.front().pos_on_page.size());
}

[[nodiscard]] PlayerShot &current_shot(PlayerShotPoolState &state) noexcept
{
	return state.shots[static_cast<std::size_t>(state.current_slot)];
}

} // namespace

bool shot_add(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	std::int16_t left_offset, std::uint8_t angle) noexcept
{
	if(!slot_is_valid(state, back_page)) return false;

	auto &shot = current_shot(state);
	shot.flag = player_shot_alive;
	shot.decay_cel = 0;
	shot.pos_on_page[back_page].x = to_subpixels(
		add_word(positions.player_topleft.x, left_offset)
	);
	shot.pos_on_page[back_page].y = state.spawn_top;
	// (Cos/Sin * 12) SAR 4 equals the exact shared Q8 vector at Q12.4
	// length 192. Calling it directly avoids a lossy intermediate vector.
	const auto velocity = pc98::masterlib::vector2(angle, 192);
	shot.velocity = {velocity.x, velocity.y};
	shot.patnum = state.patnum;
	shot.from_option = false;
	return true;
}

bool shot_option_add(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	std::int16_t left_offset, ShotSubpixelPoint velocity) noexcept
{
	if(!slot_is_valid(state, back_page)) return false;

	auto &shot = current_shot(state);
	shot.flag = player_shot_alive;
	shot.decay_cel = 1;
	shot.pos_on_page[back_page].x = to_subpixels(
		add_word(positions.option_left_topleft_on_back_page.x, left_offset)
	);
	shot.pos_on_page[back_page].y = to_subpixels(
		positions.option_left_topleft_on_back_page.y
	);
	shot.velocity = velocity;
	shot.patnum = state.option_patnum;
	shot.from_option = true;
	return true;
}

void shots_free_all(PlayerShotPoolState &state) noexcept
{
	for(auto &shot : state.shots) {
		shot.flag = player_shot_free;
	}
}

} // namespace th02::core
