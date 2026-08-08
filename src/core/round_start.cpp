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
constexpr std::uint8_t extends_max = 2U;
constexpr std::uint8_t extends_disabled = 255U;
constexpr std::uint16_t gauge_initial = 64U << 4U;

struct PlaycharSpeed {
	std::uint8_t aligned_x;
	std::uint8_t aligned_y;
	std::uint8_t diagonal_x;
	std::uint8_t diagonal_y;
	std::uint8_t gauge_charge;
};

constexpr std::array<PlaycharSpeed, 9> playchar_speeds{{
	{64U, 64U, 48U, 48U, 32U},
	{72U, 72U, 54U, 54U, 20U},
	{64U, 64U, 48U, 48U, 28U},
	{52U, 52U, 46U, 46U, 14U},
	{68U, 68U, 52U, 52U, 28U},
	{56U, 56U, 44U, 44U, 32U},
	{72U, 72U, 54U, 54U, 16U},
	{80U, 80U, 64U, 64U, 28U},
	{80U, 80U, 64U, 64U, 20U},
}};

constexpr std::array<std::array<std::uint16_t, 6>, 9>
	story_cpu_safety_frames{{
		{{100U, 50U, 20U, 0U, 0U, 0U}},
		{{200U, 100U, 50U, 20U, 0U, 300U}},
		{{200U, 100U, 50U, 10U, 0U, 400U}},
		{{300U, 200U, 100U, 50U, 10U, 500U}},
		{{400U, 250U, 150U, 70U, 20U, 600U}},
		{{500U, 300U, 200U, 100U, 50U, 1000U}},
		{{700U, 500U, 300U, 200U, 100U, 1500U}},
		{{800U, 600U, 400U, 300U, 200U, 1500U}},
		{{2000U, 1800U, 1300U, 1000U, 700U, 500U}},
	}};

std::uint16_t story_safety_frames(const GameInit &init) noexcept
{
	const auto signed_index =
		static_cast<int>(0U) - static_cast<int>(init.remaining_credits) + 3;
	const auto index = static_cast<std::size_t>(std::clamp(signed_index, 0, 5));
	auto frames = static_cast<std::uint32_t>(
		story_cpu_safety_frames[init.story_stage][index]
	);
	if(init.rank == rank_easy) {
		frames /= 3U;
	} else if(init.rank == rank_hard) {
		frames *= 2U;
	} else if(init.rank == rank_lunatic) {
		frames = (frames * 3U) + 500U;
	}
	return static_cast<std::uint16_t>(std::min(frames, 0xFFFFU));
}

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

std::uint8_t initial_bullet_base_speed(std::uint8_t rank) noexcept
{
	if(rank == rank_hard) {
		return 8U;
	}
	if(rank == rank_lunatic) {
		return 0x18U;
	}
	return 0U;
}

std::uint8_t initial_boss_level(const GameInit &init) noexcept
{
	std::uint16_t level = init.skill;
	if(init.rank == rank_easy) {
		level /= 2U;
	} else {
		if(init.rank == rank_hard) {
			level = static_cast<std::uint16_t>(level + 2U);
		} else if(init.rank == rank_lunatic) {
			level = static_cast<std::uint16_t>(level + 8U);
		}
	}
	return static_cast<std::uint8_t>(std::min<std::uint16_t>(level, 16U));
}

std::uint8_t initial_cpu_hit_damage_additional(const GameInit &init) noexcept
{
	if(init.game_mode != game_mode_story) {
		return 0U;
	}
	if(init.story_stage <= 1U) {
		return 3U;
	}
	if(init.story_stage <= 3U) {
		return 2U;
	}
	if(init.story_stage <= 5U) {
		return 1U;
	}
	return 0U;
}

} // namespace

DeterministicState initialize_first_round(const GameInit &init)
{
	DeterministicState state{};
	state.game_mode = init.game_mode;
	state.rank = init.rank;
	state.story_stage = init.story_stage;
	state.story_lives = init.story_lives;
	state.resident_rand = init.resident_rand;
	state.winner = init.winner;
	state.random_seed = static_cast<std::uint32_t>(init.resident_rand);
	state.round_speed = initial_round_speed(init.rank, state.round_id);
	state.bullet_base_speed = initial_bullet_base_speed(init.rank);
	state.gba_boss_level = initial_boss_level(init);
	state.cpu_hit_damage_additional = initial_cpu_hit_damage_additional(init);
	if(init.game_mode == game_mode_story) {
		state.gba_gauge_level.fill(
			static_cast<std::uint8_t>(init.story_stage + 1U)
		);
	} else {
		state.gba_gauge_level.fill(1U);
	}
	state.score = init.last_score;
	if(init.game_mode != game_mode_story) {
		state.score = {};
	} else {
		state.extends_gained = state.score[0][6];
		if(
			state.extends_gained >= extends_max ||
			state.score[0][7] != 0U
		) {
			state.extends_gained = extends_disabled;
		}
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
		const auto character = static_cast<std::size_t>(
			(static_cast<int>(player.paletted_character) - 1) / 2
		);
		if(character < playchar_speeds.size()) {
			const auto &speed = playchar_speeds[character];
			player.speed_aligned_x = speed.aligned_x;
			player.speed_aligned_y = speed.aligned_y;
			player.speed_diagonal_x = speed.diagonal_x;
			player.speed_diagonal_y = speed.diagonal_y;
			player.gauge_charge_speed = speed.gauge_charge;
		}
		auto &boss = state.boss_initial[player_id];
		boss.center_x = 144 << 4;
		boss.center_y = 80 << 4;
		boss.velocity_x = -32;
		boss.velocity_y = 0;
		boss.state = 0U;
		boss.motion_angle = 0U;
		boss.pattern_index = 0U;
		boss.render_state = 0U;
		if(character == 0U) {
			boss.hp = static_cast<std::uint16_t>(
				0x006EU + ((player_id != 0U) ? 0x28U : 0U)
			);
		} else if(character == 5U) {
			boss.hp = static_cast<std::uint16_t>(
				0x0064U + ((player_id != 0U) ? 0x28U : 0U)
			);
		}
	}
	// Each set_callbacks_*() call overwrites the shared background callback;
	// the second player's character therefore determines the round background.
	const auto last_character =
		(static_cast<int>(state.players[1].paletted_character) - 1) / 2;
	if(
		last_character != 0 && last_character != 1 && last_character != 6
	) {
		state.background_initial_kind = BackgroundInitialKind::type_b;
	}
	if(init.game_mode == game_mode_story && init.story_stage < 9U) {
		state.players[1].cpu_safety_frames = story_safety_frames(init);
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
