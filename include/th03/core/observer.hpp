#ifndef TH03_CORE_OBSERVER_HPP
#define TH03_CORE_OBSERVER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th03 {

namespace detail {

struct DeterministicState;
struct ObserverAccess;

} // namespace detail

enum class PlayerId : std::uint8_t {
	p1 = 0,
	p2 = 1,
};

enum class ShotPairSlot : std::uint8_t {};
enum class EnemySlot : std::uint8_t {};
enum class FireballSlot : std::uint8_t {};
enum class BulletSlot : std::uint16_t {};
enum class TrailPoint : std::uint8_t {};
enum class ReimuBossOrbitPoint : std::uint8_t {};

// Exact source enemy/fireball/explosion flag byte. Explosion hits can raise
// this above the two named explosion values, so it must not be a closed enum.
struct EfeFlag {
	std::uint8_t raw = 0;

	[[nodiscard]] constexpr bool is_free() const noexcept
	{
		return raw == 0U;
	}

	[[nodiscard]] constexpr bool is_explosion() const noexcept
	{
		return raw >= 9U;
	}

	friend bool operator==(const EfeFlag &, const EfeFlag &) = default;
};

enum class EnemyClassification : std::uint8_t {
	free,
	running_spawned,
	running_unspawned,
	explosion,
};

enum class FireballClassification : std::uint8_t {
	free,
	fall,
	transfer,
	explosion,
};

enum class FireballVariant : std::uint8_t {
	blue = 2,
	red = 3,
};

// Exact source bullet flag byte. The stock values stop at 4, but the renderer
// deliberately treats every odd byte as a pellet and therefore does not have
// a closed flag domain.
struct BulletFlagFact {
	std::uint8_t raw = 0;

	[[nodiscard]] constexpr bool is_free() const noexcept
	{
		return raw == 0U;
	}

	[[nodiscard]] constexpr bool is_bullet16() const noexcept
	{
		return raw == 2U;
	}

	[[nodiscard]] constexpr bool is_pellet_cloud() const noexcept
	{
		return raw == 4U;
	}

	[[nodiscard]] constexpr bool is_rendered_as_pellet() const noexcept
	{
		return (raw & 1U) != 0U;
	}

	friend bool operator==(
		const BulletFlagFact &,
		const BulletFlagFact &
	) = default;
};

struct MainPhaseContext {
	std::uint8_t round_id = 0;
	std::uint16_t round_or_result_frame = 0;
	std::array<std::int16_t, 2> playfield_fg_shift_x{};

	friend bool operator==(
		const MainPhaseContext &,
		const MainPhaseContext &
	) = default;
};

struct ShotPairFact {
	ShotPairSlot slot{};
	bool alive = false;
	std::int16_t topleft_x = 0;
	std::int16_t topleft_y = 0;
	PlayerId player_id = PlayerId::p1;

	friend bool operator==(
		const ShotPairFact &,
		const ShotPairFact &
	) = default;
};

struct EnemyFact {
	EnemySlot slot{};
	EfeFlag flag{};
	std::uint8_t frame = 0;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	PlayerId player_id = PlayerId::p1;
	std::uint8_t size_pixels = 0;
	std::uint8_t hp = 0;
	std::uint8_t explosion_max_enemy_hits_half = 0;

	[[nodiscard]] constexpr EnemyClassification classification() const noexcept
	{
		if(flag.raw == 0U) {
			return EnemyClassification::free;
		}
		if(flag.raw == 1U) {
			return EnemyClassification::running_spawned;
		}
		if(flag.raw == 3U) {
			return EnemyClassification::running_unspawned;
		}
		return EnemyClassification::explosion;
	}

	friend bool operator==(const EnemyFact &, const EnemyFact &) = default;
};

struct FireballFact {
	FireballSlot slot{};
	EfeFlag flag{};
	std::uint8_t frame = 0;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	PlayerId player_id = PlayerId::p1;
	FireballVariant variant = FireballVariant::blue;

