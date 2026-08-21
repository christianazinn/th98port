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
// normalized: printable ASCII, no leading/trailing/repeated spaces. Words are
// never split. The separating source space is omitted at a line break, so the
// largest geometrically representable input is 28 + 1 + 28 = 57 bytes.
[[nodiscard]] std::expected<NewClassicNametagLayout, NewClassicNametagError>
layout_new_classic_nametag(std::string_view nametag);

} // namespace th03::app
