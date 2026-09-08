#include "th05/core/shot_collision.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace th05::core;
using Point = pc98::th0405::SubpixelPoint;

ShotHitbox box(Point center, Point radius)
{
	return {center, radius};
}

bool test_cached_position_and_zero_extended_damage()
{
	ShotUpdateState state{};
	state.shots[0] = {
		shot_alive, 7, {{1000, 1001}, {900, 901}, {6, -6}},
		0, shot_normal, -7, 42
	};
	state.shots_alive[0] = {{100, 101}, 0};
	state.shots_alive_count = 1;
	std::uint8_t parity = 0;
	Point spark_position{};
	bool spark_position_ok = true;
	const auto result = shots_hittest_scan(
		state,
		box({100, 101}, {16, 16}),
		parity,
		[&](Point position) noexcept {
			spark_position = position;
			if(position != Point{100, 101}) spark_position_ok = false;
		}
	);
	if(!result || result->damage != 249 || result->hit_count != 1 ||
		parity != 1 || !spark_position_ok ||
		spark_position != Point{100, 101}) return false;
	if(state.shots[0].flag != shot_remove ||
		state.shots_alive[0].pos.x != homing_target_none ||
		state.shots_alive[0].pos.y != 101) return false;
	const auto &hitshot = state.hitshot_state.hitshots[0];
	return hitshot == HitShot{
		1, 32, {{1000, 1001}, {900, 901}, {1, -1}}
	};
}

bool test_inclusive_edges_and_hit_count_division()
{
	ShotUpdateState state{};
	state.shots[0] = {shot_alive, 0, {{10, 20}, {}, {}}, 0, 0, 5, 0};
	state.shots[1] = {shot_alive, 0, {{30, 40}, {}, {}}, 0, 0, 5, 0};
	state.shots[2] = {shot_alive, 0, {{50, 60}, {}, {}}, 0, 0, 0, 0};
	state.shots_alive[0] = {{116, 116}, 0}; // right/bottom inclusive edge
	state.shots_alive[1] = {{100, 100}, 1}; // second accepted hit
	state.shots_alive[2] = {{100, 117}, 2}; // one Q12.4 unit below
	state.shots_alive_count = 3;
	std::uint8_t parity = 0;
	unsigned sparks = 0;
	const auto result = shots_hittest_scan(
		state,
		box({100, 100}, {16, 16}),
		parity,
		[&](Point) noexcept { ++sparks; }
	);
	// First hit contributes 5; the second contributes max(5/2, 1)=2. The
	// edge is inclusive and the third cache point is outside the box.
	return result && result->damage == 7 && result->hit_count == 2 &&
		sparks == 1 && state.shots[0].flag == shot_remove &&
		state.shots[1].flag == shot_remove && state.shots[2].flag == shot_alive;
}

bool test_word_wrapped_hitbox_arithmetic()
{
	ShotUpdateState state{};
	state.shots[0] = {shot_alive, 0, {{-32768, 0}, {}, {}}, 0, 0, 1, 0};
	state.shots_alive[0] = {{-32768, 0}, 0};
	state.shots_alive_count = 1;
	std::uint8_t parity = 0;
	const auto result = shots_hittest_scan(
		state,
		box({-32768, 0}, {1, 1}),
		parity,
		[](Point) noexcept {}
	);
	return result && result->damage == 1 && result->hit_count == 1 &&
		state.shots[0].flag == shot_remove;
}

