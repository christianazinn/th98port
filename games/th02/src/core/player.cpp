#include "th02/core/player.hpp"

#include "th02/core/shot.hpp"

#include <bit>

namespace th02::core {
namespace {
std::int16_t add_word(std::int16_t value, std::int16_t delta) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(value) + delta));
}

void negate_hits_if_invincible(PlayerHitState &state) noexcept
{
	if(state.invincible_via_bomb ||
		((state.invincibility_time != 0) && !state.miss_active)) {
		state.raw_hit = 0;
	}
}

std::int8_t add_byte(std::int8_t value, int delta) noexcept
{
	return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
		static_cast<int>(value) + delta));
}

constexpr std::uint16_t input_down = 0x0002;
constexpr std::uint16_t input_shot = 0x0010;
constexpr std::uint16_t input_bomb = 0x0020;
constexpr std::uint16_t input_movement_aligned = 0x000F;
constexpr std::uint16_t input_down_left = 0x4000;
constexpr std::uint16_t input_down_right = 0x8000;
constexpr std::uint16_t input_movement_diagonal = 0xF000;
constexpr std::uint8_t shot_stream_inactive = 0xFF;
constexpr std::uint8_t shot_level_max = 9;
constexpr std::uint8_t shot_cooldown = 8;
constexpr std::uint8_t shot_patnum_a_unpowered = 0x34;
constexpr std::uint8_t shot_patnum_b_unpowered = 0x3F;

void shot_patnums_set(PlayerShotStreamState &state) noexcept
{
	if(state.shot_level == shot_level_max) {
		state.shot_patnum = state.shot_patnum_powered;
		state.shot_option_patnum = state.shot_option_patnum_powered;
	} else {
		state.shot_patnum = shot_patnum_a_unpowered;
		state.shot_option_patnum = shot_patnum_b_unpowered;
	}
}
}

bool player_move(PlayerPositionState &state, std::size_t back_page,
	std::int16_t delta_x, std::int16_t delta_y) noexcept
{
	if(back_page >= state.on_page.size()) return false;
	auto &page = state.on_page[back_page];
	state.current.y = add_word(page.y, delta_y);
	if(state.current.y < 16) {
		state.current.y = 16;
	} else if(state.current.y > 344) {
		state.current.y = 344;
	}
	page.y = state.current.y;

	state.current.x = add_word(page.x, delta_x);
	if(state.current.x >= 390) {
		state.current.x = 390;
	} else if(state.current.x <= 28) {
		state.current.x = 28;
	}
	page.x = state.current.x;
	return true;
}

std::optional<MovementIntent> movement_intent(
	std::uint16_t input, MovementSpeeds speeds, std::size_t back_page) noexcept
{
	if(back_page > 1) return std::nullopt;
	MovementIntent result{};
	// Signed byte speeds promote to signed words before negation in the DOS
	// source. The wider intermediates retain -(-128) == 128.
	std::int16_t dx = 0, dy = 0;
	auto &pose = result.pose;
	switch(input & 0xF000U) {
	case 0x1000: dy = -speeds.diagonal_y; dx = -speeds.diagonal_x; pose = PlayerPose::left; break;
	case 0x2000: dy = -speeds.diagonal_y; dx = speeds.diagonal_x; pose = PlayerPose::right; break;
	case 0x4000: dy = speeds.diagonal_y; dx = -speeds.diagonal_x; pose = PlayerPose::left; break;
	case 0x8000: dy = speeds.diagonal_y; dx = speeds.diagonal_x; pose = PlayerPose::right; break;
	default:
		switch(input & 0x000FU) {
		case 1: dy = -speeds.aligned_y; dx = 0; pose = PlayerPose::still; break;
		case 2: dy = speeds.aligned_y; dx = 0; pose = PlayerPose::still; break;
		case 4: dx = -speeds.aligned_x; dy = 0; pose = PlayerPose::left; break;
		case 8: dx = speeds.aligned_x; dy = 0; pose = PlayerPose::right; break;
		case 5: dy = -speeds.diagonal_y; dx = -speeds.diagonal_x; pose = PlayerPose::left; break;
		case 9: dy = -speeds.diagonal_y; dx = speeds.diagonal_x; pose = PlayerPose::right; break;
		case 6: dy = speeds.diagonal_y; dx = -speeds.diagonal_x; pose = PlayerPose::left; break;
		case 10: dy = speeds.diagonal_y; dx = speeds.diagonal_x; pose = PlayerPose::right; break;
		default: dx = 0; dy = 0; pose = PlayerPose::still; break;
		}
	}
	if(speeds.aligned_x == 5) {
		const auto page = static_cast<std::int16_t>(back_page);
		if(dx < 0) dx += page;
		else if(dx > 0) dx -= page;
		if(dy < 0) dy += page;
		else if(dy > 0) dy -= page;
	}
	result.delta = {dx, dy};
	return result;
}

