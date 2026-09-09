#include "th02/core/enemy.hpp"
#include "th02/core/spark.hpp"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
using namespace th02::core;

[[nodiscard]] constexpr std::optional<std::int16_t> expected_damage(
	int value
) noexcept
{
	return static_cast<std::int16_t>(value);
}

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels)
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] PlayerShot make_shot(ShotPixelPoint page_position)
{
	return {
		player_shot_alive, 0U,
		{{
			{to_subpixels(7), to_subpixels(7)},
			{to_subpixels(page_position.x), to_subpixels(page_position.y)},
		}},
		{0, 0}, 0x31U, false,
	};
}

struct OrderedSparkCallbacks final : PlayerShotHitCallbacks {
	Enemy &enemy;
	PlayerHitState &hit;
	PlayerShotSparkAllocatorCallbacks allocator;
	bool writes_precede_spark{};

	OrderedSparkCallbacks(Enemy &enemy_arg, PlayerHitState &hit_arg,
		SparkPoolState &sparks, std::size_t back_page) noexcept
		: enemy(enemy_arg), hit(hit_arg), allocator(sparks, back_page)
	{
	}

	void shot_hit_spark(PlayerShotHitSpark spark) noexcept override
	{
		writes_precede_spark = enemy.age == 0 && enemy.in_kill_anim != 0U &&
			hit.raw_hit == 1U;
		allocator.shot_hit_spark(spark);
	}
};

[[nodiscard]] bool test_composed_hit_order_and_effects()
{
	Enemy enemy{};
	enemy.pos_on_page[1] = {100, 100};
	enemy.flag = 0U;
	enemy.not_shootable = 1U;
	enemy.age = 23;
	EnemyTemplate enemy_template{};
	PlayerFrameState frame{};
	frame.page_front = 0U;
	frame.page_back = 1U;
	frame.player.on_page[0] = {100, 7};
	frame.player.current = {222, 100};
	PlayerHitState hit{0xFFU, 5U, true, true};
	PlayerShotPoolState shots{};
	shots.shots[0] = make_shot({100, 100});
	ScoreState score{};
	SparkPoolState sparks{};
	OrderedSparkCallbacks callbacks{enemy, hit, sparks, 1U};

	const auto damage = enemy_hittest(enemy, enemy_template, frame, hit, shots,
		score, 0U, 0U, false, 1U, callbacks);
	return damage == expected_damage(1) &&
		enemy.age == 0 && enemy.in_kill_anim != 0U && hit.raw_hit == 1U &&
		hit.invincibility_time == 5U && hit.invincible_via_bomb &&
		hit.miss_active &&
		score.score_delta == 1 && callbacks.writes_precede_spark &&
		callbacks.allocator.all_allocations_succeeded() &&
		sparks.sparks[0].flag == spark_alive;
}

