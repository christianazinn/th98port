#include "rpy_checkpoint.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "bytes.hpp"

namespace th03::formats::detail {
namespace {

Error error(
	ErrorCode code,
	std::string_view format_name,
	std::string message,
	std::size_t offset = 0U
)
{
	return Error{
		code,
		std::string(format_name) + " " + std::move(message),
		offset,
	};
}

PlayerStart parse_player_start(
	std::span<const std::byte> bytes,
	std::size_t offset,
	std::size_t player
)
{
	PlayerStart result{};
	result.center_x = exact::Subpixel::from_raw_bits(
		u16(bytes, offset + (player * 2U))
	);
	result.center_y = exact::Subpixel::from_raw_bits(
		u16(bytes, offset + 4U + (player * 2U))
	);
	result.halfhearts = u8(bytes, offset + 8U + player);
	result.invincibility_time = u8(bytes, offset + 10U + player);
	result.gauge_charge_speed = u8(bytes, offset + 12U + player);
	result.gauge_charged = u16(bytes, offset + 14U + (player * 2U));
	result.gauge_avail = u16(bytes, offset + 18U + (player * 2U));
	result.bombs = u8(bytes, offset + 22U + player);
	result.shot_active = u8(bytes, offset + 24U + player);
	result.cpu_frame = u16(bytes, offset + 26U + (player * 2U));
	return result;
}

GameInit parse_compact_snapshot(
	std::span<const std::byte> bytes,
	std::size_t offset
)
{
	GameInit init{};
	const auto resident_bits = u32(bytes, offset + 0U);
	init.resident_rand = std::bit_cast<std::int32_t>(resident_bits);
	init.random_seed = u32(bytes, offset + 4U);
	init.rank = u8(bytes, offset + 8U);
	init.key_mode = u8(bytes, offset + 9U);
	init.game_mode = u8(bytes, offset + 10U);
	init.story_stage = u8(bytes, offset + 11U);
	init.story_lives = u8(bytes, offset + 12U);
	init.remaining_credits = u8(bytes, offset + 13U);
	init.skill = u8(bytes, offset + 14U);
	init.demo_number = u8(bytes, offset + 15U);
	init.winner = i8(bytes, offset + 16U);
	init.show_score_menu = u8(bytes, offset + 17U);
	init.fast_op_animation = u8(bytes, offset + 18U);
	for(std::size_t player = 0U; player < 2U; ++player) {
		init.is_cpu[player] = u8(bytes, offset + 19U + player);
		init.paletted_character[player] = i8(bytes, offset + 21U + player);
		for(std::size_t digit = 0U; digit < 8U; ++digit) {
			init.last_score[player][digit] = u8(
				bytes, offset + 32U + (player * 8U) + digit
			);
		}
	}
	for(std::size_t stage = 0U; stage < 9U; ++stage) {
		init.story_opponents[stage] = i8(bytes, offset + 23U + stage);
	}
	init.autofire_mask = u8(bytes, offset + 48U);

	CompactRuntimeSnapshot runtime{};
	// V13 records the actual pre-reset seed. Historical V12 compatibility
	// normalization belongs in its one-way converter, not this frozen reader.
	runtime.round_reset_seed = u32(bytes, offset + 79U);
	runtime.random_seed = init.random_seed;
	runtime.first_formation = u8(bytes, offset + 83U);
	for(std::size_t player = 0U; player < 2U; ++player) {
		runtime.players[player] = parse_player_start(bytes, offset + 49U, player);
	}
	init.compact_runtime_snapshot = runtime;
	return init;
}

RoundStartSnapshot parse_round_start(
	std::span<const std::byte> bytes,
	std::size_t state_offset,
	std::size_t carry_offset
)
{
	RoundStartSnapshot snapshot{};
	auto &state = snapshot.state;
	auto &carry = snapshot.carry;
	state.round_id = u8(bytes, state_offset + 0U);
	state.rounds_won = {
		u8(bytes, state_offset + 1U),
		u8(bytes, state_offset + 2U),
	};
	for(std::size_t player = 0U; player < 2U; ++player) {
		for(std::size_t digit = 0U; digit < 8U; ++digit) {
			state.score[player][digit] = u8(
				bytes, state_offset + 3U + (player * 8U) + digit
			);
		}
	}
	state.round_speed = u8(bytes, state_offset + 19U);
	state.bullet_base_speed = u8(bytes, state_offset + 20U);
	state.gauge_attack_level = {
		u8(bytes, state_offset + 21U),
		u8(bytes, state_offset + 22U),
	};
	state.boss_attack_level = u8(bytes, state_offset + 23U);
	state.cpu_hit_damage_additional = u8(bytes, state_offset + 24U);
	state.extends_gained = u8(bytes, state_offset + 25U);
	state.cpu_safety_frames = {
		u16(bytes, state_offset + 26U),
		u16(bytes, state_offset + 28U),
	};

	for(std::size_t player = 0U; player < 2U; ++player) {
		carry.cpu_dodge_strategy[player] = u8(bytes, carry_offset + player);
		carry.human_movement_last[player] = u16(
			bytes, carry_offset + 2U + (player * 2U)
		);
		carry.spell_ready_frames[player] = u8(
			bytes, carry_offset + 6U + player
		);
		carry.combo_bonus_max[player] = u16(
			bytes, carry_offset + 8U + (player * 2U)
		);
		carry.combo_hits_max[player] = u8(
			bytes, carry_offset + 12U + player
		);
		carry.shot_cycle[player] = u8(bytes, carry_offset + 14U + player);
		carry.boss_panic_fired[player] = u8(
			bytes, carry_offset + 16U + player
		);
		carry.gauge_attack_next[player] = u8(
			bytes, carry_offset + 18U + player
		);
		carry.combo[player].time = u8(bytes, carry_offset + 22U + player);
		carry.combo[player].hits_highest = u8(
			bytes, carry_offset + 24U + player
		);
		carry.combo[player].bonus_total = u16(
			bytes, carry_offset + 26U + (player * 2U)
		);
		carry.chain_cursor[player] = u8(bytes, carry_offset + 30U + player);
		for(std::size_t index = 0U; index < 16U; ++index) {
			carry.chain_hits[player][index] = u8(
				bytes, carry_offset + 32U + (player * 16U) + index
			);
			carry.chain_pellet_and_fireball_value[player][index] = u8(
				bytes, carry_offset + 64U + (player * 16U) + index
			);
			carry.chain_charge_fireball[player][index] = u8(
				bytes, carry_offset + 96U + (player * 16U) + index
			);
			carry.chain_charge_exatt[player][index] = u8(
				bytes, carry_offset + 128U + (player * 16U) + index
			);
		}
	}
	carry.cpu_shot_decision = i8(bytes, carry_offset + 20U);
	carry.background_phase = u8(bytes, carry_offset + 21U);
	carry.randring_cursor = u8(bytes, carry_offset + 160U);
	carry.result_phase = u8(bytes, carry_offset + 161U);
	carry.fireball_generation_previous = u8(bytes, carry_offset + 162U);
	return snapshot;
}

} // namespace

bool range_fits(std::size_t offset, std::size_t size, std::size_t total) noexcept
{
	return offset <= total && size <= (total - offset);
}

std::expected<CompactCheckpoint, Error> parse_compact_checkpoint(
	std::span<const std::byte> bytes,
	std::size_t offset,
	std::string_view format_name
)
{
	if(!range_fits(offset, compact_checkpoint_size, bytes.size())) {
		return std::unexpected(error(
			ErrorCode::truncated,
			format_name,
			"checkpoint is truncated",
			offset
		));
	}
	CompactCheckpoint result{};
	result.sample_count = u32(bytes, offset + 0U);
	result.global_frame = u32(bytes, offset + 4U);
	result.input_size = u32(bytes, offset + 8U);
	const auto snapshot_offset = offset + compact_checkpoint_prefix_size;
	const auto state_offset = snapshot_offset + compact_snapshot_size;
	const auto carry_offset = state_offset + compact_round_state_size;
	result.init = parse_compact_snapshot(bytes, snapshot_offset);
	result.init.global_frame = result.global_frame;
	result.init.round_start_snapshot = parse_round_start(
		bytes, state_offset, carry_offset
	);
	const auto &carry = result.init.round_start_snapshot->carry;
	if(carry.background_phase > 2U) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			format_name,
			"checkpoint background phase is outside 0..2",
			carry_offset + 21U
		));
	}
	if(carry.result_phase > 2U) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			format_name,
			"checkpoint result phase is outside 0..2",
			carry_offset + 161U
		));
	}
	for(std::size_t player = 0U; player < 2U; ++player) {
		if(carry.chain_cursor[player] >= 16U) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				format_name,
				"checkpoint chain cursor is outside 0..15",
				carry_offset + 30U + player
			));
		}
		if(carry.gauge_attack_next[player] > 5U) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				format_name,
				"checkpoint pending gauge attack is outside 0..5",
				carry_offset + 18U + player
			));
		}
		if(carry.boss_panic_fired[player] > 1U) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				format_name,
				"checkpoint boss-panic latch is not Boolean",
				carry_offset + 16U + player
			));
		}
	}
	return result;
}

} // namespace th03::formats::detail
