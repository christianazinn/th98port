#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "th03/core/numeric.hpp"
#include "th03/formats/t3case.hpp"
#include "th03/formats/enedat.hpp"
#include "th03/formats/replay.hpp"
#include "th03/formats/t3split.hpp"
#include "th03/replay/run.hpp"
#include "game_internal.hpp"
#include "trig.hpp"

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const std::string &message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void put_u8(std::vector<std::byte> &bytes, std::size_t offset, std::uint8_t value)
{
	bytes[offset] = static_cast<std::byte>(value);
}

void put_u16(std::vector<std::byte> &bytes, std::size_t offset, std::uint16_t value)
{
	put_u8(bytes, offset, static_cast<std::uint8_t>(value));
	put_u8(bytes, offset + 1U, static_cast<std::uint8_t>(value >> 8U));
}

void put_u32(std::vector<std::byte> &bytes, std::size_t offset, std::uint32_t value)
{
	put_u8(bytes, offset, static_cast<std::uint8_t>(value));
	put_u8(bytes, offset + 1U, static_cast<std::uint8_t>(value >> 8U));
	put_u8(bytes, offset + 2U, static_cast<std::uint8_t>(value >> 16U));
	put_u8(bytes, offset + 3U, static_cast<std::uint8_t>(value >> 24U));
}

std::uint32_t get_u32(const std::vector<std::byte> &bytes, std::size_t offset)
{
	return (
		std::to_integer<std::uint32_t>(bytes[offset]) |
		(std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 8U) |
		(std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 16U) |
		(std::to_integer<std::uint32_t>(bytes[offset + 3U]) << 24U)
	);
}

void put_magic(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::string_view magic
)
{
	for(std::size_t index = 0; index < magic.size(); ++index) {
		put_u8(bytes, offset + index, static_cast<std::uint8_t>(magic[index]));
	}
}

void refresh_t3case(std::vector<std::byte> &bytes)
{
	const auto payload_offset = get_u32(bytes, 16U);
	const auto payload_size = get_u32(bytes, 20U);
	put_u32(
		bytes,
		52U,
		th03::formats::fnv1a32(
			std::span<const std::byte>(bytes).subspan(payload_offset, payload_size)
		)
	);
	put_u32(bytes, 56U, 0U);
	put_u32(
		bytes,
		56U,
		th03::formats::fnv1a32(
			std::span<const std::byte>(bytes).first(payload_offset)
		)
	);
}

std::vector<std::byte> make_t3case(bool normalized)
{
	constexpr std::size_t record_count = 2;
	const auto payload_offset = static_cast<std::uint32_t>(
		128U + (normalized ? th03::formats::t3case_v11_snapshot_size : 0U)
	);
	const auto payload_size = static_cast<std::uint32_t>(
		record_count * th03::formats::t3case_record_size
	);
	std::vector<std::byte> bytes(payload_offset + payload_size, std::byte{0});
	put_magic(bytes, 0U, std::string_view("T3CASE1\0", 8));
	put_u16(bytes, 8U, 1U);
	put_u16(bytes, 10U, 64U);
	put_u16(bytes, 12U, 64U);
	put_u16(bytes, 14U, 20U);
	put_u32(bytes, 16U, payload_offset);
	put_u32(bytes, 20U, payload_size);
	put_u32(bytes, 24U, 1U);
	put_u32(bytes, 28U, static_cast<std::uint32_t>(record_count));
	put_u8(bytes, 32U, normalized ? 2U : 1U);
	put_u8(bytes, 33U, 1U);
	put_u8(bytes, 34U, 1U);
	put_u8(bytes, 35U, 1U);
	put_u8(bytes, 36U, 1U);
	put_u8(bytes, 37U, 3U);
	put_u16(bytes, 38U, normalized ? 0x0008U : 0U);
	put_u32(bytes, 40U, 42U);
	put_u32(bytes, 60U, static_cast<std::uint32_t>(bytes.size()));

	put_u32(bytes, 64U, 29U);
	put_u32(bytes, 68U, 0x89ABCDEFU);
	put_u8(bytes, 72U, 1U);
	put_u8(bytes, 73U, 2U);
	put_u8(bytes, 80U, 1U);
	put_u8(bytes, 81U, 1U);
	put_u8(bytes, 82U, 1U);
	put_u8(bytes, 83U, 4U);
	for(std::size_t stage = 0; stage < 9U; ++stage) {
		put_u8(bytes, 84U + stage, 0xFFU);
	}
	put_u8(bytes, 93U, 0xFFU);

	if(normalized) {
		constexpr std::size_t snapshot = 128U;
		put_u32(bytes, snapshot, 0x89ABCDEFU);
		for(std::size_t index = 0; index < 256U; ++index) {
			put_u8(bytes, snapshot + 4U + index, static_cast<std::uint8_t>(index));
			put_u8(bytes, snapshot + 261U + index, static_cast<std::uint8_t>(index + 1U));
			put_u8(bytes, snapshot + 517U + index, static_cast<std::uint8_t>(index + 2U));
		}
		put_u8(bytes, snapshot + 260U, 7U);
		put_u16(bytes, snapshot + 905U, 0x1234U);
		put_u16(bytes, snapshot + 907U, 0x5678U);
	}

	const auto input = static_cast<std::size_t>(payload_offset);
	put_u8(bytes, input, 1U);
	put_u8(bytes, input + 1U, 0U);
	put_u32(bytes, input + 4U, 0U);
	put_u16(bytes, input + 12U, 0x0020U);
	const auto control = input + th03::formats::t3case_record_size;
	put_u8(bytes, control, 2U);
	put_u8(bytes, control + 1U, 2U);
	put_u32(bytes, control + 4U, 1U);
	put_u16(bytes, control + 18U, 1U);
	refresh_t3case(bytes);
	return bytes;
}

th03::formats::SplitRow split_row(
	th03::formats::SplitEvent event,
	std::uint32_t global_frame,
	std::uint32_t state_hash
)
{
	th03::formats::SplitRow row{};
	row.event = event;
	row.global_frame = global_frame;
	row.round_frame = global_frame;
	row.round_or_result_frame = static_cast<std::uint16_t>(global_frame);
	row.game_mode = 1U;
	row.resident_rand = 29;
	row.round_speed = 96U;
	row.state_hash = state_hash;
	return row;
}

void test_numeric()
{
	using namespace th03::exact;
	check(wrap_add<std::int16_t>(32767, 1) == std::numeric_limits<std::int16_t>::min(), "signed 16-bit addition wraps explicitly");
	check(wrap_subtract<std::uint8_t>(0U, 1U) == 255U, "unsigned 8-bit subtraction wraps");
	check(wrap_negate<std::int8_t>(std::numeric_limits<std::int8_t>::min()) == std::numeric_limits<std::int8_t>::min(), "signed 8-bit negation wraps");
	check(
		wrap_multiply<std::int16_t>(300, 300) == 24464,
		"signed 16-bit multiplication wraps before later arithmetic"
	);
	check(wrap_shift_left<std::int16_t>(-17, 4U) == -272, "signed left shift uses the wrapped PC-98 bit pattern");
	check(arithmetic_shift_right<std::int16_t>(-17, 4U) == -2, "signed right shift is arithmetic");
	check(Subpixel::from_integer_wrapping(12).raw() == 192, "Q12.4 integer conversion");
	check(Subpixel::from_raw(-17).to_integer_floor() == -2, "Q12.4 pixel conversion floors through arithmetic shift");
	check(Subpixel::from_raw(-17).to_integer_trunc() == -1, "Q12.4 slow conversion truncates toward zero");
	check(SubpixelDelta8::from_integer_wrapping(8).raw_bits() == 0x80U, "signed Q4.4 construction preserves raw wrap");
	check(DecimalSubpixel::from_raw(-19).to_integer_trunc() == -1, "decimal fixed point truncates toward zero");
}

void test_game_init_source_domain()
{
	th03::GameInit init{};
	init.game_mode = 1U;
	init.story_stage = 8U;
	init.remaining_credits = 3U;
	init.paletted_character = {1, 3};
	check(
		th03::Game::create(init).has_value(),
		"Story accepts the last gameplay stage and source initial credit count"
	);
	for(std::uint8_t rank = 0U; rank <= 3U; ++rank) {
		init.rank = rank;
		check(
			th03::Game::create(init).has_value(),
			"Game accepts every source difficulty rank"
		);
	}
	for(const auto rank : {std::uint8_t{4U}, std::uint8_t{255U}}) {
		init.rank = rank;
		const auto game = th03::Game::create(init);
		check(
			!game && game.error().code == th03::ErrorCode::invalid_value,
			"Game rejects a rank outside the source difficulty domain"
		);
	}
	init.rank = 0U;

	init.demo_number = 1U;
	for(const auto mode : {
		th03::game_mode::story,
		th03::game_mode::vs_1p_cpu,
		th03::game_mode::vs_1p_2p,
		th03::game_mode::vs_cpu_cpu,
	}) {
		init.game_mode = mode;
		const auto game = th03::Game::create(init);
		check(
			!game && game.error().code == th03::ErrorCode::invalid_value,
			"nonzero demo number is rejected outside GM_DEMO"
		);
	}
	init.game_mode = th03::game_mode::demo;
	check(
		th03::Game::create(init).has_value(),
		"GM_DEMO accepts a nonzero demo number"
	);
	init.demo_number = 0U;
	check(
		th03::Game::create(init).has_value(),
		"GM_DEMO with zero demo number preserves the source no-demo callback path"
	);
	init.game_mode = th03::game_mode::story;

	init.remaining_credits = 0U;
	check(
		th03::Game::create(init).has_value(),
		"Story accepts zero remaining credits"
	);
	for(const auto credits : {std::uint8_t{4U}, std::uint8_t{255U}}) {
		init.remaining_credits = credits;
		const auto game = th03::Game::create(init);
		check(
			!game && game.error().code == th03::ErrorCode::invalid_value,
			"Story rejects a remaining-credit byte outside the source domain"
		);
	}

	init.remaining_credits = 3U;
	for(const auto stage : {
		std::uint8_t{9U}, std::uint8_t{223U}, std::uint8_t{255U}
	}) {
		init.story_stage = stage;
		const auto game = th03::Game::create(init);
		check(
			!game && game.error().code == th03::ErrorCode::invalid_value,
			"Story rejects a stage byte outside the gameplay range"
		);
	}

	init.game_mode = th03::detail::game_mode_none;
	check(
		!th03::Game::create(init),
		"GM_NONE is not accepted as a gameplay scenario"
	);
	init.game_mode = th03::detail::game_mode_vs_1p_cpu;
	init.story_stage = 255U;
	init.remaining_credits = 255U;
	check(
		th03::Game::create(init).has_value(),
		"non-Story modes preserve resident Story bytes without validating them"
	);
	init.game_mode = 1U;
	init.runtime_snapshot = th03::RuntimeSnapshot{};
	check(
		!th03::Game::create(init),
		"a runtime snapshot cannot bypass Story construction validation"
	);
}

void test_round_start_snapshot_restoration()
{
	th03::GameInit init{};
	init.game_mode = th03::game_mode::vs_1p_cpu;
	init.is_cpu = {0U, 1U};
	init.paletted_character = {1, 3};
	init.runtime_snapshot = th03::RuntimeSnapshot{};
	th03::RoundStartSnapshot snapshot{};
	snapshot.state.round_id = 5U;
	snapshot.state.rounds_won = {1U, 2U};
	snapshot.state.score[0] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
	snapshot.state.score[1] = {9U, 8U, 7U, 6U, 5U, 4U, 3U, 2U};
	snapshot.state.round_speed = 0x6AU;
	snapshot.state.bullet_base_speed = 0x1BU;
	snapshot.state.gauge_attack_level = {12U, 13U};
	snapshot.state.boss_attack_level = 14U;
	snapshot.state.cpu_hit_damage_additional = 3U;
	snapshot.state.extends_gained = 2U;
	snapshot.state.cpu_safety_frames = {0x1234U, 0xABCDU};
	snapshot.carry.cpu_dodge_strategy = {2U, 3U};
	snapshot.carry.human_movement_last = {0x1111U, 0x2222U};
	snapshot.carry.spell_ready_frames = {4U, 5U};
	snapshot.carry.combo_bonus_max = {600U, 700U};
	snapshot.carry.combo_hits_max = {8U, 9U};
	snapshot.carry.shot_cycle = {3U, 4U};
	snapshot.carry.boss_panic_fired = {1U, 0U};
	snapshot.carry.gauge_attack_next = {2U, 5U};
	snapshot.carry.cpu_shot_decision = -2;
	snapshot.carry.background_phase = 2U;
	snapshot.carry.combo[0] = {10U, 11U, 1200U};
	snapshot.carry.combo[1] = {12U, 13U, 1400U};
	snapshot.carry.chain_cursor = {14U, 15U};
	snapshot.carry.chain_hits[1][15] = 16U;
	snapshot.carry.chain_pellet_and_fireball_value[0][14] = 17U;
	snapshot.carry.chain_charge_fireball[1][15] = 18U;
	snapshot.carry.chain_charge_exatt[0][14] = 19U;
	snapshot.carry.randring_cursor = 201U;
	snapshot.carry.result_phase = 2U;
	snapshot.carry.fireball_generation_previous = 3U;
	init.round_start_snapshot = snapshot;

	const auto state = th03::detail::initialize_first_round(init, 18U);
	check(
		state.round_id == 5U && state.players[0].rounds_won == 1U &&
		state.players[1].rounds_won == 2U && state.score == snapshot.state.score,
		"round-start state restores round identity, wins, and scores"
	);
	check(
		state.round_speed == 0x6AU && state.bullet_base_speed == 0x1BU &&
		state.gba_gauge_level == std::array<std::uint8_t, 2>{12U, 13U} &&
		state.gba_boss_level == 14U && state.cpu_hit_damage_additional == 3U &&
		state.extends_gained == 2U,
		"round-start state restores rank-derived gameplay values"
	);
	check(
		state.players[0].cpu_safety_frames == 0x1234U &&
		state.players[1].cpu_dodge_strategy == 3U &&
		state.players[0].human_movement_last == 0x1111U &&
		state.players[1].spell_ready_frames == 5U &&
		state.players[0].combo_bonus_max == 600U &&
		state.players[1].combo_hits_max == 9U,
		"round-start carry restores player retry fields"
	);
	check(
		state.autofire_shot_gate == std::array<std::uint8_t, 2>{3U, 4U} &&
		state.boss_panic_fired_in_combo == std::array<std::uint8_t, 2>{1U, 0U} &&
		state.gba_flag_next[1] == th03::detail::GaugeBossAttackFlag::boss &&
		state.cpu_target_kind_scratch == -2 &&
		state.background_phase == th03::detail::BackgroundPhase::type_b_steady,
		"round-start carry restores shared callback and CPU state"
	);
	check(
		state.combos[1].bonus_total == 1400U && state.chains.cursor[1] == 15U &&
		state.chains.hits[1][15] == 16U &&
		state.chains.pellet_and_fireball_value[0][14] == 17U &&
		state.chains.charge_fireball[1][15] == 18U &&
		state.chains.charge_exatt[0][14] == 19U &&
		state.randring_cursor == 201U &&
		state.result_phase == th03::detail::ResultPhase::closing &&
		state.fireball_generation_previous == 3U,
		"round-start carry restores combo, chain, RNG, and transition state"
	);
	check(
		th03::Game::create(init).has_value(),
		"valid round-start snapshot passes public construction validation"
	);
	init.runtime_snapshot.reset();
	check(
		!th03::Game::create(init),
		"round-start state cannot be detached from its runtime snapshot"
	);
	init.runtime_snapshot = th03::RuntimeSnapshot{};
	init.round_start_snapshot->carry.chain_cursor[0] = 16U;
	check(
		!th03::Game::create(init),
		"invalid round-start array cursor is rejected before simulation"
	);
}

