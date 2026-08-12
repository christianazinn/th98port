#include "th03/app/new_classic_score.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *label)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << label << '\n';
	}
}

th03::app::ClassicScoreCandidate candidate(
	std::uint8_t character,
	std::uint8_t high_digit,
	std::uint8_t continues = 0U
)
{
	th03::app::ClassicScoreCandidate result{};
	result.digits[7U] = high_digit;
	result.continues_used = continues;
	result.stage = 99U;
	result.playchar = character;
	return result;
}

void test_empty_roundtrip_and_layout()
{
	using namespace th03::app;
	expect(new_classic_score_file_bytes == 7812U,
		"Replay Patch score document occupies 7,812 bytes");
	auto document = new_classic_score_empty();
	document.generation = 7U;
	document.last_run_id = 19U;
	document.extra_unlocked = true;
	auto encoded = encode_new_classic_score_file(document);
	expect(encoded.has_value(), "empty expanded score document encodes");
	if(!encoded) {
		return;
	}
	expect((*encoded)[0U] == std::byte{'T'} &&
		(*encoded)[7U] == std::byte{'1'} &&
		std::to_integer<std::uint8_t>((*encoded)[16U]) == 7U &&
		std::to_integer<std::uint8_t>((*encoded)[20U]) == 19U &&
		std::to_integer<std::uint8_t>((*encoded)[31U]) == 1U,
		"header stores magic, generation, run ID, and unlock flag at source offsets");
	auto decoded = decode_new_classic_score_file(*encoded);
	expect(decoded && decoded->document == document,
		"expanded score encode/decode is exact and typed");
	auto reencoded = decoded ? encode_new_classic_score_file(decoded->document) :
		decltype(encode_new_classic_score_file(decoded->document)){
			std::unexpected(NewClassicScoreError{})
		};
	expect(reencoded && *reencoded == *encoded,
		"expanded score bytes are canonical-idempotent");
}

void test_domain_and_board_recovery()
{
	using namespace th03::app;
	auto document = new_classic_score_empty();
	auto inserted = new_classic_score_insert(document, 2U, 4U,
		candidate(4U, 7U, 2U));
	expect(inserted && *inserted == 0U,
		"valid candidate enters its rank/character board");
	auto encoded = encode_new_classic_score_file(document);
	expect(encoded.has_value(), "populated expanded score document encodes");
	if(!encoded) {
		return;
	}
	constexpr std::size_t board = new_classic_score_header_bytes +
		((2U * new_classic_score_character_count + 4U) *
		 new_classic_score_board_bytes);
	(*encoded)[board + 8U] = std::byte{10U};
	// Preserve the board checksum mismatch as the first observable defect.
	auto strict = decode_new_classic_score_file(*encoded);
	expect(!strict && strict.error().code ==
		NewClassicScoreErrorCode::board_checksum,
		"strict decode rejects a damaged board before publishing fields");
	auto recovered = decode_new_classic_score_file(
		*encoded, NewClassicScoreBoardPolicy::replace_invalid
	);
	expect(recovered && recovered->repaired_boards[2U][4U] &&
		!recovered->document.boards[2U][4U].entries[0U].valid &&
		recovered->document.boards[0U][0U] == document.boards[0U][0U],
		"recovery replaces only the damaged rank/character board");

	auto bad_header = *encoded;
	bad_header[0U] = std::byte{'X'};
	auto header = decode_new_classic_score_file(
		bad_header, NewClassicScoreBoardPolicy::replace_invalid
	);
	expect(!header && header.error().code ==
		NewClassicScoreErrorCode::header_shape,
		"board recovery never accepts an invalid versioned header");
}

