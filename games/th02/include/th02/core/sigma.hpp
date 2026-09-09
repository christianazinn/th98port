#pragma once

#include "th02/core/laser.hpp"
#include "th02/core/stage_scenery.hpp"
#include "th02/core/stage_state_fwd.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::core {

inline constexpr std::size_t sigma_blast_count = 16U;

enum class SigmaBlastPhase : std::uint8_t {
	free = 0U,
	telegraph = 1U,
	activating = 2U,
	expanding = 3U,
	collapsing = 4U,
};

// Fixed-width representation of b6.cpp's sigma_blast_t. The source uses an
// int radius_max, retained as a 16-bit PC-98 word here.
struct SigmaBlast {
	std::uint8_t phase{};
	std::uint8_t phase_frame{};
	ScreenPoint center{};
	std::int16_t radius_max{};
	std::uint8_t patnum{};
	std::uint8_t radius{};
	bool operator==(const SigmaBlast &) const = default;
};

// Private combat globals from th02/main/boss/b6.cpp. `frame` deliberately
// persists through sigma_combat_reset(), as sigma_init() does not clear it.
struct SigmaState {
	ScreenPoint topleft{};
	std::uint8_t phase{};
	std::uint8_t cel_interval_mask{};
	std::int16_t center_x{};
	std::int16_t center_y{};
	std::uint8_t pattern{};
	std::uint8_t pattern_looped_unused{};
	std::int16_t phase_damage_max{};
	std::uint8_t ring_radius{};
	std::int8_t sweep_velocity_x{};
	std::int8_t weave_velocity_x{};
	ScreenPoint stream_velocity{};
	std::int16_t stream_mirror_velocity_x{};
	std::int16_t stream_x{};
	std::int16_t stream_y{};
	std::int16_t stream_mirror_x{};
	std::int8_t blast_hitbox_margin{};
	std::uint8_t ball_ring_angle{};
	std::array<SigmaBlast, sigma_blast_count> blasts{};
	std::uint8_t beam_half_w{};
	std::uint8_t beam_vram_left{};
	std::uint8_t beam_vram_right{};
	std::int16_t beam_hitbox_left{};
	std::int16_t beam_hitbox_right{};
	std::int8_t dash_velocity_x{};
	std::uint8_t spray_half_angle{};
	std::int16_t orbit_center_x{};
	std::int16_t orbit_center_y{};
	std::uint8_t orbit_angle{};
	std::int16_t billiard_left{};
	std::uint8_t billiard_volley{};
	std::int8_t laser_i{};
	std::uint32_t frame{};
	bool operator==(const SigmaState &) const = default;
};

struct SigmaSprite {
	ScreenPoint topleft{};
	std::int16_t patnum{};
	bool operator==(const SigmaSprite &) const = default;
};

struct SigmaBlastSprite {
	ScreenPoint topleft{};
	std::uint8_t patnum{};
	bool operator==(const SigmaBlastSprite &) const = default;
};

// All presentation calls retain their b6.cpp call positions. Gameplay state,
// including the blast pool, bullets, lasers, collision, and RNG, is concrete.
class SigmaCallbacks : public StageSceneryCallbacks, public LaserSpawnCallbacks {
public:
	void sound_effect(std::uint8_t sound_id) noexcept override = 0;
	virtual void grcg_circle(ScreenPoint center, std::int16_t radius) noexcept = 0;
	virtual void grcg_circle_fill(ScreenPoint center, std::int16_t radius) noexcept = 0;
	virtual void grcg_vline(std::int16_t x, std::int16_t top,
		std::int16_t bottom) noexcept = 0;
	virtual void grcg_bytebox_fill(std::uint8_t left, std::int16_t top,
		std::uint8_t right, std::int16_t bottom) noexcept = 0;
	virtual void dot_square_ring(ScreenPoint center, std::uint8_t radius,
		std::uint8_t angle_step) noexcept = 0;
	virtual void render_sigma(SigmaSprite sprite) noexcept = 0;
	virtual void render_sigma_blast(SigmaBlastSprite sprite) noexcept = 0;
};

enum class SigmaUpdateResult : std::uint8_t {
	boss,
	stage_clear,
	invalid_page,
	invalid_shot_hittest,
	shot_spark_allocation_refused,
	bullet_invalid_page,
	bullet_source_iatan2_fault,
	source_iatan2_fault,
};

// Combat-state portion of b6.cpp::sigma_init. It intentionally omits the
// source's dialogue, assets, bullet/shot clears, BGM, and fade orchestration.
void sigma_combat_reset(StageState &state) noexcept;

// b6.cpp::sigma_bg_render. It must run before sigma_update for the one-frame
// activating-blast handshake and the source page carry/clear order.
[[nodiscard]] SigmaUpdateResult sigma_bg_render(StageState &state,
	SigmaCallbacks &callbacks) noexcept;

// Complete b6.cpp combat update and defeat animation. A completed defeat
// returns stage_clear for the caller-owned Extra-stage ending scheduler.
[[nodiscard]] SigmaUpdateResult sigma_update(StageState &state,
	SigmaCallbacks &callbacks) noexcept;

} // namespace th02::core
