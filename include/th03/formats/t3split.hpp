#ifndef TH03_FORMATS_T3SPLIT_HPP
#define TH03_FORMATS_T3SPLIT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "th03/error.hpp"

namespace th03::formats {

inline constexpr std::size_t t3split_header_size = 16;
inline constexpr std::size_t t3split_row_size = 34;

enum class SplitEvent : std::uint8_t {
	start = 1,
	round_start = 2,
	input_end = 3,
	error = 4,
	checkpoint = 5,
	finish = 6,
	route = 7,
};

struct PackedScore {
	std::array<std::uint8_t, 4> bytes{};
};

struct SplitRow {
	SplitEvent event = SplitEvent::start;
	std::uint8_t route = 0;
	std::uint8_t game_mode = 0;
	std::uint8_t story_stage = 0;
	std::uint8_t round_id = 0;
	std::int8_t winner = 0;
	std::uint8_t round_speed = 0;
	std::uint32_t global_frame = 0;
	std::uint32_t round_frame = 0;
	std::uint16_t round_or_result_frame = 0;
	PackedScore p1_score{};
	PackedScore p2_score{};
	std::int32_t resident_rand = 0;
	std::uint32_t state_hash = 0;
};

struct SplitStream {
	std::uint16_t state_hash_schema = 0;
	std::vector<SplitRow> rows;
};

enum class ComparisonStatus {
	same,
	same_prefix,
	different,
};

struct SplitMismatch {
	std::size_t canonical_row = 0;
	std::string field;
	std::string expected;
	std::string actual;
};

struct TerminalHashDifference {
	std::size_t canonical_row = 0;
	SplitEvent event = SplitEvent::route;
	std::uint8_t story_stage = 0;
	std::uint8_t round_id = 0;
	std::uint32_t expected = 0;
	std::uint32_t actual = 0;
};

struct SplitComparison {
	ComparisonStatus status = ComparisonStatus::same;
	std::size_t base_canonical_rows = 0;
	std::size_t candidate_canonical_rows = 0;
	std::size_t compared_canonical_rows = 0;
	std::optional<int> checkpoint_global_delta;
	std::vector<TerminalHashDifference> terminal_hash_differences;
	std::optional<SplitMismatch> first_mismatch;
};

std::expected<SplitStream, Error> parse_t3split(std::span<const std::byte> bytes);
std::expected<std::vector<std::byte>, Error> write_t3split(const SplitStream &stream);
std::expected<std::string, Error> unpack_score(PackedScore score);
SplitComparison compare_oracle_splits(
	const SplitStream &base,
	const SplitStream &candidate,
	bool allow_candidate_prefix = false
);

} // namespace th03::formats

#endif
