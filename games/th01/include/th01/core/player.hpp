#pragma once

#include <array>
#include <cstdint>

namespace th01::core {
struct State;

// Source-owned player modes and retained combo state (player/player.cpp).
struct PlayerState {
	std::int16_t left{304};
	std::int16_t invincibility_time{};
	bool invincible{}, invincible_against_orb{}, deflecting{}, sliding{};
	std::int8_t dash_cycle{}, mode_frame{}, mode{}, dash_direction{1};
	std::int8_t bomb_flag{}, submode{-1}, swing_deflection_frames{12};
	bool bombing{}, combo_enabled{};
	// The visual particles consume the gameplay IRAND stream when recycled.
	std::array<std::int16_t, 8> invincibility_sprite_frames{};
};

void player_reset(State &state) noexcept;
void player_update(State &state) noexcept;
void orb_player_hittest(State &state, std::int16_t repel_friction) noexcept;
void player_invincibility_update(State &state) noexcept;
void player_invincibility_particles_update(State &state) noexcept;
} // namespace th01::core
