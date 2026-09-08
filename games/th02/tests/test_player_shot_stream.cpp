#include "th02/core/player.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>

namespace {
using th02::core::PlayerShotStreamCallbacks;
using th02::core::PlayerShotStreamResult;
using th02::core::PlayerShotStreamState;

enum class Event { volley, sound, bomb };

struct Trace final : PlayerShotStreamCallbacks {
	std::array<Event, 3> events{};
	std::array<PlayerShotStreamState, 3> snapshots{};
	std::size_t count{};
	PlayerShotStreamResult volley_result{PlayerShotStreamResult::completed};

	void record(Event event, const PlayerShotStreamState &state) noexcept
	{
		events[count] = event;
		snapshots[count] = state;
		++count;
	}

	PlayerShotStreamResult shot_volley(const PlayerShotStreamState &state, std::uint8_t &shot_patnum,
		std::uint8_t &shot_option_patnum) noexcept override
	{
		record(Event::volley, state);
		// Prove that the next source-positioned callback and the later tail see
		// a mutation performed by the unported shottype callback.
		shot_patnum = 0xA1;
		shot_option_patnum = 0xB2;
		return volley_result;
	}

	void shot_sound(const PlayerShotStreamState &state) noexcept override
	{
		record(Event::sound, state);
	}

	void bomb_request(const PlayerShotStreamState &state) noexcept override
	{
		record(Event::bomb, state);
	}
};

bool check(bool condition)
{
	return condition;
}
}

