#include "th03/app/classic/persistence/classic_score.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <utility>

namespace th03::app {
namespace {

constexpr std::size_t sum_offset = 0U;
constexpr std::size_t names_offset = 2U;
constexpr std::size_t cleared_offset = 82U;
constexpr std::size_t unknown_offset = 83U;
constexpr std::size_t scores_offset = 84U;
constexpr std::size_t playchars_offset = 184U;
constexpr std::size_t stages_offset = 194U;
constexpr std::size_t key1_offset = 204U;
constexpr std::size_t key2_offset = 205U;
constexpr std::size_t cipher_bytes = 204U;
constexpr std::size_t checksum_offset = 2U;
constexpr std::size_t checksum_bytes = 204U;

[[nodiscard]] constexpr ClassicScoreError error(
	ClassicScoreErrorCode code,
	std::size_t rank = classic_score_rank_count,
	std::size_t place = classic_score_place_count,
	std::size_t cell = classic_score_cell_count,
	std::size_t offset = classic_score_file_bytes
) noexcept
{
	return {
		.code = code,
		.rank = rank,
		.place = place,
		.cell = cell,
		.offset = offset,
	};
}

[[nodiscard]] constexpr std::uint8_t byte(std::byte value) noexcept
{
	return std::to_integer<std::uint8_t>(value);
}

[[nodiscard]] constexpr std::byte as_byte(std::uint8_t value) noexcept
{
	return static_cast<std::byte>(value);
}

[[nodiscard]] constexpr std::size_t absolute_offset(
	std::size_t rank,
	std::size_t section_offset
) noexcept
{
	return rank < classic_score_rank_count ?
		rank * classic_score_section_bytes + section_offset :
		classic_score_file_bytes;
}

[[nodiscard]] constexpr std::uint8_t add8(
	std::uint8_t first,
	std::uint8_t second,
	std::uint8_t third = 0U
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<std::uint16_t>(first) + second + third
	);
}

[[nodiscard]] constexpr std::uint8_t subtract8(
	std::uint8_t value,
	std::uint8_t first,
	std::uint8_t second
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<std::uint16_t>(value) -
		static_cast<std::uint16_t>(first) - second
	);
}

[[nodiscard]] constexpr bool score_glyph_valid(
	ClassicRegistrationGlyph glyph
) noexcept
{
	const auto value = static_cast<std::uint8_t>(glyph);
	return value >= static_cast<std::uint8_t>(
		ClassicRegistrationGlyph::digit_0
	) && value <= static_cast<std::uint8_t>(
		ClassicRegistrationGlyph::digit_9
	);
}

[[nodiscard]] constexpr bool stage_glyph_valid(
	ClassicRegistrationGlyph glyph
) noexcept
{
	const auto value = static_cast<std::uint8_t>(glyph);
	return (value >= static_cast<std::uint8_t>(
		ClassicRegistrationGlyph::digit_1
	) && value <= static_cast<std::uint8_t>(
		ClassicRegistrationGlyph::digit_9
	)) || glyph == ClassicRegistrationGlyph::all;
}

[[nodiscard]] std::expected<void, ClassicScoreError> validate_board(
	const ClassicScoreBoard &board,
	std::size_t rank
) noexcept
{
	if(board.cleared != ClassicScoreCleared::not_cleared &&
		board.cleared != ClassicScoreCleared::cleared) {
		return std::unexpected(error(
			ClassicScoreErrorCode::invalid_cleared,
			rank,
			classic_score_place_count,
			classic_score_cell_count,
			absolute_offset(rank, cleared_offset)
		));
	}
	for(std::size_t place = 0U; place < board.rows.size(); ++place) {
		const auto &row = board.rows[place];
		for(std::size_t cell = 0U; cell < row.name_backwards.size(); ++cell) {
			if(!classic_registration_glyph_valid(row.name_backwards[cell])) {
				return std::unexpected(error(
					ClassicScoreErrorCode::invalid_name_glyph,
					rank,
					place,
					cell,
					absolute_offset(
						rank,
						names_offset + place * classic_score_name_length + cell
					)
				));
			}
		}
		for(std::size_t cell = 0U; cell < row.score.size(); ++cell) {
			if(!score_glyph_valid(row.score[cell])) {
				return std::unexpected(error(
					ClassicScoreErrorCode::invalid_score_glyph,
					rank,
					place,
					cell,
					absolute_offset(
						rank,
						scores_offset + place * classic_score_cell_count + cell
					)
				));
			}
		}
		if(row.playchar_optional > 9U) {
			return std::unexpected(error(
				ClassicScoreErrorCode::invalid_playchar,
				rank,
				place,
				classic_score_cell_count,
				absolute_offset(rank, playchars_offset + place)
			));
		}
		if(!stage_glyph_valid(row.stage)) {
			return std::unexpected(error(
				ClassicScoreErrorCode::invalid_stage,
				rank,
				place,
				classic_score_cell_count,
				absolute_offset(rank, stages_offset + place)
			));
		}
	}
	return {};
}