bool test_all_bounds_edges_and_one_unit_misses()
{
	ShotUpdateState state{};
	const std::array<Point, 8> positions{{
		{84, 84}, {100, 84}, {116, 84}, {116, 100},
		{116, 116}, {100, 116}, {84, 116}, {84, 100}
	}};
	const std::array<Point, 8> misses{{
		{83, 100}, {100, 83}, {117, 100}, {100, 117},
		{83, 84}, {84, 83}, {117, 116}, {116, 117}
	}};
	for(std::size_t i = 0; i < positions.size(); ++i) {
		state.shots[i] = {
			shot_alive, 0, {{100, 100}, {}, {}}, 0, 0, 1, 0
		};
		state.shots_alive[i] = {positions[i], static_cast<std::uint16_t>(i)};
	}
	for(std::size_t i = 0; i < misses.size(); ++i) {
		const auto index = i + positions.size();
		state.shots[index] = {
			shot_alive, 0, {{100, 100}, {}, {}}, 0, 0, 1, 0
		};
		state.shots_alive[index] = {
			misses[i], static_cast<std::uint16_t>(index)
		};
	}
	state.shots_alive_count = static_cast<std::uint16_t>(
		positions.size() + misses.size()
	);
	std::uint8_t parity = 0;
	const auto result = shots_hittest_scan(
		state, box({100, 100}, {16, 16}), parity,
		[](Point) noexcept {}
	);
	if(!result || result->damage != 8 || result->hit_count != 8) return false;
	for(std::size_t i = 0; i < positions.size(); ++i) {
		if(state.shots[i].flag != shot_remove ||
			state.shots_alive[i].pos.x != homing_target_none) return false;
	}
	for(std::size_t i = positions.size(); i < state.shots.size() &&
		i < positions.size() + misses.size(); ++i) {
		if(state.shots[i].flag != shot_alive) return false;
	}
	return true;
}

bool test_repeated_cache_index_and_nonalive_flag()
{
	ShotUpdateState repeated{};
	repeated.shots[0] = {
		shot_alive, 0, {{500, 600}, {490, 590}, {6, 12}},
		0, 0, 5, 0
	};
	repeated.shots_alive[0] = {{50, 60}, 0};
	repeated.shots_alive[1] = {{50, 60}, 0};
	repeated.shots_alive_count = 2;
	std::uint8_t parity = 0;
	const auto repeated_result = shots_hittest_scan(
		repeated, box({50, 60}, {1, 1}), parity,
		[](Point) noexcept {}
	);
	if(!repeated_result || repeated_result->damage != 7 ||
		repeated_result->hit_count != 2 || repeated.shots[0].flag != shot_remove ||
		repeated.shots_alive[0].pos.x != homing_target_none ||
		repeated.shots_alive[1].pos.x != homing_target_none ||
		repeated.hitshot_state.next_free_id != 2) return false;

	ShotUpdateState nonalive{};
	nonalive.shots[0] = {
		99, 0, {{700, 800}, {690, 790}, {0, 0}},
		0, 0, 4, 0
	};
	nonalive.shots_alive[0] = {{70, 80}, 0};
	nonalive.shots_alive_count = 1;
	parity = 0;
	const auto nonalive_result = shots_hittest_scan(
		nonalive, box({70, 80}, {1, 1}), parity,
		[](Point) noexcept {}
	);
	return nonalive_result && nonalive_result->damage == 4 &&
		nonalive_result->hit_count == 1 && nonalive.shots[0].flag == shot_remove;
}

bool test_none_valued_cache_position_is_not_filtered()
{
	ShotUpdateState state{};
	state.shots[0] = {
		shot_alive, 0, {{-15984, 0}, {}, {}}, 0, 0, 1, 0
	};
	state.shots_alive[0] = {{homing_target_none, 0}, 0};
	state.shots_alive_count = 1;
	std::uint8_t parity = 0;
	const auto result = shots_hittest_scan(
		state, box({homing_target_none, 0}, {1, 1}), parity,
		[](Point) noexcept {}
	);
	return result && result->damage == 1 && result->hit_count == 1 &&
		state.shots[0].flag == shot_remove;
}

