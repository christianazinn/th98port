#pragma once

#include "pc98/th0405_motion.hpp"
#include "th04/core/bullet.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th04::core {

struct State;
struct Presentation;

inline constexpr std::size_t enemy_count = 32U;
inline constexpr std::uint8_t enemy_free = 0U;
inline constexpr std::uint8_t enemy_alive = 1U;
inline constexpr std::uint8_t enemy_killed = 2U;
inline constexpr std::uint8_t enemy_alive_first_frame = 3U;
inline constexpr std::uint8_t enemy_kill_anim = 0x80U;

// th04/main/enemy/enemy.hpp. The bytecode works directly on these source
// representations; intentionally uninitialized members retain their slot's
// prior contents across a later enemies_add().
struct Enemy {
	std::uint8_t flag{}, age{};
	pc98::th0405::Motion pos{};
	std::uint8_t patnum_base{};
	std::int8_t unused_1{};
	std::int16_t hp{}, unused_2{}, score{};
	std::uint16_t script{}, script_ip{};
	std::uint8_t cur_instr_frame{}, loop_i{};
	std::int16_t speed{};
	std::uint8_t angle{}, angle_delta{};
	bool clip_x{}, clip_y{};
	std::int8_t unused_3{};
	std::uint8_t item{};
	bool damaged_this_frame{};
	std::uint8_t anim_cels{}, anim_frames_per_cel{}, anim_cur_cel{};
	bool can_be_damaged{}, autofire{}, kills_player_on_collision{};
	bool spawned_in_left_half{};
	BulletTemplate bullet_template{};
	std::uint8_t autofire_cur_frame{}, autofire_interval{};
	bool operator==(const Enemy &) const = default;
};

struct EnemyState {
	std::array<Enemy, enemy_count> enemies{};
	std::uint16_t gone{}, killed{};
	bool operator==(const EnemyState &) const = default;
};

// th04/main/enemy/add.cpp. `script_id` selects State::std_data.enemy_scripts;
// the two position operands are raw Q12.4 words. False is an unsupported
// source byte offset or random-ring state, with earlier source writes retained.
[[nodiscard]] bool enemies_add(State &, std::uint16_t script_id,
	std::int16_t center_x, std::int16_t center_y, std::uint8_t item) noexcept;

// th04/main/enemy/update.cpp and script.cpp. Runs source bytecode, movement,
// collision, real item/spark allocation and the copied-template bullet path.
// False reports a reached unsupported source operation or malformed script;
// it never converts that operation into a callback or no-op.
[[nodiscard]] bool enemies_update(State &, Presentation &) noexcept;

} // namespace th04::core
