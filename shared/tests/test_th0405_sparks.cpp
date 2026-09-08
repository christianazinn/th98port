#include "pc98/th0405_sparks.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string_view>

namespace {

using namespace pc98::th0405;

std::size_t checks{};
std::size_t failures{};

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(stderr, "FAIL: %.*s\n", static_cast<int>(message.size()), message.data());
	}
}

template <std::size_t Count>
SparkPool pool_for(std::array<Spark, Count> &slots, std::uint16_t &ring_offset)
{
	return {std::span<Spark>{slots}, ring_offset};
}

RandomState random_with_word(std::uint16_t word)
{
	RandomState random{};
	random.ring[0] = static_cast<std::uint8_t>(word);
	random.ring[1] = static_cast<std::uint8_t>(word >> 8U);
	return random;
}

void test_pool_layout_and_refusal()
{
	static_assert(sizeof(Spark) == 16U);
	static_assert(offsetof(Spark, angle) == 14U);
	std::array<Spark, SPARK_COUNT_TH04> th04{};
	std::array<Spark, SPARK_COUNT_TH05> th05{};
	std::uint16_t good_ring{};
	expect(spark_pool_valid(pool_for(th04, good_ring)), "96-slot TH04 pool is valid");
	expect(spark_pool_valid(pool_for(th05, good_ring)), "64-slot TH05 pool is valid");

	std::array<Spark, SPARK_COUNT_TH05 - 1U> short_pool{};
	std::uint16_t short_ring{};
	auto random = random_with_word(7U);
	const auto short_before = short_pool;
	const auto random_before = random;
	expect(
		!sparks_add_random(pool_for(short_pool, short_ring), random, {0, 0}, 0, 1U) &&
		short_pool == short_before && short_ring == 0U && random == random_before,
		"wrong capacity refuses before pool or RNG mutation"
	);

	std::uint16_t misaligned_ring = 1U;
	random = random_with_word(7U);
	const auto th05_before = th05;
	const auto misaligned_before = random;
	expect(
		!sparks_add_random(pool_for(th05, misaligned_ring), random, {0, 0}, 0, 1U) &&
		th05 == th05_before && misaligned_ring == 1U && random == misaligned_before,
		"misaligned byte ring refuses before pool or RNG mutation"
	);

	constexpr auto th05_bytes = SPARK_COUNT_TH05 * sizeof(Spark);
	std::uint16_t out_of_range_ring = static_cast<std::uint16_t>(th05_bytes);
	random = random_with_word(7U);
	const auto out_of_range_before = random;
	expect(
		!sparks_add_random(pool_for(th05, out_of_range_ring), random, {0, 0}, 0, 1U) &&
		th05 == th05_before && out_of_range_ring == th05_bytes &&
			random == out_of_range_before,
		"out-of-range byte ring refuses before pool or RNG mutation"
	);
}

void test_clip_and_free_allocation()
{
	std::array<Spark, SPARK_COUNT_TH05> slots{};
	std::uint16_t ring{};
	RandomState invalid_random{};
	invalid_random.cursor = 256U;
	const auto before_slots = slots;
	const auto before_random = invalid_random;
	const auto clipped = sparks_add_random(
		pool_for(slots, ring), invalid_random, {6145, 0}, 0, 4U
	);
	expect(
		clipped == SparksAddRandomResult{} && slots == before_slots && ring == 0U &&
			invalid_random == before_random,
		"source clip returns before validating or mutating pool/RNG"
	);
	const auto valid_pool_before = slots;
	const auto invalid_rng_before = invalid_random;
	expect(
		!sparks_add_random(pool_for(slots, ring), invalid_random, {0, 0}, 0, 1U) &&
			slots == valid_pool_before && ring == 0U && invalid_random == invalid_rng_before,
		"unsupported randring cursor refuses before a free-slot write"
	);

	slots[0].angle = 0x8A40U;
	slots[0].center.prev = {123, 456};
	slots[0].center.velocity = {99, -99};
	auto random = random_with_word(7U);
	const auto added = sparks_add_random(pool_for(slots, ring), random, {320, 480}, 160, 1U);
	expect(
		added == SparksAddRandomResult{1U, 1U} && ring == sizeof(Spark) &&
			random.cursor == 1U && slots[0].flag == SPARK_ALIVE && slots[0].age == 0U &&
			slots[0].center.cur == SubpixelPoint{320, 480} &&
			slots[0].center.prev == SubpixelPoint{123, 456} &&
			slots[0].center.velocity == SubpixelPoint{0, 167} &&
			slots[0].angle == 0x8A40U,
		"free allocation writes exact center and Q8 velocity while retaining previous/angle"
	);
}

