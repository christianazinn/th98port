#include "th02/core/score.hpp"
#include "th02/core/spark.hpp"

#include "pc98/masterlib_vector.hpp"

#include <bit>
#include <cstdlib>

namespace {
using namespace th02::core;

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels)
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] PlayerShot hit_shot(bool from_option, std::uint8_t decay_cel,
	std::uint8_t patnum, ShotPixelPoint position)
{
	const auto subpixels = ShotSubpixelPoint{
		to_subpixels(position.x), to_subpixels(position.y),
	};
	return PlayerShot{
		player_shot_alive, decay_cel, {{subpixels, subpixels}}, {}, patnum,
		from_option,
	};
}

[[nodiscard]] SparkPoolState seeded_sparks()
{
	SparkPoolState state{};
	for(std::size_t i = 0U; i < state.sparks.size(); ++i) {
		state.sparks[i] = {
			spark_free, static_cast<std::uint8_t>(40U + i),
			{{
				{static_cast<std::int16_t>(100 + static_cast<int>(i)),
					static_cast<std::int16_t>(200 + static_cast<int>(i))},
				{static_cast<std::int16_t>(300 + static_cast<int>(i)),
					static_cast<std::int16_t>(400 + static_cast<int>(i))},
			}},
			{static_cast<std::int16_t>(500 + static_cast<int>(i)),
				static_cast<std::int16_t>(600 + static_cast<int>(i))},
			SparkRenderAs::dot, static_cast<std::int8_t>(i),
			static_cast<std::uint8_t>(i * 32U), static_cast<std::uint8_t>(i + 1U),
			static_cast<std::int8_t>(i + 9U),
			(i & 1U) == 0U ? SparkRenderAs::dot : SparkRenderAs::sprite,
		};
	}
	return state;
}

void expect_normal_spark(Spark &spark, SparkSubpixelPoint position,
	std::int16_t requested_speed)
{
	spark.flag = spark_alive;
	spark.age = 0U;
	spark.screen_topleft[1] = position;
	spark.render_as = spark.default_render_as;
	const auto speed = static_cast<std::int16_t>(
		static_cast<std::int16_t>(spark.speed_base) + requested_speed
	);
	const auto velocity = pc98::masterlib::vector2(spark.angle, speed);
	spark.velocity = {velocity.x, velocity.y};
}

[[nodiscard]] bool test_real_collision_callbacks_allocate_sparks()
{
	PlayerShotPoolState shots{};
	shots.shots[0] = hit_shot(false, 0U, 0x31U, {100, 100});
	shots.shots[1] = hit_shot(false, 0U, 0x78U, {100, 100});
	shots.shots[2] = hit_shot(true, 1U, 0x42U, {100, 100});
	ScoreState score{};
	auto sparks = seeded_sparks();
	const auto before = sparks;
	PlayerShotSparkAllocatorCallbacks callbacks{sparks, 1U};
	const auto damage = shots_hittest(shots, score, 1U, 2U, 0U, false, 1U,
		{{100, 100}, 16, 16}, callbacks);
	if(!damage || *damage != 12 || score.score_delta != 12 ||
		!callbacks.all_allocations_succeeded() || sparks.ring_index != 3U ||
		shots.shots[1].pos_on_page[1].y != to_subpixels(92) ||
		shots.shots[2].decay_cel != 2U) {
		return false;
	}

	auto expected = before;
	expect_normal_spark(expected.sparks[0], {to_subpixels(100), to_subpixels(100)},
		30);
	// The collision's large branch changes its selected-page Y before invoking
	// the source spark call while retaining the pre-offset X coordinate.
	expect_normal_spark(expected.sparks[1], {to_subpixels(100), to_subpixels(92)},
		30);
	expect_normal_spark(expected.sparks[2], {to_subpixels(100), to_subpixels(100)},
		40);
	expected.ring_index = 3U;
	return sparks == expected;
}

[[nodiscard]] bool test_allocator_refusal_is_sticky_and_visible()
{
	PlayerShotPoolState shots{};
	shots.shots[0] = hit_shot(false, 0U, 0x31U, {100, 100});
	ScoreState score{};
	auto sparks = seeded_sparks();
	sparks.ring_index = 64U;
	const auto before = sparks;
	PlayerShotSparkAllocatorCallbacks callbacks{sparks, 0U};
	const auto damage = shots_hittest(shots, score, 0U, 0U, 0U, false, 1U,
		{{100, 100}, 16, 16}, callbacks);
	// Host-invalid spark state cannot erase the collision prefix or reclassify
	// this as complete source simulation; callers inspect the sticky failure.
	if(!damage || *damage != 1 || score.score_delta != 1 ||
		callbacks.all_allocations_succeeded() || sparks != before) {
		return false;
	}

	// A later valid source-domain allocation succeeds, but cannot erase the
	// earlier caller-visible failure recorded by this same bridge instance.
	sparks.ring_index = 0U;
	const auto repaired_damage = shots_hittest(shots, score, 0U, 0U, 0U, false,
		1U, {{100, 100}, 16, 16}, callbacks);
	return repaired_damage && *repaired_damage == 1 && score.score_delta == 2 &&
		!callbacks.all_allocations_succeeded() && sparks.ring_index == 1U &&
		sparks.sparks[0].flag == spark_alive;
}

} // namespace

int main()
{
	return (test_real_collision_callbacks_allocate_sparks() &&
		test_allocator_refusal_is_sticky_and_visible()) ? EXIT_SUCCESS : EXIT_FAILURE;
}