void test_score_most_significant_digit_is_not_bcd_normalized()
{
	th03::detail::DeterministicState state{};
	state.score[0].fill(9U);
	state.combos[0].time = 1U;
	state.combos[0].bonus_total = 1U;
	state.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.score[0] ==
			std::array<std::uint8_t, 8>{0U, 0U, 0U, 0U, 0U, 0U, 0U, 10U},
		"Score carry leaves the most-significant byte raw like score_add.asm"
	);
}

void test_anniversary_combo_display_limit()
{
	th03::detail::DeterministicState state{};
	state.hud_start_flag = 1U;
	state.combo_points_for_boss_attack = 0xFFFFU;
	state.chains.hits[0][0] = 112U;

	auto &explosion = state.enemies[0];
	explosion.flag = th03::detail::EnemyFlag::explosion_hitting_enemies;
	explosion.frame = 4U;
	explosion.center_x = 144 << 4;
	explosion.center_y = 80 << 4;
	explosion.player_id = 0U;
	explosion.size_pixels = 16U;
	explosion.chain_slot = 0U;

	auto &pellet = state.bullets[0];
	pellet.flag = th03::detail::BulletFlag::pellet;
	pellet.center_x = explosion.center_x;
	pellet.center_y = explosion.center_y;
	pellet.player_id = 0U;
	pellet.speed_next = 24U;

	check(
		th03::detail::gameplay_frame_prefix(state, th03::GameContent{}) &&
		state.chains.hits[0][0] == 113U &&
		state.combos[0].hits_highest == 99U &&
		state.players[0].combo_hits_max == 113U,
		"Anniversary clamps the live combo display while preserving the raw defeat-screen maximum"
	);

	auto replay_three_digit = state;
	replay_three_digit.combos[0].hits_highest = 113U;
	check(
		th03::detail::state_hash(state) ==
			th03::detail::state_hash(replay_three_digit),
		"Anniversary canonical hashing normalizes the Replay Patch's three-digit combo extension"
	);
}

void test_story_score_initialization()
{
	th03::GameInit init{};
	init.game_mode = th03::game_mode::story;
	init.last_score[0] = {1U, 2U, 3U, 4U, 5U, 6U, 1U, 0U};
	init.last_score[1] = {9U, 8U, 7U, 6U, 5U, 4U, 3U, 2U};
	const auto state = th03::detail::initialize_first_round(init, 0U);
	check(
		state.score[0] == init.last_score[0] &&
		state.score[1] == std::array<std::uint8_t, 8>{} &&
		state.extends_gained == 1U,
		"Story loads only P1 resident score and derives extends from that score"
	);
}

void test_defeat_playfield_shake()
{
	th03::detail::DeterministicState state{};
	state.players[0].lose_anim_time = 0x30U;
	(void)th03::detail::gameplay_frame_suffix_before_interstitial(state);
	check(
		state.playfield_fg_shift_x[0] == 6,
		"Defeat frame 0x30 applies the source's positive playfield shift"
	);
	state.players[0].lose_anim_time = 1U;
	state.playfield_fg_shift_x[0] = -6;
	(void)th03::detail::gameplay_frame_suffix_before_interstitial(state);
	check(
		state.playfield_fg_shift_x[0] == 0 &&
		state.players[0].lose_anim_time == 0xFFU,
		"The terminal defeat frame restores the playfield shift"
	);
}

void test_th3r03_round_start()
{
	th03::GameInit init{};
	init.resident_rand = 2638;
	init.random_seed = 0xE9B35FBAU;
	init.game_mode = 1U;
	init.rank = 3U;
	init.story_lives = 2U;
	init.remaining_credits = 3U;
	init.skill = 145U;
	init.is_cpu = {0U, 1U};
	init.paletted_character = {1, 11};
	init.autofire_mask = 1U;
	th03::RuntimeSnapshot snapshot{};
	snapshot.random_seed = 0xE9B35FBAU;
	for(auto &player : snapshot.players) {
		player.center_x = th03::exact::Subpixel::from_raw(2304);
		player.center_y = th03::exact::Subpixel::from_raw(5376);
		player.halfhearts = 10U;
		player.invincibility_time = 50U;
		player.gauge_charge_speed = 32U;
		player.gauge_avail = 1024U;
		player.bombs = 2U;
		player.shot_active = 1U;
	}
	snapshot.cpu_charge_ring[1][0] = 191U;
	init.runtime_snapshot = snapshot;

	auto game = th03::Game::create(init);
	check(game.has_value(), "TH3R03 first-round state initializes");
	const auto initialized = th03::detail::initialize_first_round(init);
	check(
		initialized.boss_initial[1].hp == 0x0064U,
		"TH3R03 P2 Kana boss HP excludes the adjacent palette sprite offset"
	);
	if(game) {
		const auto wrong_context = game->step(
			th03::CoreInput{}, th03::InputContext::interstitial
		);
		check(
			wrong_context.status == th03::StepStatus::invalid_input_context,
			"TH3R03 rejects interstitial input outside a warning wait"
		);
		const auto state = game->canonical_state();
		check(state.round_speed == 96U, "TH3R03 Lunatic round speed starts at raw Q4.4 96");
		check(state.state_hash == 0x5BDCE01AU, "TH3R03 native start state matches Anniversary schema-3 hash");
		th03::StepResult boundary{};
		for(std::uint32_t frame = 0; frame < 128U; ++frame) {
			th03::CoreInput input{0x0020U, 0U, 0U, 0U};
			if(frame <= 34U) {
				input.charge_mask = 1U;
			}
			if((frame >= 10U && frame <= 14U) ||
				(frame >= 37U && frame <= 85U)) {
				input.p1 = 0x0021U;
			} else if(frame >= 86U && frame <= 126U) {
				input.p1 = 0x0022U;
			}
			boundary = game->step(input);
		}
		check(boundary.status == th03::StepStatus::running,
			"TH3R03 native prefix advances through sample 128");
		check(boundary.input_boundary_state.state_hash == 0xC35124AFU,
			"TH3R03 sample-128 boundary matches Anniversary schema-3 hash");
	}
}

void test_th3r03_second_stage_start()
{
	th03::GameInit init{};
	init.global_frame = 6778U;
	init.resident_rand = 5503;
	init.game_mode = 1U;
	init.rank = 3U;
	init.story_stage = 1U;
	init.story_lives = 2U;
	init.remaining_credits = 3U;
	init.skill = 145U;
	init.is_cpu = {0U, 1U};
	init.paletted_character = {1, 13};
	init.last_score[0] = {3U, 0U, 5U, 0U, 9U, 0U, 1U, 0U};
	init.autofire_mask = 1U;
	const auto state = th03::detail::initialize_first_round(init, 18U);
	check(
		th03::detail::canonical_state(state).state_hash == 0x3F0A216BU,
		"TH3R03 fresh second-MAIN startup matches Anniversary schema-3 hash"
	);
	check(
		state.gba_boss_level == 9U,
		"TH3R03 Stage 2 Lunatic Boss Attack level uses the Story stage byte"
	);
}

void test_rikako_bomb_shift_reset()
{
	th03::detail::DeterministicState state{};
	state.players[1].paletted_character = 13;
	state.bomb_flag[1] = th03::detail::BombFlag::active;
	state.bomb_frame[1] = 128U;
	state.playfield_fg_shift_x[1] = -4;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.playfield_fg_shift_x[1] == 0,
		"Rikako bomb restores the playfield shift after its shake interval"
	);
}

void test_ellen_bomb_vector_release()
{
	th03::detail::DeterministicState state{};
	state.players[1].paletted_character = 7;
	state.bomb_flag[1] = th03::detail::BombFlag::preparing;
	state.randring_cursor = 5U;
	const th03::GameContent content{};

	(void)th03::detail::gameplay_frame_prefix(state, content);
	check(
		state.bomb_flag[1] == th03::detail::BombFlag::active &&
		state.bomb_frame[1] == 1U && state.randring_cursor == 5U &&
		std::all_of(
			state.ellen_bomb_vectors[1].begin(),
			state.ellen_bomb_vectors[1].end(),
			[](const auto &vector) { return vector.x == 19999; }
		),
		"Ellen bomb prepares eight inactive vectors without consuming RNG"
	);
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.ellen_bomb_vector_cursor == 0U,
		"Ellen bomb's early render phase resets its shared vector cursor"
	);

	state.bomb_frame[1] = 63U;
	(void)th03::detail::gameplay_frame_prefix(state, content);
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.bomb_frame[1] == 64U &&
		state.ellen_bomb_vectors[1][0].x == 9999 &&
		state.ellen_bomb_vector_cursor == 1U &&
		state.randring_cursor == 5U,
		"Ellen bomb releases one vector on render frame 64 without RNG"
	);

	state.randring[5] = 0x40U;
	state.randring[6] = 0x12U;
	(void)th03::detail::gameplay_frame_prefix(state, content);
	const auto expected_velocity = th03::detail::vector2(0x40U, 224);
	check(
		state.bomb_frame[1] == 65U && state.randring_cursor == 6U &&
		state.ellen_bomb_vectors[1][0].x == (144 << 4) &&
		state.ellen_bomb_vectors[1][0].y == (184 << 4) &&
		state.ellen_bomb_vectors[1][0].velocity_x == expected_velocity.x &&
		state.ellen_bomb_vectors[1][0].velocity_y == expected_velocity.y,
		"Ellen bomb respawns a released vector with one shared-ring draw"
	);
}

void test_rikako_boss_materialization_collision()
{
	th03::detail::DeterministicState state{};
	state.players[1].paletted_character = 13;
	state.gba_boss_launched_by = 1;
	state.boss.state = 0U;
	state.boss.pattern_frame = 5U;
	state.boss.center_x = 144 << 4;
	state.boss.center_y = 80 << 4;
	state.boss.hp = 110U;
	state.shotpairs[0].alive = true;
	state.shotpairs[0].player_id = 0U;
	state.shotpairs[0].topleft_x = 2520;
	state.shotpairs[0].topleft_y = 1600;
	state.shotpairs[0].velocity_y = -192;
	const th03::GameContent content{};

	(void)th03::detail::gameplay_frame_prefix(state, content);
	check(
		state.shotpairs[0].alive && state.boss.hp == 110U,
		"Rikako's materializing Boss Attack is not yet shot-collidable"
	);
	check(
		state.boss.pattern_frame == 6U && state.shotpairs[0].topleft_y == 1408,
		"Rikako's materialization returns before the common boss tail"
	);

	state.boss.pattern_frame = 99U;
	state.shotpairs[0].topleft_y = 1600;
	(void)th03::detail::gameplay_frame_prefix(state, content);
	check(
		!state.shotpairs[0].alive && state.boss.hp == 108U &&
		state.boss.state == 1U,
		"Rikako's Boss Attack becomes shot-collidable on materialization frame 100"
	);
}

void test_rikako_boss_dispatch_boundaries()
{
	const auto four_spread_state = [](std::uint8_t pattern) {
		th03::detail::DeterministicState state{};
		state.players[1].paletted_character = 13;
		state.gba_boss_launched_by = 1;
		state.boss.state = pattern;
		state.boss.center_x = 144 << 4;
		state.boss.center_y = 80 << 4;
		state.boss.hp = 110U;
		state.boss.params[3] = 32U;
		state.boss.params[4] = 64U;
		state.randring[0] = 0x34U;
		state.randring[1] = 1U;
		(void)th03::detail::gameplay_frame_prefix(
			state, th03::GameContent{}
		);
		return state;
	};
	for(const auto pattern : {std::uint8_t{6U}, std::uint8_t{13U}}) {
		const auto state = four_spread_state(pattern);
		const auto active_bullets = std::count_if(
			state.bullets.begin(), state.bullets.end(),
			[](const auto &bullet) {
				return bullet.flag != th03::detail::BulletFlag::free;
			}
		);
		check(
			state.randring_cursor == 2U && active_bullets == 8 &&
			state.bullets[0].flag == th03::detail::BulletFlag::bullet16 &&
			state.boss.pattern_angle == 0x34U && state.boss.direction == 2U,
			"Rikako Boss states 6 and 13 dispatch to the RNG-driven four-spread pattern"
		);
	}

	th03::detail::DeterministicState rings{};
	rings.players[1].paletted_character = 13;
	rings.gba_boss_launched_by = 1;
	rings.boss.state = 7U;
	rings.boss.center_x = 144 << 4;
	rings.boss.center_y = 80 << 4;
	rings.boss.hp = 110U;
	rings.boss.params[1] = 8U;
	rings.boss.params[2] = 28U;
	(void)th03::detail::gameplay_frame_prefix(rings, th03::GameContent{});
	const auto active_bullets = std::count_if(
		rings.bullets.begin(), rings.bullets.end(),
		[](const auto &bullet) {
			return bullet.flag != th03::detail::BulletFlag::free;
		}
	);
	check(
		rings.randring_cursor == 0U && active_bullets == 0,
		"Rikako Boss state 7 dispatches to the Extra Attack and ring pattern"
	);
}

