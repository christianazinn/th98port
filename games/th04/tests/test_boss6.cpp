#include "th04/core/b6ent.hpp"
#include "th04/core/boss.hpp"
#include "th04/core/state.hpp"
#include "th04/core/stage.hpp"

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
	void pointnum(Point point, std::uint16_t, bool) noexcept override {
		pointnums.push_back(point);
	}
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	Trace initialization{};
	if(!gameplay_init(state, 4U, 0U, 0U, 3U, 3U, false)) std::abort();
	stage_init(state, initialization);
	state.random.seed = 318U;
	state.player_pos.cur = {3000, 5000};
	state.reimu_a.homing_target = {homing_target_none, homing_target_none};
	return state;
}

[[nodiscard]] std::size_t bullets_alive(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &bullet : state.bullets.pool) {
		if(bullet.flag == bullet_alive) ++count;
	}
	return count;
}

[[nodiscard]] std::size_t live_chasecrosses(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &cross : state.yuuka6_customs.chasecrosses) {
		if(cross.flag == Yuuka6ChasecrossFlag::alive) ++count;
	}
	return count;
}

[[nodiscard]] bool setup_and_entrance()
{
	auto state = source_state();
	Trace presentation{};
	stage6_fights_setup(state);
	if(state.midboss_active || state.midboss.frames_until != 60000U ||
		state.boss_active || state.boss.pos.cur != Point{3072, 1280} ||
		state.boss.sprite != 128U || state.boss.hitbox_radius != Point{384, 768} ||
		state.boss.statebytes[0U] != 96U || state.boss.statebytes[1U] != 4U) {
		return false;
	}
	stage6_boss_activate(state);
	for(unsigned frame{}; frame < 129U; ++frame) {
		if(!stage6_fights_update(state, presentation)) return false;
	}
	if(state.boss.phase != 1U || state.boss.phase_frame != 0 ||
		presentation.sounds != std::vector<std::uint8_t>{13U}) return false;
	presentation.sounds.clear();
	for(unsigned frame{}; frame < 64U; ++frame) {
		if(!stage6_fights_update(state, presentation)) return false;
	}
	return state.boss.phase == 2U && state.boss.phase_frame == 0 &&
		state.boss.hp == 13300 && state.boss.phase_end_hp == 10600 &&
		state.boss.yuuka6_sprite_flag == Yuuka6SpriteFlag::parasol_back_open &&
		state.boss.yuuka6_phase2_fly_path <= 1U && presentation.sounds.empty();
}

[[nodiscard]] bool patterns_and_customs()
{
	{
		auto state = source_state();
		Trace presentation{};
		stage6_fights_setup(state);
		stage6_boss_activate(state);
		state.boss.phase = 2U;
		state.boss.phase_frame = 64;
		state.boss.hp = 13300;
		state.boss.phase_end_hp = 10600;
		if(!stage6_fights_update(state, presentation) || bullets_alive(state) != 40U ||
			state.boss.phase_frame != 65 ||
			presentation.sounds != std::vector<std::uint8_t>{9U}) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage6_fights_setup(state);
		stage6_boss_activate(state);
		state.boss.phase = 4U;
		state.boss.phase_frame = 64;
		state.boss.hp = 10600;
		state.boss.phase_end_hp = 5400;
		if(!stage6_fights_update(state, presentation) ||
			state.yuuka6_customs.safetycircle.flag != Yuuka6SafetyCircleFlag::grow ||
			state.yuuka6_customs.safetycircle.radius_filled != 16 ||
			state.boss.phase_frame != 65 ||
			presentation.sounds != std::vector<std::uint8_t>{8U}) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage6_fights_setup(state);
		stage6_boss_activate(state);
		state.boss.phase = 8U;
		state.boss.phase_frame = 64;
		state.boss.hp = 5400;
		state.boss.phase_end_hp = 3400;
		state.boss.yuuka6_mirror_state = 2U;
		state.boss.yuuka6_mirror_pos = {3072, 1280};
		if(!stage6_fights_update(state, presentation) || bullets_alive(state) != 12U ||
			state.thicklasers.pool[0].flag != ThickLaserFlag::line ||
			state.thicklasers.pool[1].flag != ThickLaserFlag::line ||
			state.thicklasers.pool[0].radius_max != 96 ||
			state.thicklasers.pool[0].cur_flag_frame != 1 ||
			state.boss.phase_frame != 65 ||
			presentation.sounds != std::vector<std::uint8_t>{3U, 5U, 5U}) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage6_fights_setup(state);
		stage6_boss_activate(state);
		state.stage_frame = 8U;
		state.boss.phase = 14U;
		state.boss.mode = 2U;
		state.boss.phase_frame = 56;
		state.boss.hp = 1200;
		state.boss.phase_end_hp = 0;
		if(!stage6_fights_update(state, presentation) || live_chasecrosses(state) != 2U ||
			state.yuuka6_customs.chasecrosses[0].age != 1U ||
			state.boss.phase_frame != 57 ||
			presentation.sounds != std::vector<std::uint8_t>{3U}) return false;
	}
	return true;
}

[[nodiscard]] bool defeat_and_render_tail()
{
	{
		auto state = source_state();
		Trace presentation{};
		stage6_fights_setup(state);
		stage6_boss_activate(state);
		state.boss.phase = 16U;
		state.boss.phase_frame = 0;
		state.boss.hp = 1200;
		state.boss.phase_end_hp = 0;
		state.bullets.template_.spawn_type = static_cast<std::uint8_t>(
			BulletSpawnType::pellet
		);
		if(!stage6_fights_update(state, presentation) || bullets_alive(state) != 16U ||
			state.boss.phase_frame != 1 ||
			state.bullets.template_.spawn_type != static_cast<std::uint8_t>(
				BulletSpawnType::bullet16
			)) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage6_fights_setup(state);
		stage6_boss_activate(state);
		state.boss.phase = 17U;
		state.boss.phase_frame = 31;
		state.boss.phase_state = 1U;
		if(!stage6_fights_update(state, presentation) || state.boss.phase != 254U ||
			state.boss.phase_frame != 0 || !state.bullets.zap.active() ||
			state.score.score_delta != 24064U || presentation.pointnums.size() != 70U ||
			state.player_invincibility_time != 255U || !state.boss_explosions.big.alive ||
			presentation.sounds != std::vector<std::uint8_t>{15U, 12U}) return false;
		state.boss.damage_this_frame = 1U;
		stage6_fights_render_tail(state, presentation);
		if(state.boss.damage_this_frame != 1U || state.boss_explosions.big.age != 0U ||
			state.boss_explosions.big_frame != 0U) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage6_fights_setup(state);
		stage6_boss_activate(state);
		state.boss.phase = 255U;
		if(!stage6_fights_update(state, presentation) || !state.stage_clear_requested ||
			state.palette_tone != 60U || !state.palette_changed) return false;
	}
	return true;
}

} // namespace

int main()
{
	const std::array checks{
		setup_and_entrance(), patterns_and_customs(), defeat_and_render_tail(),
	};
	for(std::size_t index{}; index < checks.size(); ++index) {
		if(!checks[index]) {
			std::cerr << "TH04 Stage 6 boss check failed: " << index << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