void test_occupied_and_zero_count()
{
	std::array<Spark, SPARK_COUNT_TH05> slots{};
	std::uint16_t ring = sizeof(Spark);
	slots[1] = {SPARK_ALIVE, 33U, {{7, 8}, {9, 10}, {11, 12}}, 0x1234U};
	auto random = random_with_word(23U);
	const auto before_slot = slots[1];
	const auto before_random = random;
	const auto occupied = sparks_add_random(pool_for(slots, ring), random, {0, 0}, 0, 1U);
	expect(
		occupied == SparksAddRandomResult{1U, 0U} && ring == 2U * sizeof(Spark) &&
			slots[1] == before_slot && random == before_random,
		"occupied slot advances only the spark ring and consumes no draw"
	);

	for(auto &slot : slots) slot.flag = SPARK_ALIVE;
	ring = 0U;
	random = random_with_word(23U);
	const auto full_before = slots;
	const auto zero_before = random;
	const auto zero = sparks_add_random(pool_for(slots, ring), random, {0, 0}, 0, 0U);
	expect(
		zero == SparksAddRandomResult{65536U, 0U} && ring == 0U && slots == full_before &&
			random == zero_before,
		"zero count performs the unsigned-word 65536-attempt scan rather than no-op"
	);
}

void test_update_lifetime_and_bounds()
{
	std::array<Spark, SPARK_COUNT_TH05> slots{};
	std::uint16_t ring{};
	slots[0] = {SPARK_FREE, 18U, {{1, 2}, {3, 4}, {5, 6}}, 7U};
	slots[1] = {SPARK_REMOVE, 19U, {{10, 11}, {12, 13}, {14, 15}}, 16U};
	slots[2] = {SPARK_ALIVE, 40U, {{160, 320}, {0, 0}, {16, -2}}, 0U};
	slots[3] = {SPARK_ALIVE, 255U, {{160, 320}, {0, 0}, {0, 0}}, 0U};
	slots[4] = {SPARK_ALIVE, 1U, {{6208, 160}, {0, 0}, {0, 0}}, 0U};
	slots[5] = {SPARK_ALIVE, 1U, {{160, 5952}, {0, 0}, {0, 0}}, 0U};
	const auto free_before = slots[0];
	expect(sparks_update(pool_for(slots, ring)), "valid active pool updates");
	expect(slots[0] == free_before, "free slot is completely retained");
	expect(
		slots[1].flag == SPARK_FREE && slots[1].age == 19U &&
			slots[1].center == Motion{{10, 11}, {12, 13}, {14, 15}},
		"non-alive flag becomes free without clearing the remaining record"
	);
	expect(
		slots[2].flag == SPARK_REMOVE && slots[2].age == 41U &&
			slots[2].center == Motion{{176, 318}, {160, 320}, {16, -1}},
		"motion, gravity, then age threshold keeps removal for the next frame"
	);
	expect(
		slots[3].flag == SPARK_ALIVE && slots[3].age == 0U &&
			slots[3].center == Motion{{160, 320}, {160, 320}, {0, 1}},
		"unsigned age overflow remains live after gravity"
	);
	expect(
		slots[4].flag == SPARK_REMOVE && slots[4].age == 1U &&
			slots[5].flag == SPARK_REMOVE && slots[5].age == 1U,
		"half-sprite unsigned right and bottom bounds remove before gravity/age"
	);
}

} // namespace

int main()
{
	test_pool_layout_and_refusal();
	test_clip_and_free_allocation();
	test_occupied_and_zero_count();
	test_update_lifetime_and_bounds();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return EXIT_FAILURE;
	}
	std::printf("%zu TH04/05 spark pool checks passed\n", checks);
	return EXIT_SUCCESS;
}
