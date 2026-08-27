#include "th03/app/new_classic/netplay/new_classic_nametag.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

#include "th03/core/utf8.hpp"

namespace th03::app {

std::expected<NewClassicNametagLayout, NewClassicNametagError>
layout_new_classic_nametag(std::string_view nametag)
{
	if(nametag.empty()) {
		return std::unexpected(NewClassicNametagError::empty);
	}
	if(nametag.size() > new_classic_nametag_max_bytes) {
		return std::unexpected(NewClassicNametagError::too_long);
	}
	if(nametag.front() == ' ' || nametag.back() == ' ' ||
		nametag.find("  ") != std::string_view::npos) {
		return std::unexpected(NewClassicNametagError::invalid_spacing);
	}
	if(!valid_utf8(nametag)) {
		return std::unexpected(NewClassicNametagError::invalid_character);
	}
	const auto cell_count = [](std::string_view text) noexcept {
		std::size_t cells = 0U;
		for(std::size_t cursor = 0U; cursor < text.size();) {
			const auto scalar = next_utf8_scalar(text, cursor);
			if(!scalar || !unicode_printable_scalar(scalar->value)) {
				return std::optional<std::size_t>{};
			}
			cells += unicode_cell_width(scalar->value);
			cursor += scalar->bytes;
		}
		return std::optional{cells};
	};
	if(!cell_count(nametag)) {
		return std::unexpected(NewClassicNametagError::invalid_character);
	}

	NewClassicNametagLayout result{};
	result.source = nametag;
	std::size_t line = 0U;
	std::size_t offset = 0U;
	while(offset < nametag.size()) {
		const auto separator = nametag.find(' ', offset);
		const auto end = separator == std::string_view::npos ?
			nametag.size() : separator;
		const auto word = nametag.substr(offset, end - offset);
		const auto word_cells = cell_count(word);
		if(!word_cells || *word_cells == 0U ||
			*word_cells > new_classic_nametag_line_cells) {
			return std::unexpected(NewClassicNametagError::word_too_long);
		}
		auto &destination = result.lines[line];
		const auto destination_cells = cell_count(destination).value_or(0U);
		const auto required = destination.empty() ?
			*word_cells : destination_cells + 1U + *word_cells;
		if(required > new_classic_nametag_line_cells) {
			++line;
			if(line >= result.lines.size()) {
				return std::unexpected(NewClassicNametagError::too_many_lines);
			}
		}
		auto &target = result.lines[line];
		if(!target.empty()) {
			target.push_back(' ');
		}
		target.append(word);
		offset = end == nametag.size() ? end : end + 1U;
	}
	result.line_count = static_cast<std::uint8_t>(line + 1U);
	return result;
}

} // namespace th03::app
