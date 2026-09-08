#include "th02/core/spark.hpp"

#include "th02/core/random.hpp"

#include "pc98/random.hpp"
#include "pc98/masterlib_vector.hpp"

#include <bit>

namespace th02::core {
namespace {

constexpr std::uint16_t playfield_height = 400U;
constexpr unsigned int subpixel_bits = 4U;
constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_top = 16;
constexpr std::int16_t playfield_right = 416;
constexpr std::int16_t playfield_bottom = 384;
constexpr std::int16_t spark_width = 8;
constexpr std::int16_t spark_height = 8;
constexpr std::uint8_t spark_speed_base_mask = 0x3FU;

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) <<
		subpixel_bits
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t word_shift_left(std::int16_t value,
	unsigned int count) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(value)) << count
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t word_add(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t signed_word_divide(std::int16_t numerator,
	std::int16_t denominator) noexcept
{
	return static_cast<std::int16_t>(
		static_cast<std::int32_t>(numerator) /
		static_cast<std::int32_t>(denominator)
	);
}

[[nodiscard]] std::int16_t arithmetic_shift_right(std::int16_t value,
	unsigned int count) noexcept
{
	const auto divisor = static_cast<std::int32_t>(1) << count;
	if(value >= 0) return static_cast<std::int16_t>(value / divisor);
	return static_cast<std::int16_t>(
		-(((-static_cast<std::int32_t>(value)) + divisor - 1) / divisor)
	);
}

[[nodiscard]] bool page_is_valid(std::size_t page) noexcept
{
	return page < Spark{}.screen_topleft.size();
}

void advance_ring_low_byte(SparkPoolState &state, std::int16_t count) noexcept
{
	const auto low = static_cast<std::uint8_t>(
		static_cast<std::uint8_t>(state.ring_index) +
		static_cast<std::uint8_t>(count)
	);
	state.ring_index = static_cast<std::uint16_t>(
		(state.ring_index & 0xFF00U) |
		static_cast<std::uint16_t>(low & (spark_count - 1U))
	);
}

} // namespace

bool sparks_add(SparkPoolState &state, std::size_t back_page,
	std::int16_t left, std::uint16_t top, std::int16_t speed_base,
	std::int16_t count, bool as_sprite) noexcept
{
	if(count > 0 && (back_page >= state.sparks.front().screen_topleft.size() ||
		state.ring_index >= spark_count)) {
		return false;
	}

	if(count > 0) {
		if(top >= playfield_height) {
			top = static_cast<std::uint16_t>(top - playfield_height);
		}
		const auto spawn = SparkSubpixelPoint{
			to_subpixels(left),
			to_subpixels(std::bit_cast<std::int16_t>(top)),
		};
		auto slot_i = static_cast<std::size_t>(state.ring_index);
		for(std::int16_t group_i = 0; group_i < count; ++group_i) {
			auto &spark = state.sparks[slot_i];
			spark.flag = spark_alive;
			spark.age = 0U;
			spark.screen_topleft[back_page] = spawn;

			std::int16_t speed{};
			std::uint8_t angle{};
			if(!as_sprite) {
				spark.render_as = spark.default_render_as;
				speed = word_add(static_cast<std::int16_t>(spark.speed_base),
					speed_base);
				angle = spark.angle;
			} else {
				spark.render_as = SparkRenderAs::sprite;
				speed = speed_base;
				const auto numerator = word_shift_left(group_i, 8U);
				angle = static_cast<std::uint8_t>(
					signed_word_divide(numerator, count)
				);
			}
			const auto velocity = pc98::masterlib::vector2(angle, speed);
			spark.velocity = {velocity.x, velocity.y};

			++slot_i;
			if(slot_i >= spark_count) {
				slot_i = 0U;
			}
		}
	}

	advance_ring_low_byte(state, count);
	return true;
}

SparkStageSeedDefaults sparks_stage_seed(SparkPoolState &state,
	RandomState &random, bool reduce_effects) noexcept
{
	const auto defaults = reduce_effects ? SparkStageSeedDefaults{7U, 24U} :
		SparkStageSeedDefaults{1U, 48U};
	for(std::size_t i = 0U; i < state.sparks.size(); ++i) {
		auto &spark = state.sparks[i];
		spark.flag = spark_free;
		spark.angle = static_cast<std::uint8_t>(pc98::irand(random.seed));
		spark.speed_base = static_cast<std::uint8_t>(
			pc98::irand(random.seed) & spark_speed_base_mask
		);
		spark.default_render_as =
			((static_cast<std::size_t>(defaults.sprite_interval) & i) == 0U) ?
				SparkRenderAs::sprite : SparkRenderAs::dot;
	}
	return defaults;
}

bool sparks_update(SparkPoolState &state, std::size_t back_page,
	SparkUpdateParameters parameters) noexcept
{
	if(!page_is_valid(back_page)) return false;

	for(auto &spark : state.sparks) {
		if(spark.flag != spark_alive) continue;

		auto &position = spark.screen_topleft[back_page];
		position.x = word_add(position.x, spark.velocity.x);
		spark.velocity.x = word_add(spark.velocity.x, parameters.acceleration_x);
		position.y = word_add(position.y, spark.velocity.y);
		const auto screen_left = arithmetic_shift_right(position.x, subpixel_bits);
		const auto screen_top = arithmetic_shift_right(position.y, subpixel_bits);
		spark.velocity.y = word_add(spark.velocity.y, 1);
		spark.age = static_cast<std::uint8_t>(spark.age + 1U);

		if(screen_left < playfield_left ||
			screen_left >= static_cast<std::int16_t>(playfield_right - spark_width) ||
			spark.age > parameters.age_max || screen_top >= playfield_bottom ||
			screen_top <= static_cast<std::int16_t>(playfield_top - spark_height)) {
			spark.flag = spark_remove;
		}
	}
	return true;
}

bool sparks_invalidate(SparkPoolState &state, std::size_t back_page,
	std::size_t front_page, SparkInvalidationCallbacks &callbacks) noexcept
{
	if(!page_is_valid(back_page) || !page_is_valid(front_page)) return false;

	for(auto &spark : state.sparks) {
		if(spark.flag == spark_free) continue;

		const auto &back_position = spark.screen_topleft[back_page];
		const auto sprite = spark.render_as != SparkRenderAs::dot;
		callbacks.invalidate_spark({
			{arithmetic_shift_right(back_position.x, subpixel_bits),
				arithmetic_shift_right(back_position.y, subpixel_bits)},
			sprite ? spark_width : static_cast<std::int16_t>(1),
			sprite ? spark_height : static_cast<std::int16_t>(1),
		});
		if(spark.flag == spark_remove) {
			spark.flag = spark_free;
		} else {
			spark.screen_topleft[front_page ^ 1U] =
				spark.screen_topleft[front_page];
		}
	}
	return true;
}

PlayerShotSparkAllocatorCallbacks::PlayerShotSparkAllocatorCallbacks(
	SparkPoolState &state, std::size_t back_page
) noexcept : state_(state), back_page_(back_page)
{
}

void PlayerShotSparkAllocatorCallbacks::shot_hit_spark(
	PlayerShotHitSpark spark
) noexcept
{
	if(!sparks_add(state_, back_page_, spark.topleft.x,
		std::bit_cast<std::uint16_t>(spark.topleft.y), spark.speed_base,
		spark.count, spark.as_sprite)) {
		all_allocations_succeeded_ = false;
	}
}

bool PlayerShotSparkAllocatorCallbacks::all_allocations_succeeded() const noexcept
{
	return all_allocations_succeeded_;
}

} // namespace th02::core
