#include "th02/core/spark.hpp"

#include "pc98/masterlib_vector.hpp"

#include <bit>
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

[[nodiscard]] SparkPoolState patterned_state()
{
	SparkPoolState state{};
	for(std::size_t i = 0U; i < state.sparks.size(); ++i) {
		state.sparks[i] = {
			static_cast<std::uint8_t>(i + 2U),
			static_cast<std::uint8_t>(i + 3U),
			{{
				{static_cast<std::int16_t>(100 + static_cast<int>(i)),
					static_cast<std::int16_t>(200 + static_cast<int>(i))},
				{static_cast<std::int16_t>(300 + static_cast<int>(i)),
					static_cast<std::int16_t>(400 + static_cast<int>(i))},
			}},
			{static_cast<std::int16_t>(500 + static_cast<int>(i)),
				static_cast<std::int16_t>(600 + static_cast<int>(i))},
			(i & 1U) == 0U ? SparkRenderAs::dot : SparkRenderAs::sprite,
			static_cast<std::int8_t>(i + 7U),
			static_cast<std::uint8_t>(i * 3U),
			static_cast<std::uint8_t>(i + 11U),
			static_cast<std::int8_t>(i + 13U),
			(i & 1U) == 0U ? SparkRenderAs::sprite : SparkRenderAs::dot,
		};
	}
	state.ring_index = 2U;
	return state;
}

[[nodiscard]] bool test_normal_spawn_preserves_unwritten_fields()
{
	auto state = patterned_state();
	const auto before = state;
	const auto velocity = pc98::masterlib::vector2(6U, 37);
	if(!sparks_add(state, 1U, -1, 400U, 24, 1, false)) return false;
	auto expected = before;
	auto &spark = expected.sparks[2];
	spark.flag = spark_alive;
	spark.age = 0U;
	spark.screen_topleft[1] = {to_subpixels(-1), 0};
	spark.velocity = {velocity.x, velocity.y};
	spark.render_as = spark.default_render_as;
	// The default mode, angle, speed_base, both unknown bytes, and front page
	// are all retained from this stage-seeded source slot.
	expected.ring_index = 3U;
	return state == expected;
}

[[nodiscard]] bool test_top_uses_one_unsigned_subtract()
{
	auto state = patterned_state();
	if(!sparks_add(state, 0U, 3, 399U, 0, 1, false) ||
		state.sparks[2].screen_topleft[0] !=
		SparkSubpixelPoint{to_subpixels(3), to_subpixels(399)}) {
		return false;
	}

	state = patterned_state();
	if(!sparks_add(state, 0U, 3, 400U, 0, 1, false) ||
		state.sparks[2].screen_topleft[0] !=
		SparkSubpixelPoint{to_subpixels(3), 0}) {
		return false;
	}

	state = patterned_state();
	if(!sparks_add(state, 0U, 3, 800U, 0, 1, false) ||
		state.sparks[2].screen_topleft[0] !=
		SparkSubpixelPoint{to_subpixels(3), to_subpixels(400)}) {
		return false;
	}

	state = patterned_state();
	const auto once_subtracted = static_cast<std::uint16_t>(0xFFFFU - 400U);
	return sparks_add(state, 0U, 3, 0xFFFFU, 0, 1, false) &&
		state.sparks[2].screen_topleft[0] == SparkSubpixelPoint{
			to_subpixels(3), to_subpixels(std::bit_cast<std::int16_t>(once_subtracted)),
		};
}

[[nodiscard]] bool test_normal_speed_is_a_signed_word_sum()
{
	auto state = patterned_state();
	state.sparks[2].speed_base = 255U;
	state.sparks[2].angle = 0U;
	if(!sparks_add(state, 0U, 0, 0U, 32767, 1, false)) return false;
	const auto velocity = pc98::masterlib::vector2(0U, -32514);
	return state.sparks[2].velocity == SparkSubpixelPoint{
		velocity.x, velocity.y,
	};
}

[[nodiscard]] bool test_sprite_ring_and_signed_word_angle()
{
	auto state = patterned_state();
	state.ring_index = 62U;
	const auto before = state;
	if(!sparks_add(state, 0U, 10, 20U, 80, 3, true)) return false;
	for(std::size_t group_i = 0U; group_i < 3U; ++group_i) {
		const auto slot = 62U + group_i < spark_count ? 62U + group_i :
			62U + group_i - spark_count;
		auto expected = before.sparks[slot];
		const auto angle = static_cast<std::uint8_t>(group_i * 256U / 3U);
		const auto velocity = pc98::masterlib::vector2(angle, 80);
		expected.flag = spark_alive;
		expected.age = 0U;
		expected.screen_topleft[0] = {to_subpixels(10), to_subpixels(20)};
		expected.velocity = {velocity.x, velocity.y};
		expected.render_as = SparkRenderAs::sprite;
		if(state.sparks[slot] != expected) return false;
	}
	if(state.ring_index != 1U) return false;

	// At group 128 the source shifts a signed 16-bit word to 0x8000 before
	// division. With count 129, that is -32768 / 129 = -254, narrowed to 2;
	// a widened group_i * 256 calculation would instead produce angle 254.
	state = patterned_state();
	state.ring_index = 0U;
	if(!sparks_add(state, 1U, 0, 0U, 80, 129, true) ||
		state.ring_index != 1U) {
		return false;
	}
	const auto wrapped_velocity = pc98::masterlib::vector2(2U, 80);
	return state.sparks[0].velocity == SparkSubpixelPoint{
		wrapped_velocity.x, wrapped_velocity.y,
	} && state.sparks[0].render_as == SparkRenderAs::sprite &&
		state.sparks[0].screen_topleft[1] == SparkSubpixelPoint{0, 0};
}

[[nodiscard]] bool test_nonpositive_counts_and_host_refusals()
{
	auto state = patterned_state();
	state.ring_index = 0xAB3EU;
	const auto before = state;
	// No source loop executes, so even an unrepresented page and high ring word
	// cannot be dereferenced. The final byte operation remains observable.
	if(!sparks_add(state, 99U, 0, 0U, 0, 0, false) ||
		state.sparks != before.sparks || state.ring_index != 0xAB3EU) {
		return false;
	}
	if(!sparks_add(state, 99U, 0, 0U, 0, -1, true) ||
		state.sparks != before.sparks || state.ring_index != 0xAB3DU) {
		return false;
	}

	for(const auto invalid_ring : {64U, 0x0100U}) {
		state = patterned_state();
		state.ring_index = static_cast<std::uint16_t>(invalid_ring);
		const auto rejected_before = state;
		if(sparks_add(state, 0U, 0, 0U, 0, 1, false) ||
			state != rejected_before) {
			return false;
		}
	}
	state = patterned_state();
	const auto rejected_before = state;
	return !sparks_add(state, 2U, 0, 0U, 0, 1, false) &&
		state == rejected_before;
}

} // namespace

int main()
{
	if(!test_normal_spawn_preserves_unwritten_fields()) {
		std::fputs("FAIL normal\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_top_uses_one_unsigned_subtract()) {
		std::fputs("FAIL top\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_normal_speed_is_a_signed_word_sum()) {
		std::fputs("FAIL normal speed\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_sprite_ring_and_signed_word_angle()) {
		std::fputs("FAIL sprite\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_nonpositive_counts_and_host_refusals()) {
		std::fputs("FAIL nonpositive/refusal\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
