#include "th05/core/sword.hpp"

#include "th05/core/bullet.hpp"
#include "th05/core/state.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_motion.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace th05::core {
namespace {

constexpr std::int16_t subpixel_factor = 16;
constexpr std::int16_t sword_half_extent = 16 * subpixel_factor;
constexpr std::int16_t sword_killbox = 14 * subpixel_factor;
constexpr std::int16_t playfield_width = 384 * subpixel_factor;
constexpr std::int16_t playfield_height = 368 * subpixel_factor;
constexpr std::int16_t pat_sword = 193;
constexpr std::int16_t sword_cels = 32;
constexpr std::int16_t pat_decay_sword = pat_sword + sword_cels;
constexpr std::int16_t pat_decay_sword_end = pat_decay_sword + 4;

[[nodiscard]] constexpr std::int16_t word(std::uint16_t bits) noexcept
{
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] constexpr std::int16_t word_add(std::int16_t left, std::int16_t right) noexcept
{
	return word(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) + static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::int16_t word_sub(std::int16_t left, std::int16_t right) noexcept
{
	return word(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) - static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::uint8_t byte_add(std::uint8_t left, std::uint8_t right) noexcept
{
	return static_cast<std::uint8_t>(static_cast<std::uint16_t>(left) + right);
}

[[nodiscard]] constexpr bool overlap(std::int16_t delta, std::int16_t extent) noexcept
{
	return static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(delta) + static_cast<std::uint16_t>(extent / 2)
	) <= static_cast<std::uint16_t>(extent);
}

[[nodiscard]] constexpr bool encloses_32px(pc98::th0405::SubpixelPoint point) noexcept
{
	return point.x > -sword_half_extent && point.x < playfield_width + sword_half_extent &&
		point.y > -sword_half_extent && point.y < playfield_height + sword_half_extent;
}

[[nodiscard]] constexpr std::int16_t sword_patnum_for_angle(std::uint8_t angle) noexcept
{
	// th05/main/bullet/patnum.cpp does not fold at 0x80 for PAT_SWORD: the
	// source byte add is followed directly by the full 32-cel vector index.
	return static_cast<std::int16_t>(pat_sword + (byte_add(angle, 7U) >> 3U));
}

} // namespace

void swords_add(State &state) noexcept
{
	auto &swords = state.swords;
	const auto speed = bullet_speed_tune_for_playperf(
		swords.sword_template.speed, state.resources.playperf
	);
	// The source writes this before testing any of the 63 slots.
	state.circles_color = 9U;
	for(auto &sword : swords.pool) {
		if(sword.flag != sword_free) continue;
		sword.flag = sword_alive;
		sword.pos.cur = swords.sword_template.origin;
		// circles_add_shrinking() has no retained native gameplay effect.
		const auto velocity = pc98::masterlib::vector2(
			swords.sword_template.angle, static_cast<std::int16_t>(speed)
		);
		sword.pos.velocity = {velocity.x, velocity.y};
		sword.angle = swords.sword_template.angle;
		sword.patnum_tiny = sword_patnum_for_angle(sword.angle);
		sword.speed = speed;
		sword.twirl_time = swords.sword_template.twirl_time;
		return;
	}
}

void swords_update(State &state, Presentation &presentation) noexcept
{
	for(std::size_t index = 0; index < state.swords.pool.size(); ++index) {
		auto &sword = state.swords.pool[index];
		if(sword.flag == sword_free) continue;

		if(state.resources.bullet_clear_time != 0U && sword.flag == sword_alive) {
			sword.flag = sword_remove;
			sword.twirl_time = 0U;
		}

		if(sword.twirl_time != 0U) {
			--sword.twirl_time;
			if(sword.twirl_time != 0U) {
				// Source DI starts at one, so pool index zero twirls clockwise.
				if(((index + 1U) & 1U) != 0U) {
					sword.patnum_tiny = word_add(sword.patnum_tiny, 2);
					if(sword.patnum_tiny >= pat_sword + sword_cels) {
						sword.patnum_tiny = word_sub(sword.patnum_tiny, sword_cels);
					}
				} else {
					sword.patnum_tiny = word_sub(sword.patnum_tiny, 2);
					if(sword.patnum_tiny < pat_sword) {
						sword.patnum_tiny = word_add(sword.patnum_tiny, sword_cels);
					}
				}
				continue;
			}
			sword.patnum_tiny = sword_patnum_for_angle(sword.angle);
			presentation.sound(3U);
		}

		const auto position = pc98::th0405::update_motion(sword.pos);
		if(!encloses_32px(position)) {
			sword.flag = sword_free;
			continue;
		}

		if(sword.flag != sword_remove) {
			if(overlap(word_sub(position.x, state.player_pos.cur.x), sword_killbox) &&
				overlap(word_sub(position.y, state.player_pos.cur.y), sword_killbox)) {
				state.player_is_hit = true;
				sword.flag = sword_remove;
			}
			continue;
		}

		if(sword.patnum_tiny < pat_decay_sword) {
			sword.patnum_tiny = pat_decay_sword;
			sword.pos.velocity.x = static_cast<std::int16_t>(sword.pos.velocity.x / 2);
			sword.pos.velocity.y = static_cast<std::int16_t>(sword.pos.velocity.y / 2);
			sword.decay_frame = 0;
			continue;
		}

		sword.decay_frame = word_add(sword.decay_frame, 1);
		if((sword.decay_frame % 4) != 0) continue;
		sword.patnum_tiny = word_add(sword.patnum_tiny, 1);
		if(sword.patnum_tiny >= pat_decay_sword_end) sword.flag = sword_free;
	}
}

void swords_render_tail(State &) noexcept
{
	// th05/main/bullet/swords_render.asm only draws each non-free 32x32 cel.
}

} // namespace th05::core
