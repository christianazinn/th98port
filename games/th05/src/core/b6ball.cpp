#include "th05/core/b6ball.hpp"

#include "th05/core/bullet.hpp"
#include "th05/core/state.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_motion.hpp"

#include <bit>
#include <cstdint>

namespace th05::core {
namespace {

constexpr std::int16_t subpixel_factor = 16;
constexpr std::int16_t b6ball_half_extent = 16 * subpixel_factor;
constexpr std::int16_t playfield_width = 384 * subpixel_factor;
constexpr std::int16_t playfield_height = 368 * subpixel_factor;
constexpr std::int16_t cloud_radius_delta = 2 * subpixel_factor;
constexpr std::int16_t cloud_radius_solid = 16 * subpixel_factor;
constexpr std::int16_t pat_decay_b6ball = 204;
constexpr std::int16_t pat_decay_b6ball_end = pat_decay_b6ball + 4;

[[nodiscard]] constexpr std::int16_t word(std::uint16_t bits) noexcept
{
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] constexpr std::int16_t word_add(
	std::int16_t left, std::int16_t right
) noexcept
{
	return word(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) + static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::int16_t word_sub(
	std::int16_t left, std::int16_t right
) noexcept
{
	return word(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) - static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr bool overlap(std::int16_t delta, std::int16_t extent) noexcept
{
	return static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(delta) + static_cast<std::uint16_t>(extent / 2)
	) <= static_cast<std::uint16_t>(extent);
}

[[nodiscard]] constexpr bool encloses_32px(pc98::th0405::SubpixelPoint point) noexcept
{
	return point.x > -b6ball_half_extent && point.x < playfield_width + b6ball_half_extent &&
		point.y > -b6ball_half_extent && point.y < playfield_height + b6ball_half_extent;
}

} // namespace

void b6balls_add(State &state) noexcept
{
	auto &b6balls = state.b6balls;
	const auto speed = bullet_speed_tune_for_playperf(
		b6balls.b6ball_template.speed, state.resources.playperf
	);
	for(auto &ball : b6balls.pool) {
		if(ball.flag != B6BallFlag::free) continue;
		ball.flag = B6BallFlag::cloud;
		ball.pos.cur = b6balls.b6ball_template.origin;
		const auto velocity = pc98::masterlib::vector2(
			b6balls.b6ball_template.angle, static_cast<std::int16_t>(speed)
		);
		ball.pos.velocity = {velocity.x, velocity.y};
		ball.patnum_tiny = b6balls.b6ball_template.patnum_tiny;
		ball.cloud_radius = 48 * subpixel_factor;
		return;
	}
}

void b6balls_update(State &state) noexcept
{
	for(auto &ball : state.b6balls.pool) {
		if(ball.flag == B6BallFlag::free) continue;
		if(ball.flag == B6BallFlag::cloud) {
			ball.cloud_radius = word_sub(ball.cloud_radius, cloud_radius_delta);
			if(ball.cloud_radius < cloud_radius_solid) {
				ball.flag = static_cast<B6BallFlag>(
					static_cast<std::uint8_t>(ball.flag) + 1U
				);
			} else {
				continue;
			}
		}

		if(state.resources.bullet_clear_time != 0U || state.bullets.zap.active()) {
			ball.flag = B6BallFlag::decay;
		}

		++ball.age;
		const auto position = pc98::th0405::update_motion(ball.pos);
		if(!encloses_32px(position)) {
			ball.flag = B6BallFlag::free;
		} else if(ball.flag != B6BallFlag::decay) {
			if(overlap(word_sub(position.x, state.player_pos.cur.x), b6ball_half_extent) &&
				overlap(word_sub(position.y, state.player_pos.cur.y), b6ball_half_extent)) {
				state.player_is_hit = true;
				ball.flag = B6BallFlag::decay;
				continue;
			}
		} else if(ball.patnum_tiny < pat_decay_b6ball) {
			ball.patnum_tiny = pat_decay_b6ball;
			ball.pos.velocity.x = static_cast<std::int16_t>(ball.pos.velocity.x / 2);
			ball.pos.velocity.y = static_cast<std::int16_t>(ball.pos.velocity.y / 2);
			ball.decay_frame = 0;
		} else {
			ball.decay_frame = word_add(ball.decay_frame, 1);
			if((ball.decay_frame % 4) == 0) {
				ball.patnum_tiny = word_add(ball.patnum_tiny, 1);
				if(ball.patnum_tiny >= pat_decay_b6ball_end) {
					ball.flag = B6BallFlag::free;
				}
			}
		}
	}
}

void b6balls_render_tail(State &) noexcept
{
	// th05/main/bullet/b6_custombullets_render.asm draws clouds, balls, and
	// Cheetos but does not alter native gameplay state.
}

} // namespace th05::core
