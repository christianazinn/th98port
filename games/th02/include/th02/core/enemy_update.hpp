#pragma once

#include "th02/core/enemy_script.hpp"
#include "th02/core/item.hpp"
#include "th02/core/spark.hpp"
#include "th02/core/stage_data.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace th02::core {

// Borrowed stage-owned arrays. Script lengths keep only the decoded source
// bytes visible; no host pointer or copied script enters enemy state.
struct EnemyUpdateScriptData {
	std::span<const EnemyTemplate> templates{};
	std::span<const std::array<std::uint8_t, stage_script_size>> scripts{};
	std::span<const std::uint8_t> script_lengths{};
};

// The two globals that the source resets and then derives from the lowest
// eligible ordinary enemy. They are not part of EnemyPoolState.
struct EnemyUpdateState {
	ScreenPoint boss_position{-1, -1};
	bool operator==(const EnemyUpdateState &) const = default;
};

// Borrowed owners reached by one complete enemies_update_and_render() call.
// max_vm_steps bounds actual enemy_run dispatches per slot; it neither counts
// skipped kill-animation iterations nor advances a source frame on refusal.
struct EnemyUpdateContext {
	EnemyUpdateScriptData script_data{};
	EnemyRunContext &run;
	PlayerHitState &player_hit;
	PlayerShotPoolState &shots;
	const PlayerShotStreamState &stream;
	ScoreState &score;
	ItemPoolState &items;
	SparkPoolState &sparks;
	std::uint32_t stage_frame{};
	std::uint8_t bomb_damage_frame_mask{};
	std::size_t max_vm_steps{};
};

struct EnemySprite {
	ScreenPoint topleft{};
	std::int16_t patnum{};
	bool operator==(const EnemySprite &) const = default;
};

// Script sound/tile boundaries and enemy blits remain synchronous presentation
// seams. Implementations must not mutate any borrowed gameplay owner.
class EnemyUpdateCallbacks : public EnemyScriptCallbacks {
public:
	virtual void render_enemy(EnemySprite sprite) noexcept = 0;
	virtual void render_enemy_hit_white(EnemySprite sprite) noexcept = 0;
};

enum class EnemyKillUpdateResult : std::uint8_t {
	animating,
	removed,
	death_spark_allocation_refused,
};

enum class EnemyUpdateResult : std::uint8_t {
	completed,
	invalid_loop_bound,
	invalid_template,
	invalid_script,
	invalid_page,
	unsupported_opcode,
	source_divide_fault,
	source_iatan2_fault,
	invalid_hittest_input,
	shot_spark_allocation_refused,
	death_spark_allocation_refused,
	semirandom_item_refused,
	vm_step_limit,
	carried_vm_status_stalled,
};

// th02/main/enemy/update.cpp::enemy_kill_update_and_render. The source's
// explode_sprite field selects only between bases 10 and 0, then age chooses
// the cel. On age zero it allocates eight real sparks at speed 20 before the
// word timer increment; a native allocator refusal preserves that prefix.
[[nodiscard]] EnemyKillUpdateResult enemy_kill_update_and_render(
	Enemy &enemy, const EnemyTemplate &enemy_template, ScreenPoint topleft,
	std::size_t page_back, std::int16_t scroll_line, SparkPoolState &sparks,
	EnemyUpdateCallbacks &callbacks
) noexcept;

// th02/main/enemy/update.cpp::enemies_update_and_render. Executes the source
// per-slot VM/collision/death/render order over the raw loop bound. The caller
// still owns stage scheduling, invalidation/page flips, and any follow-up that
// consumes boss_position. A stale carried `continue_same_frame` result entering
// a kill-animation slot is reported rather than allowed to spin forever.
[[nodiscard]] EnemyUpdateResult enemies_update_and_render(
	EnemyPoolState &enemies, EnemyUpdateState &state,
	EnemyUpdateContext &context, EnemyUpdateCallbacks &callbacks
) noexcept;

} // namespace th02::core
