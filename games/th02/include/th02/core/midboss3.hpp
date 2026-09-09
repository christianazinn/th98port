#pragma once
#include "th02/core/midboss2.hpp"
#include <span>

namespace th02::core {
struct Midboss3State {
	std::array<std::array<ScreenPoint, 2>, 5> topleft_on_page{};
	std::array<std::int16_t, 2> kill_frame{};
	std::array<std::uint8_t, 2> angle{};
	std::uint8_t loops{};
};
// Aliases of the five Stones slots, not a second owned copy.
struct Midboss3SharedState {
	std::span<std::uint8_t, 5> flag;
	std::span<std::int16_t, 5> damage;
	std::span<std::int16_t, 5> patnum;
};
struct Midboss3Context {
	PlayerFrameState &frame;
	PlayerShotPoolState &shots;
	ScoreState &score;
	SparkPoolState &sparks;
	EnemyBulletPoolState &bullets;
	RandomState &random;
	std::int16_t playperf{};
	std::int8_t rank{};
	bool bombing{};
	std::uint8_t shot_level{}, bomb_damage_frame_mask{};
	std::uint32_t stage_frame{};
	std::int16_t scroll_line{}, scroll_delta{};
};
// Existing midboss presentation records carry the required sprite/rectangle.
[[nodiscard]] Midboss2InvalidateResult midboss3_invalidate(Midboss3State &state,
	Midboss3SharedState shared, const PlayerFrameState &frame, Midboss2Callbacks &presentation) noexcept;
[[nodiscard]] Midboss2UpdateResult midboss3_update_and_render(BossState &boss,
	Midboss3State &state, Midboss3SharedState shared, Midboss3Context &context,
	Midboss2Callbacks &presentation) noexcept;
} // namespace th02::core