void test_chiyuri_extra_pair_materialization_transition()
{
	th03::detail::DeterministicState state{};
	state.players[0].paletted_character = 15;
	auto &top = state.extra_attacks[0][0];
	auto &bottom = state.extra_attacks[0][1];
	top.state = 29U;
	top.frame = 17U;
	bottom.state = 29U;
	bottom.frame = 23U;

	(void)th03::detail::gameplay_frame_prefix(state, th03::GameContent{});
	check(
		top.state == 1U && top.frame == 2U &&
		bottom.state == 29U && bottom.frame == 0U,
		"Chiyuri's completed pair advances the top frame through ZUN's slot-pointer decrement"
	);
}

void test_gauge_pattern_updates_during_hud_start()
{
	th03::detail::DeterministicState state{};
	state.players[0].paletted_character = 15;
	state.gba_flag_active[0] =
		th03::detail::GaugeBossAttackFlag::gauge_pellet_init;
	const th03::GameContent content{};

	(void)th03::detail::gameplay_frame_prefix(state, content);
	(void)th03::detail::gameplay_frame_prefix(state, content);
	(void)th03::detail::gameplay_frame_prefix(state, content);
	const auto active_bullets = std::count_if(
		state.bullets.begin(), state.bullets.end(),
		[](const auto &bullet) {
			return bullet.flag != th03::detail::BulletFlag::free;
		}
	);
	check(
		state.hud_start_flag != 0U &&
		state.gba_flag_active[0] ==
			th03::detail::GaugeBossAttackFlag::gauge_pellet_active &&
		state.chiyuri_gauge_pattern_frame[0] == 2U && active_bullets == 2,
		"Gauge patterns continue updating while the round-start HUD suppresses formations"
	);
}

void test_marisa_boss_patterns()
{
	const th03::GameContent content{};

	th03::detail::DeterministicState direct{};
	direct.players[0].paletted_character = 5;
	direct.gba_boss_launched_by = 0;
	direct.boss.state = 6U;
	direct.boss.pattern_frame = 0x3FU;
	direct.boss.center_x = 144 << 4;
	direct.boss.center_y = 80 << 4;
	direct.boss.hp = 110U;
	(void)th03::detail::gameplay_frame_prefix(direct, content);
	check(
		direct.extra_attacks[0][0].state == 3U &&
		direct.extra_attacks[0][0].frame == 0U &&
		direct.extra_attacks[0][0].center_x == (32 << 4) &&
		direct.extra_attacks[0][0].center_y == (368 << 4) &&
		direct.extra_attacks[0][0].player_id == 1U &&
		direct.extra_attacks[0][1].state == 3U &&
		direct.extra_attacks[0][1].center_x == (256 << 4) &&
		direct.extra_attacks[0][1].center_y == (368 << 4),
		"Marisa's three-pair Boss pattern allocates its first source-coordinate pair"
	);

	th03::detail::DeterministicState rings{};
	rings.players[0].paletted_character = 5;
	rings.gba_boss_launched_by = 0;
	rings.gba_boss_level = 8U;
	rings.boss.state = 11U;
	rings.boss.pattern_frame = 0x1FU;
	rings.boss.center_x = 144 << 4;
	rings.boss.center_y = 80 << 4;
	rings.boss.hp = 110U;
	rings.boss.params[2] = 40U;
	rings.boss.params[3] = 18U;
	rings.boss.params[4] = 30U;
	rings.randring_cursor = 10U;
	rings.randring[10] = 0x40U;
	(void)th03::detail::gameplay_frame_prefix(rings, content);
	const auto active_bullets = std::count_if(
		rings.bullets.begin(), rings.bullets.end(),
		[](const auto &bullet) {
			return bullet.flag != th03::detail::BulletFlag::free;
		}
	);
	check(
		rings.randring_cursor == 11U && active_bullets == 60 &&
		rings.bullets[0].flag == th03::detail::BulletFlag::bullet16 &&
		rings.bullets[0].angle_next == 0x40U &&
		rings.bullets[30].flag == th03::detail::BulletFlag::pellet_cloud &&
		rings.bullets[30].angle_next == 0x48U,
		"Marisa's high-level Boss rings preserve one RNG draw and the cloud angle offset"
	);
}

void test_kana_bullet_template_angle_inheritance()
{
	const auto make_state = [](
		std::uint8_t boss_state, std::uint16_t pattern_frame
	) {
		th03::detail::DeterministicState state{};
		state.players[1].paletted_character = 11;
		state.gba_boss_launched_by = 1;
		state.boss.state = boss_state;
		state.boss.pattern_frame = pattern_frame;
		state.boss.center_x = 144 << 4;
		state.boss.center_y = 80 << 4;
		state.boss.hp = 110U;
		state.boss.params = {24U, 24U, 31U, 25U, 8U, 24U, 64U};
		auto &extra = state.extra_attacks[1][0];
		extra.state = 1U;
		extra.frame = 0U;
		extra.center_x = 120 << 4;
		extra.center_y = 80 << 4;
		extra.angle = 0U;
		extra.speed = 8U;
		return state;
	};

	auto state = make_state(2U, 0x27U);

	(void)th03::detail::gameplay_frame_prefix(state, th03::GameContent{});
	check(
		state.bullets[0].angle_next == 0x40U &&
		state.bullets[1].angle_next == 0x40U &&
		state.bullets[2].angle_next == 0x4AU &&
		state.bullet_template_angle == 0x40U,
		"Kana's Boss accel ring inherits the Extra Attack template-angle write from earlier in the prefix"
	);

	for(const auto pattern_frame : std::array<std::uint16_t, 3>{
		0x27U, 0x3FU, 0x47U
	}) {
		auto rings = make_state(7U, pattern_frame);
		(void)th03::detail::gameplay_frame_prefix(
			rings, th03::GameContent{}
		);
		check(
			rings.bullets[1].angle_next == 0x40U,
			"Kana's three default Boss rings inherit the Extra Attack template-angle write"
		);
		if(pattern_frame == 0x3FU) {
			check(
				rings.bullet_template_angle == 0x80U,
				"Kana's spread loop preserves the template mutation after its final add"
			);
		}
	}

	auto clouds = make_state(13U, 7U);
	(void)th03::detail::gameplay_frame_prefix(clouds, th03::GameContent{});
	check(
		clouds.bullets[1].flag == th03::detail::BulletFlag::pellet_cloud &&
		clouds.bullets[1].angle_next == 0x40U,
		"Kana's cloud Boss ring inherits the Extra Attack template-angle write"
	);

	th03::detail::DeterministicState reset{};
	reset.bullet_template_angle = 0xFFU;
	(void)th03::detail::gameplay_frame_prefix(reset, th03::GameContent{});
	check(
		reset.bullet_template_angle == 0U,
		"The shared bullet-template angle resets at the next gameplay prefix"
	);
}

void test_bullet_template_post_add_angle_mutations()
{
	th03::detail::DeterministicState ellen_boss{};
	ellen_boss.players[0].paletted_character = 7;
	ellen_boss.gba_boss_launched_by = 0;
	ellen_boss.boss.state = 2U;
	ellen_boss.boss.pattern_frame = 0x23U;
	ellen_boss.boss.center_x = 144 << 4;
	ellen_boss.boss.center_y = 80 << 4;
	ellen_boss.boss.hp = 110U;
	ellen_boss.boss.params[0] = 48U;
	ellen_boss.ellen_boss_spread_angle = 0x20U;
	(void)th03::detail::gameplay_frame_prefix(
		ellen_boss, th03::GameContent{}
	);
	check(
		ellen_boss.bullets[14].angle_next == 0x58U &&
		ellen_boss.bullet_template_angle == 0x60U,
		"Ellen's eight-ring Boss loop preserves the template mutation after its final add"
	);

	const auto kotohime_extra_state = [](std::uint8_t milestone) {
		th03::detail::DeterministicState state{};
		state.players[0].paletted_character = 9;
		state.kotohime_gauge_pattern_type[0] =
			th03::detail::BulletFlag::bullet16;
		state.gauge_pattern_param_a[0] = 4U;
		state.gauge_pattern_param_b[0] = 0U;
		state.randring[0] = 0x31U;
		auto &extra = state.extra_attacks[0][0];
		extra.state = 1U;
		extra.center_x = 144 << 4;
		extra.center_y = 80 << 4;
		extra.kotohime_target_y = 80 << 4;
		extra.kotohime_milestone = milestone;
		extra.behavior_variant = 1U;
		(void)th03::detail::gameplay_frame_prefix(
			state, th03::GameContent{}
		);
		return state;
	};
	const auto kotohime_four = kotohime_extra_state(4U);
	check(
		kotohime_four.bullets[12].angle_next == 0x55U &&
		kotohime_four.bullet_template_angle == 0x61U,
		"Kotohime's four-ring Extra loop preserves its final angle increment"
	);
	const auto kotohime_ten = kotohime_extra_state(0x10U);
	check(
		kotohime_ten.bullets[36].angle_next == 0xF2U &&
		kotohime_ten.bullet_template_angle == 0xEBU,
		"Kotohime's ten-ring Extra loop preserves its final angle decrement"
	);

	th03::detail::DeterministicState ellen_gauge{};
	ellen_gauge.players[0].paletted_character = 7;
	ellen_gauge.gba_flag_active[0] =
		th03::detail::GaugeBossAttackFlag::gauge_bullet_active;
	ellen_gauge.ellen_gauge_pattern_frame[0] = 0U;
	ellen_gauge.ellen_gauge_pattern_x[0] = 0;
	ellen_gauge.ellen_gauge_pattern_y[0] = 0;
	ellen_gauge.gauge_pattern_param_b[0] = 48U;
	ellen_gauge.randring[0] = 0x11U;
	ellen_gauge.randring[1] = 0x22U;
	ellen_gauge.randring[2] = 0x33U;
	(void)th03::detail::gameplay_frame_prefix(
		ellen_gauge, th03::GameContent{}
	);
	check(
		ellen_gauge.randring_cursor == 2U &&
		ellen_gauge.bullets[0].angle_next == 0x11U &&
		ellen_gauge.bullet_template_angle == 0x22U,
		"Ellen's Gauge pattern stores its final non-firing template-angle draw"
	);

	th03::detail::DeterministicState transfer{};
	auto &source = transfer.bullets[0];
	source.flag = th03::detail::BulletFlag::transfer_pellet;
	source.center_x = 1000;
	source.center_y = 1000;
	source.velocity_x = 100;
	source.target_x_for_origin_player = 1050;
	source.target_x_for_target_player = 2000;
	source.speed_next = 24U;
	source.angle_next = 0x5AU;
	source.player_id = 0U;
	source.group_next = 29U;
	(void)th03::detail::gameplay_frame_prefix(
		transfer, th03::GameContent{}
	);
	check(
		transfer.bullets[1].flag ==
			th03::detail::BulletFlag::pellet_cloud &&
		transfer.bullet_template_angle == 0x5AU,
		"A transferred pellet copies its derived-group angle into the shared template"
	);
}

void test_mima_boss_renderer_state()
{
	th03::detail::DeterministicState state{};
	state.players[0].paletted_character = 3;
	state.gba_boss_launched_by = 0;
	state.boss.state = 1U;
	state.boss.render_state = 2U;
	state.boss.render_frame = 0x1FU;
	state.background_phase = th03::detail::BackgroundPhase::type_a_steady;

	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.boss.render_state == 2U &&
		state.boss.render_frame == 0x20U,
		"Mima's Boss renderer checks its transition before incrementing the frame"
	);
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.boss.render_state == 3U &&
		state.boss.render_frame == 0x21U,
		"Mima's Boss renderer transitions state 2 at frame 0x20"
	);

	state.boss.render_state = 5U;
	state.boss.render_frame = 0xE0U;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.boss.render_state == 5U && state.boss.render_frame == 1U,
		"Mima's Boss renderer preserves state 5 while resetting its frame"
	);

	state.boss.render_state = 2U;
	state.boss.render_frame = 0x20U;
	state.boss.hit = 1U;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.boss.render_state == 2U && state.boss.render_frame == 0x20U,
		"Mima's hit renderer returns before mutating animation state"
	);
}

