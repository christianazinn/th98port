#include "th03/assets/decode.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace th03::assets {
namespace {

constexpr std::size_t color_count = 16U;
constexpr std::size_t components_per_color = 3U;
constexpr std::size_t rgb48_size = color_count * components_per_color;

} // namespace

std::expected<Palette16Rgb4, AssetError> decode_rgb48(
	std::span<const std::byte> bytes
)
{
	if(bytes.size() != rgb48_size) {
		return std::unexpected(AssetError{
			AssetErrorCode::size_mismatch,
			"RGB palette must contain exactly 48 bytes",
			(bytes.size() < rgb48_size) ? bytes.size() : rgb48_size,
		});
	}

	Palette16Rgb4 palette{};
	for(std::size_t color = 0U; color < color_count; ++color) {
		const auto offset = color * components_per_color;
		std::array<std::uint8_t, components_per_color> components{};
		for(std::size_t component = 0U; component < components.size(); ++component) {
			components[component] = std::to_integer<std::uint8_t>(
				bytes[offset + component]
			);
			if(components[component] > 0x0FU) {
				return std::unexpected(AssetError{
					AssetErrorCode::invalid_value,
					"RGB palette component exceeds its four-bit source range",
					offset + component,
				});
			}
		}
		palette.colors[color] = Rgb4{
			components[0U],
			components[1U],
			components[2U],
		};
	}
	return palette;
}

} // namespace th03::assets
