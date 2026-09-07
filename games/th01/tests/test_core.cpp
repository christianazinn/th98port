#include "th01/core/input.hpp"
#include "th01/core/motion.hpp"
#include "th01/core/orb.hpp"
#include "pc98/random.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {
int failures{};
void check(bool condition, const char *message)
{
	if(!condition) { std::cerr << message << '\n'; ++failures; }
}

// Independent transcription of IRAND's three 16-bit MULs and ADC.
std::uint16_t limb_irand(std::uint32_t &seed)
{
	const auto low = seed & 0xFFFFU;
	const auto high = seed >> 16U;
	const auto cx = ((0x4E35U * high) + (0x015AU * low)) & 0xFFFFU;
	const auto product = (0x4E35U * low);
	const auto low_plus_one = (product & 0xFFFFU) + 1U;
	const auto result_high = ((product >> 16U) + cx + (low_plus_one >> 16U)) & 0xFFFFU;
	seed = (result_high << 16U) | (low_plus_one & 0xFFFFU);
	return static_cast<std::uint16_t>(result_high & 0x7FFFU);
}

void random_tests()
{
	std::uint32_t seed = 0xE9B35FBAU;
	check(pc98::irand(seed) == 0x6AB1U && seed == 0x6AB17D83U, "IRAND known first state");
	check(pc98::irand(seed) == 0x4E0AU && seed == 0xCE0AE620U, "IRAND keeps seed sign bit");
	for(const auto initial : std::array<std::uint32_t, 6>{0, 1, 0xFFFFFFFFU, 0xFFFF0000U, 0x0000FFFFU, 0xE9B35FBAU}) {
		auto actual = initial;
		auto expected = initial;
		for(int sample = 0; sample < 65536; ++sample) {
			const auto expected_output = limb_irand(expected);
			const auto actual_output = pc98::irand(actual);
			if(actual != expected || actual_output != expected_output) {
				check(false, "IRAND differs from MUL/ADC instruction model");
				break;
			}
		}
	}
}

void input_tests()
{
	using namespace th01::core;
	InputState state{};
	const RawInput z{.group5 = 0x02};
	sense_input(state, z);
	check(state.shot && state.history[4] == 2 && state.history[12] == 1, "Z rising edge records raw mask and press");
	state.shot = false;
	sense_input(state, z);
	check(!state.shot && state.history[12] == 1, "Consumer-cleared shot stays cleared while key held");
	sense_input(state, {});
	sense_input(state, z);
	check(state.shot && state.history[12] == 2, "Z requires release before retrigger");

	reset_input(state);
	sense_input(state, {.group7 = 0x0C});
	check(state.up && state.direction == left, "Arrow up and left");
	sense_input(state, {.group8 = 0x48});
	check(state.up && state.direction == left && state.history[0] == 0 && state.history[8] == 8, "Numpad handover preserves paired output and raw history");
	sense_input(state, {.group7 = 0x30, .group8 = 0x40});
	check(!state.up && state.down && state.direction == both, "Independent directions combine");
	sense_input(state, {.group9 = 0x09});
	check(state.down && state.direction == right, "Numpad down/right independent of arrows");

	for(const auto frame : std::array<std::int16_t, 2>{19, 20}) {
		state = {};
		state.bomb_doubletap_frame = 20;
		sense_input(state, {.group5 = 6});
		sense_input(state, {});
		state.bomb_doubletap_frame = frame;
		sense_input(state, {.group5 = 6});
		check(state.bomb == (frame == 19), "Bomb window excludes frame twenty");
		if(frame == 20) {
			check(state.history[12] == 1 && state.history[13] == 1 && state.bomb_doubletap_frame == 0,
				"Z starts new window before X is processed");
		}
	}
	state = {};
	sense_input(state, {.group5 = 2, .group7 = 0x18});
	check(state.bomb && state.history[12] == 0 && state.history[13] == 0, "Both directions plus shot triggers alternate bomb");
	sense_input(state, {});
	check(state.bomb, "Bomb is a consumer-cleared latch");
	state = {};
	state.history[12] = 255;
	sense_input(state, z);
	check(state.history[12] == 0, "Press count wraps at eight bits");

	state = {};
	sense_input(state, {.group0 = 1, .group3 = 0x10, .group5 = 2});
	check(state.paused && state.ok && state.player_is_hit, "Z then pause then OK source order");
	sense_input(state, {.group0 = 1});
	check(state.paused && !state.ok, "Held Escape does not toggle; Return release clears OK");
	sense_input(state, {});
	sense_input(state, {.group0 = 1});
	check(!state.paused, "Fresh Escape resumes");
	state.bomb_doubletap_frame = 19;
	for(int poll = 0; poll < 100; ++poll) { sense_input(state, {}); }
	check(state.bomb_doubletap_frame == 19, "Input polling alone never advances doubletap clock");

	state = {};
	state.history.fill(0xFF);
	state.direction = both;
	state.shot = state.strike = state.ok = state.paused = state.bomb = true;
	state.up = state.down = state.memory_enter = state.memory_leave = true;
	state.player_is_hit = true;
	state.bomb_doubletap_frame = 42;
	reset_repeat(state);
	check(state.history[0] == 0 && state.history[13] == 0 && state.history[14] == 255 && state.history[15] == 255,
		"Repeat reset clears exactly fourteen history bytes");
	check(!state.bomb && state.shot && state.ok && state.paused && state.direction == both,
		"Repeat reset preserves output flags except bomb");
	reset_input(state);
	check(!state.shot && !state.strike && !state.ok && !state.paused && !state.up && !state.down &&
		!state.memory_enter && !state.memory_leave && state.direction == none, "Full reset clears input outputs");
	check(state.player_is_hit && state.bomb_doubletap_frame == 42 && state.history[15] == 255,
		"Full reset preserves hit, timer, and debug history");
}

