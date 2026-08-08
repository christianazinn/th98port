#ifndef TH03_CORE_GAME_INTERNAL_HPP
#define TH03_CORE_GAME_INTERNAL_HPP

#include <array>
#include <cstdint>
#include <vector>

#include "th03/core/game.hpp"

namespace th03 {

namespace detail {

inline constexpr std::size_t player_count = 2;
inline constexpr std::size_t score_digits = 8;
inline constexpr std::size_t chain_ring_size = 16;
inline constexpr std::size_t enemy_count = 40;
inline constexpr std::size_t bullet_count = 320;
inline constexpr std::size_t fireball_count = 24;
inline constexpr std::size_t extra_attack_count = 16;
inline constexpr std::size_t extra_attack_regular_count = 8;
inline constexpr std::size_t extra_attack_rikako_count = 14;
inline constexpr std::size_t shotpair_count = 32;
inline constexpr std::size_t collmap_width = 144;
inline constexpr std::size_t collmap_height = 184;

enum class DefeatFlag : std::uint8_t {
	none = 0,
	explode = 1,
	banner = 2,
};

enum class BackgroundPhase : std::uint8_t {
	initial = 0,
	type_a_steady = 1,
	type_b_steady = 2,
};

enum class BackgroundInitialKind : std::uint8_t {
	type_a,
	type_b,
};

enum class ResultPhase : std::uint8_t {
	idle = 0,
	opening = 1,
	closing = 2,
};

enum class WarningFlag : std::uint8_t {
	none = 0,
	portrait = 1,
	flash_white = 2,
	flash_red = 3,
	flash_red_end = 33,
};

enum class GaugeBossAttackFlag : std::uint8_t {
	none = 0,
	gauge_pellet_init = 1,
	gauge_pellet_active = 2,
	gauge_bullet_init = 3,
	gauge_bullet_active = 4,
	boss = 5,
};

enum class BombFlag : std::uint8_t {
	inactive = 0,
	preparing = 1,
	active = 2,
};

struct PlayerState {
	exact::Subpixel center_x{};
	exact::Subpixel center_y{};
	bool is_hit = false;
	std::uint8_t halfhearts = 0;
	std::uint8_t invincibility_time = 0;
	std::uint8_t shot_mode = 0;
	std::uint8_t knockback_time = 0;
	std::uint8_t move_lock_time = 0;
	std::uint8_t knockback_angle = 0;
	std::uint8_t knockback_length = 0;
	bool knockback_active = false;
	std::uint16_t gauge_charged = 0;
	std::uint16_t gauge_avail = 0;
	std::uint8_t bombs = 0;
	std::uint8_t hyper_active = 0;
	bool hyper_character_callback = false;
	bool hyper_effect_active_this_frame = false;
	std::uint8_t lose_anim_time = 0;
	std::uint8_t rounds_won = 0;
	std::uint16_t cpu_frame = 0;
	std::uint8_t hit_damage_next = 0;
	std::uint8_t shot_active = 0;
	std::uint8_t cpu_dodge_strategy = 0;
	std::uint16_t human_movement_last = 0;
	std::uint8_t spell_ready_frames = 0;
	std::uint16_t combo_bonus_max = 0;
	std::uint8_t combo_hits_max = 0;
	std::uint8_t gauge_attacks_fired = 0;
	std::uint8_t boss_attacks_fired = 0;
	std::uint8_t boss_attacks_reversed = 0;
	std::uint8_t boss_panics_fired = 0;

