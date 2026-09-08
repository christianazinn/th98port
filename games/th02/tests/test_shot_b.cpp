#include "th02/core/player.hpp"
#include "th02/core/random.hpp"
#include "th02/core/shot.hpp"

#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
using namespace th02::core;

[[nodiscard]] std::int16_t add_word(std::int16_t lhs, std::int16_t rhs)
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	));
}

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels)
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] PlayerShotPoolState patterned_pool()
{
	PlayerShotPoolState state{};
	for(std::size_t i = 0; i < state.shots.size(); ++i) {
		state.shots[i] = {
			player_shot_free,
			static_cast<std::uint8_t>(80U + i),
			{{
				{static_cast<std::int16_t>(1000 + static_cast<int>(i)),
					static_cast<std::int16_t>(1100 + static_cast<int>(i))},
				{static_cast<std::int16_t>(1200 + static_cast<int>(i)),
					static_cast<std::int16_t>(1300 + static_cast<int>(i))},
			}},
			{static_cast<std::int16_t>(1400 + static_cast<int>(i)),
				static_cast<std::int16_t>(1500 + static_cast<int>(i))},
			static_cast<std::uint8_t>(160U + i), (i & 1U) != 0U,
		};
	}
	state.current_slot = -1;
	state.spawn_top = 11;
	state.patnum = 0xA1U;
	state.option_patnum = 0xB2U;
	return state;
}

[[nodiscard]] RandomState patterned_random()
{
	RandomState state{};
	for(std::size_t i = 0; i < state.ring.size(); ++i) {
		state.ring[i] = static_cast<std::uint8_t>(i);
	}
	state.seed = 0x12345678U;
	state.cursor = 20U;
	return state;
}

void expect_regular(PlayerShot &shot, std::size_t page,
	const PlayerShotWriterPositions &positions, std::int16_t left_offset,
	std::uint8_t angle, std::uint8_t patnum, std::int16_t spawn_top)
{
	shot.flag = player_shot_alive;
	shot.decay_cel = 0U;
	shot.pos_on_page[page] = {
		to_subpixels(add_word(positions.player_topleft.x, left_offset)), spawn_top,
	};
	const auto velocity = pc98::masterlib::vector2(angle, 192);
	shot.velocity = {velocity.x, velocity.y};
	shot.patnum = patnum;
	shot.from_option = false;
}

void expect_option(PlayerShot &shot, std::size_t page,
	const PlayerShotWriterPositions &positions, std::int16_t left_offset,
	ShotSubpixelPoint velocity, std::uint8_t patnum)
{
	shot.flag = player_shot_alive;
	shot.decay_cel = 1U;
	shot.pos_on_page[page] = {
		to_subpixels(add_word(
			positions.option_left_topleft_on_back_page.x, left_offset
		)),
		to_subpixels(positions.option_left_topleft_on_back_page.y),
	};
	shot.velocity = velocity;
	shot.patnum = patnum;
	shot.from_option = true;
}

[[nodiscard]] bool test_level_three_shared_scan()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	auto pool = patterned_pool();
	auto expected = pool;
	auto random = patterned_random();
	random.cursor = 0xFFU;
	std::uint8_t patnum = 0x34U;
	std::uint8_t option_patnum = 0x3FU;
	std::uint8_t option_latch = 0U;
	std::int16_t target_marker = 1234;
	if(shot_b_volley(pool, positions, 0U, random, 3U, {180, 100}, target_marker,
		patnum, option_patnum, option_latch) != ShotBVolleyResult::completed) {
		return false;
	}

	const auto spawn_top = to_subpixels(add_word(200, 32));
	expected.spawn_top = spawn_top;
	expected.current_slot = 3;
	expected.patnum = patnum;
	expected.option_patnum = option_patnum;
	expect_regular(expected.shots[0], 0U, positions, 0, 195U, patnum, spawn_top);
	expect_regular(expected.shots[1], 0U, positions, 16, 188U, patnum, spawn_top);
	expect_option(expected.shots[2], 0U, positions, 0, {0, -4}, option_patnum);
	expect_option(expected.shots[3], 0U, positions, 48, {0, -4}, option_patnum);
	return pool == expected && random.cursor == 1U && option_latch == 1U &&
		target_marker == -1;
}

