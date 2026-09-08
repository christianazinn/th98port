#include "th02/core/player.hpp"
#include "th02/core/random.hpp"
#include "th02/core/shot.hpp"

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

[[nodiscard]] std::int16_t subtract_word(std::int16_t lhs, std::int16_t rhs)
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) -
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

[[nodiscard]] bool test_regular_levels()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	for(unsigned raw_level = 0U; raw_level <= 8U; ++raw_level) {
		auto pool = patterned_pool();
		auto expected = pool;
		auto random = patterned_random();
		std::uint8_t cycle = 0U;
		std::uint8_t patnum = 0x34U;
		std::uint8_t option_patnum = 0x3FU;
		std::uint8_t option_latch = (raw_level == 2U) ? 0U : 7U;
		const auto level = static_cast<std::uint8_t>(raw_level);
		if(!shot_c_volley(pool, positions, 0U, random, level, cycle, patnum,
			option_patnum, option_latch)) return false;

		const auto spawn_top = to_subpixels(add_word(200, 32));
		expected.spawn_top = spawn_top;
		expected.patnum = (level >= 2U) ? 0x7CU : patnum;
		expected.option_patnum = option_patnum;
		const auto random_angle = static_cast<std::uint8_t>(
			(level >= 3U && level <= 5U) ? 190U : 192U
		);
		expect_regular(expected.shots[0], 0U, positions, 8, random_angle,
			expected.patnum, spawn_top);
		if(level == 2U) {
			expected.current_slot = 2;
			expect_option(expected.shots[1], 0U, positions, 0, {0, -4}, option_patnum);
			expect_option(expected.shots[2], 0U, positions, 48, {0, -4}, option_patnum);
		} else {
			expected.current_slot = 1;
		}
		const auto expected_cursor = (level >= 3U) ? 21U : 20U;
		const auto expected_latch = (level == 2U) ? 1U : 7U;
		if(pool != expected || cycle != 1U || random.cursor != expected_cursor ||
			patnum != expected.patnum || option_latch != expected_latch) return false;
	}
	return true;
}

[[nodiscard]] bool test_option_cadences()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	struct Case {
		std::uint8_t level;
		std::uint8_t cycle_before;
		ShotSubpixelPoint first_velocity;
		std::size_t option_count;
	};
	constexpr Case cases[]{
		{3U, 4U, {0, -4}, 2U}, {4U, 3U, {0, -16}, 2U},
		{5U, 3U, {4, -16}, 4U}, {6U, 2U, {6, -24}, 4U},
		{7U, 2U, {6, -26}, 4U}, {8U, 2U, {10, -48}, 4U},
	};
	for(const auto &entry : cases) {
		auto pool = patterned_pool();
		auto expected = pool;
		auto random = patterned_random();
		std::uint8_t cycle = entry.cycle_before;
		std::uint8_t patnum = 0x34U;
		std::uint8_t option_patnum = 0x3FU;
		std::uint8_t option_latch = 0U;
		if(!shot_c_volley(pool, positions, 1U, random, entry.level, cycle, patnum,
			option_patnum, option_latch)) return false;

		const auto spawn_top = to_subpixels(add_word(200, 32));
		expected.spawn_top = spawn_top;
		expected.current_slot = static_cast<std::int16_t>(entry.option_count);
		expected.patnum = 0x7CU;
		expected.option_patnum = option_patnum;
		const auto regular_angle = static_cast<std::uint8_t>(
			(entry.level <= 5U) ? 190U : 192U
		);
		expect_regular(expected.shots[0], 1U, positions, 8, regular_angle, 0x7CU,
			spawn_top);
		expect_option(expected.shots[1], 1U, positions, 0, entry.first_velocity,
			option_patnum);
		if(entry.option_count == 2U) {
			const auto y = static_cast<std::int16_t>(
				(entry.level == 3U) ? -4 : -16
			);
			expect_option(expected.shots[2], 1U, positions, 48, {0, y}, option_patnum);
		} else if(entry.level == 5U) {
			expect_option(expected.shots[2], 1U, positions, 48, {-4, -16}, option_patnum);
			expect_option(expected.shots[3], 1U, positions, -8, {0, -16}, option_patnum);
			expect_option(expected.shots[4], 1U, positions, 56, {0, -16}, option_patnum);
		} else if(entry.level == 6U) {
			expect_option(expected.shots[2], 1U, positions, 48, {-6, -24}, option_patnum);
			expect_option(expected.shots[3], 1U, positions, -8, {0, -24}, option_patnum);
			expect_option(expected.shots[4], 1U, positions, 56, {0, -24}, option_patnum);
		} else if(entry.level == 7U) {
			expect_option(expected.shots[2], 1U, positions, 48, {-6, -26}, option_patnum);
			expect_option(expected.shots[3], 1U, positions, -8, {0, -26}, option_patnum);
			expect_option(expected.shots[4], 1U, positions, 56, {0, -26}, option_patnum);
		} else {
			expect_option(expected.shots[2], 1U, positions, 48, {-10, -48}, option_patnum);
			expect_option(expected.shots[3], 1U, positions, -8, {-4, -48}, option_patnum);
			expect_option(expected.shots[4], 1U, positions, 56, {4, -48}, option_patnum);
		}
		if(pool != expected || cycle != static_cast<std::uint8_t>(
			entry.cycle_before + 1U) || random.cursor != 21U ||
			option_latch != 1U) return false;
	}
	return true;
}

