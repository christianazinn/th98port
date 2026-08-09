#include "th03/formats/replay.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "bytes.hpp"
#include "th03/formats/v11.hpp"
#include "th03/formats/v12.hpp"

namespace th03::formats {
namespace {

Error error(ErrorCode code, std::string message, std::size_t offset = 0U)
{
	return Error{code, std::move(message), offset};
}

bool packed_score_valid(const std::array<std::uint8_t, 4> &score)
{
	for(const auto byte : score) {
		if((byte & 0x0FU) > 9U || ((byte >> 4U) & 0x0FU) > 9U) {
			return false;
		}
	}
	return true;
}

std::expected<std::vector<ReplayRoundSplit>, Error> parse_round_splits(
	std::span<const std::byte> bytes
)
{
	const auto count = detail::u8(bytes, 0x81U);
	if(count > 27U) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "replay round-split count exceeds 27", 0x81U
		));
	}
	std::vector<ReplayRoundSplit> result;
	result.reserve(count);
	for(std::size_t index = 0U; index < count; ++index) {
		const auto offset = 0x82U + (index * 10U);
		const auto stage_round = detail::u8(bytes, offset + 0U);
		const auto route_winner = detail::u8(bytes, offset + 1U);
		const auto stage = static_cast<std::uint8_t>(stage_round & 0x0FU);
		const auto winner = static_cast<std::uint8_t>(route_winner & 0x0FU);
		ReplayRoundSplit split{};
		split.story_stage = stage;
		split.round_id = static_cast<std::uint8_t>(stage_round >> 4U);
		split.route = static_cast<std::uint8_t>(route_winner >> 4U);
		split.winner = (winner == 0x0FU) ? -1 : static_cast<std::int8_t>(winner);
		for(std::size_t byte = 0U; byte < 4U; ++byte) {
			split.p1_score[byte] = detail::u8(bytes, offset + 2U + byte);
			split.p2_score[byte] = detail::u8(bytes, offset + 6U + byte);
		}
		if(
			(stage > 8U && stage != 0x0FU) || split.route > 2U ||
			(winner > 1U && winner != 0x0FU) ||
			!packed_score_valid(split.p1_score) ||
			!packed_score_valid(split.p2_score)
		) {
			return std::unexpected(error(
				ErrorCode::invalid_value, "replay contains an invalid round split", offset
			));
		}
		result.push_back(split);
	}
	return result;
}

} // namespace

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

std::expected<ReplayDocument, Error> parse_replay(
	std::span<const std::byte> bytes
)
{
	if(bytes.size() < 8U) {
		return std::unexpected(error(
			ErrorCode::truncated, "replay is shorter than its magic"
		));
	}
	constexpr std::array<std::uint8_t, 8> v11_magic{
		'T', '3', 'R', 'P', 'L', 'Y', '1', '1',
	};
	constexpr std::array<std::uint8_t, 8> v12_magic{
		'T', '3', 'R', 'P', 'L', 'Y', '1', '2',
	};
	ReplayDocument document{};
	if(detail::has_magic(bytes, v11_magic)) {
		auto session = parse_v11_replay(bytes);
		if(!session) {
			return std::unexpected(session.error());
		}
		document.session = std::move(*session);
	} else if(detail::has_magic(bytes, v12_magic)) {
		auto session = parse_v12_replay(bytes);
		if(!session) {
			return std::unexpected(session.error());
		}
		document.session = std::move(*session);
		document.practice = (detail::u16(bytes, 14U) & 0x0004U) != 0U;
	} else {
		return std::unexpected(error(
			ErrorCode::bad_magic, "replay magic is not a supported T3RPLY version"
		));
	}
	auto splits = parse_round_splits(bytes);
	if(!splits) {
		return std::unexpected(splits.error());
	}
	document.expected_round_splits = std::move(*splits);
	return document;
}

} // namespace th03::formats
