#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th02::core {

struct ScoreState {
	std::int32_t score{};
	std::int32_t score_delta{};
	std::int32_t hiscore{};
	std::uint16_t score_delta_transferred_prev{};
	std::uint16_t extends_gained{};
	std::int8_t lives{};
	std::int8_t bombs{};
	std::int32_t score_reset_unknown_40000{};
	bool operator==(const ScoreState &) const = default;
};

enum class ScoreEventKind { score, hiscore, lives, bombs, sound };
struct ScoreEvent {
	ScoreEventKind kind{};
	std::int32_t value{};
	bool operator==(const ScoreEvent &) const = default;
};
struct ScoreEvents {
	std::array<ScoreEvent, 4> entries{};
	std::size_t count{};
	bool operator==(const ScoreEvents &) const = default;
};

// Re-scan thresholds without granting resources. The original sentinel does
// not bound its loop: scores at/above it are unsupported and refused without
// mutation. On success only extends_gained and score_delta change.
[[nodiscard]] bool score_extend_init(ScoreState &state) noexcept;

// Neither operation emits HUD/SFX requests or processes high scores/extends.
void score_delta_commit(ScoreState &state) noexcept;
void score_reset(ScoreState &state) noexcept;

// State changes from th02/main/score.cpp. HUD/SFX requests retain source order
// and values, but rendering, sound playback, and caller timing are separate.
// An out-of-table extend index is refused without mutation. A zero-delta
// update never reads that table and still clears the previous transfer.
[[nodiscard]] std::optional<ScoreEvents>
score_update(ScoreState &state, bool reduce_effects) noexcept;

// Unlike a delta commit, this neither clears the pending delta/previous
// transfer nor converts a capped life into a bomb (or its sound).
[[nodiscard]] std::optional<ScoreEvents>
score_grant_current_delta_as_bonus(ScoreState &state) noexcept;

} // namespace th02::core
