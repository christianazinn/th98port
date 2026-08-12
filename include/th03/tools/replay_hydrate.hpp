#ifndef TH03_TOOLS_REPLAY_HYDRATE_HPP
#define TH03_TOOLS_REPLAY_HYDRATE_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "th03/core/game.hpp"
#include "th03/error.hpp"

namespace th03::tools {

struct ReplayHydrationReport {
	std::uint16_t source_version = 0U;
	std::size_t rounds_verified = 0U;
	std::size_t round_timers_filled = 0U;
	std::size_t clear_bonuses_filled = 0U;
	bool slowdown_filled = false;
	bool changed = false;
};

struct HydratedReplay {
	std::vector<std::byte> bytes;
	ReplayHydrationReport report{};
};

// Reconstructs V13 reporting metadata by replaying the complete input stream.
// Existing nonzero metadata and every byte outside V13's fixed telemetry fields
// are preserved. Gameplay summaries and round-start checkpoints must validate
// before any output is returned.
std::expected<HydratedReplay, Error> hydrate_v13_replay(
	std::span<const std::byte> bytes,
	const GameContent &content
);

} // namespace th03::tools

#endif
