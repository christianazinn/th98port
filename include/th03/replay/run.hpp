#ifndef TH03_REPLAY_RUN_HPP
#define TH03_REPLAY_RUN_HPP

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
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

struct RoundStartRecord {
	std::uint32_t sample_index = 0;
	RoundStartObservation observation{};
};

struct RoundStartMismatch {
	std::size_t checkpoint_index = 0;
	std::uint32_t sample_index = 0;
	std::optional<std::uint32_t> actual_sample_index;
	std::string field;
};

struct RunResult {
	RunStatus status = RunStatus::input_exhausted;
	std::uint32_t consumed_samples = 0;
	std::vector<CompletedRound> completed_rounds;
	std::vector<RoundStartRecord> round_starts;
	std::optional<CanonicalState> terminal_state;
	std::optional<SessionTransition> terminal_transition;
};

// Returns the first stored checkpoint whose sample key is missing or whose
// gameplay-semantic state differs. Additional sequential round starts are
// accepted because legacy V11 conversions can only preserve stage starts.
// Comparison is exhaustive over RoundStartObservation and never uses
// CanonicalState's sparse diagnostic hash.
std::optional<RoundStartMismatch> first_round_start_mismatch(
	std::span<const RoundStartRecord> expected,
	std::span<const RoundStartRecord> actual
);

// Runs a version-neutral recorded replay at caller speed. File formats,
// expected summaries, clocks, rendering, and I/O remain outside this layer.
std::expected<RunResult, Error> run_session(
	const ReplaySession &session,
	const GameContent &content
);

} // namespace th03::replay

#endif
