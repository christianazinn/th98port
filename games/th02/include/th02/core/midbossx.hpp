#pragma once
#include "th02/core/stage_state_fwd.hpp"
#include "th02/core/bullet.hpp"
#include <array>
namespace th02::core {
struct MidbossxBurst {
	std::uint8_t cel{}, frame{};
	std::int16_t left{}, vram_y{};
	std::uint8_t group{}, angle{}, speed{};
	bool operator==(const MidbossxBurst &) const = default;
};
struct MidbossxState {
	std::array<MidbossxBurst, 30> bursts{};
	std::uint8_t cross_distance{};
	bool active{};
};
class MidbossxCallbacks {
public:
	virtual ~MidbossxCallbacks() = default;
	virtual void tile_ring_set_and_put_both_8(std::int16_t left,
		std::int16_t vram_y, std::int16_t image) noexcept = 0;
};
// This midboss is a timed scenery event, not a damageable actor.
[[nodiscard]] EnemyBulletAddResult midbossx_update_and_render(StageState &state,
	MidbossxCallbacks &presentation) noexcept;
} // namespace th02::core
