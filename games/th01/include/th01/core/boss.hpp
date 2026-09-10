#pragma once

#include "th01/core/boss_elis.hpp"
#include "th01/core/boss_kikuri.hpp"
#include "th01/core/boss_konngara.hpp"
#include "th01/core/boss_mima.hpp"
#include "th01/core/boss_sariel.hpp"
#include "th01/core/boss_yuugenmagan.hpp"

#include <cstdint>

namespace th01::core {

struct State;

// th01/main/boss/boss.hpp::boss_id_t.  The stage dispatcher owns the source
// selection point; it must store this value rather than re-derive a boss from
// a stage number after a route branch has already happened.
enum class BossId : std::uint8_t {
	none,
	singyoku,
	yuugenmagan,
	mima,
	kikuri,
	elis,
	sariel,
	konngara,
};

// A headless replacement for a source sequence that blocks for presentation
// or modal input.  No result represents an invented continuation.
enum class BossUpdateResult : std::uint8_t {
	running,
	route_selection,
	stage_cleared,
	unsupported,
};

// th01/main/boss/defeat.cpp and rroute.hpp: SinGyoku is the only boss that
// exposes this modal route branch.  Palette colors 1 and 3 label this UI, but
// the stored route_t values are the contiguous 0 and 1.
enum class BossRoute : std::int8_t {
	makai = 0,
	jigoku = 1,
};

// The common words are source globals reused by one boss owner at a time.
// Owner-specific source locals remain in their named nested state instead of
// being compressed into a generic pattern machine.
struct SingyokuState {
	std::int16_t left{};
	std::int16_t top{};
	std::int16_t invincibility_frame{};
	std::int16_t pattern_cur{};
	std::int16_t pattern_value{};
	std::int16_t slam_velocity_x{};
	std::int16_t slam_velocity_y{};
	std::uint8_t sphere_image{};
	std::uint8_t halfcircle_angle{};
	std::int8_t halfcircle_direction{};
	bool invincible{};
	bool initial_hp_rendered{};
};

struct BossState {
	BossId id{BossId::none};
	std::int16_t hp{};
	std::int16_t phase_frame{};
	std::int8_t phase{};
	bool active{};
	SingyokuState singyoku{};
	YuugenmaganState yuugenmagan{};
	MimaState mima{};
	KikuriState kikuri{};
	ElisState elis{};
	SarielState sariel{};
	KonngaraState konngara{};
};

// th01/main/boss/hit.cpp::boss_hit_update_and_render(), with its palette,
// sound, HUD, and callback-only work left to a future presentation owner.
// The return reports the source collision/bomb branch after it has mutated
// shared gameplay state; an owner can immediately perform its own source
// gameplay-only hit work if it has any.
[[nodiscard]] bool boss_hit_update(
	State &,
	std::int16_t &invincibility_frame,
	bool &is_invincible,
	bool colliding_with_orb,
	std::int16_t shot_hitbox_left,
	std::int16_t shot_hitbox_top,
	std::int16_t shot_hitbox_width_minus_shot,
	std::int16_t shot_hitbox_height_minus_shot,
	std::int16_t hit_score
) noexcept;

// th01/main_01.cpp's boss_id switch, retained as an explicit dispatcher.  The
// caller installs BossState::id at the source stage/route branch before setup.
void boss_setup(State &) noexcept;
[[nodiscard]] BossUpdateResult boss_update(State &) noexcept;
void boss_render_tail(State &) noexcept;

// th01/main/boss/b05.cpp.  These remain public while the Stage 5 driver and
// focused source-boundary tests are being built; later boss owners keep their
// own direct names rather than entering a generic fight abstraction.
void singyoku_setup(State &) noexcept;
[[nodiscard]] BossUpdateResult singyoku_update(State &) noexcept;
[[nodiscard]] BossUpdateResult singyoku_route_select(
	State &, BossRoute
) noexcept;

} // namespace th01::core
