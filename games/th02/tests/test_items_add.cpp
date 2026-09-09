#include "th02/core/item.hpp"
#include "th02/core/random.hpp"
#include "th02/core/score.hpp"

#include <bit>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] ItemPoolState patterned_pool() noexcept
{
	ItemPoolState state{};
	state.bigpower_override = 0x1234U;
	state.miss_add_gameover = true;
	state.semirandom_ring_p = 7U;
	state.semirandom_cycle = 8U;
	for(std::size_t i = 0U; i < state.items.size(); ++i) {
		auto &item = state.items[i];
		item.flag = item_remove;
		item.type = static_cast<ItemType>(static_cast<std::uint8_t>(i + 20U));
		item.position[0] = {
			static_cast<std::int16_t>(100 + static_cast<int>(i)),
			static_cast<std::int16_t>(200 + static_cast<int>(i)),
		};
		item.position[1] = {
			static_cast<std::int16_t>(300 + static_cast<int>(i)),
			static_cast<std::int16_t>(400 + static_cast<int>(i)),
		};
		item.velocity_y = static_cast<std::int16_t>(500 + static_cast<int>(i));
		item.velocity_x_during_bounce = static_cast<std::int16_t>(
			600 + static_cast<int>(i)
		);
		item.age = static_cast<std::int16_t>(700 + static_cast<int>(i));
	}
	return state;
}

[[nodiscard]] RandomState patterned_random() noexcept
{
	RandomState random{};
	random.seed = 0xCAFEBABEU;
	for(std::size_t i = 0U; i < random.ring.size(); ++i) {
		random.ring[i] = static_cast<std::uint8_t>(i);
	}
	return random;
}

void make_free(ItemPoolState &state, std::size_t index) noexcept
{
	state.items[index].flag = item_free;
}

void expect_direct_spawn(Item &item, ItemType type, std::int16_t left,
	std::int16_t top) noexcept
{
	item.flag = item_alive;
	item.type = type;
	item.position[0] = {left, to_subpixels(top)};
	item.position[1] = {left, to_subpixels(top)};
	item.velocity_y = -48;
	item.velocity_x_during_bounce = 0;
	item.age = 0;
}

[[nodiscard]] bool direct_override_precedes_full_pool_scan() noexcept
{
	auto state = patterned_pool();
	state.bigpower_override = 2U;
	const auto full_before = state;
	ScoreState score{};
	items_add(state, score, -5, 32767,
		static_cast<std::int16_t>(ItemType::point));
	if(state.items != full_before.items || state.bigpower_override != 1U ||
		state.miss_add_gameover != full_before.miss_add_gameover ||
		state.semirandom_ring_p != full_before.semirandom_ring_p ||
		state.semirandom_cycle != full_before.semirandom_cycle) {
		return false;
	}

	make_free(state, 6U);
	auto expected = state;
	expected.bigpower_override = 0U;
	expect_direct_spawn(expected.items[6U], ItemType::big_power, -5, 32767);
	items_add(state, score, -5, 32767,
		static_cast<std::int16_t>(ItemType::power));
	return state == expected;
}

[[nodiscard]] bool direct_lives_conversion_and_byte_narrowing() noexcept
{
	auto state = patterned_pool();
	state.bigpower_override = 0U;
	make_free(state, 1U);
	make_free(state, 8U);
	make_free(state, 19U);
	const auto before = state;
	ScoreState score{};
	score.lives = 5;
	items_add(state, score, 17, -2,
		static_cast<std::int16_t>(ItemType::one_up));
	score.lives = 6;
	items_add(state, score, 18, -1,
		static_cast<std::int16_t>(ItemType::one_up));
	items_add(state, score, 19, 0, static_cast<std::int16_t>(0x1234));

	auto expected = before;
	expect_direct_spawn(expected.items[1U], ItemType::bomb, 17, -2);
	expect_direct_spawn(expected.items[8U], ItemType::one_up, 18, -1);
	expect_direct_spawn(expected.items[19U], static_cast<ItemType>(0x34U), 19, 0);
	return state == expected && score.lives == 6;
}

[[nodiscard]] bool semirandom_override_and_cycle_skip_preserve_random() noexcept
{
	auto state = patterned_pool();
	state.bigpower_override = 1U;
	state.semirandom_ring_p = 4U;
	state.semirandom_cycle = 2U;
	make_free(state, 3U);
	auto random = patterned_random();
	const auto random_before = random;
	ScoreState score{};

	if(!items_add_semirandom(state, random, score, 20, 30)) return false;
	auto expected = patterned_pool();
	expected.bigpower_override = 0U;
	expected.semirandom_ring_p = 4U;
	expected.semirandom_cycle = 2U;
	expect_direct_spawn(expected.items[3U], ItemType::big_power, 20, 30);
	if(state != expected || random != random_before) return false;

	state.bigpower_override = 0U;
	state.semirandom_cycle = 1U;
	const auto before_skip = state;
	if(!items_add_semirandom(state, random, score, 20, 30)) return false;
	return state.items == before_skip.items && state.bigpower_override == 0U &&
		state.semirandom_ring_p == before_skip.semirandom_ring_p &&
		state.semirandom_cycle == 2U && random == random_before;
}

