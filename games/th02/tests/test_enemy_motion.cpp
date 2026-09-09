#include "th02/core/enemy.hpp"

#include "pc98/masterlib_vector.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>

namespace {

using namespace th02::core;

[[nodiscard]] std::int16_t add_word(
	std::int16_t lhs, std::int16_t rhs
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int8_t narrow_signed_byte(std::int16_t value) noexcept
{
	return std::bit_cast<std::int8_t>(
		static_cast<std::uint8_t>(std::bit_cast<std::uint16_t>(value))
	);
}

[[nodiscard]] Enemy patterned_enemy() noexcept
{
	Enemy enemy{};
	enemy.pos_on_page = {{{101, -202}, {303, -404}}};
	enemy.script_ip = 5;
	enemy.age = 6;
	enemy.template_id = 7;
	enemy.flag = 8U;
	enemy.anim_frame = 9U;
	enemy.in_kill_anim = 10U;
	enemy.unused_1 = 11U;
	enemy.patnum_delta = 12;
	enemy.render_as = 13;
	enemy.angle = 14;
	enemy.spawned_in_left_half = 15;
	enemy.loop_i = 16;
	enemy.velocity_x = -17;
	enemy.velocity_y = 18;
	enemy.despawn_when_offscreen_vertically = 19U;
	enemy.unused_2 = 20U;
	enemy.damage = 21;
	enemy.not_shootable = 22U;
	enemy.no_player_collision = 23U;
	enemy.pellet_group = 24U;
	enemy.pellet_speed = 25U;
	return enemy;
}

[[nodiscard]] bool test_plain_position_update_and_page_refusal() noexcept
{
	auto enemy = patterned_enemy();
	enemy.pos_on_page[1U] = {32767, -32768};
	enemy.velocity_x = 1;
	enemy.velocity_y = -1;
	auto expected = enemy;
	expected.pos_on_page[1U] = {-32768, 32767};
	if(enemy_pos_update(enemy, 1U) != EnemyMotionResult::completed ||
		enemy != expected) {
		return false;
	}

	const auto before = enemy;
	return enemy_pos_update(enemy, enemy_page_count) ==
		EnemyMotionResult::invalid_page && enemy == before;
}

[[nodiscard]] bool test_aimed_swapped_target_and_full_vector_addition() noexcept
{
	auto enemy = patterned_enemy();
	enemy.script_ip = 0;
	enemy.pos_on_page[0U] = {-100, -200};
	enemy.pos_on_page[1U] = {123, 216};
	const auto before = enemy;
	PlayerFrameState frame{};
	frame.player.current = {100, 200};
	const auto frame_before = frame;
	const std::array<std::uint8_t, 3U> script{{0U, 0U, 255U}};

	if(enemy_pos_update_aimed(enemy, frame, script, 1U) !=
		EnemyMotionResult::completed) {
		return false;
	}

	// (+24, +16) yields an exact +X angle here. The word vector is +255,
	// while the stored source velocity is its signed low byte, -1.
	if(enemy.pos_on_page[1U] != ScreenPoint{378, 216} ||
		enemy.velocity_x != -1 || enemy.velocity_y != 0 ||
		enemy.pos_on_page[0U] != before.pos_on_page[0U] ||
		enemy.script_ip != before.script_ip || enemy.age != before.age ||
		enemy.angle != before.angle || frame != frame_before) {
		return false;
	}

	// The next ordinary update consumes the narrowed cached velocity, not the
	// transient +255 word vector used by the aimed helper itself.
	return enemy_pos_update(enemy, 1U) == EnemyMotionResult::completed &&
		enemy.pos_on_page[1U] == ScreenPoint{377, 216} &&
		enemy.velocity_x == -1 && enemy.velocity_y == 0;
}

[[nodiscard]] bool test_aimed_actual_read_and_iatan_faults_are_atomic() noexcept
{
	auto missing_operand = patterned_enemy();
	missing_operand.script_ip = 0;
	const auto missing_before = missing_operand;
	PlayerFrameState frame{};
	frame.player.current = {100, 200};
	const std::array<std::uint8_t, 2U> truncated{{0U, 0U}};
	if(enemy_pos_update_aimed(missing_operand, frame, truncated, 0U) !=
		EnemyMotionResult::invalid_script_operand ||
		missing_operand != missing_before) {
		return false;
	}

	auto invalid_page = patterned_enemy();
	const auto invalid_page_before = invalid_page;
	const std::array<std::uint8_t, 3U> complete{{0U, 0U, 1U}};
	if(enemy_pos_update_aimed(invalid_page, frame, complete, enemy_page_count) !=
		EnemyMotionResult::invalid_page || invalid_page != invalid_page_before) {
		return false;
	}

	auto iatan_fault = patterned_enemy();
	iatan_fault.script_ip = 0;
	// The source target is (24, 16). A delta of (INT16_MIN, 0) hits the
	// literal master.lib iatan2 DIV-fault domain.
	iatan_fault.pos_on_page[0U] = {-32744, 16};
	const auto iatan_before = iatan_fault;
	PlayerFrameState fault_frame{};
	return enemy_pos_update_aimed(iatan_fault, fault_frame, complete, 0U) ==
		EnemyMotionResult::source_iatan2_fault && iatan_fault == iatan_before;
}

[[nodiscard]] bool test_angle_step_vector_and_raw_word_ip() noexcept
{
	auto enemy = patterned_enemy();
	enemy.script_ip = 0;
	enemy.age = 4;
	enemy.angle = 10;
	enemy.pos_on_page[1U] = {100, 200};
	const std::array<std::uint8_t, 3U> script{{20U, 2U, 255U}};
	const auto vector = pc98::masterlib::vector2(29U, 255);
	auto expected = enemy;
	expected.angle = 9;
	expected.pos_on_page[1U] = {
		add_word(100, vector.x), add_word(200, vector.y),
	};
	expected.velocity_x = narrow_signed_byte(vector.x);
	expected.velocity_y = narrow_signed_byte(vector.y);
	if(enemy_angle_step_and_move(enemy, script, 0, -1, 1U) !=
		EnemyMotionResult::completed || enemy != expected) {
		return false;
	}

	auto wrapped_ip = patterned_enemy();
	wrapped_ip.script_ip = 256;
	wrapped_ip.age = 1;
	wrapped_ip.angle = -10;
	wrapped_ip.pos_on_page[0U] = {1, 2};
	std::array<std::uint8_t, 259U> wrapped_script{};
	wrapped_script[256U] = 7U;
	wrapped_script[257U] = 2U;
	wrapped_script[258U] = 4U;
	const auto wrapped_vector = pc98::masterlib::vector2(253U, 4);
	auto wrapped_expected = wrapped_ip;
	wrapped_expected.pos_on_page[0U] = {
		add_word(1, wrapped_vector.x), add_word(2, wrapped_vector.y),
	};
	wrapped_expected.velocity_x = narrow_signed_byte(wrapped_vector.x);
	wrapped_expected.velocity_y = narrow_signed_byte(wrapped_vector.y);
	return enemy_angle_step_and_move(wrapped_ip, wrapped_script, 0, 1, 0U) ==
		EnemyMotionResult::completed && wrapped_ip == wrapped_expected;
}

[[nodiscard]] bool test_angle_step_cadence_and_fault_prefixes() noexcept
{
	const std::array<std::uint8_t, 3U> valid{{20U, 2U, 9U}};
	auto nonturn = patterned_enemy();
	nonturn.script_ip = 0;
	nonturn.age = 5;
	nonturn.angle = 10;
	auto nonturn_expected = nonturn;
	const auto nonturn_vector = pc98::masterlib::vector2(30U, 9);
	nonturn_expected.pos_on_page[0U] = {
		add_word(nonturn_expected.pos_on_page[0U].x, nonturn_vector.x),
		add_word(nonturn_expected.pos_on_page[0U].y, nonturn_vector.y),
	};
	nonturn_expected.velocity_x = narrow_signed_byte(nonturn_vector.x);
	nonturn_expected.velocity_y = narrow_signed_byte(nonturn_vector.y);
	if(enemy_angle_step_and_move(nonturn, valid, 0, -3, 0U) !=
		EnemyMotionResult::completed || nonturn != nonturn_expected) {
		return false;
	}

	auto angle_wrap = patterned_enemy();
	angle_wrap.script_ip = 0;
	angle_wrap.age = 0;
	angle_wrap.angle = 32767;
	angle_wrap.pos_on_page[0U] = {10, 20};
	auto angle_wrap_expected = angle_wrap;
	angle_wrap_expected.angle = -32768;
	angle_wrap_expected.pos_on_page[0U] = {11, 20};
	angle_wrap_expected.velocity_x = 1;
	angle_wrap_expected.velocity_y = 0;
	const std::array<std::uint8_t, 3U> wrap_script{{0U, 1U, 1U}};
	if(enemy_angle_step_and_move(angle_wrap, wrap_script, 0, 1, 0U) !=
		EnemyMotionResult::completed || angle_wrap != angle_wrap_expected) {
		return false;
	}

	auto invalid_page = patterned_enemy();
	const auto invalid_page_before = invalid_page;
	if(enemy_angle_step_and_move(invalid_page, valid, 0, 1, enemy_page_count) !=
		EnemyMotionResult::invalid_page || invalid_page != invalid_page_before) {
		return false;
	}

	auto missing_interval = patterned_enemy();
	missing_interval.script_ip = 0;
	const auto missing_interval_before = missing_interval;
	const std::array<std::uint8_t, 1U> only_base{{4U}};
	if(enemy_angle_step_and_move(missing_interval, only_base, 0, 1, 0U) !=
		EnemyMotionResult::invalid_script_operand ||
		missing_interval != missing_interval_before) {
		return false;
	}

	auto zero_interval = patterned_enemy();
	zero_interval.script_ip = 0;
	const auto zero_interval_before = zero_interval;
	const std::array<std::uint8_t, 3U> zero{{4U, 0U, 9U}};
	if(enemy_angle_step_and_move(zero_interval, zero, 0, 1, 0U) !=
		EnemyMotionResult::source_divide_fault || zero_interval != zero_interval_before) {
		return false;
	}

	// `ip + 1` is valid at 0, which advances angle before the native boundary
	// refuses the subsequent negative raw-word base read.
	auto missing_base = patterned_enemy();
	missing_base.script_ip = -1;
	missing_base.age = 0;
	missing_base.angle = 40;
	auto missing_base_expected = missing_base;
	missing_base_expected.angle = 45;
	const std::array<std::uint8_t, 1U> interval_only{{1U}};
	if(enemy_angle_step_and_move(missing_base, interval_only, 0, 5, 0U) !=
		EnemyMotionResult::invalid_script_operand ||
		missing_base != missing_base_expected) {
		return false;
	}

	// A valid interval and base likewise leave the turn visible if speed is
	// the later unavailable operand; position and velocity remain untouched.
	auto missing_speed = patterned_enemy();
	missing_speed.script_ip = 0;
	missing_speed.age = 0;
	missing_speed.angle = -4;
	auto missing_speed_expected = missing_speed;
	missing_speed_expected.angle = -1;
	const std::array<std::uint8_t, 2U> base_and_interval{{7U, 1U}};
	return enemy_angle_step_and_move(missing_speed, base_and_interval, 0, 3, 0U) ==
		EnemyMotionResult::invalid_script_operand &&
		missing_speed == missing_speed_expected;
}

} // namespace

int main()
{
	if(!test_plain_position_update_and_page_refusal()) {
		std::fputs("FAIL enemy plain position motion\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_aimed_swapped_target_and_full_vector_addition()) {
		std::fputs("FAIL enemy aimed vector motion\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_aimed_actual_read_and_iatan_faults_are_atomic()) {
		std::fputs("FAIL enemy aimed source faults\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_angle_step_vector_and_raw_word_ip()) {
		std::fputs("FAIL enemy angle-step vector/raw IP\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_angle_step_cadence_and_fault_prefixes()) {
		std::fputs("FAIL enemy angle-step cadence/fault prefix\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
