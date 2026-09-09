#pragma once

#include "th02/core/player.hpp"
#include "th02/core/score.hpp"
#include "th02/core/shot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th02::core {

inline constexpr std::size_t enemy_page_count = 2U;
inline constexpr std::int16_t enemy_hittest_width = 32;
inline constexpr std::int16_t enemy_hittest_height = 32;

// Raw source entity flags remain bytes. enemy_hittest() deliberately does not
// inspect this field; the source caller filters alive/removing records before
// invoking the collision body.
struct Enemy {
	std::array<ScreenPoint, enemy_page_count> pos_on_page{};
	std::int16_t script_ip{};
	std::int16_t age{};
	std::int16_t template_id{};
	std::uint8_t flag{};
	std::uint8_t anim_frame{};
	std::uint8_t in_kill_anim{};
	std::uint8_t unused_1{};
	std::int16_t patnum_delta{};
	std::int16_t render_as{};
	std::int16_t angle{};
	std::int16_t spawned_in_left_half{};
	std::int16_t loop_i{};
	std::int8_t velocity_x{};
	std::int8_t velocity_y{};
	std::uint8_t despawn_when_offscreen_vertically{};
	std::uint8_t unused_2{};
	std::int16_t damage{};
	std::uint8_t not_shootable{};
	std::uint8_t no_player_collision{};
	std::uint8_t pellet_group{};
	std::uint8_t pellet_speed{};
	bool operator==(const Enemy &) const = default;
};

static_assert(sizeof(Enemy) == 0x26U);
static_assert(offsetof(Enemy, pos_on_page) == 0x00U);
static_assert(offsetof(Enemy, script_ip) == 0x08U);
static_assert(offsetof(Enemy, age) == 0x0AU);
static_assert(offsetof(Enemy, template_id) == 0x0CU);
static_assert(offsetof(Enemy, flag) == 0x0EU);
static_assert(offsetof(Enemy, in_kill_anim) == 0x10U);
static_assert(offsetof(Enemy, damage) == 0x20U);
static_assert(offsetof(Enemy, no_player_collision) == 0x23U);
static_assert(offsetof(Enemy, pellet_speed) == 0x25U);

// Semantic template state used by native callers. The original final field is
// a DOS far pointer; script_index preserves its source identity without
// placing a host pointer in deterministic state. Unlike Enemy, this host
// record is not asserted to the 0x24-byte DOS layout.
struct EnemyTemplate {
	std::int16_t unused{};
	std::int16_t spawn_top{};
	std::int16_t bullet_origin_x{};
	std::int16_t bullet_origin_y{};
	std::int16_t patnum{};
	std::int16_t anim_cels{};
	std::int16_t anim_frames_per_cel{};
	std::int16_t explode_sprite{};
	std::uint8_t item{};
	std::uint8_t unused_2{};
	std::int32_t score{};
	std::int16_t hp{};
	std::uint8_t no_player_collision{};
	std::uint8_t unused_3{};
	std::int16_t autofire_interval{};
	std::int16_t width{};
	std::int16_t height{};
	std::uint32_t script_index{};
	bool operator==(const EnemyTemplate &) const = default;
};

// Source th02/main/enemy/update.cpp::enemy_hittest. The wrapper borrows all
// player/shot/score state and performs native page/level refusal before the
// source's player-collision writes. Its optional payload is the source 16-bit
// int damage word, interpreted as signed by downstream HP arithmetic.
[[nodiscard]] std::optional<std::int16_t> enemy_hittest(
	Enemy &enemy, const EnemyTemplate &enemy_template,
	const PlayerFrameState &player_frame, PlayerHitState &player_hit,
	PlayerShotPoolState &shots, ScoreState &score,
	std::uint8_t shot_level, std::uint32_t stage_frame, bool bombing,
	std::uint8_t bomb_damage_frame_mask,
	PlayerShotHitCallbacks &callbacks
) noexcept;

} // namespace th02::core