bool test_64_zero_damage_hits_minimum_and_ring_wrap()
{
	ShotUpdateState state{};
	for(std::size_t i = 0; i < shot_count; ++i) {
		state.shots[i] = {
			shot_alive, 0, {{100, 100}, {}, {}}, 0, 0, 0, 0
		};
		state.shots_alive[i] = {
			{100, 100}, static_cast<std::uint16_t>(i)
		};
	}
	state.shots_alive_count = static_cast<std::uint16_t>(shot_count);
	std::uint8_t parity = 0;
	unsigned sparks = 0;
	const auto result = shots_hittest_scan(
		state, box({100, 100}, {1, 1}), parity,
		[&](Point) noexcept { ++sparks; }
	);
	if(!result || result->damage != shot_count ||
		result->hit_count != shot_count || parity != 64 || sparks != 32 ||
		state.hitshot_state.next_free_id != 16) return false;
	for(const auto &shot : state.shots) {
		if(shot.flag != shot_remove) return false;
	}
	for(const auto &hitshot : state.hitshot_state.hitshots) {
		if(hitshot.age != 1) return false;
	}
	return true;
}

bool test_cache_is_consumed_by_later_callers()
{
	ShotUpdateState state{};
	state.shots[0] = {shot_alive, 0, {{100, 100}, {}, {}}, 0, 0, 9, 0};
	state.shots_alive[0] = {{100, 100}, 0};
	state.shots_alive_count = 1;
	std::uint8_t parity = 0;
	unsigned sparks = 0;
	const auto first = shots_hittest_scan(
		state, box({100, 100}, {1, 1}), parity,
		[&](Point) noexcept { ++sparks; }
	);
	const auto second = shots_hittest_scan(
		state, box({100, 100}, {1, 1}), parity,
		[&](Point) noexcept { ++sparks; }
	);
	return first && first->damage == 9 && second &&
		second->damage == 0 && second->hit_count == 0 && sparks == 1 &&
		parity == 1;
}

bool test_revenge_callback_order_and_gate()
{
	ShotUpdateState state{};
	state.shots[0] = {shot_alive, 0, {{300, 400}, {299, 398}, {5, 6}},
		0, 20, 3, 8};
	state.shots_alive[0] = {{30, 40}, 0};
	state.shots_alive_count = 1;
	std::uint8_t parity = 0;
	std::vector<std::string> events;
	Point revenge_origin{};
	std::uint8_t revenge_angle = 0;
	const auto result = shots_hittest_revenge_scan(
		state,
		box({30, 40}, {2, 2}),
		parity,
		true,
		[&](Point position) noexcept {
			events.emplace_back("spark");
			if(position != Point{30, 40}) revenge_origin = {-1, -1};
		},
		[&](Point position) noexcept {
			// The source stores origin before drawing RNG1, then invokes the
			// allocator; model all three synchronous steps in that order.
			events.emplace_back("origin");
			revenge_origin = position;
			events.emplace_back("rng1");
			revenge_angle = 0xA5;
			events.emplace_back("allocator");
			// The callback runs before hitshot_from() removes the source.
			if(state.shots[0].flag != shot_alive) revenge_origin = {-1, -1};
		}
	);
	if(!result || result->damage != 3 || events !=
		std::vector<std::string>{"spark", "origin", "rng1", "allocator"} ||
		revenge_origin != Point{30, 40} || revenge_angle != 0xA5 ||
		state.shots[0].flag != shot_remove) {
		return false;
	}

	ShotUpdateState gated{};
	gated.shots[0] = state.shots[0];
	gated.shots[0].flag = shot_alive;
	gated.shots_alive[0] = {{30, 40}, 0};
	gated.shots_alive_count = 1;
	parity = 0;
	events.clear();
	const auto no_revenge = shots_hittest_revenge_scan(
		gated,
		box({30, 40}, {2, 2}),
		parity,
		false,
		[&](Point) noexcept { events.emplace_back("spark"); },
		[&](Point) noexcept { events.emplace_back("allocator"); }
	);
	return no_revenge && events == std::vector<std::string>{"spark"};
}

