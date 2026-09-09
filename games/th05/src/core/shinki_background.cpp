#include "th05/core/shinki_background.hpp"

#include "th05/core/state.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_random.hpp"

#include <bit>
#include <cstdint>

namespace th05::core {
namespace {

constexpr std::int16_t particle_none = -999 * 16;
constexpr std::int16_t subpixel_factor = 16;
constexpr std::int16_t playfield_center_x = (384 * subpixel_factor) / 2;
constexpr std::int16_t playfield_center_y = (368 * subpixel_factor) / 2;
constexpr std::int16_t spinline_top = 80 * subpixel_factor;
constexpr std::int16_t spinline_bottom = (368 - 64) * subpixel_factor;
constexpr std::int16_t particle_pat_first = 172;

[[nodiscard]] constexpr std::int16_t word(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr std::int16_t word_add(
	std::int16_t left,
	std::int16_t right
) noexcept
{
	return word(static_cast<std::int32_t>(left) + right);
}

[[nodiscard]] constexpr std::int16_t word_sub(
	std::int16_t left,
	std::int16_t right
) noexcept
{
	return word(static_cast<std::int32_t>(left) - right);
}

[[nodiscard]] constexpr std::int16_t sar4(std::int16_t value) noexcept
{
	std::uint16_t shifted = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(value) >> 4U
	);
	if(value < 0) {
		shifted = static_cast<std::uint16_t>(
			static_cast<unsigned int>(shifted) | 0xF000U
		);
	}
	return std::bit_cast<std::int16_t>(shifted);
}

[[nodiscard]] constexpr std::uint8_t byte_add(
	std::uint8_t left,
	std::int16_t right
) noexcept
{
	return static_cast<std::uint8_t>(static_cast<std::int16_t>(left) + right);
}

[[nodiscard]] constexpr bool particle_visible(
	pc98::th0405::SubpixelPoint position
) noexcept
{
	const auto left = static_cast<std::int32_t>(32 + sar4(position.x) - 8);
	const auto top = static_cast<std::int32_t>(16 + sar4(position.y) - 8);
	return (left > (32 - 16)) && (left < (32 + 384)) &&
		(top > 0) && (top < (16 + 368));
}

void particles_update(ShinkiBackgroundState &background) noexcept
{
	for(auto &particle : background.particles) {
		if(particle.pos.x == particle_none) continue;
		particle.pos.x = word_add(particle.pos.x, particle.velocity.x);
		particle.pos.y = word_add(particle.pos.y, particle.velocity.y);
		if(!particle_visible(particle.pos)) {
			particle.pos = particle.origin;
			particle.age = 0;
		} else {
			particle.age = word_add(particle.age, 1);
		}
	}
}

void lineset_forward_copy(ShinkiBackgroundLineSet &set) noexcept
{
	for(std::size_t line = 18U; line > 0U; --line) {
		set.center[line] = set.center[line - 1U];
		set.radius[line] = set.radius[line - 1U];
		set.angle[line] = set.angle[line - 1U];
	}
}

void spinline_x_and_angle(
	ShinkiBackgroundLineSet &set,
	std::int16_t delta,
	std::uint16_t frame
) noexcept
{
	if(set.radius[0] > (96 * subpixel_factor)) {
		set.radius[0] = word_sub(set.radius[0], 2);
	}
	if((frame % 512U) < 256U) {
		set.center[0].x = word_add(set.center[0].x, delta);
	} else {
		set.center[0].x = word_sub(set.center[0].x, delta);
	}
	set.angle[0] = byte_add(set.angle[0], static_cast<std::int16_t>(delta / 2));
}

void spinline_formation_top(ShinkiBackgroundState &background) noexcept
{
	const auto delta = word_sub(spinline_top, background.linesets[0].center[0].y);
	for(std::size_t set = 0U; set < 2U; ++set) {
		for(std::size_t line = 19U; line-- > 0U;) {
			background.linesets[set].center[line].y = word_add(
				background.linesets[set].center[line].y, delta
			);
		}
	}
}

void spinline_formation_bottom(ShinkiBackgroundState &background) noexcept
{
	const auto delta = word_sub(background.linesets[0].center[0].y, spinline_bottom);
	for(std::size_t set = 0U; set < 2U; ++set) {
		for(std::size_t line = 19U; line-- > 0U;) {
			background.linesets[set].center[line].y = word_sub(
				background.linesets[set].center[line].y, delta
			);
		}
	}
}

[[nodiscard]] bool type_a_update_part1(State &state) noexcept
{
	auto &background = state.shinki_background;
	if(background.type_a_particles_alive == 0x00FF) {
		std::uint8_t angle = 0x40U;
		for(std::size_t set = 0U; set < 2U; ++set) {
			for(std::size_t line = 0U; line < shinki_background_lines_per_set; ++line) {
				background.linesets[set].center[line] = {
					playfield_center_x, playfield_center_y
				};
				background.linesets[set].radius[line] = subpixel_factor;
				background.linesets[set].angle[line] = angle;
			}
			angle = byte_add(angle, -0x80);
		}
		for(auto &particle : background.particles) {
			particle.pos.x = particle_none;
			particle.patnum = 0U;
		}
		background.type_a_particles_alive = 0;
	}
	if((background.type_a_particles_alive < 64) &&
		((state.boss.phase_frame % 4) == 0)) {
		auto &particle = background.particles[
			static_cast<std::size_t>(background.type_a_particles_alive)
		];
		particle.pos = {playfield_center_x, playfield_center_y};
		particle.origin = particle.pos;
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return false;
		particle.angle = static_cast<std::uint8_t>(*angle);
		particle.age = 0;
		const auto speed = pc98::th0405::randring_next16_and(state.random, 63U);
		if(!speed) return false;
		const auto velocity = pc98::masterlib::vector2(
			particle.angle,
			word(32 + static_cast<std::int32_t>(*speed))
		);
		particle.velocity = {velocity.x, velocity.y};
		background.type_a_particles_alive = word_add(
			background.type_a_particles_alive, 1
		);
	}
	if(state.boss.phase == 3U) {
		const auto delta = static_cast<std::int16_t>(
			((background.linesets_zoomed_out % 4U) < 2U) ? 2 : -2
		);
		background.linesets[0].angle[0] = byte_add(background.linesets[0].angle[0], delta);
		background.linesets[1].angle[0] = byte_add(background.linesets[1].angle[0], delta);
	}
	return true;
}

[[nodiscard]] bool type_a_update_and_render(State &state) noexcept
{
	if(!type_a_update_part1(state)) return false;
	auto &background = state.shinki_background;
	for(std::size_t set = 0U; set < 2U; ++set) {
		auto &lineset = background.linesets[set];
		lineset_forward_copy(lineset);
		lineset.radius[0] = word_add(lineset.radius[0], 64);
		const auto length = word(
			(static_cast<std::int32_t>(lineset.radius[0]) * 3) / 4
		);
		const auto offset = pc98::masterlib::vector2(
			byte_add(lineset.angle[0], -0x40), length
		);
		lineset.center[0] = {
			word_add(playfield_center_x, offset.x),
			word_add(playfield_center_y, offset.y),
		};
		if(lineset.radius[0] >= (224 * subpixel_factor)) {
			background.linesets_zoomed_out = static_cast<std::uint8_t>(
				background.linesets_zoomed_out + 1U
			);
			lineset.radius[0] = 0;
		}
	}
	particles_update(background);
	return true;
}

[[nodiscard]] bool type_b_update_part1(State &state) noexcept
{
	auto &background = state.shinki_background;
	if(!background.type_b_initialized) {
		std::int16_t center_x = 64 * subpixel_factor;
		for(std::size_t set = 0U; set < 2U; ++set) {
			for(std::size_t line = 19U; line-- > 0U;) {
				background.linesets[set].center[line] = {
					center_x, (184 + 32) * subpixel_factor
				};
				background.linesets[set].radius[line] = 256 * subpixel_factor;
				background.linesets[set].angle[line] = 0x40U;
			}
			background.linesets[set].velocity_y = 0;
			center_x = word_add(center_x, 256 * subpixel_factor);
		}
		for(auto &particle : background.particles) {
			const auto x = pc98::th0405::randring_next16_mod(state.random, 6144U);
			if(!x) return false;
			particle.pos.x = std::bit_cast<std::int16_t>(*x);
			const auto y = pc98::th0405::randring_next16_mod(state.random, 6144U);
			if(!y) return false;
			particle.pos.y = std::bit_cast<std::int16_t>(*y);
			particle.origin.x = particle.pos.x;
			particle.origin.y = -subpixel_factor;
			particle.velocity = {};
			const auto patnum = pc98::th0405::randring_next16_and(state.random, 3U);
			if(!patnum) return false;
			particle.patnum = static_cast<std::uint8_t>(
				particle_pat_first + static_cast<std::int16_t>(*patnum)
			);
		}
		background.type_b_initialized = true;
		return true;
	}

	const auto frame_mod2 = static_cast<std::int16_t>(state.stage_frame & 1U);
	for(auto &particle : background.particles) {
		if(particle.velocity.y < (10 * subpixel_factor)) {
			particle.velocity.y = word_add(particle.velocity.y, frame_mod2);
		}
	}
	return true;
}

[[nodiscard]] bool type_b_update_and_render(State &state) noexcept
{
	if(!type_b_update_part1(state)) return false;
	auto &background = state.shinki_background;
	const auto frame_mod4 = state.stage_frame & 3U;
	std::int16_t delta = 4;
	for(std::size_t set = 0U; set < 2U; ++set) {
		auto &lineset = background.linesets[set];
		lineset_forward_copy(lineset);
		lineset.center[0].y = word_add(lineset.center[0].y, lineset.velocity_y);
		if(lineset.velocity_y > (-14 * subpixel_factor)) {
			lineset.velocity_y = word_sub(
				lineset.velocity_y, frame_mod4 == 0U ? 1 : 0
			);
		}
		spinline_x_and_angle(lineset, delta, background.spinline_frame);
		delta = word_sub(0, delta);
	}
	background.spinline_frame = static_cast<std::uint16_t>(background.spinline_frame + 1U);
	if(background.linesets[0].center[0].y < spinline_top) {
		spinline_formation_top(background);
	}
	particles_update(background);
	return true;
}

void type_c_update_part1(State &state) noexcept
{
	auto &background = state.shinki_background;
	if(!background.type_c_initialized && (background.particles[0].velocity.y < 0)) {
		for(auto &particle : background.particles) {
			particle.origin.y = (368 + 1) * subpixel_factor;
		}
		background.type_c_initialized = true;
	}

	const auto frame_mod2 = static_cast<std::int16_t>(state.stage_frame & 1U);
	for(auto &particle : background.particles) {
		if(particle.velocity.y > (-10 * subpixel_factor)) {
			particle.velocity.y = word_sub(particle.velocity.y, frame_mod2);
		}
	}
}

void type_c_update_and_render(State &state) noexcept
{
	type_c_update_part1(state);
	auto &background = state.shinki_background;
	const auto frame_mod2 = static_cast<std::int16_t>(state.stage_frame & 1U);
	std::int16_t delta = 4;
	for(std::size_t set = 0U; set < 2U; ++set) {
		auto &lineset = background.linesets[set];
		lineset_forward_copy(lineset);
		lineset.center[0].y = word_add(lineset.center[0].y, lineset.velocity_y);
		if(lineset.velocity_y < (14 * subpixel_factor)) {
			lineset.velocity_y = word_add(lineset.velocity_y, frame_mod2);
		}
		spinline_x_and_angle(lineset, delta, background.spinline_frame);
		delta = word_sub(0, delta);
	}
	background.spinline_frame = static_cast<std::uint16_t>(background.spinline_frame + 1U);
	if(background.linesets[0].center[0].y < spinline_top) {
		spinline_formation_top(background);
	} else if(background.linesets[0].center[0].y > spinline_bottom) {
		spinline_formation_bottom(background);
	}
	particles_update(background);
}

void type_d_update(State &state) noexcept
{
	auto &background = state.shinki_background;
	if(!background.type_d_initialized) {
		for(auto &particle : background.particles) {
			particle.origin.y = (256 - 16) * subpixel_factor;
			particle.velocity.y = -subpixel_factor;
		}
		background.type_d_initialized = true;
	}
	for(auto &particle : background.particles) {
		auto cel = 3 - (static_cast<std::int32_t>(particle.pos.y) / (64 * subpixel_factor));
		if(cel < 0) cel = 0;
		particle.patnum = static_cast<std::uint8_t>(particle_pat_first + cel);
	}
	particles_update(background);
}

} // namespace

bool shinki_background_update_and_render(State &state) noexcept
{
	if(state.boss.phase == 0U) return true;
	if(state.boss.phase == 1U) {
		state.tiles_bb_color = 15U;
		return true;
	}
	if(state.boss.phase < 4U) return type_a_update_and_render(state);
	if(state.boss.phase < 8U) return type_b_update_and_render(state);
	if(state.boss.phase < 12U) {
		type_c_update_and_render(state);
		return true;
	}
	type_d_update(state);
	return true;
}

} // namespace th05::core