	[[nodiscard]] constexpr FireballClassification classification() const noexcept
	{
		if(flag.raw == 0U) {
			return FireballClassification::free;
		}
		if(flag.raw == 1U) {
			return FireballClassification::fall;
		}
		if(flag.raw < 9U) {
			return FireballClassification::transfer;
		}
		return FireballClassification::explosion;
	}

	friend bool operator==(const FireballFact &, const FireballFact &) = default;
};

struct PlayerFact {
	PlayerId player_id = PlayerId::p1;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	std::int8_t paletted_character = 0;
	std::uint8_t lose_anim_time = 0;
	std::uint8_t invincibility_time = 0;
	std::uint8_t hyper_active = 0;
	std::uint16_t gauge_charged = 0;

	friend bool operator==(const PlayerFact &, const PlayerFact &) = default;
};

struct BulletFact {
	BulletSlot slot{};
	BulletFlagFact flag{};
	std::uint8_t age = 0;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	PlayerId player_id = PlayerId::p1;
	bool has_trail = false;

	friend bool operator==(const BulletFact &, const BulletFact &) = default;
};

struct RawPoint {
	std::int16_t x = 0;
	std::int16_t y = 0;

	friend bool operator==(const RawPoint &, const RawPoint &) = default;
};

struct ShotPairSpawned {
	ShotPairSlot slot{};
	PlayerId player_id = PlayerId::p1;

	friend bool operator==(
		const ShotPairSpawned &,
		const ShotPairSpawned &
	) = default;
};

enum class Bullet16ConstructionKind : std::uint8_t {
	default_attack,
	kana_extra_attack,
};

struct Bullet16Spawned {
	BulletSlot slot{};
	PlayerId source_player = PlayerId::p1;
	Bullet16ConstructionKind construction =
		Bullet16ConstructionKind::default_attack;
	bool source_template_animation_enabled = true;

	friend bool operator==(
		const Bullet16Spawned &,
		const Bullet16Spawned &
	) = default;
};

struct PlayerPoseCommitted {
	PlayerId player = PlayerId::p1;
	// nullopt reuses the presenter's prior source-shared X. An engaged zero is
	// an explicit neutral write and must replace that prior value.
	std::optional<std::int8_t> final_velocity_x_write_q4_4{};

	friend bool operator==(
		const PlayerPoseCommitted &,
		const PlayerPoseCommitted &
	) = default;
};

struct PlayerDefeatStep {
	PlayerId player = PlayerId::p1;
	std::uint8_t lose_anim_time_before = 0;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	std::int16_t fg_shift_x_before_defeat_shake = 0;
	// Appended so existing aggregate fixtures retain their field mapping.
	std::uint16_t round_or_result_frame_before = 0;

	friend bool operator==(
		const PlayerDefeatStep &,
		const PlayerDefeatStep &
	) = default;
};

enum class HitCircleKind : std::uint8_t {
	enemy,
	player,
};

struct HitCircleSpawned {
	HitCircleKind kind = HitCircleKind::enemy;
	PlayerId player = PlayerId::p1;
	// Exact Q12.4 playfield center selected by the source allocation call.
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	// The source pool captures screen X at allocation time. This shift can
	// change again before the same frame's presentation phase.
	std::int16_t fg_shift_x_at_spawn = 0;

	friend bool operator==(const HitCircleSpawned &, const HitCircleSpawned &) =
		default;
};

enum class CommonEffectKind : std::uint8_t {
	expanding_circle,
	contracting_circle,
	contracting_square,
};

struct CommonEffectSpawned {
	CommonEffectKind kind = CommonEffectKind::expanding_circle;
	PlayerId player = PlayerId::p1;
	// Exact Q12.4 playfield center passed to the source allocation call.
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	std::int16_t fg_shift_x_at_spawn = 0;
	// The low byte of the shared-ring word already consumed by SUB_CE5B().
	// Circle kinds leave this at zero.
	std::uint8_t initial_angle = 0;

