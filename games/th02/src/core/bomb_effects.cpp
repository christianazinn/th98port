#include "th02/core/bomb_effects.hpp"

#include "th02/core/random.hpp"
#include "pc98/masterlib_vector.hpp"

#include <bit>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_top = 16;
constexpr std::uint16_t playfield_width = 384U;
constexpr std::uint16_t playfield_height = 368U;
constexpr std::int16_t bomb_circle_radius_start = 256;
constexpr std::int16_t bomb_circle_radius_delta = 8;
constexpr std::int16_t bomb_ring_radius_step = 32;
constexpr std::uint8_t grcg_white = 15U;
constexpr std::uint8_t grcg_reimu_b = 14U;

[[nodiscard]] std::int16_t word_from_i32(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] std::int16_t word_add(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	return word_from_i32(static_cast<std::int32_t>(lhs) + rhs);
}

[[nodiscard]] std::int16_t word_subtract(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	return word_from_i32(static_cast<std::int32_t>(lhs) - rhs);
}

[[nodiscard]] std::int16_t word_multiply(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	return word_from_i32(static_cast<std::int32_t>(lhs) * rhs);
}

[[nodiscard]] std::uint8_t frame_low_byte(std::int16_t frame) noexcept
{
	return static_cast<std::uint8_t>(
		std::bit_cast<std::uint16_t>(frame) & 0x00FFU
	);
}

[[nodiscard]] ScreenPoint polar_point(ScreenPoint center,
	std::int16_t radius, std::uint8_t angle) noexcept
{
	const auto offset = pc98::masterlib::vector2(angle, radius);
	return {
		word_add(center.x, offset.x),
		word_add(center.y, offset.y),
	};
}

void emit_ring(ScreenPoint center, std::int16_t radius, std::uint8_t angle,
	BombEffectCallbacks &callbacks
) noexcept
{
	for(std::uint8_t point = 0U; point < bomb_circle_points; ++point) {
		callbacks.circle_point(polar_point(center, radius, angle));
		angle = static_cast<std::uint8_t>(angle + bomb_circle_angle_step);
	}
}

} // namespace

BombCircleEffectResult bomb_circle_update_and_render(
	BombState &bomb, BombEffectState &effects, std::size_t page_back,
	std::int16_t scroll_line, BombEffectCallbacks &callbacks
) noexcept
{
	bomb.bomb_circle_frame = word_add(bomb.bomb_circle_frame, 1);
	if(page_back >= effects.circle_scroll_line_unused.size()) {
		return BombCircleEffectResult::invalid_page;
	}
	effects.circle_scroll_line_unused[page_back] = scroll_line;
	callbacks.grcg_set_color(grcg_white);
	const auto radius = word_subtract(bomb_circle_radius_start,
		word_multiply(bomb.bomb_frame, bomb_circle_radius_delta));
	emit_ring(bomb.bomb_circle_center, radius,
		frame_low_byte(bomb.bomb_frame), callbacks);
	callbacks.grcg_off();
	return BombCircleEffectResult::completed;
}

void bomb_b_ring_put(const BombState &bomb, std::int16_t radius_step,
	BombEffectCallbacks &callbacks
) noexcept
{
	callbacks.grcg_set_color(grcg_reimu_b);
	emit_ring(bomb.bomb_circle_center,
		word_multiply(radius_step, bomb_ring_radius_step),
		frame_low_byte(radius_step), callbacks);
	callbacks.grcg_off();
}

void bomb_particles_update_and_render(const BombState &bomb,
	BombEffectState &effects, RandomState &random,
	BombEffectCallbacks &callbacks
) noexcept
{
	const auto slot_base = static_cast<std::size_t>(
		(static_cast<std::uint16_t>(std::bit_cast<std::uint16_t>(bomb.bomb_frame)) &
			0x000FU) * 2U
	);
	for(std::size_t offset = 0U; offset < 2U; ++offset) {
		auto &position = effects.particle_positions[slot_base + offset];
		position.x = static_cast<std::int16_t>(
			(randring_next16(random) % playfield_width) +
			static_cast<std::uint16_t>(playfield_left)
		);
		position.y = static_cast<std::int16_t>(
			(randring_next16(random) % playfield_height) +
			static_cast<std::uint16_t>(playfield_top)
		);
		effects.particle_cels[slot_base + offset] = 1U;
	}

	const auto frame_mod_4 = static_cast<std::uint8_t>(
		std::bit_cast<std::uint16_t>(bomb.bomb_frame) & 0x0003U
	);
	for(std::size_t slot = 0U; slot < effects.particle_cels.size(); ++slot) {
		auto &cel = effects.particle_cels[slot];
		if(cel == 0U) continue;
		if(frame_mod_4 == static_cast<std::uint8_t>(slot & 3U)) {
			cel = static_cast<std::uint8_t>(cel + 1U);
			if(cel >= static_cast<std::uint8_t>(bomb_particle_cels + 1U)) {
				cel = 0U;
				continue;
			}
		}
		callbacks.particle(effects.particle_positions[slot],
			static_cast<std::int16_t>(cel) - 1);
	}
}

} // namespace th02::core