[[nodiscard]] bool semirandom_word_random_ring_and_wrap() noexcept
{
	auto state = patterned_pool();
	state.bigpower_override = 0U;
	state.semirandom_ring_p = 9U;
	state.semirandom_cycle = 0U;
	make_free(state, 2U);
	auto random = patterned_random();
	random.ring[0U] = 18U;
	random.ring[1U] = 1U;
	ScoreState score{};

	if(!items_add_semirandom(state, random, score, -20, 4)) return false;
	auto expected = patterned_pool();
	expected.bigpower_override = 0U;
	expected.semirandom_ring_p = 0U;
	expected.semirandom_cycle = 1U;
	expect_direct_spawn(expected.items[2U], ItemType::power, -20, 4);
	auto expected_random = patterned_random();
	expected_random.cursor = 1U;
	expected_random.ring[0U] = 18U;
	expected_random.ring[1U] = 1U;
	if(state != expected || random != expected_random) return false;

	// The source evaluates the old byte for modulo, then increments it. 255
	// therefore fires (255 % 3 == 0) and wraps to 0 after the call.
	state.semirandom_cycle = 0xFFU;
	state.semirandom_ring_p = 0U;
	make_free(state, 5U);
	auto wrap_expected = state;
	wrap_expected.semirandom_cycle = 0U;
	wrap_expected.semirandom_ring_p = 1U;
	expect_direct_spawn(wrap_expected.items[5U], ItemType::power, -20, 4);
	auto wrap_random_expected = random;
	wrap_random_expected.cursor = 2U;
	return items_add_semirandom(state, random, score, -20, 4) &&
		state == wrap_expected && random == wrap_random_expected;
}

[[nodiscard]] bool semirandom_rare_bomb_reaches_invalid_pointer() noexcept
{
	auto state = patterned_pool();
	state.bigpower_override = 0U;
	state.semirandom_ring_p = 0xFFU;
	state.semirandom_cycle = 0U;
	make_free(state, 4U);
	auto random = patterned_random();
	random.ring[0U] = 18U;
	random.ring[1U] = 0U;
	ScoreState score{};

	if(!items_add_semirandom(state, random, score, 11, 12)) return false;
	auto expected = patterned_pool();
	expected.bigpower_override = 0U;
	expected.semirandom_ring_p = 0U;
	expected.semirandom_cycle = 1U;
	expect_direct_spawn(expected.items[4U], ItemType::bomb, 11, 12);
	auto expected_random = patterned_random();
	expected_random.cursor = 1U;
	expected_random.ring[0U] = 18U;
	expected_random.ring[1U] = 0U;
	return state == expected && random == expected_random;
}

[[nodiscard]] bool semirandom_ring_uses_all_ten_source_bytes() noexcept
{
	auto state = patterned_pool();
	state.bigpower_override = 0U;
	state.semirandom_ring_p = 0U;
	state.semirandom_cycle = 0U;
	for(std::size_t i = 0U; i < 10U; ++i) make_free(state, i);
	auto random = patterned_random();
	for(std::size_t i = 0U; i <= 10U; ++i) random.ring[i] = 1U;
	ScoreState score{};

	for(std::size_t i = 0U; i < 10U; ++i) {
		if(!items_add_semirandom(state, random, score, 30, -7)) return false;
		if(i != 9U) {
			if(!items_add_semirandom(state, random, score, 30, -7) ||
				!items_add_semirandom(state, random, score, 30, -7)) {
				return false;
			}
		}
	}

	auto expected = patterned_pool();
	expected.bigpower_override = 0U;
	expected.semirandom_ring_p = 0U;
	expected.semirandom_cycle = 28U;
	constexpr std::array<ItemType, 10U> expected_types{
		ItemType::power,
		ItemType::point,
		ItemType::power,
		ItemType::point,
		ItemType::point,
		ItemType::power,
		ItemType::power,
		ItemType::point,
		ItemType::point,
		ItemType::power,
	};
	for(std::size_t i = 0U; i < expected_types.size(); ++i) {
		expect_direct_spawn(expected.items[i], expected_types[i], 30, -7);
	}
	auto expected_random = patterned_random();
	for(std::size_t i = 0U; i <= 10U; ++i) expected_random.ring[i] = 1U;
	expected_random.cursor = 10U;
	return state == expected && random == expected_random;
}

[[nodiscard]] bool semirandom_invalid_index_stops_at_actual_read() noexcept
{
	auto state = patterned_pool();
	state.bigpower_override = 0U;
	state.semirandom_ring_p = 10U;
	state.semirandom_cycle = 0U;
	make_free(state, 5U);
	const auto before = state;
	auto random = patterned_random();
	random.ring[0U] = 1U;
	random.ring[1U] = 0U;
	const auto random_before = random;
	ScoreState score{};

	if(items_add_semirandom(state, random, score, 0, 0)) return false;
	auto expected = before;
	expected.semirandom_cycle = 1U;
	auto expected_random = random_before;
	expected_random.cursor = 1U;
	return state == expected && random == expected_random;
}

}

int main()
{
	if(!direct_override_precedes_full_pool_scan()) {
		std::fputs("FAIL direct override/full-pool prefix\n", stderr);
		return EXIT_FAILURE;
	}
	if(!direct_lives_conversion_and_byte_narrowing()) {
		std::fputs("FAIL direct lives/narrowing\n", stderr);
		return EXIT_FAILURE;
	}
	if(!semirandom_override_and_cycle_skip_preserve_random()) {
		std::fputs("FAIL semirandom override/cycle\n", stderr);
		return EXIT_FAILURE;
	}
	if(!semirandom_word_random_ring_and_wrap()) {
		std::fputs("FAIL semirandom word/ring wrap\n", stderr);
		return EXIT_FAILURE;
	}
	if(!semirandom_rare_bomb_reaches_invalid_pointer()) {
		std::fputs("FAIL semirandom rare invalid-pointer path\n", stderr);
		return EXIT_FAILURE;
	}
	if(!semirandom_ring_uses_all_ten_source_bytes()) {
		std::fputs("FAIL semirandom source ring\n", stderr);
		return EXIT_FAILURE;
	}
	if(!semirandom_invalid_index_stops_at_actual_read()) {
		std::fputs("FAIL semirandom actual-read refusal\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