	friend bool operator==(
		const CommonEffectSpawned &,
		const CommonEffectSpawned &
	) = default;
};

// One ordered fact for every active late Bomb callback, before its
// character-specific work begins. This carries only the source callback inputs;
// clients decide whether and how to use them.
struct BombFrameStep {
	PlayerId player = PlayerId::p1;
	std::int8_t paletted_character = 0;
	std::uint8_t bomb_frame = 0;
	// Sampled before the late character callback mutates the current shake.
	std::int16_t fg_shift_x_at_entry = 0;

	friend bool operator==(const BombFrameStep &, const BombFrameStep &) =
		default;
};

enum class BombCompatibilityReadSite : std::uint8_t {
	reimu_falling_velocity,
	reimu_falling_x,
	mima_column_x,
	ellen_vector_angle,
};

// Raw value from a shared-ring read that the source already consumed in a
// Bomb callback solely to construct presentation-private state. Observation
// never adds or delays a read. Repeated reads are delivered synchronously in
// their source call order; no renderer slot or presentation record identity is
// exposed by the core.
struct BombCompatibilityRead {
	PlayerId player = PlayerId::p1;
	BombCompatibilityReadSite site =
		BombCompatibilityReadSite::reimu_falling_velocity;
	std::uint8_t bomb_frame = 0;
	std::uint16_t raw_value = 0;

	friend bool operator==(
		const BombCompatibilityRead &,
		const BombCompatibilityRead &
	) = default;
};

// Emitted at the prefix transition that disables an expired Bomb. In the
// source this is immediately followed by restoring that player's saved
// palette entry; a late BombFrameStep does not occur for this frame.
struct BombFinished {
	PlayerId player = PlayerId::p1;
	std::int8_t paletted_character = 0;

	friend bool operator==(const BombFinished &, const BombFinished &) = default;
};

enum class BombCommonEffectKind : std::uint8_t {
	kana,
	kotohime,
};

enum class GaugeBossAttackKind : std::uint8_t {
	none,
	gauge_pellet,
	gauge_bullet,
	boss,
};

// Kana and Kotohime derive their common-effect positions from source-shared
// visual globals. One ordered step for every active callback lets a presenter
// reproduce even non-spawning writes without putting presentation state in core.
struct BombCommonEffectStep {
	BombCommonEffectKind kind = BombCommonEffectKind::kana;
	PlayerId player = PlayerId::p1;
	std::uint8_t bomb_frame = 0;
	std::int16_t fg_shift_x_at_effects = 0;

	friend bool operator==(
		const BombCommonEffectStep &,
		const BombCommonEffectStep &
	) = default;
};

struct WarningInterstitialStarted {
	// Captured while warning flags are still flash_white, before the source
	// converts them to flash_red.
	std::array<bool, 2> flashing_players{};
	std::array<GaugeBossAttackKind, 2> next_attacks{};
	std::array<std::uint8_t, 2> gauge_levels{};
	std::uint8_t boss_level = 0;

	friend bool operator==(
		const WarningInterstitialStarted &,
		const WarningInterstitialStarted &
	) = default;
};

struct WarningPlayerLifecycle {
	// Exact semantic progress through none/portrait/white/red warning states.
	// Values 3 through 33 are the source's individual red-flash steps.
	std::uint8_t warning_step_before = 0;
	std::uint8_t warning_step_after = 0;
	GaugeBossAttackKind next_attack = GaugeBossAttackKind::none;
	GaugeBossAttackKind active_attack = GaugeBossAttackKind::none;

	friend bool operator==(
		const WarningPlayerLifecycle &,
		const WarningPlayerLifecycle &
	) = default;
};

