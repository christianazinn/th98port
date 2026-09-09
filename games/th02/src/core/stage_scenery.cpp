#include "th02/core/stage_scenery.hpp"

#include "th02/core/random.hpp"

#include <bit>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_right = 416;
constexpr std::int16_t playfield_bottom = 384;
constexpr std::int16_t screen_bottom = 400;

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

[[nodiscard]] std::int16_t screen_top_with_scroll(std::int16_t top,
	std::int16_t scroll_line) noexcept
{
	top = word_add(top, scroll_line);
	if(top >= screen_bottom) {
		top = word_subtract(top, screen_bottom);
	}
	return top;
}

[[nodiscard]] std::uint8_t byte_from_i32(std::int32_t value) noexcept
{
	return static_cast<std::uint8_t>(value);
}

void set_flash_tone(std::uint16_t &palette_tone,
	StageSceneryCallbacks &callbacks, std::uint16_t tone) noexcept
{
	palette_tone = tone;
	callbacks.palette_tone_set(tone);
	callbacks.palette_show();
}

} // namespace

StageSceneryAddResult stage_scenery_particle_add(
	StageSceneryPoolState &state, RandomState &random, std::int16_t left,
	std::int16_t top) noexcept
{
	for(auto &particle : state.particles) {
		if(particle.flag != stage_scenery_free) {
			continue;
		}
		particle.flag = stage_scenery_alive;
		particle.left[0U] = left;
		particle.top[0U] = top;
		particle.left[1U] = left;
		particle.top[1U] = top;
		particle.speed = static_cast<std::int16_t>(
			(static_cast<std::uint8_t>(randring_next8(random) & 3U)) + 3
		);
		return StageSceneryAddResult::added;
	}
	return StageSceneryAddResult::pool_full;
}

StageSceneryPageResult stage_scenery_invalidate(
	StageSceneryPoolState &state, std::size_t page_back,
	std::size_t page_front, StageSceneryCallbacks &callbacks) noexcept
{
	if(page_back >= stage_scenery_page_count ||
		page_front >= stage_scenery_page_count) {
		return StageSceneryPageResult::invalid_page;
	}
	for(auto &particle : state.particles) {
		if(particle.flag == stage_scenery_free) {
			continue;
		}
		callbacks.invalidate_scenery({
			{particle.left[page_back], particle.top[page_back]}, 2, 3
		});
		if(particle.flag == stage_scenery_remove) {
			particle.flag = stage_scenery_free;
		} else {
			particle.left[page_back] = particle.left[page_front];
			particle.top[page_back] = particle.top[page_front];
		}
	}
	return StageSceneryPageResult::completed;
}

StageSceneryPageResult stage_scenery_particles_render(
	StageSceneryPoolState &state, std::size_t page_back,
	std::int16_t scroll_line, StageSceneryCallbacks &callbacks) noexcept
{
	if(page_back >= stage_scenery_page_count) {
		return StageSceneryPageResult::invalid_page;
	}
	for(auto &particle : state.particles) {
		if(particle.flag != stage_scenery_alive) {
			continue;
		}
		particle.left[page_back] = word_subtract(
			particle.left[page_back], particle.speed
		);
		particle.top[page_back] = word_add(
			particle.top[page_back], particle.speed
		);
		const auto left = particle.left[page_back];
		const auto top = particle.top[page_back];
		if(left < playfield_left) {
			particle.flag = stage_scenery_remove;
		} else if(top < playfield_bottom && left < playfield_right) {
			callbacks.dot_square({left,
				screen_top_with_scroll(top, scroll_line)}, 2U);
		}
	}
	callbacks.grcg_off();
	return StageSceneryPageResult::completed;
}

void stage_bg_flash_update(StageBgFlashState &state,
	std::uint16_t &palette_tone, RandomState &random,
	StageSceneryCallbacks &callbacks) noexcept
{
	if(state.frame == 0) {
		return;
	}
	if(state.frame == 1) {
		state.next_frame = 100U;
	}
	state.frame = word_add(state.frame, 1);
	const auto frame = std::bit_cast<std::uint16_t>(state.frame);
	const auto next_frame = state.next_frame;
	if(frame >= next_frame && frame < static_cast<std::uint16_t>(
		next_frame + 8U)) {
		set_flash_tone(palette_tone, callbacks,
			(frame & 1U) != 0U ?
				static_cast<std::uint16_t>(140U) :
				static_cast<std::uint16_t>(100U));
		if(frame == static_cast<std::uint16_t>(next_frame + 7U)) {
			set_flash_tone(palette_tone, callbacks, 100U);
			const auto delta = static_cast<std::uint16_t>(
				(randring_next16(random) & 1023U) + 20U
			);
			state.next_frame = static_cast<std::uint16_t>(
				state.next_frame + delta
			);
		}
	}
}

