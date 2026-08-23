#include "th03/app/new_classic_nametag.hpp"

#include <iostream>
#include <string>

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

} // namespace

int main()
{
	using namespace th03::app;
	const auto short_name = layout_new_classic_nametag("AZINN");
	expect(short_name && short_name->line_count == 1U &&
		short_name->lines[0U] == "AZINN" && short_name->lines[1U].empty(),
		"a short account name occupies one title row");

	const auto title = layout_new_classic_nametag(
		"Resident of Fantasy Who Runs Through Time"
	);
	expect(title && title->line_count == 2U &&
		title->lines[0U] == "Resident of Fantasy Who Runs" &&
		title->lines[1U] == "Through Time",
		"the motivating long title wraps at a word boundary");

	const auto full = layout_new_classic_nametag(
		std::string(28U, 'A') + " " + std::string(28U, 'B')
	);
	expect(full && full->source.size() == new_classic_nametag_max_bytes &&
		full->lines[0U].size() == 28U && full->lines[1U].size() == 28U,
		"57 source bytes exactly fill two 28-cell rows");

	const auto accented = layout_new_classic_nametag(
		"Jos\xC3\xA9 Mu\xC3\xB1oz, campe\xC3\xB3n de fantas\xC3\xAD" "a"
	);
	expect(accented && accented->line_count == 2U &&
		accented->source ==
			"Jos\xC3\xA9 Mu\xC3\xB1oz, campe\xC3\xB3n de fantas\xC3\xAD" "a",
		"UTF-8 Latin nametags wrap by display cells rather than source bytes");
	const auto japanese = layout_new_classic_nametag(
		"\xE5\xB9\xBB\xE6\x83\xB3\xE3\x81\xAE\xE4\xBD\x8F\xE4\xBA\xBA"
	);
	expect(japanese && japanese->line_count == 1U &&
		japanese->lines[0U] ==
			"\xE5\xB9\xBB\xE6\x83\xB3\xE3\x81\xAE\xE4\xBD\x8F\xE4\xBA\xBA",
		"Japanese nametags occupy two display cells per wide glyph");
	const auto combining = layout_new_classic_nametag("Jose\xCC\x81");
	expect(combining && combining->line_count == 1U,
		"decomposed combining accents do not consume an extra display cell");

	expect(layout_new_classic_nametag("").error() ==
		NewClassicNametagError::empty, "empty names are rejected");
	expect(layout_new_classic_nametag(" leading").error() ==
		NewClassicNametagError::invalid_spacing,
		"leading spaces are rejected rather than silently normalized");
	expect(layout_new_classic_nametag("two  spaces").error() ==
		NewClassicNametagError::invalid_spacing,
		"repeated spaces are rejected");
	expect(layout_new_classic_nametag("line\nbreak").error() ==
		NewClassicNametagError::invalid_character,
		"control characters are rejected");
	const std::string malformed{"bad\xC3\x28", 5U};
	expect(layout_new_classic_nametag(malformed).error() ==
		NewClassicNametagError::invalid_character,
		"malformed UTF-8 is rejected rather than rendered bytewise");
	expect(layout_new_classic_nametag("\xE2\x80\xAEspoof").error() ==
		NewClassicNametagError::invalid_character,
		"unsupported bidi controls are rejected rather than invisibly reordered");
	expect(layout_new_classic_nametag("\xCC\x81").error() ==
		NewClassicNametagError::word_too_long,
		"a name cannot consist only of zero-cell combining marks");
	expect(layout_new_classic_nametag(std::string(29U, 'A')).error() ==
		NewClassicNametagError::word_too_long,
		"a word wider than one player column is rejected");
	expect(layout_new_classic_nametag(
		"AAAAAAAAAAAAAA BBBBBBBBBBBBBB CCCCCCCCCCCCCC"
	).error() == NewClassicNametagError::too_many_lines,
		"names requiring a third row are rejected even below the byte maximum");
	expect(layout_new_classic_nametag(std::string(58U, 'A')).error() ==
		NewClassicNametagError::too_long,
		"input beyond the geometric maximum is rejected first");

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
