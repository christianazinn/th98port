#include "th02/core/player.hpp"
#include "th02/core/random.hpp"
#include "th02/core/shot.hpp"

#include "pc98/masterlib_vector.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

namespace {
using namespace th02::core;

struct SpawnSpec {
	std::int16_t left_offset{};
	std::uint8_t angle{};
	std::uint8_t patnum{};
	bool operator==(const SpawnSpec &) const = default;
};

struct VolleyExpected {
	std::array<SpawnSpec, 6> spawns{};
	std::size_t count{};
	std::uint8_t random_cursor{};
	std::uint8_t final_patnum{};
};

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
			static_cast<std::uint8_t>(40U + i),
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
	state.patnum = 0x30;
	state.option_patnum = 0x37;
	return state;
}

[[nodiscard]] RandomState patterned_random()
{
	RandomState state{};
	for(std::size_t i = 0; i < state.ring.size(); ++i) {
		state.ring[i] = static_cast<std::uint8_t>(i);
	}
	state.seed = 0x12345678U;
	state.cursor = 20;
	return state;
}

[[nodiscard]] VolleyExpected expected_volley(std::uint8_t level,
	std::int8_t spread_angle_delta, std::uint8_t initial_patnum)
{
	VolleyExpected result{};
	result.random_cursor = 20;
	result.final_patnum = initial_patnum;
	auto add = [&result](std::int16_t left_offset, std::uint8_t angle,
		std::uint8_t patnum) {
		result.spawns[result.count++] = {left_offset, angle, patnum};
	};
	auto random_angle = [&result](std::uint8_t mask, std::uint8_t base) {
		const auto value = result.random_cursor;
		++result.random_cursor;
		return static_cast<std::uint8_t>((value & mask) + base);
	};
	switch(level) {
	case 0:
		add(8, 192U, initial_patnum);
		break;
	case 1:
		add(8, random_angle(3U, 190U), initial_patnum);
		break;
	case 2:
		add(0, random_angle(7U, 188U), initial_patnum);
		add(16, random_angle(7U, 188U), initial_patnum);
		break;
	case 3:
		add(0, 192U, initial_patnum);
		add(16, 192U, initial_patnum);
		break;
	case 4:
		add(8, 192U, initial_patnum);
		add(8, 187U, initial_patnum);
		add(8, 197U, initial_patnum);
		break;
	case 5:
		add(8, 192U, initial_patnum);
		add(8, 185U, initial_patnum);
		add(8, 199U, initial_patnum);
		break;
	case 6:
		add(0, 192U, initial_patnum);
		add(16, 192U, initial_patnum);
		add(8, random_angle(31U, 177U), initial_patnum);
		add(8, random_angle(31U, 177U), initial_patnum);
		break;
	case 7:
		add(0, 192U, initial_patnum);
		add(16, 192U, initial_patnum);
		add(8, 184U, initial_patnum);
		add(8, 200U, initial_patnum);
		break;
	case 8:
		add(8, 192U, initial_patnum);
		add(8, 184U, initial_patnum);
		add(8, 200U, initial_patnum);
		add(8, 176U, initial_patnum);
		add(8, 208U, initial_patnum);
		break;
	case 9:
		add(0, 192U, initial_patnum);
		add(16, 192U, initial_patnum);
		add(8, 179U, 0x34U);
		add(8, 205U, 0x34U);
		add(-24, static_cast<std::uint8_t>(192 - spread_angle_delta), 0x36U);
		add(40, static_cast<std::uint8_t>(192 + spread_angle_delta), 0x36U);
		result.final_patnum = 0x36U;
		break;
	default:
		break;
	}
	return result;
}

void set_expected_spawn(PlayerShot &shot, std::size_t page,
	const SpawnSpec &spec, const PlayerShotWriterPositions &positions,
	std::int16_t spawn_top)
{
	shot.flag = player_shot_alive;
	shot.decay_cel = 0;
	shot.pos_on_page[page] = {
		to_subpixels(add_word(positions.player_topleft.x, spec.left_offset)),
		spawn_top,
	};
	const auto velocity = pc98::masterlib::vector2(spec.angle, 192);
	shot.velocity = {velocity.x, velocity.y};
	shot.patnum = spec.patnum;
	shot.from_option = false;
}

