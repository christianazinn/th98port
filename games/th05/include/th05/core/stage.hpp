#pragma once
#include <cstdint>
namespace th05::core {
struct State;
struct Presentation;
void stage_init(State &, Presentation &) noexcept;
enum class StageResult { running, pause, dialog, gameover, cleared, unsupported };
// Stops before unimplemented blocking dialogue and post-boss clear/transition.
// The host must acknowledge dialogue explicitly; it is never silently skipped.
[[nodiscard]] StageResult stage_tick(State &, std::uint16_t input, bool shift,
	Presentation &) noexcept;
[[nodiscard]] bool stage_dialog_complete(State &) noexcept;
[[nodiscard]] bool stage_fights_setup(State &) noexcept;
}