[[nodiscard]] bool test_remaining_nonmaximum_patterns()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	for(unsigned raw_level = 0U; raw_level <= 7U; ++raw_level) {
		auto pool = patterned_pool();
		auto expected = pool;
		auto random = patterned_random();
		std::uint8_t patnum = 0x34U;
		std::uint8_t option_patnum = 0x3FU;
		std::uint8_t option_latch = (raw_level < 2U) ? 7U : 0U;
		std::int16_t target_marker = 1234;
		const auto level = static_cast<std::uint8_t>(raw_level);
		if(shot_b_volley(pool, positions, 0U, random, level, {180, 100},
			target_marker, patnum, option_patnum, option_latch) !=
			ShotBVolleyResult::completed) return false;

		const auto spawn_top = to_subpixels(add_word(200, 32));
		expected.spawn_top = spawn_top;
		expected.patnum = patnum;
		expected.option_patnum = option_patnum;
		switch(level) {
		case 0:
			expected.current_slot = 1;
			expect_regular(expected.shots[0], 0U, positions, 8, 192U, patnum,
				spawn_top);
			break;

		case 1:
			expected.current_slot = 1;
			expect_regular(expected.shots[0], 0U, positions, 8, 190U, patnum,
				spawn_top);
			break;

		case 2:
			expected.current_slot = 2;
			expect_regular(expected.shots[0], 0U, positions, 8, 192U, patnum,
				spawn_top);
			expect_option(expected.shots[1], 0U, positions, 0, {0, -4}, option_patnum);
			expect_option(expected.shots[2], 0U, positions, 48, {0, -4}, option_patnum);
			break;

		case 3:
			expected.current_slot = 3;
			expect_regular(expected.shots[0], 0U, positions, 0, 192U, patnum,
				spawn_top);
			expect_regular(expected.shots[1], 0U, positions, 16, 193U, patnum,
				spawn_top);
			expect_option(expected.shots[2], 0U, positions, 0, {0, -4}, option_patnum);
			expect_option(expected.shots[3], 0U, positions, 48, {0, -4}, option_patnum);
			break;

		case 4:
			expected.current_slot = 3;
			expect_regular(expected.shots[0], 0U, positions, 0, 192U, patnum,
				spawn_top);
			expect_regular(expected.shots[1], 0U, positions, 16, 192U, patnum,
				spawn_top);
			expect_option(expected.shots[2], 0U, positions, 0, {0, -4}, option_patnum);
			expect_option(expected.shots[3], 0U, positions, 48, {0, -4}, option_patnum);
			break;

		case 5:
			expected.current_slot = 5;
			expect_regular(expected.shots[0], 0U, positions, 0, 190U, patnum,
				spawn_top);
			expect_regular(expected.shots[1], 0U, positions, 16, 194U, patnum,
				spawn_top);
			break;

		case 6:
			expected.current_slot = 6;
			expect_regular(expected.shots[0], 0U, positions, 8, 192U, patnum,
				spawn_top);
			expect_regular(expected.shots[1], 0U, positions, 8, 187U, patnum,
				spawn_top);
			expect_regular(expected.shots[2], 0U, positions, 8, 197U, patnum,
				spawn_top);
			break;

		case 7:
			expected.current_slot = 6;
			expect_regular(expected.shots[0], 0U, positions, 8, 192U, patnum,
				spawn_top);
			expect_regular(expected.shots[1], 0U, positions, 8, 185U, patnum,
				spawn_top);
			expect_regular(expected.shots[2], 0U, positions, 8, 199U, patnum,
				spawn_top);
			break;
		default:
			return false;
		}
		if(level >= 5U) {
			const auto option_start = (level == 5U) ? 2U : 3U;
			expect_option(expected.shots[option_start], 0U, positions, 0, {0, -16},
				option_patnum);
			expect_option(expected.shots[option_start + 1U], 0U, positions, 48,
				{0, -16}, option_patnum);
			expect_option(expected.shots[option_start + 2U], 0U, positions, -8,
				{-3, -6}, option_patnum);
			expect_option(expected.shots[option_start + 3U], 0U, positions, 56,
				{3, -6}, option_patnum);
		}
		const auto expected_cursor = (level == 1U || level == 2U)
			? 21U
			: (level == 3U) ? 22U : 20U;
		const auto options_expected = (level < 2U) ? 7U : 1U;
		const auto marker_expected = (level < 2U) ? 1234 : -1;
		if(pool != expected || random.cursor != expected_cursor ||
			option_latch != options_expected || target_marker != marker_expected) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool test_level_eight_overwrites_option_pattern()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	auto pool = patterned_pool();
	auto expected = pool;
	auto random = patterned_random();
	std::uint8_t patnum = 0x34U;
	std::uint8_t option_patnum = 0x3FU;
	std::uint8_t option_latch = 0U;
	std::int16_t target_marker = 1234;
	if(shot_b_volley(pool, positions, 1U, random, 8U, {180, 100}, target_marker,
		patnum, option_patnum, option_latch) != ShotBVolleyResult::completed) {
		return false;
	}

	const auto spawn_top = to_subpixels(add_word(200, 32));
	expected.spawn_top = spawn_top;
	expected.current_slot = 9;
	expected.patnum = patnum;
	expected.option_patnum = option_patnum;
	expect_regular(expected.shots[0], 1U, positions, 0, 192U, patnum, spawn_top);
	expect_regular(expected.shots[1], 1U, positions, 16, 192U, patnum, spawn_top);
	expect_regular(expected.shots[2], 1U, positions, 8, 197U, patnum, spawn_top);
	expect_regular(expected.shots[3], 1U, positions, 8, 198U, patnum, spawn_top);
	expect_option(expected.shots[4], 1U, positions, 0, {0, -54}, option_patnum);
	expect_option(expected.shots[5], 1U, positions, 48, {0, -54}, option_patnum);
	expect_option(expected.shots[6], 1U, positions, -8, {-16, -50}, option_patnum);
	expect_option(expected.shots[7], 1U, positions, 56, {16, -50}, option_patnum);
	expect_option(expected.shots[8], 1U, positions, -16, {-32, -40}, option_patnum);
	expect_option(expected.shots[9], 1U, positions, 64, {32, -40}, option_patnum);
	return pool == expected && random.cursor == 22U && option_latch == 1U &&
		target_marker == -1;
}

