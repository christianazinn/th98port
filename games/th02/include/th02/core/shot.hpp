#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th02::core {

struct RandomState;
struct ScoreState;

inline constexpr std::size_t player_shot_count = 38;
inline constexpr std::uint8_t player_shot_free = 0;
inline constexpr std::uint8_t player_shot_alive = 1;
inline constexpr std::uint8_t player_shot_remove = 2;

struct ShotPixelPoint {
	std::int16_t x{};
	std::int16_t y{};
	bool operator==(const ShotPixelPoint &) const = default;
};

// Q12.4 coordinates and velocities, matching TH02's 16-bit subpixel_t.
struct ShotSubpixelPoint {
	std::int16_t x{};
	std::int16_t y{};
	bool operator==(const ShotSubpixelPoint &) const = default;
};

// The source's packed shot_t record. The host representation is asserted so
// the source-visible pool field boundaries cannot silently drift.
struct PlayerShot {
	std::uint8_t flag{};
	std::uint8_t decay_cel{};
	std::array<ShotSubpixelPoint, 2> pos_on_page{};
	ShotSubpixelPoint velocity{};
	std::uint8_t patnum{};
	bool from_option{};
	bool operator==(const PlayerShot &) const = default;
};

static_assert(sizeof(PlayerShot) == 16U);
static_assert(offsetof(PlayerShot, flag) == 0U);
static_assert(offsetof(PlayerShot, decay_cel) == 1U);
static_assert(offsetof(PlayerShot, pos_on_page) == 2U);
static_assert(offsetof(PlayerShot, velocity) == 10U);
static_assert(offsetof(PlayerShot, patnum) == 14U);
static_assert(offsetof(PlayerShot, from_option) == 15U);

// The fields surrounding the source pool that its writer helpers read. The
// future shottype spawners own current_slot and spawn_top setup; they are not
// implemented by these writers.
struct PlayerShotPoolState {
	std::array<PlayerShot, player_shot_count> shots{};
	std::int16_t current_slot{};
	std::int16_t spawn_top{};
	std::uint8_t patnum{};
	std::uint8_t option_patnum{};
	bool operator==(const PlayerShotPoolState &) const = default;
};

// Source globals adjacent to, but not stored inside, the 38-shot pool. The
// animation counters are per slot and survive freeing/reuse. The interval is
// signed before the source's usual conversion to uint32_t for its modulo.
struct PlayerShotLifetimeState {
	std::array<std::uint8_t, player_shot_count> animation_frames{};
	std::uint8_t option_shots_alive{};
	std::int8_t option_decay_interval{4};
	bool operator==(const PlayerShotLifetimeState &) const = default;
};

struct PlayerShotWriterPositions {
	ShotPixelPoint player_topleft{};
	ShotPixelPoint option_left_topleft_on_back_page{};
	bool operator==(const PlayerShotWriterPositions &) const = default;
};

// Pixel hitbox handed to the fixed shot-pool scan by an enemy or boss. The
// source computes its two X windows from this same top-left, but deliberately
// shifts only one of their right edges.
struct PlayerShotHitbox {
	ShotPixelPoint topleft{};
	std::int16_t width{};
	std::int16_t height{};
	bool operator==(const PlayerShotHitbox &) const = default;
};

// The synchronous source sparks_add() request issued by a successful player or
// option shot hit. It is a borrowed effect boundary, not a persistent event
// queue or a native spark-pool implementation.
struct PlayerShotHitSpark {
	ShotPixelPoint topleft{};
	std::int16_t speed_base{};
	std::int16_t count{};
	bool as_sprite{};
	bool operator==(const PlayerShotHitSpark &) const = default;
};

class PlayerShotHitCallbacks {
public:
	virtual ~PlayerShotHitCallbacks() = default;
	// This runs at source position but must not mutate the borrowed shot pool,
	// score state, or caller control arguments.
	virtual void shot_hit_spark(PlayerShotHitSpark spark) noexcept = 0;
};

// Source-sized tile work emitted by shots_invalidate(). This is a synchronous
// invalidation boundary, not a sprite render or an implicit page operation.
struct PlayerShotInvalidation {
	ShotPixelPoint topleft{};
	std::int16_t width{};
	std::int16_t height{};
	bool operator==(const PlayerShotInvalidation &) const = default;
};

class PlayerShotInvalidationCallbacks {
public:
	virtual ~PlayerShotInvalidationCallbacks() = default;
	// Runs before the source's removal/page-carry state change and must not
	// mutate the borrowed shot pool, lifetime state, or page arguments.
	virtual void invalidate_shot(PlayerShotInvalidation invalidation) noexcept = 0;
};

// Native-invalid pages fail before any update. A source DIV fault is distinct:
// a zero option-decay period faults only after the preceding in-order updates
// have reached that expression, leaving their source-visible prefix intact.
enum class PlayerShotLifetimeUpdateResult : std::uint8_t {
	completed,
	invalid_page,
	source_divide_fault,
};

