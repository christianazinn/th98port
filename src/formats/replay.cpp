#include "th03/formats/replay.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include "bytes.hpp"
#include "th03/formats/v13.hpp"

namespace th03::formats {

ReplayRoundSplit make_round_split(
	const CanonicalState &state,
	bool practice
) noexcept
{
	ReplayRoundSplit split{};
	split.story_stage = (state.game_mode == game_mode::story || practice)
		? state.story_stage : 0x0FU;
	split.round_id = state.round_id;
	split.route = state.route;
	split.winner = state.winner;
	const std::array scores{state.p1_score, state.p2_score};
	std::array<std::array<std::uint8_t, 4>, 2> packed{};
	for(std::size_t player = 0U; player < scores.size(); ++player) {
		for(std::size_t byte = 0U; byte < packed[player].size(); ++byte) {
			const auto low = static_cast<std::uint8_t>(
				scores[player][byte * 2U] % 10U
			);
			const auto high = static_cast<std::uint8_t>(
				scores[player][(byte * 2U) + 1U] % 10U
			);
			packed[player][byte] = static_cast<std::uint8_t>(
				low | (high << 4U)
			);
		}
	}
	split.p1_score = packed[0];
	split.p2_score = packed[1];
	return split;
}

bool round_splits_match(
	const ReplayRoundSplit &expected,
	const ReplayRoundSplit &actual
) noexcept
{
	return expected.story_stage == actual.story_stage &&
		expected.round_id == actual.round_id &&
		expected.route == actual.route &&
		expected.winner == actual.winner &&
		expected.p1_score == actual.p1_score &&
		expected.p2_score == actual.p2_score;
}

bool replay_can_be_verified(const ReplayDocument &document) noexcept
{
	return !document.metadata ||
		document.metadata->status == ReplayStatus::finalized;
}

std::expected<ReplayDocument, Error> parse_replay(
	std::span<const std::byte> bytes
)
{
	if(bytes.size() < 8U) {
		return std::unexpected(Error{
			ErrorCode::truncated, "replay is shorter than its magic", 0U
		});
	}
	constexpr std::array<std::uint8_t, 8> v13_magic{
		'T', '3', 'R', 'P', 'L', 'Y', '1', '3',
	};
	if(!detail::has_magic(bytes, v13_magic)) {
		return std::unexpected(Error{
			ErrorCode::bad_magic, "replay magic is not T3RPLY13", 0U
		});
	}
	return parse_v13_replay(bytes);
}

} // namespace th03::formats
