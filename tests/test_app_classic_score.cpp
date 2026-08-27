#include "th03/app/classic/persistence/classic_score.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace {

using th03::app::ClassicRegistrationGlyph;
using th03::app::ClassicScoreBoard;
using th03::app::ClassicScoreCandidate;
using th03::app::ClassicScoreCleared;
using th03::app::ClassicScoreCodecFields;
using th03::app::ClassicScoreDocument;
using th03::app::ClassicScoreErrorCode;
using th03::app::classic_score_cell_count;
using th03::app::classic_score_digit_count;
using th03::app::classic_score_file_bytes;
using th03::app::classic_score_name_length;
using th03::app::classic_score_place_count;
using th03::app::classic_score_rank_count;
using th03::app::classic_score_section_bytes;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

[[nodiscard]] constexpr std::uint8_t byte(std::byte value) noexcept
{
	return std::to_integer<std::uint8_t>(value);
}

[[nodiscard]] constexpr std::byte as_byte(std::uint8_t value) noexcept
{
	return static_cast<std::byte>(value);
}

[[nodiscard]] constexpr std::uint8_t add8(
	std::uint8_t a,
	std::uint8_t b,
	std::uint8_t c = 0U
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<std::uint16_t>(a) + b + c
	);
}

[[nodiscard]] constexpr std::uint8_t subtract8(
	std::uint8_t value,
	std::uint8_t a,
	std::uint8_t b
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<std::uint16_t>(value) -
		static_cast<std::uint16_t>(a) - b
	);
}

[[nodiscard]] std::array<std::byte, classic_score_section_bytes>
reference_decrypt(
	std::span<const std::byte, classic_score_section_bytes> encrypted
) noexcept
{
	auto plain = std::array<std::byte, classic_score_section_bytes>{};
	std::copy(encrypted.begin(), encrypted.end(), plain.begin());
	const auto key1 = byte(encrypted[204U]);
	const auto key2 = byte(encrypted[205U]);
	for(std::size_t index = 0U; index < 203U; ++index) {
		plain[index] = as_byte(add8(
			key1,
			static_cast<std::uint8_t>(
				std::rotr(byte(encrypted[index + 1U]), 3) ^ key2
			),
			byte(encrypted[index])
		));
	}
	plain[203U] = as_byte(add8(key1, key2, byte(encrypted[203U])));
	return plain;
}

[[nodiscard]] std::array<std::byte, classic_score_section_bytes>
reference_encrypt(
	std::array<std::byte, classic_score_section_bytes> plain
) noexcept
{
	auto encrypted = plain;
	const auto key1 = byte(plain[204U]);
	const auto key2 = byte(plain[205U]);
	auto feedback = key2;
	for(std::size_t remaining = 204U; remaining > 0U; --remaining) {
		const auto index = remaining - 1U;
		const auto cipher = subtract8(byte(plain[index]), key1, feedback);
		encrypted[index] = as_byte(cipher);
		feedback = static_cast<std::uint8_t>(std::rotr(cipher, 3) ^ key2);
	}
	return encrypted;
}

void reference_checksum(
	std::array<std::byte, classic_score_section_bytes> &plain
) noexcept
{
	std::uint16_t sum = 0U;
	for(std::size_t index = 2U; index < 206U; ++index) {
		sum = static_cast<std::uint16_t>(sum + byte(plain[index]));
	}
	plain[0U] = as_byte(static_cast<std::uint8_t>(sum & 0xFFU));
	plain[1U] = as_byte(static_cast<std::uint8_t>(sum >> 8U));
}

[[nodiscard]] std::array<std::byte, classic_score_file_bytes>
replace_plain_section(
	std::array<std::byte, classic_score_file_bytes> file,
	std::size_t rank,
	std::array<std::byte, classic_score_section_bytes> plain
) noexcept
{
	reference_checksum(plain);
	const auto encrypted = reference_encrypt(plain);
	std::copy(
		encrypted.begin(),
		encrypted.end(),
		file.begin() + static_cast<std::ptrdiff_t>(
			rank * classic_score_section_bytes
		)
	);
	return file;
}

