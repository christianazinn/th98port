#include "th02/formats/stage_assets.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th02::formats {
namespace {

inline constexpr std::size_t map_magic_size = 10U;
inline constexpr std::size_t bfnt_header_size = 32U;
inline constexpr std::size_t bfnt_palette_size = 48U;
inline constexpr std::uint8_t bfnt_palette_flag = 0x80U;
inline constexpr std::uint8_t bfnt_color_count_mask = 0x7FU;
inline constexpr std::uint8_t bfnt_superimpose_color_count = 3U;

[[nodiscard]] std::uint16_t le16(
	std::span<const std::uint8_t> bytes,
	std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U)
	);
}

[[nodiscard]] bool has_bytes(std::span<const std::uint8_t> bytes,
	std::size_t cursor, std::size_t count) noexcept
{
	return cursor <= bytes.size() && count <= (bytes.size() - cursor);
}

} // namespace

StageMapDecodeResult stage_map_decode(
	std::span<const std::uint8_t> bytes,
	core::TileSectionTiles &out
) noexcept
{
	constexpr auto section_byte_count =
		core::tile_scroll_section_count * core::tile_scroll_rows_per_section *
		core::tile_scroll_columns;
	if(!has_bytes(bytes, map_magic_size, section_byte_count)) {
		return StageMapDecodeResult::truncated;
	}

	core::TileSectionTiles decoded{};
	std::size_t cursor = map_magic_size;
	for(std::size_t section = 0U; section < decoded.size(); ++section) {
		for(std::size_t row = 0U; row < decoded[section].size(); ++row) {
			for(std::size_t column = 0U;
				column < decoded[section][row].size(); ++column) {
				decoded[section][row][column] = bytes[cursor++];
			}
		}
	}
	out = decoded;
	return StageMapDecodeResult::completed;
}

BftSpriteMetricsResult bft_sprite_metrics_append(
	std::span<const std::uint8_t> bytes,
	BftSpriteMetrics &metrics
) noexcept
{
	if(bytes.size() < bfnt_header_size) {
		return BftSpriteMetricsResult::truncated_header;
	}
	constexpr std::array<std::uint8_t, 5U> identifier{
		static_cast<std::uint8_t>('B'), static_cast<std::uint8_t>('F'),
		static_cast<std::uint8_t>('N'), static_cast<std::uint8_t>('T'), 0x1AU,
	};
	for(std::size_t index = 0U; index < identifier.size(); ++index) {
		if(bytes[index] != identifier[index]) {
			return BftSpriteMetricsResult::invalid_identifier;
		}
	}
	if((bytes[5U] & bfnt_color_count_mask) != bfnt_superimpose_color_count) {
		return BftSpriteMetricsResult::unsupported_color_format;
	}

	const auto pixel_width = le16(bytes, 8U);
	const auto pixel_height = le16(bytes, 10U);
	const auto first_pattern = le16(bytes, 12U);
	const auto last_pattern = le16(bytes, 14U);
	if(pixel_width == 0U || (pixel_width % 8U) != 0U ||
		(pixel_width / 8U) > 0x00FFU || pixel_height == 0U ||
		pixel_height > 0x00FFU) {
		return BftSpriteMetricsResult::unsupported_dimensions;
	}
	if(last_pattern < first_pattern) {
		return BftSpriteMetricsResult::unsupported_range;
	}
	if(metrics.pattern_count > super_pattern_capacity) {
		return BftSpriteMetricsResult::invalid_pattern_state;
	}

	std::size_t cursor = bfnt_header_size;
	const auto extension_size = static_cast<std::size_t>(le16(bytes, 28U));
	if(!has_bytes(bytes, cursor, extension_size)) {
		return BftSpriteMetricsResult::truncated_extended_header;
	}
	cursor += extension_size;
	if((bytes[5U] & bfnt_palette_flag) != 0U) {
		if(!has_bytes(bytes, cursor, bfnt_palette_size)) {
			return BftSpriteMetricsResult::truncated_palette;
		}
		cursor += bfnt_palette_size;
	}

	const auto byte_width = static_cast<std::uint16_t>(pixel_width / 8U);
	const auto plane_size = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(byte_width) * pixel_height
	);
	const auto pattern_byte_count = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(plane_size) << 2U
	);
	const auto metric = static_cast<core::EnemySpriteSizeWord>(
		(static_cast<std::uint16_t>(byte_width) << 8U) | pixel_height
	);
	const auto pattern_total = static_cast<std::size_t>(
		static_cast<std::uint16_t>(last_pattern - first_pattern)
	) + 1U;

	for(std::size_t pattern = 0U; pattern < pattern_total; ++pattern) {
		if(!has_bytes(bytes, cursor, pattern_byte_count)) {
			return BftSpriteMetricsResult::truncated_pattern_data;
		}
		cursor += pattern_byte_count;
		if(metrics.pattern_count == super_pattern_capacity) {
			return BftSpriteMetricsResult::pattern_pool_full;
		}
		metrics.sizes[metrics.pattern_count] = metric;
		++metrics.pattern_count;
	}
	return BftSpriteMetricsResult::completed;
}

} // namespace th02::formats
