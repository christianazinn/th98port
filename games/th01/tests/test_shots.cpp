#include "th01/core/shots.hpp"

#include <cstdlib>
#include <iostream>

namespace {
int failures{};
void check(bool value, const char *message)
{
	if(!value) { ++failures; std::cerr << message << '\n'; }
}
}

int main()
{
	using namespace th01::core;
	Shots shots;
	check(shots.slots[0].unknown == 0x50 && shots.slots[1].unknown == 0x60 &&
		shots.slots[2].unknown == 0x70 && shots.slots[3].unknown == 0, "Global constructor state");
	const auto initial = shots;
	check(!add_shot(shots, -1, 100) && !add_shot(shots, 640, 100) && shots == initial,
		"Horizontal spawn rejection leaves every slot unchanged");
	check(add_shot(shots, 0, -32768) == 0 && add_shot(shots, 639, 32767) == 1,
		"Spawn accepts endpoints; no vertical bounds check");
	shots.slots[0].moving = false;
	shots.slots[0].decay_frame = 2;
	check(add_shot(shots, 50, 100) == 2, "Decaying slots are not reusable");
	shots.slots[0].decay_frame = 0;
	check(add_shot(shots, 51, 101) == 0 && shots.slots[0].unknown == 0x50,
		"First free slot reused; unknown retained");
	for(auto &shot : shots.slots) shot.moving = true;
	const auto full = shots;
	check(!add_shot(shots, 320, 200) && shots == full, "Full shot pool does not overwrite");

	// Inclusive asymmetric 14-pixel shot / 6-pixel pellet overlap, all nearby
	// coordinates. Every hit changes exactly the first moving slot.
	for(int y = -16; y <= 8; ++y) {
		for(int x = -16; x <= 8; ++x) {
			shots = {};
			shots.slots[0] = {static_cast<std::int16_t>(100 + x),
				static_cast<std::int16_t>(100 + y), 0x50, true, 0};
			shots.slots[1] = shots.slots[0];
			const auto before = shots;
			const bool overlaps = x >= -14 && x <= 6 && y >= -14 && y <= 6;
			const auto result = hit_pellet(shots, 100, 100);
			check(result.has_value() == overlaps, "Pellet inclusive overlap domain");
			if(overlaps) {
				check(result == 0 && !shots.slots[0].moving && shots.slots[0].decay_frame == 1 &&
					shots.slots[1] == before.slots[1], "Only first pellet collision consumes a shot");
			} else { check(shots == before, "Pellet miss does not mutate"); }
		}
	}
	shots = {};
	shots.slots[0] = {100, 100, 0, true, 1};
	check(!hit_pellet(shots, 100, 100), "Pellet test skips decay frame one");
	check(hit_boss(shots, 100, 100, 0, 0) == 0, "Boss test does not skip decay frame one");
	shots = {};
	shots.slots[0] = {-32768, 100, 0, true, 0};
	check(hit_pellet(shots, 32767, 100) == 0, "Pellet subtraction uses signed 16-bit result");

	for(int y = 99; y <= 111; ++y) {
		for(int x = 199; x <= 221; ++x) {
			shots = {};
			shots.slots[3] = {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), 0, true, 0};
			const bool expected = x >= 200 && x <= 220 && y >= 100 && y <= 110;
			const auto result = hit_boss(shots, 200, 100, 20, 10);
			check(result.has_value() == expected, "Boss uses supplied shot-excluding inclusive box");
			if(expected) check(result == 3, "Boss selects first moving intersecting slot");
		}
	}
	shots = {};
	shots.slots[0] = {32760, 100, 0, true, 0};
	const auto before_wrap = shots;
	check(!hit_boss(shots, 32760, 100, 16, 16) && shots == before_wrap,
		"Boss extent addition wraps before signed comparison");
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
