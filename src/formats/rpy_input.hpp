#ifndef TH03_FORMATS_RPY_INPUT_HPP
#define TH03_FORMATS_RPY_INPUT_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

#include "th03/error.hpp"
#include "th03/replay/session.hpp"

namespace th03::formats::detail {

std::expected<std::vector<replay::SessionRecord>, Error> decode_rpy_input(
	std::span<const std::byte> bytes,
	std::size_t input_offset,
	std::size_t input_size,
	std::uint32_t sample_count,
	std::string_view format_name
);

} // namespace th03::formats::detail

#endif
