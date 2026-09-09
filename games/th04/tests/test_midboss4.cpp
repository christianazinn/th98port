#include "th04/core/midboss4.hpp"
#include "th04/core/state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using namespace th04::core;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	std::vector<Point> pointnums{};

	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
	void pointnum(Point point, std::uint16_t, bool) noexcept override
	{
		pointnums.push_back(point);
	}
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	state.rank = 1U;
	state.resources.playperf = 16U;
	state.resources.playperf_max = 32U;
	state.player_pos.cur = {3072, 5000};
	return state;
}

[[nodiscard]] std::size_t bullets_alive(const State &state, bool pellet) noexcept
{
	const auto first = pellet ? 0U : pellet_count;
	const auto last = pellet ? pellet_count : bullet_count;
	std::size_t count{};
	for(std::size_t index = first; index < last; ++index) {
		if(state.bullets.pool[index].flag == bullet_alive) ++count;
	}
	return count;
}

[[nodiscard]] bool setup_activation_and_entrance()
{
	auto state = source_state();
	Trace presentation{};
	state.midboss.stage4_angle_flip = 7U;
	stage4_midboss_setup(state);
	if(state.midboss.frames_until != 2800U ||
		state.midboss.pos.cur != Point{2304, -512} ||
		state.midboss.pos.prev != Point{2304, -512} ||
		state.midboss.pos.velocity != Point{64, 32} || state.midboss.hp != 1200 ||
		state.midboss.sprite != 0U || state.midboss_active ||
		state.midboss.stage4_angle_flip != 7U) return false;

	state.stage_frame = 2799U;
	stage4_midboss_activate(state);
	if(state.midboss_active) return false;
	state.stage_frame = 2800U;
	stage4_midboss_activate(state);
	if(!state.midboss_active || state.midboss.phase != 0U ||
		state.midboss.phase_frame != 0) return false;
	for(unsigned frame = 0U; frame < 48U; ++frame) {
		if(!stage4_midboss_update(state, presentation)) return false;
	}
	return state.midboss.phase == 1U && state.midboss.phase_frame == 0 &&
		state.midboss.pos.cur == Point{5376, 1024} &&
		state.midboss.pos.prev == Point{5312, 992} &&
		state.midboss.pos.velocity == Point{} && state.midboss.stage4_pattern == 0U &&
		state.midboss.stage4_passes == 0U &&
		state.reimu_a.homing_target == Point{5312, 992} && presentation.sounds.empty();
}

[[nodiscard]] bool pattern_ordering_and_stack_mirror()
{
	{
		auto state = source_state();
		Trace presentation{};
		state.midboss.phase = 1U;
		state.midboss.phase_frame = 27;
		state.midboss.stage4_pattern = 0U;
		state.midboss.pos.cur = {3072, 2000};
		if(!stage4_midboss_update(state, presentation)) return false;
		if(state.midboss.phase_frame != 28 || state.random.cursor != 4U ||
			bullets_alive(state, true) != 2U || bullets_alive(state, false) != 2U ||
			state.bullets.template_.spawn_type !=
				static_cast<std::uint8_t>(BulletSpawnType::bullet16) ||
			state.bullets.template_.patnum != 58U ||
			presentation.sounds != std::vector<std::uint8_t>{3U}) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		state.midboss.phase = 1U;
		state.midboss.phase_frame = 40;
		state.midboss.stage4_pattern = 2U;
		state.midboss.stage4_angle_flip = 0U;
		state.midboss.pos.cur = {3072, 2000};
		state.bullets.template_.angle = 0x56U;
		if(!stage4_midboss_update(state, presentation)) return false;
		if(state.midboss.phase_frame != 41 || bullets_alive(state, true) != 0U ||
			bullets_alive(state, false) != 24U ||
			state.bullets.template_.group != BulletGroup::stack ||
			state.bullets.template_.count != 12U ||
			state.bullets.template_.delta != 7U ||
			state.bullets.template_.angle != 0x4EU ||
			presentation.sounds != std::vector<std::uint8_t>{3U}) return false;
	}
	return true;
}

