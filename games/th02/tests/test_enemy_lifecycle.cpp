#include "th02/core/enemy_lifecycle.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>

namespace {

using namespace th02::core;

[[nodiscard]] EnemyPoolState patterned_pool() noexcept
{
	EnemyPoolState state{};
	state.enemies_loop_bound = 23U;
	state.spawn_row_cur = 17;
	for(std::size_t i = 0U; i < state.enemies.size(); ++i) {
		const auto value = static_cast<std::int16_t>(i);
		auto &enemy = state.enemies[i];
		enemy.pos_on_page[0U] = {
			static_cast<std::int16_t>(100 + value),
			static_cast<std::int16_t>(200 + value),
		};
		enemy.pos_on_page[1U] = {
			static_cast<std::int16_t>(300 + value),
			static_cast<std::int16_t>(400 + value),
		};
		enemy.script_ip = static_cast<std::int16_t>(500 + value);
		enemy.age = static_cast<std::int16_t>(600 + value);
		enemy.template_id = static_cast<std::int16_t>(700 + value);
		enemy.flag = enemy_free;
		enemy.anim_frame = static_cast<std::uint8_t>(i + 20U);
		enemy.in_kill_anim = 1U;
		enemy.unused_1 = static_cast<std::uint8_t>(i + 30U);
		enemy.patnum_delta = static_cast<std::int16_t>(800 + value);
		enemy.render_as = static_cast<std::int16_t>(900 + value);
		enemy.angle = static_cast<std::int16_t>(1000 + value);
		enemy.spawned_in_left_half = static_cast<std::int16_t>(1100 + value);
		enemy.loop_i = static_cast<std::int16_t>(1200 + value);
		enemy.velocity_x = static_cast<std::int8_t>(-40 + value);
		enemy.velocity_y = static_cast<std::int8_t>(40 - value);
		enemy.despawn_when_offscreen_vertically = 1U;
		enemy.unused_2 = static_cast<std::uint8_t>(i + 50U);
		enemy.damage = static_cast<std::int16_t>(1300 + value);
		enemy.not_shootable = 1U;
		enemy.no_player_collision = 1U;
		enemy.pellet_group = static_cast<std::uint8_t>(i + 60U);
		enemy.pellet_speed = static_cast<std::uint8_t>(i + 70U);
	}
	return state;
}

class Recorder final : public EnemyInvalidationCallbacks {
public:
	Recorder(const EnemyPoolState &state_arg, std::span<const std::size_t> slots)
		: state(&state_arg), before_enemies(state_arg.enemies),
		expected_count(slots.size())
	{
		for(std::size_t i = 0U; i < slots.size(); ++i) {
			expected_slots[i] = slots[i];
		}
	}

	const EnemyPoolState *state{};
	std::array<Enemy, enemy_pool_size> before_enemies{};
	std::array<std::size_t, enemy_pool_size> expected_slots{};
	std::array<EnemyInvalidation, enemy_pool_size> events{};
	std::size_t expected_count{};
	std::size_t count{};
	bool callback_precedes_state_change{true};

