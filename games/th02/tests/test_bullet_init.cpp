#include "th02/core/bullet.hpp"
#include "th02/core/random.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

using namespace th02::core;

[[nodiscard]] std::uint16_t reference_irand(std::uint32_t &seed) noexcept
{
	seed = (seed * 0x015A4E35U) + 1U;
	return static_cast<std::uint16_t>((seed >> 16U) & 0x7FFFU);
}

[[nodiscard]] EnemyBulletPoolState patterned_bullets() noexcept
{
	EnemyBulletPoolState state{};
	for(std::size_t i = 0U; i < state.bullets.size(); ++i) {
		auto &bullet = state.bullets[i];
		bullet = {
			static_cast<std::int8_t>(i + 3U),
			static_cast<std::int8_t>(i + 4U),
			{{
				{static_cast<std::int16_t>(100 + static_cast<int>(i)),
					static_cast<std::int16_t>(200 + static_cast<int>(i))},
				{static_cast<std::int16_t>(300 + static_cast<int>(i)),
					static_cast<std::int16_t>(400 + static_cast<int>(i))},
			}},
			{static_cast<std::int16_t>(500 + static_cast<int>(i)),
				static_cast<std::int16_t>(600 + static_cast<int>(i))},
			static_cast<std::uint8_t>(i + 5U),
			static_cast<std::uint8_t>(i + 6U),
			static_cast<std::uint8_t>(i + 7U),
			static_cast<std::uint8_t>(i + 8U),
			static_cast<std::uint8_t>(i + 9U),
			static_cast<std::int8_t>(i + 10U),
		};
	}
	state.rank_base_speed = -55;
	state.rank_base_stack = 0xC2U;
	state.stack = 0xA3U;
	state.easy_slow_skip_cycle = -44;
	return state;
}

[[nodiscard]] SparkPoolState patterned_sparks() noexcept
{
	SparkPoolState state{};
	for(std::size_t i = 0U; i < state.sparks.size(); ++i) {
		state.sparks[i] = {
			static_cast<std::uint8_t>(3U + i),
			static_cast<std::uint8_t>(20U + i),
			{{
				{static_cast<std::int16_t>(100 + static_cast<int>(i)),
					static_cast<std::int16_t>(200 + static_cast<int>(i))},
				{static_cast<std::int16_t>(300 + static_cast<int>(i)),
					static_cast<std::int16_t>(400 + static_cast<int>(i))},
			}},
			{static_cast<std::int16_t>(500 + static_cast<int>(i)),
				static_cast<std::int16_t>(600 + static_cast<int>(i))},
			(i & 1U) == 0U ? SparkRenderAs::dot : SparkRenderAs::sprite,
			static_cast<std::int8_t>(i), static_cast<std::uint8_t>(i * 3U),
			static_cast<std::uint8_t>(i + 7U), static_cast<std::int8_t>(i + 9U),
			(i & 1U) == 0U ? SparkRenderAs::sprite : SparkRenderAs::dot,
		};
	}
	state.ring_index = 0xAB3EU;
	return state;
}

[[nodiscard]] RandomState patterned_random(std::uint32_t seed) noexcept
{
	RandomState state{};
	state.seed = seed;
	for(std::size_t i = 0U; i < state.ring.size(); ++i) {
		state.ring[i] = static_cast<std::uint8_t>(i * 5U);
	}
	state.cursor = 0xE1U;
	return state;
}

[[nodiscard]] bool test_full_init(std::int8_t rank,
	bool reduce_effects, std::int8_t expected_speed,
	std::uint8_t expected_base_stack
) noexcept
{
	auto bullets = patterned_bullets();
	const auto bullets_before = bullets;
	auto sparks = patterned_sparks();
	const auto sparks_before = sparks;
	auto random = patterned_random(reduce_effects ? 0xCAFEBABEU : 18U);
	const auto random_before = random;
	EnemyBulletSpecialState special{-11, -22, -33};
	const auto defaults = bullets_and_sparks_init(
		bullets, sparks, random, special, rank, reduce_effects
	);
	const auto expected_defaults = reduce_effects ?
		SparkStageSeedDefaults{7U, 24U} : SparkStageSeedDefaults{1U, 48U};
	if(defaults != expected_defaults || special != EnemyBulletSpecialState{1, 64, 1} ||
		bullets.rank_base_speed != expected_speed ||
		bullets.rank_base_stack != expected_base_stack ||
		bullets.stack != expected_base_stack ||
		bullets.easy_slow_skip_cycle != bullets_before.easy_slow_skip_cycle) {
		return false;
	}

	auto expected_bullets = bullets_before;
	for(auto &bullet : expected_bullets.bullets) {
		bullet.flag = enemy_bullet_free;
	}
	expected_bullets.rank_base_speed = expected_speed;
	expected_bullets.rank_base_stack = expected_base_stack;
	expected_bullets.stack = expected_base_stack;
	if(bullets != expected_bullets) return false;

	auto expected_sparks = sparks_before;
	auto expected_seed = random_before.seed;
	for(std::size_t i = 0U; i < expected_sparks.sparks.size(); ++i) {
		auto &spark = expected_sparks.sparks[i];
		spark.flag = spark_free;
		spark.angle = static_cast<std::uint8_t>(reference_irand(expected_seed));
		spark.speed_base = static_cast<std::uint8_t>(
			reference_irand(expected_seed) & 0x3FU
		);
		spark.default_render_as =
			((static_cast<std::size_t>(defaults.sprite_interval) & i) == 0U) ?
				SparkRenderAs::sprite : SparkRenderAs::dot;
	}
	return sparks == expected_sparks && random.seed == expected_seed &&
		random.ring == random_before.ring &&
		random.cursor == random_before.cursor &&
		sparks.ring_index == sparks_before.ring_index;
}

[[nodiscard]] bool test_rank_defaults_and_reduced_effects() noexcept
{
	const struct {
		std::int8_t rank;
		std::int8_t speed;
		std::uint8_t base_stack;
	} ranks[] = {
		{-1, 0, 0U}, {0, -10, 0U}, {1, 0, 0U},
		{2, 0, 1U}, {3, 12, 1U}, {4, 0, 0U},
	};
	for(const auto rank : ranks) {
		if(!test_full_init(rank.rank, false, rank.speed, rank.base_stack) ||
			!test_full_init(rank.rank, true, rank.speed, rank.base_stack)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool test_stack_multiplier_retains_byte_wrap() noexcept
{
	auto state = patterned_bullets();
	const auto before = state;
	state.rank_base_stack = 0U;
	bullets_set_stack_multiplier(state, 0xFFU);
	if(state.stack != 0U || state.rank_base_stack != 0U ||
		state.bullets != before.bullets ||
		state.rank_base_speed != before.rank_base_speed ||
		state.easy_slow_skip_cycle != before.easy_slow_skip_cycle) {
		return false;
	}
	state.rank_base_stack = 1U;
	bullets_set_stack_multiplier(state, 0xFFU);
	if(state.stack != 0xFFU || state.rank_base_stack != 1U) return false;
	state.rank_base_stack = 200U;
	bullets_set_stack_multiplier(state, 2U);
	return state.stack == 144U && state.rank_base_stack == 200U;
}

} // namespace

int main()
{
	return (test_rank_defaults_and_reduced_effects() &&
		test_stack_multiplier_retains_byte_wrap()) ? EXIT_SUCCESS : EXIT_FAILURE;
}