[[nodiscard]] bool test_all_level_patterns()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	for(unsigned raw_level = 0U; raw_level <= 9U; ++raw_level) {
		auto pool = patterned_pool();
		auto random = patterned_random();
		const auto pool_before = pool;
		const auto random_before = random;
		std::uint8_t patnum = 0x30U;
		std::uint8_t option_latch = 1U;
		const auto level = static_cast<std::uint8_t>(raw_level);
		const auto pattern = expected_volley(level, 10, patnum);
		if(!shot_a_volley(pool, positions, 0U, random, level, 10, patnum,
			option_latch)) return false;
		auto expected = pool_before;
		expected.current_slot = static_cast<std::int16_t>(pattern.count - 1U);
		expected.spawn_top = to_subpixels(add_word(200, 32));
		expected.patnum = pattern.final_patnum;
		for(std::size_t i = 0; i < pattern.count; ++i) {
			set_expected_spawn(
				expected.shots[i], 0U, pattern.spawns[i], positions,
				expected.spawn_top
			);
		}
		auto expected_random = random_before;
		expected_random.cursor = pattern.random_cursor;
		if(pool != expected || random != expected_random ||
			patnum != pattern.final_patnum || option_latch != 0U) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool test_sparse_full_and_invalid_patterns()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};

	// The scan consumes only the free slots it encounters. A partial maximum
	// volley ends at the loop limit after its third spawn and retains the
	// patnum rewrite that occurred at volley index two.
	auto partial = patterned_pool();
	for(auto &shot : partial.shots) shot.flag = player_shot_alive;
	partial.shots[0].flag = player_shot_free;
	partial.shots[5].flag = player_shot_free;
	partial.shots[37].flag = player_shot_free;
	auto partial_expected = partial;
	auto random = patterned_random();
	const auto random_before = random;
	std::uint8_t patnum = 0x30U;
	std::uint8_t option_latch = 1U;
	if(!shot_a_volley(partial, positions, 1U, random, 9U, 10, patnum,
		option_latch)) return false;
	partial_expected.current_slot = 38;
	partial_expected.spawn_top = to_subpixels(add_word(200, 32));
	partial_expected.patnum = 0x34U;
	const std::array<SpawnSpec, 3> partial_spawns{{
		{0, 192U, 0x30U}, {16, 192U, 0x30U}, {8, 179U, 0x34U},
	}};
	for(std::size_t i = 0; i < partial_spawns.size(); ++i) {
		const std::array<std::size_t, 3> slots{{0U, 5U, 37U}};
		set_expected_spawn(partial_expected.shots[slots[i]], 1U,
			partial_spawns[i], positions, partial_expected.spawn_top);
	}
	if(partial != partial_expected || random != random_before || patnum != 0x34U ||
		option_latch != 0U) return false;
	// The wrapper's writer cache must rejoin the single source patnum after the
	// partial level-9 rewrite. A later level-0 volley therefore uses 34h rather
	// than the initial powered 30h after a pool entry becomes available again.
	partial.shots[4].flag = player_shot_free;
	auto next_expected = partial;
	option_latch = 1U;
	if(!shot_a_volley(partial, positions, 1U, random, 0U, 10, patnum,
		option_latch)) return false;
	next_expected.current_slot = 4;
	next_expected.spawn_top = to_subpixels(add_word(200, 32));
	next_expected.patnum = 0x34U;
	set_expected_spawn(next_expected.shots[4], 1U, {8, 192U, 0x34U}, positions,
		next_expected.spawn_top);
	if(partial != next_expected || random != random_before || patnum != 0x34U ||
		option_latch != 0U) return false;

	// No free slot still reaches the source loop's terminal index and clears
	// the option latch without consuming a random byte.
	auto full = patterned_pool();
	for(auto &shot : full.shots) shot.flag = player_shot_alive;
	auto full_expected = full;
	random = patterned_random();
	const auto full_random_before = random;
	patnum = 0x30U;
	option_latch = 1U;
	if(!shot_a_volley(full, positions, 0U, random, 6U, 10, patnum,
		option_latch)) return false;
	full_expected.current_slot = 38;
	full_expected.spawn_top = to_subpixels(add_word(200, 32));
	if(full != full_expected || random != full_random_before || patnum != 0x30U ||
		option_latch != 0U) return false;

	// The source switch has no default: an out-of-range byte visits the first
	// free slot, writes no record, then breaks because both volley counters are
	// zero. It still establishes spawn height and clears the latch.
	auto invalid_level = patterned_pool();
	invalid_level.shots[0].flag = player_shot_alive;
	invalid_level.shots[1].flag = player_shot_alive;
	auto invalid_expected = invalid_level;
	random = patterned_random();
	const auto invalid_random_before = random;
	patnum = 0x30U;
	option_latch = 1U;
	if(!shot_a_volley(invalid_level, positions, 0U, random, 10U, 10, patnum,
		option_latch)) return false;
	invalid_expected.current_slot = 2;
	invalid_expected.spawn_top = to_subpixels(add_word(200, 32));
	if(invalid_level != invalid_expected || random != invalid_random_before ||
		patnum != 0x30U || option_latch != 0U) return false;
	return true;
}