[[nodiscard]] auto section(
	const std::array<std::byte, classic_score_file_bytes> &file,
	std::size_t rank
) noexcept
{
	return std::span<const std::byte, classic_score_section_bytes>{
		file.data() + rank * classic_score_section_bytes,
		classic_score_section_bytes,
	};
}

[[nodiscard]] std::array<ClassicScoreCodecFields, classic_score_rank_count>
codec_fields() noexcept
{
	return {{
		{.unknown = 0x10U, .key1 = 0x00U, .key2 = 0x00U},
		{.unknown = 0x11U, .key1 = 0x01U, .key2 = 0x02U},
		{.unknown = 0x12U, .key1 = 0x80U, .key2 = 0xFFU},
		{.unknown = 0x13U, .key1 = 0xFEU, .key2 = 0x55U},
	}};
}

[[nodiscard]] std::uint8_t glyph(ClassicRegistrationGlyph value) noexcept
{
	return static_cast<std::uint8_t>(value);
}

void test_constants_and_stock()
{
	expect(classic_score_section_bytes == 206U, "source section is 206 bytes");
	expect(classic_score_file_bytes == 824U, "four ranks occupy 824 bytes");
	expect(glyph(ClassicRegistrationGlyph::space) == 14U, "REGI_SP is 14");
	expect(glyph(ClassicRegistrationGlyph::backspace) == 30U, "REGI_BS is 30");
	expect(glyph(ClassicRegistrationGlyph::digit_0) == 32U, "REGI_0 is 32");
	expect(glyph(ClassicRegistrationGlyph::end) == 46U, "REGI_END is 46");
	expect(glyph(ClassicRegistrationGlyph::all) == 48U, "REGI_ALL is 48");
	expect(glyph(ClassicRegistrationGlyph::count) == 49U, "REGI count is 49");

	const auto stock = th03::app::classic_score_stock_file(codec_fields());
	expect(stock.codec == codec_fields(), "stock constructor preserves explicit codec fields");
	for(std::size_t rank = 0U; rank < stock.boards.size(); ++rank) {
		const auto &board = stock.boards[rank];
		expect(
			board.cleared == ClassicScoreCleared::not_cleared,
			"stock board starts uncleared"
		);
		for(std::size_t place = 0U; place < board.rows.size(); ++place) {
			const auto &row = board.rows[place];
			expect(std::all_of(
				row.name_backwards.begin(), row.name_backwards.end(),
				[](auto value) { return value == ClassicRegistrationGlyph::period; }
			), "stock names are dotted");
			expect(row.playchar_optional == 0U, "stock row is No Entry");
			expect(
				row.stage == ClassicRegistrationGlyph::digit_1,
				"stock row is Stage 1"
			);
			for(std::size_t cell = 0U; cell < row.score.size(); ++cell) {
				const auto expected = place == 0U && cell == 4U ?
					ClassicRegistrationGlyph::digit_1 :
					(place > 0U && cell == 3U ?
						static_cast<ClassicRegistrationGlyph>(
							glyph(ClassicRegistrationGlyph::digit_9) -
							static_cast<std::uint8_t>(place - 1U)
						) : ClassicRegistrationGlyph::digit_0);
				expect(row.score[cell] == expected, "stock score cell matches source decoy table");
			}
		}
	}
}

