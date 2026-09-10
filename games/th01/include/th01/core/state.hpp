#pragma once

#include "th01/core/input.hpp"
#include "th01/core/player.hpp"
#include "th01/core/shot_orb.hpp"
#include "th01/core/boss.hpp"
#include "th01/core/projectiles.hpp"
#include "th01/core/stage.hpp"

#include <cstdint>

namespace th01::core {

// One REIIDEN gameplay instance. Host devices and replay containers are clients.
struct State {
	InputState input{};
	PlayerState player{};
	ShotOrbState orb{};
	Shots shots{};
	BossState boss{};
	ProjectileState projectiles{};
	StageState stage{};
	std::uint32_t random_seed{}, frame_rand{}, frame_since_start_of_binary{}, bomb_frame{};
	std::uint32_t score{};
	std::uint16_t point_value{};
	std::int16_t extend_next{1}, stage_id{}, rem_lives{4};
	std::int8_t rank{}, rem_bombs{1};
	std::int16_t cardcombo_cur{}, cardcombo_max{};
	std::int8_t route{};
	bool bomb_damaging{}, test_damage{}, stage_cleared{}, game_cleared{};
	bool pause_suffix_pending{};
	bool waiting_for_shot{};
	bool route_selection_pending{};
};
} // namespace th01::core
