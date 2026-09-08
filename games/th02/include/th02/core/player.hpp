#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "th02/core/shot.hpp"

namespace th02::core {

struct RandomState;
struct ItemPoolState;
struct SparkPoolState;

struct ScreenPoint {
	std::int16_t x{};
	std::int16_t y{};
	bool operator==(const ScreenPoint &) const = default;
};

struct PlayerPositionState {
	std::array<ScreenPoint, 2> on_page{};
	ScreenPoint current{};
	bool operator==(const PlayerPositionState &) const = default;
};

struct MovementSpeeds {
	std::int8_t aligned_x{}, aligned_y{}, diagonal_x{}, diagonal_y{};
};

enum class PlayerPose : std::int16_t { still = 0, left = 2, right = 3 };

// The frame-owned player and left-option coordinates. Both page indices are
// explicit because player_invalidate() reads their old back-page state before
// carrying only the player coordinates from front to back.
struct PlayerFrameState {
	PlayerPositionState player{};
	std::array<ScreenPoint, 2> option_left_on_page{};
	std::size_t page_front{};
	std::size_t page_back{1U};
	PlayerPose pose{PlayerPose::still};
	bool operator==(const PlayerFrameState &) const = default;
};

struct MovementIntent {
	ScreenPoint delta{};
	PlayerPose pose{PlayerPose::still};
	bool operator==(const MovementIntent &) const = default;
};

// TH02 stores the hit enum in one raw byte: 0 means not hit, 1 is an ordinary
// hit, and 0xFF is the game-over result. Keep other nonzero values representable
// rather than reducing this to bool.
struct PlayerHitState {
	std::uint8_t raw_hit{};
	std::uint8_t invincibility_time{};
	bool invincible_via_bomb{};
	bool miss_active{};
	bool operator==(const PlayerHitState &) const = default;
};

struct PlayerShotStreamState {
	std::uint8_t stream_a_phase{0xFF};
	std::uint8_t stream_b_phase{0xFF};
	std::uint8_t stream_a_cooldown{};
	std::uint8_t stream_b_cooldown{};
	std::uint8_t shot_level{};
	std::uint8_t shot_patnum{};
	std::uint8_t shot_option_patnum{};
	std::uint8_t shot_patnum_powered{};
	std::uint8_t shot_option_patnum_powered{};
	std::int8_t spread_angle_delta{};
	bool operator==(const PlayerShotStreamState &) const = default;
};

// The state owned by TH02's 68-frame player miss body. The hit state owns
// miss_active and the raw hit byte, the score owns lives/bombs, and the shot
// stream owns shot_level; do not duplicate those globals here.
struct PlayerMissState {
	std::uint8_t miss_frame{};
	std::uint8_t power{};
	std::int16_t playperf{};
	std::uint8_t stage_miss_count{};
	std::uint8_t total_miss_count{};
	std::uint8_t start_bombs{};
	bool operator==(const PlayerMissState &) const = default;
};

// Source pixel coordinates and main_patnum_t values passed to super_roll_put()
// by player_miss_update_and_render(). Scroll/VRAM conversion remains a later
// presentation owner.
struct PlayerMissSprite {
	ScreenPoint topleft{};
	std::int16_t patnum{};
	bool operator==(const PlayerMissSprite &) const = default;
};

enum class PlayerMissResult : std::uint8_t {
	completed,
	invalid_page,
	spark_allocation_failed,
	unsupported_shot_level,
};

// Source-position synchronous output from the miss body. These callbacks
// report presentation only; all miss gameplay state is concrete in the body.
class PlayerMissPresentationCallbacks {
public:
	virtual ~PlayerMissPresentationCallbacks() = default;
	virtual void sound_effect(std::uint8_t sound_id) noexcept = 0;
	virtual void hud_power(std::uint8_t power, std::uint8_t shot_level) noexcept = 0;
	virtual void hud_lives(std::int8_t lives) noexcept = 0;
	virtual void hud_bombs(std::int8_t bombs) noexcept = 0;
	virtual void render_player(PlayerMissSprite sprite) noexcept = 0;
};

// Source-sized tile invalidation emitted for the player and each option by
// player_invalidate(). This is a synchronous old-back-page boundary, not a
// sprite-render command or an implicit page operation.
struct PlayerFrameInvalidation {
	ScreenPoint topleft{};
	std::int16_t width{};
	std::int16_t height{};
	bool operator==(const PlayerFrameInvalidation &) const = default;
};

class PlayerFrameInvalidationCallbacks : public PlayerShotInvalidationCallbacks {
public:
	virtual ~PlayerFrameInvalidationCallbacks() = default;
	virtual void invalidate_player(PlayerFrameInvalidation invalidation) noexcept = 0;
	virtual void invalidate_option(PlayerFrameInvalidation invalidation) noexcept = 0;
};

enum class PlayerFrameInvalidationResult : std::uint8_t {
	completed,
	invalid_page,
};

// This is returned at the selected-shot call position. A host-invalid
// selection/page and a modeled source iatan2 fault are deliberately distinct:
// both stop the stream before its following sound/cooldown/phase/bomb work.
enum class PlayerShotStreamResult : std::uint8_t {
	completed,
	invalid_shot_selection,
	volley_invalid_page,
	volley_source_iatan2_fault,
};

// The result after player_move_and_shoot()'s hit guard, source movement and
// option writes, and its selected-volley stream tail. The latter three
// terminal results stop at the selected callback's source position.
enum class PlayerFrameResult : std::uint8_t {
	completed,
	hit_blocked,
	invalid_page,
	invalid_shot_selection,
	volley_invalid_page,
	volley_source_iatan2_fault,
};

// A borrowed synchronous seam for the three calls in the shot-stream tail.
// shot_volley() receives the source-position state plus the two mutable
// handoff patnums, returning only a source-position terminal outcome. It does
// not defer work or model a frame driver.
class PlayerShotStreamCallbacks {
public:
	virtual ~PlayerShotStreamCallbacks() = default;
	virtual PlayerShotStreamResult shot_volley(const PlayerShotStreamState &state,
		std::uint8_t &shot_patnum, std::uint8_t &shot_option_patnum) noexcept = 0;
	virtual void shot_sound(const PlayerShotStreamState &state) noexcept = 0;
	virtual void bomb_request(const PlayerShotStreamState &state) noexcept = 0;
};

// Synchronous source-position effects following a successful selected volley.
// It is separate from the concrete volley dispatcher so this core does not
// acquire sound or bomb state.
class PlayerShotStreamEffects {
public:
	virtual ~PlayerShotStreamEffects() = default;
	virtual void shot_sound(const PlayerShotStreamState &state) noexcept = 0;
	virtual void bomb_request(const PlayerShotStreamState &state) noexcept = 0;
};

// Connects the existing checked stream seam to the native A/B/C volley bodies.
// It borrows the existing pool/lifetime/random/stream state and forwards only
// successful source-position sound/bomb calls to effects; it owns neither a
// deferred queue nor a reset/frame/page driver.
class PlayerShotVolleyDispatcher final : public PlayerShotStreamCallbacks {
public:
	PlayerShotVolleyDispatcher(PlayerShotPoolState &pool,
		PlayerShotLifetimeState &lifetime, RandomState &random,
		PlayerShotVolleyContext &context, PlayerShotStreamEffects &effects) noexcept;