int main()
{
	using namespace th02::core;
	constexpr std::uint16_t input_shot = 0x0010;
	constexpr std::uint16_t input_bomb = 0x0020;
	constexpr std::uint16_t input_down = 0x0002;

	// A starts immediately, its volley callback mutates both patnums, the A
	// sound sees that mutation, then the post-sound phase/cooldown reaches bomb.
	PlayerShotStreamState state{0xFF, 0xFF, 0, 0, 9, 0, 0, 0x30, 0x37, 0};
	Trace trace;
	if(player_move_and_shoot_shot_stream_step(
		state, input_shot | input_bomb, trace
	) != PlayerShotStreamResult::completed) return EXIT_FAILURE;
	if(!check(trace.count == 3 && trace.events[0] == Event::volley &&
		trace.events[1] == Event::sound && trace.events[2] == Event::bomb) ||
		!check(trace.snapshots[0].stream_a_phase == 0 &&
			trace.snapshots[0].stream_a_cooldown == 0 &&
			trace.snapshots[0].shot_patnum == 0x30 &&
			trace.snapshots[0].shot_option_patnum == 0x37) ||
		!check(trace.snapshots[1].stream_a_phase == 0 &&
			trace.snapshots[1].stream_a_cooldown == 0 &&
			trace.snapshots[1].shot_patnum == 0xA1 &&
			trace.snapshots[1].shot_option_patnum == 0xB2) ||
		!check(trace.snapshots[2].stream_a_phase == 1 &&
			trace.snapshots[2].stream_a_cooldown == 8 &&
			trace.snapshots[2].shot_patnum == 0xA1 &&
			trace.snapshots[2].shot_option_patnum == 0xB2) ||
		!check(state == trace.snapshots[2])) return EXIT_FAILURE;

	// An A volley jumps over the B cooldown path, so B does not play its
	// wait sound or decrement even when it otherwise could.
	state = {0, 1, 0, 1, 9, 0, 0, 0x30, 0x37, 0};
	trace = {};
	if(player_move_and_shoot_shot_stream_step(
		state, input_shot, trace
	) != PlayerShotStreamResult::completed) return EXIT_FAILURE;
	if(!check(trace.count == 2 && trace.events[0] == Event::volley &&
		trace.events[1] == Event::sound) ||
		!check(state.stream_a_phase == 1 && state.stream_a_cooldown == 8 &&
			state.stream_b_phase == 1 && state.stream_b_cooldown == 1)) return EXIT_FAILURE;

	// A cooling down permits B to fire. At full power, Down grows the spread
	// before the volley and clamps it to 26; B then reaches bomb without SFX.
	state = {2, 0, 1, 0, 9, 0, 0, 0x30, 0x37, 24};
	trace = {};
	if(player_move_and_shoot_shot_stream_step(
		state, input_shot | input_bomb | input_down, trace
	) != PlayerShotStreamResult::completed) return EXIT_FAILURE;
	if(!check(trace.count == 2 && trace.events[0] == Event::volley &&
		trace.events[1] == Event::bomb) ||
		!check(trace.snapshots[0].stream_a_cooldown == 0 &&
			trace.snapshots[0].stream_b_phase == 0 &&
			trace.snapshots[0].stream_b_cooldown == 0 &&
			trace.snapshots[0].spread_angle_delta == 26) ||
		!check(trace.snapshots[1].stream_b_phase == 1 &&
			trace.snapshots[1].stream_b_cooldown == 8 &&
			trace.snapshots[1].shot_patnum == 0xA1 &&
			trace.snapshots[1].shot_option_patnum == 0xB2)) return EXIT_FAILURE;

	// The B wait path plays the sound before decrementing its cooldown.
	state = {2, 1, 1, 1, 8, 0x66, 0x77, 0x30, 0x37, 0};
	trace = {};
	if(player_move_and_shoot_shot_stream_step(
		state, input_shot | input_bomb, trace
	) != PlayerShotStreamResult::completed) return EXIT_FAILURE;
	if(!check(trace.count == 2 && trace.events[0] == Event::sound &&
		trace.events[1] == Event::bomb) ||
		!check(trace.snapshots[0].stream_b_cooldown == 1 &&
			trace.snapshots[0].shot_patnum == 0x66 &&
			trace.snapshots[1].stream_b_cooldown == 0 &&
			trace.snapshots[1].shot_patnum == 0x66)) return EXIT_FAILURE;

	// A no-movement full-power B volley draws the spread back to zero.
	state = {2, 0, 1, 0, 9, 0, 0, 0x30, 0x37, 2};
	trace = {};
	if(player_move_and_shoot_shot_stream_step(
		state, input_shot, trace
	) != PlayerShotStreamResult::completed) return EXIT_FAILURE;
	if(!check(trace.count == 1 && trace.events[0] == Event::volley &&
		trace.snapshots[0].spread_angle_delta == 0)) return EXIT_FAILURE;

	// Releasing A at phase 3 suppresses both streams by forcing B to phase 4,
	// even if B had been inactive; a held bomb is then requested after that.
	state = {3, 0xFF, 5, 7, 8, 0x66, 0x77, 0x30, 0x37, 0};
	trace = {};
	if(player_move_and_shoot_shot_stream_step(
		state, input_bomb, trace
	) != PlayerShotStreamResult::completed) return EXIT_FAILURE;
	if(!check(trace.count == 1 && trace.events[0] == Event::bomb &&
		trace.snapshots[0].stream_a_phase == 0xFF &&
		trace.snapshots[0].stream_b_phase == 4 &&
		state == trace.snapshots[0])) return EXIT_FAILURE;

	// Otherwise a released B at phase 2 becomes inactive after A's cooldown
	// decrement, with no callback in either stream.
	state = {2, 2, 1, 7, 8, 0x66, 0x77, 0x30, 0x37, 0};
	trace = {};
	if(player_move_and_shoot_shot_stream_step(
		state, 0, trace
	) != PlayerShotStreamResult::completed) return EXIT_FAILURE;
	if(!check(trace.count == 0 && state.stream_a_cooldown == 0 &&
		state.stream_b_phase == 0xFF && state.stream_b_cooldown == 7)) return EXIT_FAILURE;

	// Non-max volleys overwrite both handoff patnums with the two unpowered
	// values before calling the selected shottype callback.
	state = {0xFF, 0xFF, 0, 0, 8, 0x11, 0x22, 0x30, 0x37, 0};
	trace = {};
	if(player_move_and_shoot_shot_stream_step(
		state, input_shot, trace
	) != PlayerShotStreamResult::completed) return EXIT_FAILURE;
	if(!check(trace.count == 2 && trace.events[0] == Event::volley &&
		trace.snapshots[0].shot_patnum == 0x34 &&
		trace.snapshots[0].shot_option_patnum == 0x3F)) return EXIT_FAILURE;

	// A cooldown decrement precedes a B selected-volley call. A failed B
	// callback returns at that call site, retaining the A prefix but skipping
	// B's phase/cooldown write and the held-bomb request.
	state = {2, 0, 1, 0, 9, 0, 0, 0x30, 0x37, 0};
	trace = {};
	trace.volley_result = PlayerShotStreamResult::volley_source_iatan2_fault;
	if(player_move_and_shoot_shot_stream_step(
		state, input_shot | input_bomb, trace
	) != PlayerShotStreamResult::volley_source_iatan2_fault) return EXIT_FAILURE;
	if(!check(trace.count == 1 && trace.events[0] == Event::volley &&
		trace.snapshots[0].stream_a_cooldown == 0 &&
		trace.snapshots[0].stream_b_phase == 0 &&
		trace.snapshots[0].stream_b_cooldown == 0) ||
		!check(state.stream_a_cooldown == 0 && state.stream_b_phase == 0 &&
			state.stream_b_cooldown == 0 && state.shot_patnum == 0xA1 &&
			state.shot_option_patnum == 0xB2)) return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