// One fact after both source warning callbacks. Array order retains P1 then P2.
struct WarningFrameLifecycle {
	std::array<WarningPlayerLifecycle, 2> players{};
	// SUB_CACB rereads these live values on every red-warning redraw. They can
	// change after the blocking white flash while another player's attack work
	// continues.
	std::array<std::uint8_t, 2> gauge_levels{};
	std::uint8_t boss_level = 0;

	friend bool operator==(
		const WarningFrameLifecycle &,
		const WarningFrameLifecycle &
	) = default;
};

// Emitted exactly when one character Gauge callback disables its completed
// attack. The source restores the target player's saved palette entry here.
struct GaugeAttackFinished {
	PlayerId attacker = PlayerId::p1;
	PlayerId target = PlayerId::p2;

	friend bool operator==(
		const GaugeAttackFinished &,
		const GaugeAttackFinished &
	) = default;
};

// The late source callback draws this player's selected Extra portrait before
// advancing the warning to its blocking white-flash state.
struct WarningPortraitTransition {
	PlayerId player = PlayerId::p1;

	friend bool operator==(
		const WarningPortraitTransition &,
		const WarningPortraitTransition &
	) = default;
};

struct MainGaugePlayerState {
	std::uint16_t gauge_charged = 0;
	std::uint16_t gauge_avail = 0;
	std::uint8_t hyper_active = 0;

	friend bool operator==(
		const MainGaugePlayerState &,
		const MainGaugePlayerState &
	) = default;
};

// Late nonterminal Gauge rendering inputs, after the source's result gate.
struct MainGaugeState {
	MainPhaseContext context{};
	std::array<MainGaugePlayerState, 2> players{};

	friend bool operator==(const MainGaugeState &, const MainGaugeState &) =
		default;
};

// A successful Boss launch is visible only after the source restores the
// target playfield's palette baseline.
struct BossActivated {
	PlayerId owner = PlayerId::p1;
	std::int8_t paletted_character = 0;

	friend bool operator==(const BossActivated &, const BossActivated &) =
		default;
};

// Ellen, Rikako, and Chiyuri advance presenter-private animation state from
// their prefix callbacks. These are the exact retained inputs at that source
// dispatch, not a draw command or a copy of the private animation bytes.
struct BossAnimationStep {
	PlayerId owner = PlayerId::p1;
	std::uint8_t state_at_dispatch = 0;
	std::uint16_t pattern_frame_at_dispatch = 0;
	std::uint8_t render_state_at_dispatch = 0;

	friend bool operator==(
		const BossAnimationStep &,
		const BossAnimationStep &
	) = default;
};

// One exact call of the source palette-baseline restore helper. The presenter
// owns the palette itself; core only identifies the affected playfield.
struct BossPaletteBaselineRestored {
	PlayerId player = PlayerId::p1;

	friend bool operator==(
		const BossPaletteBaselineRestored &,
		const BossPaletteBaselineRestored &
	) = default;
};

struct MainBossFact {
	PlayerId owner = PlayerId::p1;
	std::int8_t paletted_character = 0;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	std::uint8_t hit = 0;
	std::uint8_t state = 0;
	std::uint16_t pattern_frame = 0;
	std::uint8_t render_state = 0;
	std::uint8_t render_frame = 0;
	std::uint8_t pattern_angle = 0;
	std::int16_t kotohime_radius = 0;
	std::uint8_t kotohime_orbit_count = 0;
	std::uint8_t reimu_orbit_count = 0;
	std::uint16_t yumemi_radius = 0;
	RawPoint yumemi_muzzle{};
	RawPoint yumemi_target{};

	friend bool operator==(const MainBossFact &, const MainBossFact &) = default;
};

struct WarningInterstitialSample {
	// Zero-based source mini-loop iteration, always in the inclusive 0..26
	// range. Ordinal 26 precedes the resumed late-frame suffix.
	std::uint8_t ordinal = 0;

	friend bool operator==(
		const WarningInterstitialSample &,
		const WarningInterstitialSample &
	) = default;
};

