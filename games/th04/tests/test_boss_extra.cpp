#include "th04/core/boss.hpp"
#include "th04/core/stage.hpp"
#include "th04/core/state.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
using namespace th04::core;

struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
};

[[nodiscard]] State source_state() noexcept
{
	State state{};
	Trace initialization{};
	if(!gameplay_init(state, 4U, 0U, 0U, 3U, 3U, false)) std::abort();
	state.stage_id = 6U;
	stage_init(state, initialization);
	state.random.seed = 318U;
	state.player_pos.cur = {3000, 5000};
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

[[nodiscard]] bool mugetsu_entrance_and_dialog_transition()
{
	auto state = source_state();
	Trace presentation{};
	extra_fights_setup(state);
	if(state.boss_active || state.midboss.frames_until != 5400U ||
		state.boss.pos.cur != Point{3072, 1280} || state.boss.sprite != 128U ||
		state.boss.hitbox_radius != Point{384, 768} || state.boss.statebytes[0U] != 0U) {
		return false;
	}
	extra_boss_activate(state);
	for(unsigned frame{}; frame < 129U; ++frame) {
		if(!extra_fights_update(state, presentation)) return false;
	}
	if(state.boss.phase != 1U || state.boss.phase_frame != 0 ||
		presentation.sounds != std::vector<std::uint8_t>{13U}) {
		return false;
	}
	presentation.sounds.clear();
	for(unsigned frame{}; frame < 64U; ++frame) {
		if(!extra_fights_update(state, presentation)) return false;
	}
	if(state.boss.phase != 2U || state.boss.phase_frame != 0 ||
		state.boss.hp != 9400 || state.boss.phase_end_hp != 3700) {
		return false;
	}

	state.boss.phase = 255U;
	state.boss.phase_frame = 0;
	state.boss.statebytes[0U] = 0U;
	state.dialog_pending = false;
	state.stage_clear_requested = false;
	if(!extra_fights_update(state, presentation) || !state.dialog_pending ||
		state.stage_clear_requested) {
		return false;
	}
	state.dialog_pending = false;
	extra_boss_activate(state);
	return state.boss_active && !state.dialog_pending && !state.stage_clear_requested &&
		state.boss.statebytes[0U] == 1U && state.boss.phase == 0U &&
		state.boss.pos.cur == Point{3072, 1792} && state.boss.sprite == 128U &&
		state.boss.hitbox_radius == Point{384, 768};
}

[[nodiscard]] bool extra_defeat_update_preserves_transition_boundary()
{
	auto state = source_state();
	Trace presentation{};
	extra_fights_setup(state);
	extra_boss_activate(state);
	state.boss.phase = 254U;
	state.boss.phase_frame = 0;
	state.boss.sprite = 4U;
	state.stage_frame = 0U;
	if(!extra_fights_update(state, presentation) || state.boss.phase_frame != 1 ||
		state.playfield_shake_x != -4 || state.playfield_shake_y != -4 ||
		state.bullets.slowdown_factor != 2U) return false;
	state.boss.phase_frame = 7;
	state.boss.sprite = 11U;
	if(!extra_fights_update(state, presentation) || state.boss.phase != 255U ||
		state.boss.phase_frame != 0 || !state.bombing_disabled) return false;
	if(!extra_fights_update(state, presentation) || !state.dialog_pending ||
		state.stage_clear_requested || state.palette_tone != 60U || !state.palette_changed) {
		return false;
	}
	return true;
}

[[nodiscard]] bool gengetsu_entrance_columns_and_final_clear()
{
	auto state = source_state();
	Trace presentation{};
	extra_fights_setup(state);
	extra_boss_activate(state);
	state.boss.phase = 255U;
	state.boss.statebytes[0U] = 0U;
	extra_boss_activate(state);
	for(unsigned frame{}; frame < 129U; ++frame) {
		if(!extra_fights_update(state, presentation)) return false;
	}
	if(state.boss.phase != 1U || state.boss.phase_frame != 0 || state.boss.hp != 18700) {
		return false;
	}
	for(unsigned frame{}; frame < 64U; ++frame) {
		if(!extra_fights_update(state, presentation)) return false;
	}
	if(state.boss.phase != 2U || state.boss.phase_end_hp != 14700) return false;

	state.boss.phase = 5U;
	state.boss.mode = 1U;
	state.boss.phase_frame = 1;
	state.stage_frame = 9U;
	if(!extra_fights_update(state, presentation)) return false;
	for(std::size_t i{}; i < state.boss.gengetsu_spawncolumns.size(); ++i) {
		if(state.boss.gengetsu_spawncolumns[i].y != 0) return false;
	}
	state.boss.phase_frame = 80;
	if(!extra_fights_update(state, presentation) ||
		state.thicklasers.pool[0].flag != ThickLaserFlag::line ||
		state.thicklasers.pool[0].radius_max != 64 ||
		state.thicklasers.pool[0].line_frames != 32 ||
		state.thicklasers.pool[0].static_frames != 48) return false;

	state.boss.phase = 255U;
	state.stage_clear_requested = false;
	if(!extra_fights_update(state, presentation)) return false;
	return state.stage_clear_requested && state.palette_tone == 60U && state.palette_changed;
}

[[nodiscard]] bool timeout_and_rage_paths()
{
	{
		auto state = source_state();
		Trace presentation{};
		extra_fights_setup(state);
		extra_boss_activate(state);
		state.boss.phase = 6U;
		state.boss.phase_frame = 3999;
		state.boss.hp = 1;
		state.boss.phase_end_hp = 0;
		state.stage_frame = 1U;
		if(!extra_fights_update(state, presentation) || state.boss.phase != 7U ||
			state.boss.phase_state != 0U || state.boss.phase_frame != 0) return false;
	}
	{
		auto state = source_state();
		Trace presentation{};
		extra_fights_setup(state);
		extra_boss_activate(state);
		state.boss.phase = 255U;
		extra_boss_activate(state);
		state.boss.phase = 7U;
		state.boss.phase_frame = 1500;
		state.boss.hp = 1;
		state.boss.phase_end_hp = 0;
		state.stage_frame = 1U;
		if(!extra_fights_update(state, presentation) || state.boss.phase != 8U ||
			state.boss.phase_end_hp != 0 || state.boss.mode != 255U) return false;

		state.boss.phase = 8U;
		state.boss.phase_frame = 3001;
		state.boss.hp = 1;
		state.boss.phase_end_hp = 0;
		state.stage_frame = 4U;
		if(!extra_fights_update(state, presentation) || bullets_alive(state) == 0U) return false;

		state.boss.phase_frame = 4999;
		state.stage_frame = 1U;
		if(!extra_fights_update(state, presentation) || state.boss.phase != 9U ||
			state.boss.phase_state != 0U || state.boss.phase_frame != 0) return false;
	}
	return true;
}

[[nodiscard]] bool extra_bonus_uses_16bit_product()
{
	auto state = source_state();
	Trace presentation{};
	extra_fights_setup(state);
	extra_boss_activate(state);
	state.boss.phase = 255U;
	extra_boss_activate(state);
	state.boss.phase = 7U;
	state.boss.phase_frame = 0;
	state.boss.phase_state = 0U;
	state.boss.hp = 1;
	state.boss.phase_end_hp = 0;
	state.stage_frame = 1U;
	state.shots.shots[0].flag = shot_alive;
	state.shots.shots[0].pos.cur = state.boss.pos.cur;
	state.shots.shots[0].damage = 1;
	state.shots.shots_alive[0] = {state.boss.pos.cur, 0U};
	state.shots.shots_alive_count = 1U;
	if(!extra_fights_update(state, presentation)) return false;
	if(state.score.score_delta != 62465U || state.boss.phase != 8U ||
		state.boss.phase_frame != 0 ||
		presentation.sounds != std::vector<std::uint8_t>{4U, 15U}) return false;

	auto final_state = source_state();
	Trace final_presentation{};
	extra_fights_setup(final_state);
	extra_boss_activate(final_state);
	final_state.boss.phase = 255U;
	extra_boss_activate(final_state);
	final_state.boss.phase = 8U;
	final_state.boss.phase_frame = 0;
	final_state.boss.hp = 1;
	final_state.boss.phase_end_hp = 0;
	final_state.stage_frame = 1U;
	final_state.shots.shots[0].flag = shot_alive;
	final_state.shots.shots[0].pos.cur = final_state.boss.pos.cur;
	final_state.shots.shots[0].damage = 1;
	final_state.shots.shots_alive[0] = {final_state.boss.pos.cur, 0U};
	final_state.shots.shots_alive_count = 1U;
	if(!extra_fights_update(final_state, final_presentation) ||
		final_state.boss.phase != 9U || final_state.boss.phase_state != 1U) {
		return false;
	}
	for(unsigned frame{}; frame < 32U; ++frame) {
		if(!extra_fights_update(final_state, final_presentation)) return false;
	}
	return final_state.score.score_delta == 59393U &&
		final_state.boss.phase == 254U && final_state.boss.phase_frame == 0 &&
		final_state.bullets.zap.active();
}

[[nodiscard]] bool extra_foreground_mutations()
{
	auto state = source_state();
	extra_fights_setup(state);
	extra_boss_activate(state);
	state.boss.phase = 2U;
	state.boss.damage_this_frame = 1U;
	extra_fights_render_tail(state);
	if(state.boss.damage_this_frame != 0U || state.boss.mugetsu_damage_frames != 1U) {
		return false;
	}
	state.boss.statebytes[0U] = 1U;
	state.boss.gengetsu_wave_amplitude = 2U;
	state.boss.damage_this_frame = 1U;
	extra_fights_render_tail(state);
	if(state.boss.damage_this_frame != 1U || state.boss.gengetsu_damage_frames != 0U) {
		return false;
	}
	state.boss.gengetsu_wave_amplitude = 0U;
	extra_fights_render_tail(state);
	if(state.boss.damage_this_frame != 0U || state.boss.gengetsu_damage_frames != 1U) {
		return false;
	}
	state.boss_explosions.small[0].alive = true;
	state.boss_explosions.small[0].age = 31U;
	extra_fights_render_tail(state);
	return !state.boss_explosions.small[0].alive;
}

} // namespace

int main()
{
	if(!mugetsu_entrance_and_dialog_transition()) return 1;
	if(!extra_defeat_update_preserves_transition_boundary()) return 2;
	if(!gengetsu_entrance_columns_and_final_clear()) return 3;
	if(!timeout_and_rage_paths()) return 4;
	if(!extra_bonus_uses_16bit_product()) return 5;
	if(!extra_foreground_mutations()) return 6;
	std::cout << "th04 extra boss tests passed\n";
	return 0;
}
