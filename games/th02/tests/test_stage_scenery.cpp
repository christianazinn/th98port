#include "th02/core/stage_scenery.hpp"

#include "th02/core/random.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

enum class EventKind : std::uint8_t {
	invalidate,
	palette,
	tone,
	show,
	clip,
	egc_off,
	color,
	dot,
	grcg_off,
};

struct Event {
	EventKind kind{};
	std::uint8_t byte0{};
	std::uint8_t byte1{};
	std::int16_t value0{};
	std::int16_t value1{};
	std::int16_t value2{};
	std::int16_t value3{};
	ScreenPoint point{};
	StageSceneryRgb color{};
};

class Recorder final : public StageSceneryCallbacks {
public:
	std::array<Event, 512U> events{};
	std::size_t count{};

	void invalidate_scenery(StageSceneryInvalidation invalidation) noexcept override
	{
		add({EventKind::invalidate, 0U, 0U, invalidation.topleft.x,
			invalidation.topleft.y, invalidation.width, invalidation.height});
	}

	void palette_set(std::uint8_t index, StageSceneryRgb color) noexcept override
	{
		add({EventKind::palette, index, 0U, 0, 0, 0, 0, {}, color});
	}

	void palette_tone_set(std::uint16_t tone) noexcept override
	{
		add({EventKind::tone, 0U, 0U,
			static_cast<std::int16_t>(tone)});
	}

	void palette_show() noexcept override
	{
		add({EventKind::show});
	}

	void grc_set_clip(std::int16_t left, std::int16_t top,
		std::int16_t right, std::int16_t bottom) noexcept override
	{
		add({EventKind::clip, 0U, 0U, left, top, right, bottom});
	}

	void egc_off() noexcept override
	{
		add({EventKind::egc_off});
	}

	void grcg_set_color(std::uint8_t color) noexcept override
	{
		add({EventKind::color, color});
	}

	void dot_square(ScreenPoint topleft, std::uint8_t edge) noexcept override
	{
		add({EventKind::dot, edge, 0U, 0, 0, 0, 0, topleft});
	}

	void grcg_off() noexcept override
	{
		add({EventKind::grcg_off});
	}

	[[nodiscard]] std::size_t count_kind(EventKind kind) const noexcept
	{
		std::size_t found = 0U;
		for(std::size_t i = 0U; i < count && i < events.size(); ++i) {
			if(events[i].kind == kind) ++found;
		}
		return found;
	}

private:
	void add(Event event) noexcept
	{
		if(count < events.size()) events[count] = event;
		++count;
	}
};

[[nodiscard]] bool test_add_rng_and_pool_full() noexcept
{
	StageSceneryPoolState state{};
	state.particles[0U].flag = 0xFFU;
	state.particles[1U].unused = 0xA5U;
	RandomState random{};
	random.ring[0U] = 0xFEU;
	if(stage_scenery_particle_add(state, random, 100, 200) !=
		StageSceneryAddResult::added || random.cursor != 1U ||
		state.particles[1U].flag != stage_scenery_alive ||
		state.particles[1U].unused != 0xA5U ||
		state.particles[1U].left != std::array<std::int16_t, 2U>{100, 100} ||
		state.particles[1U].top != std::array<std::int16_t, 2U>{200, 200} ||
		state.particles[1U].speed != 5) {
		return false;
	}
	StageSceneryPoolState full{};
	for(auto &particle : full.particles) particle.flag = 0xFFU;
	RandomState full_random{};
	full_random.cursor = 7U;
	const auto before = full;
	return stage_scenery_particle_add(full, full_random, 1, 2) ==
		StageSceneryAddResult::pool_full && full == before &&
		full_random.cursor == 7U;
}