void test_codec_round_trip_and_layout()
{
	auto document = th03::app::classic_score_stock_file(codec_fields());
	document.boards[1U].cleared = ClassicScoreCleared::cleared;
	document.boards[2U].rows[4U].name_backwards = {{
		ClassicRegistrationGlyph::a,
		ClassicRegistrationGlyph::n,
		ClassicRegistrationGlyph::digit_9,
		ClassicRegistrationGlyph::space,
		ClassicRegistrationGlyph::question,
		ClassicRegistrationGlyph::z,
		ClassicRegistrationGlyph::comma,
		ClassicRegistrationGlyph::all,
	}};
	document.boards[2U].rows[4U].score = {{
		ClassicRegistrationGlyph::digit_3,
		ClassicRegistrationGlyph::digit_9,
		ClassicRegistrationGlyph::digit_8,
		ClassicRegistrationGlyph::digit_7,
		ClassicRegistrationGlyph::digit_6,
		ClassicRegistrationGlyph::digit_5,
		ClassicRegistrationGlyph::digit_4,
		ClassicRegistrationGlyph::digit_3,
		ClassicRegistrationGlyph::digit_2,
		ClassicRegistrationGlyph::digit_1,
	}};
	document.boards[2U].rows[4U].playchar_optional = 9U;
	document.boards[2U].rows[4U].stage = ClassicRegistrationGlyph::all;

	const auto encoded = th03::app::encode_classic_score_file(document);
	expect(encoded.has_value(), "valid score document encodes");
	if(!encoded) {
		return;
	}
	for(std::size_t rank = 0U; rank < classic_score_rank_count; ++rank) {
		const auto plain = reference_decrypt(section(*encoded, rank));
		const auto sum = static_cast<std::uint16_t>(
			byte(plain[0U]) |
			(static_cast<std::uint16_t>(byte(plain[1U])) << 8U)
		);
		std::uint16_t expected_sum = 0U;
		for(std::size_t index = 2U; index < 206U; ++index) {
			expected_sum = static_cast<std::uint16_t>(expected_sum + byte(plain[index]));
		}
		expect(sum == expected_sum, "checksum covers plaintext bytes 2..205");
		expect(byte(plain[83U]) == document.codec[rank].unknown, "unknown byte is offset 83");
		expect(byte(plain[204U]) == document.codec[rank].key1, "key1 remains plaintext at 204");
		expect(byte(plain[205U]) == document.codec[rank].key2, "key2 remains plaintext at 205");
		const auto reference = reference_encrypt(plain);
		expect(std::equal(
			reference.begin(), reference.end(), section(*encoded, rank).begin()
		), "production encryption matches independent backward reference");
	}
	const auto decoded = th03::app::decode_classic_score_file(*encoded);
	expect(decoded.has_value(), "encoded score file decodes");
	expect(decoded && *decoded == document, "decode reproduces every logical and codec field");
	const auto reencoded = decoded ? th03::app::encode_classic_score_file(*decoded) :
		decltype(encoded){std::unexpected(decoded.error())};
	expect(reencoded.has_value(), "decoded score file re-encodes");
	expect(reencoded && *reencoded == *encoded, "decode and recovered keys round-trip every byte");
}

