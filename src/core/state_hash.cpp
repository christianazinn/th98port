#include "game_internal.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>

namespace th03::detail {
namespace {

std::uint32_t hash_u8(std::uint32_t hash, std::uint8_t value) noexcept
{
	return ((hash << 5U) + hash) ^ value;
}

std::uint32_t hash_u16(std::uint32_t hash, std::uint16_t value) noexcept
{
	hash = hash_u8(hash, static_cast<std::uint8_t>(value));
	return hash_u8(hash, static_cast<std::uint8_t>(value >> 8U));
}

std::uint32_t hash_u32(std::uint32_t hash, std::uint32_t value) noexcept
{
	hash = hash_u16(hash, static_cast<std::uint16_t>(value));
	return hash_u16(hash, static_cast<std::uint16_t>(value >> 16U));
}

template <typename Range>
std::uint32_t hash_bytes(std::uint32_t hash, const Range &bytes) noexcept
{
	for(const auto &value : bytes) {
		if constexpr(requires { value.begin(); value.end(); }) {
			hash = hash_bytes(hash, value);
		} else {
			hash = hash_u8(hash, static_cast<std::uint8_t>(value));
		}
	}
	return hash;
}

std::uint32_t hash_player(
	std::uint32_t hash,
	const PlayerState &player
) noexcept
{
	hash = hash_u16(hash, player.center_x.raw_bits());
	hash = hash_u16(hash, player.center_y.raw_bits());
	hash = hash_u8(hash, player.halfhearts);
	hash = hash_u8(hash, player.invincibility_time);
	hash = hash_u8(hash, player.shot_mode);
	hash = hash_u8(hash, player.knockback_time);
	hash = hash_u8(hash, player.move_lock_time);
	hash = hash_u16(hash, player.gauge_charged);
	hash = hash_u16(hash, player.gauge_avail);
	hash = hash_u8(hash, player.bombs);
	hash = hash_u8(hash, player.rounds_won);
	hash = hash_u16(hash, player.cpu_frame);
	hash = hash_u8(hash, player.hit_damage_next);
	hash = hash_u8(hash, player.shot_active);
	hash = hash_u8(hash, player.cpu_dodge_strategy);
	hash = hash_u16(hash, player.human_movement_last);
	hash = hash_u8(hash, player.spell_ready_frames);
	hash = hash_u16(hash, player.combo_bonus_max);
	hash = hash_u8(hash, player.combo_hits_max);
	hash = hash_u8(hash, player.gauge_attacks_fired);
	hash = hash_u8(hash, player.boss_attacks_fired);
	hash = hash_u8(hash, player.boss_attacks_reversed);
	return hash_u8(hash, player.boss_panics_fired);
}

std::uint32_t hash_combo(
	std::uint32_t hash,
	const ComboState &combo
) noexcept
{
	const auto display_hits = std::min<std::uint8_t>(combo.hits_highest, 99U);
	hash = hash_u8(hash, combo.time);
	hash = hash_u8(hash, display_hits);
	return hash_u16(hash, combo.bonus_total);
}

} // namespace

std::uint32_t state_hash(const DeterministicState &state) noexcept
{
	std::uint32_t hash = 5381U;
	hash = hash_u32(hash, state.round_frame);
	hash = hash_u16(hash, state.round_or_result_frame);
	hash = hash_u8(hash, state.round_speed);
	hash = hash_u8(hash, static_cast<std::uint8_t>(state.defeat_flag));
	hash = hash_u8(hash, std::bit_cast<std::uint8_t>(state.winner));
	hash = hash_u16(hash, state.input_player[0]);
	hash = hash_u16(hash, state.input_player[1]);
	hash = hash_u16(hash, state.input_system);
	hash = hash_bytes(hash, state.score[0]);
	hash = hash_bytes(hash, state.score[1]);
	hash = hash_player(hash, state.players[0]);
	hash = hash_player(hash, state.players[1]);
	hash = hash_u8(hash, state.randring_cursor);
	hash = hash_u8(hash, state.autofire_shot_gate[0]);
	hash = hash_u8(hash, state.autofire_shot_gate[1]);
	hash = hash_u8(
		hash, std::bit_cast<std::uint8_t>(state.cpu_target_kind_scratch)
	);
	hash = hash_u8(hash, state.boss_panic_fired_in_combo[0]);
	hash = hash_u8(hash, state.boss_panic_fired_in_combo[1]);
	hash = hash_u8(hash, static_cast<std::uint8_t>(state.background_phase));
	hash = hash_u8(hash, static_cast<std::uint8_t>(state.result_phase));
	hash = hash_combo(hash, state.combos[0]);
	hash = hash_combo(hash, state.combos[1]);
	hash = hash_bytes(hash, state.chains.cursor);
	hash = hash_bytes(hash, state.chains.hits);
	hash = hash_bytes(hash, state.chains.pellet_and_fireball_value);
	hash = hash_bytes(hash, state.chains.charge_fireball);
	hash = hash_bytes(hash, state.chains.charge_exatt);
	return hash_u32(hash, state.random_seed);
}

CanonicalState canonical_state(const DeterministicState &state) noexcept
{
	CanonicalState canonical{};
	canonical.route = state.route;
	canonical.game_mode = state.game_mode;
	canonical.story_stage = state.story_stage;
	canonical.round_id = state.round_id;
	canonical.winner = state.winner;
	canonical.round_speed = state.round_speed;
	canonical.global_frame = state.global_frame;
	canonical.round_frame = state.round_frame;
	canonical.round_or_result_frame = state.round_or_result_frame;
	canonical.p1_score = state.score[0];
	canonical.p2_score = state.score[1];
	canonical.resident_rand = state.resident_rand;
	canonical.state_hash = state_hash(state);
	return canonical;
}

} // namespace th03::detail