void motion_tests()
{
	using th01::core::move_player;
	check(move_player(304, -4) == 300 && move_player(304, 4) == 308, "Player integer motion");
	check(move_player(2, -4) == 0 && move_player(606, 4) == 608, "Clamp outside playfield");
	check(move_player(4, -4) == 0 && move_player(604, 4) == 608, "Exact boundaries");
	check(move_player(32767, 1) == 0 && move_player(-32768, -1) == 608,
		"16-bit assignment precedes signed clamp");
}
} // namespace

void orb_tests()
{
	using namespace th01::core;
	using enum OrbVelocityX;
	OrbHorizontal orb{2, left4};
	move_orb_x(orb, left8);
	check(orb.left == -6 && orb.velocity == right4, "Orb overshoot retained; stored speed reflected");
	orb = {604, left8};
	move_orb_x(orb, right4);
	check(orb.left == 608 && orb.velocity == left8, "Movement argument does not replace stored velocity");
	orb = {32767, right8};
	move_orb_x(orb, right4);
	check(orb.left == -32765 && orb.velocity == right8, "Orb wrap precedes signed edge test");
	orb = {0, left4};
	move_orb_x(orb, stationary);
	check(orb.left == 0 && orb.velocity == right4, "Zero displacement still tests edges");
	check(reflect_orb_x(stationary) == stationary &&
		reflect_orb_x(reflect_orb_x(left8)) == left8, "Orb horizontal reflection mapping");

	// All 16-bit coordinates and independent movement/stored velocity codes.
	// The reference uses wide arithmetic and lookup tables.
	constexpr std::array<int, 5> displacement{0, -4, 4, -8, 8};
	constexpr std::array<int, 5> left_edge{0, 2, 2, 4, 4};
	constexpr std::array<int, 5> right_edge{0, 1, 1, 3, 3};
	for(int coordinate = -32768; coordinate <= 32767; ++coordinate) {
		for(std::size_t movement = 0; movement < 5; ++movement) {
			int expected_left = coordinate + displacement[movement];
			if(expected_left > 32767) expected_left -= 65536;
			if(expected_left < -32768) expected_left += 65536;
			for(std::size_t stored = 0; stored < 5; ++stored) {
				orb = {static_cast<std::int16_t>(coordinate), static_cast<OrbVelocityX>(stored)};
				move_orb_x(orb, static_cast<OrbVelocityX>(movement));
				const int expected_velocity = expected_left <= 0 ? left_edge[stored] :
					(expected_left >= 608 ? right_edge[stored] : static_cast<int>(stored));
				if(orb.left != expected_left || static_cast<int>(orb.velocity) != expected_velocity) {
					check(false, "Orb horizontal domain differs from 16-bit reference");
					return;
				}
			}
		}
	}
	std::int16_t left_frame = 0;
	std::int16_t right_frame = 0;
	for(int tick = 1; tick <= 30; ++tick) {
		left_frame = rotate_orb(left_frame, left4);
		right_frame = rotate_orb(right_frame, right8);
		check(left_frame == tick % 10 && right_frame == (30 - tick) % 10,
			"Orb rotation direction and asymmetric fourth cel");
	}
	check(rotate_orb(4, stationary) == 4 && rotate_orb(32767, left4) == 9 &&
		rotate_orb(-32768, right4) == 0, "Orb rotation retains 16-bit update ordering");
}

int main()
{
	random_tests();
	input_tests();
	motion_tests();
	orb_tests();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