Stage1SceneryResult stage1_update_and_render(
	StageSceneryPoolState &state, std::int16_t &scenery_frame,
	StageBgFlashState &flash, std::uint16_t &palette_tone,
	SparkUpdateParameters &spark_parameters, RandomState &random,
	std::size_t page_back, std::int16_t scroll_line, bool reduce_effects,
	StageSceneryCallbacks &callbacks) noexcept
{
	const auto frame = scenery_frame;
	if(frame < 999 && frame != 0) {
		scenery_frame = word_add(frame, 1);
		const auto current = scenery_frame;
		const auto palette8_green = byte_from_i32(
			(static_cast<std::int32_t>(current) / 16) + 144
		);
		const auto palette8_blue = byte_from_i32(
			(static_cast<std::int32_t>(current) / 8) + 144
		);
		callbacks.palette_set(8U, {176U, palette8_green, palette8_blue});
		const auto component = static_cast<std::int32_t>(160) -
			(static_cast<std::int32_t>(current) / 16);
		const auto palette13_blue = static_cast<std::int32_t>(255) -
			(static_cast<std::int32_t>(current) / 4);
		callbacks.palette_set(13U, {
			byte_from_i32(component), byte_from_i32(component),
			byte_from_i32(palette13_blue)
		});
		if(byte_from_i32(component) <= 0x80U) {
			scenery_frame = 1000;
			callbacks.grc_set_clip(playfield_left, 0, playfield_right,
				screen_bottom - 1);
			callbacks.palette_set(14U, {224U, 192U, 176U});
			spark_parameters.acceleration_x = -1;
			flash.frame = 1;
		}
		callbacks.palette_show();
		return Stage1SceneryResult::updated;
	}

	if(frame < 1000) {
		return Stage1SceneryResult::idle;
	}
	stage_bg_flash_update(flash, palette_tone, random, callbacks);
	scenery_frame = word_add(frame, 1);
	StageSceneryAddResult add_result = StageSceneryAddResult::added;
	const auto threshold = static_cast<std::int32_t>(1002) +
		(reduce_effects ? 2 : 0);
	if(static_cast<std::int32_t>(scenery_frame) > threshold) {
		const auto left = static_cast<std::int16_t>(
			(randring_next16(random) % 640U) + 48U
		);
		add_result = stage_scenery_particle_add(state, random, left, 16);
		scenery_frame = 1000;
	}
	callbacks.egc_off();
	callbacks.grcg_set_color(8U);
	const auto render_result = stage_scenery_particles_render(
		state, page_back, scroll_line, callbacks
	);
	if(render_result == StageSceneryPageResult::invalid_page) {
		return Stage1SceneryResult::invalid_page;
	}
	if(add_result == StageSceneryAddResult::pool_full) {
		return Stage1SceneryResult::pool_full;
	}
	return Stage1SceneryResult::updated;
}

StageSceneryResult stage2_update_and_render(
	StageSceneryPoolState &state, std::uint16_t &scenery_frame, std::uint8_t &setup,
	StageBgFlashState &flash, std::uint16_t &palette_tone,
	SparkUpdateParameters &spark_parameters, RandomState &random,
	std::size_t page_back, std::int16_t scroll_line,
	StageSceneryCallbacks &callbacks) noexcept
{
	if (setup != 0) {
		callbacks.grc_set_clip(playfield_left, 0, playfield_right, screen_bottom - 1);
		setup = 0;
		spark_parameters.acceleration_x = -1;
	}
	++scenery_frame;
	StageSceneryAddResult added = StageSceneryAddResult::added;
	if ((scenery_frame & 1) == 0) {
		const auto left = static_cast<std::int16_t>((randring_next16(random) % 640U) + 48U);
		added = stage_scenery_particle_add(state, random, left, 16);
	}
	stage_bg_flash_update(flash, palette_tone, random, callbacks);
	callbacks.egc_off();
	callbacks.grcg_set_color(3);
	if (stage_scenery_particles_render(state, page_back, scroll_line, callbacks) == StageSceneryPageResult::invalid_page)
		return StageSceneryResult::invalid_page;
	return added == StageSceneryAddResult::pool_full ? StageSceneryResult::pool_full : StageSceneryResult::updated;
}
} // namespace th02::core
