#ifndef TH03_ASSETS_DECODE_HPP
#define TH03_ASSETS_DECODE_HPP

#include <cstddef>
#include <expected>
#include <span>

#include "th03/assets/types.hpp"

namespace th03::assets {

std::expected<Palette16Rgb4, AssetError> decode_rgb48(
	std::span<const std::byte> bytes
);

std::expected<PiImage, AssetError> decode_pi(
	std::span<const std::byte> bytes
);

} // namespace th03::assets

#endif