void test_chiyuri_boss_patterns_and_rng()
{
	const th03::GameContent content{};
	const auto active_bullets = [](const auto &state) {
		return std::count_if(
			state.bullets.begin(), state.bullets.end(),
			[](const auto &bullet) {
				return bullet.flag != th03::detail::BulletFlag::free;
			}
		);
	};

	th03::detail::DeterministicState materialize{};
	materialize.players[1].paletted_character = 15;
	materialize.gba_boss_launched_by = 1;
	materialize.boss.state = 0U;
	materialize.boss.pattern_frame = 95U;
	materialize.boss.center_x = 100 << 4;
	materialize.boss.center_y = 70 << 4;
	materialize.boss.hp = 140U;
	(void)th03::detail::gameplay_frame_prefix(materialize, content);
	check(
		materialize.boss.state == 1U &&
		materialize.boss.pattern_frame == 0U &&
		materialize.boss.render_state == 15U &&
		materialize.boss.pattern_angle == 15U,
		"Chiyuri materialization enters the chooser through the common visual tail"
	);
	materialize.boss.render_state = 16U;
	materialize.boss.pattern_angle = 0U;
	materialize.randring[0] = 3U;
	materialize.randring[1] = 0U;
	(void)th03::detail::gameplay_frame_prefix(materialize, content);
	check(
		materialize.boss.center_x == (192 << 4) &&
		materialize.boss.center_y == (96 << 4) &&
		materialize.boss_target_x == (100 << 4) &&
		materialize.boss_target_y == ((70 + 128) << 4) &&
		materialize.randring_cursor == 1U,
		"Chiyuri teleports at transition 16 after publishing the old target position"
	);

	th03::detail::DeterministicState cardinals{};
	cardinals.players[1].paletted_character = 15;
	cardinals.gba_boss_launched_by = 1;
	cardinals.boss.state = 2U;
	cardinals.boss.pattern_frame = 0U;
	cardinals.boss.center_x = 144 << 4;
	cardinals.boss.center_y = 80 << 4;
	cardinals.boss.hp = 140U;
	cardinals.boss.params[0] = 48U;
	cardinals.boss.params[1] = 6U;
	cardinals.randring_cursor = 10U;
	(void)th03::detail::gameplay_frame_prefix(cardinals, content);
	check(
		cardinals.randring_cursor == 14U && active_bullets(cardinals) == 4 &&
		cardinals.chiyuri_boss_pattern_angle == 232U &&
		cardinals.bullets[0].angle_next == 232U &&
		cardinals.bullets[1].angle_next == 40U &&
		cardinals.bullets[2].angle_next == 104U &&
		cardinals.bullets[3].angle_next == 168U,
		"Chiyuri cardinal pattern consumes four effect draws and emits rotated pellets"
	);

	th03::detail::DeterministicState rings{};
	rings.players[1].paletted_character = 15;
	rings.gba_boss_launched_by = 1;
	rings.boss.state = 6U;
	rings.boss.pattern_frame = 0U;
	rings.boss.center_x = 144 << 4;
	rings.boss.center_y = 80 << 4;
	rings.boss.hp = 140U;
	rings.boss.params[2] = 52U;
	rings.boss.params[3] = 32U;
	rings.randring_cursor = 20U;
	rings.randring[21] = 0x35U;
	(void)th03::detail::gameplay_frame_prefix(rings, content);
	check(
		rings.randring_cursor == 22U && active_bullets(rings) == 32 &&
		rings.bullets[0].angle_next == 0x35U &&
		rings.bullets[31].angle_next == 0x2DU,
		"Chiyuri low-level ring consumes effect then angle and emits exactly 32 bullets"
	);

	th03::detail::DeterministicState trail{};
	trail.players[1].paletted_character = 15;
	trail.gba_boss_launched_by = 1;
	trail.boss.state = 10U;
	trail.boss.pattern_frame = 47U;
	trail.boss.center_x = 144 << 4;
	trail.boss.center_y = 80 << 4;
	trail.boss.hp = 140U;
	trail.boss.params[4] = 56U;
	trail.randring_cursor = 30U;
	trail.randring[31] = 0x34U;
	(void)th03::detail::gameplay_frame_prefix(trail, content);
	check(
		trail.randring_cursor == 32U && active_bullets(trail) == 48 &&
		trail.boss.state == 1U && trail.boss.pattern_frame == 0U &&
		trail.boss.render_state == 31U && trail.bullet_trail_cursor == 0U &&
		std::all_of(
			trail.bullets.begin(), trail.bullets.begin() + 48,
			[](const auto &bullet) { return bullet.has_trail; }
		),
		"Chiyuri trail pattern preserves both draws, 48 trails, and same-frame reset"
	);

	th03::detail::DeterministicState lattice{};
	lattice.hud_start_flag = 0U;
	lattice.players[1].paletted_character = 15;
	lattice.gba_boss_launched_by = 1;
	lattice.boss.state = 14U;
	lattice.boss.pattern_frame = 31U;
	lattice.boss.center_x = 144 << 4;
	lattice.boss.center_y = 80 << 4;
	lattice.boss.hp = 140U;
	lattice.boss.params[5] = 16U;
	lattice.boss.params[6] = 12U;
	lattice.randring_cursor = 40U;
	(void)th03::detail::gameplay_frame_prefix(lattice, content);
	check(
		lattice.randring_cursor == 42U && active_bullets(lattice) == 48 &&
		lattice.bullets[0].center_x == (88 << 4) &&
		lattice.bullets[0].angle_next == 0x60U &&
		lattice.bullets[1].angle_next == 0xA0U &&
		lattice.bullets[0].speed_next == 16U &&
		lattice.bullets[24].speed_next == 40U,
		"Chiyuri horizontal lattice carries speed into its descending second pass"
	);
}

void test_kotohime_boss_materialization_renderer_state()
{
	th03::detail::DeterministicState state{};
	state.players[0].paletted_character = 9;
	state.gba_boss_launched_by = 0;
	state.boss.state = 0U;
	state.background_phase = th03::detail::BackgroundPhase::type_a_steady;

	for(std::uint16_t frame = 1U; frame < 100U; ++frame) {
		state.boss.pattern_frame = frame;
		state.round_frame = frame;
		th03::detail::gameplay_frame_suffix_after_interstitial(state);
	}
	check(
		state.boss.pattern_angle == 134U &&
		state.boss.anchor_x == (2 << 4),
		"Kotohime materialization renderer preserves its +134 orbit angle and final radius"
	);

	state.boss.pattern_angle = 10U;
	state.boss.anchor_x = 123;
	state.boss.pattern_frame = 63U;
	state.round_frame = 1U;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.boss.pattern_angle == 10U && state.boss.anchor_x == 123,
		"Kotohime materialization renderer skips odd pre-64 frames"
	);
}

void test_yumemi_boss_patterns_and_rng()
{
	const th03::GameContent content{};
	const auto active_bullets = [](const auto &state) {
		return std::count_if(
			state.bullets.begin(), state.bullets.end(),
			[](const auto &bullet) {
				return bullet.flag != th03::detail::BulletFlag::free;
			}
		);
	};

	th03::GameInit init{};
	init.paletted_character = {1, 17};
	check(
		th03::detail::initialize_first_round(init).boss_initial[1].hp == 0x82U,
		"Yumemi Boss Attack initializes with Anniversary HP 0x82"
	);

	th03::detail::DeterministicState materialize{};
	materialize.players[1].paletted_character = 17;
	materialize.gba_boss_launched_by = 1;
	materialize.boss.state = 0U;
	materialize.boss.pattern_frame = 94U;
	materialize.boss.center_x = 144 << 4;
	materialize.boss.center_y = 80 << 4;
	materialize.boss.hp = 0x82U;
	materialize.yumemi_boss_radius = 64U;
	(void)th03::detail::gameplay_frame_prefix(materialize, content);
	check(
		materialize.boss.state == 0U &&
		materialize.boss.pattern_frame == 95U &&
		materialize.yumemi_boss_radius == 64U,
		"Yumemi materialization skips the common tail through frame 95"
	);
	(void)th03::detail::gameplay_frame_prefix(materialize, content);
	check(
		materialize.boss.state == 1U &&
		materialize.boss.pattern_frame == 0U &&
		materialize.yumemi_boss_radius == 56U,
		"Yumemi materialization enters the common tail exactly on frame 96"
	);

	th03::detail::DeterministicState accel{};
	accel.hud_start_flag = 0U;
	accel.players[1].paletted_character = 17;
	accel.gba_boss_launched_by = 1;
	accel.boss.state = 2U;
	accel.boss.pattern_frame = 0U;
	accel.boss.center_x = 144 << 4;
	accel.boss.center_y = 80 << 4;
	accel.boss.hp = 0x82U;
	accel.boss.params[0] = 16U;
	accel.boss.params[1] = 16U;
	accel.randring_cursor = 10U;
	accel.randring[10] = 0x20U;
	accel.randring[11] = 0U;
	(void)th03::detail::gameplay_frame_prefix(accel, content);
	check(
		accel.randring_cursor == 13U &&
		accel.yumemi_boss_angle == 0x20U &&
		accel.yumemi_boss_angle_delta == 3 &&
		accel.yumemi_boss_radius == 248U,
		"Yumemi accelerating rings initialize with two draws plus frame-1 effect"
	);
	accel.boss.pattern_frame = 31U;
	(void)th03::detail::gameplay_frame_prefix(accel, content);
	check(
		active_bullets(accel) == 16 &&
		accel.bullets[0].center_x == (153 << 4) &&
		accel.bullets[0].center_y == (56 << 4) &&
		accel.bullets[0].angle_next == 0x20U &&
		accel.yumemi_boss_angle == 0x23U &&
		accel.yumemi_boss_speed_offset == 8U,
		"Yumemi accelerating ring fires from the muzzle before advancing state"
	);

	th03::detail::DeterministicState groups{};
	groups.players[1].paletted_character = 17;
	groups.gba_boss_launched_by = 1;
	groups.boss.state = 7U;
	groups.boss.pattern_frame = 31U;
	groups.boss.center_x = 144 << 4;
	groups.boss.center_y = 80 << 4;
	groups.boss.hp = 0x82U;
	groups.boss.params[2] = 64U;
	groups.yumemi_boss_angle = 0x40U;
	groups.randring_cursor = 20U;
	groups.randring[20] = 5U;
	groups.randring[21] = 1U;
	groups.randring[22] = 4U;
	groups.randring[23] = 0U;
	(void)th03::detail::gameplay_frame_prefix(groups, content);
	check(
		groups.randring_cursor == 23U && active_bullets(groups) == 1 &&
		groups.bullets[0].flag == th03::detail::BulletFlag::bullet16 &&
		groups.bullets[0].speed_next == 69U &&
		groups.bullets[0].group_next == 0U,
		"Yumemi random group preserves speed, type, then group draw order"
	);

	th03::detail::DeterministicState crosses{};
	crosses.players[0].center_x =
		th03::exact::Subpixel::from_integer_wrapping(120);
	crosses.players[0].center_y =
		th03::exact::Subpixel::from_integer_wrapping(240);
	crosses.players[1].paletted_character = 17;
	crosses.gba_boss_launched_by = 1;
	crosses.boss.state = 15U;
	crosses.boss.pattern_frame = 46U;
	crosses.boss.pattern_angle = 1U;
	crosses.boss.center_x = 56 << 4;
	crosses.boss.center_y = 80 << 4;
	crosses.boss.velocity_x = 32;
	crosses.boss.hp = 0x82U;
	crosses.boss.params[4] = 1U;
	(void)th03::detail::gameplay_frame_prefix(crosses, content);
	check(
		crosses.randring_cursor == 1U && crosses.boss.state == 15U &&
		crosses.boss.pattern_frame == 47U &&
		crosses.extra_attacks[1][8].state == 3U &&
		crosses.extra_attacks[1][8].center_x == (120 << 4) &&
		crosses.extra_attacks[1][8].center_y == (240 << 4) &&
		crosses.extra_attacks[1][8].yumemi_duration == 0x20U,
		"Yumemi cross spawn consumes its effect and returns before timeout"
	);
	(void)th03::detail::gameplay_frame_prefix(crosses, content);
	check(
		crosses.boss.state == 1U && crosses.boss.pattern_frame == 0U &&
		crosses.boss.pattern_angle == 0U,
		"Yumemi cross pattern applies its strict timeout on the following frame"
	);

	th03::detail::DeterministicState rain{};
	rain.hud_start_flag = 0U;
	rain.players[1].paletted_character = 17;
	rain.gba_boss_launched_by = 1;
	rain.boss.state = 5U;
	rain.boss.pattern_frame = 31U;
	rain.boss.center_x = 144 << 4;
	rain.boss.center_y = 80 << 4;
	rain.boss.velocity_x = -32;
	rain.boss.hp = 0x82U;
	rain.boss.params[5] = 8U;
	(void)th03::detail::gameplay_frame_prefix(rain, content);
	check(
		rain.randring_cursor == 24U && active_bullets(rain) == 21 &&
		rain.boss.center_x == ((144 << 4) - 32) &&
		rain.bullets[0].center_x == (153 << 4) &&
		rain.bullets[0].center_y == (56 << 4) &&
		rain.bullets[9].center_x == 0 &&
		rain.bullets[9].center_y == 0 && rain.bullets[9].angle_next == 0x20U,
		"Yumemi rain uses the pre-move muzzle then consumes 24 late-shot draws"
	);
}

void test_legacy_player_hittest_alias()
{
	th03::detail::DeterministicState state{};
	state.players[0].center_x = th03::exact::Subpixel::from_raw(0x0480);
	state.players[0].center_y = th03::exact::Subpixel::from_raw(0x11A0);
	std::int16_t collision_x = -1;
	std::int16_t collision_y = -1;
	for(const auto size : {32U, 48U, 64U}) {
		check(
			!th03::detail::player_hittest_legacy(
				state,
				0U,
				static_cast<std::uint8_t>(size),
				collision_x,
				collision_y
			),
			"TH03 P1 CPU's narrower probes miss before the broad alias"
		);
	}
	check(
		th03::detail::player_hittest_legacy(
			state, 0U, 80U, collision_x, collision_y
		),
		"TH03 P1 broad CPU probe preserves the wrapped sine-table collision"
	);
	check(
		collision_x == 0 && collision_y == 0x0CA0,
		"TH03 P1 wrapped collision reports the first nonnegative map column"
	);

	state.players[1].center_x = th03::exact::Subpixel::from_raw(0x0420);
	state.players[1].center_y = th03::exact::Subpixel::from_raw(0x1120);
	collision_x = -1;
	collision_y = -1;
	check(
		th03::detail::player_hittest_legacy(
			state, 1U, 80U, collision_x, collision_y
		),
		"TH03 broad CPU probe preserves the wrapped DGROUP collision"
	);
	check(
		collision_x == 0 && collision_y == 0x0C20,
		"TH03 wrapped collision reports the first nonnegative map column"
	);

	state.players[1].center_x = th03::exact::Subpixel::from_raw(1204);
	state.players[1].center_y = th03::exact::Subpixel::from_raw(5632);
	collision_x = -1;
	collision_y = -1;
	check(
		th03::detail::player_hittest_legacy(
			state, 1U, 80U, collision_x, collision_y
		),
		"TH03 Stage 3 broad CPU probe preserves the wrapped DGROUP collision"
	);
}

void test_hyper_callback_transition_delay()
{
	th03::detail::DeterministicState state{};
	auto &player = state.players[1];
	player.is_cpu = 1U;
	player.paletted_character = 11;
	player.center_x = th03::exact::Subpixel::from_integer_wrapping(200);
	player.center_y = th03::exact::Subpixel::from_integer_wrapping(300);
	player.speed_aligned_x = 56U;
	player.speed_aligned_y = 56U;
	player.speed_diagonal_x = 44U;
	player.speed_diagonal_y = 44U;
	player.hyper_active = 11U;
	player.gauge_avail = 128U << 4U;
	player.shot_active = 0U;

	(void)th03::detail::gameplay_frame_suffix_before_interstitial(state);
	check(
		player.center_x.raw() == ((200 << 4) - 56),
		"Hyper standby switches callbacks without applying the speed boost"
	);
	(void)th03::detail::gameplay_frame_suffix_before_interstitial(state);
	check(
		player.center_x.raw() == ((200 << 4) - 56 - 88),
		"Hyper character callback applies its speed boost on the following update"
	);
}