bool test_occupied_hitshot_still_removes_source()
{
	ShotUpdateState state{};
	state.shots[0] = {shot_alive, 0, {{100, 100}, {}, {}}, 0, 0, 1, 0};
	state.shots_alive[0] = {{100, 100}, 0};
	state.shots_alive_count = 1;
	state.hitshot_state.hitshots[0].age = 9;
	std::uint8_t parity = 0;
	const auto result = shots_hittest_scan(
		state, box({100, 100}, {1, 1}), parity, [](Point) noexcept {}
	);
	return result && state.shots[0].flag == shot_remove &&
		state.hitshot_state.next_free_id == 1 &&
		state.hitshot_state.hitshots[0].age == 9;
}

bool test_malformed_state_refuses_atomically()
{
	const auto invoke = [](ShotUpdateState &state, std::uint8_t &parity) {
		return shots_hittest_scan(
			state, box({0, 0}, {1, 1}), parity, [](Point) noexcept {}
		);
	};

	ShotUpdateState count_bad{};
	count_bad.shots_alive_count = static_cast<std::uint16_t>(shot_count + 1U);
	std::uint8_t parity = 9;
	const auto count_before = count_bad;
	if(invoke(count_bad, parity) || count_bad != count_before || parity != 9) {
		return false;
	}

	ShotUpdateState index_bad{};
	index_bad.shots_alive_count = 1;
	index_bad.shots_alive[0].shot_index = static_cast<std::uint16_t>(shot_count);
	const auto index_before = index_bad;
	parity = 11;
	if(invoke(index_bad, parity) || index_bad != index_before || parity != 11) {
		return false;
	}

	ShotUpdateState cursor_bad{};
	cursor_bad.hitshot_state.next_free_id = static_cast<std::uint16_t>(hitshot_count);
	const auto cursor_before = cursor_bad;
	parity = 13;
	if(invoke(cursor_bad, parity) || cursor_bad != cursor_before || parity != 13) {
		return false;
	}
	return true;
}

bool test_shared_spark_effect_wrapper()
{
	std::array<pc98::th0405::Spark, pc98::th0405::SPARK_COUNT_TH05> sparks{};
	std::uint16_t ring_offset = 0;
	pc98::th0405::SparkPool pool{sparks, ring_offset};
	pc98::th0405::RandomState random{};
	random.ring[0] = 0x11;
	random.ring[1] = 0x22;
	ShotUpdateState state{};
	state.shots[0] = {
		shot_alive, 0, {{100, 101}, {}, {}}, 0, shot_normal, 7, 0
	};
	state.shots_alive[0] = {{100, 101}, 0};
	state.shots_alive_count = 1;
	std::uint8_t parity = 0;
	const auto result = shots_hittest_effects_scan(
		state, box({100, 101}, {1, 1}), parity, pool, random
	);
	if(!result || result->damage != 7 || parity != 1 || random.cursor != 1 ||
		ring_offset != sizeof(pc98::th0405::Spark) ||
		sparks[0].flag != pc98::th0405::SPARK_ALIVE ||
		sparks[0].age != 0 || sparks[0].center.cur != Point{100, 101} ||
		state.shots[0].flag != shot_remove) {
		return false;
	}

	ShotUpdateState before = state;
	std::array<pc98::th0405::Spark, 1> bad_slots{};
	std::uint16_t bad_offset = 0;
	pc98::th0405::SparkPool bad_pool{bad_slots, bad_offset};
	const auto random_before = random;
	parity = 9;
	if(shots_hittest_effects_scan(
			state, box({100, 101}, {1, 1}), parity, bad_pool, random
		) || state != before || random != random_before || parity != 9) {
		return false;
	}
	std::array<pc98::th0405::Spark, pc98::th0405::SPARK_COUNT_TH04> th04_slots{};
	std::uint16_t th04_offset = 0;
	pc98::th0405::SparkPool th04_pool{th04_slots, th04_offset};
	parity = 7;
	if(shots_hittest_effects_scan(
			state, box({100, 101}, {1, 1}), parity, th04_pool, random
		) || state != before || random != random_before || parity != 7) {
		return false;
	}
	return true;
}