bool player_move_and_shoot_allowed(PlayerHitState &state) noexcept
{
	negate_hits_if_invincible(state);
	return state.raw_hit == 0;
}

void prepare_player_callbacks(PlayerHitState &state) noexcept
{
	negate_hits_if_invincible(state);
	if(state.invincibility_time > 0) {
		--state.invincibility_time;
	}
}

PlayerUpdateResult player_update_state_step(
	PlayerHitState &player, bool &quit, PlayerShotPoolState &shots,
	PlayerShotLifetimeState &lifetime, std::size_t back_page,
	std::uint32_t stage_frame, PlayerUpdateCallbacks &callbacks
) noexcept
{
	prepare_player_callbacks(player);
	const auto shot_result = shots_update_lifetime(
		shots, lifetime, back_page, stage_frame
	);
	if(shot_result == PlayerShotLifetimeUpdateResult::invalid_page) {
		return PlayerUpdateResult::shot_lifetime_invalid_page;
	}
	if(shot_result == PlayerShotLifetimeUpdateResult::source_divide_fault) {
		return PlayerUpdateResult::shot_lifetime_source_divide_fault;
	}

	if(player.raw_hit != 0U) {
		callbacks.player_miss_update(player);
		if(player.raw_hit == 0xFFU) {
			quit = true;
			player.raw_hit = 0U;
		}
		return PlayerUpdateResult::miss_handled;
	}
	return PlayerUpdateResult::completed;
}

void player_move_and_shoot_shot_stream_step(PlayerShotStreamState &state,
	std::uint16_t input, PlayerShotStreamCallbacks &callbacks) noexcept
{
	if((input & input_shot) != 0) {
		if(state.stream_a_phase == shot_stream_inactive) {
			state.stream_a_phase = 0;
			state.stream_a_cooldown = 0;
		} else if(state.stream_b_phase == shot_stream_inactive) {
			state.stream_b_phase = 0;
			state.stream_b_cooldown = 0;
		}
	} else {
		if(state.stream_a_phase >= 3) {
			state.stream_a_phase = shot_stream_inactive;
			state.stream_b_phase = 4;
		} else if(state.stream_b_phase >= 2) {
			state.stream_b_phase = shot_stream_inactive;
		}
	}

	if(state.stream_a_phase != shot_stream_inactive) {
		if(state.stream_a_cooldown == 0) {
			shot_patnums_set(state);
			callbacks.shot_volley(state, state.shot_patnum, state.shot_option_patnum);
			callbacks.shot_sound(state);
			state.stream_a_cooldown = shot_cooldown;
			++state.stream_a_phase;
			if((input & input_bomb) != 0) {
				callbacks.bomb_request(state);
			}
			return;
		}
		--state.stream_a_cooldown;
	}
	if(state.stream_b_phase < 2) {
		if(state.stream_b_cooldown == 0) {
			shot_patnums_set(state);
			if(state.shot_level == shot_level_max) {
				if((input & (input_down | input_down_left | input_down_right)) != 0) {
					state.spread_angle_delta = add_byte(state.spread_angle_delta, 5);
					if(state.spread_angle_delta > 26) {
						state.spread_angle_delta = 26;
					}
				} else if((input & (input_movement_aligned | input_movement_diagonal)) == 0) {
					state.spread_angle_delta = add_byte(state.spread_angle_delta, -5);
					if(state.spread_angle_delta < 0) {
						state.spread_angle_delta = 0;
					}
				}
			}
			callbacks.shot_volley(state, state.shot_patnum, state.shot_option_patnum);
			state.stream_b_cooldown = shot_cooldown;
			++state.stream_b_phase;
		} else {
			callbacks.shot_sound(state);
			--state.stream_b_cooldown;
		}
	}
	if((input & input_bomb) != 0) {
		callbacks.bomb_request(state);
	}
}

} // namespace th02::core
