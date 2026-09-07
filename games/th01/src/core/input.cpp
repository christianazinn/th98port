#include "th01/core/input.hpp"

#include <cstddef>

namespace th01::core {
namespace {

bool changed(InputState &state, std::size_t slot, std::uint8_t value) noexcept
{
	const bool result = state.history[slot] != value;
	state.history[slot] = value;
	return result;
}

void direction_pair(InputState &state, std::size_t slot,
	std::uint8_t arrow, std::uint8_t keypad, std::uint8_t flag) noexcept
{
	const bool arrow_changed = changed(state, slot, arrow);
	const bool keypad_changed = changed(state, slot + 8U, keypad);
	if(arrow_changed || keypad_changed) {
		if(arrow || keypad) {
			state.direction |= flag;
		} else {
			state.direction &= static_cast<std::uint8_t>(~flag);
		}
	}
}

void boolean_pair(InputState &state, std::size_t slot,
	std::uint8_t arrow, std::uint8_t keypad, bool &output) noexcept
{
	const bool arrow_changed = changed(state, slot, arrow);
	const bool keypad_changed = changed(state, slot + 8U, keypad);
	if(arrow_changed || keypad_changed) {
		output = (arrow || keypad);
	}
}

void doubletap(InputState &state, std::size_t pressed, std::size_t other) noexcept
{
	if(state.bomb_doubletap_frame < 20) {
		state.history[pressed] = static_cast<std::uint8_t>(state.history[pressed] + 1U);
	} else {
		state.bomb_doubletap_frame = 0;
		state.history[pressed] = 1;
		state.history[other] = 0;
	}
}

} // namespace

void sense_input(InputState &state, const RawInput &raw) noexcept
{
	boolean_pair(state, 0, raw.group7 & 0x04U, raw.group8 & 0x08U, state.up);
	boolean_pair(state, 1, raw.group7 & 0x20U, raw.group9 & 0x08U, state.down);
	direction_pair(state, 2, raw.group7 & 0x08U, raw.group8 & 0x40U, left);
	direction_pair(state, 3, raw.group7 & 0x10U, raw.group9 & 0x01U, right);
	const auto z = static_cast<std::uint8_t>(raw.group5 & 0x02U);
	if(changed(state, 4, z)) {
		state.shot = z != 0;
		if(z) { doubletap(state, 12, 13); }
	}
	const auto x = static_cast<std::uint8_t>(raw.group5 & 0x04U);
	if(changed(state, 5, x)) {
		state.strike = x != 0;
		if(x) { doubletap(state, 13, 12); }
	}
	const auto escape = static_cast<std::uint8_t>(raw.group0 & 0x01U);
	if(changed(state, 6, escape) && escape) { state.paused = !state.paused; }
	const auto enter = static_cast<std::uint8_t>(raw.group3 & 0x10U);
	if(changed(state, 7, enter)) {
		if(enter && state.paused && state.shot) { state.player_is_hit = true; }
		state.ok = enter != 0;
	}
	if((state.history[12] >= 2 && state.history[13] >= 2) ||
		(state.direction == both && state.shot)) {
		state.bomb = true;
		state.history[12] = 0;
		state.history[13] = 0;
	}
}

void reset_repeat(InputState &state) noexcept
{
	for(std::size_t slot = 0; slot < 14; ++slot) { state.history[slot] = 0; }
	state.bomb = false;
}

void reset_input(InputState &state) noexcept
{
	state.direction = none;
	state.up = false;
	state.down = false;
	state.ok = false;
	state.paused = false;
	state.shot = false;
	state.strike = false;
	state.memory_enter = false;
	state.memory_leave = false;
	reset_repeat(state);
}

} // namespace th01::core
