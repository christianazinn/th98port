#include "th02/core/spark.hpp"

#include <array>
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
			SparkRenderAs::dot,
		};
	}
	return state;
}

[[nodiscard]] bool test_update_state_prefix_and_bounds()
{
	auto state = patterned_state();

	state.sparks[0].flag = spark_alive;
	state.sparks[0].age = 4U;
	state.sparks[0].screen_topleft[1] = {to_subpixels(100), to_subpixels(100)};
	state.sparks[0].velocity = {16, -16};
	state.sparks[1].flag = spark_alive;
	state.sparks[1].screen_topleft[1] = {to_subpixels(31), to_subpixels(100)};
	state.sparks[1].velocity = {};
	state.sparks[2].flag = spark_alive;
	state.sparks[2].screen_topleft[1] = {to_subpixels(408), to_subpixels(100)};
	state.sparks[2].velocity = {};
	state.sparks[3].flag = spark_alive;
	state.sparks[3].screen_topleft[1] = {to_subpixels(100), to_subpixels(8)};
	state.sparks[3].velocity = {};
	state.sparks[4].flag = spark_alive;
	state.sparks[4].screen_topleft[1] = {to_subpixels(100), to_subpixels(384)};
	state.sparks[4].velocity = {};
	state.sparks[5].flag = spark_alive;
	state.sparks[5].age = 24U;
	state.sparks[5].screen_topleft[1] = {to_subpixels(100), to_subpixels(100)};
	state.sparks[5].velocity = {};
	state.sparks[6].flag = spark_remove;
	state.sparks[7].flag = 0x7FU;
	state.sparks[8].flag = spark_alive;
	state.sparks[8].screen_topleft[1] = {0x7FFF, 0};
	state.sparks[8].velocity = {0x7FFF, 0};
	state.sparks[9].flag = spark_alive;
	state.sparks[9].age = 23U;
	state.sparks[9].screen_topleft[1] = {to_subpixels(100), to_subpixels(100)};
	state.sparks[9].velocity = {};
	state.sparks[10].flag = spark_alive;
	state.sparks[10].age = 0U;
	state.sparks[10].screen_topleft[1] = {to_subpixels(32), to_subpixels(100)};
	state.sparks[10].velocity = {};
	state.sparks[11].flag = spark_alive;
	state.sparks[11].age = 0U;
	state.sparks[11].screen_topleft[1] = {to_subpixels(407), to_subpixels(100)};
	state.sparks[11].velocity = {};
	state.sparks[12].flag = spark_alive;
	state.sparks[12].age = 0U;
	state.sparks[12].screen_topleft[1] = {to_subpixels(100), to_subpixels(9)};
	state.sparks[12].velocity = {};
	state.sparks[13].flag = spark_alive;
	state.sparks[13].age = 0U;
	state.sparks[13].screen_topleft[1] = {to_subpixels(100), to_subpixels(383)};
	state.sparks[13].velocity = {};
	state.sparks[14].flag = spark_alive;
	state.sparks[14].age = 255U;
	state.sparks[14].screen_topleft[1] = {to_subpixels(100), to_subpixels(100)};
	state.sparks[14].velocity = {};
	const auto configured = state;

	if(!sparks_update(state, 1U, {-2, 24U})) return false;
	if(state.sparks[0].flag != spark_alive || state.sparks[0].age != 5U ||
		state.sparks[0].screen_topleft[1] !=
		SparkSubpixelPoint{to_subpixels(101), to_subpixels(99)} ||
		state.sparks[0].velocity != SparkSubpixelPoint{14, -15} ||
		state.sparks[0].screen_topleft[0] !=
		configured.sparks[0].screen_topleft[0]) {
		return false;
	}
	for(const std::size_t index : {1U, 2U, 3U, 4U, 5U, 8U}) {
		if(state.sparks[index].flag != spark_remove) return false;
	}
	if(state.sparks[8].screen_topleft[1].x != -2 ||
		state.sparks[8].velocity.x != static_cast<std::int16_t>(0x7FFDU) ||
		state.sparks[8].velocity.y != 1) {
		return false;
	}
	for(const std::size_t retained : {9U, 10U, 11U, 12U, 13U, 14U}) {
		if(state.sparks[retained].flag != spark_alive) return false;
	}
	if(state.sparks[9].age != 24U || state.sparks[14].age != 0U) return false;
	// F_REMOVE and every nonstandard raw flag skip the source update prefix.
	if(state.sparks[6] != configured.sparks[6] ||
		state.sparks[7] != configured.sparks[7]) {
		return false;
	}

	const auto after_update = state;
	return !sparks_update(state, 2U, {-2, 24U}) && state == after_update;
}

class RecordingInvalidation final : public SparkInvalidationCallbacks {
public:
	explicit RecordingInvalidation(const SparkPoolState &state_arg) noexcept
		: state(&state_arg)
	{
	}

	const SparkPoolState *state{};
	std::array<SparkInvalidation, 4U> rectangles{};
	std::size_t count{};
	bool callback_precedes_state_change{true};