void test_insertion_migration_and_views()
{
	using namespace th03::app;
	auto document = new_classic_score_empty();
	for(std::uint8_t character = 0U; character < 7U; ++character) {
		auto first = new_classic_score_insert(
			document, 1U, character,
			candidate(character, static_cast<std::uint8_t>(character + 1U), character)
		);
		expect(first && *first == 0U, "first character score takes first place");
		document.boards[1U][character].stats = {
			.play_frames = static_cast<std::uint32_t>(282U * character),
			.one_ccs = character,
			.continues_used = static_cast<std::uint32_t>(character * 2U),
		};
	}
	auto tie = new_classic_score_insert(document, 1U, 0U, candidate(0U, 1U));
	expect(tie && *tie == 1U,
		"exact score ties sink beneath existing equal entries");
	std::array<ClassicRegistrationGlyph, classic_score_name_length> name{{
		ClassicRegistrationGlyph::p, ClassicRegistrationGlyph::l,
		ClassicRegistrationGlyph::a, ClassicRegistrationGlyph::y,
		ClassicRegistrationGlyph::e, ClassicRegistrationGlyph::r,
		ClassicRegistrationGlyph::space, ClassicRegistrationGlyph::space,
	}};
	expect(new_classic_score_set_name(document, 1U, 0U, 0U, name).has_value() &&
		document.boards[1U][0U].entries[0U].name[0U] == 'P',
		"registration glyph names become forward ASCII in expanded storage");

	auto all = new_classic_score_view(
		document, 1U, new_classic_score_all_characters
	);
	expect(all && all->valid[0U] && all->valid[6U] && all->total[9U] &&
		all->board.rows[0U].playchar_optional == 7U &&
		all->stats.one_ccs == 21U && all->stats.continues_used == 42U,
		"All Characters sorts best rows, appends TOTAL, and saturating-sums stats");
	auto character = new_classic_score_view(document, 1U, 0U);
	expect(character && character->valid[0U] && character->valid[1U] &&
		!character->total[9U] && character->stats == document.boards[1U][0U].stats,
		"character page exposes all entries and only that character's stats");
	expect(!new_classic_score_view(document, 1U, 7U) &&
		new_classic_score_view(document, 1U, 7U).error().code ==
			NewClassicScoreErrorCode::invalid_page,
		"locked ending characters stay hidden until Extra unlock");
	document.extra_unlocked = true;
	expect(new_classic_score_view(document, 1U, 8U).has_value(),
		"Extra unlock exposes Chiyuri and Yumemi pages");

	auto legacy = classic_score_stock_file();
	legacy.boards[0U].rows[0U].playchar_optional = 3U;
	legacy.boards[0U].rows[0U].score[8U] = ClassicRegistrationGlyph::digit_4;
	legacy.boards[0U].cleared = ClassicScoreCleared::cleared;
	auto migrated = migrate_classic_score_document(legacy);
	expect(migrated && migrated->extra_unlocked &&
		migrated->boards[0U][2U].entries[0U].valid &&
		migrated->boards[0U][2U].entries[0U].score[7U] == 4U,
		"legacy migration distributes global rows into per-character boards");
}

void test_run_accounting()
{
	using namespace th03::app;
	auto document = new_classic_score_empty();
	document.last_run_id = 6U;
	document.boards[2U][1U].stats = {
		.play_frames = 0xFFFFFFF0U,
		.one_ccs = 2U,
		.continues_used = 3U,
	};
	auto applied = new_classic_score_apply_run(
		document, 2U, 1U,
		{.run_id = 7U, .play_frames = 64U, .continues_used = 4U,
		 .one_cc = true}
	);
	expect(applied && *applied && document.last_run_id == 7U &&
		document.extra_unlocked &&
		document.boards[2U][1U].stats.play_frames == 0xFFFFFFFFU &&
		document.boards[2U][1U].stats.one_ccs == 3U &&
		document.boards[2U][1U].stats.continues_used == 7U,
		"live run accounting saturates frames, counts 1CC, and unlocks Extra");
	const auto before = document;
	auto duplicate = new_classic_score_apply_run(
		document, 2U, 1U,
		{.run_id = 7U, .play_frames = 1U, .one_cc = true}
	);
	expect(duplicate && !*duplicate && document == before,
		"already committed run IDs are idempotent no-ops");
	expect(!new_classic_score_apply_run(document, 2U, 1U, {}) &&
		new_classic_score_apply_run(document, 2U, 1U, {}).error().code ==
			NewClassicScoreErrorCode::invalid_run,
		"zero run IDs are rejected before mutation");
}

} // namespace

int main()
{
	test_empty_roundtrip_and_layout();
	test_domain_and_board_recovery();
	test_insertion_migration_and_views();
	test_run_accounting();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