bool test_revenge_effect_wrapper_owns_gate_rng_and_origin()
{
	std::array<pc98::th0405::Spark, pc98::th0405::SPARK_COUNT_TH05> sparks{};
	std::uint16_t spark_offset = 0;
	pc98::th0405::SparkPool spark_pool{sparks, spark_offset};
	pc98::th0405::RandomState spark_random{};
	spark_random.ring[0] = 0x03;
	spark_random.ring[1] = 0x00;
	pc98::th0405::RandomState revenge_random{};
	revenge_random.ring[0] = 0xA5;
	revenge_random.ring[1] = 0x5A;
	ShotUpdateState state{};
	state.shots[0] = {
		shot_alive, 0, {{300, 400}, {}, {}}, 0, shot_normal, 3, 0
	};
	state.shots_alive[0] = {{30, 40}, 0};
	state.shots_alive_count = 1;
	std::uint8_t parity = 0;
	bool bullet_called = false;
	bool source_was_alive = false;
	RevengeBulletRequest request{};
	RevengeBulletTemplate bullet_template{};
	const auto result = shots_hittest_revenge_effects_scan(
		state,
		box({30, 40}, {2, 2}),
		parity,
		{ShotRank::normal, 1},
		spark_pool,
		spark_random,
		revenge_random,
		bullet_template,
		[&](RevengeBulletRequest received) noexcept {
			bullet_called = true;
			request = received;
			source_was_alive = state.shots[0].flag == shot_alive;
		}
	);
	if(!result || result->damage != 3 || !bullet_called || !source_was_alive ||
		request.origin != Point{30, 40} || request.angle != 0xA5 ||
		bullet_template.origin != Point{30, 40} || bullet_template.angle != 0xA5 ||
		spark_random.cursor != 1 || revenge_random.cursor != 1 ||
		state.shots[0].flag != shot_remove) {
		return false;
	}

	ShotUpdateState gated{};
	gated.shots[0] = state.shots[0];
	gated.shots[0].flag = shot_alive;
	gated.shots_alive[0] = {{30, 40}, 0};
	gated.shots_alive_count = 1;
	std::uint8_t gated_parity = 0;
	bool gated_bullet = false;
	RevengeBulletTemplate gated_template{};
	ScoreState gated_score{};
	const auto gated_result = shots_hittest_revenge(
		gated,
		box({30, 40}, {2, 2}),
		gated_parity,
		{ShotRank::easy, 1},
		spark_pool,
		spark_random,
		revenge_random,
		gated_template,
		gated_score,
		[&](RevengeBulletRequest) noexcept { gated_bullet = true; }
	);
	return gated_result && *gated_result == 3 && !gated_bullet &&
		gated_score.score_delta == 3 && spark_random.cursor == 2 &&
		revenge_random.cursor == 1;
}

