#pragma once

#include "th01/core/orb.hpp"
#include "th01/core/shots.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th01::core {

// The four source presentation calls made by CShots::unput_update_render().
// This bounded trace deliberately records their order instead of providing a
// renderer. A scan has eight slots and makes at most two calls per slot.
enum class ShotRenderKind : std::uint8_t {
	moving_unput,
	moving_put,
	decay_unput,
	decay_put,
};

struct ShotRenderOp {
	ShotRenderKind kind{};
	std::size_t slot{};
	std::int16_t left{};
	std::int16_t top{};
	std::uint8_t quarter{};
	bool operator==(const ShotRenderOp &) const = default;
};

struct ShotRenderTrace {
	std::array<ShotRenderOp, 16> operations{};
	std::size_t count{};
	bool operator==(const ShotRenderTrace &) const = default;
};

// State owned by the CShots-to-Orb composition. The caller still owns the
// normal Orb update, force-frame advance, portal lifetime, and rendering.
struct ShotOrbState {
	OrbHorizontal horizontal{};
	OrbVertical vertical{};
	std::int16_t top{};
	bool in_portal{};
};

enum class ShotOrbUpdateStatus : std::uint8_t {
	complete,
	unsupported_force,
	unsupported_vertical,
};

struct ShotOrbUpdateResult {
	ShotOrbUpdateStatus status{ShotOrbUpdateStatus::complete};
	ShotRenderTrace render{};
	bool operator==(const ShotOrbUpdateResult &) const = default;
};

// Source: th01/main/player/shot.cpp (CShots::unput_update_render/hittest_orb).
// The borrowed seed remains unchanged on this OF_SHOT-only path. Represented
// scalar refusals retain preceding source writes and stop this eight-slot scan.
[[nodiscard]] ShotOrbUpdateResult update_shots_with_orb(
	Shots &shots, ShotOrbState &orb, std::uint32_t &random_seed
) noexcept;

} // namespace th01::core