void test_decode_errors_and_atomicity()
{
	const auto document = th03::app::classic_score_stock_file(codec_fields());
	const auto encoded_result = th03::app::encode_classic_score_file(document);
	expect(encoded_result.has_value(), "error fixture encodes");
	if(!encoded_result) {
		return;
	}
	const auto encoded = *encoded_result;
	expect(
		!th03::app::decode_classic_score_file(
			std::span<const std::byte>{encoded}.first(encoded.size() - 1U)
		) && th03::app::decode_classic_score_file(
			std::span<const std::byte>{encoded}.first(encoded.size() - 1U)
		).error().code == ClassicScoreErrorCode::input_size,
		"short input is rejected"
	);
	std::array<std::byte, classic_score_file_bytes + 1U> long_input{};
	std::copy(encoded.begin(), encoded.end(), long_input.begin());
	expect(
		!th03::app::decode_classic_score_file(long_input) &&
		th03::app::decode_classic_score_file(long_input).error().code ==
			ClassicScoreErrorCode::input_size,
		"long input is rejected"
	);

	for(const auto absolute : {0U, 206U + 100U, 3U * 206U + 203U}) {
		auto corrupt = encoded;
		corrupt[absolute] ^= std::byte{0x01U};
		const auto decoded = th03::app::decode_classic_score_file(corrupt);
		expect(
			!decoded && decoded.error().code == ClassicScoreErrorCode::checksum_mismatch &&
			decoded.error().rank == (absolute / 206U),
			"ciphertext tamper is rejected by the affected section checksum"
		);
	}

	struct DomainCase final {
		std::size_t rank;
		std::size_t offset;
		std::uint8_t value;
		ClassicScoreErrorCode code;
	};
	const std::array cases{
		DomainCase{0U, 82U, 17U, ClassicScoreErrorCode::invalid_cleared},
		DomainCase{1U, 2U + 9U * 8U + 7U, 49U, ClassicScoreErrorCode::invalid_name_glyph},
		DomainCase{2U, 84U + 9U * 10U + 9U, 31U, ClassicScoreErrorCode::invalid_score_glyph},
		DomainCase{3U, 184U + 9U, 10U, ClassicScoreErrorCode::invalid_playchar},
		DomainCase{3U, 194U + 9U, 32U, ClassicScoreErrorCode::invalid_stage},
	};
	for(const auto &test : cases) {
		auto plain = reference_decrypt(section(encoded, test.rank));
		plain[test.offset] = as_byte(test.value);
		const auto corrupt = replace_plain_section(encoded, test.rank, plain);
		const auto decoded = th03::app::decode_classic_score_file(corrupt);
		expect(
			!decoded && decoded.error().code == test.code &&
			decoded.error().rank == test.rank,
			"checksum-valid unsafe source domain is rejected with a named error"
		);
	}

	auto invalid = document;
	invalid.boards[3U].rows[9U].stage = static_cast<ClassicRegistrationGlyph>(255U);
	const auto before = invalid;
	const auto rejected = th03::app::encode_classic_score_file(invalid);
	expect(
		!rejected && rejected.error().code == ClassicScoreErrorCode::invalid_stage &&
		rejected.error().rank == 3U && rejected.error().place == 9U,
		"late invalid document is rejected before publication"
	);
	expect(invalid == before, "failed encode cannot mutate its document");
}

[[nodiscard]] ClassicScoreCandidate candidate_with_digit(
	std::size_t digit,
	std::uint8_t value,
	std::uint8_t continues = 0U
) noexcept
{
	ClassicScoreCandidate result{};
	result.digits[digit] = value;
	result.continues_used = continues;
	result.stage = 5U;
	result.playchar = 8U;
	return result;
}

