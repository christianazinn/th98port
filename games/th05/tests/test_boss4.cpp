#include "th05/core/boss.hpp"
#include "th05/core/bullet.hpp"
#include "th05/core/laser.hpp"
#include "th05/core/random.hpp"
#include "th05/core/stage.hpp"
#include "th05/core/state.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
using namespace th05::core;
struct Trace final : Presentation {
	std::vector<std::uint8_t> sounds{};
	std::int16_t hp{}, total{};
	void sound(std::uint8_t id) noexcept override { sounds.push_back(id); }
	void boss_hp(std::int16_t value, std::int16_t max) noexcept override { hp=value; total=max; }
};
[[nodiscard]] std::size_t live_bullet_count(const State &state) noexcept
{
	std::size_t count{};
	for(const auto &bullet : state.bullets.pool.pellets) {
		count += bullet.flag != 0U;
	}
	for(const auto &bullet : state.bullets.pool.bullets16) {
		count += bullet.flag != 0U;
	}
	return count;
}
[[nodiscard]] State source_state() noexcept
{
	State state{};
	if(!gameplay_init(state,1U,0U,3U,3U,3U))std::abort();
	state.random.seed=318U; randring_fill(state.random);
	state.player_pos.cur=state.player_pos.prev={3072,5000};
	return state;
}
[[nodiscard]] bool midboss_and_pair_entrances() noexcept
{
	auto state=source_state(); Trace trace{}; stage4_fights_setup(state);
	if(state.midboss.frames_until!=3900U||state.midboss.hp!=1100||state.midboss.pos.cur!=Point{3072,-512}||state.boss.pos.cur!=Point{1536,1024}||state.stage4.yuki.pos.cur!=Point{4608,1024})return false;
	state.stage_frame=3900U;stage4_midboss_activate(state);
	for(std::uint16_t i{};i<192U;++i)if(!stage4_fights_update(state,trace))return false;
	if(state.midboss.phase!=1U||state.midboss.hp!=1100||state.boss.statebytes[14U]!=1U)return false;
	// Positive HP in M4's first live frame is an outcome-false hittest, not an
	// unsupported update (the Stage 3 regression's paired analogue).
	if(!stage4_fights_update(state,trace)||state.midboss.phase!=1U||state.midboss.hp!=1100)return false;
	state.midboss.damage_this_frame=1U;state.midboss.pos.cur.y=0;stage4_fights_render_tail(state,trace);
	if(state.midboss.damage_this_frame!=0U)return false;

	state=source_state();trace={};stage4_fights_setup(state);stage4_boss_activate(state);
	for(std::uint16_t i{};i<192U;++i)if(!stage4_fights_update(state,trace))return false;
	if(state.boss.phase!=2U||state.boss.mode!=1U||state.boss.hp!=4500||state.stage4.yuki.hp!=4500||trace.total!=9000)return false;
	// Frame 48 is the first live pair pattern frame: Mai's blue cross stack
	// and Yuki's exact-linear red spread both run after the shared gather.
	for(std::uint8_t i{};i<48U;++i)if(!stage4_fights_update(state,trace))return false;
	const auto &bullet=state.bullets.bullet_template;
	if(live_bullet_count(state)<9U||bullet.spawn_type!=bullet_spawn_type_no_decelerate||
		bullet.group!=static_cast<std::uint8_t>(BulletGroup::spread_aimed)||
		bullet.special_motion!=static_cast<std::uint8_t>(BulletSpecialMotion::exact_linear)||
		bullet.patnum!=50U||bullet.origin!=Point{
			state.stage4.yuki.pos.cur.x,
			static_cast<std::int16_t>(state.stage4.yuki.pos.cur.y - 128)
		})return false;
	state.boss.damage_this_frame=1U;state.stage4.yuki.damage_this_frame=1U;stage4_fights_render_tail(state,trace);
	return state.boss.damage_this_frame==0U&&state.stage4.yuki.damage_this_frame==0U;
}
[[nodiscard]] bool solo_and_renderer_contract() noexcept
{
	auto state=source_state();Trace trace{};stage4_fights_setup(state);stage4_boss_activate(state);
	state.stage4.fight=Stage4Fight::yuki_solo;state.boss.phase=0;state.boss.phase_frame=0;
	if(!stage4_fights_update(state,trace)||state.boss.hp!=7900||state.boss.phase_end_hp!=4600||state.boss.sprite!=196)return false;
	state.boss.damage_this_frame=1U;state.stage4.custom.b4balls[0].flag=1U;state.stage4.custom.b4balls[0].damage_this_frame=1;
	stage4_fights_render_tail(state,trace);
	if(state.boss.damage_this_frame!=1U||state.stage4.custom.b4balls[0].damage_this_frame!=0)return false;

	// Mai's phase-5 fan is manual from its spawn frame. Unlike the phase-9
	// fan below, neither age is scheduled; its own 256-frame cycle grows it.
	state=source_state();trace={};stage4_fights_setup(state);stage4_boss_activate(state);
	state.stage4.fight=Stage4Fight::mai_solo;
	state.boss.phase=5U;state.boss.phase_frame=31;state.boss.hp=5000;state.boss.phase_end_hp=2800;
	const auto fan_count=static_cast<std::uint8_t>(state.rank+5U);
	if(!stage4_fights_update(state,trace)||state.stage4.mai_laser_count!=fan_count)return false;
	for(std::uint8_t slot{};slot<fan_count;++slot){
		const auto &laser=state.lasers.pool[slot];
		if(laser.flag!=LaserFlag::fixed_wait_to_grow||laser.active_at_age!=-1||laser.shrink_at_age!=-1)return false;
	}

	// Phase 9 retains the phase-5 laser template. Its source-fixed four-slot
	// fan is therefore a useful test of persistent renderer-side fight state.
	state=source_state();trace={};stage4_fights_setup(state);stage4_boss_activate(state);
	state.stage4.fight=Stage4Fight::mai_solo;
	state.stage4.solo_pattern=Stage4SoloPattern::mai_fixed_lasers;
	state.boss.phase=9U;state.boss.mode=1U;state.boss.phase_frame=15;
	state.lasers.laser_template.color=8U;
	state.lasers.laser_template.coords.width=8U;
	if(!stage4_fights_update(state,trace))return false;
	for(std::uint8_t slot{};slot<4U;++slot){
		const auto &laser=state.lasers.pool[slot];
		if(laser.flag!=LaserFlag::fixed_wait_to_grow||laser.active_at_age!=32||laser.shrink_at_age!=90)return false;
	}
	return true;
}

