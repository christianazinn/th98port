#ifndef TH03_CORE_GAME_HPP
#define TH03_CORE_GAME_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <vector>

#include "th03/core/numeric.hpp"
#include "th03/core/observer.hpp"
#include "th03/error.hpp"

namespace th03 {

namespace game_mode {

inline constexpr std::uint8_t none = 0x00U;
inline constexpr std::uint8_t story = 0x01U;
inline constexpr std::uint8_t demo = 0x7FU;
inline constexpr std::uint8_t vs_1p_cpu = 0x80U;
inline constexpr std::uint8_t vs_1p_2p = 0x81U;
inline constexpr std::uint8_t vs_cpu_cpu = 0x82U;

} // namespace game_mode

struct CoreInput {
	std::uint16_t p1 = 0;
	std::uint16_t p2 = 0;
	std::uint16_t system = 0;
	std::uint8_t charge_mask = 0;
};

enum class InputContext : std::uint8_t {
	gameplay,
	interstitial,
	pause,
};

enum class InputProtocol : std::uint8_t {
	live,
	recorded_replay,
};

struct GameOptions {
	InputProtocol input_protocol = InputProtocol::live;
	// Replay Patch 0.2.10 exposes the complete unsigned-byte active combo
	// count. Classic leaves the source 99-hit display cap enabled.
	bool three_digit_combos = false;
};

enum class DeferredFrameAction : std::uint8_t {
	resume,
	exit_session,
};

struct PlayerStart {
	exact::Subpixel center_x;
	exact::Subpixel center_y;
	std::uint8_t halfhearts = 0;
	std::uint8_t invincibility_time = 0;
	std::uint8_t gauge_charge_speed = 0;
	std::uint16_t gauge_charged = 0;
	std::uint16_t gauge_avail = 0;
	std::uint8_t bombs = 0;
	std::uint8_t shot_active = 0;
	std::uint16_t cpu_frame = 0;
};

struct RuntimeSnapshot {
	std::uint32_t random_seed = 0;
	std::array<std::uint8_t, 256> randring{};
	std::uint8_t randring_cursor = 0;
	std::array<std::uint8_t, 256> formation_type_ring{};
	std::array<std::uint8_t, 256> formation_position_ring{};
	std::array<std::uint8_t, 2> formation_cursor{};
	std::array<std::array<std::uint8_t, 64>, 2> cpu_charge_ring{};
	std::array<std::uint8_t, 2> cpu_charge_cursor{};
	std::array<PlayerStart, 2> players{};
};

// A format-neutral recipe for compact round-start formats. Runtime tables are
// reconstructed only after GameContent supplies the actual formation count.
struct CompactRuntimeSnapshot {
	std::uint32_t round_reset_seed = 0;
	std::uint32_t random_seed = 0;
	std::uint8_t first_formation = 0;
	std::array<PlayerStart, 2> players{};
};

struct RoundStateSnapshot {
	std::uint8_t round_id = 0;
	std::array<std::uint8_t, 2> rounds_won{};
	std::array<std::array<std::uint8_t, 8>, 2> score{};
	std::uint8_t round_speed = 0;
	std::uint8_t bullet_base_speed = 0;
	std::array<std::uint8_t, 2> gauge_attack_level{};
	std::uint8_t boss_attack_level = 0;
	std::uint8_t cpu_hit_damage_additional = 0;
	std::uint8_t extends_gained = 0;
	std::array<std::uint16_t, 2> cpu_safety_frames{};
};

struct ComboCarrySnapshot {
	std::uint8_t time = 0;
	std::uint8_t hits_highest = 0;
	std::uint16_t bonus_total = 0;
};

struct RoundCarrySnapshot {
	std::array<std::uint8_t, 2> cpu_dodge_strategy{};
	std::array<std::uint16_t, 2> human_movement_last{};
	std::array<std::uint8_t, 2> spell_ready_frames{};
	std::array<std::uint16_t, 2> combo_bonus_max{};
	std::array<std::uint8_t, 2> combo_hits_max{};
	std::array<std::uint8_t, 2> shot_cycle{};
	std::array<std::uint8_t, 2> boss_panic_fired{};
	std::array<std::uint8_t, 2> gauge_attack_next{};
	std::int8_t cpu_shot_decision = 0;
	std::uint8_t background_phase = 0;
	std::array<ComboCarrySnapshot, 2> combo{};
	std::array<std::uint8_t, 2> chain_cursor{};
	std::array<std::array<std::uint8_t, 16>, 2> chain_hits{};
	std::array<std::array<std::uint8_t, 16>, 2>
		chain_pellet_and_fireball_value{};
	std::array<std::array<std::uint8_t, 16>, 2> chain_charge_fireball{};
	std::array<std::array<std::uint8_t, 16>, 2> chain_charge_exatt{};
	std::uint8_t randring_cursor = 0;
	std::uint8_t result_phase = 0;
	std::uint8_t fireball_generation_previous = 0;
};

struct RoundStartSnapshot {
	RoundStateSnapshot state{};
	RoundCarrySnapshot carry{};
};

enum class CpuTimerPolicy : std::uint8_t {
	versus_default = 0,
	story_native = 1,
	infinite = 2,
};

// Replay Patch Practice exposes the source CPU-safety recipes before a Game
// exists so its typed setup owner can freeze the exact first-round anchor.
// Ordinary gameplay and retries call the same function through GameInit.
[[nodiscard]] std::uint16_t practice_cpu_safety_frames(
	CpuTimerPolicy policy,
	std::uint8_t rank,
	std::uint8_t stage,
	std::uint8_t round,
	std::array<std::uint8_t, 2U> is_cpu
) noexcept;

// Practice is a resident rules overlay on GM_VS_1P_CPU, not a separate source
// game mode. Initial numeric values come from RoundStartSnapshot; these are the
// only rules that alter later retry/result behavior.
struct PracticeRules {
	bool uses_stock = false;
	CpuTimerPolicy retry_cpu_timer = CpuTimerPolicy::versus_default;
	std::uint8_t retry_cpu_damage = 0;
};

struct GameInit {
	std::uint32_t global_frame = 0;
	std::int32_t resident_rand = 0;
	std::uint32_t random_seed = 0;
	std::uint8_t game_mode = 0;
	std::uint8_t rank = 0;
	std::uint8_t key_mode = 0;
	std::uint8_t story_stage = 0;
	std::uint8_t story_lives = 0;
	std::uint8_t remaining_credits = 0;
	std::uint8_t skill = 0;
	std::uint8_t demo_number = 0;
	std::array<std::uint8_t, 2> is_cpu{};
	std::array<std::int8_t, 2> paletted_character{};
	std::array<std::int8_t, 9> story_opponents{};
	std::int8_t winner = 0;
	std::uint8_t show_score_menu = 0;
	std::uint8_t fast_op_animation = 0;
	std::array<std::array<std::uint8_t, 8>, 2> last_score{};
	std::uint8_t autofire_mask = 0;
	std::optional<RuntimeSnapshot> runtime_snapshot;
	std::optional<CompactRuntimeSnapshot> compact_runtime_snapshot;
	std::optional<RoundStartSnapshot> round_start_snapshot;
	std::optional<PracticeRules> practice_rules;
};

// Complete gameplay-semantic state at a round's first input boundary. `start`
// contains the resident/start fields with all snapshot recipe optionals cleared;
// `runtime` and `round` contain the effective reconstructed state instead. The
// pre-reset seed is retained separately because runtime initialization consumes
// it before the first input can be observed.
struct RoundStartObservation {
	GameInit start{};
	std::uint32_t effective_reset_seed = 0;
	RuntimeSnapshot runtime{};
	RoundStartSnapshot round{};
};

enum class ContinueChoice : std::uint8_t {
	decline,
	accept,
};

enum class SessionAction : std::uint8_t {
	exit_to_operator,
	start_gameplay,
	request_continue,
	ending,
};

struct SessionTransition {
	SessionAction action = SessionAction::exit_to_operator;
	GameInit resident{};
	// MAINL's registration menu observes resident state before Continue clears
	// both score arrays. The optional snapshot preserves that event ordering.
	std::optional<GameInit> high_score_registration;
};

struct CanonicalState {
	std::uint8_t route = 0;
	std::uint8_t game_mode = 0;
	std::uint8_t story_stage = 0;
	std::uint8_t round_id = 0;
	std::int8_t winner = 0;
	std::uint8_t round_speed = 0;
	std::uint32_t global_frame = 0;
	std::uint32_t round_frame = 0;
	std::uint16_t round_or_result_frame = 0;
	std::array<std::uint8_t, 8> p1_score{};
	std::array<std::uint8_t, 8> p2_score{};
	std::int32_t resident_rand = 0;
	std::uint32_t state_hash = 0;
};

struct EnemyFormation {
	std::vector<std::vector<std::uint8_t>> enemy_scripts;
};

struct GameContent {
	std::vector<EnemyFormation> enemy_formations;
};

enum class StepStatus {
	running,
	phase_transition,
	round_transition,
	terminal,
	invalid_input_context,
	simulation_not_implemented,
	paused,
};

struct StepResult {
	StepStatus status = StepStatus::simulation_not_implemented;
	CanonicalState input_boundary_state{};
	// Publication interval before the next process step, in host VSync samples.
	std::uint8_t next_step_vsyncs = 1U;
};

class Game {
public:
	Game(Game &&) noexcept;
	Game &operator=(Game &&) noexcept;
	~Game();
	Game(const Game &) = delete;
	Game &operator=(const Game &) = delete;

