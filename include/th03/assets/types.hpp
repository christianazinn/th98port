#ifndef TH03_ASSETS_TYPES_HPP
#define TH03_ASSETS_TYPES_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace th03::assets {

struct Rgb4 {
	std::uint8_t r = 0U;
	std::uint8_t g = 0U;
	std::uint8_t b = 0U;

	friend bool operator==(const Rgb4 &, const Rgb4 &) = default;
};

struct Palette16Rgb4 {
	std::array<Rgb4, 16U> colors{};

	friend bool operator==(
		const Palette16Rgb4 &,
		const Palette16Rgb4 &
	) = default;
};

struct IndexedImage4 {
	std::uint16_t width = 0U;
	std::uint16_t height = 0U;

	// Row-major, one byte per pixel. Every value is in [0, 15].
	std::vector<std::uint8_t> indices;
};

struct PiImage {
	// Header data is retained as format data, without native pointers.
	std::vector<std::byte> comment;
	std::uint8_t mode = 0U;
	std::uint8_t aspect_n = 0U;
	std::uint8_t aspect_m = 0U;
	std::uint8_t plane_count = 0U;
	std::array<std::uint8_t, 4U> machine{};
	std::vector<std::byte> machine_extension;

	// Mode bit 7 suppresses the inline palette.
	std::optional<Palette16Rgb4> palette;
	IndexedImage4 image;
};

enum class AssetErrorCode {
	truncated,
	bad_magic,
	unsupported_layout,
	invalid_value,
	size_mismatch,
	malformed_stream,
	output_overflow,
};

struct AssetError {
	AssetErrorCode code;
	std::string message;
	std::size_t offset = 0U;
};

} // namespace th03::assets

#endif