[[nodiscard]] bool damage_defeat_and_rearm()
{
	auto state = source_state();
	Trace presentation{};
	state.midboss_active = true;
	state.midboss.phase = 1U;
	state.midboss.phase_frame = 0;
	state.midboss.stage4_pattern = 0U;
	state.midboss.frames_until = 2800U;
	state.midboss.hp = 1;
	state.midboss.pos.cur = {3072, 2000};
	state.shots.shots[0].flag = shot_alive;
	state.shots.shots[0].pos.cur = state.midboss.pos.cur;
	state.shots.shots[0].damage = 1;
	state.shots.shots_alive[0] = {state.midboss.pos.cur, 0U};
	state.shots.shots_alive_count = 1U;
	if(!stage4_midboss_update(state, presentation)) return false;
	if(state.midboss.phase != 254U || state.midboss.sprite != 4U ||
		state.midboss.phase_frame != 0 || state.midboss.damage_this_frame != 1U ||
		!state.bullets.zap.active() || state.playfield_shake_anim_time != 12 ||
		presentation.sounds != std::vector<std::uint8_t>{4U, 12U} ||
		presentation.pointnums.size() != 30U || state.items.items[0].flag != item_alive ||
		state.items.items[0].type != static_cast<std::uint8_t>(ItemType::bomb)) return false;

	state.midboss.phase_frame = 15;
	state.midboss.sprite = 10U;
	state.midboss.pos.velocity = {32, -32};
	if(!stage4_midboss_update(state, presentation) || state.midboss.phase != 254U ||
		state.midboss.sprite != 11U || state.midboss.phase_frame != 16 ||
		state.midboss.pos.velocity != Point{}) return false;
	state.midboss.phase_frame = 31;
	if(!stage4_midboss_update(state, presentation) || state.midboss.phase != 255U ||
		state.midboss.hp != 0) return false;
	if(!stage4_midboss_update(state, presentation)) return false;
	return !state.midboss_active && state.midboss.frames_until == 5600U &&
		state.midboss.pos.cur == Point{3840, -512} &&
		state.midboss.pos.prev == Point{3840, -512} &&
		state.midboss.pos.velocity == Point{-64, 32} && state.midboss.hp == 1200 &&
		state.midboss.sprite == 0U && state.midboss.phase_frame == 0;
}

[[nodiscard]] bool gap_boundaries_and_render_tail()
{
	auto state = source_state();
	Trace presentation{};
	state.midboss_active = true;
	state.midboss.phase = 1U;
	state.midboss.stage4_pattern = 255U;
	state.midboss.pos.cur = {5376, 2000};
	if(!stage4_midboss_update(state, presentation) || state.midboss.phase_frame != 1 ||
		state.midboss.pos.velocity != Point{-64, 0}) return false;
	state.midboss.pos.cur = {768, 2000};
	state.midboss.phase_frame = 4;
	state.midboss.pos.velocity = {-64, 0};
	if(!stage4_midboss_update(state, presentation) || state.midboss.phase_frame != 0 ||
		state.midboss.stage4_passes != 1U || state.midboss.stage4_pattern != 1U ||
		state.midboss.pos.velocity != Point{}) return false;

	state.midboss.phase = 1U;
	state.midboss.damage_this_frame = 1U;
	state.midboss.pos.cur = {0, 1};
	stage4_midboss_render_tail(state, presentation);
	if(state.midboss.damage_this_frame != 1U) return false;
	state.midboss.pos.cur = {1, 1};
	stage4_midboss_render_tail(state, presentation);
	if(state.midboss.damage_this_frame != 0U) return false;
	state.midboss.damage_this_frame = 1U;
	state.midboss.pos.cur = {6144, 1};
	stage4_midboss_render_tail(state, presentation);
	return state.midboss.damage_this_frame == 1U && presentation.sounds.empty();
}

} // namespace

int main()
{
	const std::array checks{
		setup_activation_and_entrance(), pattern_ordering_and_stack_mirror(),
		damage_defeat_and_rearm(), gap_boundaries_and_render_tail(),
	};
	for(std::size_t index = 0U; index < checks.size(); ++index) {
		if(!checks[index]) {
			std::cerr << "TH04 Stage 4 midboss check failed: " << index << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