void test_active_hyper_releases_pending_charge()
{
	th03::detail::DeterministicState state{};
	auto &player = state.players[0];
	player.paletted_character = 7;
	player.hyper_active = 7U;
	player.hyper_character_callback = false;
	player.gauge_charged = 64U << 4U;
	player.gauge_avail = 255U << 4U;
	state.players[1].lose_anim_time = 0xFFU;
	state.autofire_shot_gate[0] = 0U;

	(void)th03::detail::gameplay_frame_suffix_before_interstitial(state);
	check(
		player.gauge_charged == 0U &&
		player.gauge_avail == ((255U << 4U) - 6U) &&
		state.ellen_charge[0][0].state == 0U &&
		state.ellen_charge[0][1].state == 2U &&
		state.ellen_charge[0][31].state == 62U &&
		state.randring_cursor == 32U,
		"An active hyper falls through the <=2 shot gate and releases the pending Ellen charge"
	);
}

void test_yumemi_chargeshot_lifecycle_and_collision()
{
	th03::detail::DeterministicState state{};
	auto &player = state.players[0];
	player.paletted_character = 17;
	player.center_x = th03::exact::Subpixel::from_integer_wrapping(120);
	player.center_y = th03::exact::Subpixel::from_integer_wrapping(240);
	player.gauge_charged = 64U << 4U;
	player.gauge_avail = 64U << 4U;
	state.autofire_shot_gate[0] = 3U;

	(void)th03::detail::gameplay_frame_suffix_before_interstitial(state);
	check(
		state.yumemi_charge[0].state == 1U &&
		state.yumemi_charge[0].frame == 0U &&
		state.yumemi_charge[0].x == (120 << 4) &&
		state.yumemi_charge[0].y == (240 << 4) &&
		player.gauge_charged == 0U,
		"Yumemi releases a state-1 charge at her raw player center"
	);

	const th03::GameContent content{};
	state.yumemi_charge[0].frame = 0x3FU;
	state.yumemi_charge[0].y = 100 << 4;
	player.gauge_charged = 123U;
	(void)th03::detail::gameplay_frame_prefix(state, content);
	check(
		state.yumemi_charge[0].state == 2U &&
		state.yumemi_charge[0].frame == 0U &&
		state.yumemi_charge[0].y == ((100 << 4) - 0x40) &&
		player.gauge_charged == 0U,
		"Yumemi pre-increments frame 0x3F, moves, and enters state 2"
	);
	state.yumemi_charge[0].frame = 0x3FU;
	player.gauge_charged = 123U;
	(void)th03::detail::gameplay_frame_prefix(state, content);
	check(
		state.yumemi_charge[0].state == 0U &&
		state.yumemi_charge[0].frame == 0x40U &&
		player.gauge_charged == 0U,
		"Yumemi state 2 clears after its pre-incremented frame reaches 0x40"
	);

	auto &shot = state.yumemi_charge[0];
	shot.state = 1U;
	shot.frame = 17U;
	shot.x = 100 << 4;
	shot.y = 100 << 4;
	auto &fireball = state.fireballs[0];
	fireball.flag = th03::detail::FireballFlag::fall;
	fireball.player_id = 0U;
	fireball.center_x = shot.x;
	fireball.center_y = static_cast<std::int16_t>(shot.y - (3 << 4));
	fireball.hp = 2U;
	state.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	state.randring_cursor = 11U;

	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		shot.state == 2U && shot.frame == 0U &&
		fireball.flag == th03::detail::FireballFlag::fall &&
		state.randring_cursor == 11U,
		"Yumemi state-1 collision detonates but reports a miss without RNG"
	);
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		fireball.flag == th03::detail::FireballFlag::fall && fireball.hp == 1U,
		"Yumemi state-2 collision damages the overlapping fireball"
	);
	check(
		state.randring_cursor == 13U,
		"Yumemi state-2 collision consumes two hit-circle RNG reads"
	);
}

void test_yumemi_extra_attack_rng_argument_order()
{
	th03::detail::DeterministicState state{};
	state.players[0].paletted_character = 17;
	state.round_speed = 0x50U;
	state.randring_cursor = 0U;
	state.randring[0] = 0xE3U;
	state.randring[1] = 0x1AU;
	state.randring[2] = 0xFEU;

	auto &fireball = state.fireballs.back();
	fireball.flag = th03::detail::FireballFlag::fall;
	fireball.center_x = 3840;
	fireball.center_y = 2701;
	fireball.player_id = 0U;
	fireball.hp = 0U;
	fireball.variant_as_explosion_target = 2U;
	state.chains.charge_exatt[0][0] = 4U;

	auto &shot = state.shotpairs[0];
	shot.alive = true;
	shot.player_id = 0U;
	shot.topleft_x = static_cast<std::int16_t>(fireball.center_x - (16 << 4));
	shot.topleft_y = static_cast<std::int16_t>(fireball.center_y - (8 << 4));

	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	const auto &attack = state.extra_attacks[0][0];
	check(
		attack.state == 2U && attack.target_x_for_target_player == 2275 &&
		attack.velocity_x == 73 && attack.velocity_y == 51 &&
		attack.target_x_for_origin_player == 7392 &&
		state.randring_cursor == 4U,
		"Yumemi extra attack assigns consecutive ring reads to X then Y"
	);
}

void test_story_result_route()
{
	th03::detail::DeterministicState state{};
	state.game_mode = 1U;
	state.winner = 0;
	state.last_fatal_player = 1U;
	state.defeat_flag = th03::detail::DefeatFlag::banner;
	state.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	state.players[0].combo_hits_max = 5U;
	state.players[0].gauge_attacks_fired = 1U;
	state.players[0].boss_attacks_fired = 2U;
	state.players[0].boss_attacks_reversed = 3U;
	state.players[0].boss_panics_fired = 4U;

	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(state.round_or_result_frame == 1U,
		"Story result frame zero advances without initializing the bonus");
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.result_bonus_remaining == (225000U - 1171U),
		"Story result frame one snapshots counters and adds one 1/192 bonus slice"
	);
	while(state.round_or_result_frame < 320U) {
		th03::detail::gameplay_frame_suffix_after_interstitial(state);
	}
	check(
		state.score[0] == std::array<std::uint8_t, 8>{0U, 0U, 0U, 5U, 2U, 2U, 0U, 0U},
		"Story winner bonus drains exactly into the decimal score"
	);
	for(std::size_t frame = 0U; frame < 16U; ++frame) {
		th03::detail::gameplay_frame_suffix_after_interstitial(state);
	}
	check(
		state.round_or_result_frame == 336U &&
		state.result_phase == th03::detail::ResultPhase::closing &&
		state.result_transition_complete,
		"Story result opening completes after exactly sixteen callbacks"
	);
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.route == 2U && state.round_or_result_frame == 336U,
		"Story P1 win returns route 2 before another gameplay-frame increment"
	);
}

void test_result_banner_suppresses_extra_attack_hittest()
{
	th03::detail::DeterministicState state{};
	state.defeat_flag = th03::detail::DefeatFlag::banner;
	state.hud_start_flag = 1U;
	state.players[0].paletted_character = 9;

	auto &attack = state.extra_attacks[0][0];
	attack.state = 1U;
	attack.center_x = 144 << 4;
	attack.center_y = 100 << 4;
	attack.kotohime_target_y = 200 << 4;

	auto &shot = state.shotpairs[0];
	shot.alive = true;
	shot.player_id = 1U;
	shot.topleft_x = static_cast<std::int16_t>(
		attack.center_x - (16 << 4)
	);
	shot.topleft_y = static_cast<std::int16_t>(
		attack.center_y - (8 << 4)
	);

	(void)th03::detail::gameplay_frame_prefix(state, th03::GameContent{});
	check(
		shot.alive && state.randring_cursor == 0U,
		"result banner suppresses the shared Extra Attack hitbox path"
	);
}

void test_story_loss_and_versus_result_routes()
{
	th03::detail::DeterministicState story{};
	story.game_mode = 1U;
	story.winner = 1;
	story.story_lives = 2U;
	story.defeat_flag = th03::detail::DefeatFlag::banner;
	story.result_transition_complete = true;
	story.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	th03::detail::gameplay_frame_suffix_after_interstitial(story);
	check(
		story.route == 1U && story.story_lives == 1U,
		"A completed Story loss consumes one reserve life and returns route 1"
	);
	story.route = 0U;
	story.round_complete = false;
	story.story_lives = 1U;
	th03::detail::gameplay_frame_suffix_after_interstitial(story);
	check(
		story.route == 1U && story.story_lives == 0U,
		"A Story retry may consume the final reserve life without underflow"
	);
	story.route = 0U;
	story.round_complete = false;
	th03::detail::gameplay_frame_suffix_after_interstitial(story);
	check(
		story.route == 2U && story.story_lives == 0U,
		"A Story loss at zero reserve lives returns the game-over route"
	);

	th03::detail::DeterministicState extended{};
	extended.game_mode = 1U;
	extended.winner = 1;
	extended.story_lives = 0U;
	extended.extends_gained = 0U;
	extended.score[0][6] = 1U;
	extended.defeat_flag = th03::detail::DefeatFlag::banner;
	extended.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	th03::detail::gameplay_frame_suffix_after_interstitial(extended);
	check(
		extended.story_lives == 1U && !extended.round_complete,
		"result-time score extension is awarded before the route gate"
	);
	extended.result_transition_complete = true;
	th03::detail::gameplay_frame_suffix_after_interstitial(extended);
	check(
		extended.route == 1U && extended.story_lives == 0U,
		"the route gate consumes a result-time extend as one final retry"
	);

	th03::detail::DeterministicState versus{};
	versus.game_mode = th03::detail::game_mode_vs_1p_2p;
	versus.winner = 0;
	versus.players[0].rounds_won = 1U;
	versus.defeat_flag = th03::detail::DefeatFlag::banner;
	versus.result_transition_complete = true;
	versus.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	th03::detail::gameplay_frame_suffix_after_interstitial(versus);
	check(
		versus.route == 1U,
		"A completed non-final VS round returns route 1"
	);

	versus.route = 0U;
	versus.players[0].rounds_won = 2U;
	versus.result_bonus_remaining = 0U;
	versus.result_bonus_per_frame = 0U;
	th03::detail::gameplay_frame_suffix_after_interstitial(versus);
	check(
		versus.route == 2U,
		"A completed first-to-two VS match returns route 2"
	);
}

void test_simultaneous_defeat_bonus_owner()
{
	th03::detail::DeterministicState state{};
	state.game_mode = th03::detail::game_mode_vs_1p_cpu;
	state.winner = 1;
	state.last_fatal_player = 1U;
	state.players[1].rounds_won = 2U;
	state.players[0].combo_hits_max = 2U;
	state.players[1].combo_hits_max = 9U;
	state.defeat_flag = th03::detail::DefeatFlag::banner;
	state.round_or_result_frame = 1U;
	state.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.result_bonus_remaining == (2000U - 10U) &&
		state.winner == 1,
		"simultaneous defeat awards the bonus from last-fatal PID while preserving the independently completed round winner"
	);
}

void test_final_yumemi_result_bonus()
{
	th03::detail::DeterministicState state{};
	state.game_mode = 1U;
	state.story_stage = 8U;
	state.story_lives = 2U;
	state.winner = 0;
	state.last_fatal_player = 1U;
	state.defeat_flag = th03::detail::DefeatFlag::banner;
	state.round_or_result_frame = 1U;
	state.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.result_bonus_remaining == (200000U - 1041U) &&
		state.result_bonus_per_frame == 1041U &&
		state.extends_gained == 255U,
		"Final Yumemi result adds remaining lives and disables score extends"
	);
	state.round_or_result_frame = 460U;
	state.result_phase = th03::detail::ResultPhase::idle;
	state.result_phase_frame = 0U;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.result_phase == th03::detail::ResultPhase::opening &&
		state.result_phase_frame == 1U,
		"Final Yumemi result opens its transition at frame 460"
	);
}