bool test_complete_damage_and_score_wrapper()
{
	const auto report = [](bool ok, const char *name) noexcept {
		if(!ok) std::cerr << "damage detail: " << name << "\n";
		return ok;
	};
	if(!shot_bomb_damage_for_playchar(ShotPlaychar::reimu) ||
		*shot_bomb_damage_for_playchar(ShotPlaychar::reimu) != 3 ||
		*shot_bomb_damage_for_playchar(ShotPlaychar::marisa) != 4 ||
		*shot_bomb_damage_for_playchar(ShotPlaychar::mima) != 15 ||
		*shot_bomb_damage_for_playchar(ShotPlaychar::yuuka) != 3) {
		return false;
	}
	ScoreState score{};
	const auto normal = shots_hittest_apply_damage(
		{100, 1}, box({100, 100}, {2, 2}),
		{false, false, ShotPlaychar::reimu, 6, 1, 0, {0, 0}}, score
	);
	if(!report(normal && *normal == 80 && score.score_delta == 80, "normal")) return false;

	score.score_delta = 0;
	const auto boss_stage_one = shots_hittest_apply_damage(
		{100, 1}, box({100, 100}, {2, 2}),
		{false, true, ShotPlaychar::reimu, 1, 1, 0, {0, 0}}, score
	);
	if(!report(boss_stage_one && *boss_stage_one == 114 && score.score_delta == 114, "boss-stage-one")) {
		return false;
	}

	score.score_delta = 0;
	const auto bomb_stage_six = shots_hittest_apply_damage(
		{100, 1}, box({100, 100}, {2, 2}),
		{true, true, ShotPlaychar::reimu, 6, 0, 0, {0, 0}}, score
	);
	if(!report(bomb_stage_six && *bomb_stage_six == 35 && score.score_delta == 35, "bomb-stage-six")) {
		return false;
	}

	score.score_delta = 0;
	const auto marisa_stripe = shots_hittest_apply_damage(
		{10, 1}, box({100, 100}, {10, 10}),
		{true, false, ShotPlaychar::marisa, 1, 1, 16, {100, 1000}}, score
	);
	if(!report(marisa_stripe && *marisa_stripe == 32 && score.score_delta == 32, "marisa-stripe")) {
		return false;
	}
	score.score_delta = 0;
	const auto wrapped_scale = shots_hittest_apply_damage(
		{65535, 1}, box({100, 100}, {1, 1}),
		{false, true, ShotPlaychar::marisa, 1, 1, 0, {0, 0}}, score
	);
	// The guest word wraps 0xFFFF*4 to 0xFFFC before /5. A wide host
	// multiply would incorrectly produce 52428 instead of 13106.
	if(!report(wrapped_scale && *wrapped_scale == 13106 && score.score_delta == 13106, "wrapped-scale")) {
		return false;
	}
	score.score_delta = 0;
	const auto outside_stripe = shots_hittest_apply_damage(
		{10, 1}, box({100, 100}, {10, 10}),
		{true, false, ShotPlaychar::marisa, 1, 1, 16, {111, 89}}, score
	);
	if(!report(outside_stripe && *outside_stripe == 10 && score.score_delta == 10, "outside-stripe")) {
		return false;
	}
	for(const auto [frame, expected] : std::array<std::pair<std::uint8_t,
		std::uint16_t>, 4>{
		std::pair<std::uint8_t, std::uint16_t>{static_cast<std::uint8_t>(15U), static_cast<std::uint16_t>(10U)},
		std::pair<std::uint8_t, std::uint16_t>{static_cast<std::uint8_t>(16U), static_cast<std::uint16_t>(32U)},
		std::pair<std::uint8_t, std::uint16_t>{static_cast<std::uint8_t>(79U), static_cast<std::uint16_t>(10U)},
		std::pair<std::uint8_t, std::uint16_t>{static_cast<std::uint8_t>(80U), static_cast<std::uint16_t>(10U)}
	}) {
		score.score_delta = 0;
		const auto frame_result = shots_hittest_apply_damage(
			{10, 1}, box({100, 100}, {10, 10}),
			{true, false, ShotPlaychar::marisa, 1, 1, frame, {100, 1000}}, score
		);
		if(!report(frame_result && *frame_result == expected &&
			score.score_delta == expected, "frame-bound")) return false;
	}
	score.score_delta = 0;
	const auto mod4_bomb = shots_hittest_apply_damage(
		{10, 1}, box({100, 100}, {10, 10}),
		{true, false, ShotPlaychar::marisa, 1, 0, 16, {100, 1000}}, score
	);
	if(!report(mod4_bomb && *mod4_bomb == 36 && score.score_delta == 36, "mod4-bomb")) {
		return false;
	}
	score.score_delta = 0;
	const auto x_edge = shots_hittest_apply_damage(
		{10, 1}, box({100, 100}, {10, 10}),
		{true, false, ShotPlaychar::marisa, 1, 1, 16, {110, 1000}}, score
	);
	if(!report(x_edge && *x_edge == 32, "x-edge")) return false;
	score.score_delta = 0;
	const auto x_outside = shots_hittest_apply_damage(
		{10, 1}, box({100, 100}, {10, 10}),
		{true, false, ShotPlaychar::marisa, 1, 1, 16, {111, 1000}}, score
	);
	if(!report(x_outside && *x_outside == 10, "x-outside")) return false;
	score.score_delta = 0;
	const auto y_edge = shots_hittest_apply_damage(
		{10, 1}, box({100, 100}, {10, 10}),
		{true, false, ShotPlaychar::marisa, 1, 1, 16, {100, 90}}, score
	);
	if(!report(y_edge && *y_edge == 32, "y-edge")) return false;
	score.score_delta = 0;
	const auto y_outside = shots_hittest_apply_damage(
		{10, 1}, box({100, 100}, {10, 10}),
		{true, false, ShotPlaychar::marisa, 1, 1, 16, {100, 89}}, score
	);
	if(!report(y_outside && *y_outside == 10, "y-outside")) return false;
	score.score_delta = 0xFFFFFFFFU;
	const auto score_wrap = shots_hittest_apply_damage(
		{2, 1}, box({100, 100}, {1, 1}),
		{false, false, ShotPlaychar::reimu, 1, 1, 0, {0, 0}}, score
	);
	if(!report(score_wrap && *score_wrap == 2 && score.score_delta == 1U, "score-wrap")) {
		return false;
	}

	score.score_delta = 17;
	const auto invalid = shots_hittest_apply_damage(
		{10, 1}, box({100, 100}, {1, 1}),
		{false, false, static_cast<ShotPlaychar>(99), 1, 1, 0, {0, 0}}, score
	);
	return report(!invalid && score.score_delta == 17, "invalid-char");
}