// The normal late-frame footer boundary after round-mode control and before
// frame-counter maintenance.
struct MainFrameFooterReached {
	friend bool operator==(
		const MainFrameFooterReached &,
		const MainFrameFooterReached &
	) = default;
};

enum class MainBackgroundKind : std::uint8_t {
	type_a,
	type_b,
};

enum class MainBackgroundPhase : std::uint8_t {
	initial,
	steady,
};

// Begins the source D092/D2E8 initialization stream. Snapshot-backed startup
// does not execute either initializer and therefore emits no such fact.
struct MainBackgroundInitialize {
	MainBackgroundKind kind = MainBackgroundKind::type_a;

	friend bool operator==(
		const MainBackgroundInitialize &,
		const MainBackgroundInitialize &
	) = default;
};

// One ordered fact at the existing MAIN background callback boundary. The
// round ID lets clients detect the in-process D05D repeat-round reset without
// exposing any presentation-private particle or renderer state.
struct MainBackgroundFrame {
	std::uint8_t round_id = 0;
	std::uint16_t round_or_result_frame = 0;
	MainBackgroundKind kind = MainBackgroundKind::type_a;
	MainBackgroundPhase phase = MainBackgroundPhase::initial;

	friend bool operator==(
		const MainBackgroundFrame &,
		const MainBackgroundFrame &
	) = default;
};

// Raw result of one source IRAND call already consumed by the D092, D2E8,
// D135, or D340 background callback. Repeated facts preserve source order;
// their presentation-private record identities remain outside the core.
struct MainBackgroundRandomRead {
	std::uint16_t raw_value = 0;

	friend bool operator==(
		const MainBackgroundRandomRead &,
		const MainBackgroundRandomRead &
	) = default;
};

// Borrowed views over one exact MAIN phase boundary. They are valid only for
// the duration of the observer callback; every accessor returns a fact by value.
class ShotsEnemiesView {
public:
	static constexpr std::size_t shot_count = 32U;
	static constexpr std::size_t enemy_count = 40U;

	[[nodiscard]] MainPhaseContext context() const noexcept;
	[[nodiscard]] std::optional<ShotPairFact> shot(
		ShotPairSlot slot
	) const noexcept;
	[[nodiscard]] std::optional<EnemyFact> enemy(
		EnemySlot slot
	) const noexcept;

private:
	friend struct detail::ObserverAccess;

	explicit ShotsEnemiesView(
		const detail::DeterministicState *state
	) noexcept : state_(state)
	{}

	const detail::DeterministicState *state_;
};

class FireballsView {
public:
	static constexpr std::size_t fireball_count = 24U;

	[[nodiscard]] MainPhaseContext context() const noexcept;
	[[nodiscard]] std::optional<FireballFact> fireball(
		FireballSlot slot
	) const noexcept;

private:
	friend struct detail::ObserverAccess;

	explicit FireballsView(
		const detail::DeterministicState *state
	) noexcept : state_(state)
	{}

	const detail::DeterministicState *state_;
};

class PlayersBulletsView {
public:
	static constexpr std::size_t player_count = 2U;
	static constexpr std::size_t bullet_count = 320U;
	static constexpr std::size_t trail_point_count = 6U;

	[[nodiscard]] MainPhaseContext context() const noexcept;
	[[nodiscard]] std::optional<PlayerFact> player(
		PlayerId player
	) const noexcept;
	[[nodiscard]] std::optional<BulletFact> bullet(
		BulletSlot slot
	) const noexcept;
	[[nodiscard]] std::optional<RawPoint> trail_point(
		BulletSlot slot,
		TrailPoint point
	) const noexcept;

private:
	friend struct detail::ObserverAccess;

	explicit PlayersBulletsView(
		const detail::DeterministicState *state
	) noexcept : state_(state)
	{}

	const detail::DeterministicState *state_;
};

class MainBossView {
public:
	static constexpr std::size_t reimu_orbit_point_count = 6U;

