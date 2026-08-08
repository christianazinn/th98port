#ifndef TH03_CORE_GAME_HPP
#define TH03_CORE_GAME_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

#include "th03/core/numeric.hpp"
#include "th03/error.hpp"

namespace th03 {

struct CoreInput {
	std::uint16_t p1 = 0;
	std::uint16_t p2 = 0;
	std::uint16_t system = 0;
	std::uint8_t charge_mask = 0;
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

struct GameInit {
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
};

struct CanonicalState {
	std::uint32_t global_frame = 0;
	std::uint32_t round_frame = 0;
	std::uint16_t round_or_result_frame = 0;
};

struct PresentationEvent {};

enum class StepStatus {
	running,
	phase_transition,
	terminal,
	simulation_not_implemented,
};

struct StepResult {
	StepStatus status = StepStatus::simulation_not_implemented;
};

class Game {
public:
	static std::expected<Game, Error> create(const GameInit &init);

	StepResult step(CoreInput input);
	CanonicalState canonical_state() const;
	std::span<const PresentationEvent> presentation_events() const;

private:
	explicit Game(GameInit init);
	GameInit init_;
	CanonicalState canonical_{};
	std::vector<PresentationEvent> presentation_events_;
};

} // namespace th03

#endif
