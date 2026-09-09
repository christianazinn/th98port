#include "th02/formats/stage_assets.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

using namespace th02;

void put_le16(std::vector<std::uint8_t> &bytes, std::size_t offset,
	std::uint16_t value)
{
	bytes[offset] = static_cast<std::uint8_t>(value & 0x00FFU);
	bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

[[nodiscard]] std::vector<std::uint8_t> make_bft(
	std::uint16_t width, std::uint16_t height,
	std::uint16_t first_pattern, std::uint16_t last_pattern,
	bool palette, std::uint16_t extension_size = 0U
)
{
	constexpr std::size_t header_size = 32U;
	constexpr std::size_t palette_size = 48U;
	std::vector<std::uint8_t> bytes(header_size + extension_size);
	bytes[0U] = static_cast<std::uint8_t>('B');
	bytes[1U] = static_cast<std::uint8_t>('F');
	bytes[2U] = static_cast<std::uint8_t>('N');
	bytes[3U] = static_cast<std::uint8_t>('T');
	bytes[4U] = 0x1AU;
	bytes[5U] = palette ? 0x83U : 3U;
	put_le16(bytes, 8U, width);
	put_le16(bytes, 10U, height);
	put_le16(bytes, 12U, first_pattern);
	put_le16(bytes, 14U, last_pattern);
	put_le16(bytes, 28U, extension_size);
	put_le16(bytes, 30U, extension_size);
	if(extension_size >= 4U) {
		bytes[header_size] = 0x10U;
		bytes[header_size + 3U] = 7U;
	}
	if(palette) {
		bytes.resize(bytes.size() + palette_size);
	}
	const auto pattern_count = static_cast<std::size_t>(
		static_cast<std::uint16_t>(last_pattern - first_pattern)
	) + 1U;
	const auto bytes_per_pattern = static_cast<std::size_t>(width / 8U) *
		static_cast<std::size_t>(height) * 4U;
	bytes.resize(bytes.size() + (pattern_count * bytes_per_pattern));
	return bytes;
}

[[nodiscard]] bool map_loads_fixed_sections_and_ignores_magic() noexcept
{
	constexpr auto table_size = core::tile_scroll_section_count *
		core::tile_scroll_rows_per_section * core::tile_scroll_columns;
	std::vector<std::uint8_t> bytes(10U + table_size + 3U, 0U);
	for(std::size_t index = 0U; index < 10U; ++index) {
		bytes[index] = static_cast<std::uint8_t>(0xA0U + index);
	}
	std::size_t cursor = 10U;
	for(std::size_t section = 0U;
		section < core::tile_scroll_section_count; ++section) {
		for(std::size_t row = 0U; row < core::tile_scroll_rows_per_section; ++row) {
			for(std::size_t column = 0U; column < core::tile_scroll_columns;
				++column) {
				bytes[cursor++] = static_cast<std::uint8_t>(
					(section * 40U) + (row * 5U) + column
				);
			}
		}
	}
	core::TileSectionTiles decoded{};
	if(formats::stage_map_decode(bytes, decoded) !=
		formats::StageMapDecodeResult::completed) {
		return false;
	}
	return decoded[0U][0U][0U] == 0U && decoded[1U][2U][3U] == 53U &&
		decoded[15U][7U][23U] == static_cast<std::uint8_t>(658U);
}

[[nodiscard]] bool map_refuses_truncation_without_writing() noexcept
{
	constexpr auto table_size = core::tile_scroll_section_count *
		core::tile_scroll_rows_per_section * core::tile_scroll_columns;
	std::vector<std::uint8_t> short_map(10U + table_size - 1U, 0U);
	core::TileSectionTiles decoded{};
	decoded[0U][0U][0U] = 0x5AU;
	const auto expected = decoded;
	return formats::stage_map_decode(short_map, decoded) ==
		formats::StageMapDecodeResult::truncated && decoded == expected;
}

[[nodiscard]] bool bft_appends_by_super_pattern_count() noexcept
{
	formats::BftSpriteMetrics metrics{};
	metrics.pattern_count = 128U;
	metrics.sizes[127U] = 0x0101U;
	const auto first = make_bft(16U, 3U, 42U, 43U, true, 5U);
	if(formats::bft_sprite_metrics_append(first, metrics) !=
		formats::BftSpriteMetricsResult::completed ||
		metrics.pattern_count != 130U || metrics.sizes[127U] != 0x0101U ||
		metrics.sizes[128U] != 0x0203U || metrics.sizes[129U] != 0x0203U) {
		return false;
	}
	const auto second = make_bft(32U, 4U, 7U, 9U, false);
	if(formats::bft_sprite_metrics_append(second, metrics) !=
		formats::BftSpriteMetricsResult::completed) {
		return false;
	}
	return metrics.pattern_count == 133U && metrics.sizes[130U] == 0x0404U &&
		metrics.sizes[131U] == 0x0404U && metrics.sizes[132U] == 0x0404U;
}

[[nodiscard]] bool bft_refusals_preserve_source_append_prefix() noexcept
{
	formats::BftSpriteMetrics metrics{};
	metrics.pattern_count = 3U;
	metrics.sizes[2U] = 0x0909U;
	auto truncated_second_pattern = make_bft(8U, 1U, 0U, 1U, false);
	truncated_second_pattern.resize(truncated_second_pattern.size() - 1U);
	if(formats::bft_sprite_metrics_append(truncated_second_pattern, metrics) !=
		formats::BftSpriteMetricsResult::truncated_pattern_data ||
		metrics.pattern_count != 4U || metrics.sizes[2U] != 0x0909U ||
		metrics.sizes[3U] != 0x0101U) {
		return false;
	}

	formats::BftSpriteMetrics unchanged{};
	unchanged.pattern_count = 4U;
	unchanged.sizes[3U] = 0x0808U;
	const auto expected = unchanged;
	auto bad_identifier = make_bft(8U, 1U, 0U, 0U, false);
	bad_identifier[0U] = 0U;
	if(formats::bft_sprite_metrics_append(bad_identifier, unchanged) !=
		formats::BftSpriteMetricsResult::invalid_identifier || unchanged != expected) {
		return false;
	}

	formats::BftSpriteMetrics full{};
	full.pattern_count = static_cast<std::uint16_t>(formats::super_pattern_capacity);
	auto no_pattern_data = make_bft(8U, 1U, 0U, 0U, false);
	no_pattern_data.resize(no_pattern_data.size() - 1U);
	if(formats::bft_sprite_metrics_append(no_pattern_data, full) !=
		formats::BftSpriteMetricsResult::truncated_pattern_data) {
		return false;
	}
	const auto complete = make_bft(8U, 1U, 0U, 0U, false);
	return formats::bft_sprite_metrics_append(complete, full) ==
		formats::BftSpriteMetricsResult::pattern_pool_full &&
		full.pattern_count == formats::super_pattern_capacity;
}

} // namespace

int main()
{
	const std::vector checks{
		map_loads_fixed_sections_and_ignores_magic(),
		map_refuses_truncation_without_writing(),
		bft_appends_by_super_pattern_count(),
		bft_refusals_preserve_source_append_prefix(),
	};
	for(std::size_t index = 0U; index < checks.size(); ++index) {
		if(!checks[index]) {
			std::fprintf(stderr, "TH02 stage-assets test %zu failed\n", index);
			return EXIT_FAILURE;
		}
	}
	std::fputs("TH02 stage-assets tests passed\n", stdout);
	return EXIT_SUCCESS;
}
