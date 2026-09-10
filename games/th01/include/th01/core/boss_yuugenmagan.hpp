#pragma once

#include <array>
#include <cstdint>

namespace th01::core {

struct State;
enum class BossUpdateResult : std::uint8_t;

// b10m.cpp keeps five independent 64x48 eye entities.  Their image and
// hitbox state is gameplay-visible: only visible, enabled eyes can hurt the
// Orb through boss_hit_update().
struct YuugenmaganEyeState {
	std::int16_t left{};
	std::int16_t top{};
	std::uint8_t image{};
	bool orb_hitbox_active{};
};

// b10m.cpp's persistent regular pentagram.  The initial between-eyes and
// shrink geometry are derived on demand; only this attack form survives into
// the spinning/slamming source phases.
struct YuugenmaganPentagramState {
	std::array<std::int16_t, 5U> left{};
	std::array<std::int16_t, 5U> top{};
	std::int16_t center_left{};
	std::int16_t center_top{};
	std::int16_t radius{};
	std::int16_t velocity_y{};
	std::int16_t velocity_x{};
	std::uint8_t phase{};
	std::uint8_t angle{};
};

// Source-local b10m.cpp statics.  The common HP and phase words stay in
// BossState, while the overlapping source unions deliberately share their
// corresponding scalar words here.
struct YuugenmaganState {
	std::array<YuugenmaganEyeState, 5U> eyes{};
	YuugenmaganPentagramState pentagram{};
	std::int16_t invincibility_frame{};
	std::int16_t phase_u1{};
	std::int16_t phase_u2{};
	std::int16_t target_left{};
	std::int16_t after_hit_frame{};
	std::int16_t pattern_interval{};
	std::uint8_t phase_u3{};
	std::uint8_t missile_angle_southwest{};
	std::uint8_t missile_angle_southeast{};
	bool invincible{};
	bool initial_hp_rendered{};
};

void yuugenmagan_setup(State &) noexcept;
[[nodiscard]] BossUpdateResult yuugenmagan_update(State &) noexcept;
void yuugenmagan_render_tail(State &) noexcept;

} // namespace th01::core