[[nodiscard]] bool test_level_nine_homing_and_fallback()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	const ShotPixelPoint target{180, 100};
	auto pool = patterned_pool();
	auto expected = pool;
	auto random = patterned_random();
	random.cursor = 0xFEU;
	std::uint8_t patnum = 0x30U;
	std::uint8_t option_patnum = 0x37U;
	std::uint8_t option_latch = 0U;
	std::int16_t target_marker = -1;
	if(shot_b_volley(pool, positions, 0U, random, 9U, target, target_marker,
		patnum, option_patnum, option_latch) != ShotBVolleyResult::completed) {
		return false;
	}
	const auto aim_0 = pc98::masterlib::iatan2_source(-132, 80);
	const auto aim_1 = pc98::masterlib::iatan2_source(-132, 64);
	if(!aim_0 || !aim_1) return false;
	const auto spawn_top = to_subpixels(add_word(200, 32));
	expected.spawn_top = spawn_top;
	expected.current_slot = 9;
	expected.patnum = 0x32U;
	expected.option_patnum = option_patnum;
	expect_regular(expected.shots[0], 0U, positions, 0, 192U, 0x30U, spawn_top);
	expect_regular(expected.shots[1], 0U, positions, 16, 192U, 0x30U, spawn_top);
	expect_regular(expected.shots[2], 0U, positions, 0,
		static_cast<std::uint8_t>(static_cast<int>(*aim_0) + 3), 0x32U, spawn_top);
	expect_regular(expected.shots[3], 0U, positions, 16,
		static_cast<std::uint8_t>(static_cast<int>(*aim_1) + 4), 0x32U, spawn_top);
	expect_option(expected.shots[4], 0U, positions, 0, {0, -54}, option_patnum);
	expect_option(expected.shots[5], 0U, positions, 48, {0, -54}, option_patnum);
	expect_option(expected.shots[6], 0U, positions, -8, {-16, -50}, option_patnum);
	expect_option(expected.shots[7], 0U, positions, 56, {16, -50}, option_patnum);
	expect_option(expected.shots[8], 0U, positions, -16, {-32, -40}, option_patnum);
	expect_option(expected.shots[9], 0U, positions, 64, {32, -40}, option_patnum);
	if(pool != expected || random.cursor != 0U || patnum != 0x32U ||
		option_latch != 1U || target_marker != 180) return false;

	// A nonpositive target Y selects both fixed fallback angles and consumes no
	// ring byte, while the later option phase still records the target X.
	pool = patterned_pool();
	random = patterned_random();
	const auto random_before = random;
	patnum = 0x30U;
	option_patnum = 0x37U;
	option_latch = 0U;
	target_marker = -1;
	if(shot_b_volley(pool, positions, 0U, random, 9U, {180, 0}, target_marker,
		patnum, option_patnum, option_latch) != ShotBVolleyResult::completed) {
		return false;
	}
	const auto fallback_2 = pc98::masterlib::vector2(183U, 192);
	const auto fallback_3 = pc98::masterlib::vector2(201U, 192);
	return pool.shots[2].velocity.x == fallback_2.x &&
		pool.shots[2].velocity.y == fallback_2.y &&
		pool.shots[3].velocity.x == fallback_3.x &&
		pool.shots[3].velocity.y == fallback_3.y &&
		random == random_before && patnum == 0x32U && option_latch == 1U &&
		target_marker == 180;
}

