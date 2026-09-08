#include "th05/core/shot_collision.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
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
}

int main()
{
	if(!test_cached_position_and_zero_extended_damage() ||
		!test_inclusive_edges_and_hit_count_division() ||
		!test_word_wrapped_hitbox_arithmetic() ||
		!test_all_bounds_edges_and_one_unit_misses() ||
		!test_repeated_cache_index_and_nonalive_flag() ||
		!test_none_valued_cache_position_is_not_filtered() ||
		!test_64_zero_damage_hits_minimum_and_ring_wrap() ||
		!test_cache_is_consumed_by_later_callers() ||
		!test_revenge_callback_order_and_gate() ||
		!test_occupied_hitshot_still_removes_source() ||
		!test_malformed_state_refuses_atomically()) {
		std::cerr << "TH05 shot collision test failed\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
