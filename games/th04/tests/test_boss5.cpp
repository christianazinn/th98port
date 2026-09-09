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

[[nodiscard]] bool stage5_setup_and_entrance()
{
	auto state = source_state();
	Trace presentation{};
	stage5_fights_setup(state);
	if(state.midboss_active || state.midboss.frames_until != 60000U ||
		state.boss_active || state.boss.pos.cur != Point{3072, 1024} ||
		state.boss.sprite != 128U || state.boss.hitbox_radius != Point{416, 416} ||
		state.boss.statebytes[0U] != 180U) return false;
	stage5_boss_activate(state);
	for(unsigned frame{}; frame < 129U; ++frame) {
		if(!stage5_fights_update(state, presentation)) return false;
	}
	if(state.boss.phase != 1U || state.boss.phase_frame != 0 ||
		state.boss.yuuka5_warp_phase != 0U ||
		presentation.sounds != std::vector<std::uint8_t>{13U}) return false;
	presentation.sounds.clear();
	for(unsigned frame{}; frame < 64U; ++frame) {
		if(!stage5_fights_update(state, presentation)) return false;
	}
	return state.boss.phase == 2U && state.boss.phase_frame == 0 &&
		state.boss.hp == 9000 && state.boss.phase_end_hp == 7900 &&
		state.boss.pos.cur == Point{3072, 768} &&
		state.palette[0].r == 64U && state.palette[0].g == 64U &&
		state.palette[0].b == 64U && presentation.sounds.empty();
}

[[nodiscard]] bool yuuka_patterns_and_warp()
{
	{
		auto state = source_state();
		Trace presentation{};
		stage5_fights_setup(state);
		stage5_boss_activate(state);
		state.boss.phase = 2U;
		state.boss.mode = 0U;
		state.boss.phase_frame = 1;
		state.boss.hp = 9000;
		state.boss.phase_end_hp = 0;
		if(!stage5_fights_update(state, presentation)) return false;
		state.boss.phase_frame = 15;
		if(!stage5_fights_update(state, presentation) || bullets_alive(state) != 11U ||
			state.boss.phase_frame != 16 ||
			presentation.sounds != std::vector<std::uint8_t>{3U}) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage5_fights_setup(state);
		stage5_boss_activate(state);
		state.boss.phase = 2U;
		state.boss.mode = 255U;
		state.boss.phase_frame = 31;
		state.boss.hp = 9000;
		state.boss.phase_end_hp = 0;
		state.boss.damage_this_frame = 1U;
		if(!stage5_fights_update(state, presentation) ||
			state.boss.yuuka5_warp_phase != 1U || state.boss.phase_frame != 32 ||
			state.boss.pos.cur.y != 1280 || state.boss.damage_this_frame != 0U) {
			return false;
		}
		if(!stage5_fights_update(state, presentation) ||
			state.boss.yuuka5_warp_phase != 2U || state.boss.phase_frame != 1 ||
			state.boss.pos.cur.y != 1280) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage5_fights_setup(state);
		stage5_boss_activate(state);
		state.boss.phase = 4U;
		state.boss.mode = 0U;
		state.boss.phase_frame = 17;
		state.boss.hp = 9000;
		state.boss.phase_end_hp = 0;
		if(!stage5_fights_update(state, presentation)) return false;
		presentation.sounds.clear();
		state.boss.phase_frame = 32;
		if(!stage5_fights_update(state, presentation) || bullets_alive(state) != 7U ||
			state.boss.phase_frame != 33 ||
			presentation.sounds != std::vector<std::uint8_t>{9U}) return false;
	}
	return true;
}

[[nodiscard]] bool yuuka_laser_and_defeat_render_tail()
{
	{
		auto state = source_state();
		Trace presentation{};
		stage5_fights_setup(state);
		stage5_boss_activate(state);
		state.boss.phase = 13U;
		state.boss.phase_frame = 96;
		state.boss.hp = 4500;
		state.boss.phase_end_hp = 3300;
		if(!stage5_fights_update(state, presentation) ||
			state.thicklasers.pool[0].flag != ThickLaserFlag::line ||
			state.thicklasers.pool[0].radius_max != 180 ||
			state.thicklasers.pool[0].cur_flag_frame != 1 ||
			state.boss.phase_frame != 97 ||
			presentation.sounds != std::vector<std::uint8_t>{5U}) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage5_fights_setup(state);
		stage5_boss_activate(state);
		state.boss.phase = 18U;
		state.boss.phase_frame = 31;
		state.boss.phase_state = 1U;
		if(!stage5_fights_update(state, presentation) || state.boss.phase != 254U ||
			state.boss.phase_frame != 0 || !state.bullets.zap.active() ||
			state.score.score_delta != 11264U || presentation.pointnums.size() != 60U ||
			state.player_invincibility_time != 255U || !state.boss_explosions.big.alive ||
			presentation.sounds != std::vector<std::uint8_t>{15U, 12U}) return false;
		state.boss.damage_this_frame = 1U;
		stage5_fights_render_tail(state, presentation);
		if(state.boss.damage_this_frame != 1U || state.boss_explosions.big.age != 1U ||
			state.boss_explosions.big_frame != 1U) return false;
	}
	return true;
}

} // namespace

int main()
{
	const std::array checks{
		stage5_setup_and_entrance(), yuuka_patterns_and_warp(),
		yuuka_laser_and_defeat_render_tail(),
	};
	for(std::size_t index{}; index < checks.size(); ++index) {
		if(!checks[index]) {
			std::cerr << "TH04 Stage 5 boss check failed: " << index << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
