#include "th02/core/score.hpp"
#include "th02/core/shot.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
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

[[nodiscard]] constexpr std::optional<std::int16_t> expected_damage(
	int value
) noexcept
{
	return static_cast<std::int16_t>(value);
}

[[nodiscard]] PlayerShot make_shot(bool from_option, std::uint8_t decay_cel,
	std::uint8_t patnum, ShotSubpixelPoint velocity,
	ShotPixelPoint page_zero, ShotPixelPoint page_one)
{
	return {
		player_shot_alive, decay_cel,
		{{
			{to_subpixels(page_zero.x), to_subpixels(page_zero.y)},
			{to_subpixels(page_one.x), to_subpixels(page_one.y)},
		}},
		velocity, patnum, from_option,
	};
}

struct RecordingCallbacks final : PlayerShotHitCallbacks {
	PlayerShotPoolState *pool{};
	ScoreState *score{};
	std::int32_t expected_score_delta{};
	std::array<PlayerShotHitSpark, player_shot_count> sparks{};
	std::size_t count{};
	bool score_was_unchanged{true};
	bool regular_mutation_precedes_spark{true};
	bool option_spark_precedes_decay{true};
	bool inspect_regular{};
	bool inspect_option{};

	RecordingCallbacks(PlayerShotPoolState &pool_arg, ScoreState &score_arg) noexcept
	{
		reset(pool_arg, score_arg);
	}

	void reset(PlayerShotPoolState &pool_arg, ScoreState &score_arg) noexcept
	{
		pool = &pool_arg;
		score = &score_arg;
		expected_score_delta = score_arg.score_delta;
		sparks = {};
		count = 0U;
		score_was_unchanged = true;
		regular_mutation_precedes_spark = true;
		option_spark_precedes_decay = true;
		inspect_regular = false;
		inspect_option = false;
	}

	void shot_hit_spark(PlayerShotHitSpark spark) noexcept override
	{
		if(score != nullptr && score->score_delta != expected_score_delta) {
			score_was_unchanged = false;
		}
		if(count < sparks.size()) sparks[count] = spark;
		if(inspect_regular && pool != nullptr) {
			if(count == 0U) {
				const auto &plain = pool->shots[0];
				regular_mutation_precedes_spark &= plain.decay_cel == 0U &&
					plain.velocity == ShotSubpixelPoint{81, -81};
			} else if(count == 1U) {
				const auto &animated = pool->shots[1];
				regular_mutation_precedes_spark &= animated.decay_cel == 2U &&
					animated.velocity == ShotSubpixelPoint{3, -4};
			} else if(count == 2U) {
				const auto &large = pool->shots[2];
				regular_mutation_precedes_spark &= large.decay_cel == 1U &&
					large.pos_on_page[1] == ShotSubpixelPoint{
						to_subpixels(92), to_subpixels(92),
					};
			}
		}
		if(inspect_option && pool != nullptr && count == 0U) {
			option_spark_precedes_decay &= pool->shots[1].decay_cel == 1U;
		}
		++count;
	}
};

[[nodiscard]] bool test_regular_hits_and_low_word_score()
{
	PlayerShotPoolState pool{};
	pool.shots[0] = make_shot(false, 0U, 0x31U, {81, -81}, {7, 7}, {100, 100});
	pool.shots[1] = make_shot(false, 0U, 0x32U, {25, -25}, {7, 7}, {100, 100});
	pool.shots[2] = make_shot(false, 0U, 0x78U, {25, -25}, {7, 7}, {100, 100});
	const auto page_zero_before = pool.shots[2].pos_on_page[0];
	ScoreState score{};
	score.score_delta = std::bit_cast<std::int32_t>(0x1234FFF8U);
	RecordingCallbacks callbacks{pool, score};
	callbacks.inspect_regular = true;
	const PlayerShotHitbox hitbox{{100, 100}, 16, 16};
	const auto damage = shots_hittest(pool, score, 1U, 2U, 0U, false, 1U,
		hitbox, callbacks);
	if(!damage || *damage != 15 ||
		score.score_delta != std::bit_cast<std::int32_t>(0x12340007U) ||
		callbacks.count != 3U || !callbacks.score_was_unchanged ||
		!callbacks.regular_mutation_precedes_spark ||
		callbacks.sparks[0] != PlayerShotHitSpark{{100, 100}, 30, 1, false} ||
		callbacks.sparks[1] != PlayerShotHitSpark{{100, 100}, 30, 1, false} ||
		callbacks.sparks[2] != PlayerShotHitSpark{{100, 92}, 30, 1, false} ||
		pool.shots[2].pos_on_page[0] != page_zero_before) {
		return false;
	}

	// A patnum below 0x32 still adds the flat point and sparks, but it is not
	// consumed. The same regular shot therefore damages again next call.
	callbacks.expected_score_delta = score.score_delta;
	callbacks.count = 0U;
	callbacks.inspect_regular = false;
	if(shots_hittest(pool, score, 1U, 2U, 0U, false, 1U, hitbox, callbacks) !=
		expected_damage(1) || score.score_delta !=
		std::bit_cast<std::int32_t>(0x12340008U) || callbacks.count != 1U ||
		pool.shots[0].decay_cel != 0U || pool.shots[0].flag != player_shot_alive) {
		return false;
	}
	return callbacks.sparks[0] == PlayerShotHitSpark{{100, 100}, 30, 1, false};
}

