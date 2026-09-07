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

// CDG and CD2 are the same format. A stored image count of zero denotes one
// cel; nonzero counts denote that many cels in the same byte layout.
std::expected<CdgFile, AssetError> decode_cdg(
	std::span<const std::byte> bytes,
	CdgAlphaPolicy alpha_policy = CdgAlphaPolicy::preserve
);

// Mirrors the original single-cel load path without allocating other cels.
std::expected<PlanarCel4, AssetError> decode_cdg_cel(
	std::span<const std::byte> bytes,
	std::size_t cel_index,
	CdgAlphaPolicy alpha_policy = CdgAlphaPolicy::preserve
);

// MRS is a fixed, headerless five-plane image. The decoder always preserves
// alpha because every source load does; filenames and extensions are
// intentionally absent from this API.
std::expected<MrsImage, AssetError> decode_mrs(
	std::span<const std::byte> bytes
);

// Mirrors the source's persistent mrs_hflip() transformation. Calling it
// twice restores the original image.
void horizontal_flip(MrsImage &image);

// BFT and BF2 use the same BFNT byte format. The explicit consumer selects
// the source-backed gaiji or superimpose validation and decoded pixel model;
// filenames and extensions are intentionally absent from this API.
std::expected<BfntSheet, AssetError> decode_bfnt(
	std::span<const std::byte> bytes,
	BfntConsumerPolicy consumer
);

} // namespace th03::assets

#endif