[[nodiscard]] bool test_level_nine_cycle_shapes()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	auto pool = patterned_pool();
	auto expected = pool;
	auto random = patterned_random();
	std::uint8_t cycle = 2U;
	std::uint8_t patnum = 0x30U;
	std::uint8_t option_patnum = 0x37U;
	std::uint8_t option_latch = 7U;
	if(!shot_c_volley(pool, positions, 0U, random, 9U, cycle, patnum,
		option_patnum, option_latch)) return false;
	const auto spawn_top = to_subpixels(add_word(200, 32));
	expected.spawn_top = spawn_top;
	expected.current_slot = 5;
	expected.patnum = 0x31U;
	expected.option_patnum = option_patnum;
	expect_regular(expected.shots[0], 0U, positions, 8, 192U, 0x7CU, spawn_top);
	expect_regular(expected.shots[1], 0U, positions, -16, 192U, 0x31U, spawn_top);
	expect_regular(expected.shots[2], 0U, positions, 32, 192U, 0x31U, spawn_top);
	expect_regular(expected.shots[3], 0U, positions, -16, 192U, 0x31U,
		subtract_word(spawn_top, to_subpixels(16)));
	expect_regular(expected.shots[4], 0U, positions, 32, 192U, 0x31U,
		subtract_word(spawn_top, to_subpixels(16)));
	if(pool != expected || cycle != 3U || random.cursor != 20U || patnum != 0x31U ||
		option_latch != 7U) return false;

	pool = patterned_pool();
	expected = pool;
	random = patterned_random();
	cycle = 3U;
	patnum = 0x30U;
	option_patnum = 0x37U;
	option_latch = 0U;
	if(!shot_c_volley(pool, positions, 1U, random, 9U, cycle, patnum,
		option_patnum, option_latch)) return false;
	expected.spawn_top = spawn_top;
	expected.current_slot = 6;
	expected.patnum = 0x7CU;
	expected.option_patnum = option_patnum;
	expect_regular(expected.shots[0], 1U, positions, 8, 192U, 0x7CU, spawn_top);
	expect_option(expected.shots[1], 1U, positions, 0, {-10, -160}, option_patnum);
	expect_option(expected.shots[2], 1U, positions, 48, {10, -160}, option_patnum);
	expect_option(expected.shots[3], 1U, positions, -8, {20, -140}, option_patnum);
	expect_option(expected.shots[4], 1U, positions, 56, {-20, -140}, option_patnum);
	expect_option(expected.shots[5], 1U, positions, -16, {-7, -120}, option_patnum);
	expect_option(expected.shots[6], 1U, positions, 64, {7, -120}, option_patnum);
	if(pool != expected || cycle != 4U || random.cursor != 20U ||
		patnum != 0x7CU || option_latch != 1U) return false;

	pool = patterned_pool();
	expected = pool;
	random = patterned_random();
	cycle = 1U;
	patnum = 0x30U;
	option_patnum = 0x37U;
	option_latch = 0U;
	if(!shot_c_volley(pool, positions, 1U, random, 9U, cycle, patnum,
		option_patnum, option_latch)) return false;
	expected.spawn_top = spawn_top;
	expected.current_slot = 4;
	expected.patnum = 0x7CU;
	expected.option_patnum = option_patnum;
	expect_regular(expected.shots[0], 1U, positions, 8, 192U, 0x7CU, spawn_top);
	expect_option(expected.shots[1], 1U, positions, 0, {-10, -160}, option_patnum);
	expect_option(expected.shots[2], 1U, positions, 48, {10, -160}, option_patnum);
	expect_option(expected.shots[3], 1U, positions, -8, {20, -140}, option_patnum);
	expect_option(expected.shots[4], 1U, positions, 56, {-20, -140}, option_patnum);
	return pool == expected && cycle == 2U && random.cursor == 20U &&
		patnum == 0x7CU && option_latch == 1U;
}