bool test_composed_public_consumers_update_score()
{
	std::array<pc98::th0405::Spark, pc98::th0405::SPARK_COUNT_TH05> regular_sparks{};
	std::uint16_t regular_offset = 0;
	pc98::th0405::SparkPool regular_pool{regular_sparks, regular_offset};
	pc98::th0405::RandomState regular_random{};
	regular_random.ring[0] = 1;
	ShotUpdateState regular_state{};
	regular_state.shots[0] = {
		shot_alive, 0, {{100, 100}, {}, {}}, 0, shot_normal, 10, 0
	};
	regular_state.shots_alive[0] = {{100, 100}, 0};
	regular_state.shots_alive_count = 1;
	std::uint8_t regular_parity = 0;
	ScoreState regular_score{};
	const auto regular = shots_hittest(
		regular_state, box({100, 100}, {1, 1}), regular_parity,
		{false, false, ShotPlaychar::reimu, 1, 1, 0, {0, 0}},
		regular_pool, regular_random, regular_score
	);
	if(!regular || *regular != 10 || regular_score.score_delta != 10 ||
		regular_random.cursor != 1 || regular_state.shots[0].flag != shot_remove) {
		return false;
	}
	std::array<pc98::th0405::Spark, pc98::th0405::SPARK_COUNT_TH05> invalid_sparks{};
	std::uint16_t invalid_offset = 0;
	pc98::th0405::SparkPool invalid_pool{invalid_sparks, invalid_offset};
	pc98::th0405::RandomState invalid_random{};
	ShotUpdateState invalid_state{};
	invalid_state.shots[0] = {
		shot_alive, 0, {{100, 100}, {}, {}}, 0, shot_normal, 10, 0
	};
	invalid_state.shots_alive[0] = {{100, 100}, 0};
	invalid_state.shots_alive_count = 1;
	std::uint8_t invalid_parity = 0;
	ScoreState invalid_score{};
	invalid_score.score_delta = 99;
	const auto invalid_state_before = invalid_state;
	const auto invalid_sparks_before = invalid_sparks;
	const auto invalid_random_before = invalid_random;
	const auto invalid_result = shots_hittest(
		invalid_state, box({100, 100}, {1, 1}), invalid_parity,
		{false, false, static_cast<ShotPlaychar>(99), 1, 1, 0, {0, 0}},
		invalid_pool, invalid_random, invalid_score
	);
	if(invalid_result || invalid_state != invalid_state_before ||
		invalid_sparks != invalid_sparks_before ||
		invalid_random != invalid_random_before || invalid_offset != 0 ||
		invalid_parity != 0 || invalid_score.score_delta != 99) {
		return false;
	}

	std::array<pc98::th0405::Spark, pc98::th0405::SPARK_COUNT_TH05> revenge_sparks{};
	std::uint16_t revenge_offset = 0;
	pc98::th0405::SparkPool revenge_pool{revenge_sparks, revenge_offset};
	pc98::th0405::RandomState revenge_spark_random{};
	revenge_spark_random.ring[0] = 2;
	pc98::th0405::RandomState revenge_random{};
	revenge_random.ring[0] = 0x44;
	revenge_random.ring[1] = 0x55;
	ShotUpdateState revenge_state{};
	revenge_state.shots[0] = {
		shot_alive, 0, {{120, 130}, {}, {}}, 0, shot_normal, 10, 0
	};
	revenge_state.shots_alive[0] = {{120, 130}, 0};
	revenge_state.shots_alive_count = 1;
	std::uint8_t revenge_parity = 0;
	RevengeBulletTemplate template_state{};
	ScoreState revenge_score{};
	bool allocated = false;
	const auto revenge = shots_hittest_revenge(
		revenge_state, box({120, 130}, {1, 1}), revenge_parity,
		{ShotRank::normal, 1}, revenge_pool, revenge_spark_random,
		revenge_random, template_state, revenge_score,
		[&](RevengeBulletTemplate request) noexcept {
			allocated = request.origin == Point{120, 130} && request.angle == 0x44;
		}
	);
	return revenge && *revenge == 10 && revenge_score.score_delta == 10 &&
		allocated && revenge_random.cursor == 1 &&
		revenge_state.shots[0].flag == shot_remove;
}
}

