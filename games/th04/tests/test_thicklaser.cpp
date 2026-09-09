#include "th04/core/state.hpp"
#include "th04/core/thicklaser.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using namespace th04::core;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	std::vector<ThickLaserGeometry> geometries{};

	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
	void thicklaser_geometry(const ThickLaserGeometry &geometry) noexcept override
	{
		geometries.push_back(geometry);
	}
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.player_pos.cur = {6000, 6000};
	return state;
}

[[nodiscard]] bool reset_add_and_full_pool()
{
	auto state = source_state();
	Trace trace{};
	state.thicklasers.template_.origin = {1234, 2345};
	state.thicklasers.template_.line_frames = 32;
	state.thicklasers.template_.static_frames = 48;
	state.thicklasers.template_.radius_max = 64;
	state.thicklasers.template_.col_outline = 8U;
	state.thicklasers.template_.unused_1 = 7;
	state.thicklasers.pool[0].flag = ThickLaserFlag::static_;
	state.thicklasers.pool[1].flag = ThickLaserFlag::grow;
	thicklasers_reset(state);
	if(state.thicklasers.pool[0].flag != ThickLaserFlag::free ||
		state.thicklasers.pool[1].flag != ThickLaserFlag::free ||
		state.thicklasers.template_.flag != ThickLaserFlag::line ||
		state.thicklasers.template_.cur_flag_frame != 0 ||
		state.thicklasers.template_.radius_cur != 1 ||
		state.thicklasers.template_.radius_speed != 1 ||
		state.thicklasers.template_.origin != ThickLaserPoint{1234, 2345} ||
		state.thicklasers.template_.line_frames != 32 ||
		state.thicklasers.template_.unused_1 != 7) return false;

	thicklaser_add(state, trace);
	const auto first = state.thicklasers.pool[0];
	state.thicklasers.template_.origin = {3456, 4567};
	state.thicklasers.template_.col_outline = 9U;
	thicklaser_add(state, trace);
	const auto full_before = state.thicklasers.pool;
	thicklaser_add(state, trace);
	return state.thicklasers.pool[0] == first &&
		state.thicklasers.pool[1].origin == ThickLaserPoint{3456, 4567} &&
		state.thicklasers.pool[1].col_outline == 9U &&
		state.thicklasers.pool == full_before &&
		trace.sounds == std::vector<std::uint8_t>{5U, 5U};
}

[[nodiscard]] bool lifecycle_and_post_free_timer()
{
	auto state = source_state();
	Trace trace{};
	state.thicklasers.template_ = {
		ThickLaserFlag::line, 0, {3000, 1000}, {}, 0, 2, 2, 8U, 0, 7, 1, 3,
	};
	thicklaser_add(state, trace);
	auto &laser = state.thicklasers.pool[0];
	for(std::size_t frame{}; frame < 2U; ++frame) {
		thicklasers_update_and_hittest(state, trace);
	}
	if(laser.flag != ThickLaserFlag::line || laser.cur_flag_frame != 2) return false;
	thicklasers_update_and_hittest(state, trace);
	if(laser.flag != ThickLaserFlag::grow || laser.cur_flag_frame != 1 ||
		trace.sounds != std::vector<std::uint8_t>{5U, 6U}) return false;
	thicklasers_update_and_hittest(state, trace);
	if(laser.radius_cur != 4 || laser.cur_flag_frame != 2) return false;
	thicklasers_update_and_hittest(state, trace);
	if(laser.flag != ThickLaserFlag::static_ || laser.radius_cur != 7 ||
		laser.cur_flag_frame != 1) return false;
	thicklasers_update_and_hittest(state, trace);
	thicklasers_update_and_hittest(state, trace);
	if(laser.flag != ThickLaserFlag::shrink || laser.cur_flag_frame != 1) return false;
	thicklasers_update_and_hittest(state, trace);
	if(laser.radius_cur != 4 || laser.cur_flag_frame != 2) return false;
	thicklasers_update_and_hittest(state, trace);
	return laser.flag == ThickLaserFlag::free && laser.radius_cur == 1 &&
		laser.cur_flag_frame == 3;
}

[[nodiscard]] bool white_core_collision()
{
	Trace trace{};
	{
		auto state = source_state();
		auto &laser = state.thicklasers.pool[0];
		laser.flag = ThickLaserFlag::static_;
		laser.origin = {3000, 2000};
		laser.radius_cur = 64;
		laser.static_frames = 100;
		state.player_pos.cur = {2232, 2512};
		thicklasers_update_and_hittest(state, trace);
		if(!state.player_is_hit) return false;
	}
	{
		auto state = source_state();
		auto &laser = state.thicklasers.pool[0];
		laser.flag = ThickLaserFlag::static_;
		laser.origin = {3000, 2000};
		laser.radius_cur = 64;
		laser.static_frames = 100;
		state.player_pos.cur = {2231, 2512};
		thicklasers_update_and_hittest(state, trace);
		if(state.player_is_hit) return false;
	}
	{
		auto state = source_state();
		auto &laser = state.thicklasers.pool[0];
		laser.flag = ThickLaserFlag::line;
		laser.origin = {3000, 2000};
		laser.radius_cur = 64;
		laser.line_frames = 1;
		state.player_pos.cur = {3000, 2512};
		thicklasers_update_and_hittest(state, trace);
		if(state.player_is_hit) return false;
	}
	return true;
}

[[nodiscard]] bool render_geometry_is_ordered_and_inert()
{
	auto state = source_state();
	Trace trace{};
	state.thicklasers.pool[0] = {
		ThickLaserFlag::line, 0, {1111, 2222}, {}, 3, 0, 0, 8U, 0, 0, 1, 1,
	};
	state.thicklasers.pool[1] = {
		ThickLaserFlag::static_, 0, {3333, 4444}, {}, 5, 0, 0, 9U, 0, 70, 70, 1,
	};
	const auto before = state.thicklasers.pool;
	thicklasers_render(state, trace);
	return state.thicklasers.pool == before && trace.sounds.empty() &&
		trace.geometries == std::vector<ThickLaserGeometry>{
			{ThickLaserFlag::line, {1111, 2222}, 8U, 0, 0},
			{ThickLaserFlag::static_, {3333, 4444}, 9U, 70, 16},
		};
}

} // namespace

int main()
{
	const std::array checks{
		reset_add_and_full_pool(), lifecycle_and_post_free_timer(),
		white_core_collision(), render_geometry_is_ordered_and_inert(),
	};
	for(std::size_t index{}; index < checks.size(); ++index) {
		if(!checks[index]) {
			std::cerr << "TH04 thick-laser check failed: " << index << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
