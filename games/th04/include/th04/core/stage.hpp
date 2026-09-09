#pragma once
#include <cstdint>
namespace th04::core {
struct State;
struct Presentation;
// Resource arguments are the selected credit's one-based lives and bombs.
[[nodiscard]] bool gameplay_init(State &, std::uint8_t rank, std::uint8_t playchar,
	std::uint8_t shottype, std::uint8_t lives, std::uint8_t bombs, bool demo,
	bool turbo = false) noexcept;
void stage_init(State &, Presentation &) noexcept;
enum class StageResult { running, pause, dialog, gameover, cleared, unsupported };
// Dialog is an explicit blocking boundary; a host must execute it before
// calling stage_dialog_complete. No dialogue is silently skipped here.
// `cleared` stops at entry to the post-boss flow, before clear-bonus scoring,
// dialogue, fade, or next-stage execution; it does not complete those actions.
[[nodiscard]] StageResult stage_tick(State &, std::uint16_t input, bool shift,
	Presentation &) noexcept;
[[nodiscard]] bool stage_dialog_complete(State &) noexcept;
[[nodiscard]] bool stage_fights_setup(State &) noexcept;
}