[[nodiscard]] bool test_boundaries_and_source_fault()
{
	const PlayerShotWriterPositions positions{{100, 32737}, {300, 400}};
	auto pool = patterned_pool();
	auto expected = pool;
	auto random = patterned_random();
	const auto random_before = random;
	std::uint8_t patnum = 0x30U;
	std::uint8_t option_patnum = 0x37U;
	std::uint8_t option_latch = 0U;
	std::int16_t target_marker = 1234;
	if(shot_b_volley(pool, positions, 0U, random, 9U, {1, 1}, target_marker,
		patnum, option_patnum, option_latch) !=
		ShotBVolleyResult::source_iatan2_fault) return false;
	const auto spawn_top = to_subpixels(add_word(32737, 32));
	expected.spawn_top = spawn_top;
	expected.current_slot = 2;
	expected.patnum = 0x32U;
	expected.option_patnum = option_patnum;
	expect_regular(expected.shots[0], 0U, positions, 0, 192U, 0x30U, spawn_top);
	expect_regular(expected.shots[1], 0U, positions, 16, 192U, 0x30U, spawn_top);
	if(pool != expected || random != random_before || patnum != 0x32U ||
		option_latch != 0U || target_marker != 1234) return false;

	pool = patterned_pool();
	random = patterned_random();
	const auto pool_before = pool;
	const auto page_random_before = random;
	patnum = 0x30U;
	option_patnum = 0x37U;
	option_latch = 1U;
	target_marker = 1234;
	if(shot_b_volley(pool, {{100, 200}, {300, 400}}, 2U, random, 9U, {180, 100},
		target_marker, patnum, option_patnum, option_latch) !=
		ShotBVolleyResult::invalid_page) return false;
	if(pool != pool_before || random != page_random_before || patnum != 0x30U ||
		option_patnum != 0x37U || option_latch != 1U || target_marker != 1234) {
		return false;
	}

	pool = patterned_pool();
	for(auto &shot : pool.shots) shot.flag = player_shot_alive;
	expected = pool;
	random = patterned_random();
	patnum = 0x30U;
	option_patnum = 0x37U;
	option_latch = 1U;
	target_marker = 1234;
	if(shot_b_volley(pool, {{100, 200}, {300, 400}}, 1U, random, 9U, {180, 100},
		target_marker, patnum, option_patnum, option_latch) !=
		ShotBVolleyResult::completed) return false;
	expected.current_slot = 38;
	expected.spawn_top = to_subpixels(add_word(200, 32));
	expected.patnum = patnum;
	expected.option_patnum = option_patnum;
	return pool == expected && option_latch == 1U && target_marker == 1234;
}

