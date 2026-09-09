#pragma once
#include "pc98/th0405_motion.hpp"
#include <array>
#include <cstdint>
namespace th05::core {
struct State;
struct Presentation;
struct Stage2Particle {
	std::uint8_t flag{}, angle{};
	pc98::th0405::Motion pos{};
	std::uint16_t zoom{};
};
struct Stage2Scenery {
	std::array<Stage2Particle, 64> particles{};
	std::int16_t spawned{};
	std::uint8_t bg_pulse{}, flash_tone{};
	std::int8_t pulse_direction{};
};
// Source stage_render slot: particles consume both randring and IRAND.
[[nodiscard]] bool stage2_scenery_update(State &, Presentation &) noexcept;
}
