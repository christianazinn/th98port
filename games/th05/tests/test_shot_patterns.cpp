#include "th05/core/shot_patterns.hpp"
#include "th05/core/state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
using namespace th05::core;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
};

[[nodiscard]] State source_state(std::uint8_t playchar, std::uint8_t level,
	std::uint8_t shot_time) noexcept
{
	State state{};
	state.playchar = playchar;
	state.shot_level = level;
	state.player_pos.cur = {3000, 5000};
	state.player_pos.prev = {2990, 4990};
	state.shot_spawn.player_cur = {-1, -2};
	state.shot_spawn.shot_time = shot_time;
	for(std::size_t index = 0; index < state.random.ring.size(); ++index) {
		state.random.ring[index] = static_cast<std::uint8_t>(index);
	}
	return state;
}

[[nodiscard]] std::size_t shots_alive(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &shot : state.shots.shots) {
		if(shot.flag == shot_alive) ++count;
	}
	return count;
}

[[nodiscard]] bool every_table_entry_fires_source_count()
{
	constexpr std::array expected{
		std::array<std::size_t, 10>{1U, 1U, 3U, 4U, 5U, 5U, 5U, 7U, 7U, 9U},
		std::array<std::size_t, 10>{1U, 1U, 3U, 6U, 9U, 9U, 12U, 14U, 14U, 18U},
		std::array<std::size_t, 10>{1U, 1U, 3U, 4U, 6U, 7U, 7U, 9U, 9U, 10U},
		std::array<std::size_t, 10>{1U, 1U, 3U, 4U, 5U, 7U, 8U, 10U, 11U, 13U},
	};
	for(std::uint8_t playchar = 0U; playchar < 4U; ++playchar) {
		for(std::uint8_t level = 0U; level < 10U; ++level) {
			auto state = source_state(playchar, level, 18U);
			Trace presentation{};
			if(!player_shots_fire(state, presentation) ||
				shots_alive(state) != expected[playchar][level] ||
				state.shot_spawn.player_cur != state.player_pos.cur ||
				presentation.sounds != std::vector<std::uint8_t>{1U}) return false;
		}
	}
	return true;
}

[[nodiscard]] bool reimu_l9_and_marisa_l9_layouts()
{
	{
		auto state = source_state(0U, 9U, 18U);
		Trace presentation{};
		if(!player_shots_fire(state, presentation) || state.random.cursor != 0U) return false;
		const auto &outer_l = state.shots.shots[1];
		const auto &outer_r = state.shots.shots[3];
		const auto &forward_l = state.shots.shots[9];
		if(outer_l.pos.cur != Point{2616, 5000} || outer_l.patnum_base != 22U ||
			outer_l.type != shot_homing || outer_l.damage != 2 || outer_l.angle != 170U ||
			outer_r.pos.cur != Point{3384, 5000} || outer_r.angle != 214U ||
			forward_l.pos.cur != Point{3000, 5000} || forward_l.patnum_base != 20U ||
			forward_l.type != shot_normal || forward_l.damage != 9 || forward_l.angle != 182U) return false;
	}
	{
		auto state = source_state(1U, 9U, 18U);
		Trace presentation{};
		if(!player_shots_fire(state, presentation)) return false;
		const auto &last_missile = state.shots.shots[1];
		const auto &first_star = state.shots.shots[25];
		if(last_missile.pos.cur != Point{3384, 5000} || last_missile.patnum_base != 22U ||
			last_missile.type != shot_missile_straight || last_missile.damage != 9 ||
			last_missile.pos.velocity != Point{0, -64} ||
			first_star.pos.cur != Point{2360, 5000} || first_star.patnum_base != 20U ||
			first_star.damage != 4 || first_star.pos.velocity != Point{0, -192}) return false;
	}
	return true;
}

[[nodiscard]] bool mima_l8_and_yuuka_option_cadence()
{
	{
		auto state = source_state(2U, 8U, 18U);
		Trace presentation{};
		if(!player_shots_fire(state, presentation) || state.random.cursor != 0U) return false;
		const auto &forward = state.shots.shots[1];
		const auto &option_l = state.shots.shots[7];
		const auto &option_r = state.shots.shots[9];
		if(forward.patnum_base != 20U || forward.damage != 7 || forward.angle != 186U ||
			option_l.pos.cur != Point{2616, 5000} || option_l.pos.velocity != Point{0, -320} ||
			option_l.patnum_base != 22U || option_l.damage != 4 ||
			option_r.pos.cur != Point{3384, 5000} || option_r.pos.velocity != Point{0, -320}) return false;
	}
	{
		auto state = source_state(3U, 9U, 9U);
		Trace presentation{};
		if(!player_shots_fire(state, presentation) || shots_alive(state) != 8U ||
			state.random.cursor != 0U || !presentation.sounds.empty()) return false;
		const auto &first = state.shots.shots[1];
		const auto &last = state.shots.shots[15];
		if(first.pos.cur != Point{3384, 5000} || first.angle != 191U ||
			first.patnum_base != 22U || last.pos.cur != Point{2616, 5000} ||
			last.angle != 168U || last.patnum_base != 22U) return false;
	}
	return true;
}

[[nodiscard]] bool shared_random_and_bounded_faults()
{
	{
		auto state = source_state(0U, 7U, 9U);
		state.random.cursor = 4U;
		state.random.ring[4] = 3U;
		state.random.ring[5] = 1U;
		Trace presentation{};
		if(!player_shots_fire(state, presentation) || shots_alive(state) != 2U ||
			state.random.cursor != 6U || state.shots.shots[1].angle != 185U ||
			state.shots.shots[3].angle != 199U || !presentation.sounds.empty()) return false;
	}
	{
		auto state = source_state(1U, 1U, 18U);
		state.random.cursor = 5U;
		state.random.ring[5] = 6U;
		Trace presentation{};
		if(!player_shots_fire(state, presentation) || state.random.cursor != 6U ||
			state.shots.shots[1].angle != 194U || state.shots.shots[1].damage != 10) return false;
	}
	{
		auto state = source_state(0U, 2U, 18U);
		state.random.cursor = 256U;
		Trace presentation{};
		if(player_shots_fire(state, presentation) || state.random.cursor != 256U ||
			state.shots.shots[1].flag != shot_alive || state.shots.shots[1].pos.cur != Point{2616, 5000} ||
			state.shots.shots[1].type != shot_homing || state.shots.shots[1].damage != 4 ||
			state.shots.shots[5].flag != shot_alive || state.shots.shots[5].damage != 0 ||
			presentation.sounds != std::vector<std::uint8_t>{1U}) return false;
	}
	{
		auto state = source_state(4U, 0U, 18U);
		const auto player_cur = state.shot_spawn.player_cur;
		Trace presentation{};
		if(player_shots_fire(state, presentation) || state.shot_spawn.player_cur != player_cur ||
			state.shot_spawn.shot_cursor != 0U || !presentation.sounds.empty()) return false;
		state.playchar = 0U;
		state.shot_level = 10U;
		if(player_shots_fire(state, presentation) || state.shot_spawn.player_cur != player_cur ||
			state.shot_spawn.shot_cursor != 0U || !presentation.sounds.empty()) return false;
	}
	return true;
}

} // namespace

int main()
{
	if(!every_table_entry_fires_source_count() || !reimu_l9_and_marisa_l9_layouts() ||
		!mima_l8_and_yuuka_option_cadence() || !shared_random_and_bounded_faults()) {
		std::cerr << "TH05 shot-pattern test failure\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
