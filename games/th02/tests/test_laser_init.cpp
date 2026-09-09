#include "th02/core/laser.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

using namespace th02::core;

[[nodiscard]] LaserPoolState patterned_state() noexcept
{
	LaserPoolState state{};
	for(std::size_t i = 0U; i < state.lasers.size(); ++i) {
		state.lasers[i] = {
			static_cast<std::uint8_t>(0x70U + i),
			static_cast<std::uint8_t>(0x20U + i),
			{static_cast<std::int16_t>(100 + i),
				static_cast<std::int16_t>(200 + i)},
			static_cast<std::int16_t>(300 + i),
			static_cast<std::int16_t>(400 + i),
			static_cast<std::uint8_t>(0x30U + i),
			static_cast<std::uint8_t>(0x40U + i),
		};
	}
	state.wait_frames = 0xFEU;
	return state;
}

class RecordingSpawn final : public LaserSpawnCallbacks {
public:
	explicit RecordingSpawn(const LaserPoolState &state_arg) noexcept
		: state(&state_arg)
	{
	}

	const LaserPoolState *state{};
	std::array<std::uint8_t, 4U> sound_ids{};
	std::size_t count{};
	bool callback_follows_full_write{true};

	void sound_effect(std::uint8_t sound_id) noexcept override
	{
		if(count < sound_ids.size()) sound_ids[count] = sound_id;
		if(state != nullptr) {
			const auto slot = count == 0U ? 1U : 3U;
			const auto &laser = state->lasers[slot];
			callback_follows_full_write &= laser.flag == laser_alive &&
				laser.phase == laser_phase_wait && laser.charge_cel == 0U &&
				laser.wait_frames == 254 && laser.patnum_base == 0x34U;
		}
		++count;
	}
};

