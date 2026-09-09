#include "th02/core/bomb_effects.hpp"
#include "th02/core/random.hpp"

#include "pc98/masterlib_vector.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

enum class EventKind : std::uint8_t { color, circle, particle, off };

struct Event {
	EventKind kind{};
	std::uint8_t color{};
	ScreenPoint point{};
	std::int16_t cel{};
	bool operator==(const Event &) const = default;
};

class Recorder final : public BombEffectCallbacks {
public:
	const BombState *bomb{};
	const BombEffectState *effects{};
	const RandomState *random{};
	std::array<Event, 256U> events{};
	std::size_t count{};
	bool circle_prefix_visible{true};
	bool particle_prefix_visible{true};

	void grcg_set_color(std::uint8_t color) noexcept override
	{
		if(bomb != nullptr && effects != nullptr && count == 0U) {
			circle_prefix_visible &= bomb->bomb_circle_frame == 10 &&
				effects->circle_scroll_line_unused[0U] == 123;
		}
		add({EventKind::color, color, {}, 0});
	}

	void circle_point(ScreenPoint point) noexcept override
	{
		add({EventKind::circle, 0U, point, 0});
	}

	void particle(ScreenPoint point, std::int16_t cel) noexcept override
	{
		if(bomb != nullptr && effects != nullptr && random != nullptr &&
			count == 0U) {
			particle_prefix_visible &= random->cursor == 4U &&
				effects->particle_positions[0U] == ScreenPoint{33, 160} &&
				effects->particle_positions[1U] == ScreenPoint{34, 48} &&
				effects->particle_cels[0U] == 2U &&
				effects->particle_cels[1U] == 1U;
		}
		add({EventKind::particle, 0U, point, cel});
	}

	void grcg_off() noexcept override
	{
		add({EventKind::off, 0U, {}, 0});
	}

private:
	void add(Event event) noexcept
	{
		if(count < events.size()) events[count] = event;
		++count;
	}
};

[[nodiscard]] std::int16_t word_add(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(lhs) + rhs
	));
}

[[nodiscard]] ScreenPoint polar_point(ScreenPoint center,
	std::int16_t radius, std::uint8_t angle) noexcept
{
	const auto vector = pc98::masterlib::vector2(angle, radius);
	return {word_add(center.x, vector.x), word_add(center.y, vector.y)};
}

[[nodiscard]] RandomState patterned_random() noexcept
{
	RandomState random{};
	// Four consecutive source reads overlap their high/low bytes. From cursor
	// zero, these are literal words 0x0001, 0x0200, 0x0002, and 0x0300, not
	// four independently packed 16-bit fixtures.
	random.ring[0U] = 1U;
	random.ring[1U] = 0U;
	random.ring[2U] = 2U;
	random.ring[3U] = 0U;
	random.ring[4U] = 3U;
	random.ring[5U] = 0U;
	random.ring[6U] = 4U;
	random.ring[7U] = 0U;
	return random;
}

[[nodiscard]] bool test_circle_source_prefix_geometry_and_page_boundary() noexcept
{
	BombState bomb{true, 0, {220, 196}, 9, 0, 0U, 0U};
	BombEffectState effects{};
	effects.circle_scroll_line_unused = {-9, -8};
	Recorder callbacks;
	callbacks.bomb = &bomb;
	callbacks.effects = &effects;
	if(bomb_circle_update_and_render(bomb, effects, 0U, 123, callbacks) !=
		BombCircleEffectResult::completed || !callbacks.circle_prefix_visible ||
		callbacks.count != 66U || callbacks.events[0U] !=
		Event{EventKind::color, 15U, {}, 0} || callbacks.events[65U] !=
		Event{EventKind::off, 0U, {}, 0} || bomb.bomb_circle_frame != 10 ||
		effects.circle_scroll_line_unused != std::array<std::int16_t, 2U>{123, -8} ||
		callbacks.events[1U] != Event{EventKind::circle, 0U, {476, 196}, 0} ||
		callbacks.events[17U] != Event{EventKind::circle, 0U, {220, 452}, 0} ||
		callbacks.events[33U] != Event{EventKind::circle, 0U, {-36, 196}, 0} ||
		callbacks.events[49U] != Event{EventKind::circle, 0U, {220, -60}, 0}) {
		return false;
	}

	bomb.bomb_frame = 32;
	callbacks = {};
	if(bomb_circle_update_and_render(bomb, effects, 1U, -77, callbacks) !=
		BombCircleEffectResult::completed || callbacks.count != 66U ||
		bomb.bomb_circle_frame != 11 ||
		effects.circle_scroll_line_unused != std::array<std::int16_t, 2U>{123, -77}) {
		return false;
	}
	for(std::size_t event = 1U; event <= 64U; ++event) {
		if(callbacks.events[event] != Event{EventKind::circle, 0U, {220, 196}, 0}) {
			return false;
		}
	}

	bomb.bomb_circle_frame = 0x7FFF;
	const auto before = effects;
	callbacks = {};
	return bomb_circle_update_and_render(bomb, effects, 2U, 5, callbacks) ==
		BombCircleEffectResult::invalid_page && bomb.bomb_circle_frame ==
		std::int16_t{-32768} && effects == before && callbacks.count == 0U;
}

