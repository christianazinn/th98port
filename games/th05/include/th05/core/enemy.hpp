#pragma once

#include "pc98/th0405_motion.hpp"
#include "th05/core/bullet.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th05::core {

struct State;
struct Presentation;

inline constexpr std::size_t enemy_count = 32U;
inline constexpr std::uint8_t enemy_free = 0U;
inline constexpr std::uint8_t enemy_alive = 1U;
inline constexpr std::uint8_t enemy_killed = 2U;
inline constexpr std::uint8_t enemy_alive_first_frame = 3U;
inline constexpr std::uint8_t enemy_kill_anim = 0x80U;

// th05/main/enemy/enemy.hpp. The stage bytecode operates on these source
// fields directly. Reuse after an enemy slot becomes free intentionally keeps
// every member that enemies_add() does not overwrite.
struct Enemy {
	std::uint8_t flag{}, age{};
	pc98::th0405::Motion pos{};
	std::int16_t hp{}, score{};
	std::uint16_t script{}, script_ip{};
	std::uint8_t speed{}, patnum_base{}, cur_instr_frame{}, loop_i{};
	std::uint8_t angle{}, angle_delta{};
	std::uint8_t anim_cels{}, anim_frames_per_cel{}, anim_cur_cel{};
	std::uint8_t clip{};
	std::uint8_t item{};
	bool damaged_this_frame{}, can_be_damaged{}, autofire{};
	bool kills_player_on_collision{}, spawned_in_left_half{};
	std::uint8_t autofire_cur_frame{}, autofire_interval{};
	BulletTemplate bullet_template{};
	std::uint8_t subtype{};
	std::int8_t unused_1{};
	std::int16_t unused_2{};
	std::int8_t unused_3{};
	std::array<std::int8_t, 5> padding{};
	bool operator==(const Enemy &) const = default;
};
static_assert(sizeof(Enemy) == 64U);
static_assert(offsetof(Enemy, bullet_template) == 40U);
static_assert(offsetof(Enemy, subtype) == 54U);

struct EnemyState {
	std::array<Enemy, enemy_count> enemies{};
	std::uint16_t gone{}, killed{};
	bool operator==(const EnemyState &) const = default;
};

// th05_main.asm enemies_add. Consumes the source's current eight-byte STD
// record at State::std_data.ip without advancing it; std_run owns that advance.
// False reports malformed source data or a random-ring failure after its
// preceding source writes. Exhausting the fixed enemy pool is source-success.
[[nodiscard]] bool enemies_add(State &) noexcept;

// th05_main.asm enemies_update and its source bytecode runner. Executes
// movement, shots, collision, items, Dream updates, and real bullet allocation;
// it refuses a reached malformed/unsupported source VM operation.
[[nodiscard]] bool enemies_update(State &, Presentation &) noexcept;

} // namespace th05::core