[[nodiscard]] std::array<std::byte, classic_score_section_bytes>
decrypt_section(
	std::span<const std::byte, classic_score_section_bytes> encrypted
) noexcept
{
	auto plain = std::array<std::byte, classic_score_section_bytes>{};
	std::copy(encrypted.begin(), encrypted.end(), plain.begin());
	const auto key1 = byte(encrypted[key1_offset]);
	const auto key2 = byte(encrypted[key2_offset]);
	for(std::size_t index = 0U; index < (cipher_bytes - 1U); ++index) {
		const auto feedback = static_cast<std::uint8_t>(
			std::rotr(byte(encrypted[index + 1U]), 3) ^ key2
		);
		plain[index] = as_byte(add8(key1, feedback, byte(encrypted[index])));
	}
	plain[cipher_bytes - 1U] = as_byte(add8(
		key1,
		key2,
		byte(encrypted[cipher_bytes - 1U])
	));
	return plain;
}

[[nodiscard]] std::uint16_t section_checksum(
	std::span<const std::byte, classic_score_section_bytes> plain
) noexcept
{
	std::uint16_t sum = 0U;
	for(std::size_t index = checksum_offset;
		index < (checksum_offset + checksum_bytes);
		++index) {
		sum = static_cast<std::uint16_t>(sum + byte(plain[index]));
	}
	return sum;
}

[[nodiscard]] std::expected<ClassicScoreBoard, ClassicScoreError>
parse_board(
	std::span<const std::byte, classic_score_section_bytes> plain,
	std::size_t rank
) noexcept
{
	ClassicScoreBoard board{};
	board.cleared = static_cast<ClassicScoreCleared>(byte(plain[cleared_offset]));
	for(std::size_t place = 0U; place < board.rows.size(); ++place) {
		auto &row = board.rows[place];
		for(std::size_t cell = 0U; cell < row.name_backwards.size(); ++cell) {
			row.name_backwards[cell] = static_cast<ClassicRegistrationGlyph>(
				byte(plain[names_offset + place * classic_score_name_length + cell])
			);
		}
		for(std::size_t cell = 0U; cell < row.score.size(); ++cell) {
			row.score[cell] = static_cast<ClassicRegistrationGlyph>(
				byte(plain[scores_offset + place * classic_score_cell_count + cell])
			);
		}
		row.playchar_optional = byte(plain[playchars_offset + place]);
		row.stage = static_cast<ClassicRegistrationGlyph>(
			byte(plain[stages_offset + place])
		);
	}
	if(const auto valid = validate_board(board, rank); !valid) {
		return std::unexpected(valid.error());
	}
	return board;
}

[[nodiscard]] std::expected<void, ClassicScoreError> validate_candidate(
	const ClassicScoreCandidate &candidate
) noexcept
{
	for(std::size_t index = 0U; index < candidate.digits.size(); ++index) {
		if(candidate.digits[index] > 9U) {
			return std::unexpected(error(
				ClassicScoreErrorCode::invalid_candidate,
				classic_score_rank_count,
				classic_score_place_count,
				index
			));
		}
	}
	if(candidate.continues_used > 3U || candidate.playchar > 8U ||
		!((candidate.stage >= 1U && candidate.stage <= 9U) ||
			candidate.stage == 99U)) {
		return std::unexpected(error(ClassicScoreErrorCode::invalid_candidate));
	}
	return {};
}

} // namespace

