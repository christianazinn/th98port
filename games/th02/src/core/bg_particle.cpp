#include "th02/core/bg_particle.hpp"

#include "pc98/masterlib_vector.hpp"

#include <bit>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_right = 416;
constexpr std::int16_t playfield_top = 16;
constexpr std::int16_t playfield_bottom = 384;
constexpr std::int16_t bg_particle_width = 8;
constexpr std::int16_t bg_particle_height = 8;
constexpr unsigned int subpixel_bits = 4U;

[[nodiscard]] std::int16_t word_from_i32(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] std::int16_t word_add(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	return word_from_i32(static_cast<std::int32_t>(lhs) + rhs);
}

[[nodiscard]] std::int16_t word_shift_left(std::int16_t value,
	unsigned int count) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(value)) << count
	));
}

[[nodiscard]] std::int16_t word_multiply(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	return word_from_i32(static_cast<std::int32_t>(lhs) * rhs);
}

[[nodiscard]] std::int16_t arithmetic_shift_right(std::int16_t value,
	unsigned int count) noexcept
{
	const auto divisor = static_cast<std::int32_t>(1) << count;
	if(value >= 0) return static_cast<std::int16_t>(value / divisor);
	return static_cast<std::int16_t>(
		-(((-static_cast<std::int32_t>(value)) + divisor - 1) / divisor)
	);
}

[[nodiscard]] bool page_is_valid(std::size_t page) noexcept
{
	return page < BgParticle{}.screen_topleft.size();
}

} // namespace

void bg_particles_reset(BgParticleState &state) noexcept
{
	for(auto &particle : state.particles) {
		particle.flag = bg_particle_free;
	}
	state.speed_initial = 32;
	state.speed_delta = 1;
	state.angle_delta = 0;
	state.color = 15U;
	state.unput_color = 0U;
	state.unused = -1;
	state.edge_step = 32;
}

bool bg_particles_add(BgParticleState &state, std::int16_t left,
	std::int16_t top, std::uint8_t angle) noexcept
{
	for(auto &particle : state.particles) {
		if(particle.flag != bg_particle_free) continue;
		particle.flag = bg_particle_alive;
		particle.angle = angle;
		particle.screen_topleft[0U].x = word_shift_left(left, subpixel_bits);
		particle.screen_topleft[1U].x = word_shift_left(left, subpixel_bits);
		particle.screen_topleft[0U].y = word_shift_left(top, subpixel_bits);
		particle.screen_topleft[1U].y = word_shift_left(top, subpixel_bits);
		particle.edge = 1U;
		particle.speed = static_cast<std::uint8_t>(state.speed_initial);
		return true;
	}
	return false;
}

BgParticlePageResult bg_particles_invalidate(BgParticleState &state,
	std::size_t page_back, std::size_t page_front, bool reduce_effects,
	BgParticleCallbacks &callbacks
) noexcept
{
	if(!page_is_valid(page_back) || !page_is_valid(page_front)) {
		return BgParticlePageResult::invalid_page;
	}

	callbacks.grcg_set_color(state.unput_color);
	for(std::size_t i = 0U; i < state.particles.size(); ++i) {
		auto &particle = state.particles[i];
		if(particle.flag == bg_particle_free) continue;

		const auto position = particle.screen_topleft[page_back];
		const auto topleft = ScreenPoint{
			arithmetic_shift_right(position.x, subpixel_bits),
			arithmetic_shift_right(position.y, subpixel_bits),
		};
		if(!reduce_effects || (page_back == (i & 1U))) {
			callbacks.dot_square_unput(topleft, particle.edge);
		}
		if(particle.flag == bg_particle_remove) {
			particle.flag = bg_particle_free;
			continue;
		}
		particle.screen_topleft[page_front ^ 1U] =
			particle.screen_topleft[page_front];
	}
	callbacks.grcg_off();
	return BgParticlePageResult::completed;
}

BgParticlePageResult bg_particles_update_and_render(BgParticleState &state,
	std::size_t page_back, bool reduce_effects, BgParticleCallbacks &callbacks
) noexcept
{
	if(!page_is_valid(page_back)) return BgParticlePageResult::invalid_page;

	callbacks.grcg_set_color(state.color);
	state.unused_2 = static_cast<std::uint8_t>(state.unused);
	state.unused_3 = state.color;
	for(std::size_t i = 0U; i < state.particles.size(); ++i) {
		auto &particle = state.particles[i];
		if(particle.flag != bg_particle_alive) continue;

		particle.speed = static_cast<std::uint8_t>(
			static_cast<std::int32_t>(particle.speed) + state.speed_delta
		);
		particle.angle = static_cast<std::uint8_t>(
			static_cast<std::int32_t>(particle.angle) + state.angle_delta
		);
		const auto velocity = pc98::masterlib::vector2(particle.angle,
			static_cast<std::int16_t>(particle.speed));
		auto &position = particle.screen_topleft[page_back];
		position.x = word_add(position.x, velocity.x);
		position.y = word_add(position.y, velocity.y);
		const auto left = arithmetic_shift_right(position.x, subpixel_bits);
		const auto top = arithmetic_shift_right(position.y, subpixel_bits);
		if((left >= playfield_right) ||
			(left <= static_cast<std::int16_t>(playfield_left - bg_particle_width)) ||
			(top <= static_cast<std::int16_t>(playfield_top - bg_particle_height)) ||
			(top >= playfield_bottom)) {
			particle.flag = bg_particle_remove;
			continue;
		}

		auto threshold = word_multiply(state.edge_step,
			static_cast<std::int16_t>(bg_particle_edge_max));
		auto edge = bg_particle_edge_max;
		while(threshold > state.edge_step) {
			if(static_cast<std::int16_t>(particle.speed) > threshold) break;
			threshold = word_add(threshold,
				static_cast<std::int16_t>(-state.edge_step));
			edge = static_cast<std::uint8_t>(edge - 1U);
		}
		particle.edge = edge;
		if(!reduce_effects || (page_back == (i & 1U))) {
			callbacks.dot_square({left, top}, particle.edge);
		}
	}
	callbacks.grcg_off();
	return BgParticlePageResult::completed;
}

} // namespace th02::core
