#include "th02/core/bomb.hpp"

#include "pc98/masterlib_vector.hpp"

#include <bit>
#include <cstdint>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_top = 16;
constexpr std::int16_t playfield_width = 384;
constexpr std::int16_t playfield_height = 368;
constexpr std::int16_t bomb_circle_radius_start = 256;
constexpr std::uint8_t bomb_circle_points = 64;
constexpr std::uint8_t bomb_circle_angle_step = 4;
constexpr std::int16_t bomb_tile_size = 8;
constexpr std::int16_t bomb_playfield_bottom =
	playfield_top + playfield_height;

[[nodiscard]] std::int16_t add_word(std::int16_t value,
	std::int16_t delta) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(value) + delta));
}

[[nodiscard]] std::int8_t decrement_byte(std::int8_t value) noexcept
{
	return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
		static_cast<std::int32_t>(value) - 1));
}

[[nodiscard]] std::uint8_t increment_byte(std::uint8_t value) noexcept
{
	return static_cast<std::uint8_t>(value + 1U);
}

[[nodiscard]] ScreenPoint bomb_circle_point(ScreenPoint center,
	std::int16_t radius, std::uint8_t angle) noexcept
{
	const auto offset = pc98::masterlib::vector2(angle, radius);
	return {add_word(center.x, offset.x), add_word(center.y, offset.y)};
}

} // namespace

void bomb_reset(BombState &state) noexcept
{
	state.stage_bombs_used = 0U;
	state.bombing = false;
}

PlayerBombResult player_bomb(
	BombState &bomb, ScoreState &score, PlayerHitState &player,
	EnemyBulletPoolState &bullets, std::size_t page_back,
	std::size_t page_front, BombPresentationCallbacks &callbacks
) noexcept
{
	if(bomb.bombing) {
		return PlayerBombResult::already_bombing;
	}
	if(score.bombs == 0) {
		return PlayerBombResult::no_bombs;
	}

	bomb.bombing = true;
	player.invincible_via_bomb = true;
	score.bombs = decrement_byte(score.bombs);
	callbacks.hud_bombs(score.bombs);
	bomb.bomb_frame = 0;
	bomb.stage_bombs_used = increment_byte(bomb.stage_bombs_used);
	bomb.total_bombs_used = increment_byte(bomb.total_bombs_used);
	callbacks.sound_effect(9U);
	bomb.bomb_circle_center = {
		static_cast<std::int16_t>(playfield_left + (playfield_width / 2) - 4),
		static_cast<std::int16_t>(playfield_top + (playfield_height / 2) - 4),
	};
	bomb.bomb_circle_frame = 0;
	bomb.bomb_circle_done = 0;

	if(bullets_clear(bullets, page_back, page_front, callbacks) !=
		EnemyBulletLifecycleResult::completed) {
		return PlayerBombResult::invalid_page;
	}
	return PlayerBombResult::started;
}

void bomb_invalidate(BombState &state, std::uint8_t shottype,
	BombPresentationCallbacks &callbacks) noexcept
{
	if(!state.bombing) {
		return;
	}

	if(state.bomb_circle_frame > 1) {
		state.bomb_frame = add_word(state.bomb_frame, -1);
		const auto radius = static_cast<std::int32_t>(
			bomb_circle_radius_start) -
			(static_cast<std::int32_t>(state.bomb_frame) * 8);
		const auto radius_word = std::bit_cast<std::int16_t>(
			static_cast<std::uint16_t>(radius));
		std::uint8_t angle = static_cast<std::uint8_t>(state.bomb_frame);
		for(std::uint8_t i = 0; i < bomb_circle_points; ++i) {
			const auto point = bomb_circle_point(
				state.bomb_circle_center, radius_word, angle);
			if(point.y > 8 && point.y < bomb_playfield_bottom) {
				callbacks.invalidate_bomb({
					point, bomb_tile_size, bomb_tile_size,
				});
			}
			angle = static_cast<std::uint8_t>(angle + bomb_circle_angle_step);
		}
		if(state.bomb_frame >= bomb_circle_frames) {
			state.bomb_circle_frame = 0;
		}
		state.bomb_frame = add_word(state.bomb_frame, 1);
	}

	bool render_all = (state.bomb_circle_done == 1) &&
		(state.bomb_frame <= bomb_circle_frames);
	if(shottype == 0U) {
		render_all = render_all || state.bomb_frame == 136 ||
			state.bomb_frame == 137;
	} else if(shottype == 1U) {
		render_all = render_all || state.bomb_frame == 164 ||
			state.bomb_frame == 165;
	} else {
		render_all = render_all || state.bomb_frame == 112 ||
			state.bomb_frame == 113;
	}
	if(render_all) {
		callbacks.render_all();
	}
}

} // namespace th02::core