void test_repeat_round_targeted_reset_and_rng_order()
{
	th03::GameInit init{};
	init.game_mode = th03::detail::game_mode_vs_1p_cpu;
	init.rank = 2U;
	init.is_cpu = {1U, 0U};
	init.paletted_character = {3, 16};

	th03::detail::DeterministicState state{};
	state.game_mode = init.game_mode;
	state.rank = init.rank;
	state.round_id = 7U;
	state.round_frame = 0x12345U;
	state.round_or_result_frame = 176U;
	state.route = 1U;
	state.round_complete = true;
	state.random_seed = 0x12345678U;
	state.randring_cursor = 73U;
	state.gba_gauge_level = {15U, 15U};
	state.result_phase = th03::detail::ResultPhase::opening;
	state.result_phase_frame = 16U;
	state.result_transition_complete = true;
	state.background_phase = th03::detail::BackgroundPhase::type_b_steady;
	state.chains.cursor = {5U, 9U};
	state.bomb_flag[0] = th03::detail::BombFlag::active;
	state.bomb_frame[0] = 91U;
	state.gba_flag_next[0] = th03::detail::GaugeBossAttackFlag::boss;
	state.gba_flag_active[0] = th03::detail::GaugeBossAttackFlag::boss;
	state.warning_flag[0] = th03::detail::WarningFlag::flash_red;
	state.players[0].gauge_avail = 1234U;
	state.players[1].gauge_avail = 2345U;
	state.players[0].rounds_won = 1U;
	state.players[0].combo_hits_max = 23U;
	state.players[0].cpu_safety_frames = 900U;
	state.players[1].cpu_safety_frames = 901U;
	state.players[0].speed_aligned_x = 72U;
	state.bullets[0].flag = th03::detail::BulletFlag::bullet16;
	state.bullets[0].center_x = 0x1234;
	state.enemies[0].flag = th03::detail::EnemyFlag::running_spawned;
	state.fireballs[0].flag = th03::detail::FireballFlag::fall;
	state.extra_attacks[0][0].state = 2U;
	state.extra_attacks[0][0].center_x = 0x2345;
	state.shotpairs[0].alive = true;
	state.reimu_charge[0][0].state = 1U;
	state.mima_charge[0][0].active = 1U;
	state.mima_charge_active[0] = 7U;
	state.marisa_charge[0].active = 1U;
	state.ellen_charge[0][0].state = 4U;
	state.kana_charge_state[0] = 6U;

	auto expected_seed = state.random_seed;
	auto expected_randring = state.randring;
	for(std::size_t index = expected_randring.size(); index-- > 0U;) {
		expected_randring[index] = static_cast<std::uint8_t>(
			th03::detail::irand(expected_seed)
		);
	}
	std::array<std::array<std::uint8_t, 64>, 2> expected_cpu_rings{};
	for(auto &ring : expected_cpu_rings) {
		for(auto &charge : ring) {
			charge = static_cast<std::uint8_t>(
				((th03::detail::irand(expected_seed) & 3U) << 6U) + 0x3FU
			);
		}
	}
	std::array<std::uint8_t, 256> expected_formations{};
	std::array<std::uint8_t, 256> expected_positions{};
	auto previous = std::uint8_t{1U};
	for(std::size_t index = 0U; index < expected_formations.size(); ++index) {
		std::uint8_t next = 0U;
		do {
			next = static_cast<std::uint8_t>(
				th03::detail::irand(expected_seed) % 3U
			);
		} while(next == previous);
		expected_formations[index] = next;
		previous = next;
		expected_positions[index] = static_cast<std::uint8_t>(
			(th03::detail::irand(expected_seed) & 1U) << 7U
		);
	}

	th03::detail::initialize_repeat_round(state, init, 3U);
	check(
		state.resident_rand == 0x12345 && state.round_id == 8U &&
		state.round_frame == 0U && state.round_or_result_frame == 0U &&
		state.route == 0U && !state.round_complete,
		"route 1 persists outgoing round time, increments round ID, and reopens the same Game"
	);
	check(
		state.randring_cursor == 73U && state.randring == expected_randring &&
		state.random_seed == expected_seed &&
		state.players[0].cpu_charge_ring == expected_cpu_rings[0] &&
		state.players[1].cpu_charge_ring == expected_cpu_rings[1] &&
		state.formation_type_ring == expected_formations &&
		state.formation_position_ring == expected_positions,
		"route-1 RNG follows refill, P1/P2 CPU rings, and formation randomization without resetting the TH03 ring cursor"
	);
	check(
		state.players[0].gauge_avail == 1234U &&
		state.players[1].gauge_avail == 2345U &&
		state.players[0].rounds_won == 1U &&
		state.players[0].combo_hits_max == 23U &&
		state.chains.cursor == std::array<std::uint8_t, 2>{5U, 9U} &&
		state.bomb_flag[0] == th03::detail::BombFlag::active &&
		state.bomb_frame[0] == 91U &&
		state.gba_flag_next[0] == th03::detail::GaugeBossAttackFlag::boss,
		"route 1 carries match-wide, chain, gauge-availability, and dormant attack state"
	);
	check(
		state.players[0].halfhearts == 10U &&
		state.players[0].invincibility_time == 50U &&
		state.players[0].gauge_charged == 0U &&
		state.players[0].bombs == 2U &&
		state.players[0].is_cpu == 1U &&
		state.players[0].paletted_character == 3 &&
		state.players[0].speed_aligned_x == 72U &&
		state.players[0].cpu_safety_frames == 900U,
		"route 1 resets per-round player fields while carrying callbacks, speeds, and VS CPU safety"
	);
	check(
		state.bullets[0].flag == th03::detail::BulletFlag::free &&
		state.bullets[0].center_x == 0x1234 &&
		state.enemies[0].flag == th03::detail::EnemyFlag::free &&
		state.fireballs[0].flag == th03::detail::FireballFlag::free &&
		state.extra_attacks[0][0].state == 0U &&
		state.extra_attacks[0][0].center_x == 0x2345 &&
		!state.shotpairs[0].alive,
		"route 1 clears entity active flags without blanket-zeroing their payloads"
	);
	check(
		state.reimu_charge[0][0].state == 0U &&
		state.mima_charge[0][0].active == 0U &&
		state.marisa_charge[0].active == 0U &&
		state.ellen_charge[0][0].state == 0U &&
		state.mima_charge_active[0] == 7U &&
		state.kana_charge_state[0] == 6U,
		"route 1 applies only the source's five targeted Charge/Extra reset helpers"
	);
	check(
		state.round_speed == 32U && state.bullet_base_speed == 8U &&
		state.gba_boss_level == 16U &&
		state.gba_gauge_level == std::array<std::uint8_t, 2>{16U, 16U} &&
		state.gba_flag_active[0] == th03::detail::GaugeBossAttackFlag::none &&
		state.warning_flag[0] == th03::detail::WarningFlag::none &&
		state.result_phase == th03::detail::ResultPhase::closing,
		"route 1 recomputes and clamps rank state, clears active warnings, and carries the closing transition callback"
	);
}

void test_demo_and_cpu_vs_cpu_route_zero()
{
	th03::GameInit init{};
	init.game_mode = th03::detail::game_mode_demo;
	init.demo_number = 1U;
	auto demo = th03::detail::initialize_first_round(init, 0U);
	check(
		demo.round_speed == 64U && demo.bullet_base_speed == 0U &&
		demo.gba_boss_level == 8U &&
		demo.gba_gauge_level == std::array<std::uint8_t, 2>{9U, 9U},
		"attract startup applies the source demo difficulty overrides"
	);
	demo.hud_start_flag = 0U;
	demo.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	demo.demo_frame = 6999U;
	th03::detail::gameplay_frame_suffix_after_interstitial(demo);
	check(
		demo.demo_frame == 7000U && !demo.round_complete,
		"attract mode reaches its 7000-frame gate before opening the exit transition"
	);
	th03::detail::gameplay_frame_suffix_after_interstitial(demo);
	check(
		demo.result_phase == th03::detail::ResultPhase::opening &&
		demo.result_phase_frame == 0U,
		"attract mode selects the opening transition one frame after reaching its gate"
	);
	for(std::size_t frame = 0U; frame < 16U; ++frame) {
		th03::detail::gameplay_frame_suffix_after_interstitial(demo);
	}
	check(
		demo.round_complete && demo.route == 0U &&
		demo.result_transition_complete,
		"attract mode returns source route 0 after all sixteen transition callbacks"
	);

	th03::detail::DeterministicState cpu_vs_cpu{};
	cpu_vs_cpu.game_mode = th03::detail::game_mode_vs_cpu_cpu;
	cpu_vs_cpu.players[0].is_cpu = 1U;
	cpu_vs_cpu.players[1].is_cpu = 1U;
	cpu_vs_cpu.input_system = 0x1000U;
	cpu_vs_cpu.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	(void)th03::detail::gameplay_frame_suffix_before_interstitial(cpu_vs_cpu);
	th03::detail::gameplay_frame_suffix_after_interstitial(cpu_vs_cpu);
	check(
		cpu_vs_cpu.round_complete && cpu_vs_cpu.route == 0U &&
		cpu_vs_cpu.round_frame == 1U,
		"CPU-vs-CPU Cancel completes the current frame and returns source route 0"
	);

	init.paletted_character = {1, 1};
	init.resident_rand = -123;
	th03::GameContent demo_content{};
	demo_content.enemy_formations.resize(2U);
	for(auto &formation : demo_content.enemy_formations) {
		formation.enemy_scripts.push_back({0U});
	}
	auto public_demo = th03::Game::create(init, std::move(demo_content));
	check(public_demo.has_value(), "valid Demo state creates a public Game");
	if(public_demo) {
		th03::StepResult result{};
		auto context = th03::InputContext::gameplay;
		do {
			result = public_demo->step({}, context);
			if(result.status == th03::StepStatus::phase_transition) {
				context = (context == th03::InputContext::gameplay)
					? th03::InputContext::interstitial
					: th03::InputContext::gameplay;
			}
		} while(
			result.status == th03::StepStatus::running ||
			result.status == th03::StepStatus::phase_transition
		);
		check(
			result.status == th03::StepStatus::terminal &&
			public_demo->canonical_state().resident_rand == -123,
			"terminal route remains observable before MAIN persists round time"
		);
		const auto outgoing_round_frame =
			public_demo->canonical_state().round_frame;
		check(
			public_demo->finish_main_process().has_value() &&
			public_demo->canonical_state().resident_rand ==
				static_cast<std::int32_t>(outgoing_round_frame),
			"MAIN finish persists outgoing round time for route zero"
		);
		check(
			!public_demo->finish_main_process(),
			"MAIN finish cannot be applied twice"
		);
	}
}

void test_late_pipeline_control_order()
{
	th03::detail::DeterministicState cpu_vs_cpu{};
	cpu_vs_cpu.players[0].is_cpu = 1U;
	cpu_vs_cpu.players[1].is_cpu = 1U;
	cpu_vs_cpu.background_phase =
		th03::detail::BackgroundPhase::type_a_steady;
	th03::detail::interstitial_input_latch(
		cpu_vs_cpu,
		th03::CoreInput{.system = 0x1000U}
	);
	for(std::size_t sample = 1U; sample < 27U; ++sample) {
		th03::detail::interstitial_input_latch(
			cpu_vs_cpu, th03::CoreInput{}
		);
	}
	th03::detail::gameplay_frame_suffix_after_interstitial(cpu_vs_cpu);
	check(
		cpu_vs_cpu.round_complete && cpu_vs_cpu.route == 0U &&
		cpu_vs_cpu.round_frame == 1U,
		"CPU-vs-CPU Cancel in any warning-interstitial sample survives later releases and exits after the outer frame"
	);

	th03::detail::DeterministicState result{};
	result.game_mode = 1U;
	result.winner = 0;
	result.last_fatal_player = 1U;
	result.defeat_flag = th03::detail::DefeatFlag::banner;
	result.round_or_result_frame = 320U;
	result.result_phase = th03::detail::ResultPhase::opening;
	result.result_phase_frame = 7U;
	result.background_phase =
		th03::detail::BackgroundPhase::type_a_steady;
	th03::detail::gameplay_frame_suffix_after_interstitial(result);
	check(
		result.result_phase == th03::detail::ResultPhase::opening &&
		result.result_phase_frame == 8U,
		"selecting the opening result callback preserves and advances its independent source phase byte"
	);
}

void test_main_and_mainl_session_transitions()
{
	th03::GameInit init{};
	init.game_mode = 1U;
	init.story_stage = 1U;
	init.story_lives = 2U;
	init.remaining_credits = 3U;
	init.paletted_character = {1, 3};
	init.story_opponents = {3, 5, 7, 9, 11, 13, 15, 17, 18};
	init.last_score[0][0] = 9U;
	th03::detail::DeterministicState state{};
	state.game_mode = 1U;
	state.story_stage = 1U;
	state.story_lives = 2U;
	state.winner = 0;
	state.route = 2U;
	state.round_complete = true;
	state.round_frame = 4321U;
	state.global_frame = 9000U;
	state.resident_rand = 4321;
	state.score[0] = {3U, 2U, 1U, 0U, 0U, 0U, 0U, 0U};

	auto next = th03::detail::build_session_transition(
		state, init, true
	);
	check(
		next && next->action == th03::SessionAction::start_gameplay &&
		next->resident.story_stage == 2U &&
		next->resident.paletted_character[1] == 7 &&
		next->resident.last_score[0] == state.score[0] &&
		next->resident.last_score[1] == std::array<std::uint8_t, 8>{} &&
		next->resident.resident_rand == 4321 &&
		!next->resident.runtime_snapshot,
		"ordinary Story handoff persists MAIN state and starts the next opponent"
	);

	state.winner = 1;
	state.story_stage = 4U;
	state.story_lives = 0U;
	init.story_stage = 4U;
	init.story_lives = 0U;
	init.remaining_credits = 2U;
	auto prompt = th03::detail::build_session_transition(
		state, init, true
	);
	check(
		prompt && prompt->action == th03::SessionAction::request_continue &&
		prompt->resident.story_stage == 5U &&
		prompt->resident.story_lives == 0U &&
		prompt->resident.last_score ==
			std::array<std::array<std::uint8_t, 8>, 2>{} &&
		prompt->high_score_registration &&
		prompt->high_score_registration->last_score[0] == state.score[0],
		"Story game over registers the live score before Continue clears it"
	);
	auto accepted = th03::detail::build_session_transition(
		state, init, true, th03::ContinueChoice::accept
	);
	check(
		accepted && accepted->action == th03::SessionAction::start_gameplay &&
		accepted->resident.story_stage == 4U &&
		accepted->resident.story_lives == 2U &&
		accepted->resident.remaining_credits == 1U &&
		accepted->resident.last_score ==
			std::array<std::array<std::uint8_t, 8>, 2>{},
		"Continue Yes restarts the lost stage with two lives and one fewer credit"
	);
	auto declined = th03::detail::build_session_transition(
		state, init, true, th03::ContinueChoice::decline
	);
	check(
		declined && declined->action == th03::SessionAction::exit_to_operator &&
		declined->resident.story_stage == 4U &&
		declined->resident.story_lives == 2U &&
		declined->resident.remaining_credits == 2U,
		"Continue No restores stage/lives without spending a credit before exit"
	);

	init.remaining_credits = 0U;
	auto no_credit = th03::detail::build_session_transition(
		state, init, true
	);
	check(
		no_credit &&
		no_credit->action == th03::SessionAction::exit_to_operator &&
		no_credit->resident.story_stage == 5U &&
		no_credit->resident.story_lives == 0U &&
		no_credit->resident.last_score ==
			std::array<std::array<std::uint8_t, 8>, 2>{},
		"zero-credit game over clears scores but takes Continue's early return"
	);

	init.remaining_credits = 3U;
	init.story_stage = 8U;
	state.story_stage = 8U;
	state.winner = 0;
	state.story_lives = 1U;
	auto ending = th03::detail::build_session_transition(
		state, init, true
	);
	check(
		ending && ending->action == th03::SessionAction::ending &&
		ending->resident.story_stage == 99U &&
		ending->resident.paletted_character[1] == init.paletted_character[1] &&
		ending->high_score_registration &&
		ending->high_score_registration->story_stage == 99U,
		"final Story handoff normalizes the terminal OOB opponent read and registers after stage 99"
	);

	init.game_mode = th03::detail::game_mode_vs_1p_2p;
	init.story_stage = 3U;
	state.game_mode = th03::detail::game_mode_vs_1p_2p;
	state.story_stage = 3U;
	state.winner = 1;
	state.score[1] = {7U, 6U, 5U, 4U, 3U, 2U, 1U, 0U};
	auto versus = th03::detail::build_session_transition(
		state, init, true
	);
	check(
		versus && versus->action == th03::SessionAction::exit_to_operator &&
		versus->resident.story_stage == 4U &&
		versus->resident.last_score[0] == std::array<std::uint8_t, 8>{} &&
		versus->resident.last_score[1] == state.score[1],
		"completed VS persists only its resident winner and retains MAIN's stage increment"
	);

	state.route = 0U;
	init.last_score[0][0] = 8U;
	auto aborted = th03::detail::build_session_transition(
		state, init, true
	);
	check(
		aborted && aborted->action == th03::SessionAction::exit_to_operator &&
		aborted->resident.story_stage == init.story_stage &&
		aborted->resident.last_score == init.last_score,
		"route zero exits without MAIN's stage increment or score persistence"
	);
	check(
		!th03::detail::build_session_transition(state, init, false),
		"session handoff rejects an unfinished MAIN process"
	);
}