[[nodiscard]] bool test_all_damage_table_levels()
{
	constexpr std::array<std::int16_t, 10> regular_damage{
		5, 5, 4, 4, 3, 3, 3, 2, 2, 1,
	};
	constexpr std::array<std::int16_t, 10> option_damage{
		2, 2, 2, 2, 2, 1, 1, 1, 1, 1,
	};
	constexpr std::array<std::int16_t, 10> large_damage{
		0, 0, 8, 8, 7, 7, 6, 5, 4, 3,
	};
	const PlayerShotHitbox hitbox{{100, 100}, 16, 16};
	for(std::size_t level = 0U; level < regular_damage.size(); ++level) {
		const auto source_level = static_cast<std::uint8_t>(level);
		PlayerShotPoolState plain_pool{};
		plain_pool.shots[0] = make_shot(false, 0U, 0x31U, {0, 0},
			{100, 100}, {7, 7});
		ScoreState plain_score{};
		RecordingCallbacks plain_callbacks{plain_pool, plain_score};
		if(shots_hittest(plain_pool, plain_score, 0U, source_level, 0U, false,
			1U, hitbox, plain_callbacks) != expected_damage(1) ||
			plain_score.score_delta != 1 || plain_callbacks.count != 1U) {
			return false;
		}

		PlayerShotPoolState animated_pool{};
		animated_pool.shots[0] = make_shot(false, 0U, 0x32U, {0, 0},
			{100, 100}, {7, 7});
		ScoreState animated_score{};
		RecordingCallbacks animated_callbacks{animated_pool, animated_score};
		const auto animated_expected = static_cast<std::int16_t>(
			1 + regular_damage[level]
		);
		if(shots_hittest(animated_pool, animated_score, 0U, source_level, 0U,
			false, 1U, hitbox, animated_callbacks) != expected_damage(animated_expected) ||
			animated_score.score_delta != animated_expected ||
			animated_callbacks.count != 1U) {
			return false;
		}

		PlayerShotPoolState large_pool{};
		large_pool.shots[0] = make_shot(false, 0U, 0x78U, {0, 0},
			{100, 100}, {7, 7});
		ScoreState large_score{};
		RecordingCallbacks large_callbacks{large_pool, large_score};
		const auto large_expected = static_cast<std::int16_t>(1 + large_damage[level]);
		if(shots_hittest(large_pool, large_score, 0U, source_level, 0U, false,
			1U, hitbox, large_callbacks) != expected_damage(large_expected) ||
			large_score.score_delta != large_expected || large_callbacks.count != 1U) {
			return false;
		}

		// Decay two deliberately suppresses the option spark but not its damage.
		PlayerShotPoolState option_pool{};
		option_pool.shots[0] = make_shot(true, 2U, 0x42U, {0, 0},
			{100, 100}, {7, 7});
		ScoreState option_score{};
		RecordingCallbacks option_callbacks{option_pool, option_score};
		if(shots_hittest(option_pool, option_score, 0U, source_level, 0U, false,
			1U, hitbox, option_callbacks) != expected_damage(option_damage[level]) ||
			option_score.score_delta != option_damage[level] ||
			option_callbacks.count != 0U) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool test_option_window_and_callback_order()
{
	PlayerShotPoolState pool{};
	// The option X window ends at left - 8 + width (108), unlike a regular
	// shot's unshifted right edge (116). This option at 110 must not hit.
	pool.shots[0] = make_shot(true, 1U, 0x42U, {0, 0}, {110, 100}, {110, 100});
	pool.shots[1] = make_shot(true, 1U, 0x42U, {0, 0}, {100, 100}, {100, 100});
	ScoreState score{};
	score.score_delta = 500;
	RecordingCallbacks callbacks{pool, score};
	callbacks.inspect_option = true;
	const PlayerShotHitbox hitbox{{100, 100}, 16, 16};
	if(shots_hittest(pool, score, 0U, 0U, 0U, false, 1U, hitbox, callbacks) !=
		expected_damage(2) || score.score_delta != 502 ||
		callbacks.count != 1U || !callbacks.score_was_unchanged ||
		!callbacks.option_spark_precedes_decay || pool.shots[0].decay_cel != 1U ||
		pool.shots[1].decay_cel != 2U ||
		callbacks.sparks[0] != PlayerShotHitSpark{{100, 100}, 40, 1, false}) {
		return false;
	}

	// Later option overlaps retain their damage but do not re-spark once the
	// source's spawn value of one has been raised to two.
	callbacks.expected_score_delta = score.score_delta;
	callbacks.count = 0U;
	callbacks.inspect_option = false;
	return shots_hittest(pool, score, 0U, 0U, 0U, false, 1U, hitbox, callbacks) ==
		expected_damage(2) && score.score_delta == 504 &&
		callbacks.count == 0U;
}

[[nodiscard]] bool test_boundaries_bomb_and_refusal()
{
	PlayerShotPoolState pool{};
	ScoreState score{};
	RecordingCallbacks callbacks{pool, score};
	const PlayerShotHitbox hitbox{{100, 100}, 16, 16};
	// All four source hitbox edges are strict. Slot 34 is the last accepted slot
	// in the anomalous 35-slot scan.
	pool.shots[0] = make_shot(false, 0U, 0x31U, {0, 0}, {92, 100}, {7, 7});
	pool.shots[1] = make_shot(false, 0U, 0x31U, {0, 0}, {116, 100}, {7, 7});
	pool.shots[2] = make_shot(false, 0U, 0x31U, {0, 0}, {100, 92}, {7, 7});
	pool.shots[3] = make_shot(false, 0U, 0x31U, {0, 0}, {100, 108}, {7, 7});
	pool.shots[34] = make_shot(false, 0U, 0x31U, {0, 0}, {100, 100}, {7, 7});
	if(shots_hittest(pool, score, 0U, 0U, 0U, false, 1U, hitbox, callbacks) !=
		expected_damage(1) || score.score_delta != 1 ||
		callbacks.count != 1U) {
		return false;
	}

	pool = {};
	score = {};
	callbacks.reset(pool, score);
	// The final three slots are alive and overlapping but are outside the
	// source's anomalous collision loop.
	for(std::size_t i = 35U; i < pool.shots.size(); ++i) {
		pool.shots[i] = make_shot(false, 0U, 0x32U, {16, -16},
			{100, 100}, {100, 100});
	}
	const auto pool_before = pool;
	const auto score_before = score;
	if(shots_hittest(pool, score, 0U, 0U, 0U, false, 1U, hitbox, callbacks) !=
		expected_damage(0) || pool != pool_before || score != score_before ||
		callbacks.count != 0U) {
		return false;
	}
	if(shots_hittest(pool, score, 2U, 0U, 0U, false, 1U, hitbox, callbacks) ||
		shots_hittest(pool, score, 0U, 10U, 0U, false, 1U, hitbox, callbacks) ||
		pool != pool_before || score != score_before || callbacks.count != 0U) {
		return false;
	}

	// This is an actual source SAR4 boundary: -1 subpixel floors to -1 pixel.
	// At hitbox left -1 it is excluded; moving that edge to -2 accepts it and
	// forwards the negative pixel coordinate through the synchronous callback.
	pool = {};
	score = {};
	callbacks.reset(pool, score);
	pool.shots[0] = make_shot(false, 0U, 0x31U, {0, 0}, {0, 1}, {7, 7});
	pool.shots[0].pos_on_page[0].x = -1;
	pool.shots[0].pos_on_page[0].y = 16;
	if(shots_hittest(pool, score, 0U, 0U, 0U, false, 1U,
		{{7, 8}, 16, 16}, callbacks) != expected_damage(0) ||
		score.score_delta != 0 || callbacks.count != 0U) {
		return false;
	}
	if(shots_hittest(pool, score, 0U, 0U, 0U, false, 1U,
		{{6, 8}, 16, 16}, callbacks) != expected_damage(1) ||
		score.score_delta != 1 || callbacks.count != 1U ||
		callbacks.sparks[0] != PlayerShotHitSpark{{-1, 1}, 30, 1, false}) {
		return false;
	}

	// The bomb gate tests stage_frame's low byte only. Empty scans still add a
	// gated point to the low score word, while a nonzero masked remainder does not.
	pool = {};
	score = {};
	callbacks.reset(pool, score);
	if(shots_hittest(pool, score, 0U, 0U, 0x7F000004U, true, 3U, hitbox, callbacks) !=
		expected_damage(1) || score.score_delta != 1) {
		return false;
	}
	callbacks.expected_score_delta = score.score_delta;
	return shots_hittest(pool, score, 0U, 0U, 0x00000105U, true, 3U, hitbox,
		callbacks) == expected_damage(0) && score.score_delta == 1 &&
		callbacks.count == 0U;
}

} // namespace

int main()
{
	if(!test_regular_hits_and_low_word_score()) {
		std::fputs("FAIL regular hit/score\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_all_damage_table_levels()) {
		std::fputs("FAIL damage tables\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_option_window_and_callback_order()) {
		std::fputs("FAIL option hit/order\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_boundaries_bomb_and_refusal()) {
		std::fputs("FAIL boundaries/bomb/refusal\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
