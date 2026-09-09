#include "th02/core/enemy.hpp"
#include "th02/core/random.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>

namespace {

using namespace th02::core;

[[nodiscard]] EnemyPoolState patterned_pool() noexcept
{
	EnemyPoolState state{};
	state.enemies_loop_bound = 13U;
	state.spawn_row_cur = 14;
	for(std::size_t i = 0U; i < state.enemies.size(); ++i) {
		auto &enemy = state.enemies[i];
		enemy.pos_on_page[0U] = {
			static_cast<std::int16_t>(100 + static_cast<int>(i)),
			static_cast<std::int16_t>(200 + static_cast<int>(i)),
		};
		enemy.pos_on_page[1U] = {
			static_cast<std::int16_t>(300 + static_cast<int>(i)),
			static_cast<std::int16_t>(400 + static_cast<int>(i)),
		};
		enemy.script_ip = static_cast<std::int16_t>(500 + static_cast<int>(i));
		enemy.age = static_cast<std::int16_t>(600 + static_cast<int>(i));
		enemy.template_id = static_cast<std::int16_t>(700 + static_cast<int>(i));
		enemy.flag = static_cast<std::uint8_t>(i + 3U);
		enemy.anim_frame = static_cast<std::uint8_t>(i + 30U);
		enemy.in_kill_anim = 1U;
		enemy.unused_1 = static_cast<std::uint8_t>(i + 40U);
		enemy.patnum_delta = static_cast<std::int16_t>(800 + static_cast<int>(i));
		enemy.render_as = static_cast<std::int16_t>(900 + static_cast<int>(i));
		enemy.angle = static_cast<std::int16_t>(1000 + static_cast<int>(i));
		enemy.spawned_in_left_half = static_cast<std::int16_t>(1100 + static_cast<int>(i));
		enemy.loop_i = static_cast<std::int16_t>(1200 + static_cast<int>(i));
		enemy.velocity_x = static_cast<std::int8_t>(-50 + static_cast<int>(i));
		enemy.velocity_y = static_cast<std::int8_t>(50 - static_cast<int>(i));
		enemy.despawn_when_offscreen_vertically = 1U;
		enemy.unused_2 = static_cast<std::uint8_t>(i + 60U);
		enemy.damage = static_cast<std::int16_t>(1300 + static_cast<int>(i));
		enemy.not_shootable = 1U;
		enemy.no_player_collision = 1U;
		enemy.pellet_group = static_cast<std::uint8_t>(i + 70U);
		enemy.pellet_speed = static_cast<std::uint8_t>(i + 80U);
	}
	return state;
}

[[nodiscard]] RandomState patterned_random() noexcept
{
	RandomState state{};
	state.seed = 0xCAFEBABEU;
	for(std::size_t i = 0U; i < state.ring.size(); ++i) {
		state.ring[i] = static_cast<std::uint8_t>(i);
	}
	return state;
}

[[nodiscard]] EnemyTemplate template_with_top(std::int16_t top) noexcept
{
	EnemyTemplate result{};
	result.spawn_top = top;
	return result;
}

void expect_spawn_fields(Enemy &enemy, std::int16_t left, std::int16_t top,
	std::int16_t template_id, std::uint8_t anim_frame, std::size_t page_back
) noexcept
{
	enemy.flag = enemy_alive;
	enemy.pos_on_page[page_back] = {left, top};
	enemy.patnum_delta = 0;
	enemy.script_ip = 0;
	enemy.template_id = template_id;
	enemy.age = 0;
	enemy.anim_frame = anim_frame;
	enemy.angle = 0;
	enemy.not_shootable = 0U;
	enemy.no_player_collision = 0U;
	enemy.spawned_in_left_half = (left < 208) ? 1 : 0;
	enemy.loop_i = 0;
	enemy.despawn_when_offscreen_vertically = 0U;
	enemy.render_as = 1;
	enemy.in_kill_anim = 0U;
	enemy.damage = 0;
}

[[nodiscard]] bool reset_only_writes_flags_and_spawn_row() noexcept
{
	auto state = patterned_pool();
	const auto before = state;
	enemies_reset(state);

	auto expected = before;
	for(auto &enemy : expected.enemies) enemy.flag = enemy_free;
	expected.spawn_row_cur = 0;
	return state == expected;
}

[[nodiscard]] bool direct_add_writes_partial_slot_and_strict_bound() noexcept
{
	auto state = patterned_pool();
	state.enemies_loop_bound = 1U;
	auto random = patterned_random();
	random.cursor = 0xFEU;
	random.ring[0xFEU] = 0xA5U;
	const auto random_before = random;
	auto expected = state;
	expect_spawn_fields(expected.enemies[1U], 207, -20, -42, 0xA5U, 1U);

	if(enemy_add(state, random, 207, -20, 1, -42, 1U) !=
		EnemySpawnResult::completed || state != expected) {
		return false;
	}
	auto expected_random = random_before;
	expected_random.cursor = 0xFFU;
	if(random != expected_random) return false;

	auto widened = patterned_pool();
	widened.enemies_loop_bound = 5U;
	auto widened_random = patterned_random();
	widened_random.ring[0U] = 0x11U;
	if(enemy_add(widened, widened_random, 208, 3, 6, 9, 0U) !=
		EnemySpawnResult::completed || widened.enemies_loop_bound != 7U ||
		widened_random.cursor != 1U) {
		return false;
	}

	auto slot_zero = patterned_pool();
	slot_zero.enemies_loop_bound = 0U;
	auto slot_zero_random = patterned_random();
	if(enemy_add(slot_zero, slot_zero_random, 0, 0, 0, 0, 0U) !=
		EnemySpawnResult::completed || slot_zero.enemies_loop_bound != 0U ||
		slot_zero_random.cursor != 1U) {
		return false;
	}
	return true;
}

[[nodiscard]] bool direct_add_refuses_invalid_slot_or_page_atomically() noexcept
{
	auto state = patterned_pool();
	auto random = patterned_random();
	const auto before_state = state;
	const auto before_random = random;
	if(enemy_add(state, random, 1, 2, -1, 3, 0U) !=
		EnemySpawnResult::invalid_slot || state != before_state ||
		random != before_random) {
		return false;
	}
	if(enemy_add(state, random, 1, 2,
		static_cast<std::int16_t>(enemy_pool_size), 3, 0U) !=
		EnemySpawnResult::invalid_slot || state != before_state ||
		random != before_random) {
		return false;
	}
	return enemy_add(state, random, 1, 2, 0, 3, 2U) ==
		EnemySpawnResult::invalid_page && (state == before_state) &&
		(random == before_random);
}

[[nodiscard]] bool matched_row_spawns_columns_in_source_order() noexcept
{
	EnemyPoolState state{};
	state.spawn_row_cur = 0;
	std::array<EnemySpawnRow, 1U> rows{};
	rows[0U].column[0U] = 50;
	rows[0U].column[1U] = 0;
	rows[0U].column[3U] = 1;
	for(std::size_t column = 2U; column < rows[0U].column.size(); ++column) {
		if(column != 3U) rows[0U].column[column] = -1;
	}
	const std::array<EnemyTemplate, 2U> templates{
		template_with_top(-10), template_with_top(123)
	};
	auto random = patterned_random();
	random.cursor = 0xFCU;
	random.ring[0xFCU] = 0x30U;
	random.ring[0xFDU] = 0x31U;
	const EnemySpawnGrid grid{std::span<const EnemySpawnRow>{rows}, 1};
	const EnemySpawnContext context{50, false, 1U};

	if(enemies_spawn(state, random, grid, std::span<const EnemyTemplate>{templates},
		context) != EnemySpawnResult::completed || state.spawn_row_cur != 1 ||
		state.enemies_loop_bound != 2U || random.cursor != 0xFEU) {
		return false;
	}
	auto expected = EnemyPoolState{};
	expect_spawn_fields(expected.enemies[0U], 24, -10, 0, 0x30U, 1U);
	expect_spawn_fields(expected.enemies[1U], 40, 123, 1, 0x31U, 1U);
	expected.spawn_row_cur = 1;
	expected.enemies_loop_bound = 2U;
	return state == expected;
}

[[nodiscard]] bool mismatch_midboss_and_full_pool_consume_no_rng() noexcept
{
	std::array<EnemySpawnRow, 1U> rows{};
	rows[0U].column.fill(-1);
	rows[0U].column[0U] = 10;
	rows[0U].column[1U] = 0;
	const std::array<EnemyTemplate, 1U> templates{template_with_top(77)};
	const EnemySpawnGrid grid{std::span<const EnemySpawnRow>{rows}, 1};

	auto mismatch = EnemyPoolState{};
	auto mismatch_random = patterned_random();
	if(enemies_spawn(mismatch, mismatch_random, grid,
		std::span<const EnemyTemplate>{templates}, {9, false, 0U}) !=
		EnemySpawnResult::completed || mismatch.spawn_row_cur != 0 ||
		mismatch_random.cursor != 0U) {
		return false;
	}

	auto midboss = EnemyPoolState{};
	auto midboss_random = patterned_random();
	if(enemies_spawn(midboss, midboss_random, grid,
		std::span<const EnemyTemplate>{templates}, {10, true, 0U}) !=
		EnemySpawnResult::completed || midboss.spawn_row_cur != 1 ||
		midboss_random.cursor != 0U || midboss != EnemyPoolState{
			{}, 0U, 1
		}) {
		return false;
	}

	auto full = EnemyPoolState{};
	for(auto &enemy : full.enemies) enemy.flag = enemy_alive;
	const auto full_before = full;
	auto full_random = patterned_random();
	if(enemies_spawn(full, full_random, grid,
		std::span<const EnemyTemplate>{templates}, {10, false, 0U}) !=
		EnemySpawnResult::completed || full.spawn_row_cur != 1 ||
		full_random.cursor != 0U) {
		return false;
	}
	const auto expected_full = EnemyPoolState{
		full_before.enemies, full_before.enemies_loop_bound, 1
	};
	return full == expected_full;
}

[[nodiscard]] bool actual_read_refusals_preserve_prior_prefix() noexcept
{
	std::array<EnemySpawnRow, 1U> rows{};
	rows[0U].column.fill(-1);
	rows[0U].column[0U] = 20;
	rows[0U].column[1U] = 0;
	rows[0U].column[2U] = 2;
	const std::array<EnemyTemplate, 1U> templates{template_with_top(33)};
	const EnemySpawnGrid grid{std::span<const EnemySpawnRow>{rows}, 1};
	auto state = EnemyPoolState{};
	auto random = patterned_random();
	random.ring[0U] = 0x73U;

	if(enemies_spawn(state, random, grid, std::span<const EnemyTemplate>{templates},
		{20, false, 0U}) != EnemySpawnResult::invalid_template ||
		state.spawn_row_cur != 0 || random.cursor != 1U ||
		state.enemies[0U].flag != enemy_alive ||
		state.enemies[0U].pos_on_page[0U] != ScreenPoint{24, 33} ||
		state.enemies[0U].anim_frame != 0x73U) {
		return false;
	}

	auto midboss_bad = EnemyPoolState{};
	auto midboss_bad_random = patterned_random();
	std::array<EnemySpawnRow, 1U> bad_rows{};
	bad_rows[0U].column.fill(-1);
	bad_rows[0U].column[0U] = 21;
	bad_rows[0U].column[1U] = 9;
	const EnemySpawnGrid bad_grid{std::span<const EnemySpawnRow>{bad_rows}, 1};
	if(enemies_spawn(midboss_bad, midboss_bad_random, bad_grid,
		std::span<const EnemyTemplate>{templates}, {21, true, 0U}) !=
		EnemySpawnResult::invalid_template || midboss_bad != EnemyPoolState{} ||
		midboss_bad_random.cursor != 0U) {
		return false;
	}

	const EnemySpawnGrid missing_grid{std::span<const EnemySpawnRow>{rows}, 2};
	auto missing_row = EnemyPoolState{};
	missing_row.spawn_row_cur = 1;
	auto missing_row_random = patterned_random();
	return enemies_spawn(missing_row, missing_row_random, missing_grid,
		std::span<const EnemyTemplate>{templates}, {20, false, 0U}) ==
		EnemySpawnResult::invalid_row && missing_row.spawn_row_cur == 1 &&
		missing_row_random.cursor == 0U;
}

} // namespace

int main()
{
	if(!reset_only_writes_flags_and_spawn_row()) {
		std::fputs("FAIL enemy reset source write set\n", stderr);
		return EXIT_FAILURE;
	}
	if(!direct_add_writes_partial_slot_and_strict_bound()) {
		std::fputs("FAIL enemy add partial writes/strict bound\n", stderr);
		return EXIT_FAILURE;
	}
	if(!direct_add_refuses_invalid_slot_or_page_atomically()) {
		std::fputs("FAIL enemy add native refusal\n", stderr);
		return EXIT_FAILURE;
	}
	if(!matched_row_spawns_columns_in_source_order()) {
		std::fputs("FAIL enemy matched-row spawn order\n", stderr);
		return EXIT_FAILURE;
	}
	if(!mismatch_midboss_and_full_pool_consume_no_rng()) {
		std::fputs("FAIL enemy no-spawn RNG paths\n", stderr);
		return EXIT_FAILURE;
	}
	if(!actual_read_refusals_preserve_prior_prefix()) {
		std::fputs("FAIL enemy spawn actual-read refusal prefix\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
