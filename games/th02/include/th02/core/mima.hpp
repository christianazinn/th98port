#pragma once

#include "th02/core/bullet_lifecycle.hpp"
#include "th02/core/player.hpp"
#include "th02/core/stage_scenery.hpp"
#include "th02/core/stage_state_fwd.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

inline constexpr std::size_t mima_orb_slot_count = 8U;
inline constexpr std::size_t mima_orb_count = 4U;

// Private combat globals from th02/main/boss/b5m.cpp. Their widths mirror the
// source globals; the two page position arrays intentionally retain all eight
// slots even though every formation writer uses only the lower four.
struct MimaState {
	std::array<std::array<ScreenPoint, mima_orb_slot_count>, 2U> orb_on_page{};
	std::array<std::int16_t, mima_orb_slot_count> orb_flag{};
	std::int16_t orbs_gone_unused{};
	std::int16_t damage_multiplier{};
	std::int16_t phase{};
	std::int16_t pattern{};
	std::int16_t patterns_this_phase{};
	bool hit_flash{};
	bool all_patterns{};
	std::uint8_t bg_ring_radius{};
	std::uint8_t bg_circle_radius{};
	std::uint8_t bg_ring_col_head{};
	std::uint8_t bg_ring_col_tail{};
	std::uint8_t bg_circle_col{};
	std::int16_t phase_damage_max{};
	std::int16_t patterns_max{};
	std::int16_t pattern_count{};
	std::int16_t patterns_until_vulnerable{};
	std::uint8_t ring_radius{};
	std::uint8_t bg_circle_radius_base{};
	std::uint8_t bg_circle_pulse_frame{};
	std::int16_t velocity_y{};
	std::uint8_t spiral_angle{};
	std::uint8_t aim_angle_unused{};
	std::uint8_t charge_ring_radius{};
	std::uint8_t bg_ring_phase{};
	std::uint8_t flash_frame{};
	std::int16_t left_26c56{};
	std::int16_t muzzle_left{};
	std::int16_t left_26c5a{};
	std::int16_t x_26c5c{};
	std::int16_t top_26c5e{};
	std::int16_t muzzle_top{};
	std::int16_t top_26c62{};
	std::int16_t y_26c64{};
	std::uint8_t ray_unused{};
	std::uint8_t ray_angle{};
	std::uint8_t ray_tone{};
	bool orb_variant{};
	std::int16_t orb_flight_center_x{};
	std::int16_t orb_flight_center_y{};
	std::int16_t orb_flight_radius{};
	std::int8_t orb_flight_radius_step{};
	std::uint8_t orb_flight_angle{};
	bool orb_flight_detonated{};
	ScreenPoint orb_velocity_a{};
	ScreenPoint orb_velocity_b{};
	std::uint8_t stream_angle{};
	std::uint8_t stream_speed{};
	std::uint8_t pair_angle{};
	std::int16_t orb_center_x{};
	std::int16_t orb_center_y{};
	std::int16_t orb_radius{};
	std::uint8_t orb_angle{};
	std::uint8_t star_angle{};
	std::int8_t star_direction{};
	std::uint8_t fan_angle{};
	bool form1_defeat_pending{};
	bool final_end_pending{};
	bool operator==(const MimaState &) const = default;
};

enum class MimaSpriteKind : std::uint8_t { normal, hit_flash };

struct MimaSprite {
	MimaSpriteKind kind{};
	ScreenPoint topleft{};
	std::int16_t patnum{};
	bool operator==(const MimaSprite &) const = default;
};

struct MimaOrbSprite {
	std::size_t orb{};
	ScreenPoint topleft{};
	std::int16_t patnum{};
	bool operator==(const MimaOrbSprite &) const = default;
};

// Mima's boss background directly uses circles, lines, and dot-square rings;
// its sprite and tile requests stay synchronous at their source positions.
// The lifecycle uses the EnemyBulletInvalidationCallbacks base for the actual
// bullets_clear() prefix of mima_19C8D().
class MimaCallbacks : public StageSceneryCallbacks,
	public EnemyBulletInvalidationCallbacks {
public:
	virtual void grcg_line(ScreenPoint from, ScreenPoint to) noexcept = 0;
	virtual void grcg_circle(ScreenPoint center, std::int16_t radius) noexcept = 0;
	virtual void grcg_circle_fill(ScreenPoint center, std::int16_t radius) noexcept = 0;
	virtual void dot_square_ring(ScreenPoint center, std::uint8_t radius,
		std::uint8_t angle_step) noexcept = 0;
	virtual void render_mima(MimaSprite sprite) noexcept = 0;
	virtual void render_mima_orb(MimaOrbSprite sprite) noexcept = 0;
};

enum class MimaUpdateResult : std::uint8_t {
	boss,
	form1_defeat_pending,
	final_end_pending,
	invalid_page,
	invalid_shot_hittest,
	shot_spark_allocation_refused,
	bullet_invalid_page,
	bullet_source_iatan2_fault,
	source_iatan2_fault,
};

// Combat-tail reset of b5.cpp::mima_init. The caller owns the preceding
// dialogue/assets/pages and the following side-column rendering.
void mima_combat_reset(StageState &state) noexcept;

// b5m.cpp::mima_bg_render. It is a separate pre-combat source pass: orb
// invalidation, page carry, playfield clear, then the six ring draws.
[[nodiscard]] MimaUpdateResult mima_bg_render(StageState &state,
	MimaCallbacks &callbacks) noexcept;

// Complete b5m.cpp combat update. At a first-form or final defeat it returns
// a typed lifecycle boundary after the source update suffix, rather than
// simulating b5.cpp's dialogue/resident executable handoff.
[[nodiscard]] MimaUpdateResult mima_update(StageState &state,
	MimaCallbacks &callbacks) noexcept;

// The caller invokes this only after b5.cpp::mima_19C8D() has taken its
// non-ending branch. It is exactly that call's same-frame mima_update suffix.
void mima_after_form1_defeat(StageState &state, MimaCallbacks &callbacks) noexcept;

} // namespace th02::core
