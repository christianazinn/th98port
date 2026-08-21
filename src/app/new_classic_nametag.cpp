#include "th03/app/new_classic_nametag.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

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
	if(!std::ranges::all_of(nametag, [](const char character) {
		const auto byte = static_cast<unsigned char>(character);
		return byte >= 0x20U && byte <= 0x7EU;
	})) {
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
		if(word.size() > new_classic_nametag_line_cells) {
			return std::unexpected(NewClassicNametagError::word_too_long);
		}
		auto &destination = result.lines[line];
		const auto required = destination.empty() ?
			word.size() : destination.size() + 1U + word.size();
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