[[nodiscard]] bool test_boundaries_and_cycle_wrap()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	auto pool = patterned_pool();
	auto random = patterned_random();
	const auto pool_before = pool;
	const auto random_before = random;
	std::uint8_t cycle = 0xFFU;
	std::uint8_t patnum = 0x30U;
	std::uint8_t option_patnum = 0x37U;
	std::uint8_t option_latch = 1U;
	if(shot_c_volley(pool, positions, 2U, random, 9U, cycle, patnum,
		option_patnum, option_latch)) return false;
	if(pool != pool_before || random != random_before || cycle != 0xFFU ||
		patnum != 0x30U || option_patnum != 0x37U || option_latch != 1U) {
		return false;
	}

	pool = patterned_pool();
	for(auto &shot : pool.shots) shot.flag = player_shot_alive;
	auto expected = pool;
	random = patterned_random();
	cycle = 0xFFU;
	patnum = 0x30U;
	option_patnum = 0x37U;
	option_latch = 1U;
	if(!shot_c_volley(pool, positions, 0U, random, 9U, cycle, patnum,
		option_patnum, option_latch)) return false;
	expected.spawn_top = to_subpixels(add_word(200, 32));
	expected.current_slot = 38;
	expected.patnum = 0x7CU;
	expected.option_patnum = option_patnum;
	if(pool != expected || cycle != 0U || random != patterned_random() ||
		patnum != 0x7CU || option_latch != 1U) return false;

	pool = patterned_pool();
	random = patterned_random();
	cycle = 0U;
	patnum = 0x34U;
	option_patnum = 0x3FU;
	option_latch = 1U;
	if(!shot_c_volley(pool, positions, 1U, random, 2U, cycle, patnum,
		option_patnum, option_latch)) return false;
	return pool.current_slot == 1 && pool.shots[0].flag == player_shot_alive &&
		pool.shots[1].flag == player_shot_free && cycle == 1U &&
		option_latch == 1U;
}

[[nodiscard]] std::size_t expected_option_count(std::uint8_t level,
	std::uint8_t postincrement_cycle)
{
	switch(level) {
	case 0:
	case 1:
		return 0U;
	case 2:
		return 2U;
	case 3:
		return ((postincrement_cycle % 5U) == 0U) ? 2U : 0U;
	case 4:
		return ((postincrement_cycle & 3U) == 0U) ? 2U : 0U;
	case 5:
		return ((postincrement_cycle & 3U) == 0U) ? 4U : 0U;
	case 6:
	case 7:
	case 8:
		return ((postincrement_cycle % 3U) == 0U) ? 4U : 0U;
	case 9:
		if((postincrement_cycle & 1U) != 0U) return 0U;
		return ((postincrement_cycle & 3U) == 0U) ? 6U : 4U;
	default:
		return 0U;
	}
}

