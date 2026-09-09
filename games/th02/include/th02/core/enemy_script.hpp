#pragma once

#include "th02/core/bullet.hpp"
#include "th02/core/enemy.hpp"
#include "th02/core/random.hpp"

#include <cstdint>
#include <span>

namespace th02::core {

// The three successful donor returns from enemy_run(): 0 consumes the frame,
// 1 asks its caller to dispatch another opcode this frame, and 2 finishes the
// enemy. Remaining values are explicit native host/source-fault boundaries.
enum class EnemyRunResult : std::uint8_t {
	frame_consumed,
	continue_same_frame,
	finished,
	invalid_page,
	invalid_script_operand,
	unsupported_opcode,
	source_divide_fault,
	source_iatan2_fault,
};

// Frame/stage state borrowed by one source enemy_run() invocation. The script
// span is supplied separately for the template being run; no loader or host
// pointer enters deterministic enemy state here.
struct EnemyRunContext {
	EnemyBulletPoolState &bullets;
	RandomState &random;
	const PlayerFrameState &player_frame;
	std::int16_t playperf{};
	std::int8_t rank{};
	bool bombing{};
	std::uint8_t scroll_delta{};
	std::int16_t scroll_line{};
};

// The only effects without a concrete TH02-native owner. Calls occur at the
// source positions and implementations must not mutate borrowed enemy,
// bullet, random, frame, or context state.
class EnemyScriptCallbacks {
public:
	virtual ~EnemyScriptCallbacks() = default;
	virtual void sound_effect(std::uint8_t sound_id) noexcept = 0;
	virtual void tile_ring_set_and_put_both_8(
		std::int16_t left, std::int16_t vram_y, std::int16_t image
	) noexcept = 0;
};

// th02/main/enemy/update.cpp::enemy_run. Executes exactly one opcode from
// the supplied template script. Its caller owns the returned-1 loop and the
// cross-slot return lifetime; this surface does not provide either one.
[[nodiscard]] EnemyRunResult enemy_run(
	Enemy &enemy, const EnemyTemplate &enemy_template,
	std::span<const std::uint8_t> script, EnemyRunContext &context,
	EnemyScriptCallbacks &callbacks
) noexcept;

} // namespace th02::core
