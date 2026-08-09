#include "game_internal.hpp"

#include <algorithm>

namespace th03::detail {
namespace {

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
		{{200U, 100U, 50U, 20U, 0U, 0U}},
		{{300U, 200U, 100U, 50U, 10U, 0U}},
		{{400U, 300U, 200U, 100U, 50U, 10U}},
		{{500U, 400U, 250U, 150U, 70U, 20U}},
		{{600U, 500U, 300U, 200U, 100U, 50U}},
		{{1000U, 700U, 500U, 300U, 200U, 100U}},
		{{1500U, 800U, 600U, 300U, 200U, 100U}},
		{{0xFFFAU, 1500U, 1000U, 800U, 700U, 500U}},
	}};

std::uint16_t story_safety_frames(
	const GameInit &init,
	std::uint8_t round_id
) noexcept
{
	const auto signed_index =
		static_cast<int>(round_id) -
		static_cast<int>(init.remaining_credits) + 3;
	const auto index = static_cast<std::size_t>(std::min(signed_index, 5));
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

std::uint16_t versus_safety_frames(const GameInit &init) noexcept
{
	if(init.is_cpu[0] != 0U && init.is_cpu[1] != 0U) {
		return 0xFFFFU;
	}
	switch(init.rank) {
	case rank_easy:
		return 1000U;
	case rank_normal:
		return 1900U;
	case rank_hard:
		return 4000U;
	case rank_lunatic:
		return 0xFFFFU;
	default:
		return 0U;
	}
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

std::uint8_t initial_boss_level(
	const GameInit &init,
	std::uint8_t round_id
) noexcept
{
	auto level = init.story_stage;
	if(init.rank == rank_easy) {
		level = static_cast<std::uint8_t>(level / 2U);
		level = exact::wrap_add(level, round_id);
	} else {
		level = exact::wrap_add(level, round_id);
		level = exact::wrap_add(level, round_id);
		if(init.rank == rank_hard) {
			level = exact::wrap_add(level, std::uint8_t{2U});
		} else if(init.rank == rank_lunatic) {
			level = exact::wrap_add(level, std::uint8_t{8U});
		}
	}
	return std::min<std::uint8_t>(level, 16U);
}

void initialize_runtime_rings(
	DeterministicState &state,
	std::size_t formation_count
) noexcept
{
	randring_fill(state);
	for(auto &player : state.players) {
		for(auto &charge : player.cpu_charge_ring) {
			charge = static_cast<std::uint8_t>(
				((irand(state.random_seed) & 3U) << 6U) + 0x3FU
			);
		}
		player.cpu_charge_cursor = 0U;
	}
	if(formation_count == 0U) {
		return;
	}

	auto previous = static_cast<std::uint8_t>(formation_count);
	for(std::size_t index = 0U; index < state.formation_type_ring.size(); ++index) {
		std::uint8_t next = 0U;
		do {
			next = static_cast<std::uint8_t>(
				irand(state.random_seed) % formation_count
			);
		} while(next == previous);
		state.formation_type_ring[index] = next;
		previous = next;
		state.formation_position_ring[index] = static_cast<std::uint8_t>(
			(irand(state.random_seed) & 1U) << 7U
		);
	}
	state.formation_cursor = {};
	state.enemies_alive = {};
}

std::uint16_t practice_story_safety_frames(
	std::uint8_t rank,
	std::uint8_t stage,
	std::uint8_t round
) noexcept
{
	round = std::min<std::uint8_t>(round, 5U);
	auto frames = static_cast<std::uint32_t>(
		story_cpu_safety_frames[stage][round]
	);
	if(rank == rank_easy) {
		frames /= 3U;
	} else if(rank == rank_hard) {
		frames *= 2U;
	} else if(rank == rank_lunatic) {
		frames = (frames * 3U) + 500U;
	}
	return static_cast<std::uint16_t>(std::min(frames, 0xFFFFU));
}

bool reconstruct_compact_runtime_rings(
	DeterministicState &state,
	const CompactRuntimeSnapshot &snapshot,
	std::size_t formation_count
) noexcept
{
	if(
		formation_count == 0U || formation_count > 255U ||
		snapshot.first_formation >= formation_count
	) {
		return false;
	}
	state.random_seed = snapshot.round_reset_seed;
	randring_fill(state);
	for(auto &player : state.players) {
		for(auto &charge : player.cpu_charge_ring) {
			charge = static_cast<std::uint8_t>(
				((irand(state.random_seed) & 3U) << 6U) + 0x3FU
			);
		}
		player.cpu_charge_cursor = 0U;
	}

	std::uint8_t previous = 0xFFU;
	for(std::size_t index = 0U; index < state.formation_type_ring.size(); ++index) {
		std::uint8_t next = 0U;
		if(index == 0U) {
			next = static_cast<std::uint8_t>(
				irand(state.random_seed) % formation_count
			);
			if(next != snapshot.first_formation) {
				previous = next;
				do {
					next = static_cast<std::uint8_t>(
						irand(state.random_seed) % formation_count
					);
				} while(next == previous);
			}
			if(next != snapshot.first_formation) {
				return false;
			}
		} else {
			do {
				next = static_cast<std::uint8_t>(
					irand(state.random_seed) % formation_count
				);
			} while(next == previous);
		}
		state.formation_type_ring[index] = next;
		previous = next;
		state.formation_position_ring[index] = static_cast<std::uint8_t>(
			(irand(state.random_seed) & 1U) << 7U
		);
	}
	state.formation_cursor = {};
	state.enemies_alive = {};
	state.random_seed = snapshot.random_seed;
	return true;
}

void restore_player_starts(
	DeterministicState &state,
	const std::array<PlayerStart, player_count> &starts
) noexcept
{
	for(std::size_t player_id = 0U; player_id < player_count; ++player_id) {
		auto &player = state.players[player_id];
		const auto &restored = starts[player_id];
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

void initialize_repeat_rank(
	DeterministicState &state,
	const GameInit &init
) noexcept
{
	state.round_speed = initial_round_speed(state.rank, state.round_id);
	state.bullet_base_speed = initial_bullet_base_speed(state.rank);
	state.gba_boss_level = initial_boss_level(init, state.round_id);
	if(state.game_mode != game_mode_story) {
		std::uint8_t increment = 0U;
		switch(state.rank) {
		case rank_easy:
			increment = 1U;
			break;
		case rank_normal:
		case rank_hard:
			increment = 2U;
			break;
		case rank_lunatic:
			increment = 4U;
			break;
		default:
			break;
		}
		for(auto &level : state.gba_gauge_level) {
			level = exact::wrap_add(level, increment);
		}
	}
	state.round_speed = std::min<std::uint8_t>(state.round_speed, 0x7FU);
	state.gba_boss_level = std::min<std::uint8_t>(
		state.gba_boss_level, 16U
	);
	for(auto &level : state.gba_gauge_level) {
		level = std::min<std::uint8_t>(level, 16U);
	}
}

void initialize_background_rng(DeterministicState &state) noexcept
{
	if(state.background_initial_kind == BackgroundInitialKind::type_a) {
		// sub_D092(): 48 particles, four IRAND calls apiece.
		for(std::size_t i = 0U; i < 48U * 4U; ++i) {
			(void)irand(state.random_seed);
		}
	} else {
		// sub_D2E8(): 50 particles, two IRAND calls apiece.
		for(std::size_t i = 0U; i < 50U * 2U; ++i) {
			(void)irand(state.random_seed);
		}
	}
}

void apply_round_start_snapshot(
	DeterministicState &state,
	const RoundStartSnapshot &snapshot
) noexcept
{
	const auto &round = snapshot.state;
	const auto &carry = snapshot.carry;
	state.round_id = round.round_id;
	state.score = round.score;
	state.round_speed = round.round_speed;
	state.bullet_base_speed = round.bullet_base_speed;
	state.gba_gauge_level = round.gauge_attack_level;
	state.gba_boss_level = round.boss_attack_level;
	state.cpu_hit_damage_additional = round.cpu_hit_damage_additional;
	state.extends_gained = round.extends_gained;
	for(std::size_t player_id = 0U; player_id < player_count; ++player_id) {
		auto &player = state.players[player_id];
		player.rounds_won = round.rounds_won[player_id];
		player.cpu_safety_frames = round.cpu_safety_frames[player_id];
		player.cpu_dodge_strategy = carry.cpu_dodge_strategy[player_id];
		player.human_movement_last = carry.human_movement_last[player_id];
		player.spell_ready_frames = carry.spell_ready_frames[player_id];
		player.combo_bonus_max = carry.combo_bonus_max[player_id];
		player.combo_hits_max = carry.combo_hits_max[player_id];
		state.autofire_shot_gate[player_id] = carry.shot_cycle[player_id];
		state.boss_panic_fired_in_combo[player_id] =
			carry.boss_panic_fired[player_id];
		state.gba_flag_next[player_id] = static_cast<GaugeBossAttackFlag>(
			carry.gauge_attack_next[player_id]
		);
		state.combos[player_id] = ComboState{
			carry.combo[player_id].time,
			carry.combo[player_id].hits_highest,
			carry.combo[player_id].bonus_total,
		};
	}
	state.cpu_target_kind_scratch = carry.cpu_shot_decision;
	state.background_phase = static_cast<BackgroundPhase>(carry.background_phase);
	state.chains.cursor = carry.chain_cursor;
	state.chains.hits = carry.chain_hits;
	state.chains.pellet_and_fireball_value =
		carry.chain_pellet_and_fireball_value;
	state.chains.charge_fireball = carry.chain_charge_fireball;
	state.chains.charge_exatt = carry.chain_charge_exatt;
	state.randring_cursor = carry.randring_cursor;
	state.result_phase = static_cast<ResultPhase>(carry.result_phase);
	state.fireball_generation_previous = carry.fireball_generation_previous;
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

bool compact_runtime_snapshot_is_valid(
	const CompactRuntimeSnapshot &snapshot,
	std::size_t formation_count
) noexcept
{
	DeterministicState state{};
	return reconstruct_compact_runtime_rings(state, snapshot, formation_count);
}

std::uint16_t practice_cpu_safety_frames(
	const GameInit &init,
	std::uint8_t round_id
) noexcept
{
	const auto policy = init.practice_rules->retry_cpu_timer;
	if(policy == CpuTimerPolicy::story_native) {
		return practice_story_safety_frames(
			init.rank, init.story_stage, round_id
		);
	}
	if(policy == CpuTimerPolicy::infinite) {
		return 0xFFFFU;
	}
	return versus_safety_frames(init);
}

DeterministicState initialize_first_round(
	const GameInit &init,
	std::size_t formation_count
)
{
	DeterministicState state{};
	state.global_frame = init.global_frame;
	state.game_mode = init.game_mode;
	state.rank = init.rank;
	state.story_stage = init.story_stage;
	state.story_lives = init.story_lives;
	state.skill = init.skill;
	state.practice_uses_stock =
		init.practice_rules && init.practice_rules->uses_stock;
	state.demo_number = init.demo_number;
	state.resident_rand = init.resident_rand;
	state.winner = init.winner;
	state.random_seed = static_cast<std::uint32_t>(init.resident_rand);
	state.round_speed = initial_round_speed(init.rank, state.round_id);
	state.bullet_base_speed = initial_bullet_base_speed(init.rank);
	state.gba_boss_level = initial_boss_level(init, state.round_id);
	state.cpu_hit_damage_additional = initial_cpu_hit_damage_additional(init);
	if(init.game_mode == game_mode_story) {
		state.gba_gauge_level.fill(
			static_cast<std::uint8_t>(init.story_stage + 1U)
		);
	} else {
		state.gba_gauge_level.fill(1U);
	}
	state.score = {};
	if(init.game_mode == game_mode_story) {
		state.score[0] = init.last_score[0];
		state.extends_gained = state.score[0][6];
		if(
			state.extends_gained >= extends_max ||
			state.score[0][7] != 0U
		) {
			state.extends_gained = extends_disabled;
		}
	}
	if(init.demo_number != 0U) {
		state.demo_frame = 0U;
		state.round_speed = 4U << 4U;
		state.bullet_base_speed = 0U;
		state.gba_boss_level = 8U;
		state.gba_gauge_level.fill(9U);
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
		if(
			character == 0U || character == 1U || character == 2U ||
			character == 4U || character == 6U
		) {
			boss.hp = 0x006EU;
		} else if(character == 3U || character == 5U) {
			boss.hp = 0x0064U;
		} else if(character == 7U) {
			boss.hp = 0x008CU;
		} else if(character == 8U) {
			boss.hp = 0x0082U;
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
		state.players[1].cpu_safety_frames = story_safety_frames(
			init, state.round_id
		);
	} else if(init.game_mode != game_mode_story) {
		const auto safety_frames = versus_safety_frames(init);
		for(auto &player : state.players) {
			player.cpu_safety_frames = safety_frames;
		}
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
			player.cpu_charge_ring = snapshot.cpu_charge_ring[player_id];
			player.cpu_charge_cursor = snapshot.cpu_charge_cursor[player_id];
		}
		restore_player_starts(state, snapshot.players);
	} else if(init.compact_runtime_snapshot) {
		(void)reconstruct_compact_runtime_rings(
			state, *init.compact_runtime_snapshot, formation_count
		);
		restore_player_starts(state, init.compact_runtime_snapshot->players);
	} else {
		initialize_runtime_rings(state, formation_count);
		initialize_background_rng(state);
	}
	if(init.round_start_snapshot) {
		apply_round_start_snapshot(state, *init.round_start_snapshot);
	}
	return state;
}

void initialize_repeat_round(
	DeterministicState &state,
	const GameInit &init,
	std::size_t formation_count
) noexcept
{
	// main_entry() persists the outgoing round-frame count before sub_A21F()
	// increments round_id and performs this targeted, in-process reset.
	state.resident_rand = static_cast<std::int32_t>(state.round_frame);
	state.round_id = static_cast<std::uint8_t>(state.round_id + 1U);
	const auto practice_spell_levels = state.gba_gauge_level;

	state.boss_target_active = {};
	state.result_phase_frame = 0U;
	state.result_transition_complete = false;
	state.enemy_speed = 0U;
	state.round_frame = 0U;
	state.round_or_result_frame = 0U;
	state.route = 0U;
	state.round_complete = false;
	state.session_exit_requested = false;

	for(auto &shotpair : state.shotpairs) {
		shotpair.alive = false;
	}
	for(auto &enemy : state.enemies) {
		enemy.flag = EnemyFlag::free;
	}
	for(auto &fireball : state.fireballs) {
		fireball.flag = FireballFlag::free;
	}

	// TH03's private randring_fill() deliberately carries randring_cursor.
	randring_fill(state);
	for(auto &bullet : state.bullets) {
		bullet.flag = BulletFlag::free;
	}
	state.bullet_trail_cursor = 0U;

	for(auto &player_attacks : state.extra_attacks) {
		for(auto &attack : player_attacks) {
			attack.state = 0U;
		}
	}
	for(auto &shotpair : state.shotpairs) {
		shotpair.alive = false;
	}

	// These are the five exact targeted Charge/Extra reset helpers called by
	// sub_9EBF(), in source order. Other character-owned storage carries.
	for(auto &player_groups : state.reimu_charge) {
		for(auto &group : player_groups) {
			group.state = 0U;
		}
	}
	for(auto &player_nodes : state.mima_charge) {
		for(auto &node : player_nodes) {
			node.active = 0U;
		}
	}
	for(auto &charge : state.marisa_charge) {
		charge.active = 0U;
	}
	for(auto &player_nodes : state.ellen_charge) {
		for(auto &node : player_nodes) {
			node.state = 0U;
		}
	}

	if(state.game_mode == game_mode_story && state.story_stage < 9U) {
		state.players[1].cpu_safety_frames = story_safety_frames(
			init, state.round_id
		);
	}
	initialize_repeat_rank(state, init);
	if(init.practice_rules && init.practice_rules->uses_stock) {
		const auto safety = practice_cpu_safety_frames(init, state.round_id);
		for(auto &player : state.players) {
			player.cpu_safety_frames = safety;
		}
		state.gba_gauge_level = practice_spell_levels;
		state.cpu_hit_damage_additional =
			init.practice_rules->retry_cpu_damage;
	}

	state.gba_boss_launched_by = -1;
	state.defeat_flag = DefeatFlag::none;
	state.hud_start_flag = 1U;
	state.combo_points_for_boss_attack = 5120U;

	for(std::size_t player_id = 0U; player_id < player_count; ++player_id) {
		auto &player = state.players[player_id];
		state.enemies_alive[player_id] = 0U;
		player.paletted_character = init.paletted_character[player_id];
		player.is_cpu = init.is_cpu[player_id];
		player.is_hit = false;
		player.invincibility_time = 50U;
		player.shot_mode = shot_mode_one_pair;
		player.halfhearts = 10U;
		player.knockback_time = 0U;
		player.move_lock_time = 0U;
		player.knockback_active = false;
		player.center_x = exact::Subpixel::from_integer_wrapping(144);
		player.center_y = exact::Subpixel::from_integer_wrapping(336);
		player.gauge_charged = 0U;
		player.shot_active = shot_active_enabled;
		player.hit_damage_next = 1U;
		for(auto &charge : player.cpu_charge_ring) {
			charge = static_cast<std::uint8_t>(
				((irand(state.random_seed) & 3U) << 6U) + 0x3FU
			);
		}
		player.cpu_charge_cursor = 0U;
		player.bombs = 2U;
		player.lose_anim_time = 0U;
		player.hyper_active = 0U;
		player.hyper_character_callback = false;
		player.hyper_effect_active_this_frame = false;
		player.cpu_frame = 0U;
		player.gauge_attacks_fired = 0U;
		player.boss_attacks_fired = 0U;
		player.boss_attacks_reversed = 0U;
		player.boss_panics_fired = 0U;
		state.gba_flag_active[player_id] = GaugeBossAttackFlag::none;
		state.playfield_fg_shift_x[player_id] = 0;
		state.damage_all_on[player_id] = 0U;
		state.warning_flag[player_id] = WarningFlag::none;
	}

	if(formation_count != 0U) {
		auto previous = std::uint8_t{1U};
		for(std::size_t index = 0U;
			index < state.formation_type_ring.size(); ++index) {
			std::uint8_t next = 0U;
			do {
				next = static_cast<std::uint8_t>(
					irand(state.random_seed) % formation_count
				);
			} while(next == previous);
			state.formation_type_ring[index] = next;
			previous = next;
			state.formation_position_ring[index] = static_cast<std::uint8_t>(
				(irand(state.random_seed) & 1U) << 7U
			);
		}
	}
	state.formation_cursor = {};

	// sub_A21F() carries the opening transition's closing callback into the
	// repeated round. It has no gameplay side effects but remains explicit.
	state.result_phase = ResultPhase::closing;
}

} // namespace th03::detail
