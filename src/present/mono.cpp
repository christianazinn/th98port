#include "th03/present/mono.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th03::present {
namespace {

constexpr std::int64_t pc98_vram_row_bytes = 80;

[[nodiscard]] bool valid_score_font(const ScoreFontMasks &font) noexcept {
	return font.rows.size() == score_font_mask_bytes &&
		font.row_order == PackedRowOrder::bottom_to_top;
}

[[nodiscard]] bool valid_pellet_masks(const PelletMasks &masks) noexcept {
	return masks.rows.size() == pellet_mask_bytes &&
		masks.row_order == PackedRowOrder::top_to_bottom;
}

[[nodiscard]] bool valid_staff_flake_masks(
	const StaffFlakeMasks &masks
) noexcept {
	return masks.rows.size() == staff_flake_mask_bytes &&
		masks.row_order == PackedRowOrder::top_to_bottom;
}

[[nodiscard]] std::optional<std::int32_t> align_down(
	std::int32_t value,
	std::int32_t alignment
) noexcept {
	auto wide = static_cast<std::int64_t>(value);
	auto remainder = wide % alignment;
	if(remainder < 0) {
		remainder += alignment;
	}
	wide -= remainder;
	return static_cast<std::int32_t>(wide);
}

[[nodiscard]] std::optional<std::size_t> pellet_cel_index(
	PelletCel cel
) noexcept {
	switch(cel) {
	case PelletCel::normal:
		return 0U;
	case PelletCel::transfer:
		return 1U;
	}
	return std::nullopt;
}

[[nodiscard]] bool pellet_uses_narrow_write(
	std::int32_t aligned_left,
	std::int32_t top
) noexcept {
	const auto byte_x = static_cast<std::int64_t>(aligned_left) / 8;
	const auto wide_offset = byte_x +
		(static_cast<std::int64_t>(top) * pc98_vram_row_bytes);
	const auto wrapped = static_cast<std::uint16_t>(
		static_cast<std::uint64_t>(wide_offset) & 0xFFFFU
	);
	const auto signed_offset = (wrapped <= 0x7FFFU) ?
		static_cast<std::int32_t>(wrapped) :
		(static_cast<std::int32_t>(wrapped) - 0x10000);
	return signed_offset < 8;
}

} // namespace

bool score_digit_put(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const ScoreFontMasks &font,
	std::uint8_t digit,
	Index4 color
) noexcept {
	const auto tile = GrcgTile::solid(color);
	const auto aligned_left = align_down(left, 8);
	if(!valid_score_font(font) || digit >= score_digit_count || !tile ||
		!aligned_left) {
		return false;
	}

	const auto digit_offset = static_cast<std::size_t>(digit) *
		score_digit_storage_rows;
	const BitSource source{
		font.rows.subspan(digit_offset, score_digit_draw_rows),
		8U,
		static_cast<std::uint16_t>(score_digit_draw_rows),
		1U,
		font.row_order,
	};
	return raster.grcg_write(
		source,
		{*aligned_left, top},
		GrcgWriteMode::read_modify_write,
		*tile
	);
}

bool pellet_put(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const PelletMasks &masks,
	PelletCel cel,
	Index4 color
) noexcept {
	const auto tile = GrcgTile::solid(color);
	const auto cel_index = pellet_cel_index(cel);
	const auto aligned_left = align_down(left, 16);
	if(!valid_pellet_masks(masks) || !cel_index || !tile || !aligned_left) {
		return false;
	}

	const auto preshift = static_cast<std::size_t>(
		static_cast<std::uint32_t>(left) & 0x0FU
	);
	const auto variant = ((*cel_index * pellet_preshift_count) + preshift) *
		pellet_variant_bytes;
	const bool narrow = pellet_uses_narrow_write(*aligned_left, top);
	const auto width = static_cast<std::uint16_t>(narrow ? 16U : 32U);
	const BitSource source{
		masks.rows.subspan(variant, pellet_variant_bytes),
		width,
		static_cast<std::uint16_t>(pellet_row_count),
		pellet_row_bytes,
		masks.row_order,
	};
	return raster.grcg_write(
		source,
		{*aligned_left, top},
		GrcgWriteMode::read_modify_write,
		*tile
	);
}

bool staff_flake_put_blue_or(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const StaffFlakeMasks &masks,
	std::uint8_t cel
) noexcept {
	const auto aligned_left = align_down(left, 8);
	if(!valid_staff_flake_masks(masks) || cel >= staff_flake_cel_count ||
		!aligned_left) {
		return false;
	}

	const auto cel_offset = static_cast<std::size_t>(cel) *
		staff_flake_cel_bytes;
	const auto shift = static_cast<int>(
		static_cast<std::uint32_t>(left) & 0x07U
	);
	std::array<std::uint8_t, staff_flake_cel_bytes> rotated{};
	for(std::size_t row = 0U; row < staff_flake_row_count; ++row) {
		const auto source_offset = cel_offset + (row * staff_flake_row_bytes);
		const auto source_word = static_cast<std::uint16_t>(
			static_cast<std::uint16_t>(masks.rows[source_offset]) |
			(static_cast<std::uint16_t>(masks.rows[source_offset + 1U]) << 8U)
		);
		const auto result = std::rotr(source_word, shift);
		const auto target_offset = row * staff_flake_row_bytes;
		rotated[target_offset] = static_cast<std::uint8_t>(result & 0x00FFU);
		rotated[target_offset + 1U] = static_cast<std::uint8_t>(result >> 8U);
	}

	const std::array<std::uint8_t, staff_flake_cel_bytes> zero_plane{};
	PlanarSource source{};
	for(auto &plane : source.planes) {
		plane = zero_plane;
	}
	source.planes[static_cast<std::size_t>(PlanarPlane::blue)] = rotated;
	source.width = 16U;
	source.height = static_cast<std::uint16_t>(staff_flake_row_count);
	source.stride = staff_flake_row_bytes;
	source.row_order = masks.row_order;
	return raster.blit_planar(
		source,
		{*aligned_left, top},
		PlanarWriteMode::or_planes
	);
}

} // namespace th03::present
