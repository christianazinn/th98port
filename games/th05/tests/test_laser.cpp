#include "th05/core/laser.hpp"
#include "th05/core/state.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using namespace th05::core;
using Point = pc98::th0405::SubpixelPoint;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	std::vector<LaserGeometry> geometries{};

	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
	void laser_geometry(const LaserGeometry &geometry) noexcept override
	{
		geometries.push_back(geometry);
	}
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.player_pos.cur = {5000, 5000};
	state.resources.playperf = 32U;
	return state;
}

[[nodiscard]] bool manual_spawn_and_grow()
{
	auto state = source_state();
	Trace trace{};
	state.lasers.laser_template.coords.origin = {1700, 1800};
	state.lasers.laser_template.coords.angle = 64U;
	state.lasers.laser_template.coords.width = 4U;
	state.lasers.laser_template.color = 8U;
	if(!laser_manual_fixed_spawn(state, 0U, trace)) return false;
	auto &laser = state.lasers.pool[0];
	if(laser.flag != LaserFlag::fixed_wait_to_grow ||
		laser.coords.origin != Point{1700, 1800} ||
		laser.coords.starts_at_distance != laser_distance_min ||
		laser.coords.ends_at_distance != laser_distance_max || laser.coords.width != 1U ||
		laser.grow_to_width != 4U || trace.sounds != std::vector<std::uint8_t>{5U}) {
		return false;
	}
	if(!laser_manual_grow(state, 0U, trace)) return false;
	for(std::size_t frame{}; frame < 4U; ++frame) {
		if(!lasers_update(state, trace)) return false;
	}
	return laser.flag == LaserFlag::fixed_active && laser.coords.width == 5U &&
		laser.age == 4 && trace.sounds == std::vector<std::uint8_t>{5U, 6U};
}

[[nodiscard]] bool collision_gaps_and_clear_decay()
{
	auto state = source_state();
	Trace trace{};
	auto &laser = state.lasers.pool[0];
	laser.flag = LaserFlag::fixed_active;
	laser.coords.origin = {1000, 1000};
	laser.coords.starts_at_distance = 256;
	laser.coords.ends_at_distance = 256;
	laser.coords.angle = 0U;
	laser.coords.width = 4U;
	state.player_pos.cur = {1256, 1000};
	if(!lasers_update(state, trace) || !state.player_is_hit) return false;

	state = source_state();
	auto &gap = state.lasers.pool[0];
	gap.flag = LaserFlag::fixed_active;
	gap.coords.origin = {1000, 1000};
	gap.coords.starts_at_distance = 256;
	gap.coords.ends_at_distance = 512;
	gap.coords.angle = 0U;
	state.player_pos.cur = {1384, 1000};
	if(!lasers_update(state, trace) || state.player_is_hit) return false;

	state = source_state();
	auto &shootout = state.lasers.pool[0];
	shootout.flag = LaserFlag::shootout;
	shootout.coords.starts_at_distance = 256;
	shootout.coords.ends_at_distance = 256;
	shootout.coords.angle = 0U;
	shootout.coords.width = 6U;
	shootout.shootout_speed = 64;
	shootout.active_at_age = 2;
	shootout.age = 0;
	if(!lasers_update(state, trace) || shootout.coords.ends_at_distance != 320 ||
		shootout.coords.starts_at_distance != 256 || shootout.age != 1) return false;
	state.resources.bullet_clear_time = 1U;
	if(!lasers_update(state, trace)) return false;
	return shootout.flag == LaserFlag::shootout_decay && shootout.shootout_speed == 32 &&
		shootout.coords.ends_at_distance == 352 && shootout.coords.width == 8U &&
		shootout.age == 2;
}

[[nodiscard]] bool signed_shrink_and_render_invalidation()
{
	auto state = source_state();
	Trace trace{};
	auto &shrink = state.lasers.pool[0];
	shrink.flag = LaserFlag::fixed_shrink;
	shrink.coords.width = 128U;
	if(!lasers_update(state, trace) || shrink.flag != LaserFlag::free || shrink.age != 1) {
		return false;
	}
	auto &wait = state.lasers.pool[1];
	wait.flag = LaserFlag::fixed_shrink_and_wait_to_grow;
	wait.coords.width = 3U;
	wait.age = 1;
	if(!lasers_update(state, trace) || wait.flag != LaserFlag::fixed_wait_to_grow ||
		wait.coords.width != 1U || wait.age != 1) return false;
	wait.flag = LaserFlag::free;

	auto &offscreen = state.lasers.pool[2];
	offscreen.flag = LaserFlag::fixed_active;
	offscreen.color = 4U;
	offscreen.coords.origin = {-16000, 1000};
	offscreen.coords.starts_at_distance = laser_distance_min;
	offscreen.coords.ends_at_distance = laser_distance_max;
	offscreen.coords.angle = 0U;
	offscreen.coords.width = 6U;
	lasers_render(state, trace);
	if(offscreen.flag != LaserFlag::free || offscreen.coords.width != 6U ||
		trace.geometries.size() != 1U || trace.geometries[0].coords.width != 6U) {
		return false;
	}
	auto &decay = state.lasers.pool[3];
	decay.flag = LaserFlag::shootout_decay;
	decay.coords.width = 26U;
	decay.coords.origin = {-16000, 1000};
	lasers_render(state, trace);
	return decay.flag == LaserFlag::shootout_decay && trace.geometries.size() == 2U;
}

[[nodiscard]] bool slot_refusal_and_reached_wrap_refusal()
{
	auto state = source_state();
	Trace trace{};
	if(laser_manual_fixed_spawn(state, 32U, trace) ||
		laser_manual_grow(state, 32U, trace) || laser_stop(state, 32U)) return false;
	auto &laser = state.lasers.pool[0];
	laser.flag = LaserFlag::fixed_active;
	laser.coords.origin = {0, 0};
	laser.coords.angle = 0U;
	laser.coords.starts_at_distance = -32768;
	laser.coords.ends_at_distance = 32767;
	state.player_pos.cur = {-30000, -30000};
	return !lasers_update(state, trace) && laser.age == 0;
}

} // namespace

int main()
{
	const struct {
		const char *name;
		bool (*run)();
	} tests[] = {
		{"manual spawn and grow", manual_spawn_and_grow},
		{"collision gaps and clear decay", collision_gaps_and_clear_decay},
		{"signed shrink and render invalidation", signed_shrink_and_render_invalidation},
		{"slot and reached-wrap refusals", slot_refusal_and_reached_wrap_refusal},
	};
	for(const auto &test : tests) {
		if(!test.run()) {
			std::cerr << "FAIL " << test.name << '\n';
			return 1;
		}
	}
	std::cout << "PASS " << (sizeof(tests) / sizeof(tests[0])) << " laser tests\n";
	return 0;
}