	PlayerShotStreamResult shot_volley(const PlayerShotStreamState &state,
		std::uint8_t &shot_patnum,
		std::uint8_t &shot_option_patnum) noexcept override;
	void shot_sound(const PlayerShotStreamState &state) noexcept override;
	void bomb_request(const PlayerShotStreamState &state) noexcept override;

	// The frame composition updates the existing selected-volley globals at the
	// source call boundary. It must not keep another writer-position copy.
	[[nodiscard]] PlayerShotVolleyContext &volley_context() noexcept;

private:
	PlayerShotPoolState &pool;
	PlayerShotLifetimeState &lifetime;
	RandomState &random;
	PlayerShotVolleyContext &context;
	PlayerShotStreamEffects &effects;
};

// The caller-visible outcomes of the state-only player_update_and_render()
// composition. The two shot outcomes preserve the typed lifetime seam's
// refusal distinction after the player prefix has already run.
enum class PlayerUpdateResult : std::uint8_t {
	completed,
	miss_handled,
	miss_invalid_page,
	miss_spark_allocation_failed,
	miss_unsupported_shot_level,
	shot_lifetime_invalid_page,
	shot_lifetime_source_divide_fault,
};

// Borrowed synchronous owner of player_miss_update_and_render() at the
// existing update seam. PlayerMissUpdateCallbacks binds the concrete native
// body; alternate source-bounded owners may still use this typed boundary. It
// may mutate the represented player hit state at this exact source position,
// but must not mutate the borrowed shot/lifetime state or the caller's quit
// flag.
class PlayerUpdateCallbacks {
public:
	virtual ~PlayerUpdateCallbacks() = default;
	virtual PlayerMissResult player_miss_update(PlayerHitState &state) noexcept = 0;
};

// Only the input-to-delta/pose block of player_move_and_shoot. Its caller
// must already have applied the invincibility/hit guard. No shots or bombs.
[[nodiscard]] std::optional<MovementIntent> movement_intent(
	std::uint16_t input, MovementSpeeds speeds, std::size_t back_page) noexcept;

// The prefix of player_move_and_shoot: apply its hit-immunity guard and report
// whether the remaining movement, shot-stream, and bomb work may run. It does
// not run any of that remaining work and never decrements the timer.
[[nodiscard]] bool player_move_and_shoot_allowed(PlayerHitState &state) noexcept;

// The state-only prefix before player_update_and_render's shot callback. It
// applies the same hit-immunity guard and then decrements a nonzero timer.
// The caller owns shots, the miss callback invocation, quit, and all rendering.
void prepare_player_callbacks(PlayerHitState &state) noexcept;

// State-only composition of player_update_and_render's update prefix: prepare
// the player hit state, run the typed player-shot lifetime update, then call
// the synchronous miss owner if the post-shot raw hit byte is nonzero. A miss
// callback that leaves 0xFF requests quit and has the raw byte cleared. The
// original VRAM-only render tail, page driver, and stage scheduling remain
// deliberately outside this seam; PlayerMissUpdateCallbacks supplies the
// source-bounded miss body without claiming any of those owners.
[[nodiscard]] PlayerUpdateResult player_update_state_step(
	PlayerHitState &player, bool &quit, PlayerShotPoolState &shots,
	PlayerShotLifetimeState &lifetime, std::size_t back_page,
	std::uint32_t stage_frame, PlayerUpdateCallbacks &callbacks
) noexcept;

// State prefix of player_shot_level_update_and_hud_power_put(). The source
// data contains 21 indexed entries. A host power value whose /4 index is 21
// or greater would read unrelated DOS memory and is refused here.
[[nodiscard]] bool hud_shot_level_from_power(std::uint8_t power,
	std::uint8_t &shot_level) noexcept;

// Concrete state body of player_miss_update_and_render(). It preserves source
// keyframes and caller-visible write prefixes. An unrepresented page, spark
// allocation, or shot-level table read returns immediately without rollback.
[[nodiscard]] PlayerMissResult player_miss_state_step(
	PlayerMissState &miss, PlayerFrameState &frame, PlayerHitState &hit,
	PlayerShotStreamState &stream, ScoreState &score, ItemPoolState &items,
	SparkPoolState &sparks, RandomState &random,
	PlayerMissPresentationCallbacks &callbacks
) noexcept;

// Connects the complete miss body to player_update_state_step()'s existing
// synchronous source seam without introducing a stage owner or another copy
// of any borrowed state.
class PlayerMissUpdateCallbacks final : public PlayerUpdateCallbacks {
public:
	PlayerMissUpdateCallbacks(PlayerMissState &miss, PlayerFrameState &frame,
		PlayerShotStreamState &stream, ScoreState &score, ItemPoolState &items,
		SparkPoolState &sparks, RandomState &random,
		PlayerMissPresentationCallbacks &presentation) noexcept;