[[nodiscard]] bool test_latched_options_and_sparse_scan()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	auto pool = patterned_pool();
	auto expected = pool;
	auto random = patterned_random();
	std::uint8_t patnum = 0x34U;
	std::uint8_t option_patnum = 0x3FU;
	std::uint8_t option_latch = 1U;
	std::int16_t target_marker = 1234;
	if(shot_b_volley(pool, positions, 1U, random, 2U, {180, 100}, target_marker,
		patnum, option_patnum, option_latch) != ShotBVolleyResult::completed) {
		return false;
	}
	const auto spawn_top = to_subpixels(add_word(200, 32));
	expected.spawn_top = spawn_top;
	expected.current_slot = 1;
	expected.patnum = patnum;
	expected.option_patnum = option_patnum;
	expect_regular(expected.shots[0], 1U, positions, 8, 192U, patnum, spawn_top);
	if(pool != expected || random.cursor != 21U || option_latch != 1U ||
		target_marker != -1) return false;

	pool = patterned_pool();
	for(auto &shot : pool.shots) shot.flag = player_shot_alive;
	pool.shots[3].flag = player_shot_free;
	pool.shots[6].flag = player_shot_free;
	pool.shots[9].flag = player_shot_free;
	expected = pool;
	random = patterned_random();
	patnum = 0x34U;
	option_patnum = 0x3FU;
	option_latch = 0U;
	target_marker = 1234;
	if(shot_b_volley(pool, positions, 0U, random, 3U, {180, 100}, target_marker,
		patnum, option_patnum, option_latch) != ShotBVolleyResult::completed) {
		return false;
	}
	expected.spawn_top = spawn_top;
	expected.current_slot = 38;
	expected.patnum = patnum;
	expected.option_patnum = option_patnum;
	expect_regular(expected.shots[3], 0U, positions, 0, 192U, patnum, spawn_top);
	expect_regular(expected.shots[6], 0U, positions, 16, 193U, patnum, spawn_top);
	expect_option(expected.shots[9], 0U, positions, 0, {0, -4}, option_patnum);
	return pool == expected && random.cursor == 22U && option_latch == 0U &&
		target_marker == -1;
}

struct ShotBAdapter final : PlayerShotStreamCallbacks {
	PlayerShotPoolState &pool;
	RandomState &random;
	const PlayerShotWriterPositions &positions;
	std::uint8_t &option_latch;
	ShotBVolleyResult result{ShotBVolleyResult::invalid_page};

	ShotBAdapter(PlayerShotPoolState &pool_arg, RandomState &random_arg,
		const PlayerShotWriterPositions &positions_arg,
		std::uint8_t &option_latch_arg) noexcept
		: pool(pool_arg), random(random_arg), positions(positions_arg),
		  option_latch(option_latch_arg)
	{
	}

	void shot_volley(const PlayerShotStreamState &state, std::uint8_t &patnum,
		std::uint8_t &option_patnum) noexcept override
	{
		std::int16_t unused = -1;
		result = shot_b_volley(pool, positions, 0U, random, state.shot_level,
			{180, 0}, unused, patnum, option_patnum, option_latch);
	}

	void shot_sound(const PlayerShotStreamState &) noexcept override {}
	void bomb_request(const PlayerShotStreamState &) noexcept override {}
};

[[nodiscard]] bool test_stream_callback_uses_b_body()
{
	auto pool = patterned_pool();
	auto random = patterned_random();
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	std::uint8_t option_latch = 0U;
	ShotBAdapter adapter{pool, random, positions, option_latch};
	PlayerShotStreamState stream{2U, 0U, 1U, 0U, 9U, 0U, 0U, 0x30U, 0x37U, 0};
	player_move_and_shoot_shot_stream_step(stream, 0x0010U, adapter);
	return adapter.result == ShotBVolleyResult::completed &&
		stream.shot_patnum == 0x32U && stream.shot_option_patnum == 0x37U &&
		stream.stream_b_phase == 1U && stream.stream_b_cooldown == 8U &&
		pool.current_slot == 9 && pool.shots[2].patnum == 0x32U &&
		option_latch == 1U;
}

} // namespace

int main()
{
	if(!test_level_three_shared_scan()) {
		std::fputs("FAIL level-three shared scan\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_remaining_nonmaximum_patterns()) {
		std::fputs("FAIL remaining nonmaximum patterns\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_level_eight_overwrites_option_pattern()) {
		std::fputs("FAIL level-eight option overwrite\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_level_nine_homing_and_fallback()) {
		std::fputs("FAIL level-nine homing/fallback\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_boundaries_and_source_fault()) {
		std::fputs("FAIL boundaries/source fault\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_latched_options_and_sparse_scan()) {
		std::fputs("FAIL latched options/sparse scan\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_stream_callback_uses_b_body()) {
		std::fputs("FAIL stream callback\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