void test_source_insertion()
{
	auto board = th03::app::classic_score_stock_file().boards[0U];
	const auto old_first = board.rows[0U];
	board.rows[0U].score[9U] = ClassicRegistrationGlyph::digit_7;
	const auto inserted = th03::app::classic_score_insert(
		board, candidate_with_digit(4U, 1U, 3U)
	);
	expect(inserted && *inserted == std::optional<std::size_t>{0U}, "greater score takes first place");
	expect(board.rows[1U].name_backwards == old_first.name_backwards, "insertion shifts old first row down");
	expect(
		board.rows[0U].score[9U] == ClassicRegistrationGlyph::digit_7,
		"source leaves the inserted row's dead score cell unchanged"
	);
	expect(std::all_of(
		board.rows[0U].name_backwards.begin(), board.rows[0U].name_backwards.end(),
		[](auto value) { return value == ClassicRegistrationGlyph::space; }
	), "new score name is blanked with REGI_SP");
	expect(board.rows[0U].score[0U] == ClassicRegistrationGlyph::digit_3, "continues digit is stored in cell 0");
	expect(board.rows[0U].score[5U] == ClassicRegistrationGlyph::digit_1, "score digits occupy cells 1..8");
	expect(board.rows[0U].playchar_optional == 9U, "source stores base character plus one");
	expect(board.rows[0U].stage == ClassicRegistrationGlyph::digit_5, "source stores stage as REGI digit");

	auto ties = th03::app::classic_score_stock_file().boards[0U];
	const auto tie0 = th03::app::classic_score_insert(
		ties, candidate_with_digit(3U, 1U, 0U)
	);
	expect(tie0 && *tie0 == std::optional<std::size_t>{1U}, "exact tie sinks beneath existing equal row");
	auto ties_with_continue = th03::app::classic_score_stock_file().boards[0U];
	const auto tie3 = th03::app::classic_score_insert(
		ties_with_continue, candidate_with_digit(3U, 1U, 3U)
	);
	expect(tie3 && *tie3 == std::optional<std::size_t>{1U}, "continues are ignored by source comparator");

	auto tenth = th03::app::classic_score_stock_file().boards[0U];
	const auto tenth_tie = th03::app::classic_score_insert(
		tenth, candidate_with_digit(2U, 1U)
	);
	expect(tenth_tie && !*tenth_tie, "tie with tenth place is dropped");

	auto clear_stage = th03::app::classic_score_stock_file().boards[0U];
	auto all = candidate_with_digit(4U, 1U);
	all.stage = 99U;
	const auto all_result = th03::app::classic_score_insert(clear_stage, all);
	expect(all_result && *all_result == std::optional<std::size_t>{0U}, "STAGE_ALL candidate inserts");
	expect(clear_stage.rows[0U].stage == ClassicRegistrationGlyph::all, "STAGE_ALL maps to REGI_ALL");

	auto invalid_board = th03::app::classic_score_stock_file().boards[0U];
	const auto before = invalid_board;
	auto invalid_candidate = candidate_with_digit(0U, 10U);
	const auto invalid_result = th03::app::classic_score_insert(
		invalid_board, invalid_candidate
	);
	expect(
		!invalid_result && invalid_result.error().code == ClassicScoreErrorCode::invalid_candidate,
		"invalid candidate is rejected"
	);
	expect(invalid_board == before, "invalid candidate leaves board atomic");

	const std::array invalid_candidates{
		[] {
			auto value = candidate_with_digit(0U, 1U);
			value.continues_used = 4U;
			return value;
		}(),
		[] {
			auto value = candidate_with_digit(0U, 1U);
			value.stage = 0U;
			return value;
		}(),
		[] {
			auto value = candidate_with_digit(0U, 1U);
			value.stage = 10U;
			return value;
		}(),
		[] {
			auto value = candidate_with_digit(0U, 1U);
			value.playchar = 9U;
			return value;
		}(),
	};
	for(const auto &candidate : invalid_candidates) {
		auto atomic = th03::app::classic_score_stock_file().boards[0U];
		const auto atomic_before = atomic;
		const auto result = th03::app::classic_score_insert(atomic, candidate);
		expect(
			!result && result.error().code == ClassicScoreErrorCode::invalid_candidate,
			"every non-source candidate domain is rejected"
		);
		expect(atomic == atomic_before, "candidate-domain rejection is atomic");
	}

	auto malformed_board = th03::app::classic_score_stock_file().boards[0U];
	malformed_board.rows[9U].score[9U] = ClassicRegistrationGlyph::backspace;
	const auto malformed_before = malformed_board;
	const auto malformed = th03::app::classic_score_insert(
		malformed_board, candidate_with_digit(4U, 1U)
	);
	expect(
		!malformed && malformed.error().code == ClassicScoreErrorCode::invalid_score_glyph &&
		malformed.error().place == 9U && malformed.error().cell == 9U &&
		malformed.error().offset == classic_score_file_bytes,
		"insertion validates the existing board before mutation"
	);
	expect(malformed_board == malformed_before, "invalid existing board remains unchanged");
}