	PlayerMissResult player_miss_update(PlayerHitState &state) noexcept override;

private:
	PlayerMissState &miss;
	PlayerFrameState &frame;
	PlayerShotStreamState &stream;
	ScoreState &score;
	ItemPoolState &items;
	SparkPoolState &sparks;
	RandomState &random;
	PlayerMissPresentationCallbacks &presentation;
};

// Only the shot-stream tail of player_move_and_shoot, after movement and
// option positioning. Calls its borrowed callbacks in source order; an
// unsuccessful selected volley stops at that exact source call position.
[[nodiscard]] PlayerShotStreamResult player_move_and_shoot_shot_stream_step(
	PlayerShotStreamState &state, std::uint16_t input,
	PlayerShotStreamCallbacks &callbacks
) noexcept;

// th02/main/player/player.cpp::player_move. The caller supplies the page
// established by player_invalidate; this does not copy the front page first.
// An invalid native page is rejected without changing any state.
[[nodiscard]] bool player_move(PlayerPositionState &state, std::size_t back_page,
	std::int16_t delta_x, std::int16_t delta_y) noexcept;

// player_invalidate(): run the checked shot invalidation first, emit the three
// old-back-page player/option requests, and carry only the player from front
// to back. Valid same-page inputs are preserved rather than treated as an
// imagined page alternation; invalid page indices refuse before mutation.
[[nodiscard]] PlayerFrameInvalidationResult player_frame_invalidate(
	PlayerFrameState &frame, PlayerShotPoolState &shots,
	PlayerShotLifetimeState &lifetime, bool reduce_effects,
	PlayerFrameInvalidationCallbacks &callbacks
) noexcept;

// player_move_and_shoot() after player_frame_invalidate(): apply the source hit
// guard, derive/move the player, place the option from the clamped player minus
// the requested delta, update the dispatcher-owned writer positions, and run
// its checked stream tail. It does not invalidate, flip pages, reset, schedule
// a stage, or implement miss/respawn work.
[[nodiscard]] PlayerFrameResult player_frame_move_and_shoot(
	PlayerFrameState &frame, PlayerHitState &hit, MovementSpeeds speeds,
	std::uint16_t input, PlayerShotStreamState &stream,
	PlayerShotVolleyDispatcher &dispatcher
) noexcept;

} // namespace th02::core