	std::uint8_t is_cpu = 0;
	std::int8_t paletted_character = 0;
	std::uint8_t gauge_charge_speed = 0;
	std::uint16_t cpu_safety_frames = 0;
	std::uint8_t speed_aligned_x = 0;
	std::uint8_t speed_aligned_y = 0;
	std::uint8_t speed_diagonal_x = 0;
	std::uint8_t speed_diagonal_y = 0;
	bool reimu_chargeshot_active = false;
	std::uint8_t reimu_chargeshot_age = 0;
	std::array<std::uint8_t, 64> cpu_charge_ring{};
	std::uint8_t cpu_charge_cursor = 0;
};

struct ComboState {
	std::uint8_t time = 0;
	std::uint8_t hits_highest = 0;
	std::uint16_t bonus_total = 0;
};

struct ChainState {
	std::array<std::uint8_t, player_count> cursor{};
	std::array<std::array<std::uint8_t, chain_ring_size>, player_count> hits{};
	std::array<std::array<std::uint8_t, chain_ring_size>, player_count>
		pellet_and_fireball_value{};
	std::array<std::array<std::uint8_t, chain_ring_size>, player_count>
		charge_fireball{};
	std::array<std::array<std::uint8_t, chain_ring_size>, player_count>
		charge_exatt{};
};

enum class EnemyFlag : std::uint8_t {
	free = 0,
	running_spawned = 1,
	running_unspawned = 3,
	explosion_ignoring_enemies = 9,
	explosion_hitting_enemies = 10,
};

struct EnemyState {
	EnemyFlag flag = EnemyFlag::free;
	std::uint8_t frame = 0;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	std::uint8_t hp = 0;
	std::uint8_t player_id = 0;
	std::uint8_t size_pixels = 0;
	std::uint16_t script_ip = 0;
	std::uint16_t script_op_frame = 0;
	std::int16_t velocity_x = 0;
	std::int16_t velocity_y = 0;
	std::uint16_t angle = 0;
	std::int8_t angle_speed = 0;
	std::uint8_t formation_type = 0;
	std::uint8_t formation_index = 0;
	std::uint8_t speed = 0;
	std::uint8_t loop_index = 0;
	std::uint8_t position_type = 0;
	std::uint8_t chain_slot = 0;
	std::uint8_t explosion_max_enemy_hits_half = 0;
};

struct ShotPairState {
	bool alive = false;
	std::int16_t topleft_x = 0;
	std::int16_t topleft_y = 0;
	std::int16_t velocity_y = 0;
	std::uint8_t player_id = 0;
};

struct ReimuChargeGroup {
	std::uint8_t state = 0;
	std::uint8_t frame = 0;
	std::array<std::int16_t, 7> x{};
	std::array<std::int16_t, 7> y{};
	std::uint8_t angle = 0;
	std::uint8_t speed = 0;
};

struct KanaChargeGroup {
	std::array<std::int16_t, 13> x{};
	std::array<std::int16_t, 13> y{};
	std::uint8_t angle = 0;
	std::uint8_t speed = 0;
};

struct RikakoChargeNode {
	std::int16_t x = 0;
	std::int16_t y = 0;
	std::uint8_t angle = 0;
};

struct ChiyuriChargeNode {
	std::uint8_t state = 0;
	std::uint8_t frame = 0;
	std::int16_t x = 0;
	std::int16_t y = 0;
};

struct EllenChargeNode {
	std::uint8_t state = 0;
	std::uint8_t phase = 0;
	std::uint8_t angle = 0;
	std::uint8_t speed = 0;
	std::int16_t x = 0;
	std::int16_t y = 0;
	std::int16_t velocity_x = 0;
	std::int16_t velocity_y = 0;
};

struct EllenBombVector {
	std::int16_t x = 0;
	std::int16_t y = 0;
	std::int16_t velocity_x = 0;
	std::int16_t velocity_y = 0;
};

enum class BulletFlag : std::uint8_t {
	free = 0,
	pellet = 1,
	bullet16 = 2,
	transfer_pellet = 3,
	pellet_cloud = 4,
};

struct BulletState {
	BulletFlag flag = BulletFlag::free;
	std::uint8_t age = 0;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	std::int16_t target_x_for_target_player = 0;
	std::int16_t target_x_for_origin_player = 0;
	std::int16_t velocity_x = 0;
	std::int16_t velocity_y = 0;
	std::uint8_t speed_next = 0;
	std::uint8_t angle_next = 0;
	std::uint8_t player_id = 0;
	std::uint8_t group_next = 0;
	std::uint8_t accel_type = 0;
	bool is_collidable = true;
};

enum class FireballFlag : std::uint8_t {
	free = 0,
	fall = 1,
	transfer = 2,
	explosion_ignoring_enemies = 9,
	explosion_hitting_enemies = 10,
};

struct FireballState {
	FireballFlag flag = FireballFlag::free;
	std::uint8_t frame = 0;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	std::uint8_t variant_as_explosion_target = 2;
	std::uint8_t hp = 0;
	std::uint8_t player_id = 0;
	std::uint8_t size_pixels = 48;
	std::int16_t velocity_x = 0;
	std::int16_t velocity_y = 0;
	std::int16_t target_x_for_target_player = 0;
	std::int16_t target_x_for_origin_player = 0;
	std::uint8_t fall_velocity_y = 0;
	std::uint8_t chain_slot = 0;
	std::uint8_t generation = 0;
};

struct ExtraAttackState {
	std::uint8_t state = 0;
	std::uint8_t frame = 0;
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	std::int16_t velocity_x = 0;
	std::int16_t velocity_y = 0;
	std::int16_t target_x_for_target_player = 0;
	std::int16_t target_x_for_origin_player = 0;
	std::uint8_t player_id = 0;
	std::uint8_t angle = 0;
	std::uint8_t speed = 0;
	std::uint8_t behavior_variant = 0;
};

struct BossInitialState {
	std::int16_t center_x = 0;
	std::int16_t center_y = 0;
	std::int16_t velocity_x = 0;
	std::int16_t velocity_y = 0;
	std::uint16_t hp = 0;
	std::uint8_t state = 0;
	std::uint8_t motion_angle = 0;
	std::uint8_t pattern_index = 0;
	std::uint8_t render_state = 0;
};

struct BossState : BossInitialState {
	std::uint8_t hit = 0;
	std::uint8_t pattern_previous = 0;
	std::uint8_t pattern_count = 0;
	std::uint16_t pattern_frame = 0;
	std::int16_t anchor_x = 0;
	std::int16_t anchor_y = 0;
	std::array<std::int16_t, 6> orbit_x{};
	std::array<std::int16_t, 6> orbit_y{};
	std::array<std::uint8_t, 7> params{};
	std::uint8_t render_frame = 0;
	std::uint8_t direction = 0;
	std::int8_t angle_delta = 0;
	std::uint8_t pattern_angle = 0;
};

struct DeterministicState {
	std::uint8_t game_mode = 0;
	std::uint8_t rank = 0;
	std::uint8_t story_stage = 0;
	std::uint8_t story_lives = 0;
	std::uint8_t skill = 0;
	std::uint8_t extends_gained = 255;
	std::int32_t resident_rand = 0;
	std::int8_t winner = 0;
	std::uint8_t round_id = 0;
	std::uint32_t round_frame = 0;
	std::uint16_t round_or_result_frame = 0;
	std::uint8_t round_speed = 0;
	DefeatFlag defeat_flag = DefeatFlag::none;
	std::array<std::uint16_t, player_count> input_player{};
	std::uint16_t input_system = 0;
	std::array<std::array<std::uint8_t, score_digits>, player_count> score{};
	std::array<PlayerState, player_count> players{};
	std::uint32_t random_seed = 0;
	std::array<std::uint8_t, 256> randring{};
	std::uint8_t randring_cursor = 0;
	std::array<std::uint8_t, 256> formation_type_ring{};
	std::array<std::uint8_t, 256> formation_position_ring{};
	std::array<std::uint8_t, player_count> formation_cursor{};
	std::array<std::uint8_t, player_count> autofire_shot_gate{};
	std::int8_t cpu_target_kind_scratch = 0;
	std::array<std::uint8_t, player_count> boss_panic_fired_in_combo{};
	std::array<BombFlag, player_count> bomb_flag{};
	std::array<std::uint8_t, player_count> bomb_frame{};
	std::array<std::array<EllenBombVector, 8>, player_count>
		ellen_bomb_vectors{};
	std::uint16_t ellen_bomb_vector_cursor = 0;
	std::array<std::int16_t, player_count> playfield_fg_shift_x{};
	std::array<std::uint8_t, player_count> damage_all_on{};
	std::uint8_t cpu_hit_damage_additional = 0;
	std::array<WarningFlag, player_count> warning_flag{};
	std::array<GaugeBossAttackFlag, player_count> gba_flag_next{};
	std::array<GaugeBossAttackFlag, player_count> gba_flag_active{};
	std::array<std::uint8_t, player_count> gba_gauge_level{};
	std::array<std::uint8_t, player_count> reimu_gauge_pattern_frame{};
	std::array<std::uint8_t, player_count> kana_gauge_pattern_frame{};
	std::array<std::int16_t, player_count> kana_gauge_pattern_x{};
	std::array<std::uint8_t, player_count> rikako_gauge_pattern_frame{};
	std::array<std::uint8_t, player_count> gauge_pattern_param_a{};
	std::array<std::uint8_t, player_count> gauge_pattern_param_b{};
	std::uint8_t gba_boss_level = 0;
	std::int8_t gba_boss_launched_by = -1;
	std::uint16_t combo_points_for_boss_attack = 5120;
	std::array<BossInitialState, player_count> boss_initial{};
	BossState boss{};
	std::array<std::uint8_t, player_count> boss_target_active{};
	std::int16_t boss_target_x = 0;
	std::int16_t boss_target_y = 0;
	BackgroundPhase background_phase = BackgroundPhase::initial;
	BackgroundInitialKind background_initial_kind = BackgroundInitialKind::type_a;
	ResultPhase result_phase = ResultPhase::idle;
	std::uint8_t result_phase_frame = 0;
	bool result_transition_complete = false;
	std::uint32_t result_bonus_remaining = 0;
	std::uint16_t result_bonus_per_frame = 0;
	std::array<ComboState, player_count> combos{};
	ChainState chains{};
	std::uint32_t global_frame = 0;
	std::uint8_t route = 0;
	std::uint8_t hud_start_flag = 1;
	std::uint8_t hud_start_state = 0;
	std::uint16_t hud_start_frame = 0;
	std::array<EnemyState, enemy_count> enemies{};
	std::array<BulletState, bullet_count> bullets{};
	std::array<FireballState, fireball_count> fireballs{};
	std::array<
		std::array<ExtraAttackState, extra_attack_count>, player_count
	> extra_attacks{};
	std::array<ShotPairState, shotpair_count> shotpairs{};
	std::array<std::array<ReimuChargeGroup, 12>, player_count> reimu_charge{};
	// ZUN stores this primary-charge collision throttle in one global byte,
	// shared by both players and also cleared by Reimu's hyper amulets.
	std::uint8_t reimu_chargeshot_primary = 0;
	std::array<std::array<KanaChargeGroup, 4>, player_count> kana_charge{};
	std::array<std::uint8_t, player_count> kana_charge_state{};
	std::array<std::uint8_t, player_count> kana_charge_frames{};
	std::array<std::array<RikakoChargeNode, 4>, player_count> rikako_charge{};
	std::array<std::uint8_t, player_count> rikako_charge_state{};
	std::array<std::uint8_t, player_count> rikako_charge_frames{};
	std::array<std::uint16_t, player_count> rikako_charge_radius{};
	std::array<std::int16_t, player_count> rikako_charge_origin_x{};
	std::array<std::int16_t, player_count> rikako_charge_origin_y{};
	std::array<std::array<ChiyuriChargeNode, 8>, player_count> chiyuri_charge{};
	std::array<std::array<EllenChargeNode, 32>, player_count> ellen_charge{};
	std::array<std::uint8_t, player_count> enemies_alive{};
	std::uint8_t enemy_speed = 0;
	std::uint8_t bullet_base_speed = 0;
	std::uint8_t fireball_generation_previous = 0;
	std::array<
		std::array<std::array<bool, collmap_height>, collmap_width>,
		player_count
	> collision_map{};
};

std::uint32_t state_hash(const DeterministicState &state) noexcept;
CanonicalState canonical_state(const DeterministicState &state) noexcept;
DeterministicState initialize_first_round(
	const GameInit &init,
	std::size_t formation_count = 0U
);
bool gameplay_frame_prefix(
	DeterministicState &state,
	const GameContent &content
) noexcept;
void gameplay_input_latch(
	DeterministicState &state,
	CoreInput &input,
	std::uint8_t autofire_mask
) noexcept;
bool gameplay_frame_suffix_before_interstitial(
	DeterministicState &state
) noexcept;
void interstitial_input_latch(
	DeterministicState &state,
	const CoreInput &input
) noexcept;
void gameplay_frame_suffix_after_interstitial(
	DeterministicState &state
) noexcept;
bool player_hittest_legacy(
	const DeterministicState &state,
	std::uint8_t player_id,
	std::uint8_t hitbox_size,
	std::int16_t &collision_x,
	std::int16_t &collision_y
) noexcept;
std::uint16_t irand(std::uint32_t &seed) noexcept;
void randring_fill(DeterministicState &state) noexcept;
std::uint8_t randring_next8(DeterministicState &state) noexcept;
std::uint16_t randring_next16(DeterministicState &state) noexcept;

} // namespace detail

struct Game::Impl {
	Impl(const GameInit &init, GameContent content);

	detail::DeterministicState state;
	GameInit initial_init;
	std::uint8_t autofire_mask = 0;
	GameContent content;
	std::vector<PresentationEvent> presentation_events;
	std::uint8_t warning_interstitial_samples_remaining = 0;
};

} // namespace th03

#endif