[[nodiscard]] bool test_invalidate_order_and_pages() noexcept
{
	StageSceneryPoolState state{};
	state.particles[0U].flag = stage_scenery_alive;
	state.particles[0U].left = {10, 11};
	state.particles[0U].top = {20, 21};
	state.particles[1U].flag = stage_scenery_remove;
	state.particles[1U].left = {30, 31};
	state.particles[1U].top = {40, 41};
	state.particles[2U].flag = 0xFFU;
	state.particles[2U].left = {50, 51};
	state.particles[2U].top = {60, 61};
	Recorder callbacks;
	if(stage_scenery_invalidate(state, 0U, 1U, callbacks) !=
		StageSceneryPageResult::completed || callbacks.count != 3U ||
		callbacks.events[0U].kind != EventKind::invalidate ||
		callbacks.events[0U].value0 != 10 || callbacks.events[0U].value1 != 20 ||
		callbacks.events[1U].value0 != 30 || callbacks.events[2U].value0 != 50 ||
		state.particles[0U].left[0U] != 11 ||
		state.particles[0U].top[0U] != 21 ||
		state.particles[1U].flag != stage_scenery_free ||
		state.particles[2U].flag != 0xFFU || state.particles[2U].left[0U] != 51 ||
		state.particles[2U].top[0U] != 61) {
		return false;
	}
	const auto before = state;
	callbacks = {};
	return stage_scenery_invalidate(state, 2U, 0U, callbacks) ==
		StageSceneryPageResult::invalid_page && state == before &&
		callbacks.count == 0U;
}

[[nodiscard]] bool test_render_bounds_and_page_refusal() noexcept
{
	StageSceneryPoolState state{};
	state.particles[0U] = {stage_scenery_alive, 0U, {40, 40}, {100, 100}, 5};
	state.particles[1U] = {stage_scenery_alive, 0U, {32, 32}, {100, 100}, 0};
	state.particles[2U] = {stage_scenery_alive, 0U, {20, 20}, {100, 100}, 5};
	state.particles[3U] = {stage_scenery_alive, 0U, {100, 100}, {383, 383}, 0};
	state.particles[4U] = {stage_scenery_alive, 0U, {100, 100}, {384, 384}, 0};
	state.particles[5U] = {stage_scenery_alive, 0U, {416, 416}, {100, 100}, 0};
	Recorder callbacks;
	if(stage_scenery_particles_render(state, 0U, 10, callbacks) !=
		StageSceneryPageResult::completed || callbacks.count_kind(EventKind::dot) != 3U ||
		callbacks.count_kind(EventKind::grcg_off) != 1U ||
		state.particles[0U].left[0U] != 35 || state.particles[0U].top[0U] != 105 ||
		state.particles[2U].flag != stage_scenery_remove ||
		callbacks.events[0U].point != ScreenPoint{35, 115} ||
		callbacks.events[0U].byte0 != 2U) {
		return false;
	}
	const auto before = state;
	callbacks = {};
	return stage_scenery_particles_render(state, 2U, 0, callbacks) ==
		StageSceneryPageResult::invalid_page && state == before &&
		callbacks.count == 0U;
}

[[nodiscard]] bool test_flash_cadence_and_rng() noexcept
{
	StageBgFlashState flash{};
	std::uint16_t palette_tone{};
	RandomState random{};
	Recorder callbacks;
	stage_bg_flash_update(flash, palette_tone, random, callbacks);
	if(flash.frame != 0 || flash.next_frame != 0U || random.cursor != 0U ||
		callbacks.count != 0U) {
		return false;
	}
	flash.frame = 1;
	stage_bg_flash_update(flash, palette_tone, random, callbacks);
	if(flash.frame != 2 || flash.next_frame != 100U || callbacks.count != 0U) {
		return false;
	}
	flash.frame = 99;
	callbacks = {};
	stage_bg_flash_update(flash, palette_tone, random, callbacks);
	if(flash.frame != 100 || palette_tone != 100U || callbacks.count != 2U ||
		callbacks.events[0U].kind != EventKind::tone ||
		callbacks.events[0U].value0 != 100 ||
		callbacks.events[1U].kind != EventKind::show) {
		return false;
	}
	flash.frame = 106;
	callbacks = {};
	stage_bg_flash_update(flash, palette_tone, random, callbacks);
	if(flash.frame != 107 || flash.next_frame != 120U ||
		palette_tone != 100U || random.cursor != 1U || callbacks.count != 4U ||
		callbacks.events[0U].value0 != 140 || callbacks.events[1U].kind != EventKind::show ||
		callbacks.events[2U].value0 != 100 || callbacks.events[3U].kind != EventKind::show) {
		return false;
	}
	StageBgFlashState high_half{
		std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(32767U)),
		32768U};
	callbacks = {};
	stage_bg_flash_update(high_half, palette_tone, random, callbacks);
	if(std::bit_cast<std::uint16_t>(high_half.frame) != 32768U ||
		callbacks.count != 2U || callbacks.events[0U].value0 != 100) {
		return false;
	}
	StageBgFlashState wrapped_window{
		std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(65529U)),
		65530U};
	callbacks = {};
	stage_bg_flash_update(wrapped_window, palette_tone, random, callbacks);
	return std::bit_cast<std::uint16_t>(wrapped_window.frame) == 65530U &&
		callbacks.count == 0U;
}

