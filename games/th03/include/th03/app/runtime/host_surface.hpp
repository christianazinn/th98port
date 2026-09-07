#ifndef TH03_APP_HOST_SURFACE_HPP
#define TH03_APP_HOST_SURFACE_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

#include "th03/present/source_surface.hpp"

namespace th03::app {

enum class HostRgbaByteOrder : std::uint8_t {
	rgba,
	bgra,
	argb,
	abgr,
};

// Caller-owned output. Pitch is the byte distance between consecutive rows;
// it may exceed the 640x4 bytes written by each row. This storage must not
// overlap SourceSurface::pixels.
struct HostRgbaTarget {
	std::span<std::byte> bytes{};
	std::size_t pitch_bytes = 0U;
	HostRgbaByteOrder byte_order = HostRgbaByteOrder::rgba;
};

enum class HostSurfaceErrorCode : std::uint8_t {
	invalid_byte_order,
	pitch_too_small,
	output_too_small,
	size_overflow,
	invalid_graphics_palette,
	invalid_composed_pixel,
};

struct HostSurfaceError {
	HostSurfaceErrorCode code = HostSurfaceErrorCode::output_too_small;
	// Exact source pixel for invalid_composed_pixel; otherwise SIZE_MAX.
	std::size_t pixel_offset = static_cast<std::size_t>(-1);
};

// Resolves one exact 640x400 source surface into caller-owned 8-bit RGBA
// channel bytes. Graphics RGB4 nibbles expand to v*17. TextColor stays in its
// fixed digital domain: bit 0 is blue, bit 1 red, and bit 2 green. Alpha is
// always 0xFF. All target geometry, palette entries, and source pixels are
// validated before the first output byte changes; row padding is untouched.
[[nodiscard]] std::expected<void, HostSurfaceError>
source_surface_to_host_rgba(
	const present::SourceSurface &source,
	HostRgbaTarget target
) noexcept;

struct IntegerLetterboxRect {
	std::size_t x = 0U;
	std::size_t y = 0U;
	std::size_t width = 0U;
	std::size_t height = 0U;
	std::size_t scale = 0U;

	[[nodiscard]] friend constexpr bool operator==(
		const IntegerLetterboxRect &,
		const IntegerLetterboxRect &
	) = default;
};

// Largest centered integer multiple of TH03's 640x400 (8:5) surface. A
// drawable that cannot fit one complete source pixel grid returns nullopt.
// Division selects the scale before either multiplication, keeping SIZE_MAX
// dimensions well-defined without saturation.
[[nodiscard]] std::optional<IntegerLetterboxRect> integer_letterbox_8x5(
	std::size_t drawable_width,
	std::size_t drawable_height
) noexcept;

} // namespace th03::app

#endif
