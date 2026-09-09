#include "th02/core/laser_update.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

using namespace th02::core;

class RecordingCallbacks final : public LaserUpdateCallbacks {
public:
	explicit RecordingCallbacks(const PlayerHitState *hit_arg = nullptr) noexcept
		: hit(hit_arg)
	{
	}

	const PlayerHitState *hit{};
	std::array<std::uint8_t, 8U> sounds{};
	std::size_t sound_count{};
	std::array<LaserPresentation, 32U> presentations{};
	std::size_t presentation_count{};
	bool presentations_precede_hit{true};

	void sound_effect(std::uint8_t sound_id) noexcept override
	{
		if(sound_count < sounds.size()) sounds[sound_count] = sound_id;
		++sound_count;
	}

	void render_laser(LaserPresentation presentation) noexcept override
	{
		if(hit != nullptr && hit->raw_hit != 0U) {
			presentations_precede_hit = false;
		}
		if(presentation_count < presentations.size()) {
			presentations[presentation_count] = presentation;
		}
		++presentation_count;
	}
};

class RecordingInvalidation final : public LaserInvalidationCallbacks {
public:
	std::array<LaserInvalidation, 4U> invalidations{};
	std::size_t count{};

	void invalidate_laser(LaserInvalidation invalidation) noexcept override
	{
		if(count < invalidations.size()) invalidations[count] = invalidation;
		++count;
	}
};

[[nodiscard]] PlayerFrameState player_at(
	std::int16_t x, std::int16_t y
) noexcept
{
	PlayerFrameState frame{};
	frame.player.current = {x, y};
	return frame;
}

[[nodiscard]] Laser alive_laser(
	std::uint8_t phase, ScreenPoint origin = {100, 100}
) noexcept
{
	return {laser_alive, phase, origin, 1, 1, laser_charge_cels, 50U};
}

[[nodiscard]] bool test_charge_draws_and_decrements_wait_every_call() noexcept
{
	LaserPoolState state{};
	state.lasers[0U] = {laser_alive, laser_phase_wait, {100, 10},
		std::numeric_limits<std::int16_t>::min(), 3, 3U, 50U};
	PlayerHitState hit{};
	RecordingCallbacks callbacks{&hit};
	lasers_update_and_render(state, player_at(300, 300), hit, 0U, 395,
		callbacks);
	return state.lasers[0U].wait_frames ==
		std::numeric_limits<std::int16_t>::max() &&
		state.lasers[0U].phase == laser_phase_wait &&
		callbacks.sound_count == 0U && callbacks.presentation_count == 1U &&
		callbacks.presentations[0U] == LaserPresentation{
			LaserPresentationKind::charge, {92, 397}, 33};
}

[[nodiscard]] bool test_charge_handover_between_invalidate_and_update() noexcept
{
	LaserPoolState state{};
	state.lasers[0U] = {laser_alive, laser_phase_wait, {100, 100}, 5, 3,
		3U, 50U};
	RecordingInvalidation invalidation{};
	lasers_invalidate(state, 8U, invalidation);
	if(state.lasers[0U].charge_cel != laser_charge_cels ||
		invalidation.count != 1U || invalidation.invalidations[0U] !=
		LaserInvalidation{{92, 92}, 32, 32}) {
		return false;
	}

	PlayerHitState hit{};
	RecordingCallbacks update{&hit};
	lasers_update_and_render(state, player_at(300, 300), hit, 8U, 0, update);
	if(state.lasers[0U].wait_frames != 4 || update.presentation_count == 0U ||
		update.presentations[0U].kind != LaserPresentationKind::head) {
		return false;
	}

	RecordingInvalidation after_handover{};
	lasers_invalidate(state, 9U, after_handover);
	return state.lasers[0U].charge_cel == laser_charge_cels &&
		after_handover.count == 1U && after_handover.invalidations[0U] ==
			LaserInvalidation{{92, 92}, 32, 32};
}