// A host-invalid back-page is distinguishable from master.lib's explicit
// source DIV fault. The latter can occur only after the source body has
// already written its preceding regular-shot prefix.
enum class ShotBVolleyResult : std::uint8_t {
	completed,
	invalid_page,
	source_iatan2_fault,
};

// th02/main/player/shot.cpp::shot_add. The caller has already selected a free
// slot and supplied the current back page. Invalid native slot/page values
// refuse without mutation; valid calls preserve the source store ordering.
[[nodiscard]] bool shot_add(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	std::int16_t left_offset, std::uint8_t angle) noexcept;

// th02/main/player/shot.cpp::shot_option_add. It writes the same selected slot
// but starts its distinct option-shot decay counter at one.
[[nodiscard]] bool shot_option_add(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	std::int16_t left_offset, ShotSubpixelPoint velocity) noexcept;

// th02/main/player/shot.cpp::shots_free_all. Only flags are cleared; every
// other byte in each slot remains as the future allocation scan would see it.
void shots_free_all(PlayerShotPoolState &state) noexcept;

// th02/main/player/shot.cpp::shot_a. This is the synchronous selected-shot
// body reached from the player stream tail. It writes the supplied regular
// patnum back after a full-power volley changes it, and always clears the
// separate option-shot latch on a valid native page.
[[nodiscard]] bool shot_a_volley(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	RandomState &random, std::uint8_t shot_level,
	std::int8_t spread_angle_delta, std::uint8_t &shot_patnum,
	std::uint8_t &option_shots_alive) noexcept;

// th02/main/player/shot.cpp::shot_b. This selected-shot body shares one pool
// scan between regular and option writes. At its two source homing call sites,
// it uses the exact shared master.lib iatan2 model; a modeled source DIV fault
// returns after preserving the state prefix already reached.
[[nodiscard]] ShotBVolleyResult shot_b_volley(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	RandomState &random, std::uint8_t shot_level, ShotPixelPoint boss_position,
	std::int16_t &boss_pos_x_unused, std::uint8_t &shot_patnum,
	std::uint8_t &shot_option_patnum, std::uint8_t &option_shots_alive) noexcept;

// th02/main/player/shot.cpp::shot_c. The supplied cycle is incremented before
// each source pattern/cadence decision. On a valid page this body shares one
// scan between regular and option writes, including the level-9 two writes
// selected by the source's `volley_i >= 3` Q12.4 Y-lift condition. An invalid
// host page refuses before the source cycle changes.
[[nodiscard]] bool shot_c_volley(PlayerShotPoolState &state,
	const PlayerShotWriterPositions &positions, std::size_t back_page,
	RandomState &random, std::uint8_t shot_level, std::uint8_t &shot_c_cycle,
	std::uint8_t &shot_patnum, std::uint8_t &shot_option_patnum,
	std::uint8_t &option_shots_alive) noexcept;

// th02/main/player/shot_hittest.cpp::shots_hittest. Scans only the source's
// first 35 slots, changes shot collision state and ScoreState::score_delta,
// and calls the borrowed spark seam at its source positions. Invalid native
// page/level inputs are host refusals with no mutation; tile invalidation,
// shot movement/rendering, enemy HP, frame scheduling, and spark ownership
// remain caller responsibilities.
[[nodiscard]] std::optional<std::int16_t> shots_hittest(
	PlayerShotPoolState &state, ScoreState &score, std::size_t back_page,
	std::uint8_t shot_level, std::uint32_t stage_frame, bool bombing,
	std::uint8_t bomb_damage_frame_mask, PlayerShotHitbox hitbox,
	PlayerShotHitCallbacks &callbacks
) noexcept;

// State-only part of th02/main/player/shot.cpp::shots_update_and_render. It
// advances all source-alive slots on back_page through word movement/bounds,
// regular/option decay, and the persistent regular-shot animation counters.
// Sprite blits, scroll conversion, page flips, and caller schedule remain
// deliberately outside this native core.
[[nodiscard]] PlayerShotLifetimeUpdateResult shots_update_lifetime(
	PlayerShotPoolState &state, PlayerShotLifetimeState &lifetime,
	std::size_t back_page, std::uint32_t stage_frame
) noexcept;

// State and tile-boundary part of th02/main/player/shot.cpp::shots_invalidate.
// Every raw nonfree slot optionally invalidates its source-sized rectangle on
// back_page before F_REMOVE becomes free or the current front position is
// copied to back_page. No renderer, scroll conversion, or page driver is
// implied. Both represented page arguments are explicit and independently
// validated rather than assumed to be a scheduled pair.
[[nodiscard]] bool shots_invalidate_lifetime(PlayerShotPoolState &state,
	PlayerShotLifetimeState &lifetime, std::size_t back_page,
	std::size_t front_page, bool reduce_effects,
	PlayerShotInvalidationCallbacks &callbacks) noexcept;

} // namespace th02::core
