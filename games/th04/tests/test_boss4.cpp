#include "th04/core/boss.hpp"
#include "th04/core/midboss4.hpp"
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
	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
};

[[nodiscard]] State source_state(std::uint8_t playchar) noexcept
{
	State state{};
	if(!gameplay_init(state, 3U, playchar, 0U, 3U, 3U, false)) std::abort();
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

[[nodiscard]] std::size_t spinning_orbs(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &orb : state.boss.reimu_orbs) {
		if(orb.flag == ReimuOrbFlag::moveout_spin) ++count;
	}
	return count;
}

[[nodiscard]] std::size_t live_bits(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &bit : state.boss.marisa_bits) {
		if(bit.flag != MarisaBitFlag::free) ++count;
	}
	return count;
}

[[nodiscard]] bool setup_selects_source_fights()
{
	{
		auto state = source_state(1U);
		stage4_fights_setup(state);
		if(state.midboss.frames_until != 2800U ||
			state.midboss.pos.cur != Point{2304, -512} || state.midboss.hp != 1200 ||
			state.boss.pos.cur != Point{3072, 1024} || state.boss.sprite != 128U ||
			state.boss.hitbox_radius != Point{384, 384} ||
			state.boss.statebytes[0] != 12U || state.boss.statebytes[1] != 6U ||
			state.boss.statebytes[2] != 4U || state.boss.statebytes[6] != 10U) {
			return false;
		}
	}
	{
		auto state = source_state(0U);
		stage4_fights_setup(state);
		if(state.boss.hp != 6000 || state.boss.statebytes[0] != 0U ||
			state.boss.pos.cur != Point{3072, 1024}) return false;
	}
	return true;
}

[[nodiscard]] bool stage4_midboss_composes_with_fight_driver()
{
	auto state = source_state(1U);
	Trace presentation{};
	stage4_fights_setup(state);
	state.stage_frame = 2800U;
	stage4_midboss_activate(state);
	if(!state.midboss_active || state.midboss.phase != 0U) return false;
	for(std::uint8_t frame{}; frame < 48U; ++frame) {
		if(!stage4_fights_update(state, presentation)) return false;
	}
	return state.midboss.phase == 1U && state.midboss.phase_frame == 0 &&
		state.midboss.pos.cur == Point{5376, 1024};
}

[[nodiscard]] bool reimu_entry_patterns_and_orbs()
{
	auto state = source_state(1U);
	Trace presentation{};
	stage4_fights_setup(state);
	stage4_boss_activate(state);
	for(std::uint8_t frame{}; frame < 97U; ++frame) {
		if(!stage4_fights_update(state, presentation)) return false;
	}
	if(state.boss.phase != 1U || state.boss.phase_frame != 0 ||
		state.palette[0].r != 128U || state.palette[0].g != 0U ||
		state.palette[0].b != 224U ||
		presentation.sounds != std::vector<std::uint8_t>{13U}) return false;
	presentation.sounds.clear();
	for(std::uint8_t frame{}; frame < 64U; ++frame) {
		if(!stage4_fights_update(state, presentation)) return false;
	}
	if(state.boss.phase != 2U || state.boss.hp != 9100 ||
		state.boss.phase_end_hp != 7900 || state.boss.phase_frame != 0) return false;

	state.boss.phase = 2U;
	state.boss.mode = 0U;
	state.boss.phase_frame = 46;
	state.boss.hp = 9100;
	state.boss.phase_end_hp = 7900;
	if(!stage4_fights_update(state, presentation) || bullets_alive(state) != 0U ||
		state.boss.phase_frame != 47 || presentation.sounds != std::vector<std::uint8_t>{3U}) {
		return false;
	}
	if(!stage4_fights_update(state, presentation) || state.boss.phase_frame != 48 ||
		presentation.sounds != std::vector<std::uint8_t>{3U}) {
		return false;
	}
	if(!stage4_fights_update(state, presentation) || bullets_alive(state) != 10U ||
		state.boss.phase_frame != 49 || presentation.sounds != std::vector<std::uint8_t>{3U, 3U}) {
		return false;
	}

	state = source_state(1U);
	presentation = {};
	stage4_fights_setup(state);
	stage4_boss_activate(state);
	state.boss.phase = 4U;
	state.boss.mode = 0U;
	state.boss.phase_frame = 32;
	state.boss.hp = 6300;
	state.boss.phase_end_hp = 4500;
	state.boss.pos.cur = {3072, 1536};
	if(!stage4_fights_update(state, presentation) || spinning_orbs(state) != 12U ||
		state.boss.reimu_orbs[0].spin_time != 63U || state.boss.phase_frame != 33 ||
		presentation.sounds != std::vector<std::uint8_t>{8U}) return false;
	return true;
}

[[nodiscard]] bool marisa_bits_and_foreground_damage_clear()
{
	auto state = source_state(0U);
	Trace presentation{};
	stage4_fights_setup(state);
	stage4_boss_activate(state);
	state.boss.phase = 2U;
	state.boss.mode = 0U;
	state.boss.phase_frame = 64;
	state.boss.hp = 6000;
	state.boss.phase_end_hp = 0;
	state.boss.marisa_bit_angle_speed = 2U;
	if(!stage4_fights_update(state, presentation) || live_bits(state) != 4U ||
		state.boss.marisa_bits_alive != 4U || state.boss.marisa_bits[0].hp != 220 ||
		state.boss.marisa_bits[3].hp != 450 ||
		state.boss.marisa_bits[0].flag != MarisaBitFlag::moveout_spin) return false;

	state.boss.damage_this_frame = 1U;
	state.boss.marisa_bits[0].center = {-257, 0};
	state.boss.marisa_bits[0].damage_this_frame = 1U;
	state.boss.marisa_bits[1].center = {3072, 1536};
	state.boss.marisa_bits[1].damage_this_frame = 1U;
	stage4_fights_render_tail(state, presentation);
	return state.boss.damage_this_frame == 0U &&
		state.boss.marisa_bits[0].damage_this_frame == 1U &&
		state.boss.marisa_bits[1].damage_this_frame == 0U;
}

} // namespace

int main()
{
	const std::array checks{
		setup_selects_source_fights(), stage4_midboss_composes_with_fight_driver(),
		reimu_entry_patterns_and_orbs(), marisa_bits_and_foreground_damage_clear(),
	};
	for(std::size_t index{}; index < checks.size(); ++index) {
		if(!checks[index]) {
			std::cerr << "TH04 Stage 4 boss check failed: " << index << '\n';
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
