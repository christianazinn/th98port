#include "th03/formats/t3split.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "bytes.hpp"

namespace th03::formats {
namespace {

Error error(ErrorCode code, std::string message, std::size_t offset = 0)
{
	return Error{code, std::move(message), offset};
}

bool known_event(SplitEvent event)
{
	const auto value = static_cast<std::uint8_t>(event);
	return value >= static_cast<std::uint8_t>(SplitEvent::start) &&
		value <= static_cast<std::uint8_t>(SplitEvent::route);
}

std::string hex32(std::uint32_t value)
{
	std::ostringstream output;
	output << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
	return output.str();
}

template <typename T>
std::string decimal(T value)
{
	if constexpr(std::is_enum_v<T>) {
		return std::to_string(static_cast<std::underlying_type_t<T>>(value));
	} else if constexpr(std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::int8_t>) {
		return std::to_string(static_cast<int>(value));
	} else {
		return std::to_string(value);
	}
}

struct CanonicalRow {
	SplitRow row;
	SplitEvent event;
};

std::vector<CanonicalRow> canonical_rows(const SplitStream &stream)
{
	std::vector<CanonicalRow> result;
	std::set<std::pair<std::uint8_t, std::uint8_t>> starts;
	for(const auto &row : stream.rows) {
		auto event = row.event;
		if(event == SplitEvent::start || event == SplitEvent::round_start) {
			const auto key = std::pair{row.story_stage, row.round_id};
			if(starts.contains(key)) {
				continue;
			}
			starts.insert(key);
			event = SplitEvent::round_start;
		} else if(
			event != SplitEvent::checkpoint &&
			event != SplitEvent::route &&
			event != SplitEvent::finish
		) {
			continue;
		}
		result.push_back(CanonicalRow{row, event});
	}
	return result;
}

template <typename T>
bool mismatch(
	SplitComparison &comparison,
	std::size_t row,
	const char *field,
	T expected,
	T actual
)
{
	if(expected == actual) {
		return false;
	}
	comparison.first_mismatch = SplitMismatch{
		row,
		field,
		decimal(expected),
		decimal(actual),
	};
	comparison.status = ComparisonStatus::different;
	return true;
}

bool score_mismatch(
	SplitComparison &comparison,
	std::size_t row,
	const char *field,
	PackedScore expected,
	PackedScore actual
)
{
	if(expected.bytes == actual.bytes) {
		return false;
	}
	const auto expected_text = unpack_score(expected);
	const auto actual_text = unpack_score(actual);
	comparison.first_mismatch = SplitMismatch{
		row,
		field,
		expected_text ? *expected_text : "invalid",
		actual_text ? *actual_text : "invalid",
	};
	comparison.status = ComparisonStatus::different;
	return true;
}

} // namespace

std::expected<std::string, Error> unpack_score(PackedScore score)
{
	std::array<char, 8> digits{};
	for(std::size_t digit = 0; digit < digits.size(); ++digit) {
		const auto packed = score.bytes[digit / 2U];
		const auto value = static_cast<std::uint8_t>(
			(digit % 2U == 0U) ? (packed & 0x0FU) : ((packed >> 4U) & 0x0FU)
		);
		if(value > 9U) {
			return std::unexpected(error(ErrorCode::invalid_value, "packed score contains a non-decimal digit"));
		}
		digits[7U - digit] = static_cast<char>('0' + value);
	}
	return std::string(digits.begin(), digits.end());
}

std::expected<SplitStream, Error> parse_t3split(std::span<const std::byte> bytes)
{
	constexpr std::array<std::uint8_t, 8> magic{
		'T', '3', 'S', 'P', 'L', 'T', '1', 0,
	};
	if(bytes.size() < t3split_header_size) {
		return std::unexpected(error(ErrorCode::truncated, "T3SPLIT is shorter than its 16-byte header"));
	}
	if(!detail::has_magic(bytes, magic)) {
		return std::unexpected(error(ErrorCode::bad_magic, "T3SPLIT magic is not T3SPLT1"));
	}
	if(detail::u16(bytes, 8U) != 1U) {
		return std::unexpected(error(ErrorCode::unsupported_version, "unsupported T3SPLIT version", 8U));
	}
	if(
		detail::u16(bytes, 10U) != t3split_header_size ||
		detail::u16(bytes, 12U) != t3split_row_size
	) {
		return std::unexpected(error(ErrorCode::unsupported_layout, "unsupported T3SPLIT layout"));
	}
	const auto schema = detail::u16(bytes, 14U);
	if(schema != 0U && schema != 3U) {
		return std::unexpected(error(ErrorCode::unsupported_version, "unsupported T3SPLIT state-hash schema", 14U));
	}
	const auto payload_size = bytes.size() - t3split_header_size;
	if(payload_size % t3split_row_size != 0U) {
		return std::unexpected(error(ErrorCode::truncated, "T3SPLIT has a truncated trailing row", bytes.size() - (payload_size % t3split_row_size)));
	}

	SplitStream result{};
	result.state_hash_schema = schema;
	result.rows.reserve(payload_size / t3split_row_size);
	for(std::size_t offset = t3split_header_size; offset < bytes.size(); offset += t3split_row_size) {
		SplitRow row{};
		row.event = static_cast<SplitEvent>(detail::u8(bytes, offset));
		if(!known_event(row.event)) {
			return std::unexpected(error(ErrorCode::invalid_value, "unknown T3SPLIT event", offset));
		}
		row.route = detail::u8(bytes, offset + 1U);
		row.game_mode = detail::u8(bytes, offset + 2U);
		row.story_stage = detail::u8(bytes, offset + 3U);
		row.round_id = detail::u8(bytes, offset + 4U);
		row.winner = detail::i8(bytes, offset + 5U);
		row.round_speed = detail::u8(bytes, offset + 6U);
		if(detail::u8(bytes, offset + 7U) != 0U) {
			return std::unexpected(error(ErrorCode::invalid_value, "T3SPLIT row reserved byte is nonzero", offset + 7U));
		}
		row.global_frame = detail::u32(bytes, offset + 8U);
		row.round_frame = detail::u32(bytes, offset + 12U);
		row.round_or_result_frame = detail::u16(bytes, offset + 16U);
		for(std::size_t index = 0; index < 4U; ++index) {
			row.p1_score.bytes[index] = detail::u8(bytes, offset + 18U + index);
			row.p2_score.bytes[index] = detail::u8(bytes, offset + 22U + index);
		}
		if(!unpack_score(row.p1_score) || !unpack_score(row.p2_score)) {
			return std::unexpected(error(ErrorCode::invalid_value, "T3SPLIT row has an invalid packed score", offset + 18U));
		}
		row.resident_rand = detail::i32(bytes, offset + 26U);
		row.state_hash = detail::u32(bytes, offset + 30U);
		result.rows.push_back(row);
	}
	return result;
}

std::expected<std::vector<std::byte>, Error> write_t3split(const SplitStream &stream)
{
	if(stream.state_hash_schema != 0U && stream.state_hash_schema != 3U) {
		return std::unexpected(error(ErrorCode::unsupported_version, "unsupported T3SPLIT state-hash schema"));
	}
	if(stream.rows.size() >
		((std::numeric_limits<std::size_t>::max() - t3split_header_size) / t3split_row_size)
	) {
		return std::unexpected(error(ErrorCode::count_mismatch, "T3SPLIT row count overflows file size"));
	}
	for(const auto &row : stream.rows) {
		if(!known_event(row.event)) {
			return std::unexpected(error(ErrorCode::invalid_value, "cannot write unknown T3SPLIT event"));
		}
		if(!unpack_score(row.p1_score) || !unpack_score(row.p2_score)) {
			return std::unexpected(error(ErrorCode::invalid_value, "cannot write invalid packed score"));
		}
	}
	std::vector<std::byte> result(
		t3split_header_size + (stream.rows.size() * t3split_row_size),
		std::byte{0}
	);
	constexpr std::array<std::uint8_t, 8> magic{
		'T', '3', 'S', 'P', 'L', 'T', '1', 0,
	};
	for(std::size_t index = 0; index < magic.size(); ++index) {
		detail::put_u8(result, index, magic[index]);
	}
	detail::put_u16(result, 8U, 1U);
	detail::put_u16(result, 10U, static_cast<std::uint16_t>(t3split_header_size));
	detail::put_u16(result, 12U, static_cast<std::uint16_t>(t3split_row_size));
	detail::put_u16(result, 14U, stream.state_hash_schema);

	for(std::size_t row_index = 0; row_index < stream.rows.size(); ++row_index) {
		const auto &row = stream.rows[row_index];
		const auto offset = t3split_header_size + (row_index * t3split_row_size);
		detail::put_u8(result, offset, static_cast<std::uint8_t>(row.event));
		detail::put_u8(result, offset + 1U, row.route);
		detail::put_u8(result, offset + 2U, row.game_mode);
		detail::put_u8(result, offset + 3U, row.story_stage);
		detail::put_u8(result, offset + 4U, row.round_id);
		detail::put_u8(result, offset + 5U, std::bit_cast<std::uint8_t>(row.winner));
		detail::put_u8(result, offset + 6U, row.round_speed);
		detail::put_u32(result, offset + 8U, row.global_frame);
		detail::put_u32(result, offset + 12U, row.round_frame);
		detail::put_u16(result, offset + 16U, row.round_or_result_frame);
		for(std::size_t index = 0; index < 4U; ++index) {
			detail::put_u8(result, offset + 18U + index, row.p1_score.bytes[index]);
			detail::put_u8(result, offset + 22U + index, row.p2_score.bytes[index]);
		}
		detail::put_i32(result, offset + 26U, row.resident_rand);
		detail::put_u32(result, offset + 30U, row.state_hash);
	}
	return result;
}

SplitComparison compare_oracle_splits(
	const SplitStream &base,
	const SplitStream &candidate,
	bool allow_candidate_prefix
)
{
	SplitComparison result{};
	if(base.state_hash_schema != candidate.state_hash_schema) {
		result.status = ComparisonStatus::different;
		result.first_mismatch = SplitMismatch{
			0U,
			"state_hash_schema",
			decimal(base.state_hash_schema),
			decimal(candidate.state_hash_schema),
		};
		return result;
	}
	const auto base_rows = canonical_rows(base);
	const auto candidate_rows = canonical_rows(candidate);
	result.base_canonical_rows = base_rows.size();
	result.candidate_canonical_rows = candidate_rows.size();
	result.compared_canonical_rows = std::min(base_rows.size(), candidate_rows.size());

	for(std::size_t index = 0; index < result.compared_canonical_rows; ++index) {
		const auto row_number = index + 1U;
		const auto &expected = base_rows[index];
		const auto &actual = candidate_rows[index];
		if(
			mismatch(result, row_number, "canonical_event", expected.event, actual.event) ||
			mismatch(result, row_number, "round_frame", expected.row.round_frame, actual.row.round_frame) ||
			mismatch(result, row_number, "round_or_result_frame", expected.row.round_or_result_frame, actual.row.round_or_result_frame) ||
			mismatch(result, row_number, "route", expected.row.route, actual.row.route) ||
			mismatch(result, row_number, "game_mode", expected.row.game_mode, actual.row.game_mode) ||
			mismatch(result, row_number, "story_stage", expected.row.story_stage, actual.row.story_stage) ||
			mismatch(result, row_number, "round_id", expected.row.round_id, actual.row.round_id) ||
			mismatch(result, row_number, "winner", expected.row.winner, actual.row.winner) ||
			score_mismatch(result, row_number, "p1_score", expected.row.p1_score, actual.row.p1_score) ||
			score_mismatch(result, row_number, "p2_score", expected.row.p2_score, actual.row.p2_score) ||
			mismatch(result, row_number, "resident_rand", expected.row.resident_rand, actual.row.resident_rand) ||
			mismatch(result, row_number, "round_speed", expected.row.round_speed, actual.row.round_speed)
		) {
			return result;
		}

		if(expected.event == SplitEvent::round_start || expected.event == SplitEvent::checkpoint) {
			if(expected.row.state_hash != actual.row.state_hash) {
				result.status = ComparisonStatus::different;
				result.first_mismatch = SplitMismatch{
					row_number,
					"state_hash",
					hex32(expected.row.state_hash),
					hex32(actual.row.state_hash),
				};
				return result;
			}
		} else if(expected.row.state_hash != actual.row.state_hash) {
			result.terminal_hash_differences.push_back(TerminalHashDifference{
				row_number,
				expected.event,
				expected.row.story_stage,
				expected.row.round_id,
				expected.row.state_hash,
				actual.row.state_hash,
			});
		}

		const auto delta64 =
			static_cast<std::int64_t>(actual.row.global_frame) -
			static_cast<std::int64_t>(expected.row.global_frame);
		if(expected.event == SplitEvent::checkpoint) {
			if(delta64 < -1 || delta64 > 1) {
				result.status = ComparisonStatus::different;
				result.first_mismatch = SplitMismatch{
					row_number,
					"global_frame",
					decimal(expected.row.global_frame),
					decimal(actual.row.global_frame),
				};
				return result;
			}
			const auto delta = static_cast<int>(delta64);
			if(!result.checkpoint_global_delta) {
				result.checkpoint_global_delta = delta;
			} else if(*result.checkpoint_global_delta != delta) {
				result.status = ComparisonStatus::different;
				result.first_mismatch = SplitMismatch{
					row_number,
					"checkpoint_global_delta",
					decimal(*result.checkpoint_global_delta),
					decimal(delta),
				};
				return result;
			}
		} else if(delta64 != 0) {
			result.status = ComparisonStatus::different;
			result.first_mismatch = SplitMismatch{
				row_number,
				"global_frame",
				decimal(expected.row.global_frame),
				decimal(actual.row.global_frame),
			};
			return result;
		}
	}

	if(base_rows.size() != candidate_rows.size()) {
		if(allow_candidate_prefix && candidate_rows.size() < base_rows.size()) {
			result.status = ComparisonStatus::same_prefix;
		} else {
			result.status = ComparisonStatus::different;
			result.first_mismatch = SplitMismatch{
				result.compared_canonical_rows + 1U,
				"canonical_row_count",
				decimal(base_rows.size()),
				decimal(candidate_rows.size()),
			};
		}
	}
	return result;
}

} // namespace th03::formats