void test_story_extends()
{
	th03::GameInit init{};
	init.game_mode = 1U;
	init.story_lives = 2U;
	init.last_score[0][6] = 1U;
	auto state = th03::detail::initialize_first_round(init);
	check(
		state.story_lives == 2U && state.extends_gained == 1U,
		"Story round derives its extend counter from the ten-million score digit"
	);
	state.background_phase = th03::detail::BackgroundPhase::type_a_steady;
	state.score[0][6] = 2U;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.story_lives == 3U && state.extends_gained == 255U,
		"Story score rendering awards the second extend and disables further awards"
	);
	state.score[0][6] = 3U;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.story_lives == 3U && state.extends_gained == 255U,
		"Disabled Story extends remain disabled above twenty million points"
	);
}

void test_hit_damage_progression()
{
	th03::detail::DeterministicState state{};
	state.round_or_result_frame = 1023U;
	state.players[0].hit_damage_next = 4U;
	state.players[1].hit_damage_next = 5U;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.players[0].hit_damage_next == 5U &&
		state.players[1].hit_damage_next == 6U,
		"Pending player damage rises toward the original six-half-heart cap"
	);
	state.round_or_result_frame = 2047U;
	th03::detail::gameplay_frame_suffix_after_interstitial(state);
	check(
		state.players[0].hit_damage_next == 6U &&
		state.players[1].hit_damage_next == 6U,
		"Pending player damage saturates at six half-hearts"
	);
}

void test_random()
{
	std::uint32_t seed = 0xE9B35FBAU;
	check(th03::detail::irand(seed) == 0x6AB1U && seed == 0x6AB17D83U,
		"IRAND first known answer matches master.lib");
	check(th03::detail::irand(seed) == 0x4E0AU && seed == 0xCE0AE620U,
		"IRAND high bit is masked only from its return value");

	th03::detail::DeterministicState state{};
	state.random_seed = 0xE9B35FBAU;
	th03::detail::randring_fill(state);
	check(state.random_seed == 0x21D09EBAU,
		"descending 256-byte randring fill consumes the exact RNG sequence");
	check(
		state.randring[255] == 0xB1U && state.randring[254] == 0x0AU &&
		state.randring[1] == 0xC5U && state.randring[0] == 0xD0U,
		"randring fill stores low return bytes from index 255 down to zero"
	);

	state.randring[254] = 0x34U;
	state.randring[255] = 0x12U;
	state.randring_cursor = 254U;
	check(th03::detail::randring_next16(state) == 0x1234U &&
		state.randring_cursor == 255U,
		"randring 16-bit read is little-endian and advances once");
	state.randring[255] = 0xABU;
	check(th03::detail::randring_next16(state) == 0x00ABU &&
		state.randring_cursor == 0U,
		"randring index 255 reproduces the adjacent wrapped cursor byte");
}

constexpr std::array<std::uint8_t, 256> iatan2_reference_table{{
	0,0,0,0,1,1,1,1,1,1,2,2,2,2,2,2,
	3,3,3,3,3,3,3,4,4,4,4,4,4,5,5,5,
	5,5,5,6,6,6,6,6,6,6,7,7,7,7,7,7,
	8,8,8,8,8,8,8,9,9,9,9,9,9,10,10,10,
	10,10,10,10,11,11,11,11,11,11,11,12,12,12,12,12,
	12,12,13,13,13,13,13,13,13,14,14,14,14,14,14,14,
	15,15,15,15,15,15,15,16,16,16,16,16,16,16,17,17,
	17,17,17,17,17,17,18,18,18,18,18,18,18,19,19,19,
	19,19,19,19,19,20,20,20,20,20,20,20,20,21,21,21,
	21,21,21,21,21,21,22,22,22,22,22,22,22,22,23,23,
	23,23,23,23,23,23,23,24,24,24,24,24,24,24,24,24,
	25,25,25,25,25,25,25,25,25,25,26,26,26,26,26,26,
	26,26,26,27,27,27,27,27,27,27,27,27,27,28,28,28,
	28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29,
	29,29,29,30,30,30,30,30,30,30,30,30,30,30,31,31,
	31,31,31,31,31,31,31,31,31,31,32,32,32,32,32,32,
}};

enum class Iatan2MagnitudeOrder {
	master_signed,
	native_normalized,
};

std::uint16_t iatan2_reference_magnitude(std::int16_t value)
{
	const auto bits = std::bit_cast<std::uint16_t>(value);
	return (value < 0)
		? static_cast<std::uint16_t>(0U - bits)
		: bits;
}

std::optional<std::uint8_t> iatan2_reference(
	std::int16_t y,
	std::int16_t x,
	Iatan2MagnitudeOrder order
)
{
	if((std::bit_cast<std::uint16_t>(y) |
		std::bit_cast<std::uint16_t>(x)) == 0U) {
		return std::uint8_t{0U};
	}
	const auto abs_y = iatan2_reference_magnitude(y);
	const auto abs_x = iatan2_reference_magnitude(x);
	std::uint8_t angle = 0U;
	if(abs_x == abs_y) {
		angle = 32U;
	} else {
		const auto x_is_larger = (order == Iatan2MagnitudeOrder::master_signed)
			? (std::bit_cast<std::int16_t>(abs_x) >
				std::bit_cast<std::int16_t>(abs_y))
			: (abs_x > abs_y);
		const auto numerator = x_is_larger ? abs_y : abs_x;
		const auto denominator = x_is_larger ? abs_x : abs_y;
		if(denominator == 0U) {
			return std::nullopt;
		}
		const auto dividend = static_cast<std::uint32_t>(numerator) << 8U;
		const auto quotient = dividend / denominator;
		if(quotient > std::numeric_limits<std::uint16_t>::max()) {
			return std::nullopt;
		}
		// DIV returns the quotient in AX; XLATB indexes with AL only.
		const auto table_index = static_cast<std::uint8_t>(quotient);
		angle = x_is_larger
			? iatan2_reference_table[table_index]
			: static_cast<std::uint8_t>(
				64U - iatan2_reference_table[table_index]
			);
	}
	if(x < 0) {
		angle = static_cast<std::uint8_t>(128U - angle);
	}
	if(y < 0) {
		angle = static_cast<std::uint8_t>(0U - angle);
	}
	return angle;
}

void test_iatan2_numeric_policy()
{
	constexpr std::array<std::int16_t, 7> anchors{
		-32767, -256, -1, 0, 1, 256, 32767,
	};
	bool defined_domain_matches = true;
	for(std::int32_t raw = -32767; raw <= 32767; ++raw) {
		const auto value = static_cast<std::int16_t>(raw);
		for(const auto anchor : anchors) {
			const auto yx = iatan2_reference(
				value, anchor, Iatan2MagnitudeOrder::master_signed
			);
			const auto xy = iatan2_reference(
				anchor, value, Iatan2MagnitudeOrder::master_signed
			);
			if(
				!yx || !xy ||
				th03::detail::iatan2(value, anchor) != *yx ||
				th03::detail::iatan2(anchor, value) != *xy
			) {
				defined_domain_matches = false;
				break;
			}
		}
		if(!defined_domain_matches) {
			break;
		}
	}
	check(
		defined_domain_matches,
		"iatan2 matches the literal 16-bit assembly model for every defined word against sign, zero, and magnitude boundaries"
	);

	bool every_table_index_matches = true;
	for(std::uint16_t index = 0U; index < 256U; ++index) {
		const auto smaller = static_cast<std::int16_t>(index);
		for(const auto sign_y : std::array<std::int16_t, 2>{1, -1}) {
			for(const auto sign_x : std::array<std::int16_t, 2>{1, -1}) {
				const auto y = static_cast<std::int16_t>(smaller * sign_y);
				const auto x = static_cast<std::int16_t>(256 * sign_x);
				const auto expected = iatan2_reference(
					y, x, Iatan2MagnitudeOrder::master_signed
				);
				const auto swapped = iatan2_reference(
					x, y, Iatan2MagnitudeOrder::master_signed
				);
				if(
					!expected || !swapped ||
					th03::detail::iatan2(y, x) != *expected ||
					th03::detail::iatan2(x, y) != *swapped
				) {
					every_table_index_matches = false;
					break;
				}
			}
		}
	}
	check(
		every_table_index_matches,
		"iatan2 exercises every master.lib atan-table index in both axis orders and all quadrants"
	);

	constexpr auto int16_min = std::numeric_limits<std::int16_t>::min();
	bool landmine_range_matches = true;
	bool normalization_matches = true;
	for(std::int32_t raw = -32767; raw <= 32767; ++raw) {
		const auto other = static_cast<std::int16_t>(raw);
		const auto other_magnitude = iatan2_reference_magnitude(other);
		const auto source_y = iatan2_reference(
			int16_min, other, Iatan2MagnitudeOrder::master_signed
		);
		const auto source_x = iatan2_reference(
			other, int16_min, Iatan2MagnitudeOrder::master_signed
		);
		const auto source_should_fault = (other_magnitude <= 128U);
		if(
			source_y.has_value() == source_should_fault ||
			source_x.has_value() == source_should_fault
		) {
			landmine_range_matches = false;
		}
		const auto normalized_y = iatan2_reference(
			int16_min, other, Iatan2MagnitudeOrder::native_normalized
		);
		const auto normalized_x = iatan2_reference(
			other, int16_min, Iatan2MagnitudeOrder::native_normalized
		);
		if(
			!normalized_y || !normalized_x ||
			th03::detail::iatan2(int16_min, other) != *normalized_y ||
			th03::detail::iatan2(other, int16_min) != *normalized_x
		) {
			normalization_matches = false;
		}
	}
	check(
		landmine_range_matches,
		"literal iatan2.asm faults only when its signed-CMP landmine gives DIV a denominator magnitude at most 128"
	);
	check(
		normalization_matches,
		"native iatan2 gives every single-INT16_MIN input the stable unsigned-magnitude normalization"
	);
	const auto both_min = iatan2_reference(
		int16_min, int16_min, Iatan2MagnitudeOrder::master_signed
	);
	check(
		both_min && th03::detail::iatan2(int16_min, int16_min) == *both_min,
		"equal INT16_MIN magnitudes retain master.lib's defined diagonal result"
	);
}

void test_trig()
{
	check(th03::detail::cosine8(0U) == 256 &&
		th03::detail::sine8(0U) == 0,
		"master.lib integer trig begins at (256, 0)");
	check(th03::detail::cosine8(64U) == 0 &&
		th03::detail::sine8(64U) == 256,
		"master.lib integer trig preserves the 90-degree cardinal");
	check(th03::detail::cosine8(129U) == -256 &&
		th03::detail::sine8(193U) == -256,
		"master.lib integer trig preserves repeated negative extrema");
	const auto negative = th03::detail::vector2(128U, 1);
	check(negative.x == -1 && negative.y == 0,
		"vector2 arithmetic shift preserves negative rounding quirk");
	check(
		th03::detail::iatan2(0, 1) == 0U &&
		th03::detail::iatan2(1, 0) == 64U &&
		th03::detail::iatan2(0, -1) == 128U &&
		th03::detail::iatan2(-1, 0) == 192U,
		"master.lib iatan2 preserves cardinal angles"
	);
	check(
		th03::detail::iatan2(1, 1) == 32U &&
		th03::detail::iatan2(-1, -1) == 160U,
		"master.lib iatan2 preserves diagonal quadrants"
	);
	test_iatan2_numeric_policy();
}