void test_uniform_name_replacement()
{
	auto board = th03::app::classic_score_stock_file().boards[0U];
	const std::array<ClassicRegistrationGlyph, classic_score_name_length> name{{
		ClassicRegistrationGlyph::r,
		ClassicRegistrationGlyph::e,
		ClassicRegistrationGlyph::i,
		ClassicRegistrationGlyph::m,
		ClassicRegistrationGlyph::u,
		ClassicRegistrationGlyph::period,
		ClassicRegistrationGlyph::space,
		ClassicRegistrationGlyph::question,
	}};
	const auto replaced = th03::app::classic_score_replace_uniform_name(
		board, 0U, name
	);
	expect(replaced && *replaced, "uniform dotted stock name is replaced");
	for(std::size_t index = 0U; index < name.size(); ++index) {
		expect(
			board.rows[0U].name_backwards[index] == name[name.size() - 1U - index],
			"default name is reversed into source storage order"
		);
	}

	board.rows[1U].name_backwards.fill(ClassicRegistrationGlyph::a);
	const auto repeated = th03::app::classic_score_replace_uniform_name(
		board, 1U, name
	);
	expect(repeated && *repeated, "any all-identical name is replaced");

	board.rows[2U].name_backwards.fill(ClassicRegistrationGlyph::space);
	board.rows[2U].name_backwards[3U] = ClassicRegistrationGlyph::b;
	const auto mixed_before = board.rows[2U];
	const auto mixed = th03::app::classic_score_replace_uniform_name(
		board, 2U, name
	);
	expect(mixed && !*mixed, "mixed name is retained");
	expect(board.rows[2U] == mixed_before, "nonuniform name replacement is a no-op");

	const auto before = board;
	const auto bad_place = th03::app::classic_score_replace_uniform_name(
		board, classic_score_place_count, name
	);
	expect(
		!bad_place && bad_place.error().code == ClassicScoreErrorCode::invalid_place,
		"invalid name place is rejected"
	);
	expect(board == before, "invalid name place leaves board atomic");

	auto invalid_name = name;
	invalid_name[7U] = static_cast<ClassicRegistrationGlyph>(49U);
	const auto bad_default = th03::app::classic_score_replace_uniform_name(
		board, 0U, invalid_name
	);
	expect(
		!bad_default && bad_default.error().code == ClassicScoreErrorCode::invalid_default_name,
		"invalid default-name glyph is rejected"
	);
	expect(board == before, "invalid default name leaves board atomic");
}

void optional_local_round_trip(const char *path)
{
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		expect(false, "local score file opens");
		return;
	}
	const std::vector<char> raw{
		std::istreambuf_iterator<char>{input},
		std::istreambuf_iterator<char>{},
	};
	expect(raw.size() == classic_score_file_bytes, "local score file has exact size");
	if(raw.size() != classic_score_file_bytes) {
		return;
	}
	std::array<std::byte, classic_score_file_bytes> bytes{};
	for(std::size_t index = 0U; index < bytes.size(); ++index) {
		bytes[index] = as_byte(static_cast<std::uint8_t>(
			static_cast<unsigned char>(raw[index])
		));
	}
	const auto decoded = th03::app::decode_classic_score_file(bytes);
	expect(decoded.has_value(), "authenticated local score file decodes");
	const auto encoded = decoded ? th03::app::encode_classic_score_file(*decoded) :
		decltype(th03::app::encode_classic_score_file(*decoded)){
			std::unexpected(decoded.error())
		};
	expect(encoded.has_value(), "authenticated local score file re-encodes");
	expect(encoded && *encoded == bytes, "authenticated local score file round-trips byte-exactly");
}

} // namespace

int main(int argc, char **argv)
{
	test_constants_and_stock();
	test_codec_round_trip_and_layout();
	test_decode_errors_and_atomicity();
	test_source_insertion();
	test_uniform_name_replacement();
	if(argc == 2) {
		optional_local_round_trip(argv[1]);
	} else if(argc != 1) {
		expect(false, "usage accepts zero or one local score-file path");
	}

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
