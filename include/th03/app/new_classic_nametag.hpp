#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace th03::app {

constexpr std::size_t new_classic_nametag_line_cells = 28U;
constexpr std::size_t new_classic_nametag_line_count = 2U;
constexpr std::size_t new_classic_nametag_max_bytes =
	(new_classic_nametag_line_cells * new_classic_nametag_line_count) + 1U;

enum class NewClassicNametagError : std::uint8_t {
	empty,
	too_long,
	invalid_character,
	invalid_spacing,
	word_too_long,
	too_many_lines,
};

struct NewClassicNametagLayout final {
	std::string source{};
	std::array<std::string, new_classic_nametag_line_count> lines{};
	std::uint8_t line_count = 0U;

	friend bool operator==(
		const NewClassicNametagLayout &,
		const NewClassicNametagLayout &
	) = default;
};

// Validates the exact fixed-width MAINL title replacement. Input is already
// normalized: printable UTF-8, no leading/trailing/repeated ASCII spaces.
// Words are never split. East Asian wide glyphs occupy two cells and combining
// marks occupy zero. V14's fixed snapshot slots still cap the source at 57
// UTF-8 bytes; the geometry independently caps each row at 28 display cells.
[[nodiscard]] std::expected<NewClassicNametagLayout, NewClassicNametagError>
layout_new_classic_nametag(std::string_view nametag);

} // namespace th03::app
