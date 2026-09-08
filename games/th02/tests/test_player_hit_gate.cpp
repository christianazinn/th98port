#include "th02/core/player.hpp"

#include <cstdint>
#include <cstdlib>

namespace {
bool expected_guard(std::uint8_t timer, bool bomb, bool miss_active)
{
	return bomb || ((timer != 0) && !miss_active);
}
}

int main()
{
	using namespace th02::core;
	for(unsigned raw_hit = 0; raw_hit <= 0xFFU; ++raw_hit) {
		for(unsigned timer = 0; timer <= 0xFFU; ++timer) {
			for(const bool bomb : {false, true}) {
				for(const bool miss_active : {false, true}) {
					const PlayerHitState initial{
						static_cast<std::uint8_t>(raw_hit),
						static_cast<std::uint8_t>(timer), bomb, miss_active,
					};
					const auto guard_clears = expected_guard(
						initial.invincibility_time, initial.invincible_via_bomb,
						initial.miss_active);
					const auto move_allowed = guard_clears || (initial.raw_hit == 0);

					auto move_state = initial;
					if(player_move_and_shoot_allowed(move_state) != move_allowed ||
						move_state.raw_hit != (guard_clears ? 0 : initial.raw_hit) ||
						move_state.invincibility_time != initial.invincibility_time ||
						move_state.invincible_via_bomb != initial.invincible_via_bomb ||
						move_state.miss_active != initial.miss_active) {
						return EXIT_FAILURE;
					}

					auto callback_state = initial;
					prepare_player_callbacks(callback_state);
					if(callback_state.raw_hit != (guard_clears ? 0 : initial.raw_hit) ||
						callback_state.invincibility_time !=
							static_cast<std::uint8_t>(timer == 0 ? 0 : (timer - 1U)) ||
						callback_state.invincible_via_bomb != initial.invincible_via_bomb ||
						callback_state.miss_active != initial.miss_active) {
						return EXIT_FAILURE;
					}
				}
			}
		}
	}
	PlayerHitState timer_edge{1, 1, false, false};
	prepare_player_callbacks(timer_edge);
	if(timer_edge != PlayerHitState{0, 0, false, false}) return EXIT_FAILURE;
	PlayerHitState active_miss_edge{1, 1, false, true};
	prepare_player_callbacks(active_miss_edge);
	if(active_miss_edge != PlayerHitState{1, 0, false, true}) return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
