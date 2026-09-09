#include "th02/core/bomb_animation.hpp"

#include "th02/core/random.hpp"

#include <array>
#include <bit>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_top = 16;
constexpr std::int16_t playfield_right = 416;
constexpr std::int16_t screen_bottom = 400;
constexpr std::int16_t circle_frames = 32;
constexpr std::int16_t reimu_a_clear_frame = 136;
constexpr std::int16_t reimu_a_done_frame = 156;
constexpr std::int16_t reimu_b_cel_count = 18;
constexpr std::int16_t reimu_b_clear_frame = 164;
constexpr std::int16_t reimu_b_done_frame = 184;
constexpr std::int16_t reimu_c_clear_frame = 112;
constexpr std::int16_t reimu_c_done_frame = 128;

// ReC98/th02_main.asm::BOMB_SMEAR_SPEEDS. These are source signed words,
// copied by bomb_reimu_c before each frame's bottom update.
constexpr std::array<std::int16_t, bomb_smear_columns> bomb_smear_speeds{
	4, 3, 3, 2, 2, 1, 1, 0, 0, 2, 3, 2,
	1, 0, 1, 2, 1, 0, 0, 0, 2, 3, 3, 2,
	3, 1, 2, 0, 0, 1, 1, 2, 2, 3, 1, 2,
	3, 2, 1, 0, 1, 1, 2, 2, 3, 3, 4, 4,
};

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

[[nodiscard]] std::int16_t arithmetic_shift_right_one(
	std::int16_t value) noexcept
{
	const auto bits = std::bit_cast<std::uint16_t>(value);
	const auto shifted = static_cast<std::uint16_t>(bits >> 1U);
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		shifted | ((bits & 0x8000U) != 0U ? 0x8000U : 0U)));
}

[[nodiscard]] std::uint8_t frame_low_byte(std::int16_t frame) noexcept
{
	return static_cast<std::uint8_t>(
		std::bit_cast<std::uint16_t>(frame) & 0x00FFU
	);
}

[[nodiscard]] std::uint8_t frame_low_bits(std::int16_t frame,
	std::uint8_t mask) noexcept
{
	return static_cast<std::uint8_t>(frame_low_byte(frame) & mask);
}

[[nodiscard]] std::int16_t screen_top_with_scroll(std::int16_t base,
	std::int16_t scroll_line) noexcept
{
	auto top = word_add(base, scroll_line);
	if(top >= screen_bottom) {
		top = word_subtract(top, screen_bottom);
	}
	return top;
}

void set_tone(BombAnimationState &animation,
	BombAnimationCallbacks &callbacks, std::int32_t tone) noexcept
{
	animation.palette_tone = static_cast<std::uint16_t>(tone);
	callbacks.palette_show();
}

void emit_bft(std::int16_t frame, BombAnimationCallbacks &callbacks) noexcept
{
	// bomb_bft_put() derives its source cell as (bomb_frame - 1) >> 1 and
	// then draws the complete 24-row playfield. The callback is one typed
	// presentation request for that aggregate helper, not a pixel emulator.
	callbacks.bomb_bft(arithmetic_shift_right_one(word_subtract(frame, 1)));
}

void emit_boxfill(BombAnimationCallbacks &callbacks) noexcept
{
	callbacks.boxfill(playfield_left, 0, playfield_right - 1,
		screen_bottom - 1);
}

[[nodiscard]] BombAnimationResult clear_bullets(
	EnemyBulletPoolState &bullets, std::size_t page_back,
	std::size_t page_front, BombAnimationCallbacks &callbacks) noexcept
{
	return bullets_clear(bullets, page_back, page_front, callbacks) ==
			EnemyBulletLifecycleResult::completed ?
		BombAnimationResult::running : BombAnimationResult::invalid_page;
}

void emit_a_pi(std::int16_t base_top, std::int16_t scroll_line,
	std::int16_t left, bool reduce_effects, std::int16_t frame,
	BombAnimationCallbacks &callbacks) noexcept
{
	if(!reduce_effects || (frame_low_byte(frame) & 1U) != 0U) {
		callbacks.pi_put(left, screen_top_with_scroll(base_top, scroll_line), 1U);
	}
}

void emit_b_pi(std::int16_t base_top, std::int16_t scroll_line,
	std::int16_t left, bool reduce_effects, std::int16_t frame,
	BombAnimationCallbacks &callbacks) noexcept
{
	if(!reduce_effects || (frame_low_byte(frame) & 1U) != 0U) {
		callbacks.grcg_off();
		callbacks.pi_put(left, screen_top_with_scroll(base_top, scroll_line), 1U);
	}
}

} // namespace

