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

enum class ResultPhase : std::uint8_t {
	idle = 0,
	opening = 1,
	closing = 2,
};

struct PlayerState {
	exact::Subpixel center_x{};
	exact::Subpixel center_y{};
	std::uint8_t halfhearts = 0;
	std::uint8_t invincibility_time = 0;
	std::uint8_t shot_mode = 0;
	std::uint8_t knockback_time = 0;
	std::uint8_t move_lock_time = 0;
	std::uint16_t gauge_charged = 0;
	std::uint16_t gauge_avail = 0;
	std::uint8_t bombs = 0;
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

struct DeterministicState {
	std::uint8_t game_mode = 0;
	std::uint8_t rank = 0;
	std::uint8_t story_stage = 0;
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
	std::int8_t bullet_add_scratch = 0;
	std::array<std::uint8_t, player_count> boss_panic_fired_in_combo{};
	BackgroundPhase background_phase = BackgroundPhase::initial;
	ResultPhase result_phase = ResultPhase::idle;
	std::array<ComboState, player_count> combos{};
	ChainState chains{};
	std::uint32_t global_frame = 0;
	std::uint8_t route = 0;
};

std::uint32_t state_hash(const DeterministicState &state) noexcept;
CanonicalState canonical_state(const DeterministicState &state) noexcept;
DeterministicState initialize_first_round(const GameInit &init);

} // namespace detail

struct Game::Impl {
	explicit Impl(const GameInit &init);

	detail::DeterministicState state;
	std::vector<PresentationEvent> presentation_events;
};

} // namespace th03

#endif
