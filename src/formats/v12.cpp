#include "th03/formats/v12.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "bytes.hpp"
#include "rpy_input.hpp"

namespace th03::formats {
namespace {

constexpr std::size_t v12_header_size = 416U;
constexpr std::size_t v12_checkpoint_size = 289U;
constexpr std::size_t checkpoint_prefix_size = 12U;
constexpr std::size_t compact_snapshot_size = 84U;
constexpr std::size_t round_state_size = 30U;
constexpr std::size_t round_carry_size = 163U;

Error error(ErrorCode code, std::string message, std::size_t offset = 0U)
{
	return Error{code, std::move(message), offset};
}

bool range_fits(std::size_t offset, std::size_t size, std::size_t total)
{
	return offset <= total && size <= (total - offset);
}

PlayerStart parse_player_start(
	std::span<const std::byte> bytes,
	std::size_t offset,
	std::size_t player
)
{
	PlayerStart result{};
	result.center_x = exact::Subpixel::from_raw_bits(
		detail::u16(bytes, offset + (player * 2U))
	);
	result.center_y = exact::Subpixel::from_raw_bits(
		detail::u16(bytes, offset + 4U + (player * 2U))
	);
	result.halfhearts = detail::u8(bytes, offset + 8U + player);
	result.invincibility_time = detail::u8(bytes, offset + 10U + player);
	result.gauge_charge_speed = detail::u8(bytes, offset + 12U + player);
	result.gauge_charged = detail::u16(bytes, offset + 14U + (player * 2U));
	result.gauge_avail = detail::u16(bytes, offset + 18U + (player * 2U));
	result.bombs = detail::u8(bytes, offset + 22U + player);
	result.shot_active = detail::u8(bytes, offset + 24U + player);
	result.cpu_frame = detail::u16(bytes, offset + 26U + (player * 2U));
	return result;
}

GameInit parse_compact_snapshot(
	std::span<const std::byte> bytes,
	std::size_t offset,
	std::uint8_t directory_stage_round
)
{
	GameInit init{};
	const auto resident_bits = detail::u32(bytes, offset + 0U);
	init.resident_rand = std::bit_cast<std::int32_t>(resident_bits);
	init.random_seed = detail::u32(bytes, offset + 4U);
	init.rank = detail::u8(bytes, offset + 8U);
	init.key_mode = detail::u8(bytes, offset + 9U);
	init.game_mode = detail::u8(bytes, offset + 10U);
	init.story_stage = detail::u8(bytes, offset + 11U);
	init.story_lives = detail::u8(bytes, offset + 12U);
	init.remaining_credits = detail::u8(bytes, offset + 13U);
	init.skill = detail::u8(bytes, offset + 14U);
	init.demo_number = detail::u8(bytes, offset + 15U);
	init.winner = detail::i8(bytes, offset + 16U);
	init.show_score_menu = detail::u8(bytes, offset + 17U);
	init.fast_op_animation = detail::u8(bytes, offset + 18U);
	for(std::size_t player = 0U; player < 2U; ++player) {
		init.is_cpu[player] = detail::u8(bytes, offset + 19U + player);
		init.paletted_character[player] = detail::i8(
			bytes, offset + 21U + player
		);
		for(std::size_t digit = 0U; digit < 8U; ++digit) {
			init.last_score[player][digit] = detail::u8(
				bytes, offset + 32U + (player * 8U) + digit
			);
		}
	}
	for(std::size_t stage = 0U; stage < 9U; ++stage) {
		init.story_opponents[stage] = detail::i8(bytes, offset + 23U + stage);
	}
	init.autofire_mask = detail::u8(bytes, offset + 48U);

	CompactRuntimeSnapshot runtime{};
	// Early V12 recorders overwrote a round-0 seed field. The Replay Patch's
	// compatibility reader resolves that process anchor from resident_rand.
	runtime.round_reset_seed = ((directory_stage_round >> 4U) == 0U)
		? resident_bits : detail::u32(bytes, offset + 79U);
	runtime.random_seed = init.random_seed;
	runtime.first_formation = detail::u8(bytes, offset + 83U);
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
	state.round_id = detail::u8(bytes, state_offset + 0U);
	state.rounds_won = {
		detail::u8(bytes, state_offset + 1U),
		detail::u8(bytes, state_offset + 2U),
	};
	for(std::size_t player = 0U; player < 2U; ++player) {
		for(std::size_t digit = 0U; digit < 8U; ++digit) {
			state.score[player][digit] = detail::u8(
				bytes, state_offset + 3U + (player * 8U) + digit
			);
		}
	}
	state.round_speed = detail::u8(bytes, state_offset + 19U);
	state.bullet_base_speed = detail::u8(bytes, state_offset + 20U);
	state.gauge_attack_level = {
		detail::u8(bytes, state_offset + 21U),
		detail::u8(bytes, state_offset + 22U),
	};
	state.boss_attack_level = detail::u8(bytes, state_offset + 23U);
	state.cpu_hit_damage_additional = detail::u8(bytes, state_offset + 24U);
	state.extends_gained = detail::u8(bytes, state_offset + 25U);
	state.cpu_safety_frames = {
		detail::u16(bytes, state_offset + 26U),
		detail::u16(bytes, state_offset + 28U),
	};

	for(std::size_t player = 0U; player < 2U; ++player) {
		carry.cpu_dodge_strategy[player] = detail::u8(
			bytes, carry_offset + player
		);
		carry.human_movement_last[player] = detail::u16(
			bytes, carry_offset + 2U + (player * 2U)
		);
		carry.spell_ready_frames[player] = detail::u8(
			bytes, carry_offset + 6U + player
		);
		carry.combo_bonus_max[player] = detail::u16(
			bytes, carry_offset + 8U + (player * 2U)
		);
		carry.combo_hits_max[player] = detail::u8(
			bytes, carry_offset + 12U + player
		);
		carry.shot_cycle[player] = detail::u8(
			bytes, carry_offset + 14U + player
		);
		carry.boss_panic_fired[player] = detail::u8(
			bytes, carry_offset + 16U + player
		);
		carry.gauge_attack_next[player] = detail::u8(
			bytes, carry_offset + 18U + player
		);
		carry.combo[player].time = detail::u8(
			bytes, carry_offset + 22U + player
		);
		carry.combo[player].hits_highest = detail::u8(
			bytes, carry_offset + 24U + player
		);
		carry.combo[player].bonus_total = detail::u16(
			bytes, carry_offset + 26U + (player * 2U)
		);
		carry.chain_cursor[player] = detail::u8(
			bytes, carry_offset + 30U + player
		);
		for(std::size_t index = 0U; index < 16U; ++index) {
			carry.chain_hits[player][index] = detail::u8(
				bytes, carry_offset + 32U + (player * 16U) + index
			);
			carry.chain_pellet_and_fireball_value[player][index] = detail::u8(
				bytes, carry_offset + 64U + (player * 16U) + index
			);
			carry.chain_charge_fireball[player][index] = detail::u8(
				bytes, carry_offset + 96U + (player * 16U) + index
			);
			carry.chain_charge_exatt[player][index] = detail::u8(
				bytes, carry_offset + 128U + (player * 16U) + index
			);
		}
	}
	carry.cpu_shot_decision = detail::i8(bytes, carry_offset + 20U);
	carry.background_phase = detail::u8(bytes, carry_offset + 21U);
	carry.randring_cursor = detail::u8(bytes, carry_offset + 160U);
	carry.result_phase = detail::u8(bytes, carry_offset + 161U);
	carry.fireball_generation_previous = detail::u8(bytes, carry_offset + 162U);
	return snapshot;
}

bool practice_reserved_zero(std::span<const std::byte> bytes)
{
	for(std::size_t offset = 0x52U; offset <= 0x70U; ++offset) {
		if(detail::u8(bytes, offset) != 0U) {
			return false;
		}
	}
	return true;
}

std::expected<PracticeRules, Error> parse_practice_rules(
	std::span<const std::byte> bytes,
	const GameInit &init,
	const RoundStateSnapshot &round
)
{
	const auto preset = detail::u8(bytes, 0x44U);
	const auto stage = detail::u8(bytes, 0x45U);
	const auto configured_round = detail::u8(bytes, 0x46U);
	const auto stock = detail::u8(bytes, 0x47U);
	const auto extends = detail::u8(bytes, 0x48U);
	const auto timer = detail::u8(bytes, 0x49U);
	const auto round_speed = detail::u8(bytes, 0x4AU);
	const auto bullet_speed = detail::u8(bytes, 0x4BU);
	const auto p1_spell = detail::u8(bytes, 0x4CU);
	const auto cpu_spell = detail::u8(bytes, 0x4DU);
	const auto boss_level = detail::u8(bytes, 0x4EU);
	const auto cpu_damage = detail::u8(bytes, 0x4FU);
	const auto safety = detail::u16(bytes, 0x50U);
	const bool base_valid =
		preset <= 1U && timer <= 2U && round_speed <= 0x7FU &&
		bullet_speed <= 0x7FU && p1_spell >= 1U && p1_spell <= 16U &&
		cpu_spell >= 1U && cpu_spell <= 16U && boss_level <= 15U &&
		cpu_damage <= 3U && practice_reserved_zero(bytes);
	const bool scenario_valid = (preset == 0U)
		? (
			stage == 0U && configured_round == 0U && stock == 0xFFU &&
			extends == 0U
		)
		: (
			stage < 9U && configured_round <= 5U && stock <= 4U &&
			extends <= 2U && stock <= static_cast<std::uint8_t>(2U + extends)
		);
	if(!base_valid || !scenario_valid) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V12 Practice configuration is invalid", 0x44U
		));
	}
	const auto expected_lives = (stock == 0xFFU) ? 0U : stock;
	if(init.story_lives != expected_lives) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V12 Practice stock disagrees with checkpoint 0", 0x47U
		));
	}
	const auto expected_extends = (extends >= 2U) ? 0xFFU : extends;
	if(
		round.round_id != configured_round || round.round_speed != round_speed ||
		round.bullet_base_speed != bullet_speed ||
		round.gauge_attack_level != std::array<std::uint8_t, 2>{p1_spell, cpu_spell} ||
		round.boss_attack_level != boss_level ||
		round.cpu_hit_damage_additional != cpu_damage ||
		round.extends_gained != expected_extends ||
		round.cpu_safety_frames != std::array<std::uint16_t, 2>{safety, safety}
	) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V12 Practice configuration disagrees with checkpoint 0", 0x44U
		));
	}
	return PracticeRules{
		.uses_stock = (stock != 0xFFU),
		.retry_cpu_timer = static_cast<CpuTimerPolicy>(timer),
		.retry_cpu_damage = cpu_damage,
	};
}

} // namespace