[[nodiscard]] bool test_disabled_collision_still_scans()
{
	for(const bool template_disabled : {false, true}) {
		Enemy enemy{};
		enemy.pos_on_page[1] = {100, 100};
		enemy.age = 7;
		enemy.no_player_collision = static_cast<std::uint8_t>(
			template_disabled ? 0U : 1U);
		EnemyTemplate enemy_template{};
		enemy_template.no_player_collision = static_cast<std::uint8_t>(
			template_disabled ? 1U : 0U);
		PlayerFrameState frame{};
		frame.page_front = 0U;
		frame.page_back = 1U;
		frame.player.on_page[0] = {100, 7};
		frame.player.current = {100, 100};
		PlayerHitState hit{};
		PlayerShotPoolState shots{};
		shots.shots[0] = make_shot({100, 100});
		ScoreState score{};
		SparkPoolState sparks{};
		PlayerShotSparkAllocatorCallbacks callbacks{sparks, 1U};

		const auto damage = enemy_hittest(enemy, enemy_template, frame, hit,
			shots, score, 0U, 0U, false, 1U, callbacks);
		if(damage != expected_damage(1) || enemy.age != 7 ||
			enemy.in_kill_anim != 0U || hit.raw_hit != 0U ||
			score.score_delta != 1) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool test_strict_edges_and_page_x_asymmetry()
{
	struct Case {
		std::int16_t x;
		std::int16_t y;
		bool hit;
	};
	constexpr Case cases[]{
		{84, 100, false}, {85, 100, true}, {115, 100, true},
		{116, 100, false}, {100, 84, false}, {100, 85, true},
		{100, 115, true}, {100, 116, false},
	};
	for(const auto test_case : cases) {
		Enemy enemy{};
		enemy.pos_on_page[1] = {100, 100};
		enemy.flag = 0U;
		enemy.not_shootable = 1U;
		enemy.age = 9;
		EnemyTemplate enemy_template{};
		PlayerFrameState frame{};
		frame.page_front = 0U;
		frame.page_back = 1U;
		frame.player.on_page[0] = {test_case.x, 999};
		frame.player.current = {999, test_case.y};
		PlayerHitState hit{};
		PlayerShotPoolState shots{};
		ScoreState score{};
		SparkPoolState sparks{};
		PlayerShotSparkAllocatorCallbacks callbacks{sparks, 1U};

		const auto damage = enemy_hittest(enemy, enemy_template, frame, hit,
			shots, score, 0U, 0U, false, 1U, callbacks);
		if(damage != expected_damage(0) ||
			(test_case.hit && (enemy.age != 0 || enemy.in_kill_anim == 0U ||
				hit.raw_hit != 1U)) ||
			(!test_case.hit && (enemy.age != 9 || enemy.in_kill_anim != 0U ||
				hit.raw_hit != 0U))) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool test_bombing_damage_without_shots_or_player_collision()
{
	Enemy enemy{};
	enemy.pos_on_page[1] = {100, 100};
	EnemyTemplate enemy_template{};
	enemy_template.no_player_collision = 1U;
	const PlayerFrameState frame{};
	PlayerHitState hit{};
	PlayerShotPoolState shots{};
	ScoreState score{};
	SparkPoolState sparks{};
	PlayerShotSparkAllocatorCallbacks callbacks{sparks, 1U};

	// No shot and no player overlap are present. The source shot hittest still
	// contributes the bomb-gated point after the enemy collision prefix.
	return enemy_hittest(enemy, enemy_template, frame, hit, shots, score, 0U,
		4U, true, 3U, callbacks) == expected_damage(1) &&
		score.score_delta == 1 && hit.raw_hit == 0U && enemy.age == 0 &&
		enemy.in_kill_anim == 0U && sparks.sparks[0].flag == spark_free;
}

[[nodiscard]] bool test_refusal_is_atomic(
	std::size_t page_front, std::size_t page_back, std::uint8_t shot_level
)
{
	Enemy enemy{};
	enemy.pos_on_page[1] = {100, 100};
	enemy.age = 17;
	EnemyTemplate enemy_template{};
	PlayerFrameState frame{};
	frame.page_front = 0U;
	frame.page_back = 1U;
	frame.player.on_page[0] = {100, 7};
	frame.player.current = {100, 100};
	PlayerHitState hit{2U, 5U, true, false};
	PlayerShotPoolState shots{};
	shots.shots[0] = make_shot({100, 100});
	ScoreState score{};
	score.score_delta = 71;
	SparkPoolState sparks{};
	PlayerShotSparkAllocatorCallbacks callbacks{sparks, 1U};
	frame.page_front = page_front;
	frame.page_back = page_back;
	const auto enemy_before = enemy;
	const auto frame_before = frame;
	const auto hit_before = hit;
	const auto shots_before = shots;
	const auto score_before = score;
	const auto sparks_before = sparks;
	return enemy_hittest(enemy, enemy_template, frame, hit, shots, score,
		shot_level, 0U, false, 1U, callbacks) == std::nullopt &&
		enemy == enemy_before &&
		frame == frame_before && hit == hit_before && shots == shots_before &&
		score == score_before && sparks == sparks_before;
}

[[nodiscard]] bool test_invalid_context_is_atomic()
{
	return test_refusal_is_atomic(2U, 1U, 0U) &&
		test_refusal_is_atomic(0U, 2U, 0U);
}

[[nodiscard]] bool test_invalid_level_is_atomic()
{
	return test_refusal_is_atomic(0U, 1U, 10U);
}

} // namespace

int main()
{
	if(!test_composed_hit_order_and_effects()) {
		std::fputs("FAIL composed hit/order/effects\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_disabled_collision_still_scans()) {
		std::fputs("FAIL disabled collision scan\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_strict_edges_and_page_x_asymmetry()) {
		std::fputs("FAIL strict edges/asymmetry\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_bombing_damage_without_shots_or_player_collision()) {
		std::fputs("FAIL bombing-only damage\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_invalid_context_is_atomic() || !test_invalid_level_is_atomic()) {
		std::fputs("FAIL invalid atomicity\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