BombAnimationResult bomb_reimu_a(
	BombState &bomb, BombAnimationState &animation, BombEffectState &effects,
	SparkPoolState &sparks, EnemyBulletPoolState &bullets, RandomState &random,
	std::size_t page_back, std::size_t page_front, std::int16_t scroll_line,
	bool reduce_effects, BombAnimationCallbacks &callbacks) noexcept
{
	const auto frame = bomb.bomb_frame;
	BombAnimationResult result = BombAnimationResult::running;
	callbacks.grcg_set_color(0U);
	if(frame == 1) {
		animation.col0_before_bomb_a = callbacks.palette_get(0U);
		animation.col3_before_bomb_a = callbacks.palette_get(3U);
		callbacks.palette_set(3U, {64U, 16U, 64U});
		callbacks.palette_show();
	}
	if(frame < circle_frames) {
		callbacks.grcg_set_color(3U);
		emit_bft(frame, callbacks);
	} else if(frame == circle_frames) {
		callbacks.palette_set(3U, animation.col3_before_bomb_a);
		callbacks.palette_set(0U, {128U, 0U, 128U});
		callbacks.palette_show();
		animation.tile_mode_before_bomb_a = callbacks.tile_mode_get();
		callbacks.tile_mode_set(2U);
		emit_boxfill(callbacks);
	} else if(frame < reimu_a_clear_frame) {
		if(frame > 64) {
			callbacks.grcg_set_color(11U);
		}
		emit_boxfill(callbacks);
		callbacks.grcg_off();
		if(frame <= 64) {
			const auto component = static_cast<std::uint8_t>(
				128 - ((frame - circle_frames) * 4)
			);
			callbacks.palette_set(0U, {component, 0U, component});
			if(frame == 64) {
				set_tone(animation, callbacks, 200);
				callbacks.palette_set(0U, {255U, 255U, 255U});
			}
			callbacks.palette_show();
		} else {
			const auto spark_ok = sparks_add(sparks, page_back, 212, 190,
				50, 1, false);
			if(!spark_ok) {
				result = BombAnimationResult::spark_allocation_failed;
			}
			if(frame == 70) {
				set_tone(animation, callbacks, 100);
			} else {
				emit_a_pi(90, scroll_line, 112, reduce_effects,
					frame, callbacks);
			}
			if(frame >= 86) {
				const auto component = static_cast<std::uint8_t>(
					(frame_low_byte(frame) % 3U) != 0U ? 255U : 160U
				);
				callbacks.palette_set(0U,
					{component, component, component});
				set_tone(animation, callbacks,
					(static_cast<std::int32_t>(frame) * 2) - 72);
				if(frame > 111) {
					animation.slowdown_factor = 2U;
				}
			}
		}
		if(frame < 86) {
			callbacks.grcg_set_color(15U);
			if((frame_low_bits(frame, 3U)) == 0U) {
				callbacks.sound_effect(15U);
			}
			bomb_particles_update_and_render(bomb, effects, random, callbacks);
		}
		if(frame == 86) {
			callbacks.sound_effect(17U);
		}
	} else if(frame == reimu_a_clear_frame) {
		callbacks.palette_set(0U, animation.col0_before_bomb_a);
		callbacks.palette_show();
		callbacks.sound_effect(16U);
		callbacks.tile_mode_set(animation.tile_mode_before_bomb_a);
		animation.slowdown_factor = 1U;
		set_tone(animation, callbacks, 200);
		result = clear_bullets(bullets, page_back, page_front, callbacks);
	} else if(frame < reimu_a_done_frame) {
		set_tone(animation, callbacks,
			200 - ((static_cast<std::int32_t>(frame) * 5) - 680));
	} else {
		callbacks.grcg_off();
		return BombAnimationResult::completed;
	}
	callbacks.grcg_off();
	return result;
}