	void invalidate_spark(SparkInvalidation invalidation) noexcept override
	{
		if(count < rectangles.size()) rectangles[count] = invalidation;
		if(state != nullptr) {
			if(count == 0U) {
				callback_precedes_state_change &= state->sparks[0].flag == spark_alive &&
					state->sparks[0].screen_topleft[1] ==
					SparkSubpixelPoint{to_subpixels(50), to_subpixels(60)};
			} else if(count == 1U) {
				callback_precedes_state_change &= state->sparks[1].flag == spark_remove &&
					state->sparks[1].screen_topleft[1] ==
					SparkSubpixelPoint{to_subpixels(100), to_subpixels(110)};
			}
		}
		++count;
	}
};

[[nodiscard]] bool test_invalidation_callback_remove_and_page_copy()
{
	auto state = patterned_state();
	for(auto &spark : state.sparks) spark.flag = spark_free;
	state.sparks[0].flag = spark_alive;
	state.sparks[0].render_as = SparkRenderAs::dot;
	state.sparks[0].screen_topleft[0] = {to_subpixels(10), to_subpixels(20)};
	state.sparks[0].screen_topleft[1] = {to_subpixels(50), to_subpixels(60)};
	state.sparks[1].flag = spark_remove;
	state.sparks[1].render_as = SparkRenderAs::sprite;
	state.sparks[1].screen_topleft[0] = {to_subpixels(11), to_subpixels(21)};
	state.sparks[1].screen_topleft[1] = {to_subpixels(100), to_subpixels(110)};
	state.sparks[2].flag = spark_free;
	state.sparks[3].flag = 0x7FU;
	state.sparks[3].render_as = static_cast<SparkRenderAs>(0x7FU);
	state.sparks[3].screen_topleft[0] = {to_subpixels(12), to_subpixels(22)};
	state.sparks[3].screen_topleft[1] = {to_subpixels(150), to_subpixels(160)};
	state.sparks[4].flag = spark_alive;
	state.sparks[4].render_as = SparkRenderAs::dot;
	state.sparks[4].screen_topleft[0] = {to_subpixels(13), to_subpixels(23)};
	state.sparks[4].screen_topleft[1] = {-1, -1};
	const auto before = state;
	RecordingInvalidation callbacks{state};
	if(!sparks_invalidate(state, 1U, 0U, callbacks) || callbacks.count != 4U ||
		!callbacks.callback_precedes_state_change ||
		callbacks.rectangles[0] != SparkInvalidation{{50, 60}, 1, 1} ||
		callbacks.rectangles[1] != SparkInvalidation{{100, 110}, 8, 8} ||
		callbacks.rectangles[2] != SparkInvalidation{{150, 160}, 8, 8} ||
		callbacks.rectangles[3] != SparkInvalidation{{-1, -1}, 1, 1}) {
		std::fputs("FAIL invalidation callback\n", stderr);
		return false;
	}
	if(state.sparks[0].flag != spark_alive ||
		state.sparks[0].screen_topleft[1] != before.sparks[0].screen_topleft[0] ||
		state.sparks[1].flag != spark_free ||
		state.sparks[1].screen_topleft != before.sparks[1].screen_topleft ||
		state.sparks[2] != before.sparks[2] || state.sparks[3].flag != 0x7FU ||
		state.sparks[3].screen_topleft[1] != before.sparks[3].screen_topleft[0] ||
		state.sparks[4].screen_topleft[1] != before.sparks[4].screen_topleft[0]) {
		std::fputs("FAIL invalidation state\n", stderr);
		return false;
	}

	const auto after = state;
	RecordingInvalidation rejected{state};
	if(!(!sparks_invalidate(state, 2U, 0U, rejected) && rejected.count == 0U &&
		state == after && !sparks_invalidate(state, 0U, 2U, rejected) &&
		rejected.count == 0U && state == after)) {
		std::fputs("FAIL invalidation refusal\n", stderr);
		return false;
	}

	// The assembler copies [front] to its literal opposite page. It does not
	// derive that destination from back_page, so representable equal pages keep
	// this source expression observable without a page-driver assumption.
	state = patterned_state();
	for(auto &spark : state.sparks) spark.flag = spark_free;
	state.sparks[0].flag = spark_alive;
	state.sparks[0].render_as = SparkRenderAs::dot;
	state.sparks[0].screen_topleft[0] = {to_subpixels(7), to_subpixels(8)};
	state.sparks[0].screen_topleft[1] = {to_subpixels(70), to_subpixels(80)};
	RecordingInvalidation same_page{state};
	return sparks_invalidate(state, 1U, 1U, same_page) && same_page.count == 1U &&
		same_page.rectangles[0] == SparkInvalidation{{70, 80}, 1, 1} &&
		state.sparks[0].screen_topleft[0] == SparkSubpixelPoint{
			to_subpixels(70), to_subpixels(80),
		};
}

} // namespace

int main()
{
	if(!test_update_state_prefix_and_bounds()) {
		std::fputs("FAIL update\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_invalidation_callback_remove_and_page_copy()) {
		std::fputs("FAIL invalidate\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