std::expected<ClassicScoreDocument, ClassicScoreError>
decode_classic_score_file(std::span<const std::byte> bytes) noexcept
{
	if(bytes.size() != classic_score_file_bytes) {
		return std::unexpected(error(ClassicScoreErrorCode::input_size));
	}

	ClassicScoreDocument result{};
	for(std::size_t rank = 0U; rank < classic_score_rank_count; ++rank) {
		const auto encrypted = std::span<
			const std::byte,
			classic_score_section_bytes
		>{bytes.subspan(rank * classic_score_section_bytes).data(),
			classic_score_section_bytes};
		const auto plain = decrypt_section(encrypted);
		const auto stored_sum = static_cast<std::uint16_t>(
			byte(plain[sum_offset]) |
			(static_cast<std::uint16_t>(byte(plain[sum_offset + 1U])) << 8U)
		);
		if(stored_sum != section_checksum(plain)) {
			return std::unexpected(error(
				ClassicScoreErrorCode::checksum_mismatch,
				rank,
				classic_score_place_count,
				classic_score_cell_count,
				rank * classic_score_section_bytes
			));
		}
		auto board = parse_board(plain, rank);
		if(!board) {
			return std::unexpected(board.error());
		}
		result.boards[rank] = std::move(*board);
		result.codec[rank] = {
			.unknown = byte(plain[unknown_offset]),
			.key1 = byte(plain[key1_offset]),
			.key2 = byte(plain[key2_offset]),
		};
	}
	return result;
}

std::expected<
	std::array<std::byte, classic_score_file_bytes>,
	ClassicScoreError
> encode_classic_score_file(const ClassicScoreDocument &document) noexcept
{
	std::array<std::byte, classic_score_file_bytes> result{};
	for(std::size_t rank = 0U; rank < classic_score_rank_count; ++rank) {
		const auto &board = document.boards[rank];
		if(const auto valid = validate_board(board, rank); !valid) {
			return std::unexpected(valid.error());
		}
		std::array<std::byte, classic_score_section_bytes> plain{};
		for(std::size_t place = 0U; place < board.rows.size(); ++place) {
			const auto &row = board.rows[place];
			for(std::size_t cell = 0U; cell < row.name_backwards.size(); ++cell) {
				plain[names_offset + place * classic_score_name_length + cell] =
					as_byte(static_cast<std::uint8_t>(row.name_backwards[cell]));
			}
			for(std::size_t cell = 0U; cell < row.score.size(); ++cell) {
				plain[scores_offset + place * classic_score_cell_count + cell] =
					as_byte(static_cast<std::uint8_t>(row.score[cell]));
			}
			plain[playchars_offset + place] = as_byte(row.playchar_optional);
			plain[stages_offset + place] = as_byte(
				static_cast<std::uint8_t>(row.stage)
			);
		}
		plain[cleared_offset] = as_byte(
			static_cast<std::uint8_t>(board.cleared)
		);
		plain[unknown_offset] = as_byte(document.codec[rank].unknown);
		plain[key1_offset] = as_byte(document.codec[rank].key1);
		plain[key2_offset] = as_byte(document.codec[rank].key2);
		const auto sum = section_checksum(plain);
		plain[sum_offset] = as_byte(static_cast<std::uint8_t>(sum & 0xFFU));
		plain[sum_offset + 1U] = as_byte(static_cast<std::uint8_t>(sum >> 8U));

		auto encrypted = plain;
		auto feedback = document.codec[rank].key2;
		for(std::size_t remaining = cipher_bytes; remaining > 0U; --remaining) {
			const auto index = remaining - 1U;
			const auto cipher = subtract8(
				byte(plain[index]), document.codec[rank].key1, feedback
			);
			encrypted[index] = as_byte(cipher);
			feedback = static_cast<std::uint8_t>(
				std::rotr(cipher, 3) ^ document.codec[rank].key2
			);
		}
		std::copy(
			encrypted.begin(),
			encrypted.end(),
			result.begin() + static_cast<std::ptrdiff_t>(
				rank * classic_score_section_bytes
			)
		);
	}
	return result;
}