[[nodiscard]] bool test_reset_retains_all_but_flags_and_template() noexcept
{
	auto state = patterned_state();
	const auto before = state;
	lasers_reset(state);
	if(state.wait_frames != laser_wait_frames_default) return false;
	for(std::size_t i = 0U; i < state.lasers.size(); ++i) {
		const auto &laser = state.lasers[i];
		const auto &old = before.lasers[i];
		if(laser.flag != laser_free || laser.phase != old.phase ||
			laser.origin != old.origin || laser.wait_frames != old.wait_frames ||
			laser.active_frames != old.active_frames ||
			laser.charge_cel != old.charge_cel ||
			laser.patnum_base != old.patnum_base) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool test_add_clip_first_free_raw_flags_and_order() noexcept
{
	auto state = patterned_state();
	state.lasers[1U].flag = laser_free;
	state.lasers[3U].flag = laser_free;
	const auto before_clip = state;
	RecordingSpawn callbacks{state};
	if(lasers_add(state, {31, std::numeric_limits<std::int16_t>::min()},
		-1, 0x1234, callbacks) != LaserAddResult::ignored ||
		lasers_add(state, {416, std::numeric_limits<std::int16_t>::max()},
		-1, 0x1234, callbacks) != LaserAddResult::ignored ||
		state != before_clip || callbacks.count != 0U) {
		return false;
	}
	if(lasers_add(state, {32, std::numeric_limits<std::int16_t>::min()},
		std::numeric_limits<std::int16_t>::min(), 0x1234, callbacks) !=
		LaserAddResult::spawned || callbacks.count != 1U ||
		callbacks.sound_ids[0U] != 6U || !callbacks.callback_follows_full_write ||
		state.lasers[1U] != Laser{laser_alive, laser_phase_wait,
			{32, std::numeric_limits<std::int16_t>::min()}, 254,
			std::numeric_limits<std::int16_t>::min(), 0U, 0x34U}) {
		return false;
	}
	if(lasers_add(state, {415, 0}, 0, 0x1234, callbacks) !=
		LaserAddResult::spawned || callbacks.count != 2U ||
		callbacks.sound_ids[1U] != 6U || !callbacks.callback_follows_full_write ||
		state.lasers[3U] != Laser{laser_alive, laser_phase_wait,
			{415, 0}, 254, 0, 0U, 0x34U}) {
		return false;
	}
	for(auto &laser : state.lasers) laser.flag = 0x7FU;
	const auto full = state;
	return lasers_add(state, {100, 200}, 4, 5, callbacks) ==
		LaserAddResult::ignored && state == full && callbacks.count == 2U;
}

struct InvalidationEvent {
	std::size_t slot{};
	LaserInvalidation invalidation{};
	std::uint8_t flag{};
	std::uint8_t charge_cel{};
	bool operator==(const InvalidationEvent &) const = default;
};

class RecordingInvalidation final : public LaserInvalidationCallbacks {
public:
	explicit RecordingInvalidation(const LaserPoolState &state_arg) noexcept
		: state(&state_arg)
	{
	}

	const LaserPoolState *state{};
	std::array<InvalidationEvent, laser_count> events{};
	std::size_t count{};
	std::size_t next_slot{};

	void invalidate_laser(LaserInvalidation invalidation) noexcept override
	{
		if(state == nullptr || count >= events.size()) return;
		while(next_slot < state->lasers.size() &&
			state->lasers[next_slot].flag == laser_free) {
			++next_slot;
		}
		if(next_slot >= state->lasers.size()) return;
		const auto slot = next_slot++;
		const auto &laser = state->lasers[slot];
		events[count++] = {slot, invalidation, laser.flag, laser.charge_cel};
	}
};

[[nodiscard]] bool test_invalidate_raw_flags_edges_and_source_order() noexcept
{
	auto state = patterned_state();
	for(auto &laser : state.lasers) laser.flag = laser_free;
	state.lasers[0U].flag = laser_alive;
	state.lasers[0U].origin = {100, 200};
	state.lasers[0U].charge_cel = laser_charge_cels;
	state.lasers[1U].flag = laser_remove;
	state.lasers[1U].origin = {1, 2};
	state.lasers[1U].charge_cel = laser_charge_cels;
	state.lasers[2U].flag = 0x7FU;
	state.lasers[2U].origin = {50, 450};
	state.lasers[2U].charge_cel = static_cast<std::uint8_t>(laser_charge_cels + 1U);
	state.lasers[4U].flag = laser_alive;
	state.lasers[4U].origin = {300, 301};
	state.lasers[4U].charge_cel = 0U;
	const auto before = state;
	RecordingInvalidation callbacks{state};
	lasers_invalidate(state, 0xFFFF'FFF8U, callbacks);
	if(callbacks.count != 4U ||
		callbacks.events[0U] != InvalidationEvent{0U, {{92, 192}, 32, 32},
			laser_alive, laser_charge_cels} ||
		callbacks.events[1U] != InvalidationEvent{1U, {{-7, -6}, 32, 32},
			laser_remove, laser_charge_cels} ||
		callbacks.events[2U] != InvalidationEvent{2U, {{50, 450}, 16, -50},
			0x7FU, static_cast<std::uint8_t>(laser_charge_cels + 1U)} ||
		callbacks.events[3U] != InvalidationEvent{4U, {{292, 293}, 32, 32},
			laser_alive, 0U}) {
		return false;
	}
	if(state.lasers[0U].flag != laser_alive ||
		state.lasers[0U].charge_cel != static_cast<std::uint8_t>(laser_charge_cels + 1U) ||
		state.lasers[1U].flag != laser_free ||
		state.lasers[1U].charge_cel != static_cast<std::uint8_t>(laser_charge_cels + 1U) ||
		state.lasers[2U] != before.lasers[2U] ||
		state.lasers[4U].charge_cel != 1U) {
		return false;
	}

	state = before;
	RecordingInvalidation no_charge_increment{state};
	lasers_invalidate(state, 3U, no_charge_increment);
	return no_charge_increment.count == 4U &&
		state.lasers[0U].charge_cel == laser_charge_cels &&
		state.lasers[1U].charge_cel == laser_charge_cels &&
		state.lasers[1U].flag == laser_free && state.lasers[4U].charge_cel == 0U;
}

} // namespace

int main()
{
	if(!test_reset_retains_all_but_flags_and_template()) {
		std::fputs("FAIL reset\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_add_clip_first_free_raw_flags_and_order()) {
		std::fputs("FAIL add\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_invalidate_raw_flags_edges_and_source_order()) {
		std::fputs("FAIL invalidate\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