[[nodiscard]] bool test_all_cycle_cadences()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	for(unsigned initial = 0U; initial <= 0xFFU; ++initial) {
		for(unsigned raw_level = 0U; raw_level <= 9U; ++raw_level) {
			auto pool = patterned_pool();
			auto random = patterned_random();
			random.cursor = 0xFFU;
			const auto level = static_cast<std::uint8_t>(raw_level);
			std::uint8_t cycle = static_cast<std::uint8_t>(initial);
			std::uint8_t patnum = 0x34U;
			std::uint8_t option_patnum = 0x3FU;
			// C's latch blocks level 2 only. Keep it set at later levels so
			// any scheduled option volley proves that it is deliberately ignored.
			std::uint8_t option_latch = (level >= 3U) ? 1U : 0U;
			if(!shot_c_volley(pool, positions, 0U, random, level, cycle, patnum,
				option_patnum, option_latch)) return false;

			const auto postincrement_cycle = static_cast<std::uint8_t>(initial + 1U);
			const auto regular_count = (level == 9U &&
				(postincrement_cycle % 3U) == 0U) ? 5U : 1U;
			const auto option_count = expected_option_count(level, postincrement_cycle);
			const auto total_count = regular_count + option_count;
			const auto expected_current = (option_count == 0U)
				? static_cast<std::int16_t>(regular_count)
				: static_cast<std::int16_t>(total_count - 1U);
			const auto expected_patnum = (level < 2U) ? 0x34U
				: ((level == 9U && regular_count == 5U) ? 0x31U : 0x7CU);
			const auto expected_cursor = (level >= 3U && level < 9U)
				? 0U
				: 0xFFU;
			if(cycle != postincrement_cycle || pool.current_slot != expected_current ||
				pool.patnum != expected_patnum || patnum != expected_patnum ||
				pool.option_patnum != option_patnum || random.cursor != expected_cursor ||
				option_latch != ((level < 2U) ? 0U : 1U)) return false;
			for(std::size_t i = 0; i < pool.shots.size(); ++i) {
				const auto expected_flag = (i < total_count)
					? player_shot_alive
					: player_shot_free;
				if(pool.shots[i].flag != expected_flag) return false;
			}
		}
	}
	return true;
}

struct ShotCAdapter final : PlayerShotStreamCallbacks {
	PlayerShotPoolState &pool;
	RandomState &random;
	const PlayerShotWriterPositions &positions;
	std::uint8_t &cycle;
	std::uint8_t &option_latch;
	bool accepted{};

	ShotCAdapter(PlayerShotPoolState &pool_arg, RandomState &random_arg,
		const PlayerShotWriterPositions &positions_arg, std::uint8_t &cycle_arg,
		std::uint8_t &option_latch_arg) noexcept
		: pool(pool_arg), random(random_arg), positions(positions_arg),
		  cycle(cycle_arg), option_latch(option_latch_arg)
	{
	}

	void shot_volley(const PlayerShotStreamState &state, std::uint8_t &patnum,
		std::uint8_t &option_patnum) noexcept override
	{
		accepted = shot_c_volley(pool, positions, 0U, random, state.shot_level,
			cycle, patnum, option_patnum, option_latch);
	}

	void shot_sound(const PlayerShotStreamState &) noexcept override {}
	void bomb_request(const PlayerShotStreamState &) noexcept override {}
};

[[nodiscard]] bool test_stream_callback_uses_c_body()
{
	auto pool = patterned_pool();
	auto random = patterned_random();
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	std::uint8_t cycle = 2U;
	std::uint8_t option_latch = 0U;
	ShotCAdapter adapter{pool, random, positions, cycle, option_latch};
	PlayerShotStreamState stream{2U, 0U, 1U, 0U, 9U, 0U, 0U, 0x30U, 0x37U, 0};
	player_move_and_shoot_shot_stream_step(stream, 0x0010U, adapter);
	return adapter.accepted && cycle == 3U && stream.shot_patnum == 0x31U &&
		stream.shot_option_patnum == 0x37U && stream.stream_b_phase == 1U &&
		stream.stream_b_cooldown == 8U && pool.current_slot == 5 &&
		pool.shots[1].patnum == 0x31U && option_latch == 0U;
}

} // namespace

int main()
{
	if(!test_regular_levels()) {
		std::fputs("FAIL regular levels\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_option_cadences()) {
		std::fputs("FAIL option cadences\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_level_nine_cycle_shapes()) {
		std::fputs("FAIL level-nine cycle shapes\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_boundaries_and_cycle_wrap()) {
		std::fputs("FAIL boundaries/cycle wrap\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_all_cycle_cadences()) {
		std::fputs("FAIL all cycle cadences\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_stream_callback_uses_c_body()) {
		std::fputs("FAIL stream callback\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
