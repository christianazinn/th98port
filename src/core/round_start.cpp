#include "game_internal.hpp"

#include <algorithm>

namespace th03::detail {
namespace {

constexpr std::uint8_t game_mode_story = 1U;
constexpr std::uint8_t rank_easy = 0U;
constexpr std::uint8_t rank_normal = 1U;
constexpr std::uint8_t rank_hard = 2U;
constexpr std::uint8_t rank_lunatic = 3U;
constexpr std::uint8_t shot_mode_one_pair = 1U;
constexpr std::uint8_t shot_active_enabled = 1U;
constexpr std::uint16_t gauge_initial = 64U << 4U;

std::uint8_t initial_round_speed(std::uint8_t rank, std::uint8_t round_id) noexcept
{
	switch(rank) {
	case rank_easy:
		return static_cast<std::uint8_t>(round_id << 4U);
	case rank_normal:
		return static_cast<std::uint8_t>(round_id << 5U);
	case rank_hard:
		return static_cast<std::uint8_t>((round_id << 5U) + 0x20U);
	case rank_lunatic:
		return 0x60U;
	default:
		return 0U;
	}
}

} // namespace

DeterministicState initialize_first_round(const GameInit &init)
{
	DeterministicState state{};
	state.game_mode = init.game_mode;
	state.rank = init.rank;
	state.story_stage = init.story_stage;
	state.resident_rand = init.resident_rand;
	state.winner = init.winner;
	state.random_seed = static_cast<std::uint32_t>(init.resident_rand);
	state.round_speed = initial_round_speed(init.rank, state.round_id);
	state.score = init.last_score;
	if(init.game_mode != game_mode_story) {
		state.score = {};
	}

	for(std::size_t player_id = 0; player_id < player_count; ++player_id) {
		auto &player = state.players[player_id];
		player.center_x = exact::Subpixel::from_integer_wrapping(144);
		player.center_y = exact::Subpixel::from_integer_wrapping(336);
		player.halfhearts = 10U;
		player.invincibility_time = 50U;
		player.shot_mode = shot_mode_one_pair;
		player.gauge_avail = gauge_initial;
		player.bombs = 2U;
		player.hit_damage_next = 1U;
		player.shot_active = shot_active_enabled;
		player.is_cpu = init.is_cpu[player_id];
		player.paletted_character = init.paletted_character[player_id];
	}

	if(init.runtime_snapshot) {
		const auto &snapshot = *init.runtime_snapshot;
		state.random_seed = snapshot.random_seed;
		state.randring = snapshot.randring;
		state.randring_cursor = snapshot.randring_cursor;
		state.formation_type_ring = snapshot.formation_type_ring;
		state.formation_position_ring = snapshot.formation_position_ring;
		state.formation_cursor = snapshot.formation_cursor;
		for(std::size_t player_id = 0; player_id < player_count; ++player_id) {
			auto &player = state.players[player_id];
			const auto &restored = snapshot.players[player_id];
			player.cpu_charge_ring = snapshot.cpu_charge_ring[player_id];
			player.cpu_charge_cursor = snapshot.cpu_charge_cursor[player_id];
			player.center_x = restored.center_x;
			player.center_y = restored.center_y;
			player.halfhearts = restored.halfhearts;
			player.invincibility_time = restored.invincibility_time;
			player.gauge_charge_speed = restored.gauge_charge_speed;
			player.gauge_charged = restored.gauge_charged;
			player.gauge_avail = restored.gauge_avail;
			player.bombs = restored.bombs;
			player.shot_active = restored.shot_active;
			player.cpu_frame = restored.cpu_frame;
		}
	}
	return state;
}

} // namespace th03::detail
