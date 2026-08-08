#ifndef TH03_FORMATS_ENEDAT_HPP
#define TH03_FORMATS_ENEDAT_HPP

#include <cstddef>
#include <expected>
#include <span>

#include "th03/core/game.hpp"
#include "th03/error.hpp"

namespace th03::formats {

std::expected<GameContent, Error> parse_enedat(
	std::span<const std::byte> bytes
);

} // namespace th03::formats

#endif