[[nodiscard]] bool test_reimu_b_ring_literal_and_word_radius() noexcept
{
	BombState bomb{true, 0, {220, 196}, 0, 0, 0U, 0U};
	Recorder callbacks;
	bomb_b_ring_put(bomb, 8, callbacks);
	if(callbacks.count != 66U || callbacks.events[0U] !=
		Event{EventKind::color, 14U, {}, 0} || callbacks.events[65U] !=
		Event{EventKind::off, 0U, {}, 0} ||
		callbacks.events[1U] != Event{EventKind::circle, 0U,
			polar_point({220, 196}, 256, 8U), 0} ||
		callbacks.events[64U] != Event{EventKind::circle, 0U,
			polar_point({220, 196}, 256, static_cast<std::uint8_t>(8U + (63U * 4U))), 0}) {
		return false;
	}
	callbacks = {};
	bomb_b_ring_put(bomb, 2048, callbacks);
	if(callbacks.count != 66U) return false;
	for(std::size_t event = 1U; event <= 64U; ++event) {
		if(callbacks.events[event] != Event{EventKind::circle, 0U, {220, 196}, 0}) {
			return false;
		}
	}
	return bomb == BombState{true, 0, {220, 196}, 0, 0, 0U, 0U};
}

[[nodiscard]] bool test_particles_rng_prefix_cadence_and_raw_byte_edges() noexcept
{
	BombState bomb{true, 0, {220, 196}, 0, 0, 0U, 0U};
	BombEffectState effects{};
	effects.particle_positions[2U] = {102, 202};
	effects.particle_positions[3U] = {103, 203};
	effects.particle_positions[4U] = {104, 204};
	effects.particle_positions[8U] = {108, 208};
	effects.particle_cels[2U] = 4U;
	effects.particle_cels[3U] = 1U;
	effects.particle_cels[4U] = 4U;
	effects.particle_cels[8U] = 0xFFU;
	auto random = patterned_random();
	Recorder callbacks;
	callbacks.bomb = &bomb;
	callbacks.effects = &effects;
	callbacks.random = &random;
	// The first four positions below are literal `% 384 + 32` / `% 368 + 16`
	// results for those four overlapping randring words. They prove both the
	// read order and one-cursor-per-word contract without reusing an expected
	// random helper.
	bomb_particles_update_and_render(bomb, effects, random, callbacks);
	if(!callbacks.particle_prefix_visible || random.cursor != 4U ||
		callbacks.count != 5U ||
		callbacks.events[0U] != Event{EventKind::particle, 0U, {33, 160}, 1} ||
		callbacks.events[1U] != Event{EventKind::particle, 0U, {34, 48}, 0} ||
		callbacks.events[2U] != Event{EventKind::particle, 0U, {102, 202}, 3} ||
		callbacks.events[3U] != Event{EventKind::particle, 0U, {103, 203}, 0} ||
		callbacks.events[4U] != Event{EventKind::particle, 0U, {108, 208}, -1} ||
		effects.particle_cels[0U] != 2U || effects.particle_cels[1U] != 1U ||
		effects.particle_cels[2U] != 4U || effects.particle_cels[3U] != 1U ||
		effects.particle_cels[4U] != 0U || effects.particle_cels[8U] != 0U) {
		return false;
	}

	bomb.bomb_frame = 1;
	callbacks = {};
	bomb_particles_update_and_render(bomb, effects, random, callbacks);
	return random.cursor == 8U && callbacks.count == 4U &&
		callbacks.events[0U] == Event{EventKind::particle, 0U, {33, 160}, 1} &&
		callbacks.events[1U] == Event{EventKind::particle, 0U, {34, 48}, 1} &&
		callbacks.events[2U] == Event{EventKind::particle, 0U, {35, 304}, 0} &&
		callbacks.events[3U] == Event{EventKind::particle, 0U, {36, 16}, 0} &&
		effects.particle_cels[0U] == 2U && effects.particle_cels[1U] == 2U &&
		effects.particle_cels[2U] == 1U && effects.particle_cels[3U] == 1U;
}

} // namespace

int main()
{
	if(!test_circle_source_prefix_geometry_and_page_boundary()) {
		std::fputs("FAIL circle\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_reimu_b_ring_literal_and_word_radius()) {
		std::fputs("FAIL ring\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_particles_rng_prefix_cadence_and_raw_byte_edges()) {
		std::fputs("FAIL particles\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
