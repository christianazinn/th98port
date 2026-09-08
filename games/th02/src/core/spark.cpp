#include "th02/core/spark.hpp"

#include "pc98/masterlib_vector.hpp"

#include <bit>

namespace th02::core {
namespace {

constexpr std::uint16_t playfield_height = 400U;
constexpr unsigned int subpixel_bits = 4U;

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