[[nodiscard]] bool test_wait_grow_active_and_shrink_transitions() noexcept
{
	PlayerHitState hit{};

	LaserPoolState waiting{};
	waiting.lasers[0U] = alive_laser(laser_phase_wait, {100, 350});
	waiting.lasers[0U].wait_frames = 1;
	RecordingCallbacks wait_callbacks{&hit};
	lasers_update_and_render(waiting, player_at(300, 300), hit, 7U, 20,
		wait_callbacks);
	if(waiting.lasers[0U].phase != 2U ||
		waiting.lasers[0U].wait_frames != 0 ||
		wait_callbacks.sound_count != 0U ||
		wait_callbacks.presentation_count != 3U ||
		wait_callbacks.presentations[0U] != LaserPresentation{
			LaserPresentationKind::head, {100, 370}, 93} ||
		wait_callbacks.presentations[1U] != LaserPresentation{
			LaserPresentationKind::beam, {100, 378}, 52} ||
		wait_callbacks.presentations[2U] != LaserPresentation{
			LaserPresentationKind::beam, {100, 394}, 52}) {
		return false;
	}

	LaserPoolState growing{};
	growing.lasers[0U] = alive_laser(3U);
	RecordingCallbacks not_yet{&hit};
	lasers_update_and_render(growing, player_at(300, 300), hit, 7U, 0,
		not_yet);
	if(growing.lasers[0U].phase != 3U || not_yet.sound_count != 0U) {
		return false;
	}
	RecordingCallbacks active{&hit};
	lasers_update_and_render(growing, player_at(300, 300), hit, 8U, 0,
		active);
	if(growing.lasers[0U].phase != laser_phase_active ||
		active.sound_count != 1U || active.sounds[0U] != 7U) {
		return false;
	}

	LaserPoolState terminal{};
	terminal.lasers[0U] = alive_laser(laser_phase_active);
	terminal.lasers[0U].active_frames = 1;
	RecordingCallbacks terminal_callbacks{&hit};
	lasers_update_and_render(terminal, player_at(100, 100), hit, 0U, 0,
		terminal_callbacks);
	if(terminal.lasers[0U].phase != laser_phase_shrink || hit.raw_hit != 0U ||
		terminal_callbacks.presentation_count == 0U ||
		terminal_callbacks.presentations[0U].kind != LaserPresentationKind::beam) {
		return false;
	}
	LaserPoolState ongoing{};
	ongoing.lasers[0U] = alive_laser(laser_phase_active);
	ongoing.lasers[0U].active_frames = 0;
	PlayerHitState raw_only{};
	raw_only.invincibility_time = 99U;
	raw_only.invincible_via_bomb = true;
	raw_only.miss_active = true;
	RecordingCallbacks ongoing_callbacks{&raw_only};
	lasers_update_and_render(ongoing, player_at(100, 100), raw_only, 0U, 0,
		ongoing_callbacks);
	if(ongoing.lasers[0U].active_frames != -1 ||
		ongoing.lasers[0U].phase != laser_phase_active || raw_only.raw_hit != 1U ||
		ongoing_callbacks.presentations_precede_hit == false) {
		return false;
	}

	LaserPoolState done{};
	done.lasers[0U] = alive_laser(8U);
	RecordingCallbacks done_callbacks{&hit};
	lasers_update_and_render(done, player_at(300, 300), hit, 0U, 0,
		done_callbacks);
	return done.lasers[0U].phase == laser_phase_done &&
		done.lasers[0U].flag == laser_remove &&
		done_callbacks.presentation_count == 0U;
}

[[nodiscard]] bool hit_at(
	std::int16_t player_x, std::int16_t player_y, std::uint8_t initial_hit,
	bool expected_hit, bool require_pre_hit_presentations
) noexcept
{
	auto laser = alive_laser(laser_phase_active);
	PlayerHitState hit{initial_hit};
	RecordingCallbacks callbacks{&hit};
	laser_render(laser, player_at(player_x, player_y), hit, 0, callbacks);
	return hit.raw_hit == (expected_hit ? 1U : initial_hit) &&
		callbacks.presentation_count != 0U &&
		(!require_pre_hit_presentations || callbacks.presentations_precede_hit);
}

[[nodiscard]] bool test_collision_boundaries_and_render_order() noexcept
{
	return hit_at(76, 100, 0U, false, false) &&
		hit_at(77, 100, 0U, true, true) &&
		hit_at(107, 100, 0U, true, true) &&
		hit_at(108, 100, 0U, false, false) &&
		hit_at(100, 99, 0U, false, false) &&
		hit_at(100, 100, 2U, false, false);
}

[[nodiscard]] bool test_raw_phase_alive_filter_and_extreme_beam() noexcept
{
	LaserPoolState state{};
	state.lasers[0U] = alive_laser(255U, {100, 350});
	state.lasers[1U] = alive_laser(laser_phase_active, {100, 100});
	state.lasers[1U].flag = laser_remove;
	PlayerHitState hit{};
	RecordingCallbacks callbacks{&hit};
	lasers_update_and_render(state, player_at(300, 300), hit, 0U, 0,
		callbacks);
	if(state.lasers[0U].phase != 0U ||
		state.lasers[1U].flag != laser_remove || callbacks.sound_count != 0U ||
		callbacks.presentation_count != 3U ||
		callbacks.presentations[0U] != LaserPresentation{
			LaserPresentationKind::head, {100, 350}, 91}) {
		return false;
	}

	auto extreme = alive_laser(laser_phase_shrink,
		{100, std::numeric_limits<std::int16_t>::min()});
	RecordingCallbacks extreme_callbacks{&hit};
	laser_render(extreme, player_at(300, 300), hit, 0, extreme_callbacks);
	return extreme_callbacks.presentation_count == 2072U;
}

} // namespace

int main()
{
	if(!test_charge_draws_and_decrements_wait_every_call()) {
		std::fputs("FAIL charge\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_charge_handover_between_invalidate_and_update()) {
		std::fputs("FAIL charge handover\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_wait_grow_active_and_shrink_transitions()) {
		std::fputs("FAIL transitions\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_collision_boundaries_and_render_order()) {
		std::fputs("FAIL collision\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_raw_phase_alive_filter_and_extreme_beam()) {
		std::fputs("FAIL raw/extreme\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
