#include "th02/core/item.hpp"
#include "th02/core/random.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace {
using namespace th02::core;

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels)
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] ItemPoolState patterned_pool()
{
	ItemPoolState state{};
	for(std::size_t i = 0U; i < state.items.size(); ++i) {
		auto &item = state.items[i];
		item.flag = static_cast<std::uint8_t>(item_remove + (i % 3U));
		item.type = static_cast<ItemType>(i % 5U);
		item.position = {{
			{static_cast<std::int16_t>(100 + static_cast<int>(i)),
				static_cast<std::int16_t>(200 + static_cast<int>(i))},
			{static_cast<std::int16_t>(300 + static_cast<int>(i)),
				static_cast<std::int16_t>(400 + static_cast<int>(i))},
		}};
		item.velocity_y = static_cast<std::int16_t>(500 + static_cast<int>(i));
		item.velocity_x_during_bounce = static_cast<std::int16_t>(
			600 + static_cast<int>(i)
		);
		item.age = static_cast<std::int16_t>(700 + static_cast<int>(i));
	}
	return state;
}

[[nodiscard]] RandomState patterned_random()
{
	RandomState random{};
	random.seed = 0xCAFEBABEU;
	for(std::size_t i = 0U; i < random.ring.size(); ++i) {
		random.ring[i] = static_cast<std::uint8_t>(i);
	}
	return random;
}

void set_free(ItemPoolState &state, std::size_t index)
{
	state.items[index].flag = item_free;
}

void expected_spawn(Item &item, ItemType type, std::int16_t left,
	std::int16_t top, std::int16_t velocity_y, std::int16_t velocity_x
)
{
	item.flag = item_alive;
	item.type = type;
	item.position[0] = {left, to_subpixels(top)};
	item.position[1] = {left, to_subpixels(top)};
	item.velocity_y = velocity_y;
	item.velocity_x_during_bounce = velocity_x;
	item.age = 0;
}

[[nodiscard]] bool test_normal_sparse_slots_and_rng_prefix()
{
	auto state = patterned_pool();
	constexpr std::array<std::size_t, item_miss_count> slots{1U, 4U, 7U, 11U, 19U};
	for(const auto slot : slots) set_free(state, slot);
	const auto before = state;

	auto random = patterned_random();
	random.cursor = 254U;
	random.ring[254] = 1U;
	random.ring[255] = 3U;
	random.ring[0] = 2U;
	random.ring[1] = 2U;
	random.ring[2] = 3U;
	random.ring[3] = 3U;
	random.ring[4] = 4U;
	const auto random_before = random;

	// The spawn left is deliberately outside the playfield. Trajectory still
	// selects the center table from the separate source player position 129.
	items_miss_add(state, random, -37, -4, 129);

	auto expected = before;
	constexpr std::array<ItemType, item_miss_count> types{
		ItemType::point, ItemType::power, ItemType::big_power,
		ItemType::point, ItemType::power,
	};
	constexpr std::array<std::int16_t, item_miss_count> velocity_y{
		-56, -68, -80, -68, -56,
	};
	constexpr std::array<std::int16_t, item_miss_count> velocity_x{
		-2, -1, 0, 1, 2,
	};
	for(std::size_t i = 0U; i < slots.size(); ++i) {
		expected_spawn(expected.items[slots[i]], types[i], -37, -4,
			velocity_y[i], velocity_x[i]);
	}
	auto expected_random = random_before;
	expected_random.cursor = 5U;
	return state == expected && random == expected_random;
}

[[nodiscard]] bool test_full_and_partial_pools()
{
	auto full = patterned_pool();
	auto full_random = patterned_random();
	const auto full_before = full;
	const auto full_random_before = full_random;
	items_miss_add(full, full_random, 20, 30, 256);
	if(full != full_before || full_random != full_random_before) return false;

	auto partial = patterned_pool();
	set_free(partial, 3U);
	set_free(partial, 17U);
	const auto partial_before = partial;
	auto partial_random = patterned_random();
	partial_random.ring[0] = 1U;
	partial_random.ring[1] = 1U;
	partial_random.ring[2] = 1U;
	partial_random.ring[3] = 0U;
	const auto partial_random_before = partial_random;
	items_miss_add(partial, partial_random, 300, 1, 128);

	auto partial_expected = partial_before;
	expected_spawn(partial_expected.items[3U], ItemType::point, 300, 1, -80, 2);
	expected_spawn(partial_expected.items[17U], ItemType::power, 300, 1, -68, 2);
	auto partial_random_expected = partial_random_before;
	partial_random_expected.cursor = 4U;
	return partial == partial_expected && partial_random == partial_random_expected;
}

[[nodiscard]] bool test_normal_full_quota_forces_final_big_power()
{
	auto state = patterned_pool();
	for(std::size_t i = 0U; i < static_cast<std::size_t>(item_miss_count); ++i) {
		set_free(state, i);
	}
	const auto before = state;
	auto random = patterned_random();
	for(const auto index : {0U, 2U, 4U, 6U}) {
		random.ring[index] = 1U;
	}
	for(const auto index : {1U, 3U, 5U, 7U}) {
		random.ring[index] = 0U;
	}
	random.ring[8U] = 0xFFU;
	const auto random_before = random;

	items_miss_add(state, random, 12, 2, 257);

	auto expected = before;
	for(std::size_t i = 0U; i < 4U; ++i) {
		expected_spawn(expected.items[i], ItemType::power, 12, 2,
			std::array<std::int16_t, 4U>{-80, -68, -56, -44}[i], -2);
	}
	expected_spawn(expected.items[4U], ItemType::big_power, 12, 2, -32, -2);
	auto expected_random = random_before;
	expected_random.cursor = 9U;
	return state == expected && random == expected_random;
}

[[nodiscard]] bool test_gameover_quota_and_velocity_boundaries()
{
	struct VelocityCase {
		std::int16_t source_player_x;
		std::array<std::int16_t, item_miss_count> velocity_y;
		std::array<std::int16_t, item_miss_count> velocity_x;
	};
	constexpr std::array<VelocityCase, 3U> cases{{
		{128, {-80, -68, -56, -44, -32}, {2, 2, 2, 2, 2}},
		{256, {-56, -68, -80, -68, -56}, {-2, -1, 0, 1, 2}},
		{257, {-80, -68, -56, -44, -32}, {-2, -2, -2, -2, -2}},
	}};

	for(const auto &velocity_case : cases) {
		auto state = patterned_pool();
		state.miss_add_gameover = true;
		for(std::size_t i = 0U; i < 6U; ++i) set_free(state, i);
		const auto before = state;
		auto random = patterned_random();
		random.cursor = 240U;
		const auto random_before = random;

		items_miss_add(state, random, -123, 32767, velocity_case.source_player_x);

		auto expected = before;
		for(std::size_t i = 0U; i < static_cast<std::size_t>(item_miss_count); ++i) {
			expected_spawn(expected.items[i], ItemType::big_power, -123, 32767,
				velocity_case.velocity_y[i], velocity_case.velocity_x[i]);
		}
		// Slot 5 stays free and byte-for-byte unchanged after the five-item quota.
		if(state != expected || random != random_before) return false;
	}
	return true;
}

} // namespace

int main()
{
	if(!test_normal_sparse_slots_and_rng_prefix()) {
		std::fputs("FAIL normal sparse/RNG\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_full_and_partial_pools()) {
		std::fputs("FAIL full/partial\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_normal_full_quota_forces_final_big_power()) {
		std::fputs("FAIL final big-power/RNG\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_gameover_quota_and_velocity_boundaries()) {
		std::fputs("FAIL gameover/boundaries\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
