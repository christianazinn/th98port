#include "th02/core/bg_particle.hpp"

#include "pc98/masterlib_vector.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

class RecordingCallbacks final : public BgParticleCallbacks {
public:
	std::array<std::uint8_t, 8U> colors{};
	std::size_t color_count{};
	std::array<ScreenPoint, bg_particle_count> dots{};
	std::array<std::uint8_t, bg_particle_count> dot_edges{};
	std::size_t dot_count{};
	std::array<ScreenPoint, bg_particle_count> unputs{};
	std::array<std::uint8_t, bg_particle_count> unput_edges{};
	std::size_t unput_count{};
	std::size_t grcg_off_count{};

	void invalidate_scenery(StageSceneryInvalidation) noexcept override {}
	void palette_set(std::uint8_t, StageSceneryRgb) noexcept override {}
	void palette_tone_set(std::uint16_t) noexcept override {}
	void palette_show() noexcept override {}
	void grc_set_clip(std::int16_t, std::int16_t, std::int16_t,
		std::int16_t) noexcept override {}
	void egc_off() noexcept override {}
	void grcg_set_color(std::uint8_t color) noexcept override
	{
		if(color_count < colors.size()) colors[color_count] = color;
		++color_count;
	}
	void dot_square(ScreenPoint topleft, std::uint8_t edge) noexcept override
	{
		if(dot_count < dots.size()) {
			dots[dot_count] = topleft;
			dot_edges[dot_count] = edge;
		}
		++dot_count;
	}
	void dot_square_unput(ScreenPoint topleft,
		std::uint8_t edge) noexcept override
	{
		if(unput_count < unputs.size()) {
			unputs[unput_count] = topleft;
			unput_edges[unput_count] = edge;
		}
		++unput_count;
	}
	void grcg_off() noexcept override { ++grcg_off_count; }
};

[[nodiscard]] bool test_reset_retains_record_tails() noexcept
{
	BgParticleState state{};
	state.particles[0U].flag = bg_particle_alive;
	state.particles[0U].angle = 0x23U;
	state.particles[0U].speed = 0x45U;
	state.unused_2 = 7U;
	state.unused_3 = 8U;
	bg_particles_reset(state);
	for(const auto &particle : state.particles) {
		if(particle.flag != bg_particle_free) return false;
	}
	return state.particles[0U].angle == 0x23U &&
		state.particles[0U].speed == 0x45U &&
		state.speed_initial == 32 && state.speed_delta == 1 &&
		state.angle_delta == 0 && state.edge_step == 32 &&
		state.color == 15U && state.unput_color == 0U && state.unused == -1 &&
		state.unused_2 == 7U && state.unused_3 == 8U;
}

[[nodiscard]] bool test_add_and_full_pool() noexcept
{
	BgParticleState state{};
	state.speed_initial = 0x1234;
	if(!bg_particles_add(state, 32, 16, 0xA0U)) return false;
	const auto &particle = state.particles[0U];
	if(particle.flag != bg_particle_alive || particle.angle != 0xA0U ||
		particle.screen_topleft != std::array<BgParticleSubpixelPoint, 2U>{
			BgParticleSubpixelPoint{512, 256}, BgParticleSubpixelPoint{512, 256}} ||
		particle.edge != 1U || particle.speed != 0x34U) {
		return false;
	}
	for(auto &slot : state.particles) slot.flag = bg_particle_alive;
	const auto before = state;
	return !bg_particles_add(state, 96, 32, 0U) && state == before;
}