	static std::expected<Game, Error> create(const GameInit &init);
	static std::expected<Game, Error> create(
		const GameInit &init,
		GameContent content
	);
	static std::expected<Game, Error> create(
		const GameInit &init,
		GameContent content,
		GameOptions options,
		GameplayObserver *observer = nullptr
	);

	// An observer passed to create() or either stepping call is borrowed only
	// for that call's dynamic extent and is never retained. Snapshot-backed
	// creation emits no facts for source initialization work it bypasses.
	// Observer callbacks must not reenter or mutate this Game.
	StepResult step(
		CoreInput input,
		InputContext context = InputContext::gameplay,
		GameplayObserver *observer = nullptr
	);
	std::expected<StepResult, Error> finish_deferred_frame(
		DeferredFrameAction action,
		GameplayObserver *observer = nullptr
	);
	[[nodiscard]] bool deferred_frame_pending() const noexcept;
	CanonicalState canonical_state() const;
	RoundStartObservation round_start_observation() const;
	std::expected<void, Error> start_next_round();
	std::expected<void, Error> finish_main_process();
	std::expected<SessionTransition, Error> session_transition(
		std::optional<ContinueChoice> continue_choice = std::nullopt
	) const;
	std::expected<GameInit, Error> next_story_stage_init() const;

private:
	struct Impl;
	Game(
		GameInit init,
		GameContent content,
		GameOptions options,
		GameplayObserver *observer
	);
	std::unique_ptr<Impl> impl_;
};

} // namespace th03

#endif
