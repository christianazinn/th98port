#pragma once

#include "th02/core/stage_data.hpp"
#include "th02/core/tile_scroll.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace th02::formats {

inline constexpr std::size_t super_pattern_capacity = 512U;

// The source's `super_patsize` word pool, together with `super_patnum`.
// super_entry_bfnt() appends to pattern_count; BFNT START is only used to
// derive the number of patterns to read.
struct BftSpriteMetrics {
	std::array<core::EnemySpriteSizeWord, super_pattern_capacity> sizes{};
	std::uint16_t pattern_count{};
	bool operator==(const BftSpriteMetrics &) const = default;
};

enum class StageMapDecodeResult : std::uint8_t {
	completed,
	truncated,
};

enum class BftSpriteMetricsResult : std::uint8_t {
	completed,
	truncated_header,
	invalid_identifier,
	unsupported_color_format,
	unsupported_dimensions,
	unsupported_range,
	truncated_extended_header,
	truncated_palette,
	truncated_pattern_data,
	invalid_pattern_state,
	pattern_pool_full,
};

// th02/formats/map.cpp::map_load. The ten-byte MAP identifier is read but
// deliberately not validated by the original. Extra source bytes follow the
// fixed section table harmlessly.
[[nodiscard]] StageMapDecodeResult stage_map_decode(
	std::span<const std::uint8_t> bytes,
	core::TileSectionTiles &out
) noexcept;

// Metrics-only model of master.lib::super_entry_bfnt. It accepts the
// superimpose BFNT path used by TH02 (low seven color bits equal 3), skips
// source extension/palette payloads, and consumes but does not decode pixels.
// A late payload failure retains metrics appended by earlier pattern reads,
// matching the source's per-pattern registration order.
[[nodiscard]] BftSpriteMetricsResult bft_sprite_metrics_append(
	std::span<const std::uint8_t> bytes,
	BftSpriteMetrics &metrics
) noexcept;

} // namespace th02::formats