std::expected<replay::ReplaySession, Error> parse_v12_replay(
	std::span<const std::byte> bytes
)
{
	constexpr std::array<std::uint8_t, 8> magic{
		'T', '3', 'R', 'P', 'L', 'Y', '1', '2',
	};
	if(bytes.size() < v12_header_size) {
		return std::unexpected(error(
			ErrorCode::truncated, "V12 replay is shorter than its 416-byte header"
		));
	}
	if(!detail::has_magic(bytes, magic)) {
		return std::unexpected(error(ErrorCode::bad_magic, "replay magic is not T3RPLY12"));
	}
	if(detail::u16(bytes, 8U) != replay_v12_version) {
		return std::unexpected(error(ErrorCode::unsupported_version, "V12 magic/version mismatch", 8U));
	}
	if(detail::u16(bytes, 10U) != v12_header_size || detail::u16(bytes, 12U) != 0U) {
		return std::unexpected(error(ErrorCode::unsupported_layout, "unsupported V12 header or sample layout", 10U));
	}
	const auto flags = detail::u16(bytes, 14U);
	if((flags & 0x0003U) != 0x0003U) {
		return std::unexpected(error(ErrorCode::unsupported_layout, "V12 does not advertise RLE and Charge input", 14U));
	}
	if((detail::u16(bytes, 0x3AU) & 0x000FU) != 0x000FU) {
		return std::unexpected(error(ErrorCode::unsupported_version, "V12 lacks definitive summary capability bits", 0x3AU));
	}
	if((detail::u8(bytes, 0x80U) & 0x0FU) != 0x0FU) {
		return std::unexpected(error(ErrorCode::unsupported_version, "V12 lacks definitive round-resume capability bits", 0x80U));
	}
	const auto autofire = detail::u8(bytes, 0x75U);
	if((autofire & 0xFCU) != 0U) {
		return std::unexpected(error(ErrorCode::invalid_value, "V12 Autofire mask contains undefined bits", 0x75U));
	}

	const auto game_mode_value = detail::u8(bytes, 0x12U);
	const bool practice = (flags & 0x0004U) != 0U;
	if(
		game_mode_value != game_mode::story &&
		game_mode_value != game_mode::vs_1p_cpu &&
		game_mode_value != game_mode::vs_1p_2p &&
		game_mode_value != game_mode::vs_cpu_cpu
	) {
		return std::unexpected(error(ErrorCode::invalid_value, "V12 game mode is not Story or VS", 0x12U));
	}
	if(practice && game_mode_value != game_mode::vs_1p_cpu) {
		return std::unexpected(error(ErrorCode::invalid_value, "V12 Practice must use GM_VS_1P_CPU", 0x12U));
	}
	const auto reservation = static_cast<std::size_t>(
		practice ? 5U : (game_mode_value == game_mode::story ? 15U : 3U)
	);
	const auto sample_count = detail::u32(bytes, 0x1AU);
	const auto input_offset = detail::u32(bytes, 0x2AU);
	const auto input_size = detail::u32(bytes, 0x2EU);
	const auto snapshot_offset = detail::u32(bytes, 0x32U);
	const auto snapshot_size = detail::u32(bytes, 0x36U);
	if(snapshot_offset != v12_header_size || snapshot_size != v12_checkpoint_size) {
		return std::unexpected(error(ErrorCode::unsupported_layout, "V12 checkpoint layout is not 416/289", 0x32U));
	}
	const auto expected_input_offset =
		v12_header_size + (reservation * v12_checkpoint_size);
	if(input_offset != expected_input_offset) {
		return std::unexpected(error(ErrorCode::unsupported_layout, "V12 input offset disagrees with scenario reservation", 0x2AU));
	}
	if(!range_fits(input_offset, input_size, bytes.size())) {
		return std::unexpected(error(ErrorCode::truncated, "V12 input stream extends past end of file", input_offset));
	}
	const auto checkpoint_count = detail::u8(bytes, 0x190U);
	if(checkpoint_count == 0U || checkpoint_count > reservation) {
		return std::unexpected(error(ErrorCode::invalid_value, "V12 reached-checkpoint count is invalid", 0x190U));
	}
	const auto directory_stage_round = detail::u8(bytes, 0x191U);
	if(
		detail::u32(bytes, snapshot_offset + 0U) != 0U ||
		detail::u32(bytes, snapshot_offset + 4U) != 0U ||
		detail::u32(bytes, snapshot_offset + 8U) != 0U
	) {
		return std::unexpected(error(ErrorCode::invalid_value, "V12 process anchor does not begin at cursor zero", snapshot_offset));
	}
	const auto compact_offset = static_cast<std::size_t>(snapshot_offset) +
		checkpoint_prefix_size;
	const auto state_offset = compact_offset + compact_snapshot_size;
	const auto carry_offset = state_offset + round_state_size;
	if(!range_fits(carry_offset, round_carry_size, bytes.size())) {
		return std::unexpected(error(ErrorCode::truncated, "V12 checkpoint 0 is truncated", snapshot_offset));
	}

	auto init = parse_compact_snapshot(bytes, compact_offset, directory_stage_round);
	init.global_frame = detail::u32(bytes, snapshot_offset + 4U);
	init.round_start_snapshot = parse_round_start(bytes, state_offset, carry_offset);
	const auto expected_directory = static_cast<std::uint8_t>(
		(init.round_start_snapshot->state.round_id << 4U) |
		((game_mode_value == game_mode::story || practice) ? init.story_stage : 0x0FU)
	);
	if(directory_stage_round != expected_directory) {
		return std::unexpected(error(ErrorCode::invalid_value, "V12 checkpoint directory disagrees with checkpoint 0", 0x191U));
	}
	if(practice) {
		if(detail::u8(bytes, 0x45U) != init.story_stage) {
			return std::unexpected(error(ErrorCode::invalid_value, "V12 Practice stage disagrees with checkpoint", 0x45U));
		}
		auto rules = parse_practice_rules(
			bytes, init, init.round_start_snapshot->state
		);
		if(!rules) {
			return std::unexpected(rules.error());
		}
		init.practice_rules = *rules;
	} else if(init.round_start_snapshot->state.round_id != 0U) {
		return std::unexpected(error(ErrorCode::invalid_value, "V12 Story/VS process anchor must begin at round 0", state_offset));
	}

	struct Check {
		std::uint32_t header;
		std::uint32_t snapshot;
		const char *name;
	};
	const std::array checks{
		Check{detail::u32(bytes, 0x22U), std::bit_cast<std::uint32_t>(init.resident_rand), "resident_rand"},
		Check{detail::u32(bytes, 0x26U), init.random_seed, "random_seed"},
		Check{game_mode_value, init.game_mode, "game_mode"},
		Check{detail::u8(bytes, 0x13U), init.rank, "rank"},
		Check{detail::u8(bytes, 0x14U), init.key_mode, "key_mode"},
		Check{detail::u8(bytes, 0x15U), std::bit_cast<std::uint8_t>(init.paletted_character[0]), "playchar_p1"},
		Check{detail::u8(bytes, 0x16U), std::bit_cast<std::uint8_t>(init.paletted_character[1]), "playchar_p2"},
		Check{detail::u8(bytes, 0x17U), init.story_stage, "story_stage"},
		Check{detail::u8(bytes, 0x18U), init.is_cpu[0], "is_cpu_p1"},
		Check{detail::u8(bytes, 0x19U), init.is_cpu[1], "is_cpu_p2"},
		Check{autofire, init.autofire_mask, "autofire"},
	};
	for(const auto &check : checks) {
		if(check.header != check.snapshot) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				std::string("V12 header/checkpoint mismatch: ") + check.name
			));
		}
	}

	auto records = detail::decode_rpy_input(
		bytes,
		static_cast<std::size_t>(input_offset),
		static_cast<std::size_t>(input_size),
		sample_count,
		"V12"
	);
	if(!records) {
		return std::unexpected(records.error());
	}
	replay::ReplaySession session{};
	session.source_version = replay_v12_version;
	session.init = std::move(init);
	session.records = std::move(*records);
	session.sample_count = sample_count;
	return session;
}

} // namespace th03::formats