[[nodiscard]] bool test_stage_transition_and_periodic_add() noexcept
{
	StageSceneryPoolState state{};
	std::int16_t scenery_frame{};
	StageBgFlashState flash{};
	std::uint16_t palette_tone{};
	SparkUpdateParameters spark_parameters{};
	RandomState random{};
	Recorder callbacks;
	scenery_frame = 1;
	if(stage1_update_and_render(state, scenery_frame, flash, palette_tone,
		spark_parameters, random, 0U, 0, false, callbacks) !=
		Stage1SceneryResult::updated || scenery_frame != 2 ||
		callbacks.count_kind(EventKind::palette) != 2U ||
		callbacks.count_kind(EventKind::show) != 1U) {
		return false;
	}
	scenery_frame = 511;
	callbacks = {};
	if(stage1_update_and_render(state, scenery_frame, flash, palette_tone,
		spark_parameters, random, 0U, 0, false, callbacks) !=
		Stage1SceneryResult::updated || scenery_frame != 1000 ||
		spark_parameters.acceleration_x != -1 ||
		flash.frame != 1 || callbacks.count_kind(EventKind::clip) != 1U ||
		callbacks.count_kind(EventKind::palette) != 3U ||
		callbacks.count_kind(EventKind::show) != 1U) {
		return false;
	}
	flash.frame = 0;
	scenery_frame = 1002;
	random.ring[0U] = 0U;
	random.ring[1U] = 0U;
	callbacks = {};
	if(stage1_update_and_render(state, scenery_frame, flash, palette_tone,
		spark_parameters, random, 0U, 0, false, callbacks) !=
		Stage1SceneryResult::updated || scenery_frame != 1000 ||
		random.cursor != 2U ||
		state.particles[0U].flag != stage_scenery_alive ||
		state.particles[0U].left[0U] != 45 || state.particles[0U].top[0U] != 19 ||
		callbacks.count_kind(EventKind::dot) != 1U) {
		return false;
	}
	StageSceneryPoolState full{};
	for(auto &particle : full.particles) particle.flag = stage_scenery_alive;
	scenery_frame = 1002;
	random.cursor = 0U;
	callbacks = {};
	return stage1_update_and_render(full, scenery_frame, flash, palette_tone,
		spark_parameters, random, 0U, 0, false, callbacks) ==
		Stage1SceneryResult::pool_full &&
		random.cursor == 1U;
}

[[nodiscard]] bool test_stage_invalid_page_prefix() noexcept
{
	StageSceneryPoolState state{};
	state.particles[0U].flag = stage_scenery_alive;
	std::int16_t scenery_frame = 1000;
	StageBgFlashState flash{};
	std::uint16_t palette_tone{};
	SparkUpdateParameters spark_parameters{};
	RandomState random{};
	Recorder callbacks;
	const auto before = state;
	if(stage1_update_and_render(state, scenery_frame, flash, palette_tone,
		spark_parameters, random, 2U, 0, false, callbacks) !=
		Stage1SceneryResult::invalid_page) {
		return false;
	}
	return state == before && scenery_frame == 1001 &&
		callbacks.count_kind(EventKind::egc_off) == 1U &&
		callbacks.count_kind(EventKind::color) == 1U &&
		callbacks.count_kind(EventKind::grcg_off) == 0U;
}

} // namespace

int main()
{
	if(!test_add_rng_and_pool_full()) {
		std::fputs("FAIL add\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_invalidate_order_and_pages()) {
		std::fputs("FAIL invalidate\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_render_bounds_and_page_refusal()) {
		std::fputs("FAIL render\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_flash_cadence_and_rng()) {
		std::fputs("FAIL flash\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_stage_transition_and_periodic_add()) {
		std::fputs("FAIL stage\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_stage_invalid_page_prefix()) {
		std::fputs("FAIL invalid page\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
