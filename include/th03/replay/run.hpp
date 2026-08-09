#ifndef TH03_REPLAY_RUN_HPP
#define TH03_REPLAY_RUN_HPP

#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

#include "th03/core/game.hpp"
#include "th03/error.hpp"
#include "th03/replay/session.hpp"

namespace th03::replay {

enum class RunStatus : std::uint8_t {
	complete,
	input_context_mismatch,
	process_boundary_mismatch,
	simulation_not_implemented,
	input_exhausted,
};

struct CompletedRound {
	std::uint32_t sample_index = 0;
	CanonicalState state{};
};

struct RunResult {
	RunStatus status = RunStatus::input_exhausted;
	std::uint32_t consumed_samples = 0;
	std::vector<CompletedRound> completed_rounds;
	std::optional<CanonicalState> terminal_state;
};

// Runs a version-neutral recorded replay at caller speed. File formats,
// expected summaries, clocks, rendering, and I/O remain outside this layer.
std::expected<RunResult, Error> run_session(
	const ReplaySession &session,
	const GameContent &content
);

} // namespace th03::replay

#endif
