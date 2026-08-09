#ifndef TH03_FORMATS_REPLAY_HPP
#define TH03_FORMATS_REPLAY_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "th03/error.hpp"
#include "th03/replay/session.hpp"

namespace th03::formats {

struct ReplayRoundSplit {
	std::uint8_t story_stage = 0;
	std::uint8_t round_id = 0;
	std::uint8_t route = 0;
	std::int8_t winner = -1;
	std::array<std::uint8_t, 4> p1_score{};
	std::array<std::uint8_t, 4> p2_score{};
};

struct ReplayDocument {
	replay::ReplaySession session{};
	bool practice = false;
	std::vector<ReplayRoundSplit> expected_round_splits;
};

ReplayRoundSplit make_round_split(
	const CanonicalState &state,
	bool practice
) noexcept;
bool round_splits_match(
	const ReplayRoundSplit &expected,
	const ReplayRoundSplit &actual
) noexcept;

// Version-neutral historical adapter. V13 can be added here without changing
// the deterministic core or verifier runner.
std::expected<ReplayDocument, Error> parse_replay(
	std::span<const std::byte> bytes
);

} // namespace th03::formats

#endif
