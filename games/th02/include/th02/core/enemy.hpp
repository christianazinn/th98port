#pragma once

#include "th02/core/player.hpp"
#include "th02/core/score.hpp"
#include "th02/core/shot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace th02::core {

struct RandomState;

inline constexpr std::size_t enemy_page_count = 2U;
inline constexpr std::size_t enemy_pool_size = 25U;
inline constexpr std::size_t enemy_spawn_column_count = 49U;
inline constexpr std::int16_t enemy_hittest_width = 32;
inline constexpr std::int16_t enemy_hittest_height = 32;
inline constexpr std::uint8_t enemy_free = 0U;
inline constexpr std::uint8_t enemy_alive = 1U;
inline constexpr std::uint8_t enemy_remove = 2U;

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

// Persistent source state owned by the stage-enemy subsystem. The bound is a
// raw upper-exclusive byte, deliberately not recomputed by spawn; the source
// has a distinct later invalidation pass for that work.
struct EnemyPoolState {
	std::array<Enemy, enemy_pool_size> enemies{};
	std::uint8_t enemies_loop_bound{};
	std::int16_t spawn_row_cur{};
	bool operator==(const EnemyPoolState &) const = default;
};

// One decoded source spawn-grid row. Column 0 is a signed 16-bit scroll-step
// key; columns 1 through 48 are byte-widened template IDs, with -1 as the
// source's empty-cell sentinel.
struct EnemySpawnRow {
	std::array<std::int16_t, enemy_spawn_column_count> column{};
	bool operator==(const EnemySpawnRow &) const = default;
};

// Borrowed decoded stage data, not a binary-loader format. source_row_count
// is deliberately separate from rows.size(): native code checks the source
// count first and only refuses when the source would actually dereference a
// missing host row.
struct EnemySpawnGrid {
	std::span<const EnemySpawnRow> rows{};
	std::int16_t source_row_count{};
};

// Caller-owned frame globals read by the spawn bodies. No page flip, scroll
// advancement, midboss update, template loading, or stage scheduling occurs
// in this surface.
struct EnemySpawnContext {
	std::int16_t scroll_step{};
	bool midboss_active{};
	std::size_t page_back{};
};

enum class EnemySpawnResult : std::uint8_t {
	completed,
	invalid_page,
	invalid_slot,
	invalid_row,
	invalid_template,
};

// Typed native boundary results for the three complete source movement
// helpers below. Script reads stay scoped to the donor dereference at which
// they occur, so an earlier source write (notably the angle step) remains
// visible if a later operand is unavailable.
enum class EnemyMotionResult : std::uint8_t {
	completed,
	invalid_page,
	invalid_script_operand,
	source_divide_fault,
	source_iatan2_fault,
};

// th02/main/enemy/update.cpp::enemies_reset. It frees only each raw enemy
// flag and rewinds the spawn row; retained record payload and loop bound are
// intentionally caller-visible source state.
void enemies_reset(EnemyPoolState &state) noexcept;

// th02/main/enemy/update.cpp::enemy_add. The caller supplies a slot already
// selected by the spawn loop. It writes only the donor's spawn-time fields on
// the selected back page, consumes exactly one shared-randring byte for the
// animation frame, and widens the loop bound only on strict less-than.
// template_id is stored raw: this body does not read a template.
[[nodiscard]] EnemySpawnResult enemy_add(EnemyPoolState &state,
	RandomState &random, std::int16_t left, std::int16_t top,
	std::int16_t slot, std::int16_t template_id, std::size_t page_back) noexcept;

// th02/main/enemy/update.cpp::enemies_spawn. It can consume exactly one
// equality-triggered grid row per call. Each nonempty cell reads its template
// spawn top before the midboss guard, then uses the first F_FREE slot. A full
// pool and a midboss are ordinary no-spawn paths and consume no animation RNG.
[[nodiscard]] EnemySpawnResult enemies_spawn(EnemyPoolState &state,
	RandomState &random, const EnemySpawnGrid &grid,
	std::span<const EnemyTemplate> templates,
	const EnemySpawnContext &context) noexcept;

// th02/main/enemy/update.cpp::enemy_pos_update. Adds the selected page's
// signed byte velocity to its pixel position. It neither clips nor changes
// the velocity or any other enemy field.
[[nodiscard]] EnemyMotionResult enemy_pos_update(
	Enemy &enemy, std::size_t page_back
) noexcept;

// th02/main/enemy/update.cpp::enemy_pos_update_aimed. It reads the raw
// source-word script offset +2 as the vector length, uses the donor's swapped
// player target (+24 X, +16 Y), narrows the vector into velocity bytes, then
// adds the full vector words to the selected-page position.
[[nodiscard]] EnemyMotionResult enemy_pos_update_aimed(
	Enemy &enemy, const PlayerFrameState &player_frame,
	std::span<const std::uint8_t> script, std::size_t page_back
) noexcept;

// th02/main/enemy/update.cpp::enemy_angle_step_and_move. It reads the
// interval before deciding whether to change the signed-word angle, then
// reads the base and speed operands in source order. Its full vector words
// update position before their signed-byte velocity narrowing.
[[nodiscard]] EnemyMotionResult enemy_angle_step_and_move(
	Enemy &enemy, std::span<const std::uint8_t> script,
	std::int16_t operand, std::int16_t sign, std::size_t page_back
) noexcept;

// th02/main/enemy/update.cpp::enemy_angle_spin_and_move. It reads the sign
// flag and base angle, then changes the signed-word angle with the next
// operand before reading speed. Position takes the pre-turn angle's full word
// vector; cached velocity keeps only its signed low bytes.
[[nodiscard]] EnemyMotionResult enemy_angle_spin_and_move(
	Enemy &enemy, std::span<const std::uint8_t> script,
	std::size_t page_back
) noexcept;

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