	void invalidate_enemy(EnemyInvalidation invalidation) noexcept override
	{
		if(state == nullptr || count >= expected_count ||
			count >= events.size()) {
			callback_precedes_state_change = false;
			return;
		}
		const auto slot = expected_slots[count];
		if(slot >= state->enemies.size()) {
			callback_precedes_state_change = false;
			return;
		}
		callback_precedes_state_change &=
			state->enemies[slot] == before_enemies[slot];
		events[count++] = invalidation;
	}
};

[[nodiscard]] bool test_sparse_flags_and_bound_recompute() noexcept
{
	auto state = patterned_pool();
	state.enemies[3U].flag = enemy_alive;
	state.enemies[7U].flag = enemy_remove;
	state.enemies[12U].flag = 0x7FU;
	state.enemies[24U].flag = 0x80U;
	const auto before = state;
	const std::array<std::size_t, 4U> expected_slots{3U, 7U, 12U, 24U};
	Recorder recorder{state, expected_slots};
	if(enemies_invalidate(state, 1U, 0U, recorder) !=
		EnemyLifecycleResult::completed || recorder.count != 4U ||
		!recorder.callback_precedes_state_change ||
		recorder.events[0U] != EnemyInvalidation{{303, 403}, 32, 32} ||
		recorder.events[1U] != EnemyInvalidation{{307, 407}, 32, 32} ||
		recorder.events[2U] != EnemyInvalidation{{312, 412}, 32, 32} ||
		recorder.events[3U] != EnemyInvalidation{{324, 424}, 32, 32}) {
		std::fputs("FAIL sparse callback\n", stderr);
		return false;
	}
	auto expected = before;
	expected.enemies_loop_bound = 25U;
	expected.enemies[3U].pos_on_page[1U] =
		before.enemies[3U].pos_on_page[0U];
	expected.enemies[7U].flag = enemy_free;
	expected.enemies[12U].pos_on_page[1U] =
		before.enemies[12U].pos_on_page[0U];
	expected.enemies[24U].pos_on_page[1U] =
		before.enemies[24U].pos_on_page[0U];
	if(state != expected) {
		std::fputs("FAIL sparse state or bound\n", stderr);
		return false;
	}
	return true;
}

[[nodiscard]] bool test_same_page_repeat_and_page_refusal() noexcept
{
	auto state = patterned_pool();
	for(auto &enemy : state.enemies) enemy.flag = enemy_free;
	state.enemies[5U].flag = enemy_alive;
	state.enemies[9U].flag = enemy_remove;
	const auto before = state;
	const std::array<std::size_t, 2U> expected_slots{5U, 9U};
	Recorder recorder{state, expected_slots};
	if(enemies_invalidate(state, 1U, 1U, recorder) !=
		EnemyLifecycleResult::completed || recorder.count != 2U ||
		!recorder.callback_precedes_state_change ||
		recorder.events[0U] != EnemyInvalidation{{305, 405}, 32, 32} ||
		recorder.events[1U] != EnemyInvalidation{{309, 409}, 32, 32}) {
		std::fputs("FAIL same-page callback\n", stderr);
		return false;
	}
	auto expected = before;
	expected.enemies_loop_bound = 6U;
	expected.enemies[9U].flag = enemy_free;
	if(state != expected) {
		std::fputs("FAIL same-page state\n", stderr);
		return false;
	}
	const auto after_first = state;
	const std::array<std::size_t, 1U> repeated_slots{5U};
	Recorder repeated{state, repeated_slots};
	if(enemies_invalidate(state, 1U, 1U, repeated) !=
		EnemyLifecycleResult::completed || repeated.count != 1U ||
		!repeated.callback_precedes_state_change || state != after_first) {
		std::fputs("FAIL repeated same-page\n", stderr);
		return false;
	}
	const auto before_refusal = state;
	const std::array<std::size_t, 1U> no_slots{};
	Recorder rejected{state, no_slots};
	if(enemies_invalidate(state, enemy_page_count, 1U, rejected) !=
		EnemyLifecycleResult::invalid_page || rejected.count != 0U ||
		state != before_refusal ||
		enemies_invalidate(state, 1U, enemy_page_count, rejected) !=
			EnemyLifecycleResult::invalid_page || rejected.count != 0U ||
		state != before_refusal) {
		std::fputs("FAIL invalid page refusal\n", stderr);
		return false;
	}
	return true;
}

[[nodiscard]] bool test_remove_all_and_invalidation_composition() noexcept
{
	auto state = patterned_pool();
	for(auto &enemy : state.enemies) enemy.flag = enemy_free;
	state.enemies[1U].flag = enemy_alive;
	state.enemies[3U].flag = enemy_remove;
	state.enemies[8U].flag = 0x7FU;
	state.enemies[24U].flag = 0x80U;
	const auto before = state;
	enemies_remove_all(state);
	auto expected = before;
	for(auto &enemy : expected.enemies) {
		if(enemy.flag != enemy_free) enemy.flag = enemy_remove;
	}
	expected.enemies_loop_bound = 0U;
	if(state != expected) {
		std::fputs("FAIL remove-all raw flags\n", stderr);
		return false;
	}
	const std::array<std::size_t, 4U> expected_slots{1U, 3U, 8U, 24U};
	const auto before_invalidation = state;
	Recorder recorder{state, expected_slots};
	if(enemies_invalidate(state, 1U, 0U, recorder) !=
		EnemyLifecycleResult::completed || recorder.count != 4U ||
		!recorder.callback_precedes_state_change) {
		std::fputs("FAIL remove-all composition callback\n", stderr);
		return false;
	}
	expected = before_invalidation;
	for(auto &enemy : expected.enemies) {
		if(enemy.flag == enemy_remove) enemy.flag = enemy_free;
	}
	expected.enemies_loop_bound = 0U;
	if(state != expected) {
		std::fputs("FAIL remove-all composition state\n", stderr);
		return false;
	}
	return true;
}

} // namespace

int main()
{
	if(!test_sparse_flags_and_bound_recompute() ||
		!test_same_page_repeat_and_page_refusal() ||
		!test_remove_all_and_invalidation_composition()) {
		return EXIT_FAILURE;
	}
	std::fputs("TH02 enemy lifecycle tests passed\n", stdout);
	return EXIT_SUCCESS;
}