[[nodiscard]] bool test_update_render_and_clip_prefixes() noexcept
{
	BgParticleState state{};
	state.color = 9U;
	state.speed_delta = 4;
	state.angle_delta = 1;
	state.edge_step = 8;
	state.unused = -1;
	auto &particle = state.particles[0U];
	particle.flag = bg_particle_alive;
	particle.screen_topleft[0U] = {1600, 1600};
	particle.speed = 16U;
	RecordingCallbacks callbacks{};
	const auto velocity = pc98::masterlib::vector2(1U, 20);
	if(bg_particles_update_and_render(state, 0U, false, callbacks) !=
		BgParticlePageResult::completed) return false;
	if(particle.speed != 20U || particle.angle != 1U ||
		particle.screen_topleft[0U] != BgParticleSubpixelPoint{
			static_cast<std::int16_t>(1600 + velocity.x),
			static_cast<std::int16_t>(1600 + velocity.y)} ||
		particle.edge != 2U || callbacks.color_count != 1U ||
		callbacks.colors[0U] != 9U || callbacks.dot_count != 1U ||
		callbacks.dot_edges[0U] != 2U || callbacks.grcg_off_count != 1U ||
		state.unused_2 != 0xFFU || state.unused_3 != 9U) {
		return false;
	}

	BgParticleState clipped{};
	clipped.color = 3U;
	clipped.edge_step = 32;
	auto &edge = clipped.particles[0U];
	edge.flag = bg_particle_alive;
	edge.screen_topleft[0U] = {static_cast<std::int16_t>(416 * 16), 1600};
	RecordingCallbacks clip_callbacks{};
	return bg_particles_update_and_render(clipped, 0U, false, clip_callbacks) ==
			BgParticlePageResult::completed &&
		edge.flag == bg_particle_remove && clip_callbacks.dot_count == 0U &&
		clip_callbacks.grcg_off_count == 1U;
}

[[nodiscard]] bool test_invalidation_carry_and_reduced_schedule() noexcept
{
	BgParticleState state{};
	state.unput_color = 6U;
	auto &removed = state.particles[0U];
	removed.flag = bg_particle_remove;
	removed.edge = 3U;
	removed.screen_topleft[1U] = {1600, 800};
	auto &alive = state.particles[1U];
	alive.flag = bg_particle_alive;
	alive.edge = 2U;
	alive.screen_topleft[0U] = {3200, 1600};
	alive.screen_topleft[1U] = {0, 0};
	RecordingCallbacks callbacks{};
	if(bg_particles_invalidate(state, 1U, 0U, false, callbacks) !=
		BgParticlePageResult::completed) return false;
	if(callbacks.color_count != 1U || callbacks.colors[0U] != 6U ||
		callbacks.unput_count != 2U ||
		callbacks.unputs[0U] != ScreenPoint{100, 50} ||
		callbacks.unput_edges[0U] != 3U ||
		removed.flag != bg_particle_free ||
		alive.screen_topleft[1U] != alive.screen_topleft[0U] ||
		callbacks.grcg_off_count != 1U) {
		return false;
	}

	BgParticleState reduced{};
	reduced.particles[1U].flag = bg_particle_alive;
	reduced.particles[1U].edge = 1U;
	reduced.particles[1U].screen_topleft[0U] = {1600, 800};
	RecordingCallbacks reduced_callbacks{};
	if(bg_particles_invalidate(reduced, 0U, 1U, true, reduced_callbacks) !=
		BgParticlePageResult::completed) return false;
	return reduced_callbacks.unput_count == 0U &&
		reduced.particles[1U].screen_topleft[0U] ==
			reduced.particles[1U].screen_topleft[1U];
}

[[nodiscard]] bool test_invalid_page_refuses_before_presentation() noexcept
{
	BgParticleState state{};
	state.particles[0U].flag = bg_particle_alive;
	const auto before = state;
	RecordingCallbacks callbacks{};
	return bg_particles_update_and_render(state, 2U, false, callbacks) ==
			BgParticlePageResult::invalid_page &&
		state == before && callbacks.color_count == 0U &&
		callbacks.grcg_off_count == 0U;
}

} // namespace

int main()
{
	const std::array tests{
		test_reset_retains_record_tails,
		test_add_and_full_pool,
		test_update_render_and_clip_prefixes,
		test_invalidation_carry_and_reduced_schedule,
		test_invalid_page_refuses_before_presentation,
	};
	for(std::size_t i = 0U; i < tests.size(); ++i) {
		if(!tests[i]()) {
			std::fprintf(stderr, "bg particle test %zu failed\n", i);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
