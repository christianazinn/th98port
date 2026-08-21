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
