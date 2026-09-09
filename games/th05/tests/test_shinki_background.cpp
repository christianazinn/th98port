#include "th05/core/shinki_background.hpp"
#include "th05/core/state.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>

namespace {

using namespace th05::core;
using Point = pc98::th0405::SubpixelPoint;

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.random.ring.fill(0U);
	return state;
}

[[nodiscard]] bool initial_data_and_type_a_rng()
{
	auto state = source_state();
	auto &background = state.shinki_background;
	background.particles[1].pos.y = 777;
	background.particles[1].velocity = {12, 34};
	background.linesets[2].center[0] = {44, 55};
	state.boss.phase = 2U;
	state.boss.phase_frame = 0;
	if(!shinki_background_update_and_render(state)) return false;

	return background.type_a_particles_alive == 1 && state.random.cursor == 2U &&
		background.particles[0].pos == Point{3104, 2944} &&
		background.particles[0].origin == Point{3072, 2944} &&
		background.particles[0].velocity == Point{32, 0} &&
		background.particles[0].age == 1 && background.particles[0].angle == 0U &&
		background.particles[0].patnum == 0U &&
		background.particles[1].pos == Point{-15984, 777} &&
		background.particles[1].velocity == Point{12, 34} &&
		background.linesets[0].center[0] == Point{3132, 2944} &&
		background.linesets[1].center[0] == Point{3012, 2944} &&
		background.linesets[0].radius[0] == 80 && background.linesets[0].radius[1] == 16 &&
		background.linesets[0].angle[0] == 0x40U && background.linesets[1].angle[0] == 0xC0U &&
		background.linesets[2].center[0] == Point{44, 55};
}

[[nodiscard]] bool type_b_initialization_rng_order_and_retention()
{
	auto state = source_state();
	auto &background = state.shinki_background;
	background.linesets[0].center[19] = {12, 34};
	background.linesets[0].radius[19] = 56;
	background.linesets[0].angle[19] = 78U;
	background.particles[0].age = 9;
	background.particles[0].angle = 0xA5U;
	state.boss.phase = 4U;
	if(!shinki_background_update_and_render(state)) return false;

	return background.type_b_initialized && state.random.cursor == 192U &&
		background.spinline_frame == 1U &&
		background.linesets[0].center[0] == Point{1028, 3456} &&
		background.linesets[0].velocity_y == -1 &&
		background.linesets[0].radius[0] == 4094 &&
		background.linesets[0].angle[0] == 0x42U &&
		background.linesets[1].center[0] == Point{5116, 3456} &&
		background.linesets[1].angle[0] == 0x3EU &&
		background.linesets[0].center[19] == Point{12, 34} &&
		background.linesets[0].radius[19] == 56 && background.linesets[0].angle[19] == 78U &&
		background.particles[0].pos == Point{0, 0} &&
		background.particles[0].origin == Point{0, -16} &&
		background.particles[0].velocity == Point{0, 0} &&
		background.particles[0].patnum == 172U && background.particles[0].age == 10 &&
		background.particles[0].angle == 0xA5U;
}

[[nodiscard]] bool type_c_and_d_transitions()
{
	{
		auto state = source_state();
		auto &background = state.shinki_background;
		background.particles[0].pos = {2000, 2000};
		background.particles[0].velocity = {7, -1};
		state.boss.phase = 8U;
		state.stage_frame = 1U;
		if(!shinki_background_update_and_render(state)) return false;
		if(
			!background.type_c_initialized || background.particles[0].origin.y != 5904 ||
			background.particles[63].origin.y != 5904 ||
			background.particles[0].velocity != Point{7, -2} ||
			background.particles[0].pos != Point{2007, 1998} ||
			background.particles[0].age != 1
		) return false;
	}

	auto state = source_state();
	auto &background = state.shinki_background;
	background.particles[0].pos = {100, 1024};
	background.particles[0].velocity = {7, 9};
	state.boss.phase = 12U;
	if(!shinki_background_update_and_render(state)) return false;
	return background.type_d_initialized && background.particles[0].origin.y == 3840 &&
		background.particles[63].origin.y == 3840 &&
		background.particles[0].velocity == Point{7, -16} &&
		background.particles[0].patnum == 174U &&
		background.particles[0].pos == Point{107, 1008} && background.particles[0].age == 1;
}

[[nodiscard]] bool nonuniform_type_b_rng_order()
{
	auto state = source_state();
	for (std::size_t i = 0; i < state.random.ring.size(); ++i)
		state.random.ring[i] = static_cast<std::uint8_t>(i + 1);
	state.boss.phase = 4;
	if (!shinki_background_update_and_render(state)) return false;
	// Source consumes overlapping words for X then Y, then a byte for pattern.
	// Unequal ring bytes distinguish call order; a zero ring cannot do that.
	const auto &first = state.shinki_background.particles[0];
	const auto &last = state.shinki_background.particles[63];
	return state.random.cursor == 192 && first.pos == Point{513, 770} &&
		first.origin == Point{513, -16} && first.patnum == 175 &&
		last.pos == Point{6078, 191} && last.patnum == 172;
}

[[nodiscard]] bool phase_zero_one_are_rng_free_and_particle_clip_resets()
{
	auto state = source_state();
	const auto original = state.shinki_background;
	state.boss.phase = 0U;
	if(!shinki_background_update_and_render(state) || state.shinki_background != original ||
		state.random.cursor != 0U) return false;
	state.boss.phase = 1U;
	state.tiles_bb_color = 3U;
	if(!shinki_background_update_and_render(state) || state.shinki_background != original ||
		state.random.cursor != 0U || state.tiles_bb_color != 15U) return false;

	state.boss.phase = 2U;
	state.shinki_background.type_a_particles_alive = 64;
	auto &particle = state.shinki_background.particles[0];
	particle.pos = {6300, 1000};
	particle.origin = {42, 43};
	particle.age = 7;
	if(!shinki_background_update_and_render(state)) return false;
	return particle.pos == Point{42, 43} && particle.age == 0;
}

} // namespace

int main()
{
	if(!initial_data_and_type_a_rng()) {
		std::cerr << "type A data/RNG\n";
		return EXIT_FAILURE;
	}
	if(!type_b_initialization_rng_order_and_retention()) {
		std::cerr << "type B init/RNG\n";
		return EXIT_FAILURE;
	}
	if(!type_c_and_d_transitions()) {
		std::cerr << "type C/D\n";
		return EXIT_FAILURE;
	}
	if (!nonuniform_type_b_rng_order()) {
		std::cerr << "nonuniform type B RNG order\n";
		return EXIT_FAILURE;
	}
	if(!phase_zero_one_are_rng_free_and_particle_clip_resets()) {
		std::cerr << "phase/clip\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