ClassicScoreDocument classic_score_stock_file(
	std::array<ClassicScoreCodecFields, classic_score_rank_count> codec
) noexcept
{
	ClassicScoreDocument result{};
	result.codec = codec;
	for(auto &board : result.boards) {
		board.cleared = ClassicScoreCleared::not_cleared;
		for(auto &row : board.rows) {
			row.name_backwards.fill(ClassicRegistrationGlyph::period);
			row.score.fill(ClassicRegistrationGlyph::digit_0);
			row.playchar_optional = 0U;
			row.stage = ClassicRegistrationGlyph::digit_1;
		}
		board.rows[0U].score[4U] = ClassicRegistrationGlyph::digit_1;
		for(std::size_t place = 1U; place < board.rows.size(); ++place) {
			board.rows[place].score[3U] = static_cast<ClassicRegistrationGlyph>(
				static_cast<std::uint8_t>(ClassicRegistrationGlyph::digit_9) -
				static_cast<std::uint8_t>(place - 1U)
			);
		}
	}
	return result;
}

std::expected<std::optional<std::size_t>, ClassicScoreError>
classic_score_insert(
	ClassicScoreBoard &board,
	const ClassicScoreCandidate &candidate
) noexcept
{
	if(const auto valid = validate_board(board, classic_score_rank_count); !valid) {
		return std::unexpected(valid.error());
	}
	if(const auto valid = validate_candidate(candidate); !valid) {
		return std::unexpected(valid.error());
	}

	std::size_t place = 0U;
	for(; place < board.rows.size(); ++place) {
		bool lower = false;
		bool greater = false;
		for(std::size_t remaining = classic_score_digit_count;
			remaining > 0U;
			--remaining) {
			const auto digit_index = remaining - 1U;
			const auto existing = static_cast<std::uint8_t>(
				board.rows[place].score[digit_index + 1U]
			) - static_cast<std::uint8_t>(ClassicRegistrationGlyph::digit_0);
			if(candidate.digits[digit_index] > existing) {
				greater = true;
				break;
			}
			if(candidate.digits[digit_index] < existing) {
				lower = true;
				break;
			}
		}
		if(greater) {
			break;
		}
		if(lower) {
			continue;
		}
		// Exact ties intentionally continue to the next place.
	}
	if(place == board.rows.size()) {
		return std::optional<std::size_t>{};
	}

	for(std::size_t shift = board.rows.size() - 1U; shift > place; --shift) {
		board.rows[shift] = board.rows[shift - 1U];
	}
	auto &row = board.rows[place];
	row.name_backwards.fill(ClassicRegistrationGlyph::space);
	for(std::size_t index = 0U; index < candidate.digits.size(); ++index) {
		row.score[index + 1U] = *classic_registration_digit(
			candidate.digits[index]
		);
	}
	row.score[0U] = *classic_registration_digit(candidate.continues_used);
	row.stage = candidate.stage == 99U ? ClassicRegistrationGlyph::all :
		*classic_registration_digit(candidate.stage);
	row.playchar_optional = static_cast<std::uint8_t>(candidate.playchar + 1U);
	return std::optional<std::size_t>{place};
}

std::expected<bool, ClassicScoreError>
classic_score_replace_uniform_name(
	ClassicScoreBoard &board,
	std::size_t place,
	std::span<const ClassicRegistrationGlyph, classic_score_name_length>
		default_name_forward
) noexcept
{
	if(place >= board.rows.size()) {
		return std::unexpected(error(
			ClassicScoreErrorCode::invalid_place,
			classic_score_rank_count,
			place
		));
	}
	for(std::size_t cell = 0U; cell < default_name_forward.size(); ++cell) {
		if(!classic_registration_glyph_valid(default_name_forward[cell])) {
			return std::unexpected(error(
				ClassicScoreErrorCode::invalid_default_name,
				classic_score_rank_count,
				place,
				cell
			));
		}
	}
	const auto &name = board.rows[place].name_backwards;
	for(std::size_t cell = 0U; cell < name.size(); ++cell) {
		if(!classic_registration_glyph_valid(name[cell])) {
			return std::unexpected(error(
				ClassicScoreErrorCode::invalid_name_glyph,
				classic_score_rank_count,
				place,
				cell
			));
		}
	}
	if(!std::all_of(name.begin(), name.end(), [first = name[0U]](auto glyph) {
		return glyph == first;
	})) {
		return false;
	}
	std::reverse_copy(
		default_name_forward.begin(),
		default_name_forward.end(),
		board.rows[place].name_backwards.begin()
	);
	return true;
}

} // namespace th03::app
