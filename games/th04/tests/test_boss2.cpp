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
	if(!gameplay_init(state, 1U, 0U, 0U, 3U, 3U, false)) std::abort();
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

[[nodiscard]] std::size_t spawnrays_growing(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &ray : state.boss.kurumi_spawnrays) {
		if(ray.flag == KurumiSpawnrayFlag::grow) ++count;
	}
	return count;
}

[[nodiscard]] bool setup_and_midboss_entrance()
{
	auto state = source_state();
	Trace presentation{};
	stage2_fights_setup(state);
	if(state.midboss.frames_until != 2600U ||
		state.midboss.pos.cur != Point{3072, -512} || state.midboss.hp != 750 ||
		state.midboss_active || state.boss_active ||
		state.boss.pos.cur != Point{3072, 1296} ||
		state.boss.hitbox_radius != Point{384, 384}) return false;
	state.stage_frame = 2599U;
	stage2_midboss_activate(state);
	if(state.midboss_active) return false;
	state.stage_frame = 2600U;
	stage2_midboss_activate(state);
	if(!state.midboss_active || state.midboss.phase != 0U) return false;
	for(unsigned frame = 0U; frame < 96U; ++frame) {
		if(!stage2_fights_update(state, presentation)) return false;
	}
	return state.midboss.phase == 1U && state.midboss.phase_frame == 0 &&
		state.midboss.pos.cur == Point{3072, 1024};
}

[[nodiscard]] bool midboss_survival_exit_and_top_spark()
{
	auto state = source_state();
	Trace presentation{};
	stage2_fights_setup(state);
	state.midboss_active = true;
	state.midboss.phase = 1U;
	state.midboss.phase_frame = 51;
	state.midboss.stage2_pattern = 0xFFU;
	state.midboss.stage2_direction = 1U;
	state.midboss.stage2_cycles = 16U;
	state.midboss.hp = 750;
	state.midboss.pos.cur = {3072, 1024};
	if(!stage2_fights_update(state, presentation) || state.midboss.phase != 2U ||
		state.midboss.phase_frame != 0 || state.midboss.stage2_cycles != 17U ||
		state.midboss.pos.velocity != Point{0, -16} || state.score.score_delta != 0U ||
		presentation.sounds != std::vector<std::uint8_t>{12U}) return false;
	state.midboss.pos.cur.y = 0;
	state.midboss.pos.velocity = {};
	state.stage_frame = 0U;
	if(!stage2_fights_update(state, presentation)) return false;
	return state.midboss.phase == 3U && state.midboss.hp == 0 &&
		// m2_updt.cpp emits the 48-spark leave circle, then the 16-spark
		// top-edge circle in the next update even after phase advances. The
		// source ring cursor is a byte offset over 16-byte Spark records.
		state.spark_ring_offset == 1024U;
}

[[nodiscard]] bool kurumi_phase_matrix()
{
	{
		auto state = source_state();
		Trace presentation{};
		stage2_fights_setup(state);
		state.boss_active = true;
		state.boss.phase = 2U;
		state.boss.mode = 4U;
		state.boss.phase_frame = 64;
		state.boss.hp = 3300;
		state.boss.phase_end_hp = 2050;
		if(!stage2_fights_update(state, presentation) || spawnrays_growing(state) != 2U ||
			state.boss.phase_frame != 65 ||
			presentation.sounds != std::vector<std::uint8_t>{5U, 5U}) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage2_fights_setup(state);
		state.boss_active = true;
		state.boss.phase = 4U;
		state.boss.mode = 1U;
		state.boss.phase_frame = 64;
		state.boss.hp = 550;
		state.boss.phase_end_hp = 0;
		if(!stage2_fights_update(state, presentation) || spawnrays_growing(state) != 1U ||
			state.boss.kurumi_spawnrays[0].target != Point{2880, 1136} ||
			state.boss.phase_frame != 65 ||
			presentation.sounds != std::vector<std::uint8_t>{5U}) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage2_fights_setup(state);
		state.boss_active = true;
		state.boss.phase = 4U;
		state.boss.mode = 0U;
		state.boss.phase_frame = 96;
		state.boss.hp = 550;
		state.boss.phase_end_hp = 0;
		if(!stage2_fights_update(state, presentation) || state.boss.phase_state != 1U ||
			state.boss.mode == 0U || state.boss.mode > 3U ||
			state.boss.phase_frame != 1 || bullets_alive(state) != 22U) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		stage2_fights_setup(state);
		state.boss_active = true;
		state.boss.phase = 5U;
		state.boss.mode = 0U;
		state.boss.phase_frame = 65;
		state.boss.hp = 550;
		state.boss.phase_end_hp = 0;
		if(!stage2_fights_update(state, presentation) || state.boss.mode != 1U ||
			state.boss.sprite != 12U || state.boss.angle != 128U ||
			state.boss.phase_frame != 1) return false;
	}
	return true;
}

[[nodiscard]] bool kurumi_defeat()
{
	auto state = source_state();
	Trace presentation{};
	stage2_fights_setup(state);
	state.boss_active = true;
	state.boss.phase = 6U;
	state.boss.phase_frame = 31;
	state.boss.phase_state = 1U;
	state.boss.pos.cur = {3072, 1280};
	state.pointnum_times_2 = true;
	if(!stage2_fights_update(state, presentation)) return false;
	return state.boss.phase == 254U && state.boss.phase_frame == 0 &&
		state.bullets.zap.active() && state.score.score_delta == 25600U &&
		presentation.pointnums.size() == 20U && !state.pointnum_times_2 &&
		state.player_invincibility_time == 255U && state.palette[0].r == 0U &&
		state.palette_changed;
}

} // namespace

int main()
{
	const std::array checks{
		setup_and_midboss_entrance(), midboss_survival_exit_and_top_spark(),
		kurumi_phase_matrix(), kurumi_defeat(),
	};
	for(std::size_t index = 0U; index < checks.size(); ++index) {
		if(!checks[index]) {
			std::cerr << "TH04 Stage 2 boss check failed: " << index << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