	[[nodiscard]] MainPhaseContext context() const noexcept;
	[[nodiscard]] bool round_frame_is_odd() const noexcept;
	[[nodiscard]] std::optional<MainBossFact> boss() const noexcept;
	[[nodiscard]] std::optional<RawPoint> reimu_orbit_point(
		ReimuBossOrbitPoint point
	) const noexcept;

private:
	friend struct detail::ObserverAccess;

	explicit MainBossView(
		const detail::DeterministicState *state
	) noexcept : state_(state)
	{}

	const detail::DeterministicState *state_;
};

class GameplayObserver {
public:
	virtual ~GameplayObserver() = default;

	// Callbacks are synchronous. They must not reenter or mutate the Game that
	// issued them; borrowed phase views expire when their callback returns.
	virtual void shot_pair_spawned(const ShotPairSpawned &) noexcept
	{}
	virtual void bullet16_spawned(const Bullet16Spawned &) noexcept
	{}
	virtual void player_pose_committed(const PlayerPoseCommitted &) noexcept
	{}
	virtual void player_defeat_step(const PlayerDefeatStep &) noexcept
	{}
	// One call after both players' Extra/Boss/Bomb prefix updates and before
	// shots. A presenter advances all live hit circles at this boundary.
	virtual void hit_circles_advanced() noexcept
	{}
	// Delivered synchronously at each source allocation call, in call order.
	virtual void hit_circle_spawned(const HitCircleSpawned &) noexcept
	{}
	// One call after both Gauge callbacks. A presenter advances all live common
	// effects here; later Bomb callbacks can then allocate age-zero effects.
	virtual void common_effects_advanced() noexcept
	{}
	// Delivered synchronously at each non-exceptional source allocation call.
	virtual void common_effect_spawned(const CommonEffectSpawned &) noexcept
	{}
	virtual void bomb_compatibility_read(
		const BombCompatibilityRead &
	) noexcept
	{}
	virtual void bomb_finished(const BombFinished &) noexcept
	{}
	virtual void bomb_frame_step(const BombFrameStep &) noexcept
	{}
	// Every active Kana/Kotohime Bomb callback, including non-spawning frames,
	// in the source's P1-then-P2 order.
	virtual void bomb_common_effect_step(
		const BombCommonEffectStep &
	) noexcept
	{}
	virtual void warning_interstitial_started(
		const WarningInterstitialStarted &
	) noexcept
	{}
	virtual void warning_frame_lifecycle(
		const WarningFrameLifecycle &
	) noexcept
	{}
	virtual void gauge_attack_finished(const GaugeAttackFinished &) noexcept
	{}
	virtual void warning_portrait_transition(
		const WarningPortraitTransition &
	) noexcept
	{}
	virtual void main_gauge_state(const MainGaugeState &) noexcept
	{}
	virtual void warning_interstitial_sample(
		const WarningInterstitialSample &
	) noexcept
	{}
	virtual void main_frame_footer_reached(
		const MainFrameFooterReached &
	) noexcept
	{}
	virtual void main_background_initialize(
		const MainBackgroundInitialize &
	) noexcept
	{}
	virtual void main_background_frame(const MainBackgroundFrame &) noexcept
	{}
	virtual void main_background_random_read(
		const MainBackgroundRandomRead &
	) noexcept
	{}
	virtual void boss_activated(const BossActivated &) noexcept
	{}
	virtual void boss_animation_step(const BossAnimationStep &) noexcept
	{}
	virtual void boss_palette_baseline_restored(
		const BossPaletteBaselineRestored &
	) noexcept
	{}
	virtual void observe(const MainBossView &) noexcept
	{}
	virtual void observe(const ShotsEnemiesView &) noexcept
	{}
	virtual void observe(const FireballsView &) noexcept
	{}
	virtual void observe(const PlayersBulletsView &) noexcept
	{}
};

} // namespace th03

#endif