int main()
{
	const auto run = [](bool result, const char *name) noexcept {
		if(!result) std::cerr << "failed: " << name << "\n";
		return result;
	};
	if(!run(test_cached_position_and_zero_extended_damage(), "cached") ||
		!run(test_inclusive_edges_and_hit_count_division(), "edges") ||
		!run(test_word_wrapped_hitbox_arithmetic(), "word-wrap") ||
		!run(test_all_bounds_edges_and_one_unit_misses(), "bounds") ||
		!run(test_repeated_cache_index_and_nonalive_flag(), "repeat") ||
		!run(test_none_valued_cache_position_is_not_filtered(), "none-cache") ||
		!run(test_64_zero_damage_hits_minimum_and_ring_wrap(), "ring-wrap") ||
		!run(test_cache_is_consumed_by_later_callers(), "cache-consume") ||
		!run(test_revenge_callback_order_and_gate(), "legacy-revenge") ||
		!run(test_occupied_hitshot_still_removes_source(), "occupied") ||
		!run(test_malformed_state_refuses_atomically(), "malformed") ||
		!run(test_shared_spark_effect_wrapper(), "spark-effects") ||
		!run(test_revenge_effect_wrapper_owns_gate_rng_and_origin(), "revenge-effects") ||
		!run(test_complete_damage_and_score_wrapper(), "damage") ||
		!run(test_composed_public_consumers_update_score(), "composed")) {
		std::cerr << "TH05 shot collision test failed\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