[[nodiscard]] bool pair_handoff_selects_both_survivors() noexcept
{
	for(const auto defeated_yuki : {false, true}) {
		auto state=source_state();
		Trace trace{};
		stage4_fights_setup(state);
		stage4_boss_activate(state);
		state.boss.phase=255U;
		state.stage4.pair_defeated_yuki=defeated_yuki;
		if(!stage4_fights_update(state,trace) || !state.dialog_pending ||
			state.boss.phase!=0U) return false;
		if(!stage_dialog_complete(state)) return false;
		if(!stage4_fights_update(state,trace)) return false;
		const auto expected=defeated_yuki ? Stage4Fight::mai_solo : Stage4Fight::yuki_solo;
		const auto hp=defeated_yuki ? 7800 : 7900;
		if(state.stage4.fight!=expected || state.boss.hp!=hp || state.boss.phase!=0U) return false;
	}
	return true;
}
}
int main()
{
	if(!midboss_and_pair_entrances()){std::cerr<<"TH05 Stage 4 midboss/pair test failure\n";return EXIT_FAILURE;}
	if(!solo_and_renderer_contract()){std::cerr<<"TH05 Stage 4 solo test failure\n";return EXIT_FAILURE;}
	if(!pair_handoff_selects_both_survivors()){std::cerr<<"TH05 Stage 4 handoff test failure\n";return EXIT_FAILURE;}
	return EXIT_SUCCESS;
}
