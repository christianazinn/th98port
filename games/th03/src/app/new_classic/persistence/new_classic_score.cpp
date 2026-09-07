#include "th03/app/new_classic/persistence/new_classic_score.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace th03::app {
namespace {

constexpr std::array<std::byte, 8U> magic{{
	std::byte{'T'}, std::byte{'3'}, std::byte{'N'}, std::byte{'E'},
	std::byte{'M'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
}};
constexpr std::uint16_t version = 1U;
constexpr std::uint8_t extra_unlocked_flag = 0x01U;
constexpr std::uint8_t entry_valid_flag = 0x01U;

[[nodiscard]] NewClassicScoreError score_error(
	NewClassicScoreErrorCode code,
	std::size_t rank = new_classic_score_rank_count,
	std::size_t character = new_classic_score_character_count,
	std::size_t place = new_classic_score_place_count,
	std::size_t cell = new_classic_score_digit_count,
	std::size_t offset = new_classic_score_file_bytes
) noexcept
{
	return {code, rank, character, place, cell, offset};
}

[[nodiscard]] constexpr std::uint8_t byte_at(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return std::to_integer<std::uint8_t>(bytes[offset]);
}

[[nodiscard]] constexpr std::uint16_t read_u16(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(byte_at(bytes, offset)) |
		static_cast<std::uint16_t>(byte_at(bytes, offset + 1U) << 8U);
}

[[nodiscard]] constexpr std::uint32_t read_u32(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return static_cast<std::uint32_t>(byte_at(bytes, offset)) |
		(static_cast<std::uint32_t>(byte_at(bytes, offset + 1U)) << 8U) |
		(static_cast<std::uint32_t>(byte_at(bytes, offset + 2U)) << 16U) |
		(static_cast<std::uint32_t>(byte_at(bytes, offset + 3U)) << 24U);
}

constexpr void write_u16(
	std::span<std::byte> bytes,
	std::size_t offset,
	std::uint16_t value
) noexcept
{
	bytes[offset] = static_cast<std::byte>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

constexpr void write_u32(
	std::span<std::byte> bytes,
	std::size_t offset,
	std::uint32_t value
) noexcept
{
	bytes[offset] = static_cast<std::byte>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xFFU);
	bytes[offset + 2U] = static_cast<std::byte>((value >> 16U) & 0xFFU);
	bytes[offset + 3U] = static_cast<std::byte>((value >> 24U) & 0xFFU);
}

[[nodiscard]] constexpr std::uint32_t checksum(
	std::span<const std::byte> bytes
) noexcept
{
	std::uint16_t a = 0xA5A5U;
	std::uint16_t b = 0x5A5AU;
	for(const auto value : bytes) {
		a = static_cast<std::uint16_t>(a + std::to_integer<std::uint8_t>(value));
		b = static_cast<std::uint16_t>(b + a);
	}
	return (static_cast<std::uint32_t>(b) << 16U) | a;
}

[[nodiscard]] constexpr int compare_entries(
	const NewClassicScoreEntry &lhs,
	const NewClassicScoreEntry &rhs
) noexcept
{
	for(std::size_t digit = new_classic_score_digit_count; digit > 0U; --digit) {
		if(lhs.score[digit - 1U] > rhs.score[digit - 1U]) {
			return 1;
		}
		if(lhs.score[digit - 1U] < rhs.score[digit - 1U]) {
			return -1;
		}
	}
	return 0;
}

[[nodiscard]] NewClassicScoreBoard empty_board() noexcept
{
	NewClassicScoreBoard board{};
	for(auto &entry : board.entries) {
		entry.name.fill(' ');
		entry.stage = new_classic_score_empty_stage;
	}
	return board;
}

[[nodiscard]] std::expected<void, NewClassicScoreError> validate_board(
	const NewClassicScoreBoard &board,
	std::size_t rank,
	std::size_t character,
	std::size_t board_offset
) noexcept
{
	const NewClassicScoreEntry *previous = nullptr;
	bool saw_empty = false;
	for(std::size_t place = 0U; place < board.entries.size(); ++place) {
		const auto &entry = board.entries[place];
		const auto entry_offset = board_offset +
			(place * new_classic_score_entry_bytes);
		if(!entry.valid) {
			saw_empty = true;
			continue;
		}
		if(saw_empty) {
			return std::unexpected(score_error(
				NewClassicScoreErrorCode::invalid_entry_order,
				rank, character, place, new_classic_score_digit_count,
				entry_offset + 18U
			));
		}
		for(std::size_t c = 0U; c < entry.name.size(); ++c) {
			const auto value = static_cast<unsigned char>(entry.name[c]);
			if(value < 0x20U || value > 0x7EU) {
				return std::unexpected(score_error(
					NewClassicScoreErrorCode::invalid_name,
					rank, character, place, c, entry_offset + c
				));
			}
		}
		for(std::size_t digit = 0U; digit < entry.score.size(); ++digit) {
			if(entry.score[digit] > 9U) {
				return std::unexpected(score_error(
					NewClassicScoreErrorCode::invalid_score_digit,
					rank, character, place, digit, entry_offset + 8U + digit
				));
			}
		}
		if((entry.stage < 1U || entry.stage > 9U) && entry.stage != 99U) {
			return std::unexpected(score_error(
				NewClassicScoreErrorCode::invalid_stage,
				rank, character, place, new_classic_score_digit_count,
				entry_offset + 16U
			));
		}
		if(entry.continues_used > 9U) {
			return std::unexpected(score_error(
				NewClassicScoreErrorCode::invalid_continues,
				rank, character, place, new_classic_score_digit_count,
				entry_offset + 17U
			));
		}
		if(previous && compare_entries(entry, *previous) > 0) {
			return std::unexpected(score_error(
				NewClassicScoreErrorCode::invalid_entry_order,
				rank, character, place, new_classic_score_digit_count,
				entry_offset
			));
		}
		previous = &entry;
	}
	return {};
}

[[nodiscard]] constexpr std::optional<char> glyph_ascii(
	ClassicRegistrationGlyph glyph
) noexcept
{
	const auto raw = static_cast<std::uint8_t>(glyph);
	if(raw <= static_cast<std::uint8_t>(ClassicRegistrationGlyph::m)) {
		return static_cast<char>('A' + raw);
	}
	if(glyph >= ClassicRegistrationGlyph::n &&
		glyph <= ClassicRegistrationGlyph::z) {
		return static_cast<char>('N' +
			(raw - static_cast<std::uint8_t>(ClassicRegistrationGlyph::n)));
	}
	if(glyph >= ClassicRegistrationGlyph::digit_0 &&
		glyph <= ClassicRegistrationGlyph::digit_9) {
		return static_cast<char>('0' +
			(raw - static_cast<std::uint8_t>(ClassicRegistrationGlyph::digit_0)));
	}
	switch(glyph) {
	case ClassicRegistrationGlyph::period: return '.';
	case ClassicRegistrationGlyph::comma: return ',';
	case ClassicRegistrationGlyph::exclamation: return '!';
	case ClassicRegistrationGlyph::question: return '?';
	case ClassicRegistrationGlyph::space:
	case ClassicRegistrationGlyph::space_last:
	case ClassicRegistrationGlyph::blank_1:
	case ClassicRegistrationGlyph::blank_2:
		return ' ';
	default:
		return std::nullopt;
	}
}

[[nodiscard]] constexpr ClassicRegistrationGlyph ascii_glyph(char value) noexcept
{
	if(value >= 'a' && value <= 'z') {
		value = static_cast<char>('A' + (value - 'a'));
	}
	if(value >= 'A' && value <= 'M') {
		return static_cast<ClassicRegistrationGlyph>(value - 'A');
	}
	if(value >= 'N' && value <= 'Z') {
		return static_cast<ClassicRegistrationGlyph>(
			static_cast<std::uint8_t>(ClassicRegistrationGlyph::n) +
			static_cast<std::uint8_t>(value - 'N')
		);
	}
	if(value >= '0' && value <= '9') {
		return static_cast<ClassicRegistrationGlyph>(
			static_cast<std::uint8_t>(ClassicRegistrationGlyph::digit_0) +
			static_cast<std::uint8_t>(value - '0')
		);
	}
	switch(value) {
	case '.': return ClassicRegistrationGlyph::period;
	case ',': return ClassicRegistrationGlyph::comma;
	case '!': return ClassicRegistrationGlyph::exclamation;
	case '?': return ClassicRegistrationGlyph::question;
	default: return ClassicRegistrationGlyph::space;
	}
}

[[nodiscard]] ClassicScoreRow view_row(
	const NewClassicScoreEntry &entry,
	std::uint8_t character
) noexcept
{
	ClassicScoreRow row{};
	row.name_backwards.fill(ClassicRegistrationGlyph::space);
	row.score.fill(ClassicRegistrationGlyph::digit_0);
	for(std::size_t c = 0U; c < entry.name.size(); ++c) {
		row.name_backwards[(entry.name.size() - 1U) - c] =
			ascii_glyph(entry.name[c]);
	}
	for(std::size_t digit = 0U; digit < entry.score.size(); ++digit) {
		row.score[digit + 1U] = *classic_registration_digit(entry.score[digit]);
	}
	row.score[0U] = *classic_registration_digit(entry.continues_used);
	row.playchar_optional = static_cast<std::uint8_t>(character + 1U);
	row.stage = entry.stage == 99U ? ClassicRegistrationGlyph::all :
		*classic_registration_digit(entry.stage);
	return row;
}

void set_total_row(
	NewClassicScoreView &view,
	const std::array<std::uint8_t, classic_score_cell_count> &digits
) noexcept
{
	constexpr std::array<char, classic_score_name_length> total{{
		'T', 'O', 'T', 'A', 'L', ' ', ' ', ' ',
	}};
	auto &row = view.board.rows.back();
	for(std::size_t c = 0U; c < total.size(); ++c) {
		row.name_backwards[(total.size() - 1U) - c] = ascii_glyph(total[c]);
	}
	for(std::size_t digit = 0U; digit < digits.size(); ++digit) {
		row.score[digit] = *classic_registration_digit(digits[digit]);
	}
	row.playchar_optional = 0U;
	row.stage = ClassicRegistrationGlyph::digit_1;
	view.valid.back() = true;
	view.total.back() = true;
}

} // namespace

NewClassicScoreDocument new_classic_score_empty() noexcept
{
	NewClassicScoreDocument document{};
	for(auto &rank : document.boards) {
		for(auto &board : rank) {
			board = empty_board();
		}
	}
	return document;
}

std::expected<NewClassicScoreDecodeResult, NewClassicScoreError>
decode_new_classic_score_file(
	std::span<const std::byte> bytes,
	NewClassicScoreBoardPolicy policy
) noexcept
{
	if(bytes.size() != new_classic_score_file_bytes) {
		return std::unexpected(score_error(NewClassicScoreErrorCode::input_size));
	}
	if(!std::equal(magic.begin(), magic.end(), bytes.begin()) ||
		read_u16(bytes, 8U) != version ||
		read_u16(bytes, 10U) != new_classic_score_header_bytes ||
		read_u32(bytes, 12U) != new_classic_score_file_bytes ||
		read_u16(bytes, 24U) != new_classic_score_entry_bytes ||
		read_u16(bytes, 26U) != new_classic_score_board_bytes ||
		byte_at(bytes, 28U) != new_classic_score_rank_count ||
		byte_at(bytes, 29U) != new_classic_score_character_count ||
		byte_at(bytes, 30U) != new_classic_score_place_count) {
		return std::unexpected(score_error(
			NewClassicScoreErrorCode::header_shape,
			new_classic_score_rank_count, new_classic_score_character_count,
			new_classic_score_place_count, new_classic_score_digit_count, 0U
		));
	}
	const auto flags = byte_at(bytes, 31U);
	if((flags & ~extra_unlocked_flag) != 0U) {
		return std::unexpected(score_error(
			NewClassicScoreErrorCode::header_flags,
			new_classic_score_rank_count, new_classic_score_character_count,
			new_classic_score_place_count, new_classic_score_digit_count, 31U
		));
	}
	if(read_u32(bytes, 32U) != checksum(bytes.first(32U))) {
		return std::unexpected(score_error(
			NewClassicScoreErrorCode::header_checksum,
			new_classic_score_rank_count, new_classic_score_character_count,
			new_classic_score_place_count, new_classic_score_digit_count, 32U
		));
	}

	NewClassicScoreDecodeResult result{};
	result.document = new_classic_score_empty();
	result.document.generation = read_u32(bytes, 16U);
	result.document.last_run_id = read_u32(bytes, 20U);
	result.document.extra_unlocked = (flags & extra_unlocked_flag) != 0U;
	for(std::size_t rank = 0U; rank < new_classic_score_rank_count; ++rank) {
		for(std::size_t character = 0U;
			character < new_classic_score_character_count;
			++character) {
			const auto board_index =
				(rank * new_classic_score_character_count) + character;
			const auto board_offset = new_classic_score_header_bytes +
				(board_index * new_classic_score_board_bytes);
			auto &board = result.document.boards[rank][character];
			for(std::size_t place = 0U; place < board.entries.size(); ++place) {
				const auto entry_offset = board_offset +
					(place * new_classic_score_entry_bytes);
				auto &entry = board.entries[place];
				for(std::size_t c = 0U; c < entry.name.size(); ++c) {
					entry.name[c] = static_cast<char>(byte_at(bytes, entry_offset + c));
				}
				for(std::size_t digit = 0U; digit < entry.score.size(); ++digit) {
					entry.score[digit] = byte_at(bytes, entry_offset + 8U + digit);
				}
				entry.stage = byte_at(bytes, entry_offset + 16U);
				entry.continues_used = byte_at(bytes, entry_offset + 17U);
				const auto entry_flags = byte_at(bytes, entry_offset + 18U);
				const auto reserved = byte_at(bytes, entry_offset + 19U);
				if(reserved != 0U || (entry_flags & ~entry_valid_flag) != 0U) {
					if(policy == NewClassicScoreBoardPolicy::replace_invalid) {
						result.repaired_boards[rank][character] = true;
						board = empty_board();
						break;
					}
					return std::unexpected(score_error(
						NewClassicScoreErrorCode::invalid_entry_flags,
						rank, character, place, new_classic_score_digit_count,
						entry_offset + 18U
					));
				}
				entry.valid = (entry_flags & entry_valid_flag) != 0U;
			}
			if(result.repaired_boards[rank][character]) {
				continue;
			}
			board.stats.play_frames = read_u32(bytes, board_offset + 200U);
			board.stats.one_ccs = read_u32(bytes, board_offset + 204U);
			board.stats.continues_used = read_u32(bytes, board_offset + 208U);
			if(read_u32(bytes, board_offset + 212U) != checksum(
				bytes.subspan(board_offset, 212U)
			)) {
				if(policy == NewClassicScoreBoardPolicy::replace_invalid) {
					result.repaired_boards[rank][character] = true;
					board = empty_board();
					continue;
				}
				return std::unexpected(score_error(
					NewClassicScoreErrorCode::board_checksum,
					rank, character, new_classic_score_place_count,
					new_classic_score_digit_count, board_offset + 212U
				));
			}
			if(auto valid = validate_board(board, rank, character, board_offset);
				!valid) {
				if(policy == NewClassicScoreBoardPolicy::replace_invalid) {
					result.repaired_boards[rank][character] = true;
					board = empty_board();
					continue;
				}
				return std::unexpected(valid.error());
			}
		}
	}
	return result;
}

std::expected<
	std::array<std::byte, new_classic_score_file_bytes>,
	NewClassicScoreError
> encode_new_classic_score_file(const NewClassicScoreDocument &document) noexcept
{
	auto bytes = std::array<std::byte, new_classic_score_file_bytes>{};
	std::copy(magic.begin(), magic.end(), bytes.begin());
	write_u16(bytes, 8U, version);
	write_u16(bytes, 10U, new_classic_score_header_bytes);
	write_u32(bytes, 12U, new_classic_score_file_bytes);
	write_u32(bytes, 16U, document.generation);
	write_u32(bytes, 20U, document.last_run_id);
	write_u16(bytes, 24U, new_classic_score_entry_bytes);
	write_u16(bytes, 26U, new_classic_score_board_bytes);
	bytes[28U] = static_cast<std::byte>(new_classic_score_rank_count);
	bytes[29U] = static_cast<std::byte>(new_classic_score_character_count);
	bytes[30U] = static_cast<std::byte>(new_classic_score_place_count);
	bytes[31U] = static_cast<std::byte>(
		document.extra_unlocked ? extra_unlocked_flag : 0U
	);
	write_u32(bytes, 32U, checksum(std::span<const std::byte>(bytes).first(32U)));

	for(std::size_t rank = 0U; rank < new_classic_score_rank_count; ++rank) {
		for(std::size_t character = 0U;
			character < new_classic_score_character_count;
			++character) {
			const auto board_index =
				(rank * new_classic_score_character_count) + character;
			const auto board_offset = new_classic_score_header_bytes +
				(board_index * new_classic_score_board_bytes);
			const auto &board = document.boards[rank][character];
			if(auto valid = validate_board(board, rank, character, board_offset);
				!valid) {
				return std::unexpected(valid.error());
			}
			for(std::size_t place = 0U; place < board.entries.size(); ++place) {
				const auto entry_offset = board_offset +
					(place * new_classic_score_entry_bytes);
				const auto &entry = board.entries[place];
				for(std::size_t c = 0U; c < entry.name.size(); ++c) {
					bytes[entry_offset + c] = static_cast<std::byte>(
						static_cast<unsigned char>(entry.name[c])
					);
				}
				for(std::size_t digit = 0U; digit < entry.score.size(); ++digit) {
					bytes[entry_offset + 8U + digit] =
						static_cast<std::byte>(entry.score[digit]);
				}
				bytes[entry_offset + 16U] = static_cast<std::byte>(entry.stage);
				bytes[entry_offset + 17U] =
					static_cast<std::byte>(entry.continues_used);
				bytes[entry_offset + 18U] = static_cast<std::byte>(
					entry.valid ? entry_valid_flag : 0U
				);
				bytes[entry_offset + 19U] = std::byte{0U};
			}
			write_u32(bytes, board_offset + 200U, board.stats.play_frames);
			write_u32(bytes, board_offset + 204U, board.stats.one_ccs);
			write_u32(bytes, board_offset + 208U, board.stats.continues_used);
			write_u32(
				bytes, board_offset + 212U,
				checksum(std::span<const std::byte>(bytes).subspan(board_offset, 212U))
			);
		}
	}
	return bytes;
}

std::expected<NewClassicScoreDocument, NewClassicScoreError>
migrate_classic_score_document(const ClassicScoreDocument &legacy) noexcept
{
	auto result = new_classic_score_empty();
	for(std::size_t rank = 0U; rank < legacy.boards.size(); ++rank) {
		result.extra_unlocked = result.extra_unlocked ||
			legacy.boards[rank].cleared == ClassicScoreCleared::cleared;
		for(const auto &row : legacy.boards[rank].rows) {
			if(row.playchar_optional == 0U ||
				row.playchar_optional > new_classic_score_character_count) {
				continue;
			}
			const auto character = static_cast<std::size_t>(
				row.playchar_optional - 1U
			);
			auto &entries = result.boards[rank][character].entries;
			auto destination = std::find_if(
				entries.begin(), entries.end(),
				[](const auto &entry) { return !entry.valid; }
			);
			if(destination == entries.end()) {
				continue;
			}
			NewClassicScoreEntry entry{};
			entry.name.fill(' ');
			for(std::size_t c = 0U; c < entry.name.size(); ++c) {
				const auto glyph = row.name_backwards[(entry.name.size() - 1U) - c];
				const auto ascii = glyph_ascii(glyph);
				if(!ascii) {
					return std::unexpected(score_error(
						NewClassicScoreErrorCode::invalid_name_glyph,
						rank, character,
						static_cast<std::size_t>(destination - entries.begin()), c
					));
				}
				entry.name[c] = *ascii;
			}
			for(std::size_t digit = 0U; digit < entry.score.size(); ++digit) {
				const auto glyph = row.score[digit + 1U];
				if(glyph < ClassicRegistrationGlyph::digit_0 ||
					glyph > ClassicRegistrationGlyph::digit_9) {
					return std::unexpected(score_error(
						NewClassicScoreErrorCode::invalid_score_digit,
						rank, character,
						static_cast<std::size_t>(destination - entries.begin()), digit
					));
				}
				entry.score[digit] = static_cast<std::uint8_t>(glyph) -
					static_cast<std::uint8_t>(ClassicRegistrationGlyph::digit_0);
			}
			entry.stage = row.stage == ClassicRegistrationGlyph::all ?
				std::uint8_t{99U} : static_cast<std::uint8_t>(
					static_cast<std::uint8_t>(row.stage) -
					static_cast<std::uint8_t>(ClassicRegistrationGlyph::digit_0)
				);
			entry.continues_used =
				static_cast<std::uint8_t>(row.score[0U]) -
				static_cast<std::uint8_t>(ClassicRegistrationGlyph::digit_0);
			entry.valid = true;
			*destination = entry;
		}
		for(auto &board : result.boards[rank]) {
			std::stable_sort(
				board.entries.begin(), board.entries.end(),
				[](const auto &lhs, const auto &rhs) {
					if(lhs.valid != rhs.valid) {
						return lhs.valid;
					}
					return lhs.valid && compare_entries(lhs, rhs) > 0;
				}
			);
		}
	}
	return result;
}

std::expected<std::optional<std::size_t>, NewClassicScoreError>
new_classic_score_insert(
	NewClassicScoreDocument &document,
	std::uint8_t rank,
	std::uint8_t character,
	const ClassicScoreCandidate &candidate
) noexcept
{
	if(rank >= new_classic_score_rank_count) {
		return std::unexpected(score_error(NewClassicScoreErrorCode::invalid_rank));
	}
	if(character >= new_classic_score_character_count ||
		candidate.playchar != character) {
		return std::unexpected(score_error(
			NewClassicScoreErrorCode::invalid_character, rank, character
		));
	}
	if(candidate.continues_used > 9U ||
		((candidate.stage < 1U || candidate.stage > 9U) &&
		 candidate.stage != 99U) ||
		std::ranges::any_of(candidate.digits, [](auto digit) { return digit > 9U; })) {
		return std::unexpected(score_error(
			NewClassicScoreErrorCode::invalid_candidate, rank, character
		));
	}
	NewClassicScoreEntry entry{};
	entry.name.fill(' ');
	entry.score = candidate.digits;
	entry.stage = candidate.stage;
	entry.continues_used = candidate.continues_used;
	entry.valid = true;
	auto &entries = document.boards[rank][character].entries;
	std::size_t place = 0U;
	while(place < entries.size() && entries[place].valid &&
		compare_entries(entry, entries[place]) <= 0) {
		++place;
	}
	if(place == entries.size()) {
		return std::optional<std::size_t>{};
	}
	for(std::size_t destination = entries.size() - 1U;
		destination > place;
		--destination) {
		entries[destination] = entries[destination - 1U];
	}
	entries[place] = entry;
	return std::optional<std::size_t>{place};
}

std::expected<void, NewClassicScoreError> new_classic_score_set_name(
	NewClassicScoreDocument &document,
	std::uint8_t rank,
	std::uint8_t character,
	std::size_t place,
	std::span<const ClassicRegistrationGlyph, classic_score_name_length>
		name_forward
) noexcept
{
	if(rank >= new_classic_score_rank_count) {
		return std::unexpected(score_error(NewClassicScoreErrorCode::invalid_rank));
	}
	if(character >= new_classic_score_character_count) {
		return std::unexpected(score_error(
			NewClassicScoreErrorCode::invalid_character, rank, character
		));
	}
	if(place >= new_classic_score_place_count ||
		!document.boards[rank][character].entries[place].valid) {
		return std::unexpected(score_error(
			NewClassicScoreErrorCode::invalid_place, rank, character, place
		));
	}
	auto &entry = document.boards[rank][character].entries[place];
	for(std::size_t c = 0U; c < name_forward.size(); ++c) {
		const auto ascii = glyph_ascii(name_forward[c]);
		if(!ascii) {
			return std::unexpected(score_error(
				NewClassicScoreErrorCode::invalid_name_glyph,
				rank, character, place, c
			));
		}
		entry.name[c] = *ascii;
	}
	return {};
}

std::expected<bool, NewClassicScoreError> new_classic_score_apply_run(
	NewClassicScoreDocument &document,
	std::uint8_t rank,
	std::uint8_t character,
	NewClassicScoreRun run
) noexcept
{
	if(rank >= new_classic_score_rank_count) {
		return std::unexpected(score_error(NewClassicScoreErrorCode::invalid_rank));
	}
	if(character >= new_classic_score_character_count) {
		return std::unexpected(score_error(
			NewClassicScoreErrorCode::invalid_character, rank, character
		));
	}
	if(run.run_id == 0U) {
		return std::unexpected(score_error(
			NewClassicScoreErrorCode::invalid_run, rank, character
		));
	}
	if(run.run_id <= document.last_run_id) {
		return false;
	}
	auto &stats = document.boards[rank][character].stats;
	stats.play_frames = new_classic_score_add_saturating(
		stats.play_frames, run.play_frames
	);
	stats.continues_used = new_classic_score_add_saturating(
		stats.continues_used, run.continues_used
	);
	if(run.one_cc) {
		stats.one_ccs = new_classic_score_add_saturating(stats.one_ccs, 1U);
		document.extra_unlocked = true;
	}
	document.last_run_id = run.run_id;
	return true;
}

std::expected<NewClassicScoreView, NewClassicScoreError> new_classic_score_view(
	const NewClassicScoreDocument &document,
	std::uint8_t rank,
	std::uint8_t page
) noexcept
{
	if(rank >= new_classic_score_rank_count) {
		return std::unexpected(score_error(NewClassicScoreErrorCode::invalid_rank));
	}
	const auto visible = document.extra_unlocked ?
		new_classic_score_character_count : new_classic_score_locked_character_count;
	if(page != new_classic_score_all_characters && page >= visible) {
		return std::unexpected(score_error(
			NewClassicScoreErrorCode::invalid_page, rank, page
		));
	}
	NewClassicScoreView view{};
	view.board = classic_score_stock_file().boards[rank];
	view.board.cleared = document.extra_unlocked ?
		ClassicScoreCleared::cleared : ClassicScoreCleared::not_cleared;
	view.page = page;
	if(page != new_classic_score_all_characters) {
		const auto &source = document.boards[rank][page];
		view.stats = source.stats;
		for(std::size_t place = 0U; place < source.entries.size(); ++place) {
			if(!source.entries[place].valid) {
				break;
			}
			view.board.rows[place] = view_row(source.entries[place], page);
			view.valid[place] = true;
		}
		return view;
	}

	struct Best final {
		std::uint8_t character = 0U;
		const NewClassicScoreEntry *entry = nullptr;
	};
	std::array<Best, new_classic_score_character_count> best{};
	std::size_t best_count = 0U;
	std::array<std::uint8_t, classic_score_cell_count> total_digits{};
	for(std::uint8_t character = 0U; character < visible; ++character) {
		const auto &board = document.boards[rank][character];
		view.stats.play_frames = new_classic_score_add_saturating(
			view.stats.play_frames, board.stats.play_frames
		);
		view.stats.one_ccs = new_classic_score_add_saturating(
			view.stats.one_ccs, board.stats.one_ccs
		);
		view.stats.continues_used = new_classic_score_add_saturating(
			view.stats.continues_used, board.stats.continues_used
		);
		if(!board.entries[0U].valid) {
			continue;
		}
		best[best_count++] = {character, &board.entries[0U]};
		std::uint8_t carry = 0U;
		for(std::size_t digit = 1U; digit < total_digits.size(); ++digit) {
			const auto add = digit <= new_classic_score_digit_count ?
				board.entries[0U].score[digit - 1U] : 0U;
			const auto sum = static_cast<std::uint8_t>(
				total_digits[digit] + add + carry
			);
			total_digits[digit] = static_cast<std::uint8_t>(sum % 10U);
			carry = static_cast<std::uint8_t>(sum / 10U);
		}
		const auto continues = static_cast<std::uint8_t>(
			total_digits[0U] + board.entries[0U].continues_used
		);
		total_digits[0U] = std::min<std::uint8_t>(continues, 9U);
	}
	std::sort(best.begin(), best.begin() + static_cast<std::ptrdiff_t>(best_count),
		[](const Best &lhs, const Best &rhs) {
			const auto comparison = compare_entries(*lhs.entry, *rhs.entry);
			return comparison != 0 ? comparison > 0 : lhs.character < rhs.character;
		});
	for(std::size_t place = 0U; place < best_count; ++place) {
		view.board.rows[place] = view_row(
			*best[place].entry, best[place].character
		);
		view.valid[place] = true;
	}
	set_total_row(view, total_digits);
	return view;
}

} // namespace th03::app
