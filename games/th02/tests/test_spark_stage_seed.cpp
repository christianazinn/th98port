#include "th02/core/random.hpp"
#include "th02/core/spark.hpp"

#include <array>
#include <cstdlib>

namespace {
using namespace th02::core;

[[nodiscard]] std::uint16_t reference_irand(std::uint32_t &seed)
{
	seed = (seed * 0x015A4E35U) + 1U;
	return static_cast<std::uint16_t>((seed >> 16U) & 0x7FFFU);
}

[[nodiscard]] SparkPoolState patterned_sparks()
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

[[nodiscard]] RandomState patterned_random(std::uint32_t seed)
{
	RandomState random{};
	random.seed = seed;
	for(std::size_t i = 0U; i < random.ring.size(); ++i) {
		random.ring[i] = static_cast<std::uint8_t>(i * 5U);
	}
	random.cursor = 0xE1U;
	return random;
}

[[nodiscard]] bool test_spark_seed_mode(bool reduce_effects)
{
	auto state = patterned_sparks();
	const auto before = state;
	auto random = patterned_random(reduce_effects ? 0xCAFEBABEU : 18U);
	const auto random_before = random;
	const auto defaults = sparks_stage_seed(state, random, reduce_effects);
	const auto expected_defaults = reduce_effects ?
		SparkStageSeedDefaults{7U, 24U} : SparkStageSeedDefaults{1U, 48U};
	if(defaults != expected_defaults || random.ring != random_before.ring ||
		random.cursor != random_before.cursor || state.ring_index != before.ring_index) {
		return false;
	}

	auto expected = before;
	auto reference_seed = random_before.seed;
	for(std::size_t i = 0U; i < expected.sparks.size(); ++i) {
		auto &spark = expected.sparks[i];
		spark.flag = spark_free;
		spark.angle = static_cast<std::uint8_t>(reference_irand(reference_seed));
		spark.speed_base = static_cast<std::uint8_t>(
			reference_irand(reference_seed) & 0x3FU
		);
		spark.default_render_as =
			((static_cast<std::size_t>(defaults.sprite_interval) & i) == 0U) ?
				SparkRenderAs::sprite : SparkRenderAs::dot;
	}
	if(state != expected || random.seed != reference_seed) return false;

	// The source's bitwise interval test is not a periodic modulo test.
	if(!reduce_effects) {
		return state.sparks[0].default_render_as == SparkRenderAs::sprite &&
			state.sparks[1].default_render_as == SparkRenderAs::dot &&
			state.sparks[62].default_render_as == SparkRenderAs::sprite &&
			state.sparks[63].default_render_as == SparkRenderAs::dot;
	}
	return state.sparks[0].default_render_as == SparkRenderAs::sprite &&
		state.sparks[7].default_render_as == SparkRenderAs::dot &&
		state.sparks[8].default_render_as == SparkRenderAs::sprite &&
		state.sparks[56].default_render_as == SparkRenderAs::sprite &&
		state.sparks[63].default_render_as == SparkRenderAs::dot;
}

} // namespace

int main()
{
	return (test_spark_seed_mode(false) && test_spark_seed_mode(true)) ?
		EXIT_SUCCESS : EXIT_FAILURE;
}