void test_t3case()
{
	const std::array<std::byte, 1> a{std::byte{'a'}};
	check(th03::formats::fnv1a32({}) == 0x811C9DC5U, "FNV-1a empty known answer");
	check(th03::formats::fnv1a32(a) == 0xE40C292CU, "FNV-1a 'a' known answer");

	auto direct = make_t3case(false);
	const auto parsed = th03::formats::parse_t3case(direct);
	check(parsed.has_value(), "direct T3CASE parses");
	if(parsed) {
		check(parsed->header.sample_count == 1U && parsed->records.size() == 2U, "direct T3CASE counts");
		const auto session = th03::formats::make_replay_session(*parsed);
		check(session && session->records.size() == 2U, "direct T3CASE adapts to neutral replay session");
	}

	auto normalized = make_t3case(true);
	const auto normalized_parsed = th03::formats::parse_t3case(normalized);
	check(normalized_parsed.has_value(), "normalized V11 T3CASE parses");
	if(normalized_parsed && normalized_parsed->init.runtime_snapshot) {
		check(normalized_parsed->init.random_seed == 0x89ABCDEFU, "high-bit seed remains unsigned bits");
		check(normalized_parsed->init.runtime_snapshot->players[0].center_x.raw_bits() == 0x1234U, "snapshot Q12.4 position preserves raw bits");
	} else {
		check(false, "normalized T3CASE exposes runtime snapshot");
	}
	auto normalized_v12 = normalized;
	put_u8(
		normalized_v12,
		32U,
		static_cast<std::uint8_t>(
			th03::formats::T3CaseSourceKind::normalized_v12
		)
	);
	refresh_t3case(normalized_v12);
	const auto normalized_v12_parsed =
		th03::formats::parse_t3case(normalized_v12);
	check(normalized_v12_parsed.has_value(), "normalized V12 T3CASE parses");
	if(normalized_v12_parsed) {
		const auto session =
			th03::formats::make_replay_session(*normalized_v12_parsed);
		check(
			session && session->source_version == 12U &&
			session->init.runtime_snapshot.has_value(),
			"normalized V12 T3CASE exposes its source version and runtime snapshot"
		);
	}

	auto bad = direct;
	put_u8(bad, 0U, 'X');
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects bad magic");
	bad = direct;
	put_u16(bad, 8U, 2U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects unsupported version");
	bad = direct;
	put_u16(bad, 38U, 0x0004U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects retired flag bit");
	bad = direct;
	put_u8(bad, 32U, 4U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects unknown source kind");
	bad = direct;
	put_u8(bad, 64U + 52U, 1U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects nonzero startup reserved byte");
	bad = direct;
	put_u8(bad, 35U, 2U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects scenario/startup mismatch");
	bad = direct;
	put_u8(bad, 128U + 1U, 2U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects input with control phase");
	bad = direct;
	put_u32(bad, 128U + 4U, 1U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects non-dense frame index");
	bad = direct;
	put_u16(bad, 128U + 18U, 1U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects Charge without flag");
	bad = direct;
	put_u16(bad, 128U + 20U + 18U, 2U);
	refresh_t3case(bad);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects wrong first process control");
	bad = direct;
	bad.push_back(std::byte{0});
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects trailing data");
	bad = direct;
	put_u8(bad, 128U + 12U, 0x21U);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects payload checksum mismatch");
	bad = direct;
	put_u8(bad, 64U, 30U);
	check(!th03::formats::parse_t3case(bad), "T3CASE rejects header checksum mismatch");
}

void test_enedat()
{
	std::vector<std::byte> bytes(10U, std::byte{0});
	put_u16(bytes, 0U, 6U);
	put_u16(bytes, 4U, 1U);
	put_u8(bytes, 6U, 1U);
	put_u8(bytes, 7U, 0U);
	put_u16(bytes, 8U, 0U);
	const auto parsed = th03::formats::parse_enedat(bytes);
	check(parsed && parsed->enemy_formations.size() == 1U,
		"ENEDAT parser exposes semantic formations");
	if(parsed) {
		check(parsed->enemy_formations[0].enemy_scripts[0][0] == 0U,
			"ENEDAT parser preserves script opcodes as owned bytes");
	}

	auto bad = bytes;
	put_u16(bad, 0U, 5U);
	check(!th03::formats::parse_enedat(bad),
		"ENEDAT rejects a header/payload size mismatch");
	bad = bytes;
	put_u16(bad, 4U, 17U);
	check(!th03::formats::parse_enedat(bad),
		"ENEDAT rejects the donor's unsafe over-16 formation count");
	bad = bytes;
	bad.push_back(std::byte{0});
	put_u16(bad, 0U, 7U);
	check(!th03::formats::parse_enedat(bad),
		"ENEDAT rejects trailing bytes after its terminator");
}

void test_recorded_pause_frame_boundary()
{
	th03::GameInit init{};
	init.game_mode = th03::detail::game_mode_vs_cpu_cpu;
	init.is_cpu = {1U, 1U};
	init.paletted_character = {1, 3};
	th03::GameContent content{};
	content.enemy_formations.resize(2U);
	for(auto &formation : content.enemy_formations) {
		formation.enemy_scripts.push_back({0U});
	}
	auto game = th03::Game::create(
		init,
		std::move(content),
		th03::GameOptions{
			.input_protocol = th03::InputProtocol::recorded_replay,
		}
	);
	check(game.has_value(), "recorded-pause test game creates");
	if(!game) {
		return;
	}

	auto result = game->step(
		th03::CoreInput{.system = 0x1000U},
		th03::InputContext::gameplay
	);
	check(
		result.status == th03::StepStatus::phase_transition &&
		result.input_boundary_state.global_frame == 1U &&
		result.input_boundary_state.round_frame == 0U,
		"recorded Cancel exposes its raw input boundary and defers the frame suffix"
	);
	result = game->step(
		th03::CoreInput{.system = 0x1000U},
		th03::InputContext::interstitial
	);
	check(
		result.status == th03::StepStatus::running &&
		result.input_boundary_state.global_frame == 2U &&
		result.input_boundary_state.round_frame == 0U,
		"recorded pause samples advance only the logical global frame"
	);
	result = game->step({}, th03::InputContext::interstitial);
	check(
		result.status == th03::StepStatus::running &&
		result.input_boundary_state.global_frame == 3U,
		"recorded pause release remains an auxiliary input sample"
	);
	auto resumed = game->finish_deferred_frame(
		th03::DeferredFrameAction::resume
	);
	check(
		resumed && resumed->status == th03::StepStatus::running &&
		game->canonical_state().round_frame == 1U &&
		game->canonical_state().route == 0U,
		"recorded pause Resume completes the deferred frame without applying warning-only Cancel handling"
	);
	check(
		!game->finish_deferred_frame(th03::DeferredFrameAction::resume),
		"a recorded pause frame can only be completed once"
	);

	result = game->step(
		th03::CoreInput{.system = 0x1000U},
		th03::InputContext::gameplay
	);
	check(
		result.status == th03::StepStatus::phase_transition,
		"a later recorded Cancel opens another deferred pause frame"
	);
	(void)game->step({}, th03::InputContext::interstitial);
	auto exited = game->finish_deferred_frame(
		th03::DeferredFrameAction::exit_session
	);
	check(
		exited && exited->status == th03::StepStatus::terminal &&
		game->canonical_state().route == 0U &&
		game->canonical_state().round_frame == 2U,
		"recorded pause Exit applies the sticky loop exit only after the deferred source frame"
	);
}

void test_t3split()
{
	th03::formats::SplitStream base{};
	base.state_hash_schema = 3U;
	base.rows = {
		split_row(th03::formats::SplitEvent::start, 0U, 0x11111111U),
		split_row(th03::formats::SplitEvent::checkpoint, 128U, 0x22222222U),
		split_row(th03::formats::SplitEvent::route, 200U, 0x33333333U),
		split_row(th03::formats::SplitEvent::finish, 200U, 0x33333333U),
	};
	const auto encoded_result = th03::formats::write_t3split(base);
	check(encoded_result.has_value(), "T3SPLIT writer accepts valid stream");
	if(!encoded_result) {
		return;
	}
	const auto &encoded = *encoded_result;
	const auto parsed = th03::formats::parse_t3split(encoded);
	check(parsed && parsed->rows.size() == 4U && parsed->state_hash_schema == 3U, "T3SPLIT round trip");

	auto candidate = base;
	candidate.rows.insert(candidate.rows.begin() + 1, split_row(th03::formats::SplitEvent::round_start, 0U, 0x11111111U));
	candidate.rows[2].global_frame = 127U;
	candidate.rows[3].state_hash = 0xAAAAAAAAU;
	candidate.rows[4].state_hash = 0xAAAAAAAAU;
	const auto same = th03::formats::compare_oracle_splits(base, candidate);
	check(same.status == th03::formats::ComparisonStatus::same, "oracle comparator calibrates duplicate start and checkpoint label delta");
	check(same.checkpoint_global_delta == -1, "oracle comparator reports constant checkpoint delta");
	check(same.terminal_hash_differences.size() == 2U, "oracle comparator preserves terminal hash diagnostics");

	auto bad_hash = base;
	bad_hash.rows[1].state_hash = 0xDEADBEEFU;
	check(th03::formats::compare_oracle_splits(base, bad_hash).status == th03::formats::ComparisonStatus::different, "oracle comparator rejects checkpoint hash mismatch");
	auto prefix = base;
	prefix.rows.resize(2U);
	check(th03::formats::compare_oracle_splits(base, prefix, true).status == th03::formats::ComparisonStatus::same_prefix, "oracle comparator supports explicit candidate prefix");
	auto wrong_schema = base;
	wrong_schema.state_hash_schema = 0U;
	check(th03::formats::compare_oracle_splits(base, wrong_schema).status == th03::formats::ComparisonStatus::different, "oracle comparator rejects state-hash schema mismatch");
	auto unwritable = base;
	unwritable.state_hash_schema = 2U;
	check(!th03::formats::write_t3split(unwritable), "T3SPLIT writer rejects unsupported schema");

	auto bad = encoded;
	put_u8(bad, 0U, 'X');
	check(!th03::formats::parse_t3split(bad), "T3SPLIT rejects bad magic");
	bad = encoded;
	put_u16(bad, 14U, 2U);
	check(!th03::formats::parse_t3split(bad), "T3SPLIT rejects unsupported schema");
	bad = encoded;
	bad.pop_back();
	check(!th03::formats::parse_t3split(bad), "T3SPLIT rejects truncated row");
	bad = encoded;
	put_u8(bad, 16U + 7U, 1U);
	check(!th03::formats::parse_t3split(bad), "T3SPLIT rejects nonzero reserved byte");
	bad = encoded;
	put_u8(bad, 16U + 18U, 0xFAU);
	check(!th03::formats::parse_t3split(bad), "T3SPLIT rejects non-decimal packed score");
}

void test_practice_retry_policy()
{
	th03::GameInit init{};
	init.game_mode = th03::game_mode::vs_1p_cpu;
	init.rank = 0U;
	init.story_stage = 6U;
	init.story_lives = 2U;
	init.is_cpu = {0U, 1U};
	init.paletted_character = {1, 3};
	init.runtime_snapshot = th03::RuntimeSnapshot{};
	init.practice_rules = th03::PracticeRules{
		.uses_stock = true,
		.retry_cpu_timer = th03::CpuTimerPolicy::story_native,
		.retry_cpu_damage = 2U,
	};
	th03::RoundStartSnapshot round{};
	round.state.gauge_attack_level = {7U, 8U};
	round.state.cpu_safety_frames = {333U, 333U};
	init.round_start_snapshot = round;
	check(
		th03::Game::create(init).has_value(),
		"stock Practice accepts its exact initial Story-native CPU timer"
	);

	auto state = th03::detail::initialize_first_round(init, 0U);
	state.gba_gauge_level = {12U, 13U};
	state.round_frame = 1234U;
	th03::detail::initialize_repeat_round(state, init, 0U);
	check(
		state.round_id == 1U &&
		state.gba_gauge_level == std::array<std::uint8_t, 2>{12U, 13U} &&
		state.players[0].cpu_safety_frames == 233U &&
		state.players[1].cpu_safety_frames == 233U &&
		state.cpu_hit_damage_additional == 2U,
		"stock Practice retry preserves live spell ranks and reapplies configured CPU rules"
	);

	init.round_start_snapshot->state.cpu_safety_frames = {332U, 333U};
	check(
		!th03::Game::create(init),
		"Practice construction rejects CPU safety outside the selected policy"
	);
	init.round_start_snapshot.reset();
	check(
		!th03::Game::create(init),
		"Practice rules cannot be detached from their initial round state"
	);
}

void test_version_neutral_replay_runner()
{
	th03::replay::ReplaySession session{};
	session.source_version = 13U;
	session.init.game_mode = th03::game_mode::vs_1p_2p;
	session.init.paletted_character = {1, 3};
	session.sample_count = 3U;
	session.records = {
		th03::replay::InputSample{
			.frame_index = 0U,
			.phase = th03::replay::InputPhase::gameplay,
			.input = th03::CoreInput{.system = 0x1000U},
			.round_frame = std::nullopt,
			.round_or_result_frame = std::nullopt,
		},
		th03::replay::InputSample{
			.frame_index = 1U,
			.phase = th03::replay::InputPhase::interstitial,
			.input = {},
			.round_frame = std::nullopt,
			.round_or_result_frame = std::nullopt,
		},
		th03::replay::InputSample{
			.frame_index = 2U,
			.phase = th03::replay::InputPhase::interstitial,
			.input = th03::CoreInput{.system = 0x4000U},
			.round_frame = std::nullopt,
			.round_or_result_frame = std::nullopt,
		},
		th03::replay::ProcessBoundary{3U, th03::replay::ProcessId::main},
	};
	const auto run = th03::replay::run_session(session, th03::GameContent{});
	check(
		run && run->status == th03::replay::RunStatus::complete &&
		run->consumed_samples == 3U && run->completed_rounds.size() == 1U &&
		run->completed_rounds[0].state.route == 0U,
		"version-neutral runner coordinates recorded pause exit and MAIN boundary"
	);
}

} // namespace

int main()
{
	test_numeric();
	test_game_init_source_domain();
	test_story_score_initialization();
	test_round_start_snapshot_restoration();
	test_score_most_significant_digit_is_not_bcd_normalized();
	test_anniversary_combo_display_limit();
	test_defeat_playfield_shake();
	test_th3r03_round_start();
	test_th3r03_second_stage_start();
	test_rikako_bomb_shift_reset();
	test_ellen_bomb_vector_release();
	test_rikako_boss_materialization_collision();
	test_rikako_boss_dispatch_boundaries();
	test_chiyuri_extra_pair_materialization_transition();
	test_gauge_pattern_updates_during_hud_start();
	test_marisa_boss_patterns();
	test_kana_bullet_template_angle_inheritance();
	test_bullet_template_post_add_angle_mutations();
	test_mima_boss_renderer_state();
	test_kotohime_boss_materialization_renderer_state();
	test_chiyuri_boss_patterns_and_rng();
	test_yumemi_boss_patterns_and_rng();
	test_legacy_player_hittest_alias();
	test_hyper_callback_transition_delay();
	test_active_hyper_releases_pending_charge();
	test_yumemi_chargeshot_lifecycle_and_collision();
	test_yumemi_extra_attack_rng_argument_order();
	test_story_result_route();
	test_result_banner_suppresses_extra_attack_hittest();
	test_story_loss_and_versus_result_routes();
	test_simultaneous_defeat_bonus_owner();
	test_final_yumemi_result_bonus();
	test_repeat_round_targeted_reset_and_rng_order();
	test_demo_and_cpu_vs_cpu_route_zero();
	test_late_pipeline_control_order();
	test_main_and_mainl_session_transitions();
	test_story_extends();
	test_hit_damage_progression();
	test_random();
	test_trig();
	test_t3case();
	test_enedat();
	test_recorded_pause_frame_boundary();
	test_t3split();
	test_practice_retry_policy();
	test_version_neutral_replay_runner();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