BombAnimationResult bomb_reimu_b(
	BombState &bomb, BombAnimationState &animation, BombEffectState &effects,
	SparkPoolState &sparks, EnemyBulletPoolState &bullets, RandomState &random,
	std::size_t page_back, std::size_t page_front, std::int16_t scroll_line,
	bool reduce_effects, BombAnimationCallbacks &callbacks) noexcept
{
	const auto frame = bomb.bomb_frame;
	BombAnimationResult result = BombAnimationResult::running;
	static_cast<void>(sparks);
	callbacks.grcg_set_color(15U);
	if(frame < circle_frames) {
		emit_bft(frame, callbacks);
	} else if(frame == circle_frames) {
		animation.bomb_b_cel = 0U;
		animation.tile_mode_before_bomb_b = callbacks.tile_mode_get();
		callbacks.tile_mode_set(2U);
		emit_boxfill(callbacks);
	} else if(frame < 132) {
		if(frame > circle_frames && (frame_low_bits(frame, 3U) == 0U)) {
			if(frame_low_bits(frame, 7U) == 4U) {
				callbacks.sound_effect(16U);
			}
			callbacks.bomb_b_tram(animation.bomb_b_cel);
			animation.bomb_b_cel = static_cast<std::uint8_t>(
				animation.bomb_b_cel + 1U
			);
			if(animation.bomb_b_cel >=
				static_cast<std::uint8_t>(reimu_b_cel_count)) {
				callbacks.overlay_wipe();
				bomb.bomb_frame = 132;
			}
		}
		emit_boxfill(callbacks);
		callbacks.grcg_set_color(4U);
		bomb_particles_update_and_render(bomb, effects, random, callbacks);
	} else if(frame < reimu_b_clear_frame) {
		emit_boxfill(callbacks);
		animation.slowdown_factor = 2U;
		emit_b_pi(84, scroll_line, 32, reduce_effects, frame,
			callbacks);
		if(frame_low_bits(frame, 3U) == 0U) {
			callbacks.sound_effect(15U);
		}
		std::int16_t radius_step{};
		if(frame <= 140) {
			radius_step = word_subtract(frame, 132);
		} else if(frame <= 148) {
			radius_step = word_subtract(frame, 140);
		} else if(frame <= 156) {
			radius_step = word_subtract(frame, 148);
		} else {
			radius_step = word_subtract(frame, 156);
		}
		bomb_b_ring_put(bomb, radius_step, callbacks);
		set_tone(animation, callbacks,
			200 - ((static_cast<std::int32_t>(frame) * 3) - 396));
	} else if(frame == reimu_b_clear_frame) {
		callbacks.sound_effect(16U);
		callbacks.tile_mode_set(animation.tile_mode_before_bomb_b);
		animation.slowdown_factor = 1U;
		set_tone(animation, callbacks, 200);
		result = clear_bullets(bullets, page_back, page_front, callbacks);
	} else if(frame < reimu_b_done_frame) {
		set_tone(animation, callbacks,
			200 - ((static_cast<std::int32_t>(frame) * 5) - 820));
	} else {
		callbacks.grcg_off();
		return BombAnimationResult::completed;
	}
	callbacks.grcg_off();
	return result;
}

BombAnimationResult bomb_reimu_c(
	BombState &bomb, BombAnimationState &animation, BombEffectState &effects,
	SparkPoolState &sparks, EnemyBulletPoolState &bullets, RandomState &random,
	std::size_t page_back, std::size_t page_front, std::int16_t scroll_line,
	bool reduce_effects, BombAnimationCallbacks &callbacks) noexcept
{
	const auto frame = bomb.bomb_frame;
	BombAnimationResult result = BombAnimationResult::running;
	static_cast<void>(sparks);
	static_cast<void>(reduce_effects);
	callbacks.grcg_set_color(2U);
	if(frame < circle_frames) {
		emit_bft(frame, callbacks);
	} else if(frame == circle_frames) {
		animation.tile_mode_before_bomb_c = callbacks.tile_mode_get();
		callbacks.tile_mode_set(2U);
		emit_boxfill(callbacks);
		animation.smear_bottoms.fill(20);
		callbacks.sound_effect(17U);
	} else if(frame < 100) {
		emit_boxfill(callbacks);
		if(frame >= 80) {
			set_tone(animation, callbacks,
				(static_cast<std::int32_t>(frame) * 5) - 300);
			animation.slowdown_factor = 2U;
		}
		callbacks.grcg_set_color(14U);
		bomb_particles_update_and_render(bomb, effects, random, callbacks);
		callbacks.grcg_set_color(1U);
		for(std::size_t i = 0U; i < bomb_smear_columns; ++i) {
			animation.smear_bottoms[i] = word_add(
				animation.smear_bottoms[i], bomb_smear_speeds[i]);
			callbacks.bomb_smear(static_cast<std::uint8_t>(i),
				animation.smear_bottoms[i]);
		}
	} else if(frame < 112) {
		set_tone(animation, callbacks, 100);
		if((frame_low_byte(frame) & 1U) != 0U) {
			callbacks.grcg_off();
			callbacks.pi_put(32,
				screen_top_with_scroll(playfield_top, scroll_line), 1U);
			callbacks.sound_effect(16U);
		} else {
			callbacks.grcg_set_color(15U);
			emit_boxfill(callbacks);
		}
	} else if(frame == reimu_c_clear_frame) {
		callbacks.tile_mode_set(animation.tile_mode_before_bomb_c);
		animation.slowdown_factor = 1U;
		set_tone(animation, callbacks, 200);
		result = clear_bullets(bullets, page_back, page_front, callbacks);
	} else if(frame < reimu_c_done_frame) {
		set_tone(animation, callbacks,
			200 - ((static_cast<std::int32_t>(frame) * 5) - 540));
	} else {
		callbacks.grcg_off();
		return BombAnimationResult::completed;
	}
	callbacks.grcg_off();
	return result;
}

} // namespace th02::core