[[nodiscard]] bool test_random_cursor_wrap()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	auto pool = patterned_pool();
	for(auto &shot : pool.shots) shot.flag = player_shot_alive;
	pool.shots[1].flag = player_shot_free;
	pool.shots[36].flag = player_shot_free;
	auto expected = pool;
	auto random = patterned_random();
	random.cursor = 0xFFU;
	std::uint8_t patnum = 0x30U;
	std::uint8_t option_latch = 1U;
	if(!shot_a_volley(pool, positions, 0U, random, 2U, 10, patnum,
		option_latch)) return false;

	expected.current_slot = 36;
	expected.spawn_top = to_subpixels(add_word(200, 32));
	set_expected_spawn(expected.shots[1], 0U, {0, 195U, 0x30U}, positions,
		expected.spawn_top);
	set_expected_spawn(expected.shots[36], 0U, {16, 188U, 0x30U}, positions,
		expected.spawn_top);
	return pool == expected && random.cursor == 1U && patnum == 0x30U &&
		option_latch == 0U;
}

[[nodiscard]] bool test_invalid_page_is_atomic()
{
	auto pool = patterned_pool();
	auto random = patterned_random();
	const auto pool_before = pool;
	const auto random_before = random;
	std::uint8_t patnum = 0x30U;
	std::uint8_t option_latch = 1U;
	if(shot_a_volley(pool, {{100, 200}, {300, 400}}, 2U, random, 9U, 10,
		patnum, option_latch)) return false;
	return pool == pool_before && random == random_before && patnum == 0x30U &&
		option_latch == 1U;
}

struct ShotAAdapter final : PlayerShotStreamCallbacks {
	PlayerShotPoolState &pool;
	const PlayerShotWriterPositions &positions;
	std::size_t back_page{};
	RandomState &random;
	std::uint8_t &option_latch;
	bool volley_accepted{};
	std::uint8_t sound_patnum{};
	std::uint8_t sound_option_patnum{};
	std::size_t sound_count{};

	ShotAAdapter(PlayerShotPoolState &pool_arg,
		const PlayerShotWriterPositions &positions_arg, std::size_t back_page_arg,
		RandomState &random_arg, std::uint8_t &option_latch_arg) noexcept
		: pool(pool_arg), positions(positions_arg), back_page(back_page_arg),
		  random(random_arg), option_latch(option_latch_arg)
	{
	}

	PlayerShotStreamResult shot_volley(const PlayerShotStreamState &state, std::uint8_t &patnum,
		std::uint8_t &) noexcept override
	{
		volley_accepted = shot_a_volley(pool, positions, back_page, random,
			state.shot_level, state.spread_angle_delta, patnum, option_latch);
		return volley_accepted ? PlayerShotStreamResult::completed :
			PlayerShotStreamResult::volley_invalid_page;
	}

	void shot_sound(const PlayerShotStreamState &state) noexcept override
	{
		sound_patnum = state.shot_patnum;
		sound_option_patnum = state.shot_option_patnum;
		++sound_count;
	}

	void bomb_request(const PlayerShotStreamState &) noexcept override {}
};

[[nodiscard]] bool test_stream_callback_position()
{
	auto pool = patterned_pool();
	auto random = patterned_random();
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	std::uint8_t option_latch = 1U;
	ShotAAdapter adapter{pool, positions, 0U, random, option_latch};
	PlayerShotStreamState stream{0xFFU, 0xFFU, 0U, 0U, 9U, 0U, 0U,
		0x30U, 0x37U, 10};
	return player_move_and_shoot_shot_stream_step(stream, 0x0010U, adapter) ==
		PlayerShotStreamResult::completed && adapter.volley_accepted &&
		adapter.sound_count == 1U &&
		adapter.sound_patnum == 0x36U && adapter.sound_option_patnum == 0x37U &&
		stream.shot_patnum == 0x36U && stream.shot_option_patnum == 0x37U &&
		pool.current_slot == 5 && pool.shots[5].patnum == 0x36U &&
		option_latch == 0U;
}

} // namespace

int main()
{
	if(!test_all_level_patterns()) {
		std::fputs("FAIL level patterns\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_sparse_full_and_invalid_patterns()) {
		std::fputs("FAIL sparse/full/invalid patterns\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_random_cursor_wrap()) {
		std::fputs("FAIL random cursor wrap\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_invalid_page_is_atomic()) {
		std::fputs("FAIL invalid page\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_stream_callback_position()) {
		std::fputs("FAIL stream callback\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
